`timescale 1ns/1ps
// t1test: runs sim/progs/t1check.c; the 7-segment value must be 1869.
module t1test_tb;
    reg clk = 0;
    always #10 clk = ~clk;
    wire [7:0] led, seg;
    wire [3:0] an;
    t1test #(.IMAGE("t1test.hex")) dut (.clk(clk), .btn(4'b0000), .led(led), .an(an), .seg(seg));
    initial begin
        dut.hold = 20'd10;
        wait (led == 8'h80 || dut.halted);
        $display("t1test: seg7 %04x, leds %02x, halted %0d (want 1869, 80)", dut.seg7, led, dut.halted);
        $finish;
    end
    initial begin #400000000; $display("t1test: timeout, leds %02x", led); $finish; end
endmodule
