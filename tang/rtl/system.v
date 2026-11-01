`timescale 1ns/1ps
// fpga_cpu on the Tang Nano 20K: the T1 core (verilog/rtl/t1_core.v), 64 KB
// of block RAM, the video of docs/io.md on HDMI (fpga/rtl/video.v, as on the
// Spartan-3, in true colour), the I/O registers (fpga/rtl/iosys.v), and the
// console and program loading on the USB serial port (the onboard BL616's
// second channel). Not yet: the SDRAM, so the bitmap has only the 64 KB.
//
// Clocks. One PLL makes 16.875 MHz from the 27 MHz crystal, twice: cpu_clk
// for T1, and mem_clk, the same clock shifted 180 degrees, for memory, I/O,
// the UART and video DMA. So those run half a period (29.6 ns) after T1's
// edge, by when T1's bus has settled, and read data is back long before
// T1's next edge. Another PLL makes the 720p pixel clock (dvi_tx.v).
//
// Memory is four byte lanes of 16 KB, each a dual-port block RAM: T1 (or
// the loader) on one port, video DMA reading 32-bit words on the other, so
// the two never wait for each other. It repeats through the 1 MB below the
// I/O, for T1 and for video addresses alike.
//
// Serial (115200 8N1). While T1 is held in reset:
//   'L' a2 a1 a0 n2 n1 n0 <n bytes>   write n bytes from address a
//   'G' a2 a1 a0                      run T1 from address a
// At any time the four bytes DE AD BE EF hold T1 in reset again. While T1
// runs, bytes received are CON_IN, CON_OUT bytes are sent, and an EXIT
// write sends 0x04 and the low byte of the code (tools: tang/tools/run.py).
module system #(
`ifdef SIM
    parameter FONT_FILE = "font8x14.mem"    // the simulation runs in build/
`else
    parameter FONT_FILE = "build/font8x14.mem"
`endif
) (
    input  wire       clk,                  // 27 MHz
    input  wire       s1,                   // buttons, high when pressed
    input  wire       s2,
    input  wire       uart_rx,
    output wire       uart_tx,
    output wire [5:0] led,                  // active low
    output wire [3:0] tmds_p,               // HDMI: {clock, red, green, blue}
    output wire [3:0] tmds_n
);
    // ---- clocks ------------------------------------------------------------------
    wire cpu_clk, mem_clk;
