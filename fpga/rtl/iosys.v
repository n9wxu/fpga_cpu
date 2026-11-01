`timescale 1ns/1ps
// The I/O registers of docs/io.md, as the simulator implements them, for a
// CPU with an 8-bit bus. Registers are 16 bits at even addresses:
//
//   - A write to the low byte is held; the write to the high byte commits
//     both (STW writes low then high).
//   - Reads are registered at e2 of the CPU period (cpu_lat), from the
//     settled address, so T1 sees a value that holds still until its edge.
//     A read of the low byte also latches the whole register, and a read of
//     the next byte returns the latched high byte, so LDW never sees a value
//     torn between its two bytes.
//
// Not here yet: the keyboard (KBD_DATA reads "no key"), sound, the file
// service, CON_IN (reads "no input") and CON_OUT/EXIT (ignored, but
// visible on con_we/exit_we for testbenches).
module iosys (
    input  wire        clk,                 // 50 MHz
    input  wire        io_wr,
    input  wire        io_rd,
    input  wire [15:0] io_addr,             // F0xxxx: the low 16 bits
    input  wire [7:0]  io_wdata,
    input  wire        cpu_lat,             // e2 of a CPU period
    input  wire        cur_rd,              // the CPU reads I/O now
    input  wire [15:0] cur_addr,            // the CPU's address now
    output reg  [7:0]  io_rdata = 8'd0,
    // counters
    input  wire [31:0] cycles,
    // video timing from the pixel clock domain (toggles: synchronized here)
    input  wire        line_tgl,            // a scan line started
    input  wire        line_top,            // ... and it is line 0
    // video registers
    output reg  [15:0] vid_ctrl = 16'd0,
    output reg  [2:0]  vid_backdrop = 3'd0,
    output reg  [31:0] vid_text = 32'd0,
    output reg  [31:0] vid_bmp = 32'd0,
    output reg  [15:0] vid_scrollx = 16'd0,
    output reg  [15:0] vid_scrolly = 16'd0,
    output reg  [15:0] vid_cursor = 16'd0,
    output reg         pal_we = 1'b0,       // palette write: pal_data at pal_index
    output reg  [7:0]  pal_windex = 8'd0,
    output reg  [15:0] pal_wdata = 16'd0,
    // board
    input  wire [7:0]  switches,
    input  wire [3:0]  buttons,
    output reg  [7:0]  leds = 8'd0,
    output reg  [15:0] seg7 = 16'd0,
    output reg  [7:0]  seg7_ctrl = 8'h00,
    // for testbenches
    output reg         con_we = 1'b0,
    output reg         exit_we = 1'b0,
    output reg  [15:0] wvalue = 16'd0
);
    // ---- frame and scan line -------------------------------------------------
    reg [2:0]  lt_s = 3'b000;
    reg [1:0]  top_s = 2'b00;
    reg [11:0] line = 12'd0;
    reg [15:0] frame = 16'd0;
    always @(posedge clk) begin
        lt_s  <= {lt_s[1:0], line_tgl};
        top_s <= {top_s[0], line_top};
        if (lt_s[2] != lt_s[1]) begin
            line <= top_s[1] ? 12'd0 : line + 1'b1;
            if (!top_s[1] && line == 12'd719) frame <= frame + 1'b1;   // vertical blank starts
        end
    end
    wire vblank = line >= 12'd720;

    // ---- registers -----------------------------------------------------------
    reg [15:0] kbd_ctrl = 16'd0;
    reg [7:0]  pal_index = 8'd0;
    reg [15:0] spr_lo = 16'd0, spr_hi = 16'd0;

    function [15:0] rv;                     // the live value of a register
        input [15:0] a;
        case ({a[15:1], 1'b0})
            16'h0004: rv = cycles[15:0];
            16'h0006: rv = cycles[31:16];
            16'h0008: rv = 16'hFFFF;        // CON_IN: nothing waiting
            16'h000A: rv = frame;
            16'h000C: rv = {line, 3'b000, vblank};
            16'h0100: rv = 16'hFFFF;        // KBD_DATA: no key
            16'h0104: rv = kbd_ctrl;
            16'h0200: rv = vid_ctrl;
            16'h0202: rv = {13'd0, vid_backdrop};
            16'h0204: rv = vid_text[15:0];
            16'h0206: rv = vid_text[31:16];
            16'h0208: rv = vid_bmp[15:0];
            16'h020A: rv = vid_bmp[31:16];
            16'h020C: rv = spr_lo;
            16'h020E: rv = spr_hi;
            16'h0210: rv = {8'd0, pal_index};
            16'h0214: rv = vid_scrollx;
            16'h0216: rv = vid_scrolly;
            16'h0218: rv = vid_cursor;
            16'h0302: rv = 16'd0;           // file service: never busy
            16'h0600: rv = {8'd0, leds};
            16'h0602: rv = {8'd0, switches};
            16'h0604: rv = {12'd0, buttons};
            16'h0606: rv = seg7;
            16'h0608: rv = {8'd0, seg7_ctrl};
            default:  rv = 16'd0;
        endcase
    endfunction

    // reads: the low byte live (and the whole register latched), the high
    // byte from the latch when it follows
    reg [15:0] latch = 16'd0;
    reg [15:0] latch_a = 16'hFFFF;
    wire [15:0] live = rv(cur_addr);
    always @(posedge clk)
        if (cpu_lat && cur_rd) begin
            if (!cur_addr[0]) begin
                io_rdata <= live[7:0];
                latch    <= live;
                latch_a  <= cur_addr;
            end else
                io_rdata <= (cur_addr == (latch_a | 16'd1)) ? latch[15:8] : live[15:8];
        end

    // writes
    reg [7:0] lo = 8'd0;
    wire [15:0] w = {io_wdata, lo};

    always @(posedge clk) begin
        pal_we  <= 1'b0;
        con_we  <= 1'b0;
        exit_we <= 1'b0;
        if (io_wr) begin
            if (!io_addr[0])
                lo <= io_wdata;
            else begin
                wvalue <= w;
                case ({io_addr[15:1], 1'b0})
                    16'h0000: con_we <= 1'b1;
                    16'h0002: exit_we <= 1'b1;
                    16'h0104: kbd_ctrl <= {15'd0, w[0]};
                    16'h0200: vid_ctrl <= w;
                    16'h0202: vid_backdrop <= w[2:0];
                    16'h0204: vid_text[15:0] <= w;
                    16'h0206: vid_text[31:16] <= w;
                    16'h0208: vid_bmp[15:0] <= w;
                    16'h020A: vid_bmp[31:16] <= w;
                    16'h020C: spr_lo <= w;
                    16'h020E: spr_hi <= w;
                    16'h0210: pal_index <= w[7:0];
                    16'h0212: begin
                        pal_we     <= 1'b1;
                        pal_windex <= pal_index;
                        pal_wdata  <= w;
                        pal_index  <= pal_index + 1'b1;
                    end
                    16'h0214: vid_scrollx <= w;
                    16'h0216: vid_scrolly <= w;
                    16'h0218: vid_cursor <= w;
                    16'h0600: leds <= w[7:0];
                    16'h0606: seg7 <= w;
                    16'h0608: seg7_ctrl <= w[7:0];
                    default: ;
                endcase
            end
        end
    end
endmodule
