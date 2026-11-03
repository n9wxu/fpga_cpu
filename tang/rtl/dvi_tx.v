`timescale 1ns/1ps
// DVI/HDMI transmitter for the Tang Nano 20K's HDMI connector: three TMDS
// data channels and the clock, each serialized 10:1 by an OSER10 (5x the
// pixel clock, both edges) into a TLVDS_OBUF pair.
//
// HDMI = 0: plain DVI. HDMI = 1: HDMI framing (video preambles and guard
// bands) and, once a frame, an AVI InfoFrame in a data island in vertical
// blank, saying the picture is 720p RGB, IT content, to be shown whole
// (underscan): without it TVs overscan and crop the edges. Video is
// delayed 12 pixels so the preamble can come before the active area.
//
// AUDIO = 1 (needs HDMI = 1; 720p at 74.25 MHz): 48 kHz 16-bit stereo L-PCM.
// The pixel-clock domain pulls a sample from the sink's FIFO (a_pop, with
// a_l/a_r valid in that cycle: hdmi_audio_cdc.v) at an exact 8 per 12375
// pixels, so the sample clock is the ACR's N/CTS = 6144/74250 with no drift,
// and 800 samples fill a frame. Every line has a data island at hsync with
// the packets due: Audio Sample (the samples taken since the last line, up
// to 4, parity and channel status in hardware), Audio Clock Regeneration
// (every 8th line), and once a frame the AVI and Audio InfoFrames. Every
// packet's BCH ECC is computed as it is sent. Packet types: HDMI 1.4, 5.3.
//
// pclk is the pixel clock and sclk five times it, from the same PLL through
// a CLKDIV (see dvi_clocks). The colour channels are 8 bits; the fpga_cpu's
// 3-bit colours are given full scale.
module dvi_tx #(
    parameter HDMI  = 0,
    parameter AUDIO = 0
) (
    input  wire       pclk,
    input  wire       sclk,                 // 5 x pclk
    input  wire       rst,
    input  wire       de,                   // pixel data valid (not blanking)
    input  wire       hsync,
    input  wire       vsync,                // starts on hsync's leading edge
    input  wire [7:0] r, g, b,
    input  wire [15:0] a_l, a_r,            // AUDIO: the sample (signed) when a_pop is high
    output wire       a_pop,                // AUDIO: pixel-clock pulse, 48 kHz average
    output wire [3:0] tmds_p,               // {clock, data2 (red), data1 (green), data0 (blue)}
    output wire [3:0] tmds_n
);
    wire [9:0] words [0:3];
    assign words[3] = 10'b0000011111;       // the TMDS clock: one pixel period

    generate if (!HDMI) begin : dvi
        wire [9:0] q0, q1, q2;
        tmds_enc e0 (.clk(pclk), .d(b), .c({vsync, hsync}), .de(de), .q(q0));
        tmds_enc e1 (.clk(pclk), .d(g), .c(2'b00),          .de(de), .q(q1));
        tmds_enc e2 (.clk(pclk), .d(r), .c(2'b00),          .de(de), .q(q2));
        assign words[0] = q0;
        assign words[1] = q1;
        assign words[2] = q2;
        assign a_pop = 1'b0;
    end else begin : hdmi
        localparam [9:0] VGB_02 = 10'b1011001100,   // video guard band, channels 0 and 2
                         GB_1   = 10'b0100110011;   // channel 1 (and 2 in a data island)

        // ---- 12-pixel delay; de ahead of it places the preamble ----------
        localparam N = 12;
        reg [N-1:0] de_d = 0, hs_d = 0, vs_d = 0;
        reg [23:0]  px_d [0:N-1];
        integer k;
        always @(posedge pclk) begin
            de_d <= {de, de_d[N-1:1]};
            hs_d <= {hsync, hs_d[N-1:1]};
            vs_d <= {vsync, vs_d[N-1:1]};
            px_d[N-1] <= {r, g, b};
            for (k = 0; k < N - 1; k = k + 1) px_d[k] <= px_d[k + 1];
        end
        // at the output end (index 0): the pixel now; 1..10 are the next ten
        wire o_de = de_d[0], o_hs = hs_d[0], o_vs = vs_d[0];
        wire v_guard = !o_de && (de_d[1] || de_d[2]);           // 2 pixels before video
        wire v_pre   = !o_de && !v_guard && |de_d[10:3];          // the 8 before those

        // ---- data island engine ------------------------------------------
        // An island is 8 preamble pixels, 2 guard, 32 per packet, 2 guard; it
        // starts at hsync's leading edge (the control period before it is
        // 110 pixels, after it 250 minus the packets). Packets go out in slot
        // order, as many as are due: 0 ACR, 1 audio samples, 2 AVI, 3 Audio
        // InfoFrame. Without AUDIO only the AVI InfoFrame, once a frame, from
        // the first vsync line's hsync.
        localparam [3:0] S_ACR = 0, S_SMP = 1, S_AVI = 2, S_AIF = 3;
        reg       pend = 1'b0, vs_q = 1'b0, hs_q = 1'b0;
        reg [2:0] ph = 3'd0;                    // 0 idle, 1 preamble, 2 guard, 3 packet, 4 guard
        reg [4:0] ic = 5'd0;                    // pixel in the phase
        reg [3:0] pm = 4'd0;                    // slots still to send
        reg [2:0] lc = 3'd0;                    // lines, for the ACR rate
        reg [2:0] ns = 3'd0;                    // samples this island's sample packet carries
        wire [2:0] nsnap;                       // samples waiting now (up to 4)
        wire [3:0] due = {AUDIO != 0 && pend, pend, nsnap != 3'd0, AUDIO != 0 && lc == 3'd0};
        wire       hs_edge = o_hs && !hs_q;
        wire       start   = hs_edge && ph == 3'd0 && |due;
        wire       last_px = ph == 3'd3 && ic == 5'd31;
        wire       ld      = (ph == 3'd2 && ic == 5'd1) || (last_px && |pm);
        wire [1:0] sl      = pm[0] ? 2'd0 : pm[1] ? 2'd1 : pm[2] ? 2'd2 : 2'd3;
        wire [23:0]  n_hdr;                     // the packet to load, without ECC
        wire [223:0] n_pay;
        always @(posedge pclk) begin
            vs_q <= o_vs; hs_q <= o_hs;
            if (hs_edge) lc <= lc + 1'b1;
            if (start) begin pend <= 1'b0; ph <= 3'd1; ic <= 5'd0; pm <= due; ns <= nsnap; end
            else begin
                if (ld) pm[sl] <= 1'b0;
                case (ph)
                    3'd1: begin ic <= ic + 1'b1; if (ic == 5'd7) begin ph <= 3'd2; ic <= 5'd0; end end
                    3'd2: begin ic <= ic + 1'b1; if (ic == 5'd1) begin ph <= 3'd3; ic <= 5'd0; end end
                    3'd3: begin ic <= ic + 1'b1; if (last_px) begin ph <= |pm ? 3'd3 : 3'd4; ic <= 5'd0; end end
                    3'd4: begin ic <= ic + 1'b1; if (ic == 5'd1) ph <= 3'd0; end
                    default: ;
                endcase
            end
            if (o_vs && !vs_q) pend <= 1'b1;
        end
        wire d_pre   = ph == 3'd1;
        wire d_guard = ph == 3'd2 || ph == 3'd4;
        wire d_data  = ph == 3'd3;
        wire [4:0] pi = ic;                                       // packet pixel 0-31

        // the packet on the wire: header (24 bits, then 8 of ECC) on channel 0;
        // channels 1 and 2 carry the four subpackets, 56 bits and 8 of ECC
        // each, bit 2i and 2i+1 in pixel i. Each ECC is a serial LFSR (BCH,
        // x^8 + x^7 + x + 1) fed the bits as they go, and sent after them.
        reg [23:0] hr;  reg [7:0] he;
        reg [55:0] sr [0:3];  reg [7:0] se [0:3];
        function [7:0] bch;
            input [7:0] e;
            input       b;
            bch = (e >> 1) ^ ((e[0] ^ b) ? 8'h83 : 8'h00);
        endfunction
        reg [3:0] te, to;                                         // subpacket bits 2i, 2i+1
        integer j;
        always @* begin
            for (j = 0; j < 4; j = j + 1) begin
                te[j] = pi < 5'd28 ? sr[j][0] : se[j][0];
                to[j] = pi < 5'd28 ? sr[j][1] : se[j][1];
            end
        end
        integer m;
        always @(posedge pclk) begin
            if (ld) begin
                hr <= n_hdr; he <= 8'd0;
                for (m = 0; m < 4; m = m + 1) begin sr[m] <= n_pay[56 * m +: 56]; se[m] <= 8'd0; end
            end else if (d_data) begin
                if (pi < 5'd24) begin hr <= hr >> 1; he <= bch(he, hr[0]); end
                else he <= he >> 1;
                for (m = 0; m < 4; m = m + 1)
                    if (pi < 5'd28) begin
                        sr[m] <= sr[m] >> 2;
                        se[m] <= bch(bch(se[m], sr[m][0]), sr[m][1]);
                    end else se[m] <= se[m] >> 2;
            end
        end
        wire [3:0] t0 = d_guard ? {2'b11, o_vs, o_hs}
                                : {pi != 0, pi < 5'd24 ? hr[0] : he[0], o_vs, o_hs};
        wire [3:0] t1 = te;
        wire [3:0] t2 = to;

        // ---- the packets -----------------------------------------------------
        // AVI InfoFrame (tools/avi_packet.py): RGB, 16:9, VIC 4, IT graphics
        localparam [23:0]  AVI_HDR = 24'h0D0282;
        localparam [55:0]  AVI_SB0 = 56'h00000488_2812A9;
        // Audio InfoFrame: 2 channels, the rest from the stream (PB0 = checksum)
        localparam [23:0]  AIF_HDR = 24'h0A0184;
        localparam [55:0]  AIF_SB0 = 56'h00000000_000170;
        // Audio Clock Regeneration: 48 kHz at 74.25 MHz (HDMI 1.4 table 7-1)
        localparam [19:0]  ACR_N = 20'd6144, ACR_CTS = 20'd74250;
        localparam [55:0]  ACR_SB = {ACR_N[7:0], ACR_N[15:8], 4'd0, ACR_N[19:16],
                                     ACR_CTS[7:0], ACR_CTS[15:8], 4'd0, ACR_CTS[19:16], 8'd0};
        wire [23:0]  smp_hdr;
        wire [223:0] smp_pay;
        assign n_hdr = sl == S_ACR ? 24'h000001 : sl == S_SMP ? smp_hdr : sl == S_AVI ? AVI_HDR : AIF_HDR;
        assign n_pay = sl == S_ACR ? {4{ACR_SB}} : sl == S_SMP ? smp_pay
                     : sl == S_AVI ? {168'd0, AVI_SB0} : {168'd0, AIF_SB0};

        if (AUDIO) begin : aud
            // the 48 kHz tick: 8 per 12375 pixels
            reg [13:0] acc = 14'd0;
            wire [13:0] accn = acc + 14'd8;
            wire        tick = accn >= 14'd12375;
            always @(posedge pclk) acc <= tick ? accn - 14'd12375 : accn;
            assign a_pop = tick;

            // samples taken, waiting for the next island
            reg [31:0] q [0:7];
            reg [2:0]  wp = 3'd0, rp = 3'd0;
            wire [2:0] qn = wp - rp;
            assign nsnap = qn > 3'd4 ? 3'd4 : qn;
            always @(posedge pclk) if (tick) begin q[wp] <= {a_l, a_r}; wp <= wp + 1'b1; end

            // IEC 60958 channel status, consumer, L-PCM, no copyright, 48 kHz,
            // 16 bits (max 20): bytes 0, 3, 4 = 04, 02, 02; all else zero
            localparam [191:0] CS = 192'h02_02_00_00_04;
            reg [7:0] fc = 8'd0;                // frame in the 192-frame block of the next sample
            reg [31:0] smp [0:3];
            reg [7:0]  fi [0:3];
            reg [55:0] sp_ [0:3];
            reg [3:0]  bf, pr;
            integer i;
            always @* begin
                bf = 4'd0; pr = 4'd0;
                for (i = 0; i < 4; i = i + 1) begin
                    smp[i] = q[rp + i[2:0]];
                    fi[i]  = (fc + i) >= 192 ? fc + i - 192 : fc + i;
                    pr[i]  = i < ns;
                    bf[i]  = i < ns && fi[i] == 8'd0;
                    sp_[i] = i >= ns ? 56'd0 :
                        {^smp[i][15:0] ^ CS[fi[i]], CS[fi[i]], 2'b00, ^smp[i][31:16] ^ CS[fi[i]], CS[fi[i]], 2'b00,
                         smp[i][15:8], smp[i][7:0], 8'd0, smp[i][31:24], smp[i][23:16], 8'd0};
                end
            end
            assign smp_hdr = {bf, 4'd0, 4'd0, pr, 8'h02};
            assign smp_pay = {sp_[3], sp_[2], sp_[1], sp_[0]};
            always @(posedge pclk) if (ld && sl == S_SMP) begin
                rp <= rp + ns;
                fc <= (fc + ns) >= 192 ? fc + ns - 192 : fc + ns;
            end
        end else begin : noaud
            assign a_pop    = 1'b0;
            assign nsnap    = 3'd0;
            assign smp_hdr  = 24'd0;
            assign smp_pay  = 224'd0;
        end

        // ---- encoders (3 clocks) and the words that replace their output --
        wire [9:0] q0, q1, q2;
        tmds_enc e0 (.clk(pclk), .d(px_d[0][7:0]),   .c({o_vs, o_hs}), .de(o_de), .q(q0));
        tmds_enc e1 (.clk(pclk), .d(px_d[0][15:8]),  .c({1'b0, v_pre | d_pre}), .de(o_de), .q(q1));
        tmds_enc e2 (.clk(pclk), .d(px_d[0][23:16]), .c({1'b0, d_pre}), .de(o_de), .q(q2));

        reg [29:0] sp [0:2];                // special words, 3 clocks to meet the encoders
        reg [2:0]  spv = 3'b000;
        always @(posedge pclk) begin
            spv   <= {v_guard | d_guard | d_data, spv[2:1]};
            sp[2] <= v_guard ? {VGB_02, GB_1, VGB_02}
                   : d_guard ? {GB_1, GB_1, terc4(t0)}
                   :           {terc4(t2), terc4(t1), terc4(t0)};
            sp[1] <= sp[2];
            sp[0] <= sp[1];
        end
        reg [29:0] w = 30'd0;
        always @(posedge pclk) w <= spv[0] ? sp[0] : {q2, q1, q0};
        assign words[0] = w[9:0];
        assign words[1] = w[19:10];
        assign words[2] = w[29:20];
    end endgenerate

    function [9:0] terc4;
        input [3:0] d;
        case (d)
            4'h0: terc4 = 10'b1010011100;  4'h1: terc4 = 10'b1001100011;
            4'h2: terc4 = 10'b1011100100;  4'h3: terc4 = 10'b1011100010;
            4'h4: terc4 = 10'b0101110001;  4'h5: terc4 = 10'b0100011110;
            4'h6: terc4 = 10'b0110001110;  4'h7: terc4 = 10'b0100111100;
            4'h8: terc4 = 10'b1011001100;  4'h9: terc4 = 10'b0100111001;
            4'hA: terc4 = 10'b0110011100;  4'hB: terc4 = 10'b1011000110;
            4'hC: terc4 = 10'b1010001110;  4'hD: terc4 = 10'b1001110001;
            4'hE: terc4 = 10'b0101100011;  default: terc4 = 10'b1011000011;
        endcase
    endfunction

    genvar i;
    generate
        for (i = 0; i < 4; i = i + 1) begin : lane
            wire s;
`ifdef SIM
            // simulation: the serial bit stream is not modelled
            assign s = words[i][0];
            assign tmds_p[i] = s;
            assign tmds_n[i] = ~s;
`else
            OSER10 ser (
                .D0(words[i][0]), .D1(words[i][1]), .D2(words[i][2]), .D3(words[i][3]),
                .D4(words[i][4]), .D5(words[i][5]), .D6(words[i][6]), .D7(words[i][7]),
                .D8(words[i][8]), .D9(words[i][9]),
                .FCLK(sclk), .PCLK(pclk), .RESET(rst), .Q(s)
            );
            TLVDS_OBUF obuf (.I(s), .O(tmds_p[i]), .OB(tmds_n[i]));
`endif
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
