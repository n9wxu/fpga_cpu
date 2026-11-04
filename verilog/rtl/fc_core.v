// FC: the pipelined fpga_cpu core for FPGAs (docs/fc.md): today's ISA
// (cpu/isa.yaml, encoding variant A) on a 16-bit datapath, one instruction
// a clock, built from what an FPGA has: distributed RAM for the registers,
// carry chains for the adders, one hard multiplier, block RAM behind the
// memory ports. It is the c0 rung of docs/cores.md, and its timing is
// fsim's --core c0 model: a load's result one clock late, a taken branch
// two clocks, data through a 32-bit port.
//
// Pipeline:
//   F  fetch: a queue of 16-bit parcels, filled from the instruction port
//      (32-bit words, requests pipelined, responses in order)
//   D  decode (t1_dec.v): the registers to read, the hazard check
//   X  register read (three pair-wide ports), execute: 16-bit ALU, funnel
//      shifter, 17x17 multiplier, 32-bit
//      pointer/address adder, branch and jump resolution, faults; issues
//      the data port request
//   M  memory: the data port's response
//   W  writeback: two banked register writes a clock (one per bank); an
//      instruction that writes a register and a pointer pair (a load
//      through [pN]+ or -[pN]) takes a second clock here
//
// Registers: pair p is registers 2p (low half, the even bank) and 2p+1
// (high half, the odd bank); a pair reads or writes both banks at one
// index, a register one bank. X reads the register file (distributed RAM,
// asynchronous) and takes what M and W are about to write, forwarded. A
// load's result reaches the next instruction one clock late.
//
// Memory ports (both synchronous): a request (req, addr) is taken at a
// clock edge when gnt is high; read data comes back in order with rvalid,
// one or more clocks later. The data port's byte enables (be) and wdata
// are for writes (we); a write has no response.
//
// Faults, as fsim (and T1): halted with halt_code FD for an odd address in
// a word or pair access, a data address or jump target in
// 0x100000-0xEFFFFF, executing a parcel fetched from there, and a bad
// reset_pc; reserved encodings FE; SYS n, RETI FF.

`timescale 1ns / 1ns
`default_nettype none

