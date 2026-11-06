`timescale 1ns/1ps
// The video extensions' registers (docs/io.md, "Video extensions"; F00220-
// F00235), for fpga/rtl/video.v with EXT = 1. Bytes as iosys takes them:
// the even byte is held, the odd one completes the word.
//
// It also watches the iosys video registers that are double-buffered with
// these (VID_CTRL bit 4, VID_BMP, VID_SPR): VID_SPR's value goes to the video
// from here, and a write to any of them sets VID_STAT bit 0 (flip pending)
// until the video takes the buffer registers for a frame (frame_latch).
module vidx_regs (
    input  wire        clk,
    input  wire        io_wr,
    input  wire [15:0] io_addr,             // F0xxxx: the low 16 bits
    input  wire [7:0]  io_wdata,
    output reg  [15:0] rd = 16'd0,          // the register at io_addr (a clock behind it)
    input  wire [15:0] vid_ctrl,            // iosys's VID_CTRL (to see bit 4 change)
    input  wire        line_tgl,            // video timing (pclk toggles), for VID_STAT bit 1
    input  wire        line_top,
    input  wire        frame_latch,         // from the video: the buffer registers were taken
    input  wire [15:0] spr_over,
    output reg  [15:0] vid_ctrl2 = 16'd0,
    output reg  [31:0] vid_bmpb = 32'd0,
    output reg  [15:0] vid_scrollxb = 16'd0,
    output reg  [15:0] vid_scrollyb = 16'd0,
    output reg  [31:0] vid_lscr = 32'd0,
    output reg  [31:0] vid_spr = 32'd0,
    output reg         spr_we = 1'b0,
    output reg  [7:0]  spr_wa = 8'd0,
    output reg  [15:0] spr_wd = 16'd0
);
    reg [7:0]  lo = 8'd0;
    wire [15:0] w = {io_wdata, lo};
    reg [7:0]  spr_index = 8'd0;
    reg        pending = 1'b0;

    // vertical blank, from the line timing (as iosys counts it)
    reg [2:0]  lt_s = 3'd0, top_s = 3'd0;
    reg [9:0]  line = 10'd0;
    always @(posedge clk) begin
        lt_s <= {lt_s[1:0], line_tgl};
        top_s <= {top_s[1:0], line_top};
        if (lt_s[2] != lt_s[1]) line <= top_s[1] ? 10'd0 : line + 1'b1;
    end
    wire vblank = line >= 10'd720;

    wire       vsel = io_addr[15:5] == 11'h011;       // F00220-F0023F
    wire [3:0] ri = io_addr[4:1];
    wire       base_reg = io_addr[15:4] == 12'h020;   // F00200-F0020F (iosys's)
    wire buf_wr = io_wr && io_addr[0] && (
        (base_reg && (io_addr[3:1] == 3'd4 || io_addr[3:1] == 3'd5 || io_addr[3:1] == 3'd6 || io_addr[3:1] == 3'd7)) ||
        (vsel && (ri == 4'd2 || ri == 4'd3 || ri == 4'd6 || ri == 4'd7)) ||
        (base_reg && io_addr[3:1] == 3'd0 && w[4] != vid_ctrl[4]));

    always @(posedge clk) begin
        spr_we <= 1'b0;
        if (buf_wr) pending <= 1'b1;
        else if (frame_latch) pending <= 1'b0;
        if (io_wr) begin
            if (!io_addr[0])
                lo <= io_wdata;
            else if (base_reg && io_addr[3:1] == 3'd6) vid_spr[15:0] <= w;
            else if (base_reg && io_addr[3:1] == 3'd7) vid_spr[31:16] <= w;
            else if (vsel) case (ri)
                4'd0: vid_ctrl2 <= {11'd0, w[4:0]};
                4'd2: vid_bmpb[15:0] <= w;
                4'd3: vid_bmpb[31:16] <= w;
                4'd4: vid_scrollxb <= w;
                4'd5: vid_scrollyb <= w;
                4'd6: vid_lscr[15:0] <= w;
                4'd7: vid_lscr[31:16] <= w;
                4'd8: spr_index <= w[7:0];
                4'd9: begin
                    spr_we <= 1'b1; spr_wa <= spr_index; spr_wd <= w;
                    spr_index <= spr_index + 1'b1;
                end
                default: ;
            endcase
        end
        // reads: only the status; the other registers are write-only (read 0)
        rd <= (ri == 4'd1) ? {14'd0, vblank, pending} : (ri == 4'd10) ? spr_over : 16'd0;
    end
endmodule
