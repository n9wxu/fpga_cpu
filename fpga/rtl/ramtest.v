`timescale 1ns/1ps
// RAM machine test for the Spartan-3 Starter Board: rammux.v and
// sram_phy.v under a realistic load from mock clients, every word read
// checked, with the read-capture and write-strobe clock phases movable at
// run time. Driven over JTAG by tools/ramtest.py.
//
// Clients (rammux ports, 0 first):
//   0 video     a line of 200 words (160 bitmap + 40 text, as video.v) every
//               LINE clocks, in bursts of 16 from region V, checked against
//               the pattern; a line not all back by its period is a miss
//   1 media     (none yet)
//   2 CPU write single words at random addresses in region C, one every
//               PACE clocks; data = the pattern ^ the round
//   3 CPU read  each written word read straight back (a read after a
//               write); then, at the round's end, all of them again
//   4 prefetch  bursts of 8 from random places in V, checked, two out at
//               a time, whenever the RAM is free (fills the rest of the bandwidth)
// Regions (word addresses): V = 00000-07FFF (filled with the pattern at
// the start), C = 20000-2FFFF.
//
// Registers, at word addresses 3FC00 up through the JTAG loader's port
// (byte address 0x0FF000; jtagload.py read_words/load):
//   0 control (rw): 0 run, 1 wmode (two-clock writes), 2 video, 3 prefetch,
//     4 CPU, 5 clear the counters (self-clearing), 6 fill V again,
//     15:8 PACE, 31:16 LINE
//   1 capture phase: write the target (DCM steps, signed); read {current, target}
//   2 write-strobe phase: the same
//   3 status (r): 0 V filled, 1 phases settled, 3:2 DCM overflow (STATUS[0])
//   4 clocks counted  5-9 words per port  10-14 errors per port
//   15 video misses  16 first error's address  17 its data  18 expected
//   19 clocks lost to bus turnarounds (and two-clock writes)  20 clocks with nothing asked
module ramtest (
    input  wire        clk,                 // 50 MHz pin
    input  wire [3:0]  btn,
    input  wire [7:0]  sw,
    output reg  [7:0]  led = 8'd0,
    output reg  [3:0]  an  = 4'b1111,
    output reg  [7:0]  seg = 8'hFF,
    output wire [17:0] sram_a,
    inout  wire [31:0] sram_d,
    output wire        sram_oe_n,
    output wire        sram_we_n,
    output wire [1:0]  sram_ce_n,
    output wire [3:0]  sram_be_n
);
    // ================================================================ clocks
    wire c50, ccap, cwe;
    reg  ps_go_c = 1'b0, ps_inc_c = 1'b0, ps_go_w = 1'b0, ps_inc_w = 1'b0;
    wire ps_done_c, ps_done_w;
    wire [7:0] st_c, st_w;
`ifdef SIM
    // simulation: fixed phases (ns) instead of DCMs
    parameter real CAP_NS = 5.0, WE_NS = 8.0;
    assign c50 = clk;
    assign #(CAP_NS) ccap = clk;
    assign #(WE_NS) cwe = clk;
    assign ps_done_c = ps_go_c, ps_done_w = ps_go_w;
    assign st_c = 8'd0, st_w = 8'd0;
