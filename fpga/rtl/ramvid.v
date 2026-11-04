`timescale 1ns/1ps
// The RAM machine's status monitor, on the Spartan-3 Starter Board: the
// RAM machine (rammux.v + sram_phy.v) serving a 1280x720 framebuffer in
// SRAM (fbvideo.v, 3-bit colour, 4 bits a pixel), a mock CPU (mockcpu.v:
// the T1 demo's memory traffic, FC-like at 25 MHz) and a drawer that
// renders the status screen into the framebuffer through the RAM machine
// too. Once a second every counter is copied; the formatter writes them
// as text (tools/mkstatus.py lays the screen out) and the drawer redraws
// the framebuffer from the text.
//
// Ports, highest priority first:
//   0 video       fbvideo: 160 words a line, bursts of 16
//   1 audio       48 words (stereo 16-bit samples) every 1 ms, as three
//                 bursts of 16, from T1 (checked); not all back within the
//                 millisecond is a miss
//   2 CPU writes  the write buffer, single words, region T2
//   3 CPU reads   loads, single words, region T1 (or T2: read after write)
//   4 CPU fetch   prefetch, bursts of 8, region T1
//   5 draw        the status screen into the framebuffer, bursts of 16
//                 (first: the fill of T1 with the pattern)
// Regions (words): framebuffer 00000-1C1FF (written by the drawer only),
// T1 20000-27FFF, T2 30000-37FFF.
//
// Registers through the JTAG loader's port (byte address 0x0FF000 up):
//   word 3FC00 control: 0 CPU on, 1 draw continuously (else once a
//     second), 2 wmode (two-clock writes)
//   words 3FC20 + i: source i of the last second (mkstatus.py's SRC)
module ramvid (
    input  wire        clk,                 // 50 MHz pin
    input  wire [3:0]  btn,
    input  wire [7:0]  sw,
    output reg  [7:0]  led = 8'd0,
    output reg  [3:0]  an  = 4'b1111,
    output reg  [7:0]  seg = 8'hFF,
    output wire        vga_r, vga_g, vga_b, vga_hs, vga_vs,
    output wire [17:0] sram_a,
    inout  wire [31:0] sram_d,
    output wire        sram_oe_n,
    output wire        sram_we_n,
    output wire [1:0]  sram_ce_n,
    output wire [3:0]  sram_be_n
);
    localparam SEC = 50_000_000;

    // ================================================================ clocks
    wire c50, ccap, cwe, pclk;
`ifdef SIM
    parameter SIM_SEC = 0;                  // a shorter "second" in simulation
    assign c50 = clk;
    assign #5 ccap = clk;
    assign cwe = ccap;                     // the write strobe: the capture clock
    reg pclk_r = 1'b0;
    always #6.734 pclk_r = ~pclk_r;
    assign pclk = pclk_r;
