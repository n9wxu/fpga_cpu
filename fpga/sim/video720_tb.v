`timescale 1ns/1ps
// video720: 1280x720 sync timing, horizontal pixel doubling, nothing lit in
// blanking, and a write through the text RAM's second port. Writes the
// second frame to vga_frame.txt, one character per pixel ('0'-'7' = rgb);
// sim/frame2png.py turns it into an image and checks the vertical doubling.
// Runs in build/, where the font and the demo screen are.
module video720_tb;
    reg clk = 0, wclk = 0;
    always #6.734 clk  = ~clk;            // 74.25 MHz
    always #10    wclk = ~wclk;           // 50 MHz

    reg        we = 0;
    reg [10:0] waddr = 0;
    reg [15:0] wdata = 0;
    wire hs, vs;
    wire [2:0] rgb;
    video720 dut (.clk(clk), .wclk(wclk), .we(we), .waddr(waddr), .wdata(wdata),
                  .hsync(hs), .vsync(vs), .rgb(rgb));

    integer errors = 0;

    // Sync periods and widths in pixel clocks; syncs are positive.
    localparam LINE = 1650, FRAME = 1650 * 750;
    integer t = 0, hs_rise = -1, vs_rise = -1, hs_n = 0, vs_n = 0;
    reg hs_q = 0, vs_q = 0;
    always @(posedge clk) begin
        t = t + 1;
        if (!hs_q && hs) begin
            if (hs_rise >= 0 && t - hs_rise != LINE) begin
                errors = errors + 1; $display("FAIL: hsync period %0d, want %0d", t - hs_rise, LINE);
            end
            hs_rise = t; hs_n = hs_n + 1;
        end
        if (hs_q && !hs && t - hs_rise != 40) begin
            errors = errors + 1; $display("FAIL: hsync width %0d, want 40", t - hs_rise);
        end
        if (!vs_q && vs) begin
            if (!(!hs_q && hs)) begin
                errors = errors + 1; $display("FAIL: vsync does not start with hsync");
            end
            if (vs_rise >= 0 && t - vs_rise != FRAME) begin
                errors = errors + 1; $display("FAIL: vsync period %0d, want %0d", t - vs_rise, FRAME);
            end
            vs_rise = t; vs_n = vs_n + 1;
        end
        if (vs_q && !vs) begin
            if (t - vs_rise != 5 * LINE) begin
                errors = errors + 1; $display("FAIL: vsync width %0d, want %0d", t - vs_rise, 5 * LINE);
            end
            if (!(!hs_q && hs)) begin
                errors = errors + 1; $display("FAIL: vsync does not end with hsync");
            end
        end
        hs_q = hs; vs_q = vs;
    end

    // rgb comes out three clocks after the counters that produced it.
    reg [10:0] h1 = 0, h2 = 0, h3 = 0;
    reg [9:0]  v1 = 0, v2 = 0, v3 = 0;
    reg [2:0]  prev = 0;
    integer f, frame = 0, written = 0, blank_bad = 0, double_bad = 0, warm = 0;
    always @(posedge clk) begin
        if (warm >= 3) begin
            if (h3 == 0 && v3 == 0) frame = frame + 1;
            if (h3 < 1280 && v3 < 720) begin
                if (h3[0] && rgb != prev) double_bad = double_bad + 1;
                if (frame == 2) begin
                    $fwrite(f, "%0d", rgb);
                    if (h3 == 1279) $fwrite(f, "\n");
                    written = written + 1;
                end
            end else if (rgb != 0)
                blank_bad = blank_bad + 1;
            prev = rgb;
        end
        warm = warm + 1;
        h1 <= dut.hc; h2 <= h1; h3 <= h2;
        v1 <= dut.vc; v2 <= v1; v3 <= v2;
    end

    initial begin
        f = $fopen("vga_frame.txt", "w");
        // Bottom-right text cell: a white-on-red 'W' through the write port.
        @(posedge wclk);
        we <= 1; waddr <= 24 * 80 + 79; wdata <= {2'b00, 3'd4, 3'd7, 8'h57};
        @(posedge wclk);
        we <= 0;
        wait (frame == 3);
        $fclose(f);
        if (written != 1280 * 720) begin errors = errors + 1; $display("FAIL: %0d pixels in frame", written); end
        if (blank_bad)  begin errors = errors + 1; $display("FAIL: %0d lit pixels in blanking", blank_bad); end
        if (double_bad) begin errors = errors + 1; $display("FAIL: %0d pixels differ from their left twin", double_bad); end
        if (errors) $display("video720: %0d error(s)", errors);
        else        $display("video720: ok (%0d lines, %0d frames)", hs_n, vs_n);
        $finish;
    end
endmodule
