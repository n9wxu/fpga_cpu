// T1S's decoder (t1s_dec.v and its ROM) against T1F's (t1_dec.v): reads
// "ir p2" pairs from +vec=FILE and prints every disagreement. An illegal
// instruction (FE) or another trap need only agree on cls and tcode; the
// sequencer looks at nothing else for them.
`timescale 1ns / 1ns
`default_nettype none
module t1s_dec_tb;
    parameter EXT_X = 0;
    reg  [15:0] ir, p2;
    wire [3:0]  a_cls, a_fn, a_ra, a_rb, a_rd, a_xr, a_shn, a_cc, b_cls, b_fn, b_ra, b_rb, b_rd, b_xr, b_shn, b_cc;
    wire        a_fsh, a_mp, a_bimm, a_mld, a_sfill, a_zx, a_shreg, a_pstop, a_mw, a_msig, a_nar, a_ifill, a_corr;
    wire        b_fsh, b_mp, b_bimm, b_mld, b_sfill, b_zx, b_shreg, b_pstop, b_mw, b_msig, b_nar, b_ifill, b_corr;
    wire [23:0] a_imm, b_imm;
    wire [2:0]  a_pn, a_sr, b_pn, b_sr;
    wire [1:0]  a_msz, a_mmode, a_sc, a_shk, a_pa, a_pb, b_msz, b_mmode, b_sc, b_shk, b_pa, b_pb;
    wire [7:0]  a_tc, b_tc;
    t1f_dec #(.EXT_X(EXT_X)) ref_ (.ir(ir), .p2(p2), .cls(a_cls), .fn(a_fn), .ra_r(a_ra), .rb_r(a_rb), .rd_r(a_rd),
        .xr(a_xr), .sh_n(a_shn), .cc(a_cc), .fsh(a_fsh), .mp(a_mp), .b_imm(a_bimm), .mld(a_mld), .sfill(a_sfill),
        .zx(a_zx), .sh_reg(a_shreg), .pstop(a_pstop), .mw(a_mw), .msig(a_msig), .narrow(a_nar), .ifill(a_ifill),
        .corr(a_corr), .imm(a_imm), .pn(a_pn), .sr(a_sr), .msz(a_msz), .mmode(a_mmode), .sc(a_sc), .shk(a_shk),
        .pa_src(a_pa), .pb_src(a_pb), .tcode(a_tc));
    wire [9:0]  rkey;
    reg  [80:0] rword;
    t1s_key key (.ir(ir), .p2(p2), .k(rkey));
    generate if (EXT_X) begin : x1
        always @* begin
`include "t1s_rom_x1.vh"
        end
    end else begin : x0
        always @* begin
`include "t1s_rom_x0.vh"
        end
    end endgenerate
    t1s_fields dut (.w(rword), .ir(ir), .p2(p2), .cls(b_cls), .fn(b_fn), .ra_r(b_ra), .rb_r(b_rb), .rd_r(b_rd),
        .xr(b_xr), .sh_n(b_shn), .cc(b_cc), .fsh(b_fsh), .mp(b_mp), .b_imm(b_bimm), .mld(b_mld), .sfill(b_sfill),
        .zx(b_zx), .sh_reg(b_shreg), .pstop(b_pstop), .mw(b_mw), .msig(b_msig), .narrow(b_nar), .ifill(b_ifill),
        .corr(b_corr), .imm(b_imm), .pn(b_pn), .sr(b_sr), .msz(b_msz), .mmode(b_mmode), .sc(b_sc), .shk(b_shk),
        .pa_src(b_pa), .pb_src(b_pb), .tcode(b_tc));
    wire [127:0] av = {a_fn, a_ra, a_rb, a_rd, a_xr, a_shn, a_cc, a_fsh, a_mp, a_bimm, a_mld, a_sfill, a_zx, a_shreg,
                       a_pstop, a_mw, a_msig, a_nar, a_ifill, a_corr, a_imm, a_pn, a_sr, a_msz, a_mmode, a_sc, a_shk, a_pa, a_pb};
    wire [127:0] bv = {b_fn, b_ra, b_rb, b_rd, b_xr, b_shn, b_cc, b_fsh, b_mp, b_bimm, b_mld, b_sfill, b_zx, b_shreg,
                       b_pstop, b_mw, b_msig, b_nar, b_ifill, b_corr, b_imm, b_pn, b_sr, b_msz, b_mmode, b_sc, b_shk, b_pa, b_pb};
    reg [8*512-1:0] path;
    integer f, n, cnt, bad;
    initial begin
        if (!$value$plusargs("vec=%s", path)) begin $display("usage: +vec=FILE"); $finish; end
        f = $fopen(path, "r"); cnt = 0; bad = 0;
        while (!$feof(f)) begin
            n = $fscanf(f, "%h %h\n", ir, p2);
            if (n == 2) begin
                #1; cnt = cnt + 1;
                if (a_cls != b_cls || a_tc != b_tc || (a_cls != 4'd12 && av != bv)) begin
                    bad = bad + 1;
                    if (bad <= 20) $display("MISMATCH %h %h key %h: cls %h/%h tcode %h/%h\n  ref %h\n  t1s %h",
                                            ir, p2, rkey, a_cls, b_cls, a_tc, b_tc, av, bv);
                end
            end
        end
        $display("%0d vectors, %0d mismatches", cnt, bad);
        $finish;
    end
endmodule
`default_nettype wire
