`timescale 1ns/1ps
// board_test with tiny dividers: checks the BCD count (including the
// 9999 -> 0000 wrap), what the display shows for each digit, and the
// scanner's bounce.
module board_test_tb;
    localparam CLK_HZ = 20, STEP = 3;

    reg clk = 0, btn0 = 1;
    wire [7:0] led;
    wire [3:0] an;
    wire [7:0] seg;
    always #1 clk = ~clk;

    board_test #(.CLK_HZ(CLK_HZ), .STEP_TICKS(STEP), .SCAN_BITS(4), .PWM_SHIFT(0))
        dut (.clk(clk), .btn0(btn0), .led(led), .an(an), .seg(seg));

    integer errors = 0;
    task check(input cond, input [8*48-1:0] what);
        if (!cond) begin
            errors = errors + 1;
            $display("FAIL at %0t: %0s", $time, what);
        end
    endtask

    function [6:0] glyph(input [3:0] d);
        case (d)
            0: glyph = 7'h3F; 1: glyph = 7'h06; 2: glyph = 7'h5B; 3: glyph = 7'h4F;
            4: glyph = 7'h66; 5: glyph = 7'h6D; 6: glyph = 7'h7D; 7: glyph = 7'h07;
            8: glyph = 7'h7F; default: glyph = 7'h6F;
        endcase
    endfunction

    wire [15:0] bcd = {dut.d3, dut.d2, dut.d1, dut.d0};

    // The display always shows the digit its active anode selects (outputs
    // are registered, so compare one clock after the select).
    reg [1:0] sel_q;
    reg [15:0] bcd_q;
    always @(posedge clk) begin
        sel_q <= dut.sel;
        bcd_q <= bcd;
        if (!btn0 && $time > 20) begin
            check(an == ~(4'b0001 << sel_q), "one anode, the selected one");
            check(seg == {1'b1, ~glyph(bcd_q[4*sel_q +: 4])}, "segments show the digit");
        end
    end

    // Scanner position sequence: 0 1 .. 7 6 .. 0 1 ..
    integer steps = 0, expect_pos = 0, expect_dir = 1;
    always @(posedge clk)
        if (!btn0 && dut.step) begin
            #0.1;
            if (expect_pos == 7) expect_dir = -1;
            if (expect_pos == 0) expect_dir = 1;
            expect_pos = expect_pos + expect_dir;
            check(dut.pos == expect_pos, "scanner position");
            check(dut.bright[4*dut.pos +: 4] == 15, "current LED full");
            steps = steps + 1;
        end

    integer n;
    initial begin
        repeat (5) @(posedge clk);
        btn0 = 0;
        // sample mid-second, clear of the reset synchronizer's latency
        @(negedge dut.rst);
        repeat (CLK_HZ / 2) @(posedge clk);
        for (n = 1; n <= 10002; n = n + 1) begin
            repeat (CLK_HZ) @(posedge clk);
            #0.1;
            if (n == 1)     check(bcd == 16'h0001, "1 s");
            if (n == 59)    check(bcd == 16'h0059, "59 s");
            if (n == 1234)  check(bcd == 16'h1234, "1234 s");
            if (n == 9999)  check(bcd == 16'h9999, "9999 s");
            if (n == 10000) check(bcd == 16'h0000, "wrap to 0000");
            if (n == 10002) check(bcd == 16'h0002, "2 s after wrap");
        end
        btn0 = 1;
        repeat (4) @(posedge clk);
        check(bcd == 0 && dut.pos == 0, "reset");
        if (steps < 100) begin errors = errors + 1; $display("FAIL: only %0d scanner steps", steps); end
        if (errors) $display("board_test: %0d error(s)", errors);
        else        $display("board_test: ok (%0d scanner steps)", steps);
        $finish;
    end
endmodule
