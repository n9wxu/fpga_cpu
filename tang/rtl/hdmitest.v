`timescale 1ns/1ps
// HDMI check for the Tang Nano 20K: 1280x720 60 Hz, eight colour bars, a
// grey ramp along the bottom, and a white square bouncing over them.
module hdmitest (
    input  wire       clk,                  // 27 MHz
    output wire [5:0] led,
    output wire [3:0] tmds_p,
    output wire [3:0] tmds_n
);
    wire sclk, pclk, locked;
    dvi_clocks clocks (.clk27(clk), .sclk(sclk), .pclk(pclk), .locked(locked));

`ifdef MODE480
    // 640x480 timing (bring-up): 640 + 16 + 96 + 48, 480 + 10 + 2 + 33, negative syncs
    localparam HA = 640, HT = 800, HS0 = 656, HS1 = 752, VA = 480, VT = 525, VS0 = 490, VS1 = 492, SP = 1'b0;
`else
    // 720p timing: 1280 + 110 + 40 + 220, 720 + 5 + 5 + 20, positive syncs
    localparam HA = 1280, HT = 1650, HS0 = 1390, HS1 = 1430, VA = 720, VT = 750, VS0 = 725, VS1 = 730, SP = 1'b1;
`endif
    reg [10:0] hc = 0;
    reg [9:0]  vc = 0;
    always @(posedge pclk)
        if (hc == HT - 1) begin
            hc <= 0;
            vc <= (vc == VT - 1) ? 10'd0 : vc + 1'b1;
        end else
            hc <= hc + 1'b1;
    wire de = hc < HA && vc < VA;
    wire hs = (hc >= HS0 && hc < HS1) == SP;
    wire vs = (vc >= VS0 && vc < VS1) == SP;

    // the square
    reg [10:0] bx = 100;
    reg [9:0]  by = 100;
    reg        dx = 1'b1, dy = 1'b1;
    always @(posedge pclk)
        if (hc == 0 && vc == VA) begin
            bx <= dx ? bx + 4'd4 : bx - 4'd4;
            by <= dy ? by + 4'd3 : by - 4'd3;
            if (dx && bx >= HA - 64 - 4) dx <= 1'b0;
            if (!dx && bx < 8)             dx <= 1'b1;
            if (dy && by >= VA - 64 - 3)  dy <= 1'b0;
            if (!dy && by < 6)             dy <= 1'b1;
        end
    wire box = hc >= bx && hc < bx + 64 && vc >= by && vc < by + 64;

    wire [2:0] bar = hc[10:7] < 10 ? 3'd7 - hc[9:7] : 3'd0;   // 160-pixel bars
    reg [7:0] r, g, b;
    reg       de_q, hs_q, vs_q;
    always @(posedge pclk) begin             // one register stage, sync included
        if (box)             {r, g, b} <= 24'hFFFFFF;
        else if (vc >= VA - 80)  {r, g, b} <= {3{hc[10:3]}};       // grey ramp
        else                 {r, g, b} <= {{8{bar[2]}}, {8{bar[1]}}, {8{bar[0]}}};
        de_q <= de; hs_q <= hs; vs_q <= vs;
    end

    dvi_tx tx (.pclk(pclk), .sclk(sclk), .rst(!locked), .de(de_q), .hsync(hs_q), .vsync(vs_q),
               .r(r), .g(g), .b(b), .tmds_p(tmds_p), .tmds_n(tmds_n));

    reg [25:0] blink = 0;
    always @(posedge pclk) blink <= blink + 1'b1;
    assign led = ~{locked, 4'd0, blink[25]};
endmodule
