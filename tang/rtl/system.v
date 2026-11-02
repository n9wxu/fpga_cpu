`timescale 1ns/1ps
// fpga_cpu on the Tang Nano 20K: the T1 core (verilog/rtl/t1_core.v), 64 KB
// of block RAM and the 8 MB SDRAM, the video of docs/io.md on HDMI
// (fpga/rtl/video.v, as on the Spartan-3, in true colour), the I/O
// registers (fpga/rtl/iosys.v), and the console and program loading on the
// USB serial port (the onboard BL616's second channel).
//
// Clocks. A PLL makes fclk, 67.5 MHz, for everything but the video, and the
// SDRAM's clock, the same shifted 180 degrees (sdram.v). T1's clock is fclk
// through a DQCE clock gate: a T1 cycle is a period of fclk cycles ending
// with the one edge the gate lets through. T1's bus settles in under two
// fclk cycles, so it is taken at the second edge of the period (lat):
//   - block RAM (000000-00FFFF) and I/O: read or written at that edge, and
//     T1's edge two cycles later: 4 cycles, 16.875 MHz;
//   - the SDRAM (the rest, repeating every 8 MB): a write is done when its
//     command goes out, a read when its data is captured, and T1's edge
//     follows: 4 cycles for a write to an open row, 7 for a read.
// Another PLL makes the 720p pixel clock (dvi_tx.v). Video buffers (text,
// bitmap) must be in the SDRAM: video DMA reads only that.
//
// CYCLE (and SR0-SR1) count time in T1 periods (fclk / 4, 16.875 MHz), so
// stalls show as clocks per instruction.
//
// Serial (115200 8N1). While T1 is held in reset:
//   'L' a2 a1 a0 n2 n1 n0 <n bytes>   write n bytes from address a
//   'G' a2 a1 a0                      run T1 from address a
// At any time the four bytes DE AD BE EF hold T1 in reset again. While T1
// runs, bytes received are CON_IN, CON_OUT bytes are sent, and an EXIT
// write sends 0x04 and the low byte of the code (tools: tang/tools/run.py).
`ifndef SD_PSDA
`define SD_PSDA "1000"                     // the SDRAM's clock at 180 degrees
`endif
module system #(
`ifdef SIM
    parameter FONT_FILE = "font8x14.mem"    // the simulation runs in build/
`else
    parameter FONT_FILE = "build/font8x14.mem"
`endif
) (
    input  wire        clk,                 // 27 MHz (in simulation: fclk)
    input  wire        s1,                  // buttons, high when pressed
    input  wire        s2,
    input  wire        uart_rx,
    output wire        uart_tx,
    output wire [5:0]  led,                 // active low
    output wire [3:0]  tmds_p,              // HDMI: {clock, red, green, blue}
    output wire [3:0]  tmds_n,
    // the SDRAM in the package (the names place the pins)
    output wire        O_sdram_clk,
    output wire        O_sdram_cke,
    output wire        O_sdram_cs_n,
    output wire        O_sdram_ras_n,
    output wire        O_sdram_cas_n,
    output wire        O_sdram_wen_n,
    output wire [10:0] O_sdram_addr,
    output wire [1:0]  O_sdram_ba,
    output wire [3:0]  O_sdram_dqm,
    inout  wire [31:0] IO_sdram_dq
);
    // ---- clocks ------------------------------------------------------------------
    wire fclk, sdclk;
`ifdef SIM
    assign fclk = clk;
    assign sdclk = ~clk;
`else
    // 27 MHz x 5/2 = 67.5 MHz: PFD 13.5 MHz, VCO 67.5 x 8 = 540 MHz
    rPLL #(
        .FCLKIN("27"),
        .IDIV_SEL(1),
        .FBDIV_SEL(4),
        .ODIV_SEL(8),
        .PSDA_SEL(`SD_PSDA),                // CLKOUTP: the SDRAM's clock phase, n x 22.5 degrees
        .DUTYDA_SEL("1000"),
        .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk), .CLKOUT(fclk), .CLKOUTP(sdclk), .CLKOUTD(), .CLKOUTD3(), .LOCK(),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
