// Reference outputs of the T1F decoder (t1_dec.v, module t1f_dec): reads
// "ir p2" pairs (hex) from +vec=FILE and prints, for each, every output in
// hex: cls fn ra rb rd xr shn cc fsh mp bimm mld sfill zx shreg pstop mw msig
// narrow ifill corr imm pn sr msz mmode sc shk pa pb tcode.
// verilog/tb/t1sgen.py builds T1S's decode ROM from these.
`timescale 1ns / 1ns
`default_nettype none
module t1f_dec_dump;
    parameter EXT_X = 0;
    reg  [15:0] ir, p2;
    wire [3:0]  cls, fn, ra_r, rb_r, rd_r, xr, sh_n, cc;
    wire        fsh, mp, b_imm, mld, sfill, zx, sh_reg, pstop, mw, msig, narrow, ifill, corr;
    wire [23:0] imm;
    wire [2:0]  pn, sr;
    wire [1:0]  msz, mmode, sc, shk, pa_src, pb_src;
    wire [7:0]  tcode;
    t1f_dec #(.EXT_X(EXT_X)) d (.ir(ir), .p2(p2), .cls(cls), .fn(fn), .ra_r(ra_r), .rb_r(rb_r), .rd_r(rd_r),
        .xr(xr), .sh_n(sh_n), .cc(cc), .fsh(fsh), .mp(mp), .b_imm(b_imm), .mld(mld), .sfill(sfill), .zx(zx),
        .sh_reg(sh_reg), .pstop(pstop), .mw(mw), .msig(msig), .narrow(narrow), .ifill(ifill), .corr(corr),
        .imm(imm), .pn(pn), .sr(sr), .msz(msz), .mmode(mmode), .sc(sc), .shk(shk), .pa_src(pa_src),
        .pb_src(pb_src), .tcode(tcode));
    reg [8*512-1:0] path;
    integer f, n;
    initial begin
        if (!$value$plusargs("vec=%s", path)) begin $display("usage: +vec=FILE"); $finish; end
        f = $fopen(path, "r");
        while (!$feof(f)) begin
            n = $fscanf(f, "%h %h\n", ir, p2);
            if (n == 2) begin
                #1;
                $display("%h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h %h",
                    cls, fn, ra_r, rb_r, rd_r, xr, sh_n, cc, fsh, mp, b_imm, mld, sfill, zx, sh_reg, pstop,
                    mw, msig, narrow, ifill, corr, imm, pn, sr, msz, mmode, sc, shk, pa_src, pb_src, tcode);
            end
        end
        $finish;
    end
endmodule
`default_nettype wire
