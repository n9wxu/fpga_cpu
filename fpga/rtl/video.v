`timescale 1ns/1ps
// The fpga_cpu video (docs/io.md, "Video"), as sim/src/video.c draws it:
// 1280x720 60 Hz, a 640x360 logical screen shown 2x, with the backdrop,
// the bitmap (scrolled in x and y) and the 80x25 text layer. The bitmap is
// 8-color or 16-color (4 bits a pixel, direct colour), or hi-color through
// the 256-entry palette, dithered 2x2 to the 3-bit VGA.
// HICOL8 = 0 is the low video tier (the Spartan-3): no hi-color and no
// palette, and the bitmap line buffer is half as big. The Tang Nano 20K is
// the high tier (HICOL8 = 1).
// EXT = 1 (the Tang Nano 20K) adds docs/io.md's "Video extensions":
// playfield B (8 bpp, its own scroll), a per-line scroll table, the sprite
// buffer layer, 64 hardware sprites (up to 16 a line, built into a line
// buffer by the DMA engine), and double buffering (the buffer addresses
// are taken at the start of each frame). With EXT = 0 none of it is built.
//
// Two outputs: rgb, the 3-bit VGA colour, and rgb24 with de for a digital
// display (HDMI). In rgb24 the 3-bit colours are full scale; with
// TRUE_COLOR = 1 (fsim --true-color) hi-color is the palette's true colour
// and a stippled text background is a 50% blend over what is beneath it,
// with TRUE_COLOR = 0 both are as in rgb (dither, checkerboard). Unused
// outputs are trimmed away.
//
// Two clock domains:
//
//   clk (50 MHz)    the DMA engine: for each logical line it reads the
//                   bitmap row and the text row from SRAM through memsys
//                   into a line buffer, while the line before it is shown.
//   pclk (74.25)    timing, the line buffers' read side, palette, font,
//                   dither and composition.
//
// Buffer addresses (VID_BMP, VID_TEXT) must be multiples of 4.
module video #(
    parameter FONT_FILE = "font8x14.mem",
    parameter TRUE_COLOR = 0,
    parameter HICOL8 = 1,
    parameter EXT = 0
) (
    input  wire        clk,
    input  wire        pclk,
    // registers (clk domain)
    input  wire [15:0] vid_ctrl,
    input  wire [2:0]  vid_backdrop,
    input  wire [31:0] vid_text,
    input  wire [31:0] vid_bmp,
    input  wire [15:0] vid_scrollx,
    input  wire [15:0] vid_scrolly,
    input  wire [15:0] vid_cursor,
    input  wire        pal_we,
    input  wire [7:0]  pal_windex,
    input  wire [15:0] pal_wdata,
    // EXT: the video extensions' registers (clk domain; unconnected without EXT)
    input  wire [15:0] vid_ctrl2,
    input  wire [31:0] vid_bmpb,
    input  wire [15:0] vid_scrollxb,
    input  wire [15:0] vid_scrollyb,
    input  wire [31:0] vid_lscr,
    input  wire [31:0] vid_spr,
    input  wire        spr_we,              // a sprite attribute word: spr_wd at spr_wa
    input  wire [7:0]  spr_wa,
    input  wire [15:0] spr_wd,
    output reg         frame_latch = 1'b0,  // a pulse: the buffer registers were taken for a frame
    output reg  [15:0] spr_over = 16'd0,    // last frame's lines with more than 16 sprites
    // DMA through memsys (clk domain)
    output wire        dma_req,
    output reg  [17:0] dma_addr = 18'd0,
    output reg  [8:0]  dma_left = 9'd0,
    input  wire [4:0]  dma_take,            // words taken now (memsys: up to 4; rammux: up to 16)
    input  wire        dma_valid,
    input  wire [31:0] dma_data,
    // timing to the I/O block (pclk domain toggles)
    output reg         line_tgl = 1'b0,
    output reg         line_top = 1'b0,
    // VGA
    output reg         hsync = 1'b0,
    output reg         vsync = 1'b0,
    output reg  [2:0]  rgb = 3'b000,
    // digital: 8 bits per channel, and data enable (the visible area)
    output reg  [23:0] rgb24 = 24'd0,
    output reg         de = 1'b0,
    // diagnostics (clk): a line was asked for while the last one's fetch was
    // still under way (the DMA fell behind). May be left unconnected.
    output reg         late = 1'b0
);
    function [23:0] full;                    // a 3-bit colour at full scale
        input [2:0] c;
        full = {{8{c[2]}}, {8{c[1]}}, {8{c[0]}}};
    endfunction
    function [23:0] half;                    // and at half
        input [2:0] c;
        half = {c[2], 7'd0, c[1], 7'd0, c[0], 7'd0};
    endfunction
    localparam H_VIS = 1280, H_SYNC0 = 1390, H_SYNC1 = 1430, H_TOTAL = 1650;
    localparam V_VIS = 720,  V_SYNC0 = 725,  V_SYNC1 = 730,  V_TOTAL = 750;
    localparam LW = 640, LH = 360, TROWS = 25, CELL_H = 14;

    // VID_CTRL bits
    // (without HICOL8, bit 2 is ignored: no hi-color)
    wire c_text = vid_ctrl[0], c_bmp = vid_ctrl[1], c_hicol = HICOL8 && vid_ctrl[2];
    localparam LBW  = HICOL8 ? 512 : 256;    // bitmap line buffer words: 2 x 160 (8 bpp) or 2 x 80
    localparam PALN = HICOL8 ? 256 : 1;      // the palette (none on the low tier)

    // =====================================================================
    // Line buffers, palette (written at clk, read at pclk)
    // =====================================================================
    reg [31:0] lb_bmp [0:LBW-1];             // {buffer, word 0-159 (0-79)}
    (* ram_style = "block" *)
    reg [31:0] lb_txt [0:127];               // {buffer, word 0-39}
    reg [15:0] pal [0:PALN-1];
    localparam XW = EXT ? 512 : 1;           // EXT line buffers: {buffer, word 0-159}
    reg [31:0] lb_b [0:XW-1];                // playfield B
    reg [31:0] lb_s [0:XW-1];                // the sprite buffer layer
    reg [7:0]  lb_h0 [0:XW-1], lb_h1 [0:XW-1], lb_h2 [0:XW-1], lb_h3 [0:XW-1];   // hardware sprites, byte lanes

    reg        lbw_bmp = 1'b0, lbw_txt = 1'b0, lbw_b = 1'b0, lbw_s = 1'b0;
    reg [8:0]  lbw_a = 9'd0;
    reg [31:0] lbw_d = 32'd0;
    reg [3:0]  lbh_we = 4'd0;                // hardware sprite line buffer: byte enables
    reg [8:0]  lbh_a = 9'd0;
    reg [31:0] lbh_d = 32'd0;
    always @(posedge clk) begin
        if (lbw_bmp) lb_bmp[HICOL8 ? lbw_a : {1'b0, lbw_a[8], lbw_a[6:0]}] <= lbw_d;
        if (lbw_txt) lb_txt[lbw_a[6:0]] <= lbw_d;
        if (HICOL8 && pal_we) pal[pal_windex] <= pal_wdata;
        if (EXT && lbw_b) lb_b[lbw_a] <= lbw_d;
        if (EXT && lbw_s) lb_s[lbw_a] <= lbw_d;
        if (EXT && lbh_we[0]) lb_h0[lbh_a] <= lbh_d[7:0];
        if (EXT && lbh_we[1]) lb_h1[lbh_a] <= lbh_d[15:8];
        if (EXT && lbh_we[2]) lb_h2[lbh_a] <= lbh_d[23:16];
        if (EXT && lbh_we[3]) lb_h3[lbh_a] <= lbh_d[31:24];
    end

    // EXT: sprite attributes (64 x 4 words; block RAM, one registered read
    // port for the line scan and the sprite fetch, which take turns)
    (* ram_style = "block" *) reg [15:0] at0 [0:63];
    (* ram_style = "block" *) reg [15:0] at1 [0:63];
    (* ram_style = "block" *) reg [15:0] at2 [0:63];
    (* ram_style = "block" *) reg [15:0] at3 [0:63];
    reg [15:0] aq0 = 0, aq1 = 0, aq2 = 0, aq3 = 0;
    wire [5:0] at_ra;                        // the read address (from the engine below)
    always @(posedge clk) begin
        aq0 <= at0[at_ra]; aq1 <= at1[at_ra]; aq2 <= at2[at_ra]; aq3 <= at3[at_ra];
    end
    always @(posedge clk)
        if (EXT && spr_we)
            case (spr_wa[1:0])
                2'd0: at0[spr_wa[7:2]] <= spr_wd;
                2'd1: at1[spr_wa[7:2]] <= spr_wd;
                2'd2: at2[spr_wa[7:2]] <= spr_wd;
                default: at3[spr_wa[7:2]] <= spr_wd;
            endcase

    // =====================================================================
    // DMA engine (clk)
    // =====================================================================
    // VID_SCROLLY (and EXT: VID_SCROLLYB) mod 360, by repeated subtraction
    // whenever it changes
    reg [15:0] sy_src = 16'd0, sy_rem = 16'd0;
    always @(posedge clk)
        if (vid_scrolly != sy_src) begin
            sy_src <= vid_scrolly;
            sy_rem <= vid_scrolly;
        end else if (sy_rem >= LH)
            sy_rem <= sy_rem - LH;
    wire [8:0] sym = sy_rem[8:0];
    reg [15:0] syb_src = 16'd0, syb_rem = 16'd0;
    always @(posedge clk)
        if (vid_scrollyb != syb_src) begin
            syb_src <= vid_scrollyb;
            syb_rem <= vid_scrollyb;
        end else if (syb_rem >= LH)
            syb_rem <= syb_rem - LH;
    wire [8:0] sybm = syb_rem[8:0];

    // requests from the pixel side: fetch logical line rq_line into buffer rq_line[0]
    reg        rq_tgl = 1'b0;                // pclk
    reg  [8:0] rq_line = 9'd0;               // pclk, stable around the toggle
    reg  [4:0] rq_row = 5'd0;                // pclk: its text row
    reg  [2:0] rq_s = 3'b000;
    always @(posedge clk) rq_s <= {rq_s[1:0], rq_tgl};
    wire rq = rq_s[2] != rq_s[1];

    // EXT: the buffer registers, taken at the start of each frame (line 0's request)
    reg [31:0] bmp_f = 0, bmpb_f = 0, lscr_f = 0, spr_f = 0;
    wire [31:0] bmp_a = EXT ? bmp_f : vid_bmp;
    wire        x_pfb = EXT && vid_ctrl2[0], x_lscr = EXT && vid_ctrl2[2], x_hws = EXT && vid_ctrl2[3];
    wire        x_sl = EXT && vid_ctrl[3], x_s8 = EXT && vid_ctrl[6];

    // The line's fetch is a list of segments, each read into its place:
    //   0 the line-scroll entry (EXT, 1 word)   1 playfield A   2 playfield B (EXT)
    //   3 the sprite buffer layer's row (EXT)   4 the text row
    // then (EXT) the hardware sprites, one at a time (state 4).
    reg  [2:0] st = 3'd0;                    // 0 idle, 1 plan, 2 fetch, 3 wait for the sprites' turn, 4 sprites
    reg  [8:0] ln = 9'd0;                    // line being fetched
    reg  [4:0] tr = 5'd0;                    // its text row
    reg  [9:0] by = 10'd0, byb = 10'd0;
    reg  [8:0] wi = 9'd0;                    // words back from memory
    reg  [2:0] seg = 3'd0;                    // the segment being asked for
    reg  [8:0] len0 = 0, len1 = 0, len2 = 0, len3 = 0, len4 = 0;
    reg [19:0] adr1 = 0, adr2 = 0, adr3 = 0, adr4 = 0;
    wire [8:0] c1 = len0, c2 = len0 + len1, c3 = c2 + len2, c4 = c3 + len3, ctot = c4 + len4;
    wire [19:0] boff  = c_hicol ? ({by, 9'd0} + {by, 7'd0}) : ({by, 8'd0} + {by, 6'd0});   // by * 640 or 320
    wire [19:0] bboff = {byb, 9'd0} + {byb, 7'd0};                                        // byb * 640
    wire [19:0] soff  = x_s8 ? ({ln, 9'd0} + {ln, 7'd0}) : ({ln, 8'd0} + {ln, 6'd0});     // ln * 640 or 320
    wire [19:0] toff  = {tr, 7'd0} + {tr, 5'd0};                                          // tr * 160
    wire [19:0] sbuf  = !vid_ctrl[4] ? 20'd0 : x_s8 ? 20'd230400 : 20'd115200;           // the shown sprite buffer

    // EXT line scroll: this line's horizontal offsets (the scroll register mod 640,
    // plus the entry, 0-639), kept per buffer for the pixel side
    reg  [9:0]  lsx_a [0:1], lsx_b [0:1];    // read at pclk while the line is shown
    reg  [15:0] sx_c = 0, sxb_c = 0;         // VID_SCROLLX / VID_SCROLLXB mod 640 (clk)
    reg  [15:0] sx_cs = 0, sxb_cs = 0;
    always @(posedge clk) begin
        if (vid_scrollx != sx_cs) begin sx_cs <= vid_scrollx; sx_c <= vid_scrollx; end
        else if (sx_c >= LW) sx_c <= sx_c - LW;
        if (vid_scrollxb != sxb_cs) begin sxb_cs <= vid_scrollxb; sxb_c <= vid_scrollxb; end
        else if (sxb_c >= LW) sxb_c <= sxb_c - LW;
    end
    reg  [31:0] lsw = 32'd0;                 // the line-scroll entry
    reg         lsw_v = 1'b0;
    wire [10:0] la = {1'b0, sx_c[9:0]}  + (lsw_v ? {1'b0, lsw[9:0]}  : 11'd0);
    wire [10:0] lb = {1'b0, sxb_c[9:0]} + (lsw_v ? {1'b0, lsw[25:16]} : 11'd0);

    // EXT hardware sprites: the scan finds the 16 lowest-numbered sprites on the line
    reg  [6:0]  sc = 7'd64;                  // sprite whose attributes are asked for; 64: done
    reg  [5:0]  sc_d = 6'd0;                 // ... and the one whose attributes are here
    reg         sc_dv = 1'b0;
    reg  [5:0]  sl_i [0:15];                 // the list: sprite number and row
    reg  [4:0]  sl_r [0:15];
    reg  [4:0]  sl_n = 5'd0;
    reg         sl_over = 1'b0;
    wire [5:0]  sc6 = sc[5:0];
    wire [9:0]  sdy = {1'b0, ln} + 10'd32 - {1'b0, aq1[8:0]};          // the line's row in the sprite (mod 1024)
    wire        s_vis = aq3[15] && sdy < {5'd0, aq1[15:11]} + 10'd1 && aq3[14:13] != 2'd3;
    // ... the line buffer's half for this line is cleared first
    reg  [8:0]  clr = 9'd160;                // word being cleared; 160: done
    // ... then each listed sprite, last first (the lower number drawn later, in front)
    reg  [4:0]  sk = 5'd0;                   // list entry being drawn
    reg  [1:0]  sp = 2'd0;                   // 3 attributes, 0 ask, 1 receive, 2 write
    reg  [31:0] swb [0:7];                   // the sprite's row
    reg  [3:0]  sw_n = 4'd0, sw_got = 4'd0;  // its words, and how many are back
    reg  [3:0]  sw_j = 4'd0;                 // word being written
    reg         sw_h = 1'b0;                 // its first or second part
    reg  [10:0] sx0 = 11'd0;                 // the sprite's left column (signed)
    reg         sflip = 1'b0;
    reg  [15:0] over_n = 16'd0;
    wire [5:0]  ski = sl_i[sk];
    assign      at_ra = (st == 3'd4) ? ski : sc6;
    wire [3:0]  nw_k = aq3[14:13] == 2'd0 ? 4'd2 : aq3[14:13] == 2'd1 ? 4'd4 : 4'd8;
    // the word being placed (flipped: the words in reverse order, each byte-reversed)
    wire [31:0] swr = swb[sflip ? sw_n - 4'd1 - sw_j : sw_j];
    wire [31:0] swv = sflip ? {swr[7:0], swr[15:8], swr[23:16], swr[31:24]} : swr;
    wire [10:0] sxs = sx0 + {5'd0, sw_j, 2'b00};          // its first pixel's column
    wire [1:0]  sa  = sxs[1:0];
    wire [10:0] swd = {{2{sxs[10]}}, sxs[10:2]} + (sw_h ? 11'd1 : 11'd0);   // the line buffer word
    wire [3:0]  snz = {swv[31:24] != 0, swv[23:16] != 0, swv[15:8] != 0, swv[7:0] != 0};
    wire [63:0] sdsh = {32'd0, swv} << {sa, 3'b000};
    wire [7:0]  snsh = {4'd0, snz} << sa;
    wire        swd_ok = !swd[10] && swd < 11'd160;

    assign dma_req = (st == 3'd2 || (st == 3'd4 && sp == 2'd0)) && dma_left != 0;

    always @(posedge clk) begin
        lbw_bmp <= 1'b0;
        lbw_txt <= 1'b0;
        lbw_b   <= 1'b0;
        lbw_s   <= 1'b0;
        lbh_we  <= 4'd0;
        frame_latch <= 1'b0;
        late <= rq && st != 3'd0;
        if (rq) begin
            ln <= rq_line;
            tr <= rq_row;
            by  <= ({1'b0, rq_line} + sym  >= LH) ? {1'b0, rq_line} + sym  - LH : {1'b0, rq_line} + sym;
            byb <= ({1'b0, rq_line} + sybm >= LH) ? {1'b0, rq_line} + sybm - LH : {1'b0, rq_line} + sybm;
            if (EXT && rq_line == 9'd0) begin
                bmp_f <= vid_bmp; bmpb_f <= vid_bmpb; lscr_f <= vid_lscr; spr_f <= vid_spr;
                frame_latch <= 1'b1;
                spr_over <= over_n;
                over_n <= 16'd0;
            end
            st <= 3'd1;
            wi <= 9'd0;
            dma_left <= 9'd0;
            lsw_v <= 1'b0;
            sc <= x_hws ? 7'd0 : 7'd64;
            sl_n <= 5'd0;
            sl_over <= 1'b0;
            clr <= x_hws ? 9'd0 : 9'd160;
        end else case (st)
            3'd1: begin
                len0 <= x_lscr ? 9'd1 : 9'd0;
                len1 <= c_bmp ? (c_hicol ? 9'd160 : 9'd80) : 9'd0;
                len2 <= x_pfb ? 9'd160 : 9'd0;
                len3 <= x_sl ? (x_s8 ? 9'd160 : 9'd80) : 9'd0;
                len4 <= (c_text && ln < TROWS * CELL_H) ? 9'd40 : 9'd0;
                adr1 <= bmp_a[19:0] + boff;
                adr2 <= bmpb_f[19:0] + bboff;
                adr3 <= spr_f[19:0] + sbuf + soff;
                adr4 <= vid_text[19:0] + toff;
                dma_addr <= (lscr_f[19:0] + {9'd0, ln, 2'b00}) >> 2;
                dma_left <= x_lscr ? 9'd1 : 9'd0;
                seg <= 3'd0;
                st <= 3'd2;
            end
            3'd2: begin
                if (dma_take != 0) begin
                    dma_addr <= dma_addr + dma_take;
                    dma_left <= dma_left - dma_take;
                end else if (dma_left == 0) begin
                    if (seg != 3'd4) begin
                        seg <= seg + 1'b1;
                        case (seg)
                            3'd0: begin dma_addr <= adr1[19:2]; dma_left <= len1; end
                            3'd1: begin dma_addr <= adr2[19:2]; dma_left <= len2; end
                            3'd2: begin dma_addr <= adr3[19:2]; dma_left <= len3; end
                            default: begin dma_addr <= adr4[19:2]; dma_left <= len4; end
                        endcase
                    end else if (wi == ctot)
                        st <= x_hws ? 3'd3 : 3'd0;
                end
            end
            3'd3:                            // the scan and the clearing done: draw the list, last first
                if (sc == 7'd64 && !sc_dv && clr == 9'd160) begin
                    if (sl_over) over_n <= over_n + 1'b1;
                    if (sl_n == 5'd0) st <= 3'd0;
                    else begin sk <= sl_n - 1'b1; sp <= 2'd3; st <= 3'd4; dma_left <= 9'd0; sw_n <= 4'd0; end
                end
            3'd4: case (sp)
                2'd0: begin                  // ask for the sprite's row
                    if (sw_n == 4'd0) begin
                        sw_n   <= nw_k;
                        sw_got <= 4'd0;
                        sflip  <= aq3[12];
                        sx0    <= {1'b0, aq0[9:0]} - 11'd32;
                        dma_addr <= ({aq3[3:0], aq2} + ({15'd0, sl_r[sk]} << ({1'b0, aq3[14:13]} + 3'd3))) >> 2;   // row x width
                        dma_left <= {5'd0, nw_k};
                    end else if (dma_take != 0) begin
                        dma_addr <= dma_addr + dma_take;
                        dma_left <= dma_left - dma_take;
                        if (dma_left == {4'd0, dma_take}) sp <= 2'd1;
                    end
                end
                2'd1: if (sw_got == sw_n) begin sp <= 2'd2; sw_j <= 4'd0; sw_h <= 1'b0; end
                2'd2: begin                  // two writes a word: its bytes at the column, then the spill
                    if (swd_ok) begin
                        lbh_a  <= {ln[0], swd[7:0]};
                        lbh_d  <= sw_h ? sdsh[63:32] : sdsh[31:0];
                        lbh_we <= sw_h ? snsh[7:4] : snsh[3:0];
                    end
                    sw_h <= ~sw_h;
                    if (sw_h) begin
                        if (sw_j == sw_n - 1'b1) begin
                            if (sk == 5'd0) st <= 3'd0;
                            else begin sk <= sk - 1'b1; sp <= 2'd3; sw_n <= 4'd0; end
                        end else
                            sw_j <= sw_j + 1'b1;
                    end
                end
                default: sp <= 2'd0;         // (the sprite's attributes are read: aq0-aq3 next clock)
            endcase
            default: ;
        endcase

        // words coming back: into their segment's place
        if (dma_valid && st == 3'd2 && !rq) begin
            lbw_d <= dma_data;
            if (wi < c1) begin
                lsw <= dma_data; lsw_v <= 1'b1;
            end else if (wi < c2) begin
                lbw_bmp <= 1'b1; lbw_a <= {ln[0], wi[7:0] - c1[7:0]};
            end else if (wi < c3) begin
                lbw_b <= 1'b1; lbw_a <= {ln[0], wi[7:0] - c2[7:0]};
            end else if (wi < c4) begin
                lbw_s <= 1'b1; lbw_a <= {ln[0], wi[7:0] - c3[7:0]};
            end else begin
                lbw_txt <= 1'b1; lbw_a <= {2'b00, ln[0], wi[5:0] - c4[5:0]};
            end
            wi <= wi + 1'b1;
        end
        if (dma_valid && st == 3'd4 && !rq) begin
            swb[sw_got[2:0]] <= dma_data;
            sw_got <= sw_got + 1'b1;
        end

        // line scroll: the line's offsets (once its entry is in, or at once without the table)
        if (!rq && (st == 3'd1 ? !x_lscr : lsw_v)) begin
            lsx_a[ln[0]] <= la >= LW ? la - LW : la[9:0];
            lsx_b[ln[0]] <= lb >= LW ? lb - LW : lb[9:0];
            lsw_v <= 1'b0;
        end

        // the sprite scan, one sprite a clock (its attributes a clock after its address)
        if (rq) sc_dv <= 1'b0;
        else begin
            sc_dv <= sc != 7'd64;
            sc_d  <= sc6;
            if (sc != 7'd64) sc <= sc + 1'b1;
            if (sc_dv && s_vis) begin
                if (sl_n == 5'd16) sl_over <= 1'b1;
                else begin sl_i[sl_n[3:0]] <= sc_d; sl_r[sl_n[3:0]] <= sdy[4:0]; sl_n <= sl_n + 1'b1; end
            end
        end
        // clearing the sprite line buffer's half (not while the sprites are written)
        if (!rq && clr != 9'd160 && st != 3'd4) begin
            lbh_a  <= {ln[0], clr[7:0]};
            lbh_d  <= 32'd0;
            lbh_we <= 4'hF;
            clr <= clr + 1'b1;
        end
    end

    // =====================================================================
    // Pixel side (pclk)
    // =====================================================================
    // registers brought over from clk (each is static for many frames, so a
    // two-stage synchronizer per bit is enough)
    reg [15:0] ctrl_s0 = 0, ctrl_p = 0;
    reg [2:0]  bd_s0 = 0, bd_p = 0;
    reg [15:0] sx_s0 = 0, sx_p = 0;
    reg [15:0] cur_s0 = 0, cur_p = 0;
    always @(posedge pclk) begin
        ctrl_s0 <= vid_ctrl;     ctrl_p <= ctrl_s0;
        bd_s0   <= vid_backdrop; bd_p   <= bd_s0;
        sx_s0   <= vid_scrollx;  sx_p   <= sx_s0;
        cur_s0  <= vid_cursor;   cur_p  <= cur_s0;
    end
    wire p_text = ctrl_p[0], p_bmp = ctrl_p[1], p_hicol = HICOL8 && ctrl_p[2], p_cursor = ctrl_p[5];
    wire p_c16 = !p_hicol && ctrl_p[7];      // 16-color: 9-15 are 1-7 at half brightness, 8 transparent
    reg [15:0] c2_s0 = 0, c2_p = 0;
    always @(posedge pclk) begin c2_s0 <= vid_ctrl2; c2_p <= c2_s0; end
    wire p_pfb = EXT && c2_p[0], p_bfront = c2_p[1], p_hws = EXT && c2_p[3], p_hbeh = c2_p[4];
    wire p_sl = EXT && ctrl_p[3], p_s8 = ctrl_p[6];

    // VID_SCROLLX mod 640
    reg [15:0] sx_src = 16'd0, sx_rem = 16'd0;
    always @(posedge pclk)
        if (sx_p != sx_src) begin
            sx_src <= sx_p;
            sx_rem <= sx_p;
        end else if (sx_rem >= LW)
            sx_rem <= sx_rem - LW;
    wire [9:0] sxm = sx_rem[9:0];

    // ---- counters ------------------------------------------------------------
    reg [10:0] hc = 0;
    reg [9:0]  vc = 0;
    reg [4:0]  row = 0;                      // text row
    reg [3:0]  gl = 0;                       // glyph line
    reg [10:0] row_base = 0;                 // row * 80
    reg [5:0]  blink = 0;                    // frames, for the cursor

    wire eol = hc == H_TOTAL - 1;
    always @(posedge pclk) begin
        hc <= eol ? 11'd0 : hc + 1'b1;
        if (eol) begin
            if (vc == V_TOTAL - 1) begin
                vc <= 0; row <= 0; gl <= 0; row_base <= 0;
                blink <= blink + 1'b1;
            end else begin
                vc <= vc + 1'b1;
                if (vc[0]) begin
                    if (gl == CELL_H - 1) begin
                        gl <= 0; row <= row + 1'b1; row_base <= row_base + 7'd80;
                    end else
                        gl <= gl + 1'b1;
                end
            end
        end
    end

    // line start: tell the I/O block, and ask for the next logical line.
    // What to do at the end of the line is worked out during the line.
    wire [9:0] nvc = (vc == V_TOTAL - 1) ? 10'd0 : vc + 1'b1;   // the line about to start
    // The request is made as logical line y starts, for line y + 1; its
    // text row is this one's, plus one when this is a cell's last line.
    reg        n_top = 1'b0, n_rq = 1'b0;
    reg  [8:0] n_line = 9'd0;
    reg  [4:0] n_row = 5'd0;
    always @(posedge pclk) begin
        n_top  <= nvc == 0;
        n_rq   <= !nvc[0] && (nvc < V_VIS - 2 || nvc == V_TOTAL - 2);
        n_line <= (nvc == V_TOTAL - 2) ? 9'd0 : nvc[9:1] + 1'b1;
        // at the end of line vc (odd: the second scan line of a logical
        // line), row/gl are about to advance for the line that starts
        n_row  <= (nvc == V_TOTAL - 2 || nvc == 0) ? 5'd0 :      // lines 0 and 1
                  (gl == CELL_H - 1 || (gl == CELL_H - 2 && vc[0])) ? row + 1'b1 : row;
        if (eol) begin
            line_tgl <= ~line_tgl;
            line_top <= n_top;
            if (n_rq) begin
                rq_line <= n_line;
                rq_row  <= n_row;
                rq_tgl  <= ~rq_tgl;
            end
        end
    end

    // vsync on hsync's leading edge (CEA-861)
    reg vs_line = 1'b0;
    always @(posedge pclk)
        if (hc == H_SYNC0 - 1) vs_line <= vc >= V_SYNC0 && vc < V_SYNC1;

    // ---- stage 0: addresses --------------------------------------------------
    wire [9:0]  x  = hc[10:1];
    wire [8:0]  y  = vc[9:1];
    // EXT: the line's scroll comes with its line buffer (line scroll); else VID_SCROLLX
    wire [9:0]  sxa = EXT ? lsx_a[y[0]] : sxm;
    wire [10:0] bxs = x + sxa;
    wire [9:0]  bx = (bxs >= LW) ? bxs - LW : bxs[9:0];
    wire [10:0] bxbs = x + lsx_b[y[0]];
    wire [9:0]  bxb = (bxbs >= LW) ? bxbs - LW : bxbs[9:0];
    wire [9:0]  bbyte = p_hicol ? bx : {1'b0, bx[9:1]};
    wire [6:0]  col = hc[10:4];

    reg [31:0] bword, tword, xbword, xsword, xhword;
    always @(posedge pclk) begin
        bword <= lb_bmp[HICOL8 ? {y[0], bbyte[9:2]} : {1'b0, y[0], bbyte[8:2]}];
        tword <= lb_txt[{y[0], col[6:1]}];
        // (every lane read every clock, so each is a plain block RAM read port)
        xbword <= lb_b[{y[0], bxb[9:2]}];
        xsword <= lb_s[{y[0], p_s8 ? x[9:2] : {1'b0, x[9:3]}}];
        xhword <= {lb_h3[{y[0], x[9:2]}], lb_h2[{y[0], x[9:2]}], lb_h1[{y[0], x[9:2]}], lb_h0[{y[0], x[9:2]}]};
    end
    reg [1:0] bbsel1, sbsel1, hsel1;
    reg       snhi1;
    wire [7:0] xh1 = hsel1 == 2'd0 ? xhword[7:0] : hsel1 == 2'd1 ? xhword[15:8] : hsel1 == 2'd2 ? xhword[23:16] : xhword[31:24];
    always @(posedge pclk) begin
        hsel1  <= x[1:0];
        bbsel1 <= bxb[1:0];
        sbsel1 <= p_s8 ? x[1:0] : x[2:1];
        snhi1  <= !x[0];
    end

    reg [1:0] bsel1;
    reg       nhi1, tsel1, x01, y01, vis1, txt1, cur1, hs1, vs1;
    reg [2:0] gx1;
    reg [3:0] gl1;
    always @(posedge pclk) begin
        bsel1 <= bbyte[1:0];
        nhi1  <= !bx[0];                     // 8-color: the even pixel is the high nibble
        tsel1 <= col[0];
        gx1   <= hc[3:1];
        gl1   <= gl;
        x01   <= x[0];
        y01   <= y[0];
        vis1  <= hc < H_VIS && vc < V_VIS;
        txt1  <= row < TROWS;
        cur1  <= p_cursor && !blink[5] && (row_base + col == cur_p[10:0]) && gl >= CELL_H - 2;
        hs1   <= hc >= H_SYNC0 && hc < H_SYNC1;
        vs1   <= vs_line;
    end

    // ---- stage 1: pick the byte and the cell; read palette and font ------------
    reg [7:0] byte1;
    always @*
        case (bsel1)
            2'd0: byte1 = bword[7:0];
            2'd1: byte1 = bword[15:8];
            2'd2: byte1 = bword[23:16];
            default: byte1 = bword[31:24];
        endcase
    wire [15:0] cell1 = tsel1 ? tword[31:16] : tword[15:0];

    // EXT: the layers' pixels here, and the front-most opaque one. The palette
    // is read once, for that layer's index; a 4-bit layer (playfield A in
    // 8/16-colour, the sprite buffer at 4 bpp) is direct colour.
    wire [7:0] xb1 = bbsel1 == 2'd0 ? xbword[7:0] : bbsel1 == 2'd1 ? xbword[15:8] : bbsel1 == 2'd2 ? xbword[23:16] : xbword[31:24];
    wire [7:0] xsb = sbsel1 == 2'd0 ? xsword[7:0] : sbsel1 == 2'd1 ? xsword[15:8] : sbsel1 == 2'd2 ? xsword[23:16] : xsword[31:24];
    wire [3:0] xsn = snhi1 ? xsb[7:4] : xsb[3:0];
    wire [3:0] anib = nhi1 ? byte1[7:4] : byte1[3:0];
    wire a_op1 = p_bmp && (p_hicol ? byte1 != 0 : p_c16 ? anib != 4'd8 : !anib[3]);
    wire b_op1 = p_pfb && xb1 != 0;
    wire s_op1 = p_sl && (p_s8 ? xsb != 0 : !xsn[3]);
    wire h_op1 = p_hws && xh1 != 0;
    // back to front: B (behind), A, B (in front), H (behind the sprite buffer), S, H
    reg       x_op, x_pal, x_isa;            // anything; through the palette; it is playfield A (c16 rules)
    reg [7:0] x_idx;                         // palette index
    reg [3:0] x_nib;                         // direct 4-bit colour
    always @* begin
        x_op = 1'b0; x_pal = p_hicol; x_isa = 1'b1; x_idx = byte1; x_nib = anib;
        if (b_op1 && !p_bfront) begin x_op = 1'b1; x_pal = 1'b1; x_isa = 1'b0; x_idx = xb1; end
        if (a_op1) begin x_op = 1'b1; x_pal = p_hicol; x_isa = 1'b1; x_idx = byte1; x_nib = anib; end
        if (b_op1 && p_bfront) begin x_op = 1'b1; x_pal = 1'b1; x_isa = 1'b0; x_idx = xb1; end
        if (h_op1 && p_hbeh) begin x_op = 1'b1; x_pal = 1'b1; x_isa = 1'b0; x_idx = xh1; end
        if (s_op1) begin x_op = 1'b1; x_pal = p_s8; x_isa = 1'b0; x_idx = xsb; x_nib = xsn; end
        if (h_op1 && !p_hbeh) begin x_op = 1'b1; x_pal = 1'b1; x_isa = 1'b0; x_idx = xh1; end
    end
    wire [7:0] pidx1 = EXT ? x_idx : byte1;

    reg [7:0]  font [0:256*CELL_H-1];
    initial $readmemh(FONT_FILE, font);
    wire [11:0] faddr = {cell1[7:0], 3'b000} + {cell1[7:0], 2'b00} + {cell1[7:0], 1'b0} + gl1;

    reg [15:0] c565;
    reg [7:0]  glyph;
    always @(posedge pclk) begin
        c565  <= HICOL8 ? pal[pidx1] : 16'd0;
        glyph <= font[faddr];
    end

    reg [7:0]  idx2;
    reg [3:0]  nib2;
    reg [15:0] cell2;
    reg [2:0]  gx2;
    reg        x02, y02, vis2, txt2, cur2, hs2, vs2;
    reg       xop2, xpal2, xisa2;
    always @(posedge pclk) begin
        idx2  <= byte1;
        nib2  <= EXT ? x_nib : nhi1 ? byte1[7:4] : byte1[3:0];
        xop2  <= x_op; xpal2 <= x_pal; xisa2 <= x_isa;
        cell2 <= cell1;
        gx2 <= gx1; x02 <= x01; y02 <= y01;
        vis2 <= vis1; txt2 <= txt1; cur2 <= cur1; hs2 <= hs1; vs2 <= vs1;
    end

    // ---- stage 2: dither and compose -------------------------------------------
    // 2x2 ordered dither (video.c hicolor): threshold t/8 of full scale,
    // t = 2 * bayer[y][x] + 1 with bayer = {{0, 2}, {3, 1}}
    reg [2:0] t;
    always @*
        case ({y02, x02})
            2'b00: t = 3'd1;
            2'b01: t = 3'd5;
            2'b10: t = 3'd7;
            default: t = 3'd3;
        endcase
    wire [4:0] r5 = c565[15:11], b5 = c565[4:0];
    wire [5:0] g6 = c565[10:5];
    wire [2:0] dith = { {r5, 3'b000} > t * 8'd31,
                        {g6, 3'b000} > t * 9'd63,
                        {b5, 3'b000} > t * 8'd31 };

    // the chosen layer: through the palette, or direct (16-colour rules for playfield A only)
    wire q_pal = EXT ? xpal2 : p_hicol;
    wire q_c16 = EXT ? (xisa2 && p_c16) : p_c16;
    // register the layers' colours and whether each is opaque here
    reg [2:0]  bmp3, fg3, bg3;
    reg [23:0] bmp24;
    reg        bmp_op3, on3, bg_op3, txt3, vis3, hs3, vs3;
    reg        bg24_op3, blend3;
    always @(posedge pclk) begin
        // 16-color: half brightness is a checkerboard with black on the
        // VGA (as a stippled text background), half scale on true colour
        bmp3    <= q_pal ? dith : (q_c16 && nib2[3] && (x02 ^ y02)) ? 3'd0 : nib2[2:0];
        bmp24   <= (q_pal && TRUE_COLOR) ? {r5, r5[4:2], g6, g6[5:4], b5, b5[4:2]} :
                   (q_c16 && nib2[3] && TRUE_COLOR) ? half(nib2[2:0]) :
                   full(q_pal ? dith : (q_c16 && nib2[3] && (x02 ^ y02)) ? 3'd0 : nib2[2:0]);
        bmp_op3 <= EXT ? xop2 : p_bmp && (p_hicol ? idx2 != 0 : p_c16 ? nib2 != 4'd8 : !nib2[3]);
        on3     <= glyph[3'd7 - gx2] | cur2;
        bg_op3  <= !cell2[14] && !(cell2[15] && (x02 ^ y02));
        bg24_op3 <= TRUE_COLOR ? !cell2[14] && !cell2[15] : !cell2[14] && !(cell2[15] && (x02 ^ y02));
        blend3  <= TRUE_COLOR && !cell2[14] && cell2[15];
        fg3     <= cell2[10:8];
        bg3     <= cell2[13:11];
        txt3    <= p_text && txt2;
        vis3 <= vis2; hs3 <= hs2; vs3 <= vs2;
    end

    // ---- stage 3: compose, back to front ------------------------------------------
    reg [2:0]  c;
    reg [23:0] c24, under;
    wire [23:0] bgf = full(bg3);
    wire [8:0]  sr = under[23:16] + bgf[23:16], sg = under[15:8] + bgf[15:8], sb = under[7:0] + bgf[7:0];
    always @* begin
        c = bd_p;
        if (bmp_op3) c = bmp3;
        if (txt3) begin
            if (on3)          c = fg3;
            else if (bg_op3)  c = bg3;
        end
        under = bmp_op3 ? bmp24 : full(bd_p);
        c24 = under;
        if (txt3) begin
            if (on3)            c24 = full(fg3);
            else if (bg24_op3)  c24 = bgf;
            else if (blend3)    c24 = {sr[8:1], sg[8:1], sb[8:1]};
        end
    end

    always @(posedge pclk) begin
        hsync <= hs3;
        vsync <= vs3;
        rgb   <= vis3 ? c : 3'b000;
        rgb24 <= vis3 ? c24 : 24'd0;
        de    <= vis3;
    end
endmodule
