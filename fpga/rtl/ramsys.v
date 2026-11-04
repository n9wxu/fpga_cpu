`timescale 1ns/1ps
// fpga_cpu on the Spartan-3 Starter Board, on the RAM machine: system.v's
// I/O and video, with T1S (T1F, synchronous, its decode and register file in
// block RAM: verilog/rtl/t1s_core.v), with the SRAM behind rammux.v and sram_phy.v instead
// of memsys.v. Programs load over JTAG as before (tools/jtagload.py).
//
// rammux ports, highest priority first:
//   0 video   the line's bitmap and text rows (video.v), bursts of up to 16
//   1 CPU     T1S's data through cpumem.v: single words, a write buffer
//             and the last word read
//   2 fetch   T1S's instructions (t1s_fetch): single words, two out at most.
//             After the data port, so a store reaches the RAM before
//             T1S fetches the code it changed again
//   3 loader  JTAG: single words, while T1S is held in reset
//   4 sound   audio_lite.v's streaming player: a word an output frame
module ramsys (
    input  wire        clk,                 // 50 MHz pin
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
    output wire [3:0]  sram_be_n,
    // I2S to a DAC on the A2 expansion connector (audio_lite.v)
    output wire        i2s_bck, i2s_lrck, i2s_data
);
    parameter PER = 5;                      // 50 MHz cycles in a T1F period, at least

    // ================================================================ clocks
    wire clk50, ccap, pclk;
`ifdef SIM
    assign clk50 = clk;
    assign #5 ccap = clk;
    reg pclk_r = 1'b0;
    always #6.734 pclk_r = ~pclk_r;
    assign pclk = pclk_r;
