`timescale 1ns/1ps
// 1280x720 60 Hz video with the fpga_cpu's 80x25 text layer (docs/io.md):
// a 640x360 logical screen shown at 2x, text in its top 640x350, the
// backdrop colour everywhere text does not cover.
//
// Text cells are 16 bits: [7:0] CP437 character, [10:8] foreground,
// [13:11] background, [14] transparent background, [15] stipple (the
// background drawn as a checkerboard with the backdrop). The glyphs come
// from the simulator's character ROM, so both draw the same pixels.
//
// Timing (CEA-861 720p): 74.25 MHz, 1280 + 110 front porch + 40 sync + 220
// back porch, 720 + 5 + 5 + 20 lines, positive syncs, vsync edges on hsync's
// leading edge. 45.000 kHz lines, 60.000 Hz frames.
//
// Pipeline: counters -> text RAM -> font ROM -> colour, three clocks; the
// syncs are delayed to match.
module video720 #(
    parameter FONT_FILE = "font8x14.mem",    // 256 x 14 bytes, $readmemh
    parameter TEXT_FILE = "text_demo.hex",   // 2048 x 16-bit cells
    parameter [2:0] BACKDROP = 3'd1
) (
    input  wire        clk,                  // 74.25 MHz pixel clock
    // text RAM write port, in its own clock domain
    input  wire        wclk,
    input  wire        we,
    input  wire [10:0] waddr,                // row * 80 + column
    input  wire [15:0] wdata,
    output reg         hsync = 1'b0,
    output reg         vsync = 1'b0,
    output reg  [2:0]  rgb   = 3'b000        // {r, g, b}
);
    localparam H_VIS = 1280, H_SYNC0 = 1390, H_SYNC1 = 1430, H_TOTAL = 1650;
    localparam V_VIS = 720,  V_SYNC0 = 725,  V_SYNC1 = 730,  V_TOTAL = 750;
    localparam COLS = 80, ROWS = 25, CELL_H = 14;

    reg [15:0] text [0:2047];
    reg [7:0]  font [0:256*CELL_H-1];
    initial begin
        $readmemh(TEXT_FILE, text);
        $readmemh(FONT_FILE, font);
    end

    always @(posedge wclk)
        if (we) text[waddr] <= wdata;

    // ---- counters ----------------------------------------------------------
    // Logical pixel x = hc / 2, logical line y = vc / 2. The text row and the
    // glyph line advance after the second scan line of each logical line.
    reg [10:0] hc = 0;
    reg [9:0]  vc = 0;
    reg [4:0]  row = 0;                      // text row, 0-25
    reg [3:0]  gl = 0;                       // glyph line, 0-13
    reg [10:0] row_base = 0;                 // row * 80

    wire eol = (hc == H_TOTAL - 1);
    always @(posedge clk) begin
        hc <= eol ? 11'd0 : hc + 1'b1;
        if (eol) begin
            if (vc == V_TOTAL - 1) begin
                vc <= 0; row <= 0; gl <= 0; row_base <= 0;
            end else begin
                vc <= vc + 1'b1;
                if (vc[0]) begin
                    if (gl == CELL_H - 1) begin
                        gl <= 0; row <= row + 1'b1; row_base <= row_base + COLS;
                    end else
                        gl <= gl + 1'b1;
                end
            end
        end
    end

    // ---- stage 1: read the text cell -----------------------------------------
    // vsync changes with hsync's leading edge, as CEA-861 has it: vs_line
    // takes its new value one pixel early, so vs1 and hs1 change together.
    reg vs_line = 1'b0;
    always @(posedge clk)
        if (hc == H_SYNC0 - 1) vs_line <= vc >= V_SYNC0 && vc < V_SYNC1;

    reg [15:0] tcell;
    always @(posedge clk) tcell <= text[row_base + hc[10:4]];

    reg [2:0] gx1;                           // pixel within the glyph
    reg [3:0] gl1;
    reg       vis1, txt1, par1, hs1, vs1;
    always @(posedge clk) begin
        gx1  <= hc[3:1];
        gl1  <= gl;
        vis1 <= hc < H_VIS && vc < V_VIS;
        txt1 <= row < ROWS;
        par1 <= hc[1] ^ vc[1];               // (x + y) & 1 in logical pixels
        hs1  <= hc >= H_SYNC0 && hc < H_SYNC1;
        vs1  <= vs_line;
    end

    // ---- stage 2: read the glyph row -------------------------------------------
    wire [11:0] faddr = {tcell[7:0], 3'b000} + {tcell[7:0], 2'b00} + {tcell[7:0], 1'b0} + gl1;  // char * 14 + line
    reg  [7:0]  glyph;
    always @(posedge clk) glyph <= font[faddr];

    reg [15:0] tcell2;
    reg [2:0]  gx2;
    reg        vis2, txt2, par2, hs2, vs2;
    always @(posedge clk) begin
        tcell2 <= tcell; gx2 <= gx1;
        vis2 <= vis1; txt2 <= txt1; par2 <= par1; hs2 <= hs1; vs2 <= vs1;
    end

    // ---- stage 3: colour ---------------------------------------------------------
    wire on      = glyph[3'd7 - gx2];        // bit 7 is the leftmost pixel
    wire show_bg = !tcell2[14] && !(tcell2[15] && par2);

    always @(posedge clk) begin
        hsync <= hs2;
        vsync <= vs2;
        if (!vis2)        rgb <= 3'b000;
        else if (!txt2)   rgb <= BACKDROP;
        else if (on)      rgb <= tcell2[10:8];
        else if (show_bg) rgb <= tcell2[13:11];
        else              rgb <= BACKDROP;
    end
endmodule
