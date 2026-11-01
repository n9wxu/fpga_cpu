`timescale 1ns/1ps
// fpga_cpu on the Spartan-3 Starter Board: the T1 core
// (verilog/rtl/t1_core.v), 1 MB of SRAM, the video of docs/io.md at
// 1280x720, the I/O registers, and program loading over JTAG.
//
// T1's bus outputs settle ~40 ns after its clock edge and read data needs
// ~24 ns more to get through it, so with the SRAM's 15 ns (pins included)
// in between it runs one clock per five 50 MHz cycles (10 MHz), on a gated
// global clock; see memsys.v for how the SRAM is shared with the video. Programs are linked for 0x1000
// (fcc's default) and loaded with fpga/tools/jtagload.py.
module system (
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

    // ---- the CPU -----------------------------------------------------------------
    wire        cpu_ce, cpu_clk;
    wire [23:0] cpu_addr;
    wire        cpu_rd, cpu_wr;
    wire [7:0]  cpu_dout, cpu_din;
    wire [1:0]  sr_sel;
    reg  [15:0] sr_in;
    wire        halted, retire;
    wire [7:0]  halt_code;
    wire        run;
    wire [23:0] entry;

    // T1's reset is a register on T1's own clock. (As a 50 MHz register
    // changing at T1's edge, T1's later-arriving gated clock could take the
    // new value at that edge and leave reset with no fetch made: its bus
    // outputs follow rst, and memsys registers them two cycles later.)
    reg t1_rst = 1'b1;
    always @(posedge cpu_clk) t1_rst <= !run;

`ifdef SIM
    clkgate gate (.clk(clk), .ce(cpu_ce), .gclk(cpu_clk));      // see sim/clkgen_sim.v
`else
    clkgate gate (.clk(clk50), .ce(cpu_ce), .gclk(cpu_clk));
`endif

    wire        cpu_lat;

    t1_core #(.EXT_X(0)) cpu (     // extension X off: fcc uses it only with --ext
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(entry),
        .addr(cpu_addr), .mem_rd(cpu_rd), .mem_wr(cpu_wr), .dout(cpu_dout), .din(cpu_din),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    // SR1-SR3 and CYCLE_LO/HI: T1 clocks and instructions since the start,
    // counted on T1's clock (the I/O block reads CYCLE mid-period)
    reg [31:0] cycles = 32'd0;
    reg [15:0] insts = 16'd0;              // only the low 16 bits are read (SR3)
    always @(posedge cpu_clk)
        if (t1_rst) begin
            cycles <= 32'd0;
            insts  <= 16'd0;
        end else begin
            cycles <= cycles + 1'b1;
            insts  <= insts + retire;
        end
    always @*
        case (sr_sel)
            2'd0: sr_in = cycles[15:0];
            2'd1: sr_in = cycles[31:16];
            default: sr_in = insts;
        endcase

    // ---- memory ------------------------------------------------------------------
    wire        io_wr, io_rd;
    wire [15:0] io_addr;
    wire [7:0]  io_wdata, io_rdata;
    wire        dma_req, dma_valid;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    wire [2:0]  dma_take;
    wire [31:0] dma_data;
    wire        ldr_req, ldr_rd, ldr_done;
    wire [31:0] ldr_q;
    wire [17:0] ldr_addr;
    wire [31:0] ldr_data;

    memsys mem (
        .clk(clk50), .cpu_ce(cpu_ce),
        .cpu_addr(cpu_addr), .cpu_rd(cpu_rd), .cpu_wr(cpu_wr), .cpu_dout(cpu_dout), .cpu_din(cpu_din),
        .io_wr(io_wr), .io_rd(io_rd), .io_addr(io_addr), .io_wdata(io_wdata),
        .cpu_lat(cpu_lat), .io_rdata(io_rdata),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take),
        .dma_valid(dma_valid), .dma_data(dma_data),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_addr(ldr_addr), .ldr_data(ldr_data),
        .ldr_done(ldr_done), .ldr_q(ldr_q),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n)
    );

    // ---- I/O -----------------------------------------------------------------------
    wire [15:0] vid_ctrl, vid_scrollx, vid_scrolly, vid_cursor, pal_wdata, seg7;
    wire [2:0]  vid_backdrop;
    wire [31:0] vid_text, vid_bmp;
    wire        pal_we, line_tgl, line_top;
    wire [7:0]  pal_windex, seg7_ctrl;

    // buttons and switches: two-stage synchronizers
    reg [3:0] btn_s0 = 0, btn_s = 0;
    reg [7:0] sw_s0 = 0, sw_s = 0;
    always @(posedge clk50) begin
        btn_s0 <= btn; btn_s <= btn_s0;
        sw_s0  <= sw;  sw_s  <= sw_s0;
    end

    iosys io (
        .clk(clk50), .io_wr(io_wr), .io_rd(io_rd), .io_addr(io_addr), .io_wdata(io_wdata),
        .cpu_lat(cpu_lat), .cur_rd(cpu_rd && cpu_addr[23:16] == 8'hF0),
        .cur_addr(cpu_addr[15:0]), .io_rdata(io_rdata), .cycles(cycles),
        .line_tgl(line_tgl), .line_top(line_top),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .switches(sw_s), .buttons(btn_s), .leds(led), .seg7(seg7), .seg7_ctrl(seg7_ctrl),
        .con_we(), .exit_we(), .wvalue()
    );

    video vid (
        .clk(clk50), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take),
        .dma_valid(dma_valid), .dma_data(dma_data),
        .line_tgl(line_tgl), .line_top(line_top),
        .hsync(vga_hs), .vsync(vga_vs), .rgb({vga_r, vga_g, vga_b})
    );

    // ---- JTAG ----------------------------------------------------------------------
    jtagldr ldr (
        .clk(clk50), .status({1'b0, halted, !t1_rst, 5'd0, halt_code, insts}),
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
