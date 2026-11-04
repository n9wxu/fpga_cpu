// T1S: T1F (t1f_core.v) made synchronous and pipelined for a block-RAM FPGA
// (docs/t1f.md, "T1S"). It runs on the system clock with a clock enable
// (ce: one step per enabled edge, at least two clocks apart):
//   - decode in a block-RAM ROM (t1s_dec.v), register file in a block RAM
//   - two instructions in flight: t1s_fetch (below) reads 32-bit words from
//     the PC on, through its own port, at every clock, into a queue; the
//     next instruction is decoded from there at the edge the current one
//     ends, and starts at once. The byte bus is for data only (loads,
//     stores), so a step without one can be short (fpga/rtl/cpumem.v)
//   - PC is its own register (the next instruction's address); a jump
//     empties the queue and restarts fetching there; a store within 32
//     bytes after the PC refetches, so code sees every store, as in fsim
// What follows is T1's description; the sequencer is T1's.
//
// T1: the smallest fpga_cpu core (docs/scaling.md, docs/t1.md).
//
// Today's ISA (cpu/isa.yaml, encoding variant A) on an 8-bit datapath and an
// 8-bit bus, at about the transistor count of a 6502. Everything is done a
// byte at a time:
//   - register file: r0-r15 and two hidden temporaries for multiply, as 36
//     bytes with two read ports and one write port (t1_rf.v)
//   - one 8-bit adder/logic unit, and a funnel shifter that does any 16-bit
//     shift in two byte passes
//   - PC and data address registers of 21 bits: bit 20 stands for address
//     bits 23:20 all being ones (I/O and boot ROM), so the external address
//     is {4{a[20]}, a[19:0]}
//   - one memory access per clock, from the PC (fetch) or AR (data), through
//     one shared incrementer
//
// Speedups, each chosen for its cost (docs/t1.md has the numbers):
//   - the next instruction's first parcel is prefetched in idle bus cycles,
//     so most register instructions cost only their ALU passes. A store
//     prefetches only one byte and drops it if the store writes it, so
//     self-modifying code needs no SYNC
//   - AR remembers which pair its upper 16 bits came from ("AR tag"): a later
//     access through that pair with a small displacement computes only the
//     low byte, and so does one through a pair just written by LEA or ADDPI
//   - branches, small displacements and ADDPI stop after the low byte when
//     the carry cannot change the upper bytes
//   - multiply skips the multiplier's leading zeros, and for MUL picks the
//     operand whose high byte is zero as the multiplier
//
// Extension X (parameter EXT_X): MULPU, FSHR, FSHL, CLZ and DSTEP run on the
// same byte ALU, funnel shifter and multiply sequencer. With EXT_X = 0 they
// trap as illegal instructions, as fsim does without --ext.
//
// Faults: halted with halt_code FD for an odd address in a word or pair
// access, an access or fetch at 0x100000 (stepping over the end of RAM, or
// an address in 0x100000-0xEFFFFF), and a reset_pc that is odd or in that
// range. Reserved encodings, including odd pair numbers, halt with FE.
//
// Bus: one access per cycle. addr, mem_rd, mem_wr and dout are combinational
// from the current state; din is sampled at the clock edge.

`timescale 1ns / 1ns
`default_nettype none

module t1s_core #(
    parameter EXT_X = 1               // extension X: MULPU, FSHR, FSHL, CLZ, DSTEP (else they trap as illegal)
) (
    input  wire        clk,
    input  wire        ce,           // clock enable: low holds everything (a stall)
    input  wire        rst,          // synchronous, active high
    input  wire [23:0] reset_pc,
    output wire [23:0] addr,
    output wire        mem_rd,
    output wire        mem_wr,
    output wire [7:0]  dout,
    input  wire [7:0]  din,
    // the instruction port (t1s_fetch): 32-bit words, in order
    output wire        i_req,        // a word wanted at i_addr: held until i_ack
    output wire [23:2] i_addr,
    input  wire        i_ack,        // taken (i_addr may change at this edge)
    input  wire        i_valid,      // a word back, on i_data
    input  wire [31:0] i_data,
    output wire [1:0]  sr_sel,       // which of SR1-SR3 MFSR is reading
    input  wire [15:0] sr_in,        // SR1-SR3 (cycle and instruction counters) live outside the core
    input  wire        irq,          // the interrupt line (docs/t1.md "Interrupts")
    output reg         halted,
    output reg  [7:0]  halt_code,    // SYS imm; FE illegal, FD address out of range
    output reg         retire        // an instruction completed at the last clock edge
);

    // ---------------------------------------------------------------- state

    localparam S_FETCH = 3'd0, S_EX = 3'd2, S_DATA = 3'd3, S_HALT = 3'd4;

    reg [2:0]  state;
    reg [3:0]  k;              // step within the state
    reg [20:0] pcf;            // PC: the next instruction's address (folded 21-bit address)
    reg [20:0] ar;             // data address; the multiplier during MUL
    reg [15:0] ir, p2;         // first and second parcel
    reg [15:0] q0, q1, q2, q3; // the instruction queue: the parcels from the PC on
    reg [2:0]  qn;             // parcels in it
    reg        ep;             // fetch epoch: changes at a jump (t1s_fetch restarts at the PC)
    reg        smc;            // this instruction stored near the PC: refetch after it
    reg        fN, fZ, fC, fV;
    reg        cy;             // carry between byte passes
    reg        zacc;           // the low result byte was zero
    reg        l1;             // bit carried between multiply passes
    reg [1:0]  l2;             // index bits carried between scaled passes
    reg        sgn;            // index sign, loaded byte's sign, multiplicand's sign
    reg        cst;            // saved carry-stop / fold result
    reg [4:0]  cnt;            // shift count; multiply steps left
    reg [2:0]  tag;            // the pair whose bytes 1-2 AR[20:8] holds
    reg        tagv;
    reg        accz;           // multiply accumulator still zero
    reg        swp;            // multiply operands swapped; in a store, its data is in the temporaries
    reg        pcx;            // the PC has run past 0x0FFFFF (reads as 0x100000: no memory there)
    // interrupts: EPC (the PC an interrupt replaced, folded as pcf) and IE
    reg [20:0] epc;
    reg        ie;

    // ---------------------------------------------------------------- decode

    localparam C_ALU = 4'd0, C_SHF = 4'd1, C_MUL = 4'd2, C_MEM = 4'd3, C_PAIR = 4'd4,
               C_BR = 4'd5, C_JAL = 4'd6, C_JR = 4'd7, C_JALR = 4'd8, C_MFSR = 4'd9,
               C_MTSR = 4'd10, C_NOP = 4'd11, C_TRAP = 4'd12, C_CLZ = 4'd13, C_DST = 4'd14;

    // ALUFN, isa.yaml tables.ALUFN
    localparam F_ADD = 4'd0, F_ADC = 4'd1, F_SUB = 4'd2, F_SBC = 4'd3, F_AND = 4'd4,
               F_OR = 4'd5, F_XOR = 4'd6, F_MOV = 4'd7, F_CMP = 4'd8, F_CPC = 4'd9,
               F_NEG = 4'd14, F_NOT = 4'd15;

    localparam M_D = 2'd0, M_X = 2'd1, M_PI = 2'd2, M_PD = 2'd3;

    function is32(input [15:0] x);
        is32 = (x[15:13] == 3'b111) || (x[15:12] == 4'b1101);
    endfunction

    // Decode through one block RAM (t1s_dec.v, t1s_rom_x*.vh, made by
    // verilog/tb/t1sgen.py from T1F's decoder): at each step the key of the
    // instruction the next step runs goes into the ROM, whose output
    // register is the decode, and the parcels into dir/dp2, which the field
    // selects read. That instruction is IR's, or, when this step starts a
    // new one (dnb), np's (a 16-bit one: p2 is not looked at). T1F has two
    // decoders and a register set each.
    reg        dnb;                // this edge starts the instruction at the queue's head
    wire [15:0] h0, h1;                // the next instruction's parcels (av, below)
    wire [15:0] kir = dnb ? h0 : ir;
    wire [15:0] kp2 = dnb ? h1 : p2;
    wire [9:0]  rkey;
    t1s_key dkey (.ir(kir), .p2(kp2), .k(rkey));
    (* rom_style = "block" *) reg [80:0] rword;
    reg  [15:0] dir, dp2;
    generate if (EXT_X) begin : rom_x1
        always @(posedge clk) if (ce) begin