`ifdef SIM
    assign cpu_clk = clk;                   // the testbench drives the clock
    assign mem_clk = ~clk;
`else
    // 27 MHz x 5/8 = 16.875 MHz: PFD 3.375 MHz, VCO 16.875 x 64 = 1080 MHz
    rPLL #(
        .FCLKIN("27"),
        .IDIV_SEL(7),
        .FBDIV_SEL(4),
        .ODIV_SEL(64),
        .PSDA_SEL("1000"),                  // CLKOUTP: 8 x 22.5 = 180 degrees
        .DUTYDA_SEL("1000"),
        .DEVICE("GW2AR-18C")
    ) pll (
        .CLKIN(clk), .CLKOUT(cpu_clk), .CLKOUTP(mem_clk), .CLKOUTD(), .CLKOUTD3(), .LOCK(),
        .CLKFB(1'b0), .FBDSEL(6'd0), .IDSEL(6'd0), .ODSEL(6'd0),
        .DUTYDA(4'd0), .PSDA(4'd0), .FDLY(4'd0), .RESET(1'b0), .RESET_P(1'b0)
    );
`endif
    localparam CLK_HZ = 16875000;

    wire sclk, pclk, plocked;
    dvi_clocks vclk (.clk27(clk), .sclk(sclk), .pclk(pclk), .locked(plocked));

    // ---- T1 -------------------------------------------------------------------
    wire [23:0] addr;
    wire        rd, wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    wire [7:0]  din;
    reg  [15:0] sr_in;
    reg         run = 1'b0;
    reg  [23:0] entry = 24'h001000;
    reg         t1_rst = 1'b1;
    always @(posedge cpu_clk) t1_rst <= !run;

    t1_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(entry),
        .addr(addr), .mem_rd(rd), .mem_wr(wr), .dout(dout), .din(din),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    reg [31:0] cycles = 32'd0;
    reg [15:0] insts = 16'd0;
    always @(posedge cpu_clk)
        if (t1_rst) begin
            cycles <= 32'd0; insts <= 16'd0;
        end else begin
            cycles <= cycles + 1'b1; insts <= insts + retire;
        end
    always @*
        case (sr_sel)
            2'd0: sr_in = cycles[15:0];
            2'd1: sr_in = cycles[31:16];
            default: sr_in = insts;
        endcase

    // ---- memory -------------------------------------------------------------------
    wire        io = addr[23:16] == 8'hF0;
    reg         ld_we = 1'b0;               // the loader writes when T1 is held
    reg  [15:0] ld_a = 16'd0;
    reg  [7:0]  ld_d = 8'd0;
    wire        m_we = run ? (wr && !io) : ld_we;
    wire [15:0] m_a  = run ? addr[15:0] : ld_a;
    wire [7:0]  m_d  = run ? dout : ld_d;

    wire        dma_req;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    reg         dma_valid = 1'b0;
    wire [31:0] dma_data;
    wire [7:0]  lane_q [0:3];
    reg  [1:0]  m_lane = 2'd0;

    genvar l;
    generate
        for (l = 0; l < 4; l = l + 1) begin : lane
            wire [7:0] qa, qb;
            ram16k8 ram (
                .clk(mem_clk),
                .a_addr(m_a[15:2]), .a_we(m_we && m_a[1:0] == l), .a_d(m_d), .a_q(qa),
                .b_addr(dma_addr[13:0]), .b_q(qb)
            );
            assign lane_q[l] = qa;
            assign dma_data[8*l +: 8] = qb;
        end
    endgenerate
    always @(posedge mem_clk) begin
        m_lane    <= m_a[1:0];
        dma_valid <= dma_req;               // a word a clock while video asks
    end
    wire [7:0] mem_q = lane_q[m_lane];

    // ---- I/O ------------------------------------------------------------------------
    reg  [1:0] s_s0 = 2'b00, s_s = 2'b00;
    always @(posedge mem_clk) begin s_s0 <= {s2, s1}; s_s <= s_s0; end

    wire [15:0] vid_ctrl, vid_scrollx, vid_scrolly, vid_cursor, pal_wdata, wvalue;
    wire [31:0] vid_text, vid_bmp;
    wire [2:0]  vid_backdrop;
    wire [7:0]  pal_windex, io_q, leds;
    wire        pal_we, line_tgl, line_top, con_take, con_we, exit_we;
    reg  [8:0]  con_in = 9'h1FF;            // bit 8: empty
    reg         io_sel = 1'b0;
    always @(posedge mem_clk) io_sel <= io;
    assign din = io_sel ? io_q : mem_q;

    iosys ios (
        .clk(mem_clk), .io_wr(run && io && wr), .io_rd(run && io && rd), .io_addr(addr[15:0]),
        .io_wdata(dout), .cpu_lat(1'b1), .cur_rd(run && io && rd), .cur_addr(addr[15:0]),
        .io_rdata(io_q), .cycles(cycles),
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
        .clk(mem_clk), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take({2'b00, dma_req}),
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
    localparam BAUD_DIV = CLK_HZ / 115200;  // 146 (+0.3%)
`endif
    wire       rx_ok;
    wire [7:0] rx_byte;
    reg        tx_go = 1'b0;
    reg  [7:0] tx_byte = 8'd0;
    wire       tx_busy;
    uart #(.DIV(BAUD_DIV)) serial (
        .clk(mem_clk), .rx(uart_rx), .rx_ok(rx_ok), .rx_byte(rx_byte),
        .tx_go(tx_go), .tx_byte(tx_byte), .tx_busy(tx_busy), .tx(uart_tx)
    );

    // console output: a 256-byte FIFO in front of the transmitter
    reg [7:0] txq [0:255];
    reg [7:0] txq_w = 8'd0, txq_r = 8'd0;
    wire      txq_empty = txq_w == txq_r;
    wire      txq_full  = (txq_w + 1'b1) == txq_r;
    reg       txq_we = 1'b0;
    reg [7:0] txq_d = 8'd0;
    always @(posedge mem_clk) begin
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

    always @(posedge mem_clk) begin
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
                            ld_a <= {lv[7:0], rx_byte} - 1'b1;   // pre-decremented: each byte advances it
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

// 16 KB of dual-port block RAM, one clock: port A reads or writes (no read
// while writing), port B reads. Eight 16K x 1 DPBs, instantiated because
// yosys gives a memory with two read ports a copy per port.
module ram16k8 (
    input  wire        clk,
    input  wire [13:0] a_addr,
    input  wire        a_we,
    input  wire [7:0]  a_d,
    output wire [7:0]  a_q,
    input  wire [13:0] b_addr,
    output wire [7:0]  b_q
);
`ifdef SIM
    reg [7:0] ram [0:16383];
    reg [7:0] qa = 8'd0, qb = 8'd0;
    always @(posedge clk) begin
        if (a_we) ram[a_addr] <= a_d;
        else qa <= ram[a_addr];
        qb <= ram[b_addr];
    end
    assign a_q = qa;
    assign b_q = qb;
`else
    genvar i;
    generate
        for (i = 0; i < 8; i = i + 1) begin : bit_
            wire [15:0] doa, dob;
            DPB #(
                .READ_MODE0(1'b0), .READ_MODE1(1'b0),
                .WRITE_MODE0(2'b00), .WRITE_MODE1(2'b00),
                .BIT_WIDTH_0(1), .BIT_WIDTH_1(1),
                .BLK_SEL_0(3'b000), .BLK_SEL_1(3'b000),
                .RESET_MODE("SYNC")
            ) b (
                .CLKA(clk), .CEA(1'b1), .OCEA(1'b1), .RESETA(1'b0), .WREA(a_we),
                .ADA(a_addr), .BLKSELA(3'b000), .DIA({15'd0, a_d[i]}), .DOA(doa),
                .CLKB(clk), .CEB(1'b1), .OCEB(1'b1), .RESETB(1'b0), .WREB(1'b0),
                .ADB(b_addr), .BLKSELB(3'b000), .DIB(16'd0), .DOB(dob)
            );
            assign a_q[i] = doa[0];
            assign b_q[i] = dob[0];
        end
    endgenerate
`endif
endmodule
