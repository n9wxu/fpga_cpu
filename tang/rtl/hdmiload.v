`timescale 1ns/1ps
// HDMI audio under SDRAM load, for the Tang Nano 20K: hdmiaudio.v's 720p
// colour bars and 440/660 Hz tone (through the 67.5 MHz domain, as the
// system's audio engine), with the system's SDRAM PHY (rtl/sdram_phy.v,
// same clocks, phase and pin drive) kept busy by a traffic generator. It
// asks whether SDRAM traffic alone, with no CPU and no player, upsets the
// TV's HDMI audio packets.
//
// After the SDRAM's power-up, 64K words (256 KB) are filled with data that
// depends on the address (an xorshift of it: about half the bits change
// from word to word). Then the load cycles through four phases, 15 s each:
//   0  idle (refresh only)
//   1  reads: sequential, one word a clock (8-word bursts, back to back)
//   2  writes: sequential, one a clock as the PHY takes them
//   3  mixed: 8 reads, then a write, repeated (video bursts and CPU writes)
// Writes write the same address-derived data, so every read word can be
// checked: a mismatch lights LED 4 (and stays lit).
// LEDs (active low): 0 PLL locked, 1-2 the phase, 3 the SDRAM is ready,
// 4 a read word was wrong, 5 blink.
`ifndef SD_PSDA
`define SD_PSDA "0000"
`endif
module hdmiload (
    input  wire        clk,                 // 27 MHz
    input  wire        s1,
    output wire [5:0]  led,
    output wire [3:0]  tmds_p,
    output wire [3:0]  tmds_n,
    output wire        O_sdram_clk,
    output wire        O_sdram_cke,
    output wire        O_sdram_cs_n,
    output wire        O_sdram_ras_n,
    output wire        O_sdram_cas_n,
    output wire        O_sdram_wen_n,
    output wire [10:0] O_sdram_addr,
    output wire [1:0]  O_sdram_ba,
    output wire [3:0]  O_sdram_dqm,
    inout  wire [31:0] IO_sdram_dq
);
    wire sclk, pclk, locked;
    dvi_clocks clocks (.clk27(clk), .sclk(sclk), .pclk(pclk), .locked(locked));

    // 27 MHz x 5/2 = 67.5 MHz, and the SDRAM's clock, as system.v
    wire fclk, sdclk;
    rPLL #(
        .FCLKIN("27"), .IDIV_SEL(1), .FBDIV_SEL(4), .ODIV_SEL(8),
        .PSDA_SEL(`SD_PSDA), .DUTYDA_SEL("1000"), .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk), .CLKOUT(fclk), .CLKOUTP(sdclk), .CLKOUTD(), .CLKOUTD3(), .LOCK(),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
    assign O_sdram_clk = sdclk;

    // ---- the tone, through the FIFO, into HDMI --------------------------------------
    wire        s_valid, s_ready, over, a_pop, a_under;
    wire [15:0] s_l, s_r, a_l, a_r;
    hdmi_audio_tone tone (.clk(fclk), .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready), .over(over));
    hdmi_audio_cdc  fifo (.s_clk(fclk), .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready),
                          .p_clk(pclk), .p_pop(a_pop), .p_l(a_l), .p_r(a_r), .p_under(a_under));

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
    wire [2:0] bar = hc[10:7] < 10 ? 3'd7 - hc[9:7] : 3'd0;
    reg [7:0] r, g, b;
    reg       de_q, hs_q, vs_q;
    always @(posedge pclk) begin
        {r, g, b} <= {{8{bar[2]}}, {8{bar[1]}}, {8{bar[0]}}};
        de_q <= de; hs_q <= hs; vs_q <= vs;
    end
`ifndef REPLAY
    // -DNOAUDIO: no audio packets (a 48 kHz tick drains the FIFO instead)
`ifdef NOAUDIO
    localparam HA_ = 0;
    reg [10:0] np_t = 11'd0;
    always @(posedge pclk) np_t <= (np_t == 11'd1546) ? 11'd0 : np_t + 1'b1;
    wire tx_pop;
    assign a_pop = np_t == 11'd0;
`else
    localparam HA_ = 1;
    wire tx_pop;
    assign a_pop = tx_pop;
`endif
    dvi_tx #(.HDMI(1), .AUDIO(HA_)) tx (.pclk(pclk), .sclk(sclk), .rst(!locked), .de(de_q), .hsync(hs_q), .vsync(vs_q),
               .r(r), .g(g), .b(b), .a_l(a_l), .a_r(a_r), .a_pop(tx_pop), .tmds_p(tmds_p), .tmds_n(tmds_n));
`else
    // -DREPLAY: the data islands of a capture from the system (-DPKTCAP,
    // tools/pktcap_rom.py -> build/pk/replay.hex) in place of this one's.
    // dvi_tx (no pins) makes the stream; where its island starts (every
    // line has one) the ROM's next island goes out instead, word for word,
    // except channel 0's hsync and vsync bits, which are taken from dvi_tx's
    // own word at that pixel. If dvi_tx's island is the longer, its tail
    // becomes control period. The ROM loops.
    wire        isl;
    wire [29:0] nw;
    dvi_tx #(.HDMI(1), .AUDIO(1), .NOSER(1)) tx (.pclk(pclk), .sclk(sclk), .rst(!locked), .de(de_q), .hsync(hs_q), .vsync(vs_q),
               .r(r), .g(g), .b(b), .a_l(a_l), .a_r(a_r), .a_pop(a_pop), .tmds_p(), .tmds_n(), .isl(isl), .wout(nw));
    reg  [31:0] rom [0:8191];
    initial $readmemh("build/pk/replay.hex", rom);
    reg  [12:0] rp = 13'd0;
    reg  [31:0] rq = 32'd0;                 // rom[rp]
    reg         isl_p = 1'b0, on = 1'b0;
    wire        go   = isl && !isl_p;       // dvi_tx's island starts at this pixel
    wire        take = go || on;            // a ROM word goes out
    wire [12:0] rp_n = !take ? rp : rq[30] && rq[31] ? 13'd0 : rp + 1'b1;
    always @(posedge pclk) begin
        isl_p <= isl;
        rp <= rp_n;
        rq <= rom[rp_n];
        if (take) on <= !rq[31];
    end
    // hsync and vsync from dvi_tx's channel 0 word: a control token or TERC4
    function [1:0] hsvs(input [9:0] w);     // {vsync, hsync}
        case (w)
            10'b1101010100: hsvs = 2'd0; 10'b0010101011: hsvs = 2'd1;
            10'b0101010100: hsvs = 2'd2; 10'b1010101011: hsvs = 2'd3;
            default: hsvs = {t4d(w)};
        endcase
    endfunction
    function [1:0] t4d(input [9:0] w);      // a TERC4 word's low two bits
        case (w)
            10'b1010011100, 10'b0101110001, 10'b1011001100, 10'b1010001110: t4d = 2'd0;
            10'b1001100011, 10'b0100011110, 10'b0100111001, 10'b1001110001: t4d = 2'd1;
            10'b1011100100, 10'b0110001110, 10'b0110011100, 10'b0101100011: t4d = 2'd2;
            default:                                                       t4d = 2'd3;
        endcase
    endfunction
    function [1:0] t4h(input [9:0] w);      // a TERC4 word's high two bits
        case (w)
            10'b1010011100, 10'b1001100011, 10'b1011100100, 10'b1011100010: t4h = 2'd0;
            10'b0101110001, 10'b0100011110, 10'b0110001110, 10'b0100111100: t4h = 2'd1;
            10'b1011001100, 10'b0100111001, 10'b0110011100, 10'b1011000110: t4h = 2'd2;
            default:                                                       t4h = 2'd3;
        endcase
    endfunction
    function [9:0] ctl(input [1:0] c);
        case (c)
            2'd0: ctl = 10'b1101010100; 2'd1: ctl = 10'b0010101011;
            2'd2: ctl = 10'b0101010100; default: ctl = 10'b1010101011;
        endcase
    endfunction
    function [9:0] terc4(input [3:0] d);
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
    wire [1:0] sv = hsvs(nw[9:0]);
    wire       rc = rq[9:0] == 10'b1101010100 || rq[9:0] == 10'b0010101011 ||
                    rq[9:0] == 10'b0101010100 || rq[9:0] == 10'b1010101011;
    wire [9:0] r0 = rc ? ctl(sv) : terc4({t4h(rq[9:0]), sv});
    wire [29:0] ow = take ? {rq[29:10], r0}
                   : isl  ? {10'b1101010100, 10'b1101010100, ctl(sv)}
                   :        nw;
    reg  [29:0] owr = 30'd0;                // a register before the serializers
    always @(posedge pclk) owr <= ow;
    wire [9:0] lw [0:3];
    assign lw[0] = owr[9:0];
    assign lw[1] = owr[19:10];
    assign lw[2] = owr[29:20];
    assign lw[3] = 10'b0000011111;
    genvar i;
    generate for (i = 0; i < 4; i = i + 1) begin : lane
        wire s;
        OSER10 ser (
            .D0(lw[i][0]), .D1(lw[i][1]), .D2(lw[i][2]), .D3(lw[i][3]), .D4(lw[i][4]),
            .D5(lw[i][5]), .D6(lw[i][6]), .D7(lw[i][7]), .D8(lw[i][8]), .D9(lw[i][9]),
            .FCLK(sclk), .PCLK(pclk), .RESET(!locked), .Q(s)
        );
        TLVDS_OBUF obuf (.I(s), .O(tmds_p[i]), .OB(tmds_n[i]));
    end endgenerate