`include "t1s_rom_x1.vh"
        end
    end else begin : rom_x0
        always @(posedge clk) if (ce) begin
`include "t1s_rom_x0.vh"
        end
    end endgenerate
    always @(posedge clk) if (ce) begin dir <= kir; dp2 <= kp2; end

    wire [3:0] cls, fn, ra_r, rb_r, rd_r, xr, sh_n, cc;
    wire       fsh, mp, b_imm, mld, sfill, zx, sh_reg, pstop, mw, msig, narrow, ifill, corr;
    wire [23:0] imm;
    wire [2:0] pn, sr;
    wire [1:0] msz, mmode, sc, shk, pa_src, pb_src;
    wire [7:0] tcode;
    t1s_fields dfld (.w(rword), .ir(dir), .p2(dp2), .cls(cls), .fn(fn), .ra_r(ra_r), .rb_r(rb_r), .rd_r(rd_r),
        .xr(xr), .sh_n(sh_n), .cc(cc), .fsh(fsh), .mp(mp), .b_imm(b_imm), .mld(mld), .sfill(sfill), .zx(zx),
        .sh_reg(sh_reg), .pstop(pstop), .mw(mw), .msig(msig), .narrow(narrow), .ifill(ifill), .corr(corr),
        .imm(imm), .pn(pn), .sr(sr), .msz(msz), .mmode(mmode), .sc(sc), .shk(shk), .pa_src(pa_src),
        .pb_src(pb_src), .tcode(tcode));
    wire       illegal = (cls == C_TRAP) && (tcode == 8'hFE);

    // ---------------------------------------------------------------- datapath

    // controls, set by the sequencer
    reg  [5:0] ra, rb, wa;
    reg        we, wd_a, wd_m, wd_d;       // wd_m: write the product's byte mbyte; wd_d: din (a load)
    reg  [1:0] mbyte;
    reg  [15:0] mop, mop_n;                // multiply: the operands' low bytes
    reg  [31:0] prod, prod_n;              // the product (its low 32 bits)
    function [31:0] mul17(input [16:0] x, input [16:0] y);
        mul17 = $signed(x) * $signed(y);
    endfunction
    reg        mem_rd_c, mem_wr_c;         // before the reset gate
    reg  [1:0] asel;                   // register, PC byte, zero
    reg  [2:0] bsel;                   // register, bus, immediate, zero, scaled index, SR, sign fill, -4
    reg  [2:0] aop;                    // add, and, or, xor, pass A, pass B, shifter
    reg        a_shl, shl_in, b_inv, cin;
    reg  [1:0] bk;                     // byte of the immediate or PC
    reg  [1:0] sxs, sys;               // funnel inputs X, Y: register, zero, fill
    reg  [3:0] shamt;                  // funnel position 0-8
    reg        f_bit, sbase_fill;

    localparam A_RF = 2'd0, A_PC = 2'd1, A_ZERO = 2'd2;
    localparam B_RF = 3'd0, B_DIN = 3'd1, B_IMM = 3'd2, B_ZERO = 3'd3, B_SCALE = 3'd4, B_SR = 3'd5, B_FILL = 3'd6, B_M4 = 3'd7;
    localparam OP_ADD = 3'd0, OP_AND = 3'd1, OP_OR = 3'd2, OP_XOR = 3'd3, OP_PA = 3'd4, OP_PB = 3'd5, OP_SH = 3'd6;

    wire [7:0] rfa, rfb;
    reg  [7:0] wd;

    t1s_rf rf (.clk(clk), .ce(ce), .ra(ra), .rb(rb), .qa(rfa), .qb(rfb), .we(we & ~rst & ce), .wa(wa), .wd(wd));

    wire [7:0] pc_byte = (bk == 2'd0) ? pcf[7:0] : (bk == 2'd1) ? pcf[15:8] :
                         (bk == 2'd2) ? (pcx ? 8'h10 : {{4{pcf[20]}}, pcf[19:16]}) : 8'h00;
    wire [7:0] imm_byte = (bk == 2'd0) ? imm[7:0] : (bk == 2'd1) ? imm[15:8] :
                          (bk == 2'd2) ? imm[23:16] : {8{ifill}};
    // SR1-SR3 are registered here, so MFSR reads the counters as they were
    // at the last clock edge (a clock old) and sr_in has a whole cycle
    reg  [15:0] sr_q;
    always @(posedge clk) if (ce) sr_q <= sr_in;
    wire [15:0] srv = (sr == 3'd0) ? {12'd0, fN, fZ, fC, fV} : (sr <= 3'd3) ? sr_q : 16'd0;
    assign sr_sel = sr[1:0] - 2'd1;

    // scaled index byte: rX (or its fill) times 1, 2 or 4
    wire [7:0] sbase = sbase_fill ? (zx ? 8'h00 : {8{sgn}}) : rfb;
    wire [1:0] l2in = (k == 4'd0) ? 2'b00 : l2;
    wire [7:0] scaled = (sc == 2'd0) ? sbase : (sc == 2'd1) ? {sbase[6:0], l2in[1]} : {sbase[5:0], l2in};

    reg [7:0] a_in, b_in;
    always @* begin
        case (asel)
        A_PC:    a_in = pc_byte;
        A_ZERO:  a_in = 8'h00;
        default: a_in = rfa;
        endcase
        case (bsel)
        B_IMM:   b_in = imm_byte;
        B_ZERO:  b_in = 8'h00;
        B_SCALE: b_in = scaled;
        B_SR:    b_in = bk[0] ? srv[15:8] : srv[7:0];
        B_FILL:  b_in = {8{sgn & sfill}};
        B_M4:    b_in = (bk == 2'd0) ? 8'hFC : 8'hFF;
        default: b_in = rfb;
        endcase
    end

    wire [7:0] a_eff = a_shl ? {a_in[6:0], shl_in} : a_in;
    wire [7:0] b_eff = b_inv ? ~b_in : b_in;
    wire [8:0] sum = {1'b0, a_eff} + {1'b0, b_eff} + {8'd0, cin};
    wire       cout = sum[8];
    wire       ovf = (a_eff[7] == b_eff[7]) && (sum[7] != a_eff[7]);

    // funnel: w = {X, Y, 0} >> shamt; the byte is w[8:1], w[9] is the bit
    // above it (left shifts' carry) and w[0] the bit below (right shifts')
    wire [7:0]  sh_fb = {8{f_bit}};
    wire [7:0]  sh_x = (sxs == 2'd1) ? 8'h00 : (sxs == 2'd2) ? sh_fb : rfa;
    wire [7:0]  sh_y = (sys == 2'd1) ? 8'h00 : (sys == 2'd2) ? sh_fb : rfb;
    wire [17:0] sh_w = {1'b0, sh_x, sh_y, 1'b0} >> shamt;

    reg [7:0] res;
    always @* begin
        case (aop)
        OP_AND:  res = a_eff & b_eff;
        OP_OR:   res = a_eff | b_eff;
        OP_XOR:  res = a_eff ^ b_eff;
        OP_PA:   res = a_eff;
        OP_PB:   res = b_eff;
        OP_SH:   res = sh_w[8:1];
        default: res = sum[7:0];
        endcase
        wd = wd_d ? din : wd_m ? prod[8*mbyte +: 8] : wd_a ? a_in : res;   // din goes nowhere else in the datapath
    end
    wire rz = (res == 8'h00);

    // the carry out of the low byte cannot change bytes 1 and up
    wire cstop = narrow && (cout == ifill);   // narrow: bytes 1-2 of the immediate are all ifill

    function cond(input [3:0] c, input n, input z, input cf, input v);
        case (c)
        4'd0:  cond = z;
        4'd1:  cond = !z;
        4'd2:  cond = cf;
        4'd3:  cond = !cf;
        4'd4:  cond = n;
        4'd5:  cond = !n;
        4'd6:  cond = v;
        4'd7:  cond = !v;
        4'd8:  cond = cf && !z;
        4'd9:  cond = !cf || z;
        4'd10: cond = n == v;
        4'd11: cond = n != v;
        4'd12: cond = !z && (n == v);
        4'd13: cond = z || (n != v);
        default: cond = 1'b1;
        endcase
    endfunction

    // byte j of register r, or of pair r:r+1 (r even)
    function [5:0] rbyte(input [3:0] r, input [1:0] j);
        rbyte = {1'b0, r[3:1], r[0] | j[1], j[0]};
    endfunction
    function [5:0] pbyte(input [2:0] p, input [1:0] j);
        pbyte = {1'b0, p, j};
    endfunction
    function [5:0] tbyte(input [1:0] j);                  // hidden temporaries
        tbyte = {4'b1000, j};
    endfunction

    // the bus is for data only, at AR
    reg         bus_ar;                // (unused: the bus is always at AR)
    wire [20:0] binc = ar + 21'd1;
    assign addr = {{4{ar[20]}}, ar[19:0]};
    assign dout = rfb;
    assign mem_rd = mem_rd_c & ~rst;     // nothing leaves the core while in reset
    assign mem_wr = mem_wr_c & ~rst;

    // ---------------------------------------------------------------- instruction fetch
    //
    // t1s_fetch runs at every clock, reading words from the PC on into a
    // two-word landing buffer. At each step the queue (4 parcels) gives up
    // the instruction that starts, and takes the landing buffer's first word
    // when it had room for it (2 parcels or fewer: decided in t1s_fetch from
    // qn, so that word leaves the landing buffer at the same edge). A jump,
    // or a store that may have written the queued code, empties the queue
    // and changes ep: t1s_fetch drops what it has and restarts at the PC.
    wire        l_v, l_hi;             // landing word: valid, only its high parcel (a jump to 4n+2)
    wire [31:0] l_d;
    t1s_fetch fq (.clk(clk), .rst(rst), .ce(ce), .pc(pcf), .ep(ep), .qn(qn),
        .i_req(i_req), .i_addr(i_addr), .i_ack(i_ack), .i_valid(i_valid), .i_data(i_data),
        .l_v(l_v), .l_d(l_d), .l_hi(l_hi));
    // The queue with the landing word behind it when it takes that (av, an
    // parcels): an instruction starts from there, so one whose last parcel
    // lands during a step starts at that step's end (the landing buffer's
    // paths into the core have one clock).
    wire        l_take = l_v && qn <= 3'd2;
    wire [63:0] keepq = ~(64'hFFFFFFFFFFFFFFFF << {qn, 4'd0});
    wire [63:0] lw = l_hi ? {48'd0, l_d[31:16]} : {32'd0, l_d};
    wire [63:0] av = ({q3, q2, q1, q0} & keepq) | (l_take ? (lw << {qn, 4'd0}) & ~keepq : 64'd0);
    wire [2:0]  an = qn + (l_take ? (l_hi ? 3'd1 : 3'd2) : 3'd0);
    assign h0 = av[15:0]; assign h1 = av[31:16];
    wire        whole = an != 3'd0 && (!is32(h0) || an >= 3'd2);   // the next instruction is there
    wire        end2 = !pcf[20] && pcf[19:1] == 19'h7FFFF;      // its second parcel would be at 0x100000
    wire [20:0] sdiff = ar - pcf;          // a store this close after the PC may change queued code

    // ---------------------------------------------------------------- sequencer

    reg [2:0]  state_n;
    reg [3:0]  k_n;
    reg [20:0] pcf_n, ar_n;
    reg [15:0] ir_n, p2_n;
    reg [1:0]  l2_n;
    reg        fN_n, fZ_n, fC_n, fV_n, cy_n, zacc_n, l1_n, sgn_n, cst_n, accz_n, swp_n, pcx_n;
    reg        start, flush, smc_n;
    reg [1:0]  pp;                     // parcels the starting instruction takes from the queue
    reg [4:0]  cnt_n;
    reg [2:0]  tag_n;
    reg        tagv_n, tag_set, halted_n;
    reg [20:0] epc_n;
    reg        ie_n;
    reg [7:0]  halt_code_n;
    reg        pf_ok, done, jump, bad;
    reg [7:0]  arb;                    // byte going into AR or PC bits 23:16

    wire       take = cond(cc, fN, fZ, fC, fV);
    wire       tag_hit = tagv && (tag == pn);
    wire       st_alias = !mld && mmode[1] && (rd_r[3:1] == pn);
    wire [3:0] mlast = (msz == 2'd0) ? 4'd0 : (msz == 2'd1) ? 4'd1 : 4'd3;
    wire [3:0] mreg = swp ? rb_r : ra_r;                   // multiplicand
    wire [3:0] qreg = swp ? ra_r : rb_r;                   // multiplier
    wire [3:0] n = sh_reg ? cnt[3:0] : sh_n;               // shift count
    wire       nbig = n[3] && (n[2:0] != 3'd0);            // n > 8
    wire       hf = rd_r[0] && (rd_r[3:1] == ra_r[3:1]);   // FSHx: rd is the upper register of the source pair
    wire       ph = (k == 4'd1) ? hf : ~hf;                // FSHx: result byte of this pass
    wire [1:0] fxr = 2'd1 + {1'b0, nbig}, fyr = {1'b0, nbig}, fxl = 2'd2 - {1'b0, nbig}, fyl = 2'd1 - {1'b0, nbig};
    wire [3:0] kk = corr ? {k[3], ~k[2], k[1:0]} : k;      // branch stage order
    wire [1:0] j = k[1:0] - 2'd2;                          // multiply: byte of this pass
    wire       fold_ok = (arb[7:4] == 4'h0) || (arb[7:4] == 4'hF);
    wire [4:0] fold = {arb[7:4] == 4'hF, arb[3:0]};

    always @* begin
        ra = 6'd0; rb = 6'd0; wa = 6'd0; we = 1'b0; wd_a = 1'b0;
        asel = A_RF; bsel = B_RF; aop = OP_ADD; a_shl = 1'b0; shl_in = 1'b0; b_inv = 1'b0; cin = 1'b0;
        bk = k[1:0]; mbyte = 2'd0; sxs = 2'd0; sys = 2'd0; shamt = 4'd0; f_bit = 1'b0; sbase_fill = 1'b0;
        mem_rd_c = 1'b0; mem_wr_c = 1'b0; bus_ar = 1'b0;
        state_n = state; k_n = k; pcf_n = pcf; ar_n = ar; ir_n = ir; p2_n = p2;
        fN_n = fN; fZ_n = fZ; fC_n = fC; fV_n = fV; cy_n = cy; zacc_n = zacc; l1_n = l1; l2_n = l2;
        sgn_n = sgn; cst_n = cst; cnt_n = cnt; tag_n = tag; tagv_n = tagv; tag_set = 1'b0;
        accz_n = accz; swp_n = swp; halted_n = halted; halt_code_n = halt_code;
        pf_ok = 1'b0; done = 1'b0; jump = 1'b0; bad = 1'b0; arb = 8'h00; pcx_n = pcx;
        start = 1'b0; flush = 1'b0; smc_n = smc; pp = 2'd0;
        dnb = 1'b0; wd_m = 1'b0; wd_d = 1'b0; mop_n = mop; prod_n = prod;
        epc_n = epc; ie_n = ie;

        case (state)
        // waiting for the next instruction to be whole in the queue (it is
        // started below); nothing at 0x100000
        S_FETCH: if (pcx || (an == 3'd1 && is32(h0) && end2)) bad = 1'b1;

        S_EX: case (cls)
        C_ALU: begin                                       // two byte passes
            pf_ok = 1'b1;
            ra = rbyte(ra_r, {1'b0, k[0]}); rb = rbyte(rb_r, {1'b0, k[0]});
            asel = (fn == F_NEG) ? A_ZERO : A_RF;
            bsel = b_imm ? B_IMM : B_RF;
            case (fn)
            F_AND: aop = OP_AND;
            F_OR:  aop = OP_OR;
            F_XOR: aop = OP_XOR;
            F_MOV, F_NOT: aop = OP_PB;
            default: aop = OP_ADD;
            endcase
            b_inv = (fn == F_SUB) || (fn == F_SBC) || (fn == F_CMP) || (fn == F_CPC) || (fn == F_NEG) || (fn == F_NOT);
            if (k == 4'd0) cin = (fn == F_ADC || fn == F_SBC || fn == F_CPC) ? fC : (fn == F_SUB || fn == F_CMP || fn == F_NEG);
            else cin = cy;
            we = (fn != F_CMP) && (fn != F_CPC);
            wa = rbyte(rd_r, {1'b0, k[0]});
            if (k == 4'd0) begin
                cy_n = cout; zacc_n = rz; k_n = 4'd1;
            end else begin
                done = 1'b1;
                if (fn != F_MOV) begin
                    fN_n = res[7];
                    fZ_n = zacc && rz && ((fn == F_SBC || fn == F_CPC) ? fZ : 1'b1);
                    if (aop == OP_ADD) begin fC_n = cout; fV_n = ovf; end
                    else fV_n = 1'b0;
                end
            end
        end

        // Shifts: two funnel passes. Left writes the high byte first, right
        // the low byte first, so rd may be the source.
        //   left  hi: {b1,b0} at 8-n (n <= 8), {b0,0} at 16-n    carry above
        //   left  lo: {b0,0} at 8-n (n < 8), else zero
        //   right lo: {b1,b0} at n (n <= 8), {F,b1} at n-8       carry below
        //   right hi: {F,b1} at n (n <= 8), else F
        //
        // FSHR/FSHL (extension X) are the same two passes over a pair, taking
        // result byte ph from bytes (X, Y) of the pair and writing rd, no flags:
        //   FSHR  X = byte ph+1+q, Y = byte ph+q,   shift n - 8q
        //   FSHL  X = byte ph+2-q, Y = byte ph+1-q, shift 8 - n + 8q  (16 - n if q)
        // with q = n > 8. The byte that overlaps the source goes second:
        // high first when rd is the upper register of the pair.
        C_SHF: begin
            pf_ok = 1'b1; aop = OP_SH; we = 1'b1;
            if (sh_reg && k == 4'd0) begin                 // load the count
                we = 1'b0; ra = rbyte(rb_r, 2'd0); cnt_n = {1'b0, rfa[3:0]}; k_n = 4'd1;
            end else if (fsh) begin
                ra = rbyte(ra_r, (shk[0] ? fxr : fxl) + {1'b0, ph});
                rb = rbyte(ra_r, (shk[0] ? fyr : fyl) + {1'b0, ph});
                shamt = shk[0] ? (nbig ? n - 4'd8 : n) : (nbig ? 4'd0 - n : 4'd8 - n);
                wa = rbyte(rd_r, ph);
                if (k == 4'd1) k_n = 4'd2; else done = 1'b1;
            end else if (k == {3'd0, sh_reg}) begin        // first pass
                if (shk == 2'd0) begin                     // left, high byte
                    ra = rbyte(ra_r, nbig ? 2'd0 : 2'd1); rb = rbyte(ra_r, 2'd0);
                    sys = nbig ? 2'd1 : 2'd0; shamt = nbig ? 4'd0 - n : 4'd8 - n;
                    wa = rbyte(rd_r, 2'd1); fN_n = res[7];
                    if (n != 4'd0) fC_n = sh_w[9];
                end else begin                             // right, low byte
                    ra = rbyte(ra_r, 2'd1); rb = rbyte(ra_r, nbig ? 2'd1 : 2'd0);
                    f_bit = (shk == 2'd2) && (nbig ? rfb[7] : rfa[7]);
                    sxs = nbig ? 2'd2 : 2'd0; shamt = nbig ? n - 4'd8 : n;
                    wa = rbyte(rd_r, 2'd0);
                    if (n != 4'd0) fC_n = sh_w[0];
                end
                zacc_n = rz; k_n = k + 4'd1;
            end else begin                                 // second pass
                if (shk == 2'd0) begin                     // left, low byte
                    ra = rbyte(ra_r, 2'd0);
                    sxs = n[3] ? 2'd1 : 2'd0; sys = 2'd1; shamt = 4'd8 - n;
                    wa = rbyte(rd_r, 2'd0);
                end else begin                             // right, high byte
                    rb = rbyte(ra_r, 2'd1);
                    f_bit = (shk == 2'd2) && rfb[7];
                    sxs = 2'd2; shamt = nbig ? 4'd8 : n;
                    wa = rbyte(rd_r, 2'd1); fN_n = res[7];
                end
                fZ_n = zacc && rz; fV_n = 1'b0; done = 1'b1;
            end
        end

        // Multiply on the FPGA's multiplier: the operands' low bytes, then
        // their high bytes with the product (17 x 17, signed for MULH), then
        // the product's bytes into rd: bytes 0-1 (MUL), 2-3 (MULH, MULHU), or
        // 0-3 into the pair (MULPU).
        C_MUL: begin
            pf_ok = 1'b1;
            if (k == 4'd0) begin
                ra = rbyte(ra_r, 2'd0); rb = rbyte(rb_r, 2'd0);
                mop_n = {rfa, rfb}; k_n = 4'd1;
            end else if (k == 4'd1) begin
                ra = rbyte(ra_r, 2'd1); rb = rbyte(rb_r, 2'd1);
                prod_n = mul17({msig & rfa[7], rfa, mop[15:8]}, {msig & rfb[7], rfb, mop[7:0]});
                k_n = 4'd2;
            end else begin                                 // k 2-5: write a byte
                we = 1'b1; wd_m = 1'b1;
                wa = mp ? rbyte(rd_r, k[1:0] - 2'd2) : rbyte(rd_r, {1'b0, k[0]});
                mbyte = mp ? k[1:0] - 2'd2 : {mw, k[0]};
                if (k == (mp ? 4'd5 : 4'd3)) done = 1'b1; else k_n = k + 4'd1;
            end
        end

        // A store prefetches at most one byte (one byte of the next parcel,
        // at an even address), and drops it if the store writes that address
        // (S_DATA): backing up is just clearing PC bit 0. So the next
        // instruction always sees the store, as in the simulator, with no
        // SYNC.
        // A store through [pN]+ / -[pN] whose data register lies in pN first
        // copies the data to the temporaries, since the pointer write-back
        // comes before the data cycles but the data is the old value.
        C_MEM: if (st_alias && !swp) begin
            ra = rbyte(rd_r, k[1:0]); aop = OP_PA; we = 1'b1; wa = tbyte(k[1:0]);
            if (k == mlast) begin swp_n = 1'b1; k_n = 4'd0; end else k_n = k + 4'd1;
        end else begin                                     // effective address into AR, then S_DATA
            pf_ok = mld;
            ra = pbyte(pn, k[1:0]);
            cin = (k == 4'd0) ? (mmode == M_PD) : cy; cy_n = cout;
            case (mmode)
            M_D:  bsel = B_IMM;
            M_X:  begin rb = rbyte(xr, k[1:0]); bsel = B_SCALE; sbase_fill = k[1]; l2_n = sbase[7:6]; end
            default: begin bsel = (k == 4'd0) ? B_IMM : B_ZERO; b_inv = (mmode == M_PD); we = 1'b1; wa = pbyte(pn, k[1:0]); end
            endcase
            arb = (mmode == M_PI) ? rfa : res;             // [pN]+ addresses the old value
            case (k)
            4'd0: begin
                ar_n[7:0] = arb; cst_n = (mmode == M_D) ? cstop : (mmode == M_PI) ? !cout : 1'b1;
                if (msz != 2'd0 && arb[0]) bad = 1'b1;     // odd address for a word or pair
                if (mmode == M_D && cstop && tag_hit) begin state_n = S_DATA; k_n = 4'd0; end
                else k_n = 4'd1;
            end
            4'd1: begin ar_n[15:8] = arb; if (mmode == M_X) sgn_n = rfb[7]; k_n = 4'd2; end
            4'd2: begin
                ar_n[20:16] = fold; bad = !fold_ok;
                tag_n = pn; tagv_n = cst && (mmode != M_X); tag_set = 1'b1;
                if (mmode[1]) k_n = 4'd3; else begin state_n = S_DATA; k_n = 4'd0; end
            end
            default: begin tag_set = 1'b1; state_n = S_DATA; k_n = 4'd0; end
            endcase
        end

        C_PAIR: begin                                      // pD = A + B over four bytes
            pf_ok = (pa_src != 2'd2);
            ra = rbyte(k[2] ? rd_r : ra_r, k[1:0]);
            asel = k[2] ? A_RF : (pa_src == 2'd1) ? A_ZERO : (pa_src == 2'd2) ? A_PC : A_RF;
            case (k[2] ? 2'd3 : pb_src)
            2'd0: bsel = B_IMM;
            2'd1: begin bsel = B_RF; rb = rbyte(rb_r, k[1:0]); end
            2'd2: begin bsel = B_SCALE; rb = rbyte(xr, k[1:0]); sbase_fill = k[1]; l2_n = sbase[7:6]; end
            default: bsel = k[2] ? B_M4 : B_ZERO;
            endcase
            cin = (k[1:0] == 2'd0) ? 1'b0 : cy; cy_n = cout;
            we = 1'b1; wa = rbyte(rd_r, k[1:0]);
            arb = res;
            case (k)
            4'd0: begin ar_n[7:0] = res; if (pstop && cstop) done = 1'b1; else k_n = 4'd1; end
            4'd1: begin ar_n[15:8] = res; if (pb_src == 2'd2) sgn_n = rfb[7]; k_n = 4'd2; end
            4'd2: begin ar_n[20:16] = fold; cst_n = fold_ok; k_n = 4'd3; end
            4'd3: begin
                tag_n = rd_r[3:1]; tagv_n = cst && !corr; tag_set = 1'b1;
                if (corr) k_n = 4'd4; else done = 1'b1;
            end
            default: begin                                 // LEAPC: pD -= 4
                if (cout || k == 4'd7) done = 1'b1; else k_n = k + 4'd1;
            end
            endcase
        end

        // Branches and JAL: PC += offset over three bytes. B/32 and JAL offsets
        // are from the instruction, the PC is past it, so they first take 4
        // off the PC (stage A, k = 0-2; JAL also saves the old PC as lr, with
        // byte 3 zero, in k = 3), then add the offset (stage B). Stage A cannot
        // leave the folded range, so the range check is on the final target.
        // kk is k with the stage bit swapped for corr: A is kk 4-7, B is 0-2.
        C_BR, C_JAL: begin
            asel = A_PC; cin = (kk[1:0] == 2'd0) ? 1'b0 : cy; cy_n = cout;
            bsel = kk[2] ? B_M4 : B_IMM; arb = res;
            if (cls == C_JAL && kk[2]) begin we = 1'b1; wa = 6'd24 + {4'd0, kk[1:0]}; wd_a = 1'b1; end
            if (k == 4'd0 && cls == C_BR && !take) begin
                pf_ok = 1'b1; done = 1'b1;
            end else if (kk[2]) case (kk[1:0])
            2'd0: begin pcf_n[7:0] = res; if (cout && cls != C_JAL) k_n = 4'd4; else k_n = k + 4'd1; end
            2'd1: begin pcf_n[15:8] = res; if (cout && cls != C_JAL) k_n = 4'd4; else k_n = k + 4'd1; end
            2'd2: begin pcf_n[20:16] = fold; pcx_n = 1'b0; k_n = (cls == C_JAL) ? 4'd3 : 4'd4; end
            default: begin asel = A_ZERO; k_n = 4'd4; end  // JAL: lr bits 31:24 = 0
            endcase else case (kk[1:0])
            2'd0: begin pcf_n[7:0] = res; if (!corr && cstop) jump = 1'b1; else k_n = k + 4'd1; end
            2'd1: begin pcf_n[15:8] = res; k_n = k + 4'd1; end
            default: begin pcf_n[20:16] = fold; bad = !fold_ok; jump = 1'b1; end
            endcase
        end

        C_JR, C_JALR: begin                                // PC = pN (JALR: lr = PC), a byte a cycle
            rb = pbyte(pn, k[1:0]); aop = OP_PB; arb = res;
            asel = (k == 4'd3) ? A_ZERO : A_PC; wd_a = 1'b1;
            we = (cls == C_JALR); wa = 6'd24 + {4'd0, k[1:0]};
            case (k)
            4'd0: begin pcf_n[7:0] = {res[7:1], 1'b0}; k_n = 4'd1; end
            4'd1: begin pcf_n[15:8] = res; k_n = 4'd2; end
            4'd2: begin pcf_n[20:16] = fold; bad = !fold_ok; if (cls == C_JR) jump = 1'b1; else k_n = 4'd3; end
            default: jump = 1'b1;
            endcase
        end

        C_MFSR: begin
            pf_ok = 1'b1; bsel = B_SR; aop = OP_PB; we = 1'b1; wa = rbyte(rd_r, {1'b0, k[0]});
            if (k == 4'd0) k_n = 4'd1; else done = 1'b1;
        end

        C_MTSR: begin                                      // SR0 the flags, SR6 bit 0 IE
            pf_ok = 1'b1; ra = rbyte(rd_r, 2'd0);
            if (sr == 3'd0) begin fN_n = rfa[3]; fZ_n = rfa[2]; fC_n = rfa[1]; fV_n = rfa[0]; end
            if (sr == 3'd6) ie_n = rfa[0];
            done = 1'b1;
        end

        C_NOP: begin pf_ok = 1'b1; done = 1'b1; end

        // CLZ rd, rs (extension X): rs into AR, rd = 0, then one cycle per
        // leading zero shifts AR and counts up in rd.lo (rd.lo = 16 stops it).
        C_CLZ: begin
            pf_ok = 1'b1; asel = A_ZERO; bsel = B_ZERO;
            case (k)
            4'd0: begin
                ra = rbyte(ra_r, 2'd0); rb = rbyte(ra_r, 2'd1); ar_n[7:0] = rfa; ar_n[15:8] = rfb;
                tagv_n = 1'b0; tag_set = 1'b1; we = 1'b1; wa = rbyte(rd_r, 2'd0); k_n = 4'd1;
            end
            4'd1: begin we = 1'b1; wa = rbyte(rd_r, 2'd1); k_n = 4'd2; end
            default: begin
                ra = rbyte(rd_r, 2'd0);
                if (ar[15] || rfa[4]) done = 1'b1;
                else begin
                    asel = A_RF; cin = 1'b1; we = 1'b1; wa = rbyte(rd_r, 2'd0);
                    ar_n[15:0] = {ar[14:0], 1'b0};
                end
            end
            endcase
        end

        // DSTEP pd, ps (extension X). Pass 1 (k 0-3): pd*2 + C minus ps, byte
        // by byte into the temporaries; the carry out, or the bit shifted out
        // of pd, says r >= ps. Pass 2 (k 4-7): pd = that result (temporaries)
        // or pd*2 + C again; C = the decision.
        C_DST: begin
            pf_ok = 1'b1; a_shl = 1'b1; shl_in = (k[1:0] == 2'd0) ? fC : l1;
            if (!k[2]) begin
                ra = rbyte(rd_r, k[1:0]); rb = rbyte(ra_r, k[1:0]); b_inv = 1'b1;
                cin = (k[1:0] == 2'd0) ? 1'b1 : cy; cy_n = cout; l1_n = a_in[7];
                we = 1'b1; wa = tbyte(k[1:0]);
                if (k[1:0] == 2'd3) cst_n = a_in[7] | cout;
                k_n = k + 4'd1;
            end else begin
                ra = cst ? tbyte(k[1:0]) : rbyte(rd_r, k[1:0]); a_shl = !cst; aop = OP_PA; l1_n = a_in[7];
                we = 1'b1; wa = rbyte(rd_r, k[1:0]);
                if (k[1:0] == 2'd3) begin fC_n = cst; done = 1'b1; end
                else k_n = k + 4'd1;
            end
        end

        default:
            if (cls == C_TRAP && ir == 16'hA300) begin     // RETI: back to EPC, IE set
                pcf_n = epc; ie_n = 1'b1; jump = 1'b1;
            end else begin                                 // SYS, illegal
                halted_n = 1'b1; halt_code_n = tcode; state_n = S_HALT;
            end
        endcase

        S_DATA: begin                                      // one byte per cycle at AR
            bus_ar = 1'b1;
            if (k == 4'd4) begin                           // high byte of LDBU / LDBS
                bus_ar = 1'b0; pf_ok = 1'b1;
                bsel = B_FILL; aop = OP_PB; we = 1'b1; wa = rbyte(rd_r, 2'd1); done = 1'b1;
            end else begin
                if (mld) begin
                    mem_rd_c = 1'b1; wd_d = 1'b1; we = 1'b1; wa = rbyte(rd_r, k[1:0]); sgn_n = din[7];
                end else begin
                    mem_wr_c = 1'b1; rb = swp ? tbyte(k[1:0]) : rbyte(rd_r, k[1:0]);
                    if (sdiff[20:5] == 16'd0) smc_n = 1'b1;  // at most 18 bytes are fetched past the PC
                end
                if (k == mlast) begin
                    if (mld && msz == 2'd0) k_n = 4'd4; else done = 1'b1;
                end else begin
                    ar_n = binc; k_n = k + 4'd1;
                    if (!ar[20] && binc[20]) bad = 1'b1;   // 0x0FFFFF + 1: no memory at 0x100000
                    if (ar[7:0] == 8'hFF) begin tagv_n = 1'b0; tag_set = 1'b1; end
                end
            end
        end

        default: ;                                         // S_HALT
        endcase

        // the next instruction: from the queue if it is whole there, else
        // S_FETCH waits for it. After a store that may have changed it, it
        // is fetched again (the instruction sees the store, as in fsim).
        if (done) begin
            k_n = 4'd0;
            if (smc_n) begin flush = 1'b1; state_n = S_FETCH; end
            else if (whole) start = 1'b1;
            else state_n = S_FETCH;
        end else if (state == S_FETCH && !bad && whole) start = 1'b1;
        if (start) begin
            ir_n = h0; p2_n = h1; pp = is32(h0) ? 2'd2 : 2'd1;
            pcf_n = pcf + (is32(h0) ? 21'd4 : 21'd2);
            if (!pcf[20] && pcf_n[20]) pcx_n = 1'b1;   // 0x0FFFFF + 1: no wrap into I/O
            k_n = 4'd0; state_n = S_EX;
        end
        // an interrupt instead of that instruction (IE as this step leaves it:
        // an MTSR that sets it lets one in before the next instruction). It
        // overrides the start's next state only: start, and the decode it
        // feeds (dnb, kir), stay off the interrupt's logic (their path is the
        // 50 MHz clock's longest); the decode made for it goes unused.
        if (start && ie_n && irq) begin
            epc_n = pcf; ie_n = 1'b0; pp = 2'd0;
            pcf_n = 21'h000010; pcx_n = 1'b0; flush = 1'b1; k_n = 4'd0; state_n = S_FETCH;
        end
        if (jump) begin k_n = 4'd0; flush = 1'b1; state_n = S_FETCH; pcx_n = 1'b0; end
        if (done | jump) begin swp_n = 1'b0; smc_n = 1'b0; end
        if (bad) begin halted_n = 1'b1; halt_code_n = 8'hFD; state_n = S_HALT; end
        dnb = start && !bad;

        // a write to bytes 1-2 of the tagged pair makes AR stale
        if (!tag_set && we && !wa[5] && wa[4:2] == tag && (wa[1:0] == 2'd1 || wa[1:0] == 2'd2))
            tagv_n = 1'b0;
    end

    // the queue after this step: av without the starting instruction's
    // parcels (empty after a flush)
    wire [63:0] qv_n = av >> {pp, 4'd0};
    wire [2:0]  qn_n = flush ? 3'd0 : an - {1'b0, pp};

    // reset_pc must be an even address in RAM (0-0x0FFFFF) or I/O and ROM (0xF00000 up): the core
    // cannot represent 0x100000-0xEFFFFF, so it halts with FD rather than fold it
    wire rbad = reset_pc[0] || (reset_pc[23:20] != 4'h0 && reset_pc[23:20] != 4'hF);

    always @(posedge clk) if (!ce) retire <= 1'b0;    // a stall retires nothing
    else begin
        if (rst) begin
            state <= rbad ? S_HALT : S_FETCH; k <= 4'd0; pcx <= 1'b0;
            pcf <= {reset_pc[23:20] == 4'hF, reset_pc[19:0]}; ar <= 21'd0;
            ir <= 16'd0; p2 <= 16'd0; qn <= 3'd0; ep <= 1'b0; smc <= 1'b0;
            fN <= 1'b0; fZ <= 1'b0; fC <= 1'b0; fV <= 1'b0;
            cy <= 1'b0; zacc <= 1'b0; l1 <= 1'b0; l2 <= 2'd0; sgn <= 1'b0; cst <= 1'b0; cnt <= 5'd0;
            tag <= 3'd0; tagv <= 1'b0; accz <= 1'b0; swp <= 1'b0;
            epc <= 21'd0; ie <= 1'b0;
            halted <= rbad; halt_code <= rbad ? 8'hFD : 8'd0; retire <= 1'b0;
        end else begin
            state <= state_n; k <= k_n; pcf <= pcf_n; ar <= ar_n;
            ir <= ir_n; p2 <= p2_n; qn <= qn_n; ep <= ep ^ flush; smc <= smc_n;
            {q3, q2, q1, q0} <= qv_n;
            fN <= fN_n; fZ <= fZ_n; fC <= fC_n; fV <= fV_n;
            cy <= cy_n; zacc <= zacc_n; l1 <= l1_n; l2 <= l2_n; sgn <= sgn_n; cst <= cst_n; cnt <= cnt_n;
            tag <= tag_n; tagv <= tagv_n; accz <= accz_n; swp <= swp_n; pcx <= pcx_n;
            halted <= halted_n; halt_code <= halt_code_n;
            epc <= epc_n; ie <= ie_n;
            retire <= done | jump;
            mop <= mop_n; prod <= prod_n;
        end
    end

endmodule

// T1S's register file: the 36 bytes of t1_rf.v in one block RAM. A block
// RAM reads at a clock edge, so a step's register reads are made at the
// clock before its edge (or earlier): T1S needs at least two clocks a step
// (ce at most every other clock). Port B reads ra; port A reads rb, and at
// the step's edge (ce) writes instead. Both read every clock, so qa and qb
// follow ra and rb a clock behind; a write at a step's edge is seen by the
// next step's reads.
module t1s_rf (
    input  wire       clk,
    input  wire       ce,
    input  wire [5:0] ra,
    input  wire [5:0] rb,
    output reg  [7:0] qa,
    output reg  [7:0] qb,
    input  wire       we,
    input  wire [5:0] wa,
    input  wire [7:0] wd
);
    (* ram_style = "block" *) reg [7:0] mem [0:63];
    integer i;
    initial for (i = 0; i < 64; i = i + 1) mem[i] = 8'h00;
    wire [5:0] aa = ce ? wa : rb;
    always @(posedge clk) begin
        if (we) mem[aa] <= wd;
        qb <= mem[aa];
    end
    always @(posedge clk) qa <= mem[ra];
endmodule

`default_nettype wire


// T1S's instruction fetch: words from the PC on, at every clock (not only
// at T1S's steps), into a two-word landing buffer the core's queue takes
// from at its steps. At most two words are landed or on their way. When the
// core's epoch (ep) changes, everything landed or still coming is dropped
// and fetching restarts at the core's PC (a register: it holds the jump's
// target by then; steps are at least two clocks apart, so the landing
// buffer is empty again before the core's next step looks at it). Fetching
// stops at 0x100000 (no memory there); the core faults if it gets there.
module t1s_fetch (
    input  wire        clk,
    input  wire        rst,
    input  wire        ce,
    input  wire [20:0] pc,
    input  wire        ep,
    input  wire [2:0]  qn,
    output wire        i_req,
    output wire [23:2] i_addr,
    input  wire        i_ack,
    input  wire        i_valid,
    input  wire [31:0] i_data,
    output wire        l_v,
    output wire [31:0] l_d,
    output wire        l_hi
);
    reg        fep = 1'b0;
    reg [20:2] fa = 19'd0;             // the next word to ask for (folded address)
    reg        stop = 1'b1;            // fa ran past 0x0FFFFF
    reg        skip = 1'b0;            // the next word landed: its low parcel is before the PC
    reg [1:0]  nout = 2'd0, ndrop = 2'd0;   // words asked for and not back; of those, to drop
    reg [31:0] ld0 = 32'd0, ld1 = 32'd0;
    reg        lh0 = 1'b0, lh1 = 1'b0;
    reg [1:0]  nl = 2'd0;              // words landed

    wire restart = rst || fep != ep;
    assign i_req = !restart && !stop && ({1'b0, nl} + {1'b0, nout}) < 3'd2;
    assign i_addr = {{4{fa[20]}}, fa[19:2]};
    assign l_v = nl != 2'd0;
    assign l_d = ld0;
    assign l_hi = lh0;
    wire take = ce && l_v && qn <= 3'd2;   // the core's queue takes ld0 at this edge (t1s_core: l_take)
    wire keep = i_valid && ndrop == 2'd0;
    wire [2:0] nout_n = {1'b0, nout} + {2'd0, i_ack} - {2'd0, i_valid};

    always @(posedge clk) begin
        nout <= nout_n[1:0];
        if (restart) begin
            fep <= ep; fa <= pc[20:2]; skip <= pc[1]; stop <= 1'b0;
            nl <= 2'd0; ndrop <= nout_n[1:0];          // what is still out is the old code's
        end else begin
            if (i_valid && ndrop != 2'd0) ndrop <= ndrop - 2'd1;
            if (i_ack) begin
                fa <= fa + 19'd1;
                if (!fa[20] && fa[19:2] == 18'h3FFFF) stop <= 1'b1;
            end
            if (keep) skip <= 1'b0;
            // the landing buffer: take from the front, land at the back
            case ({take, keep})
            2'b10: begin ld0 <= ld1; lh0 <= lh1; nl <= nl - 2'd1; end
            2'b01: begin
                if (nl == 2'd0) begin ld0 <= i_data; lh0 <= skip; end
                else begin ld1 <= i_data; lh1 <= skip; end
                nl <= nl + 2'd1;
            end
            2'b11: begin
                if (nl == 2'd1) begin ld0 <= i_data; lh0 <= skip; end
                else begin ld0 <= ld1; lh0 <= lh1; ld1 <= i_data; lh1 <= skip; end
            end
            default: ;
            endcase
        end
    end
endmodule
