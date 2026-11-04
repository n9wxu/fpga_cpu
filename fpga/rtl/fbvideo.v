`timescale 1ns/1ps
// A 1280x720 60 Hz framebuffer in SRAM, 4 bits a pixel (3-bit colour,
// bit 3 unused), read every scan line through a rammux port: 160 words a
// line, eight pixels a word, the leftmost in bits 3:0. The VGA timing is
// video720.v's (vsync on hsync's leading edge).
//
// As line y starts on the screen, line y + 1 is asked for (into the other
// half of a two-line buffer); asking for the next while the last is not
// all back is an underrun (the RAM missed a line's deadline).
module fbvideo #(
    parameter [17:0] BASE = 18'd0           // word address of the framebuffer
) (
    input  wire        clk,                 // the RAM's clock (50 MHz)
    input  wire        pclk,                // 74.25 MHz
    // rammux port
    output wire        req,
    output wire [17:0] addr,
    output wire [4:0]  len,
    input  wire        ack,
    input  wire        rvalid,
    input  wire [31:0] rdata,
    // status (clk)
    output reg         underrun = 1'b0,     // pulse
    output reg         line_done = 1'b0,    // pulse: a line is in
    // VGA
    output reg         hsync = 1'b0,
    output reg         vsync = 1'b0,
    output reg  [2:0]  rgb = 3'b000
);
    localparam H_VIS = 1280, H_SYNC0 = 1390, H_SYNC1 = 1430, H_TOTAL = 1650;
    localparam V_VIS = 720,  V_SYNC0 = 725,  V_SYNC1 = 730,  V_TOTAL = 750;
    localparam WPL = 160;                   // words a line

    // ---- the line buffer: two lines, written on clk, read on pclk
    reg  [31:0] lb [0:511];
    reg         lbw = 1'b0;
    reg  [8:0]  lbw_a = 9'd0;
    reg  [31:0] lbw_d = 32'd0;
    always @(posedge clk) if (lbw) lb[lbw_a] <= lbw_d;

    // ---- pixel side
    reg  [10:0] hc = 11'd0;
    reg  [9:0]  vc = 10'd0;
    wire        eol = hc == H_TOTAL - 1;
    always @(posedge pclk) begin
        hc <= eol ? 11'd0 : hc + 1'b1;
        if (eol) vc <= (vc == V_TOTAL - 1) ? 10'd0 : vc + 1'b1;
    end
    // at the start of line vc (visible, or the frame's last), ask for the next
    reg         rq_tgl = 1'b0;
    reg  [9:0]  rq_line = 10'd0;
    always @(posedge pclk)
        if (hc == 11'd0 && (vc < V_VIS - 1 || vc == V_TOTAL - 1)) begin
            rq_line <= (vc == V_TOTAL - 1) ? 10'd0 : vc + 1'b1;
            rq_tgl  <= ~rq_tgl;
        end
    reg vs_line = 1'b0;
    always @(posedge pclk) if (hc == H_SYNC0 - 1) vs_line <= vc >= V_SYNC0 && vc < V_SYNC1;

    reg  [31:0] w1;
    reg  [2:0]  px1;
    reg         vis1, hs1, vs1;
    always @(posedge pclk) begin
        w1   <= lb[{vc[0], hc[10:3]}];
        px1  <= hc[2:0];
        vis1 <= hc < H_VIS && vc < V_VIS;
        hs1  <= hc >= H_SYNC0 && hc < H_SYNC1;
        vs1  <= vs_line;
    end
    always @(posedge pclk) begin
        rgb   <= vis1 ? w1[4 * px1 +: 3] : 3'b000;
        hsync <= hs1;
        vsync <= vs1;
    end

    // ---- RAM side
    reg  [2:0]  rq_s = 3'b000;
    always @(posedge clk) rq_s <= {rq_s[1:0], rq_tgl};
    wire        rq = rq_s[2] != rq_s[1];
    reg  [9:0]  ln = 10'd0;                 // the line being fetched
    reg  [7:0]  asked = 8'd0, got = 8'd0;
    reg         act = 1'b0;
    reg  [17:0] lbase = 18'd0;
    assign req  = act && asked != WPL;
    assign addr = lbase + {10'd0, asked};
    assign len  = (WPL - asked >= 16) ? 5'd16 : WPL - asked;
    always @(posedge clk) begin
        lbw <= 1'b0; underrun <= 1'b0; line_done <= 1'b0;
        if (rq) begin
            if (act && got != WPL) underrun <= 1'b1;   // the last line was not all in
            act <= 1'b1; asked <= 8'd0; got <= 8'd0;
            ln <= rq_line;
            lbase <= BASE + {rq_line, 7'd0} + {2'd0, rq_line, 5'd0};   // line * 160
        end else begin
            if (ack) asked <= asked + ((WPL - asked >= 16) ? 8'd16 : WPL - asked);
            if (rvalid && act) begin
                lbw <= 1'b1; lbw_a <= {ln[0], got}; lbw_d <= rdata;
                got <= got + 8'd1;
                if (got == WPL - 1) line_done <= 1'b1;
            end
        end
    end
endmodule
