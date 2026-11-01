`timescale 1ns/1ps
// The fpga_cpu video (docs/io.md, "Video"), as sim/src/video.c draws it:
// 1280x720 60 Hz, a 640x360 logical screen shown 2x, with the backdrop,
// the bitmap (8-color or hi-color through the 256-entry palette, dithered
// 2x2 to the 3-bit VGA, scrolled in x and y) and the 80x25 text layer.
// The sprite layer is not here yet.
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
    parameter TRUE_COLOR = 0
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
    // DMA through memsys (clk domain)
    output wire        dma_req,
    output reg  [17:0] dma_addr = 18'd0,
    output reg  [8:0]  dma_left = 9'd0,
    input  wire [2:0]  dma_take,
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
    output reg         de = 1'b0
);
    function [23:0] full;                    // a 3-bit colour at full scale
        input [2:0] c;
        full = {{8{c[2]}}, {8{c[1]}}, {8{c[0]}}};
    endfunction
    localparam H_VIS = 1280, H_SYNC0 = 1390, H_SYNC1 = 1430, H_TOTAL = 1650;
    localparam V_VIS = 720,  V_SYNC0 = 725,  V_SYNC1 = 730,  V_TOTAL = 750;
    localparam LW = 640, LH = 360, TROWS = 25, CELL_H = 14;

    // VID_CTRL bits
    wire c_text = vid_ctrl[0], c_bmp = vid_ctrl[1], c_hicol = vid_ctrl[2];

    // =====================================================================
    // Line buffers, palette (written at clk, read at pclk)
    // =====================================================================
    reg [31:0] lb_bmp [0:511];               // {buffer, word 0-159}
    (* ram_style = "block" *)
    reg [31:0] lb_txt [0:127];               // {buffer, word 0-39}
    reg [15:0] pal [0:255];

    reg        lbw_bmp = 1'b0, lbw_txt = 1'b0;
    reg [8:0]  lbw_a = 9'd0;
    reg [31:0] lbw_d = 32'd0;
    always @(posedge clk) begin
        if (lbw_bmp) lb_bmp[lbw_a] <= lbw_d;
        if (lbw_txt) lb_txt[lbw_a[6:0]] <= lbw_d;
        if (pal_we)  pal[pal_windex] <= pal_wdata;
    end

    // =====================================================================
    // DMA engine (clk)
    // =====================================================================
    // VID_SCROLLY mod 360, by repeated subtraction whenever it changes
    reg [15:0] sy_src = 16'd0, sy_rem = 16'd0;
    always @(posedge clk)
        if (vid_scrolly != sy_src) begin
            sy_src <= vid_scrolly;
            sy_rem <= vid_scrolly;
        end else if (sy_rem >= LH)
            sy_rem <= sy_rem - LH;
    wire [8:0] sym = sy_rem[8:0];

    // requests from the pixel side: fetch logical line rq_line into buffer rq_line[0]
    reg        rq_tgl = 1'b0;                // pclk
    reg  [8:0] rq_line = 9'd0;               // pclk, stable around the toggle
    reg  [4:0] rq_row = 5'd0;                // pclk: its text row
    reg  [2:0] rq_s = 3'b000;
    always @(posedge clk) rq_s <= {rq_s[1:0], rq_tgl};
    wire rq = rq_s[2] != rq_s[1];

    reg  [1:0] st = 2'd0;                    // 0 idle, 1 plan, 2 fetch bitmap, 3 fetch text
    reg  [8:0] ln = 9'd0;                    // line being fetched
    reg  [4:0] tr = 5'd0;                    // its text row
    reg  [9:0] by = 10'd0;
    reg  [8:0] bwords = 9'd0, twords = 9'd0;
    reg  [8:0] wi = 9'd0;                    // words written into the line buffers
    reg [19:0] tstart = 20'd0;
    wire [19:0] boff = c_hicol ? ({by, 9'd0} + {by, 7'd0}) : ({by, 8'd0} + {by, 6'd0});   // by * 640 or 320
    wire [19:0] toff = {tr, 7'd0} + {tr, 5'd0};                                          // tr * 160

    assign dma_req = (st == 2'd2 || st == 2'd3) && dma_left != 0;

    always @(posedge clk) begin
        lbw_bmp <= 1'b0;
        lbw_txt <= 1'b0;
        if (rq) begin
            ln <= rq_line;
            tr <= rq_row;
            by <= ({1'b0, rq_line} + sym >= LH) ? {1'b0, rq_line} + sym - LH : {1'b0, rq_line} + sym;
            st <= 2'd1;
            wi <= 9'd0;
            dma_left <= 9'd0;
        end else case (st)
            2'd1: begin
                bwords   <= c_bmp ? (c_hicol ? 9'd160 : 9'd80) : 9'd0;
                twords   <= (c_text && ln < TROWS * CELL_H) ? 9'd40 : 9'd0;
                tstart   <= vid_text[19:0] + toff;
                dma_addr <= (vid_bmp[19:0] + boff) >> 2;
                dma_left <= c_bmp ? (c_hicol ? 9'd160 : 9'd80) : 9'd0;
                st       <= 2'd2;
            end
            2'd2, 2'd3: begin
                if (dma_take != 0) begin
                    dma_addr <= dma_addr + dma_take;
                    dma_left <= dma_left - dma_take;
                end else if (dma_left == 0) begin
                    if (st == 2'd2) begin
                        dma_addr <= tstart[19:2];
                        dma_left <= twords;
                        st       <= 2'd3;
                    end else if (wi == bwords + twords)
                        st <= 2'd0;
                end
            end
            default: ;
        endcase

        if (dma_valid && st != 2'd1 && !rq) begin
            lbw_d <= dma_data;
            if (wi < bwords) begin
                lbw_bmp <= 1'b1;
                lbw_a   <= {ln[0], wi[7:0]};
            end else begin
                lbw_txt <= 1'b1;
                lbw_a   <= {2'b00, ln[0], wi[5:0] - bwords[5:0]};
            end
            wi <= wi + 1'b1;
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
    wire p_text = ctrl_p[0], p_bmp = ctrl_p[1], p_hicol = ctrl_p[2], p_cursor = ctrl_p[5];

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
    wire [10:0] bxs = x + sxm;
    wire [9:0]  bx = (bxs >= LW) ? bxs - LW : bxs[9:0];
    wire [9:0]  bbyte = p_hicol ? bx : {1'b0, bx[9:1]};
    wire [6:0]  col = hc[10:4];

    reg [31:0] bword, tword;
    always @(posedge pclk) begin
        bword <= lb_bmp[{y[0], bbyte[9:2]}];
        tword <= lb_txt[{y[0], col[6:1]}];
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

    reg [7:0]  font [0:256*CELL_H-1];
    initial $readmemh(FONT_FILE, font);
    wire [11:0] faddr = {cell1[7:0], 3'b000} + {cell1[7:0], 2'b00} + {cell1[7:0], 1'b0} + gl1;

    reg [15:0] c565;
    reg [7:0]  glyph;
    always @(posedge pclk) begin
        c565  <= pal[byte1];
        glyph <= font[faddr];
    end

    reg [7:0]  idx2;
    reg [3:0]  nib2;
    reg [15:0] cell2;
    reg [2:0]  gx2;
    reg        x02, y02, vis2, txt2, cur2, hs2, vs2;
    always @(posedge pclk) begin
        idx2  <= byte1;
        nib2  <= nhi1 ? byte1[7:4] : byte1[3:0];
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

    // register the layers' colours and whether each is opaque here
    reg [2:0]  bmp3, fg3, bg3;
    reg [23:0] bmp24;
    reg        bmp_op3, on3, bg_op3, txt3, vis3, hs3, vs3;
    reg        bg24_op3, blend3;
    always @(posedge pclk) begin
        bmp3    <= p_hicol ? dith : nib2[2:0];
        bmp24   <= (p_hicol && TRUE_COLOR) ? {r5, r5[4:2], g6, g6[5:4], b5, b5[4:2]}
                                           : full(p_hicol ? dith : nib2[2:0]);
        bmp_op3 <= p_bmp && (p_hicol ? idx2 != 0 : !nib2[3]);
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