`endif
    assign O_sdram_clk = sdclk;
    localparam CLK_HZ = 67500000;

    wire sclk, pclk, plocked;
    dvi_clocks vclk (.clk27(clk), .sclk(sclk), .pclk(pclk), .locked(plocked));

    // T1's clock: fclk where cpu_ce was high in the cycle before
    reg  cpu_ce = 1'b0;
    wire cpu_clk;
`ifdef SIM
    reg ce_l = 1'b0;
    always @(negedge fclk) ce_l <= cpu_ce;
    assign cpu_clk = fclk & ce_l;
`else
    DQCE gate (.CLKIN(fclk), .CE(cpu_ce), .CLKOUT(cpu_clk));
`endif

    // ---- T1 -------------------------------------------------------------------
    wire [23:0] addr;
    wire        rd, wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    reg  [7:0]  din;
    reg  [15:0] sr_in;
    reg         run = 1'b0;
    reg  [23:0] entry = 24'h001000;
    reg         t1_rst = 1'b1;
    always @(posedge cpu_clk) t1_rst <= !run;

`ifdef T1F
    t1f_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .ce(1'b1), .rst(t1_rst), .reset_pc(entry),
`else
    t1_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(entry),
`endif
        .addr(addr), .mem_rd(rd), .mem_wr(wr), .dout(dout), .din(din),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    reg [15:0] insts = 16'd0;
    always @(posedge cpu_clk)
        if (t1_rst) insts <= 16'd0;
        else        insts <= insts + retire;

    // time in T1 periods; what T1 reads (SR0/SR1) changes only mid-period
    reg [1:0]  tb = 2'd0;
    reg [31:0] cycles = 32'd0, cycles_t1 = 32'd0;
    always @(posedge fclk) begin
        tb <= tb + 1'b1;
        if (!run) cycles <= 32'd0;
        else if (tb == 2'd3) cycles <= cycles + 1'b1;
    end
    always @*
        case (sr_sel)
            2'd0: sr_in = cycles_t1[15:0];
            2'd1: sr_in = cycles_t1[31:16];
            default: sr_in = insts;
        endcase

    // ---- the period: T1's bus at lat, then its edge when the access is done ---------
`ifdef T1F
    localparam PER = 2;                     // fclk cycles in a T1F period (BRAM, I/O)