`else
    wire clk_ibuf, c0, c0b, cc0, cw0;
    IBUFG ibuf (.I(clk), .O(clk_ibuf));
    // the logic's clock, deskewed
    DCM #(.CLKIN_PERIOD(20.0), .CLK_FEEDBACK("1X"), .STARTUP_WAIT("FALSE")) d0 (
        .CLKIN(clk_ibuf), .CLKFB(c50), .RST(1'b0), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLK0(c0));
    BUFG b0 (.I(c0), .O(c50));
    // read capture and write strobe: the logic's clock again (from it, so
    // their phases are relative to it), phases moved at run time
    DCM #(.CLKIN_PERIOD(20.0), .CLK_FEEDBACK("1X"), .CLKOUT_PHASE_SHIFT("VARIABLE"),
          .PHASE_SHIFT(64), .STARTUP_WAIT("FALSE")) dc (
        .CLKIN(c50), .CLKFB(ccap), .RST(1'b0), .PSCLK(c50), .PSEN(ps_go_c), .PSINCDEC(ps_inc_c),
        .DSSEN(1'b0), .CLK0(cc0), .PSDONE(ps_done_c), .STATUS(st_c));
    BUFG bc (.I(cc0), .O(ccap));
    DCM #(.CLKIN_PERIOD(20.0), .CLK_FEEDBACK("1X"), .CLKOUT_PHASE_SHIFT("VARIABLE"),
          .PHASE_SHIFT(100), .STARTUP_WAIT("FALSE")) dw (
        .CLKIN(c50), .CLKFB(cwe), .RST(1'b0), .PSCLK(c50), .PSEN(ps_go_w), .PSINCDEC(ps_inc_w),
        .DSSEN(1'b0), .CLK0(cw0), .PSDONE(ps_done_w), .STATUS(st_w));
    BUFG bw (.I(cw0), .O(cwe));
`endif

    // ================================================================ registers
    reg  [31:0] ctrl = {16'd2220, 8'd8, 8'b0001_1101};   // video, prefetch, CPU, run; wmode 0
    wire        run = ctrl[0], wmode = ctrl[1], en_v = ctrl[2], en_p = ctrl[3], en_c = ctrl[4];
    wire [7:0]  pace = ctrl[15:8];
    wire [15:0] lineper = ctrl[31:16];
    reg  signed [15:0] cap_t = 16'sd0, cap_c = 16'sd0, we_t = 16'sd0, we_c = 16'sd0;
    reg  [31:0] n_clk = 32'd0, n_mis = 32'd0, n_turn = 32'd0, n_idle = 32'd0;
    reg  [159:0] n_w = 160'd0, n_e = 160'd0;    // words, errors: 32 bits a port
    reg  [31:0] e_a = 32'd0, e_got = 32'd0, e_exp = 32'd0;
    reg         filled = 1'b0, clr = 1'b0, refill = 1'b0;

    // DCM phase steps toward the targets, one at a time
    reg  busy_c = 1'b0, busy_w = 1'b0;
    always @(posedge c50) begin
        ps_go_c <= 1'b0; ps_go_w <= 1'b0;
        if (busy_c) begin if (ps_done_c) busy_c <= 1'b0; end
        else if (cap_c != cap_t) begin
            ps_go_c <= 1'b1; ps_inc_c <= cap_t > cap_c; busy_c <= 1'b1;
            cap_c <= cap_c + ((cap_t > cap_c) ? 16'sd1 : -16'sd1);
        end
        if (busy_w) begin if (ps_done_w) busy_w <= 1'b0; end
        else if (we_c != we_t) begin
            ps_go_w <= 1'b1; ps_inc_w <= we_t > we_c; busy_w <= 1'b1;
            we_c <= we_c + ((we_t > we_c) ? 16'sd1 : -16'sd1);
        end
    end
    wire settled = !busy_c && !busy_w && cap_c == cap_t && we_c == we_t;

    // ================================================================ the RAM machine
    localparam NP = 5, AW = 18, LW = 5;
    reg  [NP-1:0]    p_req, p_we;
    reg  [NP*AW-1:0] p_addr;
    reg  [NP*LW-1:0] p_len;
    reg  [NP*32-1:0] p_wdata;
    reg  [NP*4-1:0]  p_wbe;
    wire [NP-1:0]    p_ack, p_wnext, p_rvalid;
    wire [31:0]      p_rdata;
    wire             phy_cmd, phy_we, phy_ready, phy_rvalid;
    wire [AW-1:0]    phy_addr;
    wire [31:0]      phy_wdata, phy_rdata;
    wire [3:0]       phy_be;
    rammux #(.NP(NP), .AW(AW), .LW(LW), .WMASK(5'b00100)) mux (
        .clk(c50), .p_req(p_req), .p_we(p_we), .p_addr(p_addr), .p_len(p_len), .p_ack(p_ack),
        .p_wdata(p_wdata), .p_wbe(p_wbe), .p_wnext(p_wnext), .p_rvalid(p_rvalid), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));
    sram_phy phy (
        .clk(c50), .clk_cap(ccap), .clk_we(cwe), .wmode(wmode),
        .cmd(phy_cmd), .we(phy_we), .addr(phy_addr), .wdata(phy_wdata), .be(phy_be),
        .ready(phy_ready), .rvalid(phy_rvalid), .rdata(phy_rdata),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n));

    // the pattern
    function [31:0] pat(input [17:0] a);
        pat = {~a[13:0], a} ^ {a[5:0], 26'h2A5A5A5};
    endfunction

    // a 32-bit LFSR step
    function [31:0] lfsr(input [31:0] x);
        lfsr = {x[30:0], x[31] ^ x[21] ^ x[1] ^ x[0]};
    endfunction

    // ================================================================ clients
    // ---- the fill of V (through port 2, before anything else)
    reg  [15:0] fa = 16'd0;                 // the next word to fill (bursts of 16)
    reg  [15:0] fa_w = 16'd0;               // the fill word being written now

    // ---- port 0: video
    reg  [15:0] lt = 16'd0;                 // clocks into the line
    reg  [14:0] vb = 15'd0;                 // the line's first word
    reg  [7:0]  vreq = 8'd0, vgot = 8'd0;   // words asked for, back
    reg         vact = 1'b0;

    // ---- ports 2 and 3: the CPU
    reg  [31:0] ws = 32'h1234_5678, wseed = 32'h1234_5678;   // write address LFSR, the round's seed
    reg  [7:0]  round = 8'd0;
    reg  [8:0]  wn = 9'd0;                  // writes this round
    reg  [7:0]  pc = 8'd0;                  // pace counter
    reg  [1:0]  cst = 2'd0;                 // 0 write, 1 read back, 2 read back all
    reg  [15:0] wa = 16'd0;                 // the address just written
    reg  [31:0] rs = 32'd0;                 // read-back LFSR
    reg  [8:0]  rn = 9'd0;
    reg  [15:0] ra = 16'd0;                 // the address being read
    reg         rwait = 1'b0;
    function [31:0] cpat(input [15:0] a, input [7:0] r);
        cpat = pat({2'b10, a}) ^ {4{r}};
    endfunction

    // ---- port 4: prefetch
    reg  [31:0] ps = 32'hCAFE_F00D;
    reg  [14:0] pb = 15'd0, pb2 = 15'd0;    // the first words of the burst coming back, and the next
    reg  [2:0]  pgot = 3'd0;                // the word of the burst (wraps at 8)
    reg  [1:0]  pact = 2'd0;                // bursts asked for, not all back

    // each port's request (wires, put together below: XST mis-builds
    // part-select assignments to the port vectors in an always block)
    wire        req0 = run && filled && en_v && vact && vreq < 8'd200;
    wire [17:0] adr0 = {3'b000, vb + {7'd0, vreq}};
    wire [4:0]  len0 = (8'd200 - vreq >= 8'd16) ? 5'd16 : (8'd200 - vreq);
    wire        req2 = filled ? (run && en_c && cst == 2'd0 && !rwait && pc == 8'd0) : 1'b1;
    wire [17:0] adr2 = filled ? {2'b10, ws[15:0]} : {2'b00, fa};
    wire [4:0]  len2 = filled ? 5'd1 : 5'd16;
    wire [31:0] wd2  = filled ? cpat(ws[15:0], round) : pat({2'b00, fa_w});
    wire        req3 = run && en_c && filled && (cst == 2'd1 || cst == 2'd2) && !rwait;
    wire [17:0] adr3 = {2'b10, cst == 2'd1 ? wa : rs[15:0]};
    wire        req4 = run && en_p && filled && pact != 2'd2;   // a second burst may be asked for while one comes back
    wire [17:0] adr4 = {3'b000, ps[14:3], 3'b000};
    always @* begin
        p_req   = {req4, req3, req2, 1'b0, req0};
        p_we    = 5'b00100;
        p_addr  = {adr4, adr3, adr2, 18'd0, adr0};
        p_len   = {5'd8, 5'd1, len2, 5'd0, len0};
        p_wdata = {64'd0, wd2, 64'd0};
        p_wbe   = 20'h00F00;
    end

    // checking reads: what each port expects for the word coming back now
    wire [31:0] exp0 = pat({3'b000, vb + {7'd0, vgot}});
    wire [31:0] exp3 = cpat(cst == 2'd1 ? wa : ra, round);
    wire [31:0] exp4 = pat({3'b000, pb + {12'd0, pgot}});
    wire [17:0] at0 = {3'b000, vb + {7'd0, vgot}}, at3 = {2'b10, cst == 2'd1 ? wa : ra},
                at4 = {3'b000, pb + {12'd0, pgot}};
    wire [4:0]  bad = {p_rvalid[4] && p_rdata != exp4, p_rvalid[3] && p_rdata != exp3, 2'b00,
                       p_rvalid[0] && p_rdata != exp0};
    genvar gp;
    generate
        for (gp = 0; gp < 5; gp = gp + 1) begin : cnt
            always @(posedge c50)
                if (clr) begin n_w[gp * 32 +: 32] <= 32'd0; n_e[gp * 32 +: 32] <= 32'd0; end
                else begin
                    if (p_rvalid[gp] || p_wnext[gp]) n_w[gp * 32 +: 32] <= n_w[gp * 32 +: 32] + 32'd1;
                    if (bad[gp]) n_e[gp * 32 +: 32] <= n_e[gp * 32 +: 32] + 32'd1;
                end
        end
    endgenerate

    always @(posedge c50) begin
        n_clk <= n_clk + 32'd1;
        if (phy_cmd && !phy_ready) n_turn <= n_turn + 32'd1;
        if (!phy_cmd) n_idle <= n_idle + 32'd1;
        // the first error
        if (e_a == 32'd0 && bad != 5'd0) begin
            e_a <= {14'd1, bad[0] ? at0 : bad[3] ? at3 : at4};
            e_got <= p_rdata;
            e_exp <= bad[0] ? exp0 : bad[3] ? exp3 : exp4;
        end

        // ---- fill
        if (!filled) begin
            if (p_wnext[2]) begin
                fa_w <= fa_w + 16'd1;
                if (fa_w == 16'h7FFF) filled <= 1'b1;
            end
            if (p_ack[2]) fa <= fa + 16'd16;
        end
        if (refill) begin filled <= 1'b0; fa <= 16'd0; fa_w <= 16'd0; end

        // ---- video
        lt <= (lt >= lineper - 16'd1) ? 16'd0 : lt + 16'd1;
        if (lt == 16'd0 && run && filled && en_v) begin
            if (vact && vgot != 8'd200) n_mis <= n_mis + 32'd1;   // the last line was late
            else begin
                vact <= 1'b1; vreq <= 8'd0; vgot <= 8'd0;
                vb <= (vb >= 15'd32400) ? 15'd0 : vb + 15'd200;
            end
        end
        if (p_ack[0]) vreq <= vreq + ((8'd200 - vreq >= 8'd16) ? 8'd16 : (8'd200 - vreq));
        if (p_rvalid[0]) begin
            vgot <= vgot + 8'd1;
        end

        // ---- CPU
        if (pc != 8'd0) pc <= pc - 8'd1;
        case (cst)
        2'd0: if (p_ack[2] && filled) begin
                  wa <= ws[15:0]; ws <= lfsr(ws); wn <= wn + 9'd1; pc <= pace; cst <= 2'd1;
              end
        2'd1: begin
                  if (p_ack[3]) rwait <= 1'b1;
                  if (p_rvalid[3]) begin
                      rwait <= 1'b0;
                      if (wn == 9'd256) begin cst <= 2'd2; rs <= wseed; rn <= 9'd0; end
                      else cst <= 2'd0;
                  end
              end
        2'd2: begin
                  if (p_ack[3]) begin rwait <= 1'b1; ra <= rs[15:0]; rs <= lfsr(rs); end
                  if (p_rvalid[3]) begin
                      rwait <= 1'b0; rn <= rn + 9'd1;
                      if (rn == 9'd255) begin
                          cst <= 2'd0; wn <= 9'd0; round <= round + 8'd1;
                          wseed <= ws; // the next round goes on from here
                      end
                  end
              end
        default: cst <= 2'd0;
        endcase

        // ---- prefetch
        // two bursts may be out: a queue of their first words (pb the head)
        begin : pf
            reg done;
            done = p_rvalid[4] && pgot == 3'd7;
            if (p_ack[4]) ps <= lfsr(ps);
            case ({p_ack[4], done})
                2'b10: begin if (pact == 2'd0) pb <= {ps[14:3], 3'b000}; else pb2 <= {ps[14:3], 3'b000}; end
                2'b01: pb <= pb2;
                2'b11: begin if (pact == 2'd1) pb <= {ps[14:3], 3'b000};
                             else begin pb <= pb2; pb2 <= {ps[14:3], 3'b000}; end end
                default: ;
            endcase
            pact <= pact + (p_ack[4] ? 2'd1 : 2'd0) - (done ? 2'd1 : 2'd0);
            if (p_rvalid[4]) pgot <= pgot + 3'd1;
        end

        if (clr) begin n_clk <= 32'd0; n_mis <= 32'd0; n_turn <= 32'd0; n_idle <= 32'd0; e_a <= 32'd0; end
    end

    // ================================================================ JTAG
    wire        ldr_req, ldr_rd;
    wire [17:0] ldr_addr;
    wire [31:0] ldr_data;
    reg         ldr_done = 1'b0;
    reg  [31:0] ldr_q = 32'd0;
    wire        rsel = ldr_addr[17:5] == 13'h1FE0;      // 3FC00-3FC1F
    wire [4:0]  ri = ldr_addr[4:0];
    reg  [31:0] rv;
    always @* case (ri)
        5'd0: rv = ctrl;
        5'd1: rv = {cap_c, cap_t};
        5'd2: rv = {we_c, we_t};
        5'd3: rv = {28'd0, st_w[0], st_c[0], settled, filled};
        5'd4: rv = n_clk;
        5'd5:  rv = n_w[31:0];
        5'd6:  rv = n_w[63:32];
        5'd7:  rv = n_w[95:64];
        5'd8:  rv = n_w[127:96];
        5'd9:  rv = n_w[159:128];
        5'd10: rv = n_e[31:0];
        5'd11: rv = n_e[63:32];
        5'd12: rv = n_e[95:64];
        5'd13: rv = n_e[127:96];
        5'd14: rv = n_e[159:128];
        5'd15: rv = n_mis;
        5'd16: rv = e_a;
        5'd17: rv = e_got;
        5'd18: rv = e_exp;
        5'd19: rv = n_turn;
        5'd20: rv = n_idle;
        default: rv = 32'hDEAD0000 | {27'd0, ri};
    endcase
    always @(posedge c50) begin
        ldr_done <= 1'b0; clr <= 1'b0; refill <= 1'b0;
        if (ctrl[5]) begin ctrl[5] <= 1'b0; clr <= 1'b1; end
        if (ctrl[6]) begin ctrl[6] <= 1'b0; refill <= 1'b1; end
        if (ldr_req && !ldr_done) begin
            ldr_done <= 1'b1;
            ldr_q <= rsel ? rv : 32'd0;
            if (!ldr_rd && rsel) case (ri)
                5'd0: ctrl <= ldr_data;
                5'd1: cap_t <= ldr_data[15:0];
                5'd2: we_t <= ldr_data[15:0];
                default: ;
            endcase
        end
    end
    wire [15:0] errs = n_e[15:0] + n_e[111:96] + n_e[143:128];
    jtagldr ldr (
        .clk(c50), .status({filled, settled, 14'd0, errs}),
        .run(), .entry(),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_q(ldr_q), .trace_rd(),
        .ldr_addr(ldr_addr), .ldr_data(ldr_data), .ldr_done(ldr_done)
    );

    // ================================================================ display: errors in hex; LEDs
    reg [17:0] refresh = 18'd0;
    always @(posedge c50) refresh <= refresh + 1'b1;
    wire [1:0] dsel = refresh[17:16];
    wire [6:0] glyph;
    seg7 dec (.digit(errs[4*dsel +: 4]), .glyph(glyph));
    always @(posedge c50) begin
        an  <= ~(4'b0001 << dsel);
        seg <= {1'b1, ~glyph};
        led <= {errs != 16'd0, n_mis != 32'd0, settled, filled, 1'b0, en_p, en_c, en_v};
    end
endmodule
