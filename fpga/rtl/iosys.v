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
// Not here yet: the keyboard (KBD_DATA reads "no key"), the file service.
// CON_IN comes from con_in (bit 8: empty; con_take pulses when a read takes
// the byte); CON_OUT and EXIT are con_we/exit_we with wvalue.
//
// HICOL8 = 0 is the low video tier (the Spartan-3): no palette (PAL_INDEX,
// PAL_DATA do nothing), no sprite registers, VID_CTRL keeps only the bits
// that video.v uses there (0 text, 1 bitmap, 5 cursor, 7 16-color), and the
// other video registers are write-only (they read 0).
// AUDIO = 1: a sound block's registers (audio_lite.v) read through aud_rd
// (16 bits, zero when the address is not its own); it takes the writes
// itself, from the same io_wr/io_addr/io_wdata.
module iosys #(
    parameter HICOL8 = 1,
    parameter AUDIO = 0,
    parameter IRQ = 0                       // 1: the interrupt controller's device sources (irq_src)
) (
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
    input  wire [31:0] insts,              // instructions retired (INSTRET)
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
    // console: CON_IN in, CON_OUT/EXIT out (with wvalue)
    input  wire [8:0]  con_in,              // bit 8: nothing waiting
    output reg         con_take = 1'b0,
    output reg         con_we = 1'b0,
    output reg         exit_we = 1'b0,
    output reg  [15:0] wvalue = 16'd0,
    // sound (AUDIO = 1; unconnected otherwise)
    input  wire [15:0] aud_rd,
    // interrupts (F00700): sources in, the line out
    input  wire [6:0]  irq_src,             // pulses: sources 1-6 (bit 0 is the frame, from here)
    output wire        irq
);
    // ---- frame and scan line -------------------------------------------------
    // line_tgl and line_top change at the same pixel-clock edge and cross
    // separately, so the toggle is seen three clocks after line_top has
    // settled (with one clock between them, the skew of the two crossings
    // sometimes lost line 0: lines counted to 1499 and a frame in three
    // went uncounted, FRAME at 40 Hz)
    reg [4:0]  lt_s = 5'b00000;
    reg [1:0]  top_s = 2'b00;
    reg [11:0] line = 12'd0;
    reg [15:0] frame = 16'd0;
    always @(posedge clk) begin
        lt_s  <= {lt_s[3:0], line_tgl};
        top_s <= {top_s[0], line_top};
        if (lt_s[4] != lt_s[3]) begin
            line <= top_s[1] ? 12'd0 : line + 1'b1;
            if (!top_s[1] && line == 12'd719) frame <= frame + 1'b1;   // vertical blank starts
        end
    end
    wire vblank = line >= 12'd720;
    reg  frame_tick = 1'b0;                 // FRAME moved this clock
    always @(posedge clk) frame_tick <= (lt_s[4] != lt_s[3]) && !top_s[1] && line == 12'd719;

    // ---- interrupts: 8 sources (bit 7: software), pending and enabled
    reg  [7:0] irq_pend = 8'd0, irq_en = 8'd0;
    assign irq = (irq_pend & irq_en) != 8'd0;

    // ---- registers -----------------------------------------------------------
    reg [15:0] kbd_ctrl = 16'd0;
    reg [7:0]  pal_index = 8'd0;
    reg [15:0] spr_lo = 16'd0, spr_hi = 16'd0;

    function [15:0] rv;                     // the live value of a register
        input [15:0] a;
        case ({a[15:1], 1'b0})
            16'h0004: rv = cycles[15:0];
            16'h0006: rv = cycles[31:16];
            16'h0700: rv = {8'd0, irq_pend};
            16'h0702: rv = {8'd0, irq_en};
            16'h0010: rv = insts[15:0];
            16'h0012: rv = insts[31:16];
            16'h0008: rv = con_in[8] ? 16'hFFFF : {8'd0, con_in[7:0]};
            16'h000A: rv = frame;
            16'h000C: rv = {line, 3'b000, vblank};
            16'h0100: rv = 16'hFFFF;        // KBD_DATA: no key
            16'h0104: rv = kbd_ctrl;
            16'h0200: rv = vid_ctrl;
            // (the low tier: the video's addresses, scroll, cursor and
            // backdrop are write-only, read 0: a smaller read multiplexer)
            16'h0202: rv = HICOL8 ? {13'd0, vid_backdrop} : 16'd0;
            16'h0204: rv = HICOL8 ? vid_text[15:0] : 16'd0;
            16'h0206: rv = HICOL8 ? vid_text[31:16] : 16'd0;
            16'h0208: rv = HICOL8 ? vid_bmp[15:0] : 16'd0;
            16'h020A: rv = HICOL8 ? vid_bmp[31:16] : 16'd0;
            16'h020C: rv = HICOL8 ? spr_lo : 16'd0;
            16'h020E: rv = HICOL8 ? spr_hi : 16'd0;
            16'h0210: rv = HICOL8 ? {8'd0, pal_index} : 16'd0;
            16'h0214: rv = HICOL8 ? vid_scrollx : 16'd0;
            16'h0216: rv = HICOL8 ? vid_scrolly : 16'd0;
            16'h0218: rv = HICOL8 ? vid_cursor : 16'd0;
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
    wire [15:0] live = rv(cur_addr) | (AUDIO ? aud_rd : 16'd0);
    always @(posedge clk) begin
        con_take <= 1'b0;
        if (cpu_lat && cur_rd) begin
            if (!cur_addr[0]) begin
                io_rdata <= live[7:0];
                latch    <= live;
                latch_a  <= cur_addr;
                con_take <= {cur_addr[15:1], 1'b0} == 16'h0008;
            end else
                io_rdata <= (cur_addr == (latch_a | 16'd1)) ? latch[15:8] : live[15:8];
        end
    end

    // writes
    reg [7:0] lo = 8'd0;
    wire [15:0] w = {io_wdata, lo};

    // the pending sources: set by the sources and IRQ_SET, cleared by
    // writing 1s to IRQ_PEND (a source that fires in the same clock wins)
    reg  [7:0] irq_set_w, irq_clr_w;
    always @* begin
        irq_set_w = {1'b0, IRQ ? irq_src : 7'd0} | {7'd0, frame_tick};
        irq_clr_w = 8'd0;
        if (io_wr && io_addr[0] && {io_addr[15:1], 1'b0} == 16'h0704) irq_set_w = irq_set_w | w[7:0];
        if (io_wr && io_addr[0] && {io_addr[15:1], 1'b0} == 16'h0700) irq_clr_w = w[7:0];
    end
    always @(posedge clk) irq_pend <= (irq_pend & ~irq_clr_w) | irq_set_w;

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
                    16'h0200: vid_ctrl <= HICOL8 ? w : (w & 16'h00A3);
                    16'h0202: vid_backdrop <= w[2:0];
                    16'h0204: vid_text[15:0] <= w;
                    16'h0206: vid_text[31:16] <= w;
                    16'h0208: vid_bmp[15:0] <= w;
                    16'h020A: vid_bmp[31:16] <= w;
                    16'h020C: if (HICOL8) spr_lo <= w;
                    16'h020E: if (HICOL8) spr_hi <= w;
                    16'h0210: if (HICOL8) pal_index <= w[7:0];
                    16'h0212: if (HICOL8) begin
                        pal_we     <= 1'b1;
                        pal_windex <= pal_index;
                        pal_wdata  <= w;
                        pal_index  <= pal_index + 1'b1;
                    end
                    16'h0214: vid_scrollx <= w;
                    16'h0216: vid_scrolly <= w;
                    16'h0218: vid_cursor <= w;
                    16'h0702: irq_en <= w[7:0];
                    16'h0600: leds <= w[7:0];
                    16'h0606: seg7 <= w;
                    16'h0608: seg7_ctrl <= w[7:0];
                    default: ;
                endcase
            end
        end
    end
endmodule