`else
    localparam PER = 4;
`endif
    reg  [3:0]  ph = 4'd0;                  // fclk cycles since T1's edge
    wire        lat = ph == (PER == 2 ? 4'd0 : 4'd1);   // the bus has settled (T1F 11.6 ns, T1 ~21 ns)
    wire        io  = addr[23:16] == 8'hF0;
    wire        low = addr[23:16] == 8'h00;
    reg  [1:0]  src = 2'd0;                 // din from: 0 block RAM, 1 I/O, 2 SDRAM
    reg         t1_wait = 1'b0;             // T1's SDRAM access under way

    // the loader (below) writes when T1 is held
    reg         ld_we = 1'b0;
    reg  [23:0] ld_a = 24'd0;
    reg  [7:0]  ld_d = 8'd0;
    wire        ld_low = ld_a[23:16] == 8'h00;

    // SDRAM requests, from T1 or the loader
    reg         c_req = 1'b0, c_we = 1'b0;
    reg  [22:0] c_addr = 23'd0;
    reg  [7:0]  c_wd = 8'd0;
    wire        c_wdone, c_rsoon, sd_ready;
    wire [7:0]  c_q;

    // T1F: at lat (one fclk cycle after T1F's edge) the address alone says
    // whether this is block RAM or I/O (served now, T1F's edge a cycle
    // later) or the SDRAM, whose request is taken a cycle later still, when
    // rd/wr (from T1F's decode) have had two cycles. T1 has four-cycle
    // periods and takes everything at lat.
    reg         sd_q = 1'b0, io_q2 = 1'b0;  // the SDRAM region, I/O, at lat
    wire        sd_now = (PER == 2) ? (ph == 4'd1 && sd_q) : (lat && !io && !low);
    wire        io_lat = (PER == 2) ? (ph == 4'd1 && io_q2) : lat;   // I/O registers: their address decode gets two cycles
    always @(posedge fclk) begin
        ph     <= cpu_ce ? 4'd0 : (ph == 4'd15 ? ph : ph + 1'b1);
        cpu_ce <= 1'b0;
        if (lat) begin
            cycles_t1 <= cycles;            // SR0/SR1 hold still from here to T1's edge
            src  <= io ? 2'd1 : low ? 2'd0 : 2'd2;
            sd_q <= !io && !low;
            io_q2 <= io;
        end
        if (sd_now && run) begin
            if (run && (rd || wr)) begin
                c_req   <= 1'b1;
                c_we    <= wr;
                c_addr  <= addr[22:0];
                c_wd    <= dout;
                t1_wait <= 1'b1;
            end else if (PER == 2)
                cpu_ce <= 1'b1;             // no access after all
        end
        // (held in reset, T1 is clocked whatever its bus says)
        if (ph == PER - 2 && !t1_wait && (!run || PER != 2 || low)) cpu_ce <= 1'b1;
        if (PER == 2 && ph == 4'd1 && io_q2 && run) cpu_ce <= 1'b1;   // I/O done at io_lat
        if (c_req && (c_wdone || c_rsoon)) begin
            c_req <= 1'b0;
            if (t1_wait) begin t1_wait <= 1'b0; cpu_ce <= 1'b1; end
        end
        if (!run && ld_we && !ld_low) begin
            c_req <= 1'b1; c_we <= 1'b1; c_addr <= ld_a[22:0]; c_wd <= ld_d;
        end
    end

    // ---- block RAM: 64 KB at 000000 ----------------------------------------------------
    reg  [7:0]  mem [0:65535];
    reg  [7:0]  mem_q = 8'd0;
    wire        m_we = run ? (lat && wr && low) : (ld_we && ld_low);
    wire [15:0] m_a  = run ? addr[15:0] : ld_a[15:0];
    wire [7:0]  m_d  = run ? dout : ld_d;
    always @(posedge fclk)
        if (m_we) mem[m_a] <= m_d;
        else if (lat) mem_q <= mem[m_a];

    // ---- SDRAM ----------------------------------------------------------------------------
    wire        dma_take;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    wire        dma_valid;
    wire [31:0] dma_data;
    sdram #(
`ifdef SIM
        .INIT_WAIT(100)
`else
        .INIT_WAIT(15000)
`endif
    ) sd (
        .clk(fclk),
        .c_req(c_req), .c_we(c_we), .c_addr(c_addr), .c_wd(c_wd),
        .c_wdone(c_wdone), .c_rsoon(c_rsoon), .c_q(c_q),
        .d_left(dma_left), .d_addr({3'b000, dma_addr}), .d_take(dma_take),
        .d_valid(dma_valid), .d_data(dma_data),
        .sd_cke(O_sdram_cke), .sd_cs_n(O_sdram_cs_n), .sd_ras_n(O_sdram_ras_n),
        .sd_cas_n(O_sdram_cas_n), .sd_we_n(O_sdram_wen_n), .sd_a(O_sdram_addr),
        .sd_ba(O_sdram_ba), .sd_dqm(O_sdram_dqm), .sd_dq(IO_sdram_dq), .ready(sd_ready)
    );

    // ---- I/O ------------------------------------------------------------------------
    reg  [1:0] s_s0 = 2'b00, s_s = 2'b00;
    always @(posedge fclk) begin s_s0 <= {s2, s1}; s_s <= s_s0; end

    wire [15:0] vid_ctrl, vid_scrollx, vid_scrolly, vid_cursor, pal_wdata, wvalue;
    wire [31:0] vid_text, vid_bmp;
    wire [2:0]  vid_backdrop;
    wire [7:0]  pal_windex, io_q, leds;
    wire        pal_we, line_tgl, line_top, con_take, con_we, exit_we;
    reg  [8:0]  con_in = 9'h1FF;            // bit 8: empty

    always @*
        case (src)
            2'd0: din = mem_q;
            2'd1: din = io_q;
            default: din = c_q;
        endcase

    iosys ios (
        .clk(fclk), .io_wr(io_lat && run && io && wr), .io_rd(io_lat && run && io && rd),
        .io_addr(addr[15:0]), .io_wdata(dout), .cpu_lat(io_lat), .cur_rd(run && io && rd),
        .cur_addr(addr[15:0]), .io_rdata(io_q), .cycles(cycles),
        .line_tgl(line_tgl), .line_top(line_top),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .switches(8'd0), .buttons({2'b00, s_s}), .leds(leds), .seg7(), .seg7_ctrl(),
        .con_in(con_in), .con_take(con_take), .con_we(con_we), .exit_we(exit_we), .wvalue(wvalue)
    );

    // ---- video ------------------------------------------------------------------------
    wire [23:0] rgb24;
    wire        hs, vs, de;
    video #(.FONT_FILE(FONT_FILE), .TRUE_COLOR(1)) vid (
        .clk(fclk), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take({2'b00, dma_take}),
        .dma_valid(dma_valid), .dma_data(dma_data),
        .line_tgl(line_tgl), .line_top(line_top),
        .hsync(hs), .vsync(vs), .rgb(), .rgb24(rgb24), .de(de)
    );
    dvi_tx #(.HDMI(1)) tx (.pclk(pclk), .sclk(sclk), .rst(!plocked), .de(de), .hsync(hs), .vsync(vs),
               .r(rgb24[23:16]), .g(rgb24[15:8]), .b(rgb24[7:0]), .tmds_p(tmds_p), .tmds_n(tmds_n));

    // ---- UART -------------------------------------------------------------------------
`ifdef SIM
    localparam BAUD_DIV = 4;                // fast serial in simulation
`else
    localparam BAUD_DIV = CLK_HZ / 115200;  // 585 (+0.1%)
`endif
    wire       rx_ok;
    wire [7:0] rx_byte;
    reg        tx_go = 1'b0;
    reg  [7:0] tx_byte = 8'd0;
    wire       tx_busy;
    uart #(.DIV(BAUD_DIV)) serial (
        .clk(fclk), .rx(uart_rx), .rx_ok(rx_ok), .rx_byte(rx_byte),
        .tx_go(tx_go), .tx_byte(tx_byte), .tx_busy(tx_busy), .tx(uart_tx)
    );

    // console output: a 256-byte FIFO in front of the transmitter
    reg [7:0] txq [0:255];
    reg [7:0] txq_w = 8'd0, txq_r = 8'd0;
    wire      txq_empty = txq_w == txq_r;
    wire      txq_full  = (txq_w + 1'b1) == txq_r;
    reg       txq_we = 1'b0;
    reg [7:0] txq_d = 8'd0;
    always @(posedge fclk) begin
        tx_go <= 1'b0;
        if (txq_we && !txq_full) begin
            txq[txq_w] <= txq_d;
            txq_w <= txq_w + 1'b1;
        end
        if (!tx_busy && !tx_go && !txq_empty) begin
            tx_byte <= txq[txq_r];
            tx_go   <= 1'b1;
            txq_r   <= txq_r + 1'b1;
        end
    end

    // ---- console and loader ------------------------------------------------------------
    localparam L_IDLE = 3'd0, L_ADDR = 3'd1, L_LEN = 3'd2, L_DATA = 3'd3, L_GO = 3'd4;
    reg [2:0]  ls = L_IDLE;
    reg [1:0]  lk = 2'd0;
    reg [23:0] lv = 24'd0;
    reg [23:0] ln = 24'd0;
    reg        lgo = 1'b0;
    reg [7:0]  code = 8'd0;
    reg [31:0] sync = 32'd0;

    always @(posedge fclk) begin
        txq_we <= 1'b0;
        ld_we  <= 1'b0;

        // CON_OUT, and EXIT: 0x04 then the code
        if (con_we) begin txq_we <= 1'b1; txq_d <= wvalue[7:0]; end
        if (exit_we) begin txq_we <= 1'b1; txq_d <= 8'h04; lgo <= 1'b1; code <= wvalue[7:0]; end
        if (lgo && !txq_we && !exit_we) begin
            txq_we <= 1'b1; txq_d <= code; lgo <= 1'b0;
        end

        if (con_take) con_in[8] <= 1'b1;   // a read takes it

        // serial input: the sync word, the loader, or CON_IN
        if (rx_ok) begin
            sync <= {sync[23:0], rx_byte};
            if ({sync[23:0], rx_byte} == 32'hDEADBEEF) begin
                run <= 1'b0; ls <= L_IDLE;
            end else if (run)
                con_in <= {1'b0, rx_byte};
            else case (ls)
                L_IDLE: begin
                    lk <= 2'd0;
                    if (rx_byte == "L") ls <= L_ADDR;
                    if (rx_byte == "G") ls <= L_GO;
                end
                L_ADDR, L_GO: begin
                    lv <= {lv[15:0], rx_byte};
                    lk <= lk + 1'b1;
                    if (lk == 2'd2) begin
                        if (ls == L_GO) begin
                            entry <= {lv[15:0], rx_byte};
                            run   <= 1'b1;
                            ls    <= L_IDLE;
                        end else begin
                            ld_a <= {lv[15:0], rx_byte} - 1'b1;   // pre-decremented: each byte advances it
                            lk   <= 2'd0;
                            ls   <= L_LEN;
                        end
                    end
                end
                L_LEN: begin
                    ln <= {ln[15:0], rx_byte};
                    lk <= lk + 1'b1;
                    if (lk == 2'd2) ls <= ({ln[15:0], rx_byte} == 0) ? L_IDLE : L_DATA;
                end
                L_DATA: begin
                    ld_a  <= ld_a + 1'b1;
                    ld_d  <= rx_byte;
                    ld_we <= 1'b1;
                    ln    <= ln - 1'b1;
                    if (ln == 24'd1) ls <= L_IDLE;
                end
                default: ls <= L_IDLE;
            endcase
        end
    end

    // LEDs: T1's LEDS register; LED5 shows T1 halted, LED4 held in reset
    assign led = ~{halted, !run, leds[3:0]};
endmodule


// 8N1 UART, DIV clocks a bit.
module uart #(
    parameter DIV = 58
) (
    input  wire       clk,
    input  wire       rx,
    output reg        rx_ok = 1'b0,
    output reg  [7:0] rx_byte = 8'd0,
    input  wire       tx_go,
    input  wire [7:0] tx_byte,
    output wire       tx_busy,
    output reg        tx = 1'b1
);
    // receive: sample mid-bit
    reg [1:0]  rs = 2'b11;
    reg [15:0] rc = 16'd0;
    reg [3:0]  rb = 4'd0;
    reg        rbusy = 1'b0;
    reg [7:0]  rsh = 8'd0;
    always @(posedge clk) begin
        rs <= {rs[0], rx};
        rx_ok <= 1'b0;
        if (!rbusy) begin
            if (!rs[1]) begin rbusy <= 1'b1; rc <= DIV / 2; rb <= 4'd0; end
        end else if (rc != 0)
            rc <= rc - 1'b1;
        else begin
            rc <= DIV - 1;
            if (rb == 4'd0) begin
                if (rs[1]) rbusy <= 1'b0;   // false start
                rb <= 4'd1;
            end else if (rb <= 4'd8) begin
                rsh <= {rs[1], rsh[7:1]};
                rb  <= rb + 1'b1;
            end else begin
                rbusy <= 1'b0;
                if (rs[1]) begin rx_byte <= rsh; rx_ok <= 1'b1; end
            end
        end
    end

    // transmit
    reg [15:0] tc = 16'd0;
    reg [3:0]  tb = 4'd0;
    reg [9:0]  tsh = 10'h3FF;
    assign tx_busy = tb != 0;
    always @(posedge clk)
        if (tb == 0) begin
            if (tx_go) begin tsh <= {1'b1, tx_byte, 1'b0}; tb <= 4'd10; tc <= DIV - 1; tx <= 1'b0; end
        end else if (tc != 0)
            tc <= tc - 1'b1;
        else begin
            tc  <= DIV - 1;
            tsh <= {1'b1, tsh[9:1]};
            tx  <= tsh[1];
            tb  <= tb - 1'b1;
        end
endmodule
