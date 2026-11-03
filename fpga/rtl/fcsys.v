`timescale 1ns/1ps
// fpga_cpu on the Spartan-3 Starter Board with FC, the pipelined core
// (verilog/rtl/fc_core.v, docs/fc.md): 1 MB of SRAM, the video of
// docs/io.md at 1280x720, the I/O registers, program loading over JTAG.
// The same board interface, video, I/O and loader as system.v; fcmem.v
// shares the SRAM.
//
// FC runs at 25 MHz, on the 50 MHz clock gated to every other edge. With
// no instruction cache, fetches and data share the one SRAM port, one
// access an FC clock (video DMA takes every other clock while it fetches a
// line).
module fcsys (
    input  wire        clk,                 // 50 MHz
    input  wire [3:0]  btn,
    input  wire [7:0]  sw,
    output wire [7:0]  led,
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
    wire clk50, pclk, locked;
    clkgen clocks (.clk_in(clk), .clk50(clk50), .clk_pix(pclk), .locked(locked));

    // ---- FC on its gated clock ----------------------------------------------------
    wire        cpu_ce, cpu_clk;
    // (from clk50 in simulation too: FC's edges must come with or after
    // fcmem's, as on the board, where the gated clock is the later one)
    clkgate gate (.clk(clk50), .ce(cpu_ce), .gclk(cpu_clk));
    wire        run;
    wire [23:0] entry;
    reg         fc_rst = 1'b1;              // on FC's own clock (as system.v's t1_rst)
    always @(posedge cpu_clk) fc_rst <= !run;

    wire        i_req, i_gnt, i_rvalid, d_req, d_we, d_gnt, d_rvalid, halted, retire;
    wire [23:2] i_addr;
    wire [23:0] d_addr;
    wire [3:0]  d_be;
    wire [31:0] i_rdata, d_wdata, d_rdata;
    wire [1:0]  sr_sel;
    reg  [15:0] sr_in;
    wire [7:0]  halt_code;
    fc_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(fc_rst), .reset_pc(entry),
        .i_req(i_req), .i_addr(i_addr), .i_gnt(i_gnt), .i_rvalid(i_rvalid), .i_rdata(i_rdata),
        .d_req(d_req), .d_we(d_we), .d_addr(d_addr), .d_be(d_be), .d_wdata(d_wdata),
        .d_gnt(d_gnt), .d_rvalid(d_rvalid), .d_rdata(d_rdata),
        .sr_sel(sr_sel), .sr_in(sr_in), .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    // SR1-SR3 and CYCLE: FC clocks and instructions since the start
    reg [31:0] cycles = 32'd0;
    reg [15:0] insts = 16'd0;
    always @(posedge cpu_clk)
        if (fc_rst) begin cycles <= 32'd0; insts <= 16'd0; end
        else begin cycles <= cycles + 1'b1; insts <= insts + retire; end
    always @*
        case (sr_sel)
            2'd0: sr_in = cycles[15:0];
            2'd1: sr_in = cycles[31:16];
            default: sr_in = insts;
        endcase

    // ---- memory ------------------------------------------------------------------
    wire        io_wr, io_lat;
    wire [15:0] io_addr;
    wire [7:0]  io_wdata, io_rdata;
    wire        dma_valid;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    wire [2:0]  dma_take;
    wire [31:0] dma_data;
    wire        ldr_req, ldr_rd, ldr_done;
    wire [31:0] ldr_q, ldr_data;
    wire [17:0] ldr_addr;
    fcmem mem (
        .clk(clk50), .cpu_ce(cpu_ce),
        .i_req(i_req), .i_addr(i_addr), .i_gnt(i_gnt), .i_rvalid(i_rvalid), .i_rdata(i_rdata),
        .d_req(d_req), .d_we(d_we), .d_addr(d_addr), .d_be(d_be), .d_wdata(d_wdata),
        .d_gnt(d_gnt), .d_rvalid(d_rvalid), .d_rdata(d_rdata),
        .io_wr(io_wr), .io_lat(io_lat), .io_addr(io_addr), .io_wdata(io_wdata), .io_rdata(io_rdata),
        .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take), .dma_valid(dma_valid), .dma_data(dma_data),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_addr(ldr_addr), .ldr_data(ldr_data),
        .ldr_done(ldr_done), .ldr_q(ldr_q),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n)
    );

    // ---- I/O ---------------------------------------------------------------------
    wire        line_tgl, line_top;
    wire [15:0] vid_ctrl, vid_scrollx, vid_scrolly, vid_cursor, pal_wdata;
    wire [2:0]  vid_backdrop;
    wire [31:0] vid_text, vid_bmp;
    wire        pal_we;
    wire [7:0]  pal_windex;
    wire [15:0] seg7;
    wire [7:0]  seg7_ctrl;
    reg  [3:0]  btn_s0 = 4'd0, btn_s = 4'd0;
    reg  [7:0]  sw_s0 = 8'd0, sw_s = 8'd0;
    always @(posedge clk50) begin
        btn_s0 <= btn; btn_s <= btn_s0;
        sw_s0  <= sw;  sw_s  <= sw_s0;
    end
    iosys io (
        .clk(clk50), .io_wr(io_wr), .io_rd(1'b0), .io_addr(io_addr), .io_wdata(io_wdata),
        .cpu_lat(io_lat), .cur_rd(io_lat), .cur_addr(io_addr), .io_rdata(io_rdata), .cycles(cycles),
        .line_tgl(line_tgl), .line_top(line_top),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .switches(sw_s), .buttons(btn_s), .leds(led), .seg7(seg7), .seg7_ctrl(seg7_ctrl),
        .con_in(9'h100), .con_take(), .con_we(), .exit_we(), .wvalue()
    );

    video vid (
        .clk(clk50), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take),
        .dma_valid(dma_valid), .dma_data(dma_data),
        .line_tgl(line_tgl), .line_top(line_top),
        .hsync(vga_hs), .vsync(vga_vs), .rgb({vga_r, vga_g, vga_b}), .rgb24(), .de()
    );

    // ---- JTAG ----------------------------------------------------------------------
    jtagldr ldr (
        .clk(clk50), .status({1'b0, halted, !fc_rst, 5'd0, halt_code, insts}),
        .run(run), .entry(entry),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_q(ldr_q), .trace_rd(),
        .ldr_addr(ldr_addr), .ldr_data(ldr_data), .ldr_done(ldr_done)
    );

    // ---- seven-segment display: SEG7 as four hex digits ------------------------------
    reg [17:0] refresh = 18'd0;
    always @(posedge clk50) refresh <= refresh + 1'b1;
    wire [1:0] dsel = refresh[17:16];
    wire [6:0] glyph;
    seg7 dec (.digit(seg7[4*dsel +: 4]), .glyph(glyph));
    always @(posedge clk50) begin
        an  <= seg7_ctrl[4 + dsel] ? 4'b1111 : ~(4'b0001 << dsel);
        seg <= {~seg7_ctrl[dsel], ~glyph};
    end
endmodule
