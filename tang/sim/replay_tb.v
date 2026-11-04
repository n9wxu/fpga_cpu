`timescale 1ns/1ps
// rtl/hdmiload.v -DREPLAY in simulation: the PLL, serializers and LVDS
// buffers stubbed; +words=FILE dumps the words going to the serializers
// (for tools/hdmi_audio_check.py, NOTONE=1). +lines=N lines (default 400).
module replay_tb;
    reg clk = 1'b0;
    always #18.5 clk = ~clk;
    wire [31:0] dq;
    wire [10:0] a; wire [1:0] ba; wire [3:0] dqm;
    wire cke, cs_n, ras_n, cas_n, we_n, sdclk;
    hdmiload dut (.clk(clk), .s1(1'b0), .led(), .tmds_p(), .tmds_n(),
        .O_sdram_clk(sdclk), .O_sdram_cke(cke), .O_sdram_cs_n(cs_n), .O_sdram_ras_n(ras_n),
        .O_sdram_cas_n(cas_n), .O_sdram_wen_n(we_n), .O_sdram_addr(a), .O_sdram_ba(ba),
        .O_sdram_dqm(dqm), .IO_sdram_dq(dq));
    sdram_model mem (.clk(sdclk), .cke(cke), .cs_n(cs_n), .ras_n(ras_n), .cas_n(cas_n), .we_n(we_n),
        .a(a), .ba(ba), .dqm(dqm), .dq(dq));
    reg [1023:0] wfile;
    integer wf = 0, n = 0, lines = 400;
    initial begin
        if (!$value$plusargs("lines=%d", lines)) lines = 400;
        if ($value$plusargs("words=%s", wfile)) wf = $fopen(wfile, "w");
    end
    always @(posedge dut.pclk) begin
        if (wf != 0) $fwrite(wf, "%h %h %h\n", dut.owr[9:0], dut.owr[19:10], dut.owr[29:20]);
        n = n + 1;
        if (n == lines * 1650) begin $display("[replay_tb] %0d pixels, bad %b", n, dut.bad); $finish; end
    end
endmodule
// stubs
module rPLL #(parameter FCLKIN = "27", parameter IDIV_SEL = 0, parameter FBDIV_SEL = 0, parameter ODIV_SEL = 0,
              parameter PSDA_SEL = "0000", parameter DUTYDA_SEL = "1000", parameter DEVICE = "")
    (input CLKIN, output CLKOUT, output CLKOUTP, output CLKOUTD, output CLKOUTD3, output LOCK,
     input CLKFB, input [5:0] FBDSEL, input [5:0] IDSEL, input [5:0] ODSEL, input [3:0] DUTYDA,
     input [3:0] PSDA, input [3:0] FDLY, input RESET, input RESET_P);
    reg f = 1'b0;
    always #7.407 f = ~f;
    assign CLKOUT = f, CLKOUTP = f, LOCK = 1'b1;
endmodule
module OSER10 (input D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, FCLK, PCLK, RESET, output Q);
    assign Q = D0;
endmodule
module TLVDS_OBUF (input I, output O, output OB);
    assign O = I, OB = ~I;
endmodule