`endif

    // ---- the SDRAM and its load ------------------------------------------------------
    reg         cmd = 1'b0, we = 1'b0;
    reg  [20:0] addr = 21'd0;
    reg  [31:0] wdata = 32'd0;
    wire        ready, rvalid, sd_ready;
    wire [31:0] rdata;
    sdram_phy #(.INIT_WAIT(15000)) phy (
        .clk(fclk), .cap(3'd3),
        .in_cmd(cmd), .in_we(we), .in_addr(addr), .in_wdata(wdata), .in_be(4'hF),
        .ready(ready), .rvalid(rvalid), .rdata(rdata), .init_done(sd_ready),
        .sd_cke(O_sdram_cke), .sd_cs_n(O_sdram_cs_n), .sd_ras_n(O_sdram_ras_n),
        .sd_cas_n(O_sdram_cas_n), .sd_we_n(O_sdram_wen_n), .sd_a(O_sdram_addr),
        .sd_ba(O_sdram_ba), .sd_dqm(O_sdram_dqm), .sd_dq(IO_sdram_dq)
    );

    // the data at an address
    function [31:0] pat(input [15:0] a);
        reg [31:0] x;
        begin
            x = {a, ~a} ^ 32'h9E3779B9;
            x = x ^ (x << 13);
            x = x ^ (x >> 17);
            x = x ^ (x << 5);
            pat = x;
        end
    endfunction

    localparam [26:0] PHASE_T = 27'd67_500_000 - 1;   // 1 s at 67.5 MHz
    reg  [26:0] sec = 27'd0;
    reg  [3:0]  secs = 4'd0;
    reg  [1:0]  phase = 2'd0;
    reg         filled = 1'b0;
    reg  [15:0] wa = 16'd0, ra = 16'd0, ca = 16'd0;  // write, read issue, read check addresses
    reg  [3:0]  mix = 4'd0;
    reg         bad = 1'b0;

    // the next command, taken when ready
    wire do_w = !filled || phase == 2'd2 || (phase == 2'd3 && mix == 4'd8);
    wire do_r = filled && (phase == 2'd1 || (phase == 2'd3 && mix != 4'd8));
    always @(posedge fclk) begin
        if (filled) begin
            if (sec == PHASE_T) begin
                sec <= 27'd0;
                if (secs == 4'd14) begin secs <= 4'd0; phase <= phase + 1'b1; end
                else secs <= secs + 1'b1;
            end else
                sec <= sec + 1'b1;
        end
        if (!cmd || ready) begin
            cmd <= 1'b0;
            if (sd_ready && (do_w || do_r)) begin
                cmd <= 1'b1;
                if (do_w) begin
                    we <= 1'b1; addr <= {5'd0, wa}; wdata <= pat(wa);
                    wa <= wa + 1'b1;
                    if (wa == 16'hFFFF) filled <= 1'b1;
                end else begin
                    we <= 1'b0; addr <= {5'd0, ra};
                    ra <= ra + 1'b1;
                end
                mix <= (mix == 4'd8) ? 4'd0 : mix + 1'b1;
            end
        end
        if (rvalid) begin
            if (rdata != pat(ca)) bad <= 1'b1;
            ca <= ca + 1'b1;
        end
    end

    reg [25:0] blink = 0;
    always @(posedge pclk) blink <= blink + 1'b1;
`ifdef LOGICLOAD
    // -DLOGICLOAD: a ring of 6144 registers running rule 30 (each the XOR of
    // its left neighbour with itself OR its right one: about half of them
    // change every fclk), on for 10 s and off (held) for 10 s in turn, from
    // 10 s after configuration: the switching of a CPU's worth of logic,
    // with nothing in it that the HDMI path uses. LED 5 lit: the load is on.
    localparam LN = 6144;
    reg  [LN-1:0] rr = {{(LN-1){1'b0}}, 1'b1};
    reg  [26:0]   lt = 27'd0;
    reg  [4:0]    ls = 5'd0;
    reg           lon = 1'b0;
    always @(posedge fclk) begin
        lt <= (lt == 27'd67_499_999) ? 27'd0 : lt + 1'b1;
        if (lt == 27'd0) begin
            ls <= (ls == 5'd19) ? 5'd0 : ls + 1'b1;
            lon <= ls >= 5'd9 && ls != 5'd19;      // seconds 10-19 of each 20 on
        end
        if (lon) rr <= {rr[LN-2:0], rr[LN-1]} ^ (rr | {rr[0], rr[LN-1:1]});
    end
    // all of it observed (on LED 4 for one pixel clock in 2^26, unseen), so
    // none of it is optimized away
    reg rr_k = 1'b0;
    always @(posedge fclk) rr_k <= ^rr;
    assign led = ~{lon, bad | (rr_k && blink == 26'd0), sd_ready, phase, locked};
`else
    assign led = ~{blink[25], bad, sd_ready, phase, locked};
`endif
endmodule
