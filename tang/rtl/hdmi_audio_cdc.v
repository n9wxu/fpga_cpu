`timescale 1ns/1ps
// Audio samples across clock domains: a 16-entry asynchronous FIFO with
// gray-code pointers, from the audio engine's clock (s_clk, any rate:
// 67.5 MHz on the Tang) to the HDMI pixel clock, where dvi_tx (AUDIO=1) pops
// one stereo sample 48,000 times a second.
//
// Write side (s_clk): s_valid with s_l and s_r (signed 16-bit) is taken in
// a cycle where s_ready is high; with s_ready low the sample is not taken.
// Read side (p_clk): dvi_tx's a_pop pulse pops; p_l and p_r show the oldest
// sample before the pop, and are 0 (silence) when the FIFO is empty, in
// which case the pop does nothing. p_under pulses on such a pop.
//
//   hdmi_audio_cdc fifo (.s_clk(fclk), .s_valid(v), .s_l(l), .s_r(r), .s_ready(rdy),
//                        .p_clk(pclk), .p_pop(pop), .p_l(a_l), .p_r(a_r), .p_under());
//   dvi_tx #(.HDMI(1), .AUDIO(1)) tx (..., .a_l(a_l), .a_r(a_r), .a_pop(pop), ...);
//
// The engine should offer samples at 48 kHz (as exact as the clocks: both
// come from the 27 MHz crystal, so the rates cannot drift) and the FIFO
// rides half full. Nothing is reset; the pointers start at zero.
module hdmi_audio_cdc (
    input  wire        s_clk,
    input  wire        s_valid,
    input  wire [15:0] s_l,
    input  wire [15:0] s_r,
    output wire        s_ready,
    input  wire        p_clk,
    input  wire        p_pop,
    output wire [15:0] p_l,
    output wire [15:0] p_r,
    output wire        p_under
);
    reg [31:0] mem [0:15];
    reg  [4:0] rb = 5'd0, rg = 5'd0;                // read side pointers, binary and gray

    // write side: binary and gray pointers (5 bits: the extra one tells full from empty)
    reg  [4:0] wb = 5'd0, wg = 5'd0;
    reg  [4:0] rg_s1 = 5'd0, rg_s2 = 5'd0;          // the read pointer, in s_clk
    wire       wr   = s_valid && s_ready;
    wire [4:0] wb_n = wb + wr;
    wire [4:0] wg_n = wb_n ^ (wb_n >> 1);
    // full: the write pointer is a lap ahead of the read pointer (top two gray bits differ)
    assign s_ready = wg != {~rg_s2[4:3], rg_s2[2:0]};
    always @(posedge s_clk) begin
        if (wr) mem[wb[3:0]] <= {s_l, s_r};
        wb <= wb_n;
        wg <= wg_n;
        rg_s1 <= rg;
        rg_s2 <= rg_s1;
    end

    // read side
    reg  [4:0] wg_p1 = 5'd0, wg_p2 = 5'd0;          // the write pointer, in p_clk
    wire       empty = rg == wg_p2;
    wire [4:0] rb_n = rb + (p_pop && !empty);
    wire [31:0] d = mem[rb[3:0]];
    assign {p_l, p_r} = empty ? 32'd0 : d;
    assign p_under = p_pop && empty;
    always @(posedge p_clk) begin
        rb <= rb_n;
        rg <= rb_n ^ (rb_n >> 1);
        wg_p1 <= wg;
        wg_p2 <= wg_p1;
    end
endmodule