`else
    localparam SIM_SEC = 0;
    wire clk_ibuf, c0, c90, cw0, fx1, fx1_buf, fx2, lk1;
    IBUFG ibuf (.I(clk), .O(clk_ibuf));
    // the logic's clock, deskewed, and a quarter later: read capture
    DCM #(.CLKIN_PERIOD(20.0), .CLK_FEEDBACK("1X"), .STARTUP_WAIT("FALSE")) d0 (
        .CLKIN(clk_ibuf), .CLKFB(c50), .RST(1'b0), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLK0(c0), .CLK90(c90));
    BUFG b0 (.I(c0), .O(c50));
    BUFG bc (.I(c90), .O(ccap));
    // the write strobe: the same quarter-clock (WE# low 5-15 ns into the
    // write's 20 ns; ramtest.v's sweep found writes clean well either side)
    assign cwe = ccap;
    // the pixel clock, 74.25 MHz (clkgen.v's two DCMs, from c50)
    DCM #(.CLKIN_PERIOD(20.0), .CLKFX_MULTIPLY(27), .CLKFX_DIVIDE(20), .CLK_FEEDBACK("NONE"),
          .DFS_FREQUENCY_MODE("LOW"), .STARTUP_WAIT("FALSE")) dp1 (
        .CLKIN(c50), .CLKFB(1'b0), .RST(1'b0), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLKFX(fx1), .LOCKED(lk1));
    BUFG bp1 (.I(fx1), .O(fx1_buf));
    reg [15:0] rst2 = 16'hFFFF;
    always @(posedge c50) rst2 <= lk1 ? {rst2[14:0], 1'b0} : 16'hFFFF;
    DCM #(.CLKIN_PERIOD(14.815), .CLKFX_MULTIPLY(11), .CLKFX_DIVIDE(10), .CLK_FEEDBACK("NONE"),
          .DFS_FREQUENCY_MODE("LOW"), .STARTUP_WAIT("FALSE")) dp2 (
        .CLKIN(fx1_buf), .CLKFB(1'b0), .RST(rst2[15]), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLKFX(fx2));
    BUFG bpix (.I(fx2), .O(pclk));
`endif
    localparam [25:0] TICKS = (SIM_SEC != 0) ? SIM_SEC : SEC;

    reg  [31:0] ctrl = 32'h0000_0001;       // CPU on, draw once a second, wmode 0
    wire        cpu_on = ctrl[0], draw_cont = ctrl[1], wmode = ctrl[2];

    function [31:0] pat(input [17:0] a);
        pat = {~a[13:0], a} ^ {a[5:0], 26'h2A5A5A5};
    endfunction

    // ================================================================ the RAM machine
    localparam NP = 6, AW = 18, LW = 5;
    wire [NP-1:0]    p_req, p_we;
    wire [NP*AW-1:0] p_addr;
    wire [NP*LW-1:0] p_len;
    wire [NP*32-1:0] p_wdata;
    wire [NP-1:0]    p_ack, p_wnext, p_rvalid;
    wire [31:0]      p_rdata;
    wire             phy_cmd, phy_we, phy_ready, phy_rvalid;
    wire [AW-1:0]    phy_addr;
    wire [31:0]      phy_wdata, phy_rdata;
    wire [3:0]       phy_be;
    rammux #(.NP(NP), .AW(AW), .LW(LW), .WMASK(6'b100100)) mux (
        .clk(c50), .p_req(p_req), .p_we(p_we), .p_addr(p_addr), .p_len(p_len), .p_ack(p_ack),
        .p_wdata(p_wdata), .p_wbe({NP{4'hF}}), .p_wnext(p_wnext), .p_rvalid(p_rvalid), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));
    sram_phy phy (
        .clk(c50), .clk_cap(ccap), .clk_we(cwe), .wmode(wmode),
        .cmd(phy_cmd), .we(phy_we), .addr(phy_addr), .wdata(phy_wdata), .be(phy_be),
        .ready(phy_ready), .rvalid(phy_rvalid), .rdata(phy_rdata),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n));

    // ---- port 0: video
    wire        v_req, v_underrun, v_line;
    wire [17:0] v_addr;
    wire [4:0]  v_len;
    fbvideo #(.BASE(18'h00000)) video (
        .clk(c50), .pclk(pclk), .req(v_req), .addr(v_addr), .len(v_len), .ack(p_ack[0]),
        .rvalid(p_rvalid[0]), .rdata(p_rdata), .underrun(v_underrun), .line_done(v_line),
        .hsync(vga_hs), .vsync(vga_vs), .rgb({vga_r, vga_g, vga_b}));

    // ---- ports 2-4: the mock CPU
    reg         filled = 1'b0;
    wire        w_req, r_req, f_req;
    wire [17:0] w_addr, r_addr, f_addr;
    wire [31:0] w_data;
    wire        c_cycle, c_inst, c_sfetch, c_sload, c_sstore, c_err_r, c_err_f;
    mockcpu #(.T1(18'h20000), .T2(18'h30000)) cpu (
        .clk(c50), .en(cpu_on && filled),
        .w_req(w_req), .w_addr(w_addr), .w_data(w_data), .w_ack(p_ack[2]),
        .r_req(r_req), .r_addr(r_addr), .r_ack(p_ack[3]), .r_valid(p_rvalid[3]),
        .f_req(f_req), .f_addr(f_addr), .f_ack(p_ack[4]), .f_valid(p_rvalid[4]), .rdata(p_rdata),
        .c_cycle(c_cycle), .c_inst(c_inst), .c_sfetch(c_sfetch), .c_sload(c_sload),
        .c_sstore(c_sstore), .c_err_r(c_err_r), .c_err_f(c_err_f));

    // ---- port 1: audio
    reg  [15:0] au_t = 16'd0;               // clocks into the millisecond
    reg  [14:0] au_a = 15'd0;               // next word (in T1)
    reg  [14:0] au_c = 15'd0;               // the word coming back
    reg  [5:0]  au_asked = 6'd48, au_got = 6'd48;
    reg         au_miss = 1'b0, au_err = 1'b0;
    wire        a_req = filled && au_asked != 6'd48;
    always @(posedge c50) begin
        au_miss <= 1'b0; au_err <= 1'b0;
        au_t <= (au_t == 16'd49999) ? 16'd0 : au_t + 16'd1;
        if (au_t == 16'd49999 && filled) begin
            if (au_got != 6'd48 || au_asked != 6'd48) au_miss <= 1'b1;
            au_asked <= 6'd0; au_got <= p_rvalid[1] ? 6'd1 : 6'd0;
        end else if (p_ack[1]) au_asked <= au_asked + 6'd16;
        if (p_ack[1]) au_a <= au_a + 15'd16;
        if (p_rvalid[1]) begin
            if (!(au_t == 16'd49999 && filled)) au_got <= au_got + 6'd1;
            au_c <= au_c + 15'd1;
            if (p_rdata != pat(18'h20000 + {3'd0, au_c})) au_err <= 1'b1;
        end
    end

    // ---- port 5: the fill of T1, then the drawer
    reg  [14:0] fl_a = 15'd0, fl_w = 15'd0; // fill: next burst, word being written
    wire        d_req;
    wire [17:0] d_addr;
    wire [31:0] d_data;

    assign p_req   = {filled ? d_req : 1'b1, f_req, r_req, w_req, a_req, v_req};
    assign p_we    = 6'b100100;
    assign p_addr  = {filled ? d_addr : 18'h20000 + {3'd0, fl_a}, f_addr, r_addr, w_addr, 18'h20000 + {3'd0, au_a}, v_addr};
    assign p_len   = {5'd16, 5'd8, 5'd1, 5'd1, 5'd16, v_len};
    assign p_wdata = {filled ? d_data : pat(18'h20000 + {3'd0, fl_w}), 64'd0, w_data, 64'd0};
    always @(posedge c50) if (!filled) begin
        if (p_ack[5]) fl_a <= fl_a + 15'd16;
        if (p_wnext[5]) begin fl_w <= fl_w + 15'd1; if (fl_w == 15'h7FFF) filled <= 1'b1; end
    end

    // ================================================================ counters
    // mkstatus.py's SRC, in order; a second's worth, copied each second
    // (the cumulative ones are not cleared)
    localparam NS = 23;
    localparam [NS-1:0] CUMUL = (23'd1 << 9) | (23'd1 << 10) | (23'd1 << 11) | (23'd1 << 17)
                              | (23'd1 << 20) | (23'd1 << 21) | (23'd1 << 22);
    reg         redraw_p = 1'b0, sec_p = 1'b0;
    reg  [25:0] tick = 26'd0;
    wire        bad = c_err_r || c_err_f || au_err;
    wire [2*NS-1:0] inc = {
        {1'b0, au_err || au_miss},          // 22 e_audio (errors and misses)
        {1'b0, bad},                        // 21 e_total
        {1'b0, tick == TICKS - 40},          // 20 uptime (before the swap: counted in its second)
        {1'b0, redraw_p},                   // 19 redraws
        {1'b0, v_line},                     // 18 lines
        {1'b0, v_underrun},                 // 17 underruns
        {c_sstore, 1'b0},                   // 16 st_store (CPU clocks: twice)
        {c_sload, 1'b0},                    // 15 st_load
        {c_sfetch, 1'b0},                   // 14 st_fetch
        {1'b0, c_cycle},                    // 13 cycles
        {1'b0, c_inst},                     // 12 insts
        {1'b0, c_err_f},                    // 11 e_fetch
        {1'b0, c_err_r},                    // 10 e_cpu
        {1'b0, v_underrun},                 //  9 e_video (its deadlines)
        {1'b0, !phy_cmd},                   //  8 idle
        {1'b0, phy_cmd && !phy_ready},      //  7 turnarounds
        {1'b0, phy_cmd && phy_ready},       //  6 busy
        {1'b0, p_rvalid[4]},                //  5 w_fetch
        {1'b0, p_rvalid[3]},                //  4 w_cpurd
        {1'b0, p_wnext[2]},                 //  3 w_cpuwr
        {1'b0, p_wnext[5]},                 //  2 w_draw
        {1'b0, p_rvalid[1]},                //  1 w_media (audio)
        {1'b0, p_rvalid[0]}};               //  0 w_video

    // Counting, cheaply: each source has a small count of events not yet
    // added (at most 2 a clock, 23 clocks between visits: 6 bits); one
    // adder visits the sources in turn, adding into LUT RAM. The RAM has
    // two banks: one counts this second while the other holds the last,
    // for the formatter and JTAG (a copy of the RAM, written alike, gives
    // that read port). At each second the banks swap; a source's first
    // visit in the new bank starts from 0, or, if it is cumulative, from
    // its value in the old one.
    reg         bank = 1'b0;                // the bank counting
    reg  [4:0]  slot = 5'd0;
    reg  [5:0]  pend [0:NS-1];
    reg  [NS-1:0] fresh = {NS{1'b0}};
    reg  [25:0] acc  [0:63];                // {bank, slot}: the counting RAM
    reg  [25:0] accr [0:63];                // its copy, for reading
    reg         copied = 1'b0;
    wire [5:0]  pend_s = pend[slot];
    wire [25:0] cur = acc[{bank, slot}], old = acc[{!bank, slot}];
    wire [25:0] base = fresh[slot] ? (CUMUL[slot] ? old : 26'd0) : cur;
    wire [25:0] sum = base + {20'd0, pend_s};
    integer m;
    initial for (m = 0; m < 64; m = m + 1) begin acc[m] = 26'd0; accr[m] = 26'd0; end
    genvar g;
    generate
        for (g = 0; g < NS; g = g + 1) begin : pending
            initial pend[g] = 6'd0;
            always @(posedge c50)
                pend[g] <= ((slot == g) ? 6'd0 : pend[g]) + {4'd0, inc[2*g +: 2]};
        end
    endgenerate
    always @(posedge c50) begin
        sec_p <= 1'b0; copied <= 1'b0;
        acc[{bank, slot}] <= sum; accr[{bank, slot}] <= sum;
        slot <= (slot == NS - 1) ? 5'd0 : slot + 5'd1;
        tick <= (tick == TICKS - 1) ? 26'd0 : tick + 26'd1;
        if (tick == TICKS - 1) begin
            bank <= !bank; fresh <= {NS{1'b1}}; sec_p <= 1'b1; copied <= 1'b1;
            fresh[slot] <= 1'b1;
        end else fresh[slot] <= 1'b0;
    end

    // ================================================================ the formatter
    // For each field (stat_fields.hex): the value, divided as its format
    // says, to decimal, then its characters into the text RAM, last first.
    reg  [15:0] text [0:2047];
    reg  [31:0] fields [0:63];
    reg  [7:0]  colours [0:7];
    initial begin
        $readmemh("stat_text.hex", text);
        $readmemh("stat_fields.hex", fields);
        $readmemh("stat_colours.hex", colours);
    end
    localparam FI = 3'd0, FL = 3'd1, FD = 3'd2, FB = 3'd3, FW = 3'd4, FN = 3'd5;
    reg  [2:0]  fs = FI;
    reg  [5:0]  fi = 6'd0;
    reg  [31:0] fw = 32'd0;                 // the field
    wire [10:0] f_pos = fw[10:0];
    wire [4:0]  f_src = fw[15:11];
    wire [2:0]  f_fmt = fw[18:16];
    wire [4:0]  f_wid = fw[23:19];
    wire [2:0]  f_col = fw[26:24];
    reg  [4:0]  sa = 5'd0;                  // snapshot read address
    wire        jrd;                        // JTAG reads it while the formatter is idle
    wire [4:0]  jsa;
    wire [25:0] sv = accr[{!bank, jrd ? jsa : sa}];
    reg  [31:0] num = 32'd0, den = 32'd0, quo = 32'd0;
    reg  [32:0] rem = 33'd0;
    reg  [5:0]  k = 6'd0;
    reg  [39:0] bcd = 40'd0;
    reg  [31:0] bin = 32'd0;
    reg  [4:0]  wi = 5'd0;                  // the character being written
    reg         t_we = 1'b0;
    reg  [10:0] t_wa = 11'd0;
    reg  [15:0] t_wd = 16'd0;
    reg         fmt_done = 1'b0;

    // the character at position wi (from the right)
    wire [1:0]  dec = (f_fmt == 3'd1 || f_fmt == 3'd2) ? 2'd1 : (f_fmt == 3'd4) ? 2'd2 : 2'd0;
    wire [4:0]  kd = (dec != 2'd0 && wi > {3'd0, dec}) ? wi - 5'd1 : wi;
    reg  [3:0]  nsig;
    integer j;
    always @* begin
        nsig = 4'd0;
        for (j = 0; j < 10; j = j + 1) if (bcd[4*j +: 4] != 4'd0) nsig = j + 1;
    end
    reg  [3:0]  dg;                         // digit kd (a case: XST mis-builds the indexed part-select)
    always @* case (kd)
        5'd0: dg = bcd[3:0];   5'd1: dg = bcd[7:4];   5'd2: dg = bcd[11:8];  5'd3: dg = bcd[15:12];
        5'd4: dg = bcd[19:16]; 5'd5: dg = bcd[23:20]; 5'd6: dg = bcd[27:24]; 5'd7: dg = bcd[31:28];
        5'd8: dg = bcd[35:32]; 5'd9: dg = bcd[39:36]; default: dg = 4'd0;
    endcase
    // double dabble's step: each digit over 4 gets 3 more, before the shift
    wire [39:0] bcd_adj;
    genvar gd;
    generate
        for (gd = 0; gd < 10; gd = gd + 1) begin : dab
            assign bcd_adj[4*gd +: 4] = (bcd[4*gd +: 4] > 4'd4) ? bcd[4*gd +: 4] + 4'd3 : bcd[4*gd +: 4];
        end
    endgenerate
    wire        show = kd < 5'd10 && (kd < {1'b0, nsig} || kd <= {3'd0, dec});
    wire [4:0]  bj = f_wid - 5'd1 - wi;     // a bar's tcell, from the left
    wire [7:0]  ch = (f_fmt == 3'd3) ? ((bj < quo[4:0] || quo > 32'd31) ? 8'hDB : 8'hFA)
                   : (dec != 2'd0 && wi == {3'd0, dec}) ? 8'h2E
                   : show ? {4'h3, dg} : 8'h20;

    always @(posedge c50) begin
        t_we <= 1'b0; fmt_done <= 1'b0;
        fw <= fields[fi];
        case (fs)
        FI: if (copied) begin fi <= 6'd0; fs <= FN; end
        FN: fs <= FL;                       // fields[fi] is in fw now
        FL: if (fw == 32'd0) begin fs <= FI; fmt_done <= 1'b1; end
            else if (sa != f_src) sa <= f_src;           // a clock for the read
            else begin
                // the numerator and the divisor (fmt 4: the source before)
                k <= 6'd0; rem <= 33'd0;
                case (f_fmt)
                3'd1: begin num <= {6'd0, sv}; den <= 32'd50000; end
                3'd2: begin num <= {6'd0, sv}; den <= 32'd25000; end
                3'd3: begin num <= {6'd0, sv}; den <= 32'd1666667; end   // 30 cells
                3'd4: begin num <= {sv, 6'd0} + {1'b0, sv, 5'd0} + {4'd0, sv, 2'd0};   // * 100
                             den <= 32'd0; sa <= f_src - 5'd1; end
                default: begin num <= {6'd0, sv}; den <= 32'd1; end
                endcase
                fs <= FD;
            end
        FD: begin
                if (f_fmt == 3'd4 && k == 6'd0 && den == 32'd0) begin
                    den <= {6'd0, sv};       // the source before, read now
                    if (sv == 26'd0) begin quo <= 32'd0; bin <= 32'd0; bcd <= 40'd0; k <= 6'd0; fs <= FB; end
                end else if (k == 6'd32) begin
                    bin <= quo; bcd <= 40'd0; k <= 6'd0; fs <= FB;
                end else begin
                    // restoring division, a bit a clock
                    if ({rem[31:0], num[31]} >= {1'b0, den}) begin
                        rem <= {rem[31:0], num[31]} - {1'b0, den}; quo <= {quo[30:0], 1'b1};
                    end else begin
                        rem <= {rem[31:0], num[31]}; quo <= {quo[30:0], 1'b0};
                    end
                    num <= {num[30:0], 1'b0};
                    k <= k + 6'd1;
                end
            end
        FB: if (k == 6'd32) begin wi <= 5'd0; fs <= FW; end
            else begin
                // double dabble: add 3 to digits over 4, then shift a bit in
                bcd <= {bcd_adj[38:0], bin[31]};
                bin <= {bin[30:0], 1'b0};
                k <= k + 6'd1;
            end
        FW: begin
                t_we <= 1'b1; t_wa <= f_pos - {6'd0, wi};
                t_wd <= {colours[f_col], ch};
                if (wi == f_wid - 5'd1) begin fi <= fi + 6'd1; fs <= FN; end
                else wi <= wi + 5'd1;
            end
        default: fs <= FI;
        endcase
    end
    always @(posedge c50) if (t_we) text[t_wa] <= t_wd;

    // ================================================================ the drawer
    // The text, 80 x 25 cells of 16 x 28 pixels (the 8 x 14 font doubled),
    // into the framebuffer: each glyph line makes two words. Lines 700-719
    // are drawn black. Pipeline: the cell, the glyph line, the word, into a
    // FIFO; the port writes bursts of 16 from it.
    reg  [7:0]  font [0:3583];
    initial $readmemh("font8x14.mem", font);
    reg         dact = 1'b0;
    reg  [9:0]  dy = 10'd0;                 // line
    reg  [4:0]  dsub = 5'd0;                // line in the tcell row (0-27)
    reg  [10:0] drb = 11'd0;                // row * 80
    reg  [4:0]  drow = 5'd0;
    reg  [7:0]  dx = 8'd0;                  // word in the line
    reg  [5:0]  fq_n = 6'd0;
    wire        issue = dact && fq_n < 6'd28;
    // stage 1: the cell
    reg  [15:0] tcell = 16'd0;
    reg         s1 = 1'b0, h1 = 1'b0, b1 = 1'b0;
    reg  [3:0]  gl1 = 4'd0;
    always @(posedge c50) begin
        tcell <= text[drb + {4'd0, dx[7:1]}];
        s1 <= issue; h1 <= dx[0]; b1 <= drow >= 5'd25; gl1 <= dsub[4:1];
    end
    // stage 2: the glyph line
    reg  [7:0]  gly = 8'd0;
    reg         s2 = 1'b0, h2 = 1'b0, b2 = 1'b0;
    reg  [5:0]  at2 = 6'd0;
    wire [11:0] faddr = {tcell[7:0], 3'b000} + {tcell[7:0], 2'b00} + {tcell[7:0], 1'b0} + {8'd0, gl1};
    always @(posedge c50) begin
        gly <= font[faddr];
        s2 <= s1; h2 <= h1; b2 <= b1; at2 <= tcell[13:8];
    end
    // stage 3: the word, into the FIFO
    wire [3:0]  bits = h2 ? gly[3:0] : gly[7:4];
    wire [3:0]  fgc = {1'b0, at2[2:0]}, bgc = {1'b0, at2[5:3]};
    wire [3:0]  q0 = bits[3] ? fgc : bgc, q1 = bits[2] ? fgc : bgc,
                q2 = bits[1] ? fgc : bgc, q3 = bits[0] ? fgc : bgc;
    wire [31:0] word = b2 ? 32'd0 : {q3, q3, q2, q2, q1, q1, q0, q0};
    reg  [31:0] fq [0:31];                  // distributed RAM
    reg  [4:0]  fq_w = 5'd0, fq_r = 5'd0;
    wire        pop = filled && p_wnext[5];
    always @(posedge c50) begin
        if (s2) begin fq[fq_w] <= word; fq_w <= fq_w + 5'd1; end
        if (pop) fq_r <= fq_r + 5'd1;
        fq_n <= fq_n + (s2 ? 6'd1 : 6'd0) - (pop ? 6'd1 : 6'd0);
    end
    assign d_data = fq[fq_r];
    // the port: a burst of 16 whenever 16 are waiting
    reg  [16:0] da = 17'd0;
    assign d_req  = fq_n >= 6'd16;
    assign d_addr = {1'b0, da};
    always @(posedge c50) if (filled && p_ack[5]) da <= (da == 17'd115200 - 17'd16) ? 17'd0 : da + 17'd16;

    // the walk over the screen
    reg  draw_go = 1'b0;                    // once a second
    always @(posedge c50) begin
        redraw_p <= 1'b0;
        if (fmt_done) draw_go <= 1'b1;
        if (!dact && filled && (draw_go || draw_cont)) begin
            dact <= 1'b1; draw_go <= 1'b0;
            dy <= 10'd0; dsub <= 5'd0; drb <= 11'd0; drow <= 5'd0; dx <= 8'd0;
        end else if (issue) begin
            if (dx != 8'd159) dx <= dx + 8'd1;
            else begin
                dx <= 8'd0;
                if (dy == 10'd719) begin dact <= 1'b0; redraw_p <= 1'b1; end
                dy <= dy + 10'd1;
                if (dsub == 5'd27) begin dsub <= 5'd0; drow <= drow + 5'd1; drb <= drb + 11'd80; end
                else dsub <= dsub + 5'd1;
            end
        end
    end

    reg  err_seen = 1'b0;

    // ================================================================ JTAG
    wire        ldr_req, ldr_rd;
    wire [17:0] ldr_addr;
    wire [31:0] ldr_data;
    reg         ldr_done = 1'b0;
    reg  [31:0] ldr_q = 32'd0;
    assign jrd = fs == FI && ldr_req;
    assign jsa = ldr_addr[4:0];
    always @(posedge c50) begin
        ldr_done <= 1'b0;
        if (ldr_req && !ldr_done) begin
            ldr_done <= 1'b1;
            ldr_q <= (ldr_addr[17:5] == 13'h1FE0) ? ctrl : (ldr_addr[17:5] == 13'h1FE1) ? {6'd0, sv} : 32'd0;
            if (!ldr_rd && ldr_addr == 18'h3FC00) ctrl <= ldr_data;
        end
    end
    jtagldr ldr (
        .clk(c50), .status({filled, err_seen, 30'd0}),
        .run(), .entry(),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_q(ldr_q), .trace_rd(),
        .ldr_addr(ldr_addr), .ldr_data(ldr_data), .ldr_done(ldr_done)
    );

    // ================================================================ display and LEDs
    // The 7-segment digits show the SRAM address's top 16 bits (sram_a[17:2])
    // in hex, each digit sampled as it is lit: the regions in use show
    // through. An LED a client: it toggles every 2^k words the client moves
    // (k so each blinks about once a second at its usual rate), so a steady
    // client blinks steadily and a random one does not.
    //   0 video  1 audio  2 CPU writes  3 CPU reads  4 CPU fetch
    //   5 draw  6 turnarounds  7 an error (stays on)
    reg [17:0] refresh = 18'd0;
    always @(posedge c50) refresh <= refresh + 1'b1;
    wire [1:0] dsel = refresh[17:16];
    reg  [3:0] nib = 4'd0;
    wire [6:0] glyph;
    seg7 sdec (.digit(nib), .glyph(glyph));
    reg  [22:0] lc0 = 0, lc4 = 0;
    reg  [15:0] lc1 = 0;
    reg  [18:0] lc2 = 0;
    reg  [20:0] lc3 = 0;
    reg  [16:0] lc5 = 0;
    reg  [21:0] lc6 = 0;
    always @(posedge c50) begin
        nib <= sram_a[4*dsel + 2 +: 4];
        if (p_rvalid[0]) lc0 <= lc0 + 1'b1;
        if (p_rvalid[1]) lc1 <= lc1 + 1'b1;
        if (p_wnext[2])  lc2 <= lc2 + 1'b1;
        if (p_rvalid[3]) lc3 <= lc3 + 1'b1;
        if (p_rvalid[4]) lc4 <= lc4 + 1'b1;
        if (p_wnext[5])  lc5 <= lc5 + 1'b1;
        if (phy_cmd && !phy_ready) lc6 <= lc6 + 1'b1;
        if (bad) err_seen <= 1'b1;
        an  <= ~(4'b0001 << dsel);
        seg <= {1'b1, ~glyph};
        led <= {err_seen, lc6[21], lc5[16], lc4[22], lc3[20], lc2[18], lc1[15], lc0[22]};
    end
endmodule
