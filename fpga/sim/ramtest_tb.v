`timescale 1ns/1ps
// ramtest.v in simulation, against the SRAM model: run +cycles=N (default
// 300000) and print the counters ramtest.py reads on the board.
module ramtest_tb;
    reg clk = 0;
    always #10 clk = ~clk;
    wire [17:0] sram_a;
    wire [31:0] sram_d;
    wire        sram_oe_n, sram_we_n;
    wire [1:0]  sram_ce_n;
    wire [3:0]  sram_be_n;
    wire [7:0]  led, seg;
    wire [3:0]  an;
    ramtest dut (.clk(clk), .btn(4'd0), .sw(8'd0), .led(led), .an(an), .seg(seg),
                 .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
                 .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n));
    sram_model sram (.a(sram_a), .d(sram_d), .oe_n(sram_oe_n), .we_n(sram_we_n),
                     .ce_n(sram_ce_n), .be_n(sram_be_n));
    integer n, i;
    initial begin
        if (!$value$plusargs("cycles=%d", n)) n = 300000;
        if ($test$plusargs("wmode1")) dut.ctrl[1] = 1'b1;
        repeat (n) @(posedge clk);
        $display("clocks %0d  filled %b  video misses %0d", dut.n_clk, dut.filled, dut.n_mis);
        for (i = 0; i < 5; i = i + 1)
            $display("port %0d: words %0d errors %0d", i, dut.n_w[i*32 +: 32], dut.n_e[i*32 +: 32]);
        if (dut.e_a != 0) $display("first error at %05x: got %08x expected %08x", dut.e_a[17:0], dut.e_got, dut.e_exp);
        $finish;
    end
endmodule
