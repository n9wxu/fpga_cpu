`timescale 1ns/1ps
// ramvid.v in simulation against the SRAM model, with a short "second"
// (SIM_SEC clocks): runs until the status screen has been formatted and
// drawn twice, then dumps a frame (build/ramvid_frame.txt, one rgb digit a
// pixel) and the counters of the last second.
module ramvid_tb;
    reg clk = 0;
    always #10 clk = ~clk;
    wire [17:0] sram_a;
    wire [31:0] sram_d;
    wire        sram_oe_n, sram_we_n;
    wire [1:0]  sram_ce_n;
    wire [3:0]  sram_be_n;
    wire [7:0]  led, seg;
    wire [3:0]  an;
    wire        r, g, b, hs, vs;
    ramvid #(.SIM_SEC(400000)) dut (.clk(clk), .btn(4'd0), .sw(8'd0), .led(led), .an(an), .seg(seg),
        .vga_r(r), .vga_g(g), .vga_b(b), .vga_hs(hs), .vga_vs(vs),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n));
    sram_model sram (.a(sram_a), .d(sram_d), .oe_n(sram_oe_n), .we_n(sram_we_n),
                     .ce_n(sram_ce_n), .be_n(sram_be_n));
    integer i, f, x, y, redraws;
    initial begin
        redraws = 0;
        while (redraws < 2) begin @(posedge clk); if (dut.redraw_p) redraws = redraws + 1; end
        $display("filled %b, %0d redraws at %0t", dut.filled, redraws, $time);
        for (i = 0; i < 23; i = i + 1) $display("src %2d: %0d", i, dut.accr[{!dut.bank, i[4:0]}]);
        // a frame: wait for vsync to end, then the visible lines
        @(negedge vs);
        f = $fopen("ramvid_frame.txt", "w");
        for (y = 0; y < 750; y = y + 1) begin
            @(posedge dut.pclk); while (dut.video.hc != 0) @(posedge dut.pclk);
            if (dut.video.vc < 720) begin
                // the output is 2 pixel clocks behind the counters
                repeat (2) @(posedge dut.pclk);
                for (x = 0; x < 1280; x = x + 1) begin
                    @(posedge dut.pclk); #1;
                    $fwrite(f, "%0d", {r, g, b});
                end
                $fwrite(f, "\n");
            end
        end
        $fclose(f);
        $display("errors: e_total %0d  underruns %0d  audio %0d", dut.accr[{!dut.bank, 5'd21}], dut.accr[{!dut.bank, 5'd17}], dut.accr[{!dut.bank, 5'd22}]);
        $finish;
    end
    // +big: the board's numbers, put in as each second ends
    initial if ($test$plusargs("big")) forever begin
            @(posedge dut.copied); #1;
            dut.accr[{!dut.bank, 5'd0}] = 6912000;  dut.accr[{!dut.bank, 5'd5}] = 16588842;
            dut.accr[{!dut.bank, 5'd4}] = 2014620;  dut.accr[{!dut.bank, 5'd6}] = 26320243;
            dut.accr[{!dut.bank, 5'd12}] = 12787124; dut.accr[{!dut.bank, 5'd13}] = 25000000;
            dut.accr[{!dut.bank, 5'd15}] = 15492568; dut.accr[{!dut.bank, 5'd8}] = 22551794;
        end
endmodule
