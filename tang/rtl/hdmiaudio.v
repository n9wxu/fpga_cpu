`timescale 1ns/1ps
// HDMI audio check for the Tang Nano 20K: 1280x720 60 Hz colour bars, and a
// stereo test tone, 440 Hz left and 660 Hz right, 48 kHz 16-bit, in the TV's
// speakers. The tone comes from a 67.5 MHz domain (the system's clock, a
// second PLL) through hdmi_audio_cdc into dvi_tx, as an audio engine would.
// LEDs (active low): 0 locked, 1 a sample was lost, 2 the FIFO ran dry, 5 blink.
module hdmiaudio (
    input  wire       clk,                  // 27 MHz
    output wire [5:0] led,
    output wire [3:0] tmds_p,
    output wire [3:0] tmds_n
);
    wire sclk, pclk, locked;
    dvi_clocks clocks (.clk27(clk), .sclk(sclk), .pclk(pclk), .locked(locked));

    // 27 MHz x 5/2 = 67.5 MHz, as system.v
    wire fclk;
`ifdef SIM
    reg f = 1'b0;
    always #7.407 f = ~f;
    assign fclk = f;
`else
    rPLL #(
        .FCLKIN("27"), .IDIV_SEL(1), .FBDIV_SEL(4), .ODIV_SEL(8), .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk), .CLKOUT(fclk), .CLKOUTP(), .CLKOUTD(), .CLKOUTD3(), .LOCK(),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
`endif

    // the "engine"
    wire        s_valid, s_ready, over, a_pop, a_under;
    wire [15:0] s_l, s_r, a_l, a_r;
    hdmi_audio_tone tone (.clk(fclk), .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready), .over(over));
    hdmi_audio_cdc  fifo (.s_clk(fclk), .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready),
                          .p_clk(pclk), .p_pop(a_pop), .p_l(a_l), .p_r(a_r), .p_under(a_under));

    // 720p timing: 1280 + 110 + 40 + 220, 720 + 5 + 5 + 20, positive syncs
    localparam HA = 1280, HT = 1650, HS0 = 1390, HS1 = 1430, VA = 720, VT = 750, VS0 = 725, VS1 = 730;
    reg [10:0] hc = 0;
    reg [9:0]  vc = 0;
    always @(posedge pclk)
        if (hc == HT - 1) begin
            hc <= 0;
            vc <= (vc == VT - 1) ? 10'd0 : vc + 1'b1;
        end else
            hc <= hc + 1'b1;
    wire de = hc < HA && vc < VA;
    wire hs = hc >= HS0 && hc < HS1;
    wire vs = vc >= VS0 && vc < VS1;

    wire [2:0] bar = hc[10:7] < 10 ? 3'd7 - hc[9:7] : 3'd0;   // 160-pixel bars
    reg [7:0] r, g, b;
    reg       de_q, hs_q, vs_q;
    always @(posedge pclk) begin
        {r, g, b} <= {{8{bar[2]}}, {8{bar[1]}}, {8{bar[0]}}};
        de_q <= de; hs_q <= hs; vs_q <= vs;
    end

    dvi_tx #(.HDMI(1), .AUDIO(1)) tx (.pclk(pclk), .sclk(sclk), .rst(!locked), .de(de_q), .hsync(hs_q), .vsync(vs_q),
               .r(r), .g(g), .b(b), .a_l(a_l), .a_r(a_r), .a_pop(a_pop), .tmds_p(tmds_p), .tmds_n(tmds_n));

    reg [25:0] blink = 0;
    // the FIFO is empty at the first pops while the PLLs settle: only a dry
    // run after the first half second counts
    reg        dry_seen = 1'b0;
    always @(posedge pclk) begin
        blink <= blink + 1'b1;
        if (blink[25]) dry_seen <= dry_seen | a_under;
    end
    assign led = ~{blink[25], 2'd0, dry_seen, over, locked};
endmodule
