`timescale 1ns/1ps
// DVI transmitter for the Tang Nano 20K's HDMI connector: three TMDS data
// channels and the clock, each serialized 10:1 by an OSER10 (5x the pixel
// clock, both edges) into a TLVDS_OBUF pair. Plain DVI (no HDMI data
// islands), which HDMI and DVI monitors both take.
//
// pclk is the pixel clock and sclk five times it, from the same PLL through
// a CLKDIV (see dvi_clocks). The colour channels are 8 bits; the fpga_cpu's
// 3-bit colours (and dithered hi-color) are given full scale.
module dvi_tx (
    input  wire       pclk,
    input  wire       sclk,                 // 5 x pclk
    input  wire       rst,
    input  wire       de,                   // pixel data valid (not blanking)
    input  wire       hsync,
    input  wire       vsync,
    input  wire [7:0] r, g, b,
    output wire [3:0] tmds_p,               // {clock, data2 (red), data1 (green), data0 (blue)}
    output wire [3:0] tmds_n
);
    wire [9:0] q0, q1, q2;
    tmds_enc e0 (.clk(pclk), .d(b), .c({vsync, hsync}), .de(de), .q(q0));
    tmds_enc e1 (.clk(pclk), .d(g), .c(2'b00),          .de(de), .q(q1));
    tmds_enc e2 (.clk(pclk), .d(r), .c(2'b00),          .de(de), .q(q2));

    wire [9:0] words [0:3];
    assign words[0] = q0;
    assign words[1] = q1;
    assign words[2] = q2;
    assign words[3] = 10'b0000011111;       // the TMDS clock: one pixel period

    genvar i;
    generate
        for (i = 0; i < 4; i = i + 1) begin : lane
            wire s;
`ifdef SIM
            // simulation: the serial bit stream is not modelled
            assign s = words[i][0];
`else
            OSER10 ser (
                .D0(words[i][0]), .D1(words[i][1]), .D2(words[i][2]), .D3(words[i][3]),
                .D4(words[i][4]), .D5(words[i][5]), .D6(words[i][6]), .D7(words[i][7]),
                .D8(words[i][8]), .D9(words[i][9]),
                .FCLK(sclk), .PCLK(pclk), .RESET(rst), .Q(s)
            );
`endif
            TLVDS_OBUF obuf (.I(s), .O(tmds_p[i]), .OB(tmds_n[i]));
        end
    endgenerate
endmodule

// TMDS 8b/10b encoder (DVI 1.0, section 3.2.3), in three register stages
// (count the input's ones; transition-minimize and count again; DC-balance)
// to make 74.25 MHz. Control and data enable travel with the data.
module tmds_enc (
    input  wire       clk,
    input  wire [7:0] d,
    input  wire [1:0] c,                    // control in blanking: {c1, c0}
    input  wire       de,
    output reg  [9:0] q = 10'd0
);
    function [3:0] ones8;
        input [7:0] v;
        ones8 = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
    endfunction

    // stage 1
    reg [7:0] d1 = 8'd0;
    reg [1:0] c1 = 2'd0;
    reg       de1 = 1'b0;
    reg [3:0] n1d = 4'd0;
    always @(posedge clk) begin
        d1 <= d; c1 <= c; de1 <= de; n1d <= ones8(d);
    end

    // stage 2: transition minimizing
    wire       xnor_ = (n1d > 4'd4) || (n1d == 4'd4 && !d1[0]);
    wire [8:0] qm;
    assign qm[0] = d1[0];
    genvar i;
    generate
        for (i = 1; i < 8; i = i + 1) begin : m
            assign qm[i] = xnor_ ? ~(qm[i-1] ^ d1[i]) : (qm[i-1] ^ d1[i]);
        end
    endgenerate
    assign qm[8] = !xnor_;
    reg [8:0] qm2 = 9'd0;
    reg [1:0] c2 = 2'd0;
    reg       de2 = 1'b0;
    reg [3:0] n1q = 4'd0;
    always @(posedge clk) begin
        qm2 <= qm; c2 <= c1; de2 <= de1; n1q <= ones8(qm[7:0]);
    end

    // stage 3: DC balance
    wire signed [5:0] diff = $signed({2'b00, n1q}) - $signed(6'd4);   // ones - zeros, halved
    reg  signed [5:0] cnt = 6'sd0;          // running disparity
    always @(posedge clk)
        if (!de2) begin
            cnt <= 6'sd0;
            case (c2)
                2'b00: q <= 10'b1101010100;
                2'b01: q <= 10'b0010101011;
                2'b10: q <= 10'b0101010100;
                default: q <= 10'b1010101011;
            endcase
        end else if (cnt == 0 || diff == 0) begin
            q   <= {~qm2[8], qm2[8], qm2[8] ? qm2[7:0] : ~qm2[7:0]};
            cnt <= qm2[8] ? cnt + 2 * diff : cnt - 2 * diff;
        end else if ((cnt > 0 && diff > 0) || (cnt < 0 && diff < 0)) begin
            q   <= {1'b1, qm2[8], ~qm2[7:0]};
            cnt <= cnt + {4'd0, qm2[8], 1'b0} - 2 * diff;
        end else begin
            q   <= {1'b0, qm2[8], qm2[7:0]};
            cnt <= cnt - {4'd0, !qm2[8], 1'b0} + 2 * diff;
        end
endmodule

// 720p clocks: 27 MHz x 55/4 = 371.25 MHz serial, / 5 = 74.25 MHz pixels.
module dvi_clocks (
    input  wire clk27,
    output wire sclk,
    output wire pclk,
    output wire locked
);
`ifdef SIM
    reg p = 1'b0;
    always #6.734 p = ~p;
    assign pclk = p;
    assign sclk = 1'b0;
    assign locked = 1'b1;
`else
    rPLL #(
        .FCLKIN("27"),
`ifdef MODE480
        .IDIV_SEL(2), .FBDIV_SEL(13), .ODIV_SEL(4),     // 126 MHz: 640x480 at 25.2 MHz
`else
        .IDIV_SEL(3),                       // PFD 6.75 MHz
        .FBDIV_SEL(54),                     // x 55 = 371.25 MHz
        .ODIV_SEL(2),                       // VCO 742.5 MHz
`endif
        .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk27), .CLKOUT(sclk), .CLKOUTP(), .CLKOUTD(), .CLKOUTD3(), .LOCK(locked),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
    CLKDIV #(.DIV_MODE("5")) div (.HCLKIN(sclk), .RESETN(locked), .CALIB(1'b0), .CLKOUT(pclk));
`endif
endmodule
