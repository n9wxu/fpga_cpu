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

module t1f_core #(
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
    output wire [1:0]  sr_sel,       // which of SR1-SR3 MFSR is reading
    input  wire [15:0] sr_in,        // SR1-SR3 (cycle and instruction counters) live outside the core
    output reg         halted,
    output reg  [7:0]  halt_code,    // SYS imm; FF RETI, FE illegal, FD address out of range
    output reg         retire        // an instruction completed at the last clock edge
);

    // ---------------------------------------------------------------- state

    localparam S_FETCH = 3'd0, S_P2 = 3'd1, S_EX = 3'd2, S_DATA = 3'd3, S_HALT = 3'd4, S_DEC = 3'd5;

    reg [2:0]  state;
    reg [3:0]  k;              // step within the state
    reg [20:0] pcf;            // next byte to fetch (folded 21-bit address)
    reg [20:0] ar;             // data address; the multiplier during MUL
    reg [15:0] ir, p2;         // first and second parcel
    reg [15:0] nb;             // prefetched next parcel
    reg [1:0]  nbn;            // bytes in nb
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
    reg        pcx;            // the fetch pointer has run past 0x0FFFFF (reads as 0x100000: no memory there)

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

    // The decode is registered: the sequencer runs from these, decoded a
    // clock earlier from registers, never from din. One decoder takes IR and
    // P2 (an instruction that has just arrived goes through S_DEC for a
    // clock). The other takes the prefetch buffer, so an instruction already
    // whole in it at the end of the one before starts executing at once: its
    // first clock uses that decoder's registers (use_b), the rest IR's.
    reg  [3:0] cls_qi, cls_qb;
    reg  [3:0] fn_qi, fn_qb;
    reg  [3:0] ra_r_qi, ra_r_qb;
    reg  [3:0] rb_r_qi, rb_r_qb;
    reg  [3:0] rd_r_qi, rd_r_qb;
    reg  [3:0] xr_qi, xr_qb;
    reg  [3:0] sh_n_qi, sh_n_qb;
    reg  [3:0] cc_qi, cc_qb;
    reg  fsh_qi, fsh_qb;
    reg  mp_qi, mp_qb;
    reg  b_imm_qi, b_imm_qb;
    reg  mld_qi, mld_qb;
    reg  sfill_qi, sfill_qb;
    reg  zx_qi, zx_qb;
    reg  sh_reg_qi, sh_reg_qb;
    reg  pstop_qi, pstop_qb;
    reg  mw_qi, mw_qb;
    reg  msig_qi, msig_qb;
    reg  narrow_qi, narrow_qb;
    reg  ifill_qi, ifill_qb;
    reg  corr_qi, corr_qb;
    reg  [23:0] imm_qi, imm_qb;
    reg  [2:0] pn_qi, pn_qb;
    reg  [2:0] sr_qi, sr_qb;
    reg  [1:0] msz_qi, msz_qb;
    reg  [1:0] mmode_qi, mmode_qb;
    reg  [1:0] sc_qi, sc_qb;
    reg  [1:0] shk_qi, shk_qb;
    reg  [1:0] pa_src_qi, pa_src_qb;
    reg  [1:0] pb_src_qi, pb_src_qb;
    reg  [7:0] tcode_qi, tcode_qb;
    reg        use_b;
    wire [3:0] cls = use_b ? cls_qb : cls_qi;
    wire [3:0] fn = use_b ? fn_qb : fn_qi;
    wire [3:0] ra_r = use_b ? ra_r_qb : ra_r_qi;
    wire [3:0] rb_r = use_b ? rb_r_qb : rb_r_qi;
    wire [3:0] rd_r = use_b ? rd_r_qb : rd_r_qi;
    wire [3:0] xr = use_b ? xr_qb : xr_qi;
    wire [3:0] sh_n = use_b ? sh_n_qb : sh_n_qi;
    wire [3:0] cc = use_b ? cc_qb : cc_qi;
    wire fsh = use_b ? fsh_qb : fsh_qi;
    wire mp = use_b ? mp_qb : mp_qi;
    wire b_imm = use_b ? b_imm_qb : b_imm_qi;
    wire mld = use_b ? mld_qb : mld_qi;
    wire sfill = use_b ? sfill_qb : sfill_qi;
    wire zx = use_b ? zx_qb : zx_qi;
    wire sh_reg = use_b ? sh_reg_qb : sh_reg_qi;
    wire pstop = use_b ? pstop_qb : pstop_qi;
    wire mw = use_b ? mw_qb : mw_qi;
    wire msig = use_b ? msig_qb : msig_qi;
    wire narrow = use_b ? narrow_qb : narrow_qi;
    wire ifill = use_b ? ifill_qb : ifill_qi;
    wire corr = use_b ? corr_qb : corr_qi;
    wire [23:0] imm = use_b ? imm_qb : imm_qi;
    wire [2:0] pn = use_b ? pn_qb : pn_qi;
    wire [2:0] sr = use_b ? sr_qb : sr_qi;
    wire [1:0] msz = use_b ? msz_qb : msz_qi;
    wire [1:0] mmode = use_b ? mmode_qb : mmode_qi;
    wire [1:0] sc = use_b ? sc_qb : sc_qi;
    wire [1:0] shk = use_b ? shk_qb : shk_qi;
    wire [1:0] pa_src = use_b ? pa_src_qb : pa_src_qi;
    wire [1:0] pb_src = use_b ? pb_src_qb : pb_src_qi;
    wire [7:0] tcode = use_b ? tcode_qb : tcode_qi;
    wire       illegal = (cls == C_TRAP) && (tcode == 8'hFE);

    wire [3:0] cls_i;
    wire [3:0] fn_i;
    wire [3:0] ra_r_i;
    wire [3:0] rb_r_i;
    wire [3:0] rd_r_i;
    wire [3:0] xr_i;
    wire [3:0] sh_n_i;
    wire [3:0] cc_i;
    wire fsh_i;
    wire mp_i;
    wire b_imm_i;
    wire mld_i;
    wire sfill_i;
    wire zx_i;
    wire sh_reg_i;
    wire pstop_i;
    wire mw_i;
    wire msig_i;
    wire narrow_i;
    wire ifill_i;
    wire corr_i;
    wire [23:0] imm_i;
    wire [2:0] pn_i;
    wire [2:0] sr_i;
    wire [1:0] msz_i;
    wire [1:0] mmode_i;
    wire [1:0] sc_i;
    wire [1:0] shk_i;
    wire [1:0] pa_src_i;
    wire [1:0] pb_src_i;
    wire [7:0] tcode_i;
    wire [3:0] cls_b;
    wire [3:0] fn_b;
    wire [3:0] ra_r_b;
    wire [3:0] rb_r_b;
    wire [3:0] rd_r_b;
    wire [3:0] xr_b;
    wire [3:0] sh_n_b;
    wire [3:0] cc_b;
    wire fsh_b;
    wire mp_b;
    wire b_imm_b;
    wire mld_b;
    wire sfill_b;
    wire zx_b;
    wire sh_reg_b;
    wire pstop_b;
    wire mw_b;
    wire msig_b;
    wire narrow_b;
    wire ifill_b;
    wire corr_b;
    wire [23:0] imm_b;
    wire [2:0] pn_b;
    wire [2:0] sr_b;
    wire [1:0] msz_b;
    wire [1:0] mmode_b;
    wire [1:0] sc_b;
    wire [1:0] shk_b;
    wire [1:0] pa_src_b;
    wire [1:0] pb_src_b;
    wire [7:0] tcode_b;
    t1f_dec #(.EXT_X(EXT_X)) dec_i (.ir(ir), .p2(p2), .cls(cls_i), .fn(fn_i), .ra_r(ra_r_i), .rb_r(rb_r_i), .rd_r(rd_r_i), .xr(xr_i), .sh_n(sh_n_i), .cc(cc_i), .fsh(fsh_i), .mp(mp_i), .b_imm(b_imm_i), .mld(mld_i), .sfill(sfill_i), .zx(zx_i), .sh_reg(sh_reg_i), .pstop(pstop_i), .mw(mw_i), .msig(msig_i), .narrow(narrow_i), .ifill(ifill_i), .corr(corr_i), .imm(imm_i), .pn(pn_i), .sr(sr_i), .msz(msz_i), .mmode(mmode_i), .sc(sc_i), .shk(shk_i), .pa_src(pa_src_i), .pb_src(pb_src_i), .tcode(tcode_i));
    // (16-bit instructions only, so no second parcel: its logic drops out)
    t1f_dec #(.EXT_X(EXT_X)) dec_b (.ir(nb), .p2(16'd0), .cls(cls_b), .fn(fn_b), .ra_r(ra_r_b), .rb_r(rb_r_b), .rd_r(rd_r_b), .xr(xr_b), .sh_n(sh_n_b), .cc(cc_b), .fsh(fsh_b), .mp(mp_b), .b_imm(b_imm_b), .mld(mld_b), .sfill(sfill_b), .zx(zx_b), .sh_reg(sh_reg_b), .pstop(pstop_b), .mw(mw_b), .msig(msig_b), .narrow(narrow_b), .ifill(ifill_b), .corr(corr_b), .imm(imm_b), .pn(pn_b), .sr(sr_b), .msz(msz_b), .mmode(mmode_b), .sc(sc_b), .shk(shk_b), .pa_src(pa_src_b), .pb_src(pb_src_b), .tcode(tcode_b));
    reg        dnb;                // this edge starts the instruction in NB

    always @(posedge clk) if (ce) begin
        cls_qi <= cls_i; cls_qb <= cls_b;
        fn_qi <= fn_i; fn_qb <= fn_b;
        ra_r_qi <= ra_r_i; ra_r_qb <= ra_r_b;
        rb_r_qi <= rb_r_i; rb_r_qb <= rb_r_b;
        rd_r_qi <= rd_r_i; rd_r_qb <= rd_r_b;
        xr_qi <= xr_i; xr_qb <= xr_b;
        sh_n_qi <= sh_n_i; sh_n_qb <= sh_n_b;
        cc_qi <= cc_i; cc_qb <= cc_b;
        fsh_qi <= fsh_i; fsh_qb <= fsh_b;
        mp_qi <= mp_i; mp_qb <= mp_b;
        b_imm_qi <= b_imm_i; b_imm_qb <= b_imm_b;
        mld_qi <= mld_i; mld_qb <= mld_b;
        sfill_qi <= sfill_i; sfill_qb <= sfill_b;
        zx_qi <= zx_i; zx_qb <= zx_b;
        sh_reg_qi <= sh_reg_i; sh_reg_qb <= sh_reg_b;
        pstop_qi <= pstop_i; pstop_qb <= pstop_b;
        mw_qi <= mw_i; mw_qb <= mw_b;
        msig_qi <= msig_i; msig_qb <= msig_b;
        narrow_qi <= narrow_i; narrow_qb <= narrow_b;
        ifill_qi <= ifill_i; ifill_qb <= ifill_b;
        corr_qi <= corr_i; corr_qb <= corr_b;
        imm_qi <= imm_i; imm_qb <= imm_b;
        pn_qi <= pn_i; pn_qb <= pn_b;
        sr_qi <= sr_i; sr_qb <= sr_b;
        msz_qi <= msz_i; msz_qb <= msz_b;
        mmode_qi <= mmode_i; mmode_qb <= mmode_b;
        sc_qi <= sc_i; sc_qb <= sc_b;
        shk_qi <= shk_i; shk_qb <= shk_b;
        pa_src_qi <= pa_src_i; pa_src_qb <= pa_src_b;
        pb_src_qi <= pb_src_i; pb_src_qb <= pb_src_b;
        tcode_qi <= tcode_i; tcode_qb <= tcode_b;
        use_b <= dnb;
    end

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

    t1_rf rf (.clk(clk), .ra(ra), .rb(rb), .qa(rfa), .qb(rfb), .we(we & ~rst & ce), .wa(wa), .wd(wd));

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

    // bus address: PC (fetch) or AR (data), and the shared incrementer
    reg         bus_ar;
    wire [20:0] baddr = bus_ar ? ar : pcf;
    wire [20:0] binc = baddr + 21'd1;
    assign addr = {{4{baddr[20]}}, baddr[19:0]};
    assign dout = rfb;
    assign mem_rd = mem_rd_c & ~rst;     // nothing leaves the core while in reset
    assign mem_wr = mem_wr_c & ~rst;

    // ---------------------------------------------------------------- sequencer

    reg [2:0]  state_n;
    reg [3:0]  k_n;
    reg [20:0] pcf_n, ar_n;
    reg [15:0] ir_n, p2_n, nb_n;
    reg [1:0]  nbn_n, l2_n;
    reg        fN_n, fZ_n, fC_n, fV_n, cy_n, zacc_n, l1_n, sgn_n, cst_n, accz_n, swp_n, pcx_n, pinc;
    reg [4:0]  cnt_n;
    reg [2:0]  tag_n;
    reg        tagv_n, tag_set, halted_n;
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
        state_n = state; k_n = k; pcf_n = pcf; ar_n = ar; ir_n = ir; p2_n = p2; nb_n = nb; nbn_n = nbn;
        fN_n = fN; fZ_n = fZ; fC_n = fC; fV_n = fV; cy_n = cy; zacc_n = zacc; l1_n = l1; l2_n = l2;
        sgn_n = sgn; cst_n = cst; cnt_n = cnt; tag_n = tag; tagv_n = tagv; tag_set = 1'b0;
        accz_n = accz; swp_n = swp; halted_n = halted; halt_code_n = halt_code;
        pf_ok = 1'b0; done = 1'b0; jump = 1'b0; bad = 1'b0; arb = 8'h00; pcx_n = pcx; pinc = 1'b0;
        dnb = 1'b0; wd_m = 1'b0; wd_d = 1'b0; mop_n = mop; prod_n = prod;

        case (state)
        S_FETCH: if (pcx) bad = 1'b1;                      // fetch a parcel into IR (nothing at 0x100000)
        else begin
            mem_rd_c = 1'b1; pcf_n = binc; pinc = 1'b1;
            if (nbn == 2'd0) begin
                nb_n[7:0] = din; nbn_n = 2'd1;
            end else begin
                ir_n = {din, nb[7:0]}; nbn_n = 2'd0; k_n = 4'd0;
                state_n = S_DEC;                           // S_DEC looks at IR (not din) for a second parcel
            end
        end

        S_P2: if (pcx) bad = 1'b1;                         // second parcel
        else begin
            mem_rd_c = 1'b1; pcf_n = binc; pinc = 1'b1;
            if (k == 4'd0) begin p2_n[7:0] = din; k_n = 4'd1; end
            else begin p2_n[15:8] = din; k_n = 4'd1; state_n = S_DEC; end   // k 1: the parcels are in
        end

        // The decode registers load. Coming from the first parcel (k 0) of a
        // 32-bit instruction, this clock fetches the second parcel's low byte
        // (S_P2 takes the high byte and comes back). No prefetch here:
        // branches, JAL and LEAPC take the PC (the fetch pointer) as the next
        // instruction's.
        S_DEC: if (k == 4'd0 && is32(ir)) begin
            if (pcx) bad = 1'b1;
            else begin mem_rd_c = 1'b1; pcf_n = binc; pinc = 1'b1; p2_n[7:0] = din; k_n = 4'd1; state_n = S_P2; end
        end else begin k_n = 4'd0; state_n = S_EX; end

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
            pf_ok = mld || nbn == 2'd0;
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

        C_MTSR: begin
            pf_ok = 1'b1; ra = rbyte(rd_r, 2'd0);
            if (sr == 3'd0) begin fN_n = rfa[3]; fZ_n = rfa[2]; fC_n = rfa[1]; fV_n = rfa[0]; end
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

        default: begin halted_n = 1'b1; halt_code_n = tcode; state_n = S_HALT; end   // SYS, RETI, illegal
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
                    if (nbn == 2'd1 && ar[7:1] == pcf[7:1]) begin nbn_n = 2'd0; pcf_n[0] = 1'b0; end   // wrote the prefetched byte (low bits only: may drop a good one)
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

        // prefetch the next parcel when the bus is free
        if (pf_ok && !pcx && !mem_rd_c && !mem_wr_c && nbn != 2'd2) begin
            mem_rd_c = 1'b1; bus_ar = 1'b0; pcf_n = binc; pinc = 1'b1;
            if (nbn == 2'd0) nb_n[7:0] = din; else nb_n[15:8] = din;
            nbn_n = nbn + 2'd1;
        end

        if (done) begin
            k_n = 4'd0;
            if (nbn_n == 2'd2) begin
                ir_n = nb_n; nbn_n = 2'd0;
                state_n = (nbn != 2'd2) ? S_DEC : is32(nb) ? S_P2 : S_EX;   // not from din: S_DEC sees it
            end else
                state_n = S_FETCH;
        end
        if (jump) begin k_n = 4'd0; nbn_n = 2'd0; state_n = S_FETCH; pcx_n = 1'b0; end
        else if (pinc && !pcf[20] && binc[20]) pcx_n = 1'b1;   // 0x0FFFFF + 1: no wrap into I/O
        if (done | jump) swp_n = 1'b0;
        if (bad) begin halted_n = 1'b1; halt_code_n = 8'hFD; state_n = S_HALT; end
        dnb = state_n == S_EX && done && !jump && !bad;   // NB was whole: decoded from NB

        // a write to bytes 1-2 of the tagged pair makes AR stale
        if (!tag_set && we && !wa[5] && wa[4:2] == tag && (wa[1:0] == 2'd1 || wa[1:0] == 2'd2))
            tagv_n = 1'b0;
    end

    // reset_pc must be an even address in RAM (0-0x0FFFFF) or I/O and ROM (0xF00000 up): the core
    // cannot represent 0x100000-0xEFFFFF, so it halts with FD rather than fold it
    wire rbad = reset_pc[0] || (reset_pc[23:20] != 4'h0 && reset_pc[23:20] != 4'hF);

    always @(posedge clk) if (!ce) retire <= 1'b0;    // a stall retires nothing
    else begin
        if (rst) begin
            state <= rbad ? S_HALT : S_FETCH; k <= 4'd0; pcx <= 1'b0;
            pcf <= {reset_pc[23:20] == 4'hF, reset_pc[19:0]}; ar <= 21'd0;
            ir <= 16'd0; p2 <= 16'd0; nb <= 16'd0; nbn <= 2'd0;
            fN <= 1'b0; fZ <= 1'b0; fC <= 1'b0; fV <= 1'b0;
            cy <= 1'b0; zacc <= 1'b0; l1 <= 1'b0; l2 <= 2'd0; sgn <= 1'b0; cst <= 1'b0; cnt <= 5'd0;
            tag <= 3'd0; tagv <= 1'b0; accz <= 1'b0; swp <= 1'b0;
            halted <= rbad; halt_code <= rbad ? 8'hFD : 8'd0; retire <= 1'b0;
        end else begin
            state <= state_n; k <= k_n; pcf <= pcf_n; ar <= ar_n;
            ir <= ir_n; p2 <= p2_n; nb <= nb_n; nbn <= nbn_n;
            fN <= fN_n; fZ <= fZ_n; fC <= fC_n; fV <= fV_n;
            cy <= cy_n; zacc <= zacc_n; l1 <= l1_n; l2 <= l2_n; sgn <= sgn_n; cst <= cst_n; cnt <= cnt_n;
            tag <= tag_n; tagv <= tagv_n; accz <= accz_n; swp <= swp_n; pcx <= pcx_n;
            halted <= halted_n; halt_code <= halt_code_n;
            retire <= done | jump;
            mop <= mop_n; prod <= prod_n;
        end
    end

endmodule

`default_nettype wire

// T1F's instruction decoder: the parcels to the controls of the sequencer
// (t1_core.v's decode, unchanged, as a module so the core can have two).
module t1f_dec #(
    parameter EXT_X = 1
) (
    input  wire [15:0] ir,
    input  wire [15:0] p2,
    output reg  [3:0] cls,
    output reg  [3:0] fn,
    output reg  [3:0] ra_r,
    output reg  [3:0] rb_r,
    output reg  [3:0] rd_r,
    output reg  [3:0] xr,
    output reg  [3:0] sh_n,
    output reg  [3:0] cc,
    output reg  fsh,
    output reg  mp,
    output reg  b_imm,
    output reg  mld,
    output reg  sfill,
    output reg  zx,
    output reg  sh_reg,
    output reg  pstop,
    output reg  mw,
    output reg  msig,
    output reg  narrow,
    output reg  ifill,
    output reg  corr,
    output reg  [23:0] imm,
    output reg  [2:0] pn,
    output reg  [2:0] sr,
    output reg  [1:0] msz,
    output reg  [1:0] mmode,
    output reg  [1:0] sc,
    output reg  [1:0] shk,
    output reg  [1:0] pa_src,
    output reg  [1:0] pb_src,
    output reg  [7:0] tcode
);
    localparam C_ALU = 4'd0, C_SHF = 4'd1, C_MUL = 4'd2, C_MEM = 4'd3, C_PAIR = 4'd4,
               C_BR = 4'd5, C_JAL = 4'd6, C_JR = 4'd7, C_JALR = 4'd8, C_MFSR = 4'd9,
               C_MTSR = 4'd10, C_NOP = 4'd11, C_TRAP = 4'd12, C_CLZ = 4'd13, C_DST = 4'd14;

    // ALUFN, isa.yaml tables.ALUFN
    localparam F_ADD = 4'd0, F_ADC = 4'd1, F_SUB = 4'd2, F_SBC = 4'd3, F_AND = 4'd4,
               F_OR = 4'd5, F_XOR = 4'd6, F_MOV = 4'd7, F_CMP = 4'd8, F_CPC = 4'd9,
               F_NEG = 4'd14, F_NOT = 4'd15;

    localparam M_D = 2'd0, M_X = 2'd1, M_PI = 2'd2, M_PD = 2'd3;


    // B/16 offset relative to the next instruction: 2*off8 - 2 = 2*(off8 - 1)
    wire [8:0] boff = {ir[7], ir[7:0]} - 9'd1;
    reg        ill;

    always @* begin
        cls = C_TRAP; tcode = 8'hFE; ill = 1'b0; fsh = 1'b0; mp = 1'b0;
        fn = F_ADD; ra_r = 4'd0; rb_r = 4'd0; rd_r = 4'd0; xr = 4'd0; sh_n = 4'd0; cc = 4'd14;
        b_imm = 1'b0; mld = 1'b0; sfill = 1'b0; zx = 1'b0; sh_reg = 1'b0; pstop = 1'b0;
        mw = 1'b0; msig = 1'b0; narrow = 1'b0; ifill = 1'b0; corr = 1'b0;
        imm = 24'd0; pn = 3'd0; sr = 3'd0;
        msz = 2'd0; mmode = M_D; sc = 2'd0; shk = 2'd0; pa_src = 2'd0; pb_src = 2'd0;
        // ill: a reserved bit or field value is set. Every form checks all of
        // its fixed bits, and pair fields (2*N) must be even.
        case (ir[15:12])
        4'h0: begin                                        // ALU2: rd = rd op rs
            fn = ir[3:0]; ra_r = ir[11:8]; rb_r = ir[7:4]; rd_r = ir[11:8];
            if (ir[3:0] >= 4'd10 && ir[3:0] <= 4'd12) begin cls = C_SHF; shk = ir[1:0] - 2'd2; sh_reg = 1'b1; end
            else if (ir[3:0] == 4'd13) cls = C_MUL;
            else cls = C_ALU;
        end
        4'h1, 4'h2, 4'h3: begin                            // ADDI / MOVI / CMPI imm8
            cls = C_ALU; fn = (ir[13:12] == 2'd1) ? F_ADD : (ir[13:12] == 2'd2) ? F_MOV : F_CMP;
            ra_r = ir[11:8]; rd_r = ir[11:8]; b_imm = 1'b1; imm = {{16{ir[7]}}, ir[7:0]};
        end
        4'h4, 4'h5: begin                                  // LDW/STW [pN + disp5*2]
            cls = C_MEM; mld = ~ir[12]; msz = 2'd1; rd_r = ir[11:8]; pn = ir[7:5];
            imm = {18'd0, ir[4:0], 1'b0}; narrow = 1'b1;
        end
        4'h6: begin                                        // LDBU/STB [pN + disp4]
            cls = C_MEM; mld = ~ir[4]; rd_r = ir[11:8]; pn = ir[7:5]; imm = {20'd0, ir[3:0]}; narrow = 1'b1;
        end
        4'h7: begin                                        // LDP/STP [pN + disp5*2]
            cls = C_MEM; mld = ~ir[5]; msz = 2'd2; rd_r = {ir[11:9], 1'b0}; pn = ir[8:6];
            imm = {18'd0, ir[4:0], 1'b0}; narrow = 1'b1;
        end
        4'h8: begin                                        // LDW/STW [sp + disp7*2]
            cls = C_MEM; mld = ~ir[7]; msz = 2'd1; rd_r = ir[11:8]; pn = 3'd7;
            imm = {16'd0, ir[6:0], 1'b0}; narrow = 1'b1;
        end
        4'h9: begin                                        // Bcc off8 (cc 15 is reserved)
            cls = (ir[11:8] == 4'd15) ? C_TRAP : C_BR; cc = ir[11:8];
            imm = {{8{boff[8]}}, {7{boff[8]}}, boff[7], boff[6:0], 1'b0};
            ifill = boff[8]; narrow = (boff[8] == boff[7]);
        end
        4'hA: case (ir[11:8])                              // MISC
            4'h0: begin cls = C_JR; pn = ir[2:0]; ill = |ir[7:3]; end
            4'h1: begin cls = C_JALR; pn = ir[2:0]; ill = |ir[7:3]; end
            4'h2: tcode = ir[7:0];                         // SYS
            4'h3: if (ir[7:0] == 8'h00) tcode = 8'hFF;     // RETI: no interrupts yet
                  else if (ir[7:0] == 8'h01 || ir[7:0] == 8'h02) cls = C_NOP;   // SYNC, NOP
            4'h5: begin cls = C_PAIR; rd_r = {ir[6:4], 1'b0}; ra_r = {ir[2:0], 1'b0}; pb_src = 2'd3; ill = ir[7] | ir[3]; end  // MOVP
            4'h6: begin cls = C_PAIR; rd_r = {ir[6:4], 1'b0}; ra_r = {ir[6:4], 1'b0}; pb_src = 2'd2; xr = ir[3:0]; ill = ir[7]; end  // ADDP
            4'h7, 4'h8, 4'h9: begin                        // SHLI / SHRI / SARI imm4
                cls = C_SHF; shk = ir[9:8] + 2'd1; sh_n = ir[3:0]; ra_r = ir[7:4]; rd_r = ir[7:4];
            end
            4'hB, 4'hC, 4'hD, 4'hE: begin                  // LDW [pN]+, STW -[pN], LDBU [pN]+, STB [pN]+
                cls = C_MEM; mld = ir[8]; mmode = (ir[11:8] == 4'hC) ? M_PD : M_PI;
                msz = {1'b0, ir[11:8] == 4'hB || ir[11:8] == 4'hC}; imm = msz[0] ? 24'd2 : 24'd1;
                rd_r = ir[7:4]; pn = ir[2:0]; ill = ir[3];
            end
            4'hF: begin cls = ir[3] ? C_MTSR : C_MFSR; rd_r = ir[7:4]; sr = ir[2:0]; end
            default: ;
            endcase
        4'hB: begin                                        // LDW/STW [pN + rX*2]
            cls = C_MEM; mld = ~ir[4]; msz = 2'd1; mmode = M_X; rd_r = ir[11:8]; pn = ir[7:5]; xr = ir[3:0]; sc = 2'd1;
        end
        4'hC: begin                                        // ADDPI pD, imm9
            cls = C_PAIR; rd_r = {ir[11:9], 1'b0}; ra_r = {ir[11:9], 1'b0};
            imm = {{15{ir[8]}}, ir[8:0]}; ifill = ir[8]; narrow = 1'b1; pstop = 1'b1;
        end
        4'hD: begin                                        // LEA pD, [abs24] / LEAPC
            cls = C_PAIR; rd_r = {ir[11:9], 1'b0}; imm = {ir[7:0], p2};
            if (ir[8]) begin pa_src = 2'd2; ifill = ir[7]; corr = 1'b1; end
            else pa_src = 2'd1;
        end
        4'hE: if (!ir[11]) begin                           // MEM, 32-bit
            rd_r = ir[7:4]; pn = ir[3:1]; ra_r = {ir[3:1], 1'b0};
            imm = {{8{p2[15]}}, p2}; ifill = p2[15];
            ill = ir[4] && (ir[10:8] == 3'd3 || ir[10:8] >= 3'd6);        // LDP, STP, LEA name a pair
            if (ir[10:8] == 3'd7) begin                    // LEA pD, [pN + disp16] / [pN + rX*s]
                cls = C_PAIR;
                if (ir[0]) begin
                    pb_src = 2'd2; xr = p2[15:12]; sc = p2[9:8]; zx = p2[10];
                    ill = ill | p2[11] | (p2[9:8] == 2'd3) | (|p2[7:0]);  // no pi/pd, scale code 3 reserved
                end
            end else begin
                cls = C_MEM; mld = ~ir[10]; sfill = (ir[10:8] == 3'd1);
                msz = ir[10] ? ir[9:8] : (ir[9:8] == 2'd3) ? 2'd2 : {1'b0, ir[9]};
                if (ir[0]) begin
                    if (p2[11:8] == 4'h8 || p2[11:8] == 4'h9) begin
                        mmode = p2[8] ? M_PD : M_PI;
                        imm = (msz == 2'd0) ? 24'd1 : (msz == 2'd1) ? 24'd2 : 24'd4; ifill = 1'b0;
                        ill = ill | (|p2[15:12]) | (|p2[7:0]);
                    end else begin
                        mmode = M_X; xr = p2[15:12]; sc = p2[9:8]; zx = p2[10];
                        ill = ill | p2[11] | (p2[9:8] == 2'd3) | (|p2[7:0]);
                    end
                end
            end
        end else begin                                     // ALUI: rd = rs op imm16
            rd_r = ir[7:4]; ra_r = ir[3:0]; b_imm = 1'b1; imm = {8'd0, p2};
            case (ir[10:8])
            3'd0: begin cls = C_ALU; fn = F_ADD; end
            3'd1: begin cls = C_ALU; fn = F_AND; end
            3'd2: begin cls = C_ALU; fn = F_OR;  end
            3'd3: begin cls = C_ALU; fn = F_XOR; end
            3'd4: begin cls = C_ALU; fn = F_CMP; ill = |ir[7:4]; end
            3'd5: begin cls = C_ALU; fn = F_MOV; ill = |ir[3:0]; end
            3'd6: begin cls = C_PAIR; imm = {{8{p2[15]}}, p2}; ifill = p2[15]; ill = ir[4] | ir[0]; end   // ADDPI pD, pS, imm16
            default: ;
            endcase
        end
        4'hF: if (!ir[11]) begin                           // Bcc off23 / JAL: 2*off23, then -4
            cls = (ir[10:7] == 4'd15) ? C_JAL : C_BR; cc = ir[10:7];
            imm = {ir[6:0], p2, 1'b0}; corr = 1'b1;
        end else begin                                     // REG3
            rd_r = ir[7:4]; ra_r = ir[3:0]; rb_r = p2[15:12];
            case (ir[10:8])
            3'd0: begin
                fn = p2[11:8]; ill = |p2[7:0];
                if (p2[11:8] >= 4'd10 && p2[11:8] <= 4'd12) begin cls = C_SHF; shk = p2[9:8] - 2'd2; sh_reg = 1'b1; end
                else if (p2[11:8] == 4'd13) cls = C_MUL;
                else begin
                    cls = C_ALU;
                    if (p2[11:8] == F_MOV || p2[11:8] == F_NEG || p2[11:8] == F_NOT) begin rb_r = ir[3:0]; ill = ill | (|p2[15:12]); end
                    if (p2[11:8] == F_CMP || p2[11:8] == F_CPC) ill = ill | (|ir[7:4]);
                end
            end
            3'd1: if (p2[11:8] <= 4'd2) begin cls = C_SHF; shk = p2[9:8]; sh_n = p2[3:0]; ill = (|p2[15:12]) | (|p2[7:4]); end
            3'd2: if (p2[11:8] <= 4'd2) begin              // ADDP / ADDPU / ADDPP
                cls = C_PAIR; ill = ir[4] | ir[0] | (|p2[7:0]);
                if (p2[9]) begin pb_src = 2'd1; ill = ill | p2[12]; end
                else begin pb_src = 2'd2; xr = p2[15:12]; zx = p2[8]; end
            end
            3'd3: if (p2[11:10] == 2'd0 && p2[9:8] != 2'd3) begin    // MULH, MULHU, MULPU
                cls = C_MUL; mw = 1'b1; msig = ~p2[9] & ~p2[8]; sfill = ~p2[9] & ~p2[8]; ill = |p2[7:0];
                if (p2[9]) begin mp = 1'b1; ill = ill | ir[4]; if (!EXT_X) cls = C_TRAP; end
            end
            3'd4: if (EXT_X && p2[11:9] == 3'd0) begin     // FSHR, FSHL
                cls = C_SHF; fsh = 1'b1; sh_reg = 1'b1; shk = {1'b0, ~p2[8]}; ill = (|p2[7:0]) | ir[0];
            end
            3'd5: if (EXT_X && p2[11:9] == 3'd0) begin     // CLZ, DSTEP
                if (p2[8]) begin cls = C_DST; ill = (|p2[15:12]) | (|p2[7:0]) | ir[4] | ir[0]; end
                else begin cls = C_CLZ; ill = (|p2[15:12]) | (|p2[7:0]); end
            end
            default: ;
            endcase
        end
        default: ;
        endcase
        if (ill) begin cls = C_TRAP; tcode = 8'hFE; end
    end
endmodule