module fc_core #(
    parameter EXT_X = 1
) (
    input  wire        clk,
    input  wire        rst,             // synchronous, active high
    input  wire [23:0] reset_pc,
    // instruction port
    output wire        i_req,
    output wire [23:2] i_addr,
    input  wire        i_gnt,
    input  wire        i_rvalid,
    input  wire [31:0] i_rdata,
    output wire        i_flush,         // fetch restarts this clock (a branch, a jump, SYNC, a store into code):
                                        // a prefetch buffer behind the port drops what it holds
    // data port
    output wire        d_req,
    output wire        d_we,
    output wire [23:0] d_addr,          // byte address (the word is d_addr[23:2])
    output wire [3:0]  d_be,
    output wire [31:0] d_wdata,
    input  wire        d_gnt,
    input  wire        d_rvalid,
    input  wire [31:0] d_rdata,
    // SR1-SR3 (cycle and instruction counters) live outside the core
    output wire [1:0]  sr_sel,
    input  wire [15:0] sr_in,
    output reg         halted,
    output reg  [7:0]  halt_code,
    output reg         retire           // an instruction completed at the last clock edge
);
    localparam C_ALU = 4'd0, C_SHF = 4'd1, C_MUL = 4'd2, C_MEM = 4'd3, C_PAIR = 4'd4,
               C_BR = 4'd5, C_JAL = 4'd6, C_JR = 4'd7, C_JALR = 4'd8, C_MFSR = 4'd9,
               C_MTSR = 4'd10, C_NOP = 4'd11, C_TRAP = 4'd12, C_CLZ = 4'd13, C_DST = 4'd14;
    localparam F_ADD = 4'd0, F_ADC = 4'd1, F_SUB = 4'd2, F_SBC = 4'd3, F_AND = 4'd4,
               F_OR = 4'd5, F_XOR = 4'd6, F_MOV = 4'd7, F_CMP = 4'd8, F_CPC = 4'd9,
               F_NEG = 4'd14, F_NOT = 4'd15;
    localparam M_D = 2'd0, M_X = 2'd1, M_PI = 2'd2, M_PD = 2'd3;

    function is32(input [15:0] x);
        is32 = (x[15:13] == 3'b111) || (x[15:12] == 4'b1101);
    endfunction
    // an address the core may use: RAM (0x000000-0x0FFFFF) or I/O and ROM (0xF00000 up)
    function okaddr(input [23:0] a);
        okaddr = (a[23:20] == 4'h0) || (a[23:20] == 4'hF);
    endfunction

    // ================================================================ stall control
    wire w_hold;                        // W: second clock of a two-write instruction
    wire m_hold;                        // M: waiting for read data
    wire x_hold;                        // X: waiting for the data port, or a split access
    wire d_hazard;                      // D: needs a load's result that is not there yet
    wire adv_m = !w_hold;               // M's contents move to W
    wire adv_x = adv_m && !m_hold;      // X's move to M
    wire adv_d = adv_x && !x_hold;      // D's move to X (or X takes a bubble)

    // redirect from X: a taken branch or jump, SYNC, a store into fetched code, a halt
    reg         redir;
    reg  [23:0] redir_pc;
    assign i_flush = redir;
    reg         halt_go;                // X ends the program (a fault or trap)
    reg         stopping;               // nothing more is fetched or decoded
    reg         halt_pend;              // a fault or trap left X: halt when the rest has retired

    // ================================================================ F: fetch
    // Two words of buffer (fw0, fw1) and a head pointer: the next parcel is
    // fw0's low (hp 0) or high half (hp 1). A word arriving this clock is seen
    // by D at once (the view vw0/vw1). ff* marks a word from an address with
    // no memory: executing a parcel of it faults.
    reg  [31:0] fw0, fw1;
    reg         fv0, fv1, ff0, ff1, hp;
    reg  [23:0] qpc;                    // the head parcel's address
    reg  [21:0] fa;                     // the next word to request
    reg  [1:0]  ofl;                    // requests out (accepted, no response yet)
    reg  [1:0]  kill;                   // responses still to come for a stream redirected away

    wire        fa_bad = !okaddr({fa, 2'b00});
    wire [1:0]  f_used = {1'b0, fv0} + {1'b0, fv1} + ofl;
    wire        f_want = !stopping && !redir && f_used < 2'd2;
    assign i_req  = f_want && !fa_bad;
    assign i_addr = fa;
    wire        f_take = i_req && i_gnt;
    wire        f_fake = f_want && fa_bad && ofl == 2'd0;   // no memory there: a fault word, in order
    wire        r_keep = i_rvalid && kill == 2'd0;
    wire        in_v = r_keep || f_fake;
    wire [31:0] in_w = r_keep ? i_rdata : 32'h0;
    wire        in_f = !r_keep;

    // what D sees: the buffer, then the arriving word
    wire        vv0 = fv0 | in_v, vv1 = fv1 | (fv0 & in_v);
    wire [31:0] vw0 = fv0 ? fw0 : in_w, vw1 = fv1 ? fw1 : in_w;
    wire        vf0 = fv0 ? ff0 : in_f, vf1 = fv1 ? ff1 : in_f;
    wire [15:0] p0 = hp ? vw0[31:16] : vw0[15:0];
    wire [15:0] p1 = hp ? vw1[15:0] : vw0[31:16];
    wire        a1 = hp ? vv1 : vv0;    // a second parcel is there
    wire        pf1 = hp ? vf1 : vf0;

    // ================================================================ D: decode and register read
    reg         d_valid;
    reg  [15:0] d_ir, d_p2;
    reg  [23:0] d_pc;
    reg         d_len4, d_bad;

    // the next instruction for D
    wire        nx_32 = is32(p0);
    wire        nx_ok = !stopping && !redir && vv0 && (!nx_32 || a1);
    wire        d_take = adv_d && !d_hazard || !d_valid;    // D is free to load this clock
    wire        d_load = d_take && nx_ok;
    wire [1:0]  d_used = d_load ? (nx_32 ? 2'd2 : 2'd1) : 2'd0;

    wire [3:0]  cls, fn, ra_r, rb_r, rd_r, xr, sh_n, cc;
    wire        fsh, mp, b_imm, mld, sfill, zx, sh_reg, pstop, mw, msig, narrow, ifill, corr;
    wire [23:0] imm;
    wire [2:0]  pn, sr;
    wire [1:0]  msz, mmode, sc, shk, pa_src, pb_src;
    wire [7:0]  tcode;
    t1f_dec #(.EXT_X(EXT_X)) dec (.ir(d_ir), .p2(d_p2),
        .cls(cls), .fn(fn), .ra_r(ra_r), .rb_r(rb_r), .rd_r(rd_r), .xr(xr), .sh_n(sh_n), .cc(cc),
        .fsh(fsh), .mp(mp), .b_imm(b_imm), .mld(mld), .sfill(sfill), .zx(zx), .sh_reg(sh_reg),
        .pstop(pstop), .mw(mw), .msig(msig), .narrow(narrow), .ifill(ifill), .corr(corr),
        .imm(imm), .pn(pn), .sr(sr), .msz(msz), .mmode(mmode), .sc(sc), .shk(shk),
        .pa_src(pa_src), .pb_src(pb_src), .tcode(tcode));

    // which registers D reads: three ports, each a pair index and the halves used
    reg  [2:0]  ia, ib, ic;
    reg  [1:0]  ua, ub, uc;             // {high half, low half} used
    function [1:0] half(input [3:0] r);
        half = r[0] ? 2'b10 : 2'b01;
    endfunction
    always @* begin
        ia = ra_r[3:1]; ib = rb_r[3:1]; ic = xr[3:1];
        ua = 2'b00; ub = 2'b00; uc = 2'b00;
        case (cls)
        C_ALU: begin
            if (fn != F_MOV && fn != F_NEG && fn != F_NOT) ua = half(ra_r);
            if (!b_imm) ub = half(rb_r);
        end
        C_SHF: begin
            ua = fsh ? 2'b11 : half(ra_r);
            if (sh_reg) ub = half(rb_r);
        end
        C_MUL: begin ua = half(ra_r); ub = half(rb_r); end
        C_MEM: begin
            ia = pn; ua = 2'b11;
            ib = rd_r[3:1];
            if (!mld) ub = (msz == 2'd2) ? 2'b11 : half(rd_r);
            if (mmode == M_X) uc = half(xr);
        end
        C_PAIR: begin
            if (pa_src == 2'd0) ua = 2'b11;
            if (pb_src == 2'd1) ub = 2'b11;
            if (pb_src == 2'd2) uc = half(xr);
        end
        C_JR, C_JALR: begin ia = pn; ua = 2'b11; end
        C_MTSR: begin ia = rd_r[3:1]; ua = half(rd_r); end
        C_CLZ: ua = half(ra_r);
        C_DST: begin ia = rd_r[3:1]; ua = 2'b11; ib = ra_r[3:1]; ub = 2'b11; end
        default: ;
        endcase
    end

    // what an instruction writes: wr1 (its result: a register or a pair) and
    // wr2 (the pointer of [pN]+ / -[pN])
    reg  [2:0]  w1i;                    // pair index
    reg  [1:0]  w1h;                    // halves
    reg         w1ld;                   // wr1 is a load's (its value comes from M)
    reg         w2e;
    always @* begin
        w1i = rd_r[3:1]; w1h = 2'b00; w1ld = 1'b0; w2e = 1'b0;
        case (cls)
        C_ALU:   if (fn != F_CMP && fn != F_CPC) w1h = half(rd_r);
        C_SHF:   w1h = half(rd_r);
        C_MUL:   w1h = mp ? 2'b11 : half(rd_r);
        C_MEM:   begin
            if (mld) begin w1h = (msz == 2'd2) ? 2'b11 : half(rd_r); w1ld = 1'b1; end
            w2e = mmode[1];
        end
        C_PAIR, C_DST: w1h = 2'b11;
        C_JAL, C_JALR: begin w1i = 3'd6; w1h = 2'b11; end
        C_MFSR, C_CLZ: w1h = half(rd_r);
        default: ;
        endcase
    end

    // ---- the register file: two banks of 8 x 16 (even and odd registers),
    // three read ports each, one write port each
    reg  [15:0] ev [0:7];
    reg  [15:0] od [0:7];
    reg         rf_we_e, rf_we_o;
    reg  [2:0]  rf_wi_e, rf_wi_o;
    reg  [15:0] rf_wd_e, rf_wd_o;
    always @(posedge clk) begin
        if (rf_we_e) ev[rf_wi_e] <= rf_wd_e;
        if (rf_we_o) od[rf_wi_o] <= rf_wd_o;
    end

    // ---- write descriptors in M and W, for forwarding
    reg         m_valid, w_valid;
    reg  [2:0]  m_w1i, w_w1i, m_w2i, w_w2i;
    reg  [1:0]  m_w1h, w_w1h;
    reg         m_w1ld, m_w2e, w_w2e;
    reg  [31:0] m_v1, w_v1, m_v2, w_v2;     // values (a register's in both halves)

    wire        w_e1 = w_valid && w_w1h != 2'b00, w_e2 = w_valid && w_w2e;

    // ================================================================ X: execute
    reg         x_valid;
    reg  [23:0] x_pc;
    reg         x_len4, x_bad;
    reg  [3:0]  x_cls, x_fn, x_ra, x_rb, x_rd, x_shn, x_cc;
    reg         x_isync;
    reg         x_fsh, x_mp, x_bimm, x_mld, x_sfill, x_zx, x_shreg, x_mw, x_msig, x_ifill, x_corr;
    reg  [23:0] x_imm;
    reg  [2:0]  x_pn, x_sr, x_ia, x_ib, x_ic;
    reg  [1:0]  x_msz, x_mmode, x_sc, x_shk, x_pas, x_pbs;
    reg  [7:0]  x_tcode;
    reg  [2:0]  x_w1i;
    reg  [1:0]  x_w1h;
    reg         x_w1ld, x_w2e;
    reg  [1:0]  x_ph;                   // a split pair access: 0 first word, 1 its data back, 2 second word
    reg  [15:0] x_lo;                   // a split pair load's first half
    reg         x_ahi, x_xhi;           // port A's register, the index register: the pair's high half
    reg         fN, fZ, fC, fV;

    // load-use: D needs a register a load in X writes
    assign d_hazard = d_valid && x_valid && x_w1ld && (x_w1h != 2'b00) &&
                      (((ia == x_w1i) && |(ua & x_w1h)) || ((ib == x_w1i) && |(ub & x_w1h)) ||
                       ((ic == x_w1i) && |(uc & x_w1h)));

    // forwarding into X: M (not a load's register: the hazard keeps that out), then W
    wire        m_e1 = m_valid && m_w1h != 2'b00 && !m_w1ld, m_e2 = m_valid && m_w2e;
    // (explicit logic: a function that read the stages' signals would be
    // re-evaluated only when its arguments change)
    reg  [31:0] pa, pb, pc_;
    always @* begin
        // the register file, read here (writes up to the last clock are in it)
        pa = {od[x_ia], ev[x_ia]}; pb = {od[x_ib], ev[x_ib]}; pc_ = {od[x_ic], ev[x_ic]};
        // W (older), then M (newer); within a stage the pointer, then the result
        if (w_e2 && w_w2i == x_ia) pa = w_v2;
        if (w_e2 && w_w2i == x_ib) pb = w_v2;
        if (w_e2 && w_w2i == x_ic) pc_ = w_v2;
        if (w_e1 && w_w1i == x_ia) begin if (w_w1h[0]) pa[15:0] = w_v1[15:0]; if (w_w1h[1]) pa[31:16] = w_v1[31:16]; end
        if (w_e1 && w_w1i == x_ib) begin if (w_w1h[0]) pb[15:0] = w_v1[15:0]; if (w_w1h[1]) pb[31:16] = w_v1[31:16]; end
        if (w_e1 && w_w1i == x_ic) begin if (w_w1h[0]) pc_[15:0] = w_v1[15:0]; if (w_w1h[1]) pc_[31:16] = w_v1[31:16]; end
        if (m_e2 && m_w2i == x_ia) pa = m_v2;
        if (m_e2 && m_w2i == x_ib) pb = m_v2;
        if (m_e2 && m_w2i == x_ic) pc_ = m_v2;
        if (m_e1 && m_w1i == x_ia) begin if (m_w1h[0]) pa[15:0] = m_v1[15:0]; if (m_w1h[1]) pa[31:16] = m_v1[31:16]; end
        if (m_e1 && m_w1i == x_ib) begin if (m_w1h[0]) pb[15:0] = m_v1[15:0]; if (m_w1h[1]) pb[31:16] = m_v1[31:16]; end
        if (m_e1 && m_w1i == x_ic) begin if (m_w1h[0]) pc_[15:0] = m_v1[15:0]; if (m_w1h[1]) pc_[31:16] = m_v1[31:16]; end
    end
    // 16-bit operands: the half their register names
    wire [15:0] ra16 = x_ahi ? pa[31:16] : pa[15:0];
    wire [15:0] rb16 = x_rb[0] ? pb[31:16] : pb[15:0];
    wire [15:0] xi16 = x_xhi ? pc_[31:16] : pc_[15:0];     // the index register

    // ---- ALU (16 bits)
    reg  [15:0] alu_a, alu_b, alu_r;
    reg         alu_cin, alu_c, alu_v, alu_flags, alu_cv;
    wire [16:0] alu_sum = {1'b0, alu_a} + {1'b0, alu_b} + {16'd0, alu_cin};
    always @* begin
        alu_a = (x_fn == F_NEG) ? 16'h0000 : ra16;
        alu_b = x_bimm ? x_imm[15:0] : rb16;
        alu_cin = 1'b0; alu_cv = 1'b0;
        case (x_fn)
        F_ADD: begin alu_cv = 1'b1; end
        F_ADC: begin alu_cv = 1'b1; alu_cin = fC; end
        F_SUB, F_CMP, F_NEG: begin alu_cv = 1'b1; alu_b = ~alu_b; alu_cin = 1'b1; end
        F_SBC, F_CPC: begin alu_cv = 1'b1; alu_b = ~alu_b; alu_cin = fC; end
        default: ;
        endcase
        case (x_fn)
        F_AND:   alu_r = alu_a & alu_b;
        F_OR:    alu_r = alu_a | alu_b;
        F_XOR:   alu_r = alu_a ^ alu_b;
        F_MOV:   alu_r = alu_b;
        F_NOT:   alu_r = ~alu_b;
        default: alu_r = alu_sum[15:0];
        endcase
        alu_c = alu_sum[16];
        alu_v = (alu_a[15] == alu_b[15]) && (alu_sum[15] != alu_a[15]);
        alu_flags = (x_fn != F_MOV);
    end

    // ---- funnel shifter: 16 bits out of {X, Y}, any of SHL/SHR/SAR, FSHR/FSHL
    wire [3:0]  sh_cnt = x_shreg ? rb16[3:0] : x_shn;
    reg  [15:0] sh_x, sh_y;
    reg  [4:0]  sh_amt;                 // right shift of {X, Y, 0}, 0-16
    always @* begin
        if (x_fsh) begin                // the pair's high and low registers
            sh_x = pa[31:16]; sh_y = pa[15:0];
            sh_amt = x_shk[0] ? {1'b0, sh_cnt} : 5'd16 - {1'b0, sh_cnt};
        end else if (x_shk == 2'd0) begin   // left: {rs, 0} right by 16 - n
            sh_x = ra16; sh_y = 16'h0000;
            sh_amt = 5'd16 - {1'b0, sh_cnt};
        end else begin                  // right: {fill, rs} right by n
            sh_x = (x_shk == 2'd2) ? {16{ra16[15]}} : 16'h0000; sh_y = ra16;
            sh_amt = {1'b0, sh_cnt};
        end
    end
    wire [32:0] sh_w = {sh_x, sh_y, 1'b0} >> sh_amt;
    wire [15:0] sh_r = sh_w[16:1];
    // the last bit out: left, the one above the result; right, the one below
    wire        sh_c = (x_shk == 2'd0) ? sh_w[17] : sh_w[0];

    // ---- multiplier (17 x 17: signed for MULH)
    wire [16:0] mul_a = {x_msig & ra16[15], ra16}, mul_b = {x_msig & rb16[15], rb16};
    wire [33:0] mul_p = $signed(mul_a) * $signed(mul_b);

    // ---- CLZ
    reg  [4:0]  clz_n;
    integer j;
    always @* begin
        clz_n = 5'd16;
        for (j = 0; j < 16; j = j + 1) if (ra16[j]) clz_n = 15 - j;
    end

    // ---- the 32-bit adder: pair ops, effective addresses, DSTEP
    wire [31:0] imm32 = {{8{x_ifill}}, x_imm};
    wire [31:0] xe32 = x_zx ? {16'h0000, xi16} : {{16{xi16[15]}}, xi16};
    wire [31:0] xsc32 = xe32 << x_sc;
    reg  [31:0] ad_a, ad_b;
    reg         ad_cin;
    always @* begin
        ad_a = pa; ad_b = 32'h0; ad_cin = 1'b0;
        case (x_cls)
        C_MEM: case (x_mmode)
            M_D:  ad_b = imm32;
            M_X:  ad_b = xsc32;
            M_PI: ad_b = imm32;                         // the new pointer (the address is the old one)
            default: begin ad_b = ~imm32; ad_cin = 1'b1; end   // -[pN]: pointer and address
            endcase
        C_PAIR: begin
            ad_a = (x_pas == 2'd1) ? 32'h0 : (x_pas == 2'd2) ? {8'h00, x_pc} : pa;
            case (x_pbs)
            2'd0: ad_b = imm32;
            2'd1: ad_b = pb;
            2'd2: ad_b = xsc32;
            default: ad_b = 32'h0;
            endcase
        end
        C_DST: begin ad_a = {pa[30:0], fC}; ad_b = ~pb; ad_cin = 1'b1; end
        default: ;
        endcase
    end
    wire [32:0] ad_s = {1'b0, ad_a} + {1'b0, ad_b} + {32'd0, ad_cin};

    // the effective address and the pointer write-back
    wire [23:0] ea = (x_mmode == M_PI) ? pa[23:0] : ad_s[23:0];
    wire [31:0] newp = ad_s[31:0];
    // the first and the last byte must both be in RAM or I/O (a pair at 0x0FFFFE runs off the end)
    wire [23:0] ea_end = ea + {22'd0, x_msz == 2'd2 ? 2'd3 : {1'b0, x_msz[0]}};
    wire        ea_bad = !okaddr(ea) || !okaddr(ea_end) || (x_msz != 2'd0 && ea[0]);
    wire        split = x_msz == 2'd2 && ea[1];         // a pair across two words

    // ---- branches and jumps
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
    wire [23:0] x_npc = x_pc + (x_len4 ? 24'd4 : 24'd2);
    wire [23:0] br_tgt = (x_corr ? x_pc : x_npc) + x_imm;
    wire [23:0] jr_tgt = {pa[23:1], 1'b0};
    wire        take = cond(x_cc, fN, fZ, fC, fV);

    // ---- SR
    assign sr_sel = x_sr[1:0] - 2'd1;
    wire [15:0] srv = (x_sr == 3'd0) ? {12'd0, fN, fZ, fC, fV} : (x_sr <= 3'd3) ? sr_in : 16'd0;

    // ---- the instruction's result, flags, redirect, fault
    reg  [31:0] x_v1;
    reg         x_flw;                  // flags change
    reg         nN, nZ, nC, nV;
    reg         x_jump, x_fault, x_trap;
    reg  [23:0] x_jtgt;
    reg  [7:0]  x_hcode;
    always @* begin
        x_v1 = 32'h0; x_flw = 1'b0; nN = fN; nZ = fZ; nC = fC; nV = fV;
        x_jump = 1'b0; x_jtgt = x_npc; x_fault = 1'b0; x_trap = 1'b0; x_hcode = 8'hFD;
        case (x_cls)
        C_ALU: begin
            x_v1 = {alu_r, alu_r};
            if (alu_flags) begin
                x_flw = 1'b1;
                nN = alu_r[15];
                nZ = (alu_r == 16'h0000) && ((x_fn == F_SBC || x_fn == F_CPC) ? fZ : 1'b1);
                if (alu_cv) begin nC = alu_c; nV = alu_v; end
                else nV = 1'b0;
            end
        end
        C_SHF: begin
            x_v1 = {sh_r, sh_r};
            if (!x_fsh) begin
                x_flw = 1'b1; nN = sh_r[15]; nZ = sh_r == 16'h0000; nV = 1'b0;
                if (sh_cnt != 4'd0) nC = sh_c;
            end
        end
        C_MUL:  x_v1 = x_mp ? mul_p[31:0] : x_mw ? {mul_p[31:16], mul_p[31:16]} : {mul_p[15:0], mul_p[15:0]};
        C_MEM:  begin x_v1 = 32'h0; if (ea_bad) x_fault = 1'b1; end
        C_PAIR: x_v1 = ad_s[31:0];
        C_BR:   if (take) begin
            x_jump = 1'b1; x_jtgt = br_tgt;
            if (!okaddr(br_tgt)) x_fault = 1'b1;
        end
        C_JAL:  begin
            x_v1 = {8'h00, x_npc}; x_jump = 1'b1; x_jtgt = br_tgt;
            if (!okaddr(br_tgt)) x_fault = 1'b1;
        end
        C_JR, C_JALR: begin
            x_v1 = {8'h00, x_npc}; x_jump = 1'b1; x_jtgt = jr_tgt;
            if (!okaddr(jr_tgt)) x_fault = 1'b1;
        end
        C_MFSR: x_v1 = {srv, srv};
        C_MTSR: if (x_sr == 3'd0) begin
            x_flw = 1'b1; nN = ra16[3]; nZ = ra16[2]; nC = ra16[1]; nV = ra16[0];
        end
        C_NOP:  ;                                       // NOP, SYNC (SYNC redirects, below)
        C_CLZ:  x_v1 = {11'd0, clz_n, 11'd0, clz_n};
        C_DST:  begin
            // r = pd*2 + C (33 bits) against ps: C = r >= ps; if so subtract
            x_flw = 1'b1;
            nC = pa[31] | ad_s[32];
            x_v1 = nC ? ad_s[31:0] : {pa[30:0], fC};
        end
        default: begin x_trap = 1'b1; x_hcode = x_tcode; end
        endcase
        if (x_bad) begin x_fault = 1'b1; x_trap = 1'b0; x_hcode = 8'hFD; end
        if (x_fault) x_hcode = 8'hFD;
    end
    wire        x_sync = x_cls == C_NOP && x_isync;

    // ---- the data port: a request while X moves on (two for a split pair)
    wire        x_mem = x_valid && x_cls == C_MEM && !x_fault;
    wire [23:0] ea_w = (split && x_ph == 2'd2) ? {ea[23:2] + 22'd1, 2'b00} : ea;   // the second word of a split
    reg  [3:0]  be;
    reg  [31:0] wd;
    wire [31:0] stv = pb;               // the store's data: a register or the pair
    wire [15:0] st16 = x_rd[0] ? stv[31:16] : stv[15:0];
    always @* begin
        case (x_msz)
        2'd0:    begin be = 4'b0001 << ea[1:0]; wd = {4{st16[7:0]}}; end
        2'd1:    begin be = ea[1] ? 4'b1100 : 4'b0011; wd = {2{st16}}; end
        default: if (!split) begin be = 4'b1111; wd = stv; end
                 else if (x_ph != 2'd2) begin be = 4'b1100; wd = {stv[15:0], 16'h0000}; end
                 else begin be = 4'b0011; wd = {16'h0000, stv[31:16]}; end
        endcase
    end
    assign d_req   = x_mem && adv_x && x_ph != 2'd1;
    assign d_we    = !x_mld;
    assign d_addr  = ea_w;
    assign d_be    = be;
    assign d_wdata = wd;
    assign x_hold  = x_mem && !(d_gnt && (!split || x_ph == 2'd2));
    wire        x_leave = x_valid && adv_x && !x_hold;   // X's instruction moves to M this clock

    // a store into code already fetched: refetch from the next instruction
    reg  [23:0] f_hi;                   // fetched up to here (the first byte not requested)
    // (the words the store writes, against those fetched after it)
    wire [21:0] st_w0 = ea[23:2], st_w1 = ea[23:2] + {21'd0, split};
    wire        smc = x_mem && !x_mld && st_w1 >= x_npc[23:2] && st_w0 < f_hi[23:2];

    // ================================================================ M: memory
    reg  [23:0] m_pc;
    reg         m_ld, m_split, m_sfill;
    reg  [1:0]  m_msz, m_ea;
    reg  [15:0] m_lo;                   // a split pair's first half
    reg  [3:0]  m_fl;
    wire        m_need = m_valid && m_ld;
    assign m_hold = m_need && !d_rvalid;
    // the loaded value, placed for the destination
    reg  [31:0] ld_v;
    always @* begin
        case (m_msz)
        2'd0: begin
            ld_v[7:0] = d_rdata[8 * m_ea +: 8];
            ld_v[15:8] = {8{m_sfill & ld_v[7]}};
            ld_v[31:16] = ld_v[15:0];
        end
        2'd1: begin ld_v[15:0] = m_ea[1] ? d_rdata[31:16] : d_rdata[15:0]; ld_v[31:16] = ld_v[15:0]; end
        default: ld_v = m_split ? {d_rdata[15:0], m_lo} : d_rdata;
        endcase
    end

    // ================================================================ W: writeback
    reg  [23:0] w_pc, ret_pc;           // ret_pc, ret_fl: the instruction that just retired (testbenches)
    reg  [3:0]  w_fl, ret_fl;
    reg         w_first;                // a two-write instruction's first clock (the pointer)
    assign w_hold = w_valid && w_e1 && w_e2 && w_first;
    always @* begin
        rf_we_e = 1'b0; rf_we_o = 1'b0;
        rf_wi_e = w_w1i; rf_wi_o = w_w1i;
        rf_wd_e = w_v1[15:0]; rf_wd_o = w_v1[31:16];
        if (w_valid && !rst) begin
            if (w_e2 && (w_first || !w_e1)) begin           // the pointer, first
                rf_we_e = 1'b1; rf_we_o = 1'b1; rf_wi_e = w_w2i; rf_wi_o = w_w2i;
                rf_wd_e = w_v2[15:0]; rf_wd_o = w_v2[31:16];
            end else if (w_e1) begin
                rf_we_e = w_w1h[0]; rf_we_o = w_w1h[1];
            end
        end
    end

    // ================================================================ the clock
    wire        rbad = reset_pc[0] || !okaddr(reset_pc);
    always @* begin
        redir = 1'b0; redir_pc = x_npc; halt_go = 1'b0;
        if (x_leave) begin
            if (x_fault || x_trap) halt_go = 1'b1;
            else if (x_jump) begin redir = 1'b1; redir_pc = x_jtgt; end
            else if (x_sync || smc) redir = 1'b1;
        end
    end

    // the buffer after D takes its parcels and a word arrives
    wire [1:0]  hs = {1'b0, hp} + d_used;   // past the head: 2 or 3 leave fw0 behind
    always @(posedge clk) begin
        if (rst) begin
            fv0 <= 1'b0; fv1 <= 1'b0; hp <= reset_pc[1]; qpc <= reset_pc; fa <= reset_pc[23:2];
            ofl <= 2'd0; kill <= 2'd0; f_hi <= {reset_pc[23:2], 2'b00};
            stopping <= rbad;
        end else begin
            ofl <= ofl + {1'b0, f_take} - {1'b0, i_rvalid};
            if (i_rvalid && kill != 2'd0 && !redir) kill <= kill - 2'd1;
            if (redir) begin
                fv0 <= 1'b0; fv1 <= 1'b0; hp <= redir_pc[1]; qpc <= redir_pc;
                fa <= redir_pc[23:2]; f_hi <= {redir_pc[23:2], 2'b00};
                kill <= ofl - {1'b0, i_rvalid};     // everything still out is the old stream's
            end else begin
                if (f_take || f_fake) begin fa <= fa + 22'd1; f_hi <= {fa + 22'd1, 2'b00}; end
                if (hs[1]) begin                    // the head word is used up
                    fw0 <= vw1; fv0 <= vv1; ff0 <= vf1; fv1 <= 1'b0; hp <= hs[0];
                end else begin
                    fw0 <= vw0; fv0 <= vv0; ff0 <= vf0; fw1 <= vw1; fv1 <= vv1; ff1 <= vf1; hp <= hs[0];
                end
                qpc <= qpc + {21'd0, d_used, 1'b0};
            end
            if (halt_go) stopping <= 1'b1;
        end
    end

    // D
    always @(posedge clk) begin
        if (rst) d_valid <= 1'b0;
        else if (redir || halt_go) d_valid <= 1'b0;
        else if (d_take) begin
            d_valid <= nx_ok;
            if (nx_ok) begin
                d_ir <= p0; d_p2 <= p1; d_pc <= qpc;
                d_len4 <= nx_32;
                d_bad <= vf0 || (nx_32 && pf1);
            end
        end
    end

    // X
    always @(posedge clk) begin
        if (rst) begin
            x_valid <= 1'b0; x_ph <= 1'b0;
            fN <= 1'b0; fZ <= 1'b0; fC <= 1'b0; fV <= 1'b0;
        end else begin
            if (x_leave && !x_fault && !x_trap && x_flw) begin fN <= nN; fZ <= nZ; fC <= nC; fV <= nV; end
            // a split pair: first word, (a load: its data), second word
            if (x_mem && split && adv_x) begin
                if (x_ph == 2'd0 && d_gnt) x_ph <= x_mld ? 2'd1 : 2'd2;
                if (x_ph == 2'd1 && d_rvalid) begin x_ph <= 2'd2; x_lo <= d_rdata[31:16]; end
            end
            if (adv_d) begin
                x_ph <= 2'd0;
                x_valid <= d_valid && !d_hazard && !redir && !halt_go;
                x_pc <= d_pc; x_len4 <= d_len4; x_bad <= d_bad;
                x_cls <= cls; x_fn <= fn; x_ra <= ra_r; x_rb <= rb_r; x_rd <= rd_r; x_shn <= sh_n; x_cc <= cc;
                x_fsh <= fsh; x_mp <= mp; x_bimm <= b_imm; x_mld <= mld; x_sfill <= sfill; x_zx <= zx;
                x_shreg <= sh_reg; x_mw <= mw; x_msig <= msig; x_ifill <= ifill; x_corr <= corr;
                x_imm <= imm; x_pn <= pn; x_sr <= sr; x_msz <= msz; x_mmode <= mmode; x_sc <= sc;
                x_shk <= shk; x_pas <= pa_src; x_pbs <= pb_src; x_tcode <= tcode;
                x_ia <= ia; x_ib <= ib; x_ic <= ic; x_xhi <= xr[0];
                x_ahi <= (cls == C_MTSR) ? rd_r[0] : ra_r[0];
                x_w1i <= w1i; x_w1h <= w1h; x_w1ld <= w1ld; x_w2e <= w2e;
                x_isync <= d_ir == 16'hA301;
            end
        end
    end

    // M
    always @(posedge clk) begin
        if (rst) m_valid <= 1'b0;
        else begin
            if (adv_x) begin
                m_valid <= x_leave && !x_fault && !x_trap;
                m_lo <= x_lo;
                m_pc <= x_pc;
                m_w1i <= x_w1i; m_w1h <= x_w1h; m_w1ld <= x_w1ld; m_w2i <= x_pn; m_w2e <= x_w2e;
                m_v1 <= x_v1; m_v2 <= newp;
                m_ld <= x_cls == C_MEM && x_mld; m_split <= split; m_sfill <= x_sfill;
                m_msz <= x_msz; m_ea <= ea[1:0];
                m_fl <= (x_flw ? {nN, nZ, nC, nV} : {fN, fZ, fC, fV});
            end
        end
    end

    // W
    always @(posedge clk) begin
        if (rst) begin w_valid <= 1'b0; w_first <= 1'b1; retire <= 1'b0; halted <= rbad; halt_code <= rbad ? 8'hFD : 8'h00; end
        else begin
            retire <= 1'b0;
            if (w_hold) w_first <= 1'b0;
            else begin
                w_first <= 1'b1;
                if (w_valid) begin retire <= 1'b1; ret_pc <= w_pc; ret_fl <= w_fl; end
                w_valid <= adv_m && m_valid && !m_hold;
                w_pc <= m_pc; w_fl <= m_fl;
                w_w1i <= m_w1i; w_w1h <= m_w1h; w_w2i <= m_w2i; w_w2e <= m_w2e;
                w_v1 <= m_ld ? ld_v : m_v1; w_v2 <= m_v2;
            end
            // the end: a fault or trap stops the program once everything older has retired
            if (halt_go) halt_code <= x_hcode;
            if (stopping && !rbad && !x_valid && !m_valid && !w_valid && halt_pend) halted <= 1'b1;
        end
    end
    always @(posedge clk) if (rst) halt_pend <= 1'b0; else if (halt_go) halt_pend <= 1'b1;
endmodule

`default_nettype wire