`else
    wire clk_ibuf, c0, c90, fx1, fx1_buf, fx2, lk1;
    IBUFG ibuf (.I(clk), .O(clk_ibuf));
    // the logic's clock, deskewed, and a quarter later: read capture and the write strobe
    DCM #(.CLKIN_PERIOD(20.0), .CLK_FEEDBACK("1X"), .STARTUP_WAIT("FALSE")) d0 (
        .CLKIN(clk_ibuf), .CLKFB(clk50), .RST(1'b0), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLK0(c0), .CLK90(c90));
    BUFG b0 (.I(c0), .O(clk50));
    BUFG bc (.I(c90), .O(ccap));
    // the pixel clock, 74.25 MHz (clkgen.v's two DCMs, from clk50)
    DCM #(.CLKIN_PERIOD(20.0), .CLKFX_MULTIPLY(27), .CLKFX_DIVIDE(20), .CLK_FEEDBACK("NONE"),
          .DFS_FREQUENCY_MODE("LOW"), .STARTUP_WAIT("FALSE")) dp1 (
        .CLKIN(clk50), .CLKFB(1'b0), .RST(1'b0), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLKFX(fx1), .LOCKED(lk1));
    BUFG bp1 (.I(fx1), .O(fx1_buf));
    reg [15:0] rst2 = 16'hFFFF;
    always @(posedge clk50) rst2 <= lk1 ? {rst2[14:0], 1'b0} : 16'hFFFF;
    DCM #(.CLKIN_PERIOD(14.815), .CLKFX_MULTIPLY(11), .CLKFX_DIVIDE(10), .CLK_FEEDBACK("NONE"),
          .DFS_FREQUENCY_MODE("LOW"), .STARTUP_WAIT("FALSE")) dp2 (
        .CLKIN(fx1_buf), .CLKFB(1'b0), .RST(rst2[15]), .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0),
        .DSSEN(1'b0), .CLKFX(fx2));
    BUFG bpix (.I(fx2), .O(pclk));
`endif

    // ================================================================ the CPU
    wire [4:0]  p_ack, p_rvalid;
    wire [31:0] p_rdata;
    wire        cpu_ce;
    wire [23:0] cpu_addr;
    wire        cpu_rd, cpu_wr;
    wire [7:0]  cpu_dout, cpu_din;
    wire [1:0]  sr_sel;
    reg  [15:0] sr_in;
    wire        halted, retire;
    wire [7:0]  halt_code;
    wire        run;
    wire [23:0] entry;

    // T1S is synchronous: it runs on clk50, one step each time cpumem
    // enables it (cpu_ce: at most one clock in PER), so its paths have PER
    // clocks (ramsys.ucf). Its reset changes only on its enabled edges.
    reg t1_rst = 1'b1;
    always @(posedge clk50) if (cpu_ce) t1_rst <= !run;

    wire        i_req;
    wire [23:2] i_addr;
    t1s_core #(.EXT_X(0)) cpu (
        .clk(clk50), .ce(cpu_ce), .rst(t1_rst), .reset_pc(entry),
        .addr(cpu_addr), .mem_rd(cpu_rd), .mem_wr(cpu_wr), .dout(cpu_dout), .din(cpu_din),
        .i_req(i_req), .i_addr(i_addr), .i_ack(p_ack[2]), .i_valid(p_rvalid[2]), .i_data(p_rdata),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    reg [31:0] insts = 32'd0;              // SR3 and the loader's status read the low 16 bits
    // insts: retire pulses for a clock after a step
    always @(posedge clk50) begin
        if (cpu_ce && t1_rst) insts <= 32'd0;
        else insts <= insts + {31'd0, retire};
    end
`ifdef SIM
    // T1S's steps, for the testbench's statistics only
    reg [31:0] cycles = 32'd0;
    always @(posedge clk50)
        if (cpu_ce) cycles <= t1_rst ? 32'd0 : cycles + 1'b1;
`endif

    // CYCLE (the I/O block) and SR1-SR2: 10 MHz ticks, real time (T1S's
    // steps vary in length here; programs take CYCLE at T1_KHZ = 10000, as
    // on system.v). There is no step counter: its 32 LUTs went to sound.
    reg [31:0] tick10 = 32'd0;
    reg [2:0]  div5 = 3'd0;
    always @(posedge clk50) begin
        div5 <= (div5 == 3'd4) ? 3'd0 : div5 + 3'd1;
        if (div5 == 3'd4) tick10 <= tick10 + 32'd1;
    end
    always @*
        case (sr_sel)
            2'd0: sr_in = tick10[15:0];
            2'd1: sr_in = tick10[31:16];
            default: sr_in = insts[15:0];
        endcase

    // ================================================================ the RAM machine
    localparam NP = 5;
    wire [NP-1:0]    p_wnext;
    wire             phy_cmd, phy_we, phy_ready, phy_rvalid;
    wire [17:0]      phy_addr;
    wire [31:0]      phy_wdata, phy_rdata;
    wire [3:0]       phy_be;

    // port 0: video
    wire        dma_req, dma_valid;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    wire [4:0]  dma_len = (dma_left >= 9'd16) ? 5'd16 : dma_left[4:0];
    wire [4:0]  dma_take = p_ack[0] ? dma_len : 5'd0;
    assign dma_valid = p_rvalid[0];

    // port 1: the CPU
    wire        c_req, c_we;
    wire [17:0] c_addr;
    wire [31:0] c_wdata;
    wire [3:0]  c_wbe;
    wire        io_wr, io_rd, cpu_lat;
    wire [15:0] io_addr;
    wire [7:0]  io_wdata, io_rdata;
    wire        s_period, s_miss, s_wstall;
    cpumem #(.PER(PER), .LAT(2)) mem (
        .clk(clk50), .rst(!run), .cpu_ce(cpu_ce),
        .cpu_addr(cpu_addr), .cpu_rd(cpu_rd), .cpu_wr(cpu_wr), .cpu_dout(cpu_dout), .cpu_din(cpu_din),
        .io_wr(io_wr), .io_rd(io_rd), .io_addr(io_addr), .io_wdata(io_wdata), .cpu_lat(cpu_lat),
        .io_rdata(io_rdata),
        .p_req(c_req), .p_we(c_we), .p_addr(c_addr), .p_wdata(c_wdata), .p_wbe(c_wbe),
        .p_ack(p_ack[1]), .p_rvalid(p_rvalid[1]), .p_rdata(p_rdata),
        .s_period(s_period), .s_miss(s_miss), .s_wstall(s_wstall));

    // port 3: the JTAG loader (one word at a time)
    wire        ldr_req, ldr_rd;
    wire [17:0] ldr_addr;
    wire [31:0] ldr_data;
    reg         ldr_done = 1'b0, l_out = 1'b0;
    reg  [31:0] ldr_q = 32'd0;
    wire        l_req = ldr_req && !l_out && !ldr_done;
    always @(posedge clk50) begin
        ldr_done <= 1'b0;
        if (p_ack[3]) begin
            if (ldr_rd) l_out <= 1'b1;
            else ldr_done <= 1'b1;
        end
        if (p_rvalid[3]) begin l_out <= 1'b0; ldr_q <= p_rdata; ldr_done <= 1'b1; end
    end

    // port 4: sound (reads only)
    wire        a_req;
    wire [17:0] a_addr;

    rammux #(.NP(NP), .AW(18), .LW(5), .WMASK(5'b01010)) mux (
        .clk(clk50),
        .p_req({a_req, l_req, i_req, c_req, dma_req}), .p_we({1'b0, !ldr_rd, 1'b0, c_we, 1'b0}),
        .p_addr({a_addr, ldr_addr, i_addr[19:2], c_addr, dma_addr}), .p_len({5'd1, 5'd1, 5'd1, 5'd1, dma_len}), .p_ack(p_ack),
        .p_wdata({32'd0, ldr_data, 32'd0, c_wdata, 32'd0}), .p_wbe({4'h0, 4'hF, 4'h0, c_wbe, 4'h0}), .p_wnext(p_wnext),
        .p_rvalid(p_rvalid), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));
    sram_phy phy (
        .clk(clk50), .clk_cap(ccap), .clk_we(ccap), .wmode(1'b0),
        .cmd(phy_cmd), .we(phy_we), .addr(phy_addr), .wdata(phy_wdata), .be(phy_be),
        .ready(phy_ready), .rvalid(phy_rvalid), .rdata(phy_rdata),
        .sram_a(sram_a), .sram_d(sram_d), .sram_oe_n(sram_oe_n), .sram_we_n(sram_we_n),
        .sram_ce_n(sram_ce_n), .sram_be_n(sram_be_n));

    // ================================================================ I/O and video
    wire [15:0] vid_ctrl, vid_scrollx, vid_scrolly, vid_cursor, pal_wdata, seg7;
    wire [2:0]  vid_backdrop;
    wire [31:0] vid_text, vid_bmp;
    wire        pal_we, line_tgl, line_top;
    wire [7:0]  pal_windex, seg7_ctrl;
    reg [3:0] btn_s0 = 0, btn_s = 0;
    reg [7:0] sw_s0 = 0, sw_s = 0;
    always @(posedge clk50) begin
        btn_s0 <= btn; btn_s <= btn_s0;
        sw_s0  <= sw;  sw_s  <= sw_s0;
    end

    wire [15:0] aud_rd;
    audio_lite #(.AW(18)) aud (
        .clk(clk50), .io_wr(io_wr), .io_addr(io_addr), .io_wdata(io_wdata),
        .rd_addr(cpu_addr[15:0]), .rd_data(aud_rd),
        .p_req(a_req), .p_addr(a_addr), .p_ack(p_ack[4]), .p_rvalid(p_rvalid[4]), .p_rdata(p_rdata),
        .i2s_bck(i2s_bck), .i2s_lrck(i2s_lrck), .i2s_data(i2s_data));

    iosys #(.HICOL8(0), .AUDIO(1)) io (
        .clk(clk50), .io_wr(io_wr), .io_rd(io_rd), .io_addr(io_addr), .io_wdata(io_wdata),
        .cpu_lat(cpu_lat), .cur_rd(cpu_rd && cpu_addr[23:16] == 8'hF0),
        .cur_addr(cpu_addr[15:0]), .io_rdata(io_rdata), .cycles(tick10), .insts(insts),
        .line_tgl(line_tgl), .line_top(line_top),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .switches(sw_s), .buttons(btn_s), .leds(led), .seg7(seg7), .seg7_ctrl(seg7_ctrl),
        .con_in(9'h100), .con_take(), .con_we(), .exit_we(), .wvalue(), .aud_rd(aud_rd)
    );

    video #(.HICOL8(0)) vid (                 // the Spartan-3: 16 colours at most (docs/io.md)
        .clk(clk50), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take),
        .dma_valid(dma_valid), .dma_data(p_rdata),
        .line_tgl(line_tgl), .line_top(line_top),
        .hsync(vga_hs), .vsync(vga_vs), .rgb({vga_r, vga_g, vga_b})
    );

    // ================================================================ JTAG
    jtagldr ldr (
        .clk(clk50), .status({1'b0, halted, !t1_rst, 5'd0, halt_code, insts[15:0]}),
        .run(run), .entry(entry),
        .ldr_req(ldr_req), .ldr_rd(ldr_rd), .ldr_q(ldr_q), .trace_rd(),
        .ldr_addr(ldr_addr), .ldr_data(ldr_data), .ldr_done(ldr_done)
    );

    // ================================================================ seven-segment: SEG7
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
