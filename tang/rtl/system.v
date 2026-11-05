`timescale 1ns/1ps
// fpga_cpu on the Tang Nano 20K: the T1 core (verilog/rtl/t1_core.v), 64 KB
// of block RAM and the 8 MB SDRAM (on the RAM machine: ../fpga/rtl/rammux.v
// in front of rtl/sdram_phy.v), the video of docs/io.md on HDMI
// (fpga/rtl/video.v, as on the Spartan-3, in true colour), the I/O
// registers (fpga/rtl/iosys.v), and the console and program loading on the
// USB serial port (the onboard BL616's second channel).
//
// Clocks. A PLL makes fclk, 67.5 MHz, for everything but the video, and the
// SDRAM's clock, the same in phase by default (SD_PSDA, below; sdram_phy.v).
// T1's clock is fclk
// through a DQCE clock gate: a T1 cycle is a period of fclk cycles ending
// with the one edge the gate lets through. T1's bus settles in under two
// fclk cycles, so it is taken at the second edge of the period (lat):
//   - block RAM (000000-00FFFF) and I/O: read or written at that edge, and
//     T1's edge two cycles later: 4 cycles, 16.875 MHz;
//   - the SDRAM (the rest, repeating every 8 MB; T1F has 24-bit addresses,
//     AW24, and reaches all of it, T1 the first 1 MB), through rammux port 0: a
//     write is done when its command goes out, a read when its word is back
//     (rvalid), and T1's edge follows: 4 cycles for a write to an open row,
//     7 for a read, longer behind a video burst.
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
`define SD_PSDA "0000"                     // the SDRAM's clock in phase with fclk: burst reads
                                           // pass at capture 3 on every placement tried (180
                                           // degrees put the word at the window's edge)
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
    (* keep *) wire fclk;                 // kept: constraints/system.sdc names it
    wire sdclk;
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

    // T1's clock: fclk where ce was high in the cycle before (ce: cpu_ce, or
    // the clock in which T1's SDRAM read comes back)
    reg  cpu_ce = 1'b0;
    wire ce;
    wire cpu_clk;
`ifdef SIM
    reg ce_l = 1'b0;
    always @(negedge fclk) ce_l <= ce;
    assign cpu_clk = fclk & ce_l;
`else
    DQCE gate (.CLKIN(fclk), .CE(ce), .CLKOUT(cpu_clk));
`endif

    // ---- T1 -------------------------------------------------------------------
    wire [23:0] addr;
    wire        rd, wr, halted, retire;
    wire        t1_irq, aud_irq;                // the interrupt line (iosys); the audio engine's flags
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;
    reg  [7:0]  din;
    reg  [15:0] sr_in;
    reg         run_req = 1'b0;             // the loader's 'G' (until DE AD BE EF)
    wire        run;                        // ... once the cache has been cleared
    reg  [23:0] entry = 24'h001000;
    reg         t1_rst = 1'b1;
    always @(posedge cpu_clk) t1_rst <= !run;

`ifdef T1F
    // 24-bit addresses: T1F reaches all 8 MB of the SDRAM (T1 folds its addresses to 1 MB);
    // interrupts from iosys's controller (F00700): the vertical blank, the audio engine
    t1f_core #(.EXT_X(0), .AW24(1), .IRQ(1)) cpu (
        .clk(cpu_clk), .ce(1'b1), .rst(t1_rst), .reset_pc(entry), .irq(t1_irq),
`else
    t1_core #(.EXT_X(0)) cpu (
        .clk(cpu_clk), .rst(t1_rst), .reset_pc(entry),
`endif
        .addr(addr), .mem_rd(rd), .mem_wr(wr), .dout(dout), .din(din),
        .sr_sel(sr_sel), .sr_in(sr_in),
        .halted(halted), .halt_code(halt_code), .retire(retire)
    );

    reg [31:0] insts = 32'd0;              // SR3: the low 16 bits; INSTRET (I/O): all 32
    always @(posedge cpu_clk)
        if (t1_rst) insts <= 32'd0;
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
            default: sr_in = insts[15:0];
        endcase

    // ---- the period: T1's bus at lat, then its edge when the access is done ---------
    // T1F's bus is looked at two fclk cycles after its edge, as T1's: the
    // paths from T1F's registers to what samples its bus are not timed
    // (constraints/system.sdc) and come out at 13.8-15.1 ns in this design,
    // so one cycle (14.8 ns, lat at e1) is not enough: placements where it
    // failed blacked out the HDMI picture. (PER = 2 sampled at e1.)
    localparam PER = 4;
    reg  [3:0]  ph = 4'd0;                  // fclk cycles since T1's edge
    wire        lat = ph == (PER == 2 ? 4'd0 : 4'd1);   // the bus has settled (T1F 11.6 ns, T1 ~21 ns)
    wire        io  = addr[23:16] == 8'hF0;
    reg  [1:0]  src = 2'd0;                 // din from: 0 the cache, 1 I/O, 2 c_word (a line fill)
    reg         aud_s = 1'b0;               // ... I/O: the audio engine's registers
    // ... or the video extensions' (F00220) and the blitter's (F00800), read through the same latch
    wire        x_vsel = addr[15:5] == 11'h011, x_bsel = addr[15:5] == 11'h040;
    wire        x_sel = x_vsel || x_bsel;
    reg  [7:0]  aud_q = 8'd0;               // the byte read from them
    reg         t1_wait = 1'b0;             // T1 waits: a line fill, or the write buffer

    // the loader (below) writes when T1 is held
    reg         ld_we = 1'b0;
    reg  [23:0] ld_a = 24'd0;
    reg  [7:0]  ld_d = 8'd0;

    // ---- memory: the SDRAM, through a cache ------------------------------------------
    // All memory (everything but I/O, F0xxxx) is the SDRAM, repeating every
    // 8 MB. In front of it, for T1: 32 KB of block RAM as a direct-mapped
    // cache of 2048 lines of 16 bytes, write-through (no allocate on a
    // write), with a one-entry write buffer. The SDRAM is always up to date,
    // so video and audio, which read it directly, see every write once the
    // buffer has gone out (a few clocks). The cache is cleared while T1 is
    // held (the loader writes the SDRAM directly).
    //
    // T1F's period (T1's is longer, below):
    //   e0  T1's edge; its bus settles
    //   e1  lat: the address goes to the tag and data block RAMs
    //   chk (the clock after e1): the tag compares. A read that hits, or a
    //       write the buffer can take, is done: cpu_ce at e2, T1's edge e3
    //       (3 clocks; block RAM took 2). A write goes into the buffer at e2
    //       (and into the cache if its line is there). (Raising ce straight
    //       from the compare, for e2, put the block RAM's output and the
    //       compare on DQCE's enable, an untimed path: T1 halted on the board.)
    //   otherwise, at e2: a read that missed fills its line (a burst of 4
    //       words through rammux port 0; T1's edge the clock after the last
    //       word, its byte from c_word); a write waits for the buffer; a
    //       period with no access after all ends at e3.
    reg  [22:0] m_a = 23'd0;                // the address at lat
    reg         mem_l = 1'b0, wr_l = 1'b0;  // a memory access (not I/O); a write
    wire [8:0]  tag_q;                      // {valid, tag} of the line at m_a
    wire [31:0] line_q;                     // the word at m_a
    wire        hit = tag_q[8] && tag_q[7:0] == m_a[22:15];

    // port 0 of rammux: one request at a time (the write buffer, a line
    // fill, or a loader write)
    reg         c_req = 1'b0, c_we = 1'b0, c_fill = 1'b0;
    reg  [22:0] c_addr = 23'd0;
    reg  [7:0]  c_wd = 8'd0;
    reg  [31:0] c_word = 32'd0;             // the filled line's word T1 asked for
    wire [4:0]  p_ack, p_wnext, p_rvalid;   // rammux: 0 T1 and the loader, 1 video, 2 audio, 3 the SDRAM self-test, 4 the blitter
    wire [31:0] p_rdata;
    reg         f_on = 1'b0;                // a fill's words are coming back
    reg  [1:0]  f_k = 2'd0;                 // ... the next one's place in the line
    reg         w_on = 1'b0;                // T1's write waits for the buffer
    reg         hit_s = 1'b0;               // ... and its line is in the cache
    wire        fill_we = f_on && p_rvalid[0];

    // clearing the cache: every line, while T1 is held; T1 runs after a
    // whole pass (2048 clocks) since it was last held
    reg  [10:0] sw = 11'd0;
    reg         clean = 1'b0, run_d = 1'b0;
    assign run = run_req && clean;

    // T1F's chk clock (T1: the clock after lat, registered below)
    wire        chk = ph == (PER == 2 ? 4'd1 : 4'd2) && mem_l && run && !t1_wait;
    wire        ok  = wr_l ? !c_req : hit;  // done now: T1's edge at e2
    wire        cw_we = (chk && wr_l && ok && hit) || (w_on && !c_req && hit_s);   // T1's write into the cache
    assign ce = cpu_ce;

    // T1F: at lat (one fclk cycle after T1F's edge) the address alone says
    // whether this is memory or I/O; rd/wr (from T1F's decode) have had two
    // cycles at e2. T1 has four-cycle periods.
    reg         io_q2 = 1'b0;               // I/O, at lat
    wire        io_lat = (PER == 2) ? (ph == 4'd1 && io_q2) : lat;   // I/O registers: their address decode gets two cycles
    always @(posedge fclk) begin
        ph     <= ce ? 4'd0 : (ph == 4'd15 ? ph : ph + 1'b1);
        cpu_ce <= 1'b0;
        if (lat) begin
            cycles_t1 <= cycles;            // SR0/SR1 hold still from here to T1's edge
            src   <= io ? 2'd1 : 2'd0;
            aud_s <= addr[15:12] == 4'h1 || x_sel;   // F01000-F01FFF: the audio engine; F00220, F00800: below
            io_q2 <= io;
            m_a   <= addr[22:0];
            mem_l <= !io;
            wr_l  <= wr;
        end

        // ---- the chk clock: done, or the wait begins
        if (chk) begin
            if (ok) begin
                cpu_ce <= 1'b1;
                if (wr_l) begin
                    c_req <= 1'b1; c_we <= 1'b1; c_fill <= 1'b0; c_addr <= m_a; c_wd <= dout;
                end
            end else if (wr_l) begin
                t1_wait <= 1'b1; w_on <= 1'b1; hit_s <= hit;
            end else if (rd) begin
                t1_wait <= 1'b1;            // a miss: the fill goes out when the buffer is free
                f_on <= 1'b1; f_k <= 2'd0;
            end else
                cpu_ce <= 1'b1;             // no access after all
        end

        // ---- the write waiting for the buffer
        if (w_on && !c_req) begin
            c_req <= 1'b1; c_we <= 1'b1; c_fill <= 1'b0; c_addr <= m_a; c_wd <= dout;
            w_on <= 1'b0; t1_wait <= 1'b0; cpu_ce <= 1'b1;
        end

        // ---- a line fill: requested once the buffer is free, then its words
        if (f_on && !c_req && !c_fill) begin
            c_req <= 1'b1; c_we <= 1'b0; c_fill <= 1'b1; c_addr <= m_a;
        end
        if (fill_we) begin
            f_k <= f_k + 1'b1;
            if (f_k == m_a[3:2]) c_word <= p_rdata;
            if (f_k == 2'd3) begin
                f_on <= 1'b0; c_fill <= 1'b0; t1_wait <= 1'b0; cpu_ce <= 1'b1; src <= 2'd2;
            end
        end

        // rammux took the request
        if (c_req && p_ack[0]) c_req <= 1'b0;

        // (held in reset, T1 is clocked whatever its bus says)
        if (ph == PER - 2 && (!run || (PER != 2 && io_q2))) cpu_ce <= 1'b1;
        if (PER == 2 && ph == 4'd1 && io_q2 && run) cpu_ce <= 1'b1;   // I/O done at io_lat

        // the loader
        if (!run && ld_we) begin
            c_req <= 1'b1; c_we <= 1'b1; c_fill <= 1'b0; c_addr <= ld_a[22:0]; c_wd <= ld_d;
        end

        // clearing the cache
        run_d <= run_req;
        if (run_d && !run_req) begin
            clean <= 1'b0; sw <= 11'd0;
        end else if (!run) begin
            sw <= sw + 1'b1;
            if (sw == 11'd2047) clean <= 1'b1;
        end
    end

    // the cache's block RAM: tags (2048 x 9) and data (four byte lanes of 8K)
    // A line fill writes its tag after its last word; in the other clocks a
    // blitter write's line is invalidated (inv_ack), so T1 reads what the
    // blitter wrote.
    wire        fill_tag = fill_we && f_k == 2'd3;
    wire        blt_inv, blt_inv_ack;
    wire [10:0] blt_inv_line;
    assign      blt_inv_ack = blt_inv && (!run || !fill_tag);
    wire [8:0]  tag_wd = (run && fill_tag) ? {1'b1, m_a[22:15]} : 9'd0;
    wire        tag_we = run ? (fill_tag || blt_inv) : 1'b1;
    wire [10:0] tag_wa = !run ? sw : fill_tag ? m_a[14:4] : blt_inv_line;
    cram #(.AW(11), .DW(9)) tags (.clk(fclk), .we(tag_we), .wa(tag_wa), .wd(tag_wd),
                                  .re(lat), .ra(addr[14:4]), .q(tag_q));
    genvar gl;
    generate
        for (gl = 0; gl < 4; gl = gl + 1) begin : lane
            wire we = fill_we || (cw_we && m_a[1:0] == gl);
            cram #(.AW(13), .DW(8)) d (.clk(fclk), .we(we),
                .wa(fill_we ? {m_a[14:4], f_k} : m_a[14:2]),
                .wd(fill_we ? p_rdata[8*gl +: 8] : dout),
                .re(lat), .ra(addr[14:2]), .q(line_q[8*gl +: 8]));
        end
    endgenerate
    wire [7:0]  cache_q = line_q[8*m_a[1:0] +: 8];
    wire [7:0]  c_q = c_word[8*m_a[1:0] +: 8];

    // ---- SDRAM: the RAM machine ------------------------------------------------------------
    // rammux ports, highest priority first:
    //   0 T1's cache (line fills, 4 words; the write buffer, one byte) and the
    //     loader while T1 is held
    //   1 video DMA (video.v): runs of up to VLEN words, one SDRAM burst each
    //     (8 clocks). A run is not interrupted, so that is the longest T1
    //     waits behind one; the line buffers leave video a line of slack.
    //   2 the audio engine (../fpga/rtl/audio_engine.v): sample data, bursts of
    //     8 words (16 clocks), read only. Each voice reads ahead 16 words, so
    //     the lowest priority is enough.
    localparam VLEN = 8;                    // an SDRAM burst (rows are multiples of 8 words)
    wire        dma_req, dma_valid;
    wire [17:0] dma_addr;
    wire [8:0]  dma_left;
    // video.v advances its address by dma_take a clock after rammux's ack
    // (registered: off the grant -> ready -> ack path), and its request is
    // held off in that clock (its address is not yet the next one). A burst
    // takes 7 clocks or more, so this costs nothing.
    wire [4:0]  dma_len  = (dma_left >= VLEN) ? VLEN : dma_left[4:0];
    reg  [4:0]  dma_take = 5'd0;
    always @(posedge fclk) dma_take <= p_ack[1] ? dma_len : 5'd0;
    wire        v_req = dma_req && dma_take == 5'd0;
    assign dma_valid = p_rvalid[1];

    wire        phy_cmd, phy_we, phy_ready, phy_rvalid, sd_ready;
    wire [20:0] phy_addr;
    wire [31:0] phy_wdata, phy_rdata;
    wire [3:0]  phy_be;
    wire        a_req;
    wire [20:0] a_addr;
    wire [4:0]  a_len;
    // port 3: the SDRAM self-test (below), while T1 is held
    reg  [2:0]  sd_cap = 3'd3;              // the PHY's read capture (2-4 clocks): 'K', 'C'
    wire        bt_req, bt_we;
    wire [20:0] bt_addr;
    wire [4:0]  bt_len;
    wire [31:0] bt_wd;
    wire [3:0]  bt_wbe;
    reg  [4:0]  bt_rlen = 5'd8;             // 'C' 8, 'D' 4, 'E' 1; 'B' 8 after byte writes
    reg         bt_bytew = 1'b0;
    //   4 the blitter (blitter.v): chunks of up to 17 words, lowest priority
    wire        blt_req, blt_we;
    wire [20:0] blt_addr;
    wire [4:0]  blt_len;
    wire [31:0] blt_wd;
    wire [3:0]  blt_wbe;
    rammux #(.NP(5), .AW(21), .LW(5), .WMASK(5'b11001)) mux (
        .clk(fclk),
        .p_req({blt_req, bt_req, a_req, v_req, c_req}), .p_we({blt_we, bt_we, 2'b00, c_we}),
        .p_addr({blt_addr, bt_addr, a_addr, 3'b000, dma_addr, c_fill ? {c_addr[22:4], 2'b00} : c_addr[22:2]}),
        .p_len({blt_len, bt_len, a_len, dma_len, c_fill ? 5'd4 : 5'd1}), .p_ack(p_ack),
        .p_wdata({blt_wd, bt_wd, 64'd0, {4{c_wd}}}), .p_wbe({blt_wbe, bt_wbe, 8'h00, 4'b0001 << c_addr[1:0]}), .p_wnext(p_wnext),
        .p_rvalid(p_rvalid), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));
    sdram_phy #(
`ifdef SIM
        .INIT_WAIT(100)
`else
        .INIT_WAIT(15000)
`endif
    ) phy (
        .clk(fclk), .cap(sd_cap),
        .in_cmd(phy_cmd), .in_we(phy_we), .in_addr(phy_addr), .in_wdata(phy_wdata), .in_be(phy_be),
        .ready(phy_ready), .rvalid(phy_rvalid), .rdata(phy_rdata), .init_done(sd_ready),
        .sd_cke(O_sdram_cke), .sd_cs_n(O_sdram_cs_n), .sd_ras_n(O_sdram_ras_n),
        .sd_cas_n(O_sdram_cas_n), .sd_we_n(O_sdram_wen_n), .sd_a(O_sdram_addr),
        .sd_ba(O_sdram_ba), .sd_dqm(O_sdram_dqm), .sd_dq(IO_sdram_dq)
    );

    // ---- the SDRAM self-test ------------------------------------------------------------
    // Serial 'C' (T1 held): write 1024 words (each different) at 0E0000, read
    // them back in runs of 8 (SDRAM bursts) with the read capture at 2, 3 and
    // 4 clocks, and print "\nCAP c ERR eeee" for each (words wrong, hex; the
    // capture is left at 3). 'K' n sets the capture (2-4) for what follows.
    wire        bt_go, bt_pv;
    wire [2:0]  bt_cap;
    wire        bt_capw;
    wire [7:0]  bt_pc;
    wire        bt_pr;
// -DSDBIST: the SDRAM self-test (serial 'C', 'D', 'E', 'B', 'K'): left out by default,
// for room (the video extensions and the blitter)
`ifdef SDBIST
    sdbist bist (.clk(fclk), .go(bt_go), .rlen(bt_rlen), .bytew(bt_bytew),
                 .req(bt_req), .we(bt_we), .addr(bt_addr), .len(bt_len), .wd(bt_wd), .wbe(bt_wbe),
                 .ack(p_ack[3]), .rvalid(p_rvalid[3]), .rdata(p_rdata),
                 .cap(bt_cap), .cap_we(bt_capw), .pc(bt_pc), .pv(bt_pv), .pr(bt_pr));
`else
    assign bt_req = 1'b0; assign bt_we = 1'b0; assign bt_addr = 21'd0; assign bt_len = 5'd0;
    assign bt_wd = 32'd0; assign bt_wbe = 4'd0; assign bt_cap = 3'd0; assign bt_capw = 1'b0;
    assign bt_pc = 8'd0; assign bt_pv = 1'b0;
`endif

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
            2'd0: din = cache_q;
            2'd1: din = aud_s ? aud_q : io_q;
            default: din = c_q;
        endcase

    iosys #(.IRQ(1)) ios (
        .clk(fclk), .io_wr(io_lat && run && io && wr), .io_rd(io_lat && run && io && rd),
        .io_addr(addr[15:0]), .io_wdata(dout), .cpu_lat(io_lat), .cur_rd(run && io && rd),
        .cur_addr(addr[15:0]), .io_rdata(io_q), .cycles(cycles), .insts(insts),
        .line_tgl(line_tgl), .line_top(line_top),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .switches(8'd0), .buttons({2'b00, s_s}), .leds(leds), .seg7(), .seg7_ctrl(),
        .con_in(con_in), .con_take(con_take), .con_we(con_we), .exit_we(exit_we), .wvalue(wvalue),
        .irq_src({3'd0, aud_irq, 3'd0}), .irq(t1_irq)   // source 3: an audio engine voice's interrupt flag
    );

    // ---- audio ---------------------------------------------------------------------------
    // The engine's registers (F01000 global, F01800 voices: docs/audio.md) on
    // the same write strobe as iosys. Reads: the engine's rd_data (16 bits)
    // is valid a clock after the address, which has settled by io_lat; as in
    // iosys, the low byte is read live and the word latched, and the high
    // byte comes from the latch when it follows.
    wire [15:0] vx_rd, blt_rd, aud_rd;
    wire [15:0] aud_or_x = x_vsel ? vx_rd : x_bsel ? blt_rd : aud_rd;
    reg  [15:0] aud_latch = 16'd0;
    reg  [15:0] aud_la = 16'hFFFF;
    always @(posedge fclk)
        if (io_lat && run && io && rd) begin
            if (!addr[0]) begin aud_q <= aud_or_x[7:0]; aud_latch <= aud_or_x; aud_la <= addr[15:0]; end
            else aud_q <= (addr[15:0] == (aud_la | 16'd1)) ? aud_latch[15:8] : aud_or_x[15:8];
        end

    wire        s_valid, s_ready, a_pop, a_under;
    wire [15:0] s_l, s_r, a_l, a_r;
    audio_engine #(.AW(21)) aud (
        .clk(fclk),
        .io_wr(io_lat && run && io && wr), .io_addr(addr[15:0]), .io_wdata(dout),
        .rd_addr(addr[15:0]), .rd_data(aud_rd), .irq(aud_irq),
        .p_req(a_req), .p_we(), .p_addr(a_addr), .p_len(a_len),
        .p_ack(p_ack[2]), .p_rvalid(p_rvalid[2]), .p_rdata(p_rdata),
        .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready)
    );
    // to the pixel clock: dvi_tx pops a sample 48,000 times a second, and the
    // engine makes one whenever the FIFO has room
    hdmi_audio_cdc afifo (.s_clk(fclk), .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready),
                          .p_clk(pclk), .p_pop(a_pop), .p_l(a_l), .p_r(a_r), .p_under(a_under));

    // ---- video ------------------------------------------------------------------------
    wire [23:0] rgb24;
    wire        hs, vs, de, v_late;
    // the video extensions' registers (F00220) and the blitter (F00800)
    wire [15:0] vid_ctrl2, vid_scrollxb, vid_scrollyb, spr_over, spr_wd;
    wire [31:0] vid_bmpb, vid_lscr, vid_spr;
    wire        spr_we, frame_latch;
    wire [7:0]  spr_wa;
    vidx_regs vxr (
        .clk(fclk), .io_wr(io_lat && run && io && wr), .io_addr(addr[15:0]), .io_wdata(dout), .rd(vx_rd),
        .vid_ctrl(vid_ctrl), .line_tgl(line_tgl), .line_top(line_top), .frame_latch(frame_latch),
        .spr_over(spr_over), .vid_ctrl2(vid_ctrl2), .vid_bmpb(vid_bmpb), .vid_scrollxb(vid_scrollxb),
        .vid_scrollyb(vid_scrollyb), .vid_lscr(vid_lscr), .vid_spr(vid_spr),
        .spr_we(spr_we), .spr_wa(spr_wa), .spr_wd(spr_wd)
    );
    blitter #(.AW(21)) blt (
        .clk(fclk), .io_wr(io_lat && run && io && wr), .io_addr(addr[15:0]), .io_wdata(dout), .rd(blt_rd), .busy(),
        .p_req(blt_req), .p_we(blt_we), .p_addr(blt_addr), .p_len(blt_len), .p_ack(p_ack[4]),
        .p_wdata(blt_wd), .p_wbe(blt_wbe), .p_wnext(p_wnext[4]), .p_rvalid(p_rvalid[4]), .p_rdata(p_rdata),
        .inv_req(blt_inv), .inv_line(blt_inv_line), .inv_ack(blt_inv_ack)
    );
    video #(.FONT_FILE(FONT_FILE), .TRUE_COLOR(1), .EXT(1)) vid (
        .vid_ctrl2(vid_ctrl2), .vid_bmpb(vid_bmpb), .vid_scrollxb(vid_scrollxb), .vid_scrollyb(vid_scrollyb),
        .vid_lscr(vid_lscr), .vid_spr(vid_spr), .spr_we(spr_we), .spr_wa(spr_wa), .spr_wd(spr_wd),
        .frame_latch(frame_latch), .spr_over(spr_over),
        .clk(fclk), .pclk(pclk),
        .vid_ctrl(vid_ctrl), .vid_backdrop(vid_backdrop), .vid_text(vid_text), .vid_bmp(vid_bmp),
        .vid_scrollx(vid_scrollx), .vid_scrolly(vid_scrolly), .vid_cursor(vid_cursor),
        .pal_we(pal_we), .pal_windex(pal_windex), .pal_wdata(pal_wdata),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_left(dma_left), .dma_take(dma_take),
        .dma_valid(dma_valid), .dma_data(p_rdata),
        .line_tgl(line_tgl), .line_top(line_top),
        .hsync(hs), .vsync(vs), .rgb(), .rgb24(rgb24), .de(de), .late(v_late)
    );
    // -DNOAUDIO: HDMI without audio packets (a test build). -DPKTCAP too:
    // the TV gets no audio packets, and a copy of dvi_tx with audio (no pins)
    // makes them, pacing the FIFO as the real one would, into a capture
    // buffer that goes out on the console (below)
`ifdef PKTCAP
 `define NOAUDIO
`endif
`ifdef NOAUDIO
    localparam HDMI_AUDIO = 0;
`else
    localparam HDMI_AUDIO = 1;
`endif
    // without audio packets a 48 kHz tick drains the FIFO, so the engine (and a
    // player paced by it) keeps the same load
    wire        tx_pop;
    reg  [10:0] np_t = 11'd0;
    always @(posedge pclk) np_t <= (np_t == 11'd1546) ? 11'd0 : np_t + 1'b1;   // 74.25 MHz / 1547
`ifdef PKTCAP
    wire        sh_pop, sh_isl;
    wire [29:0] sh_w;
    assign a_pop = sh_pop;
`else
    assign a_pop = HDMI_AUDIO ? tx_pop : (np_t == 11'd0);
`endif
    // -DBARS: hdmiaudio.v's colour bars in place of the picture (a test
    // build): everything else, video's fetches and timing included, runs
    wire [23:0] tx_rgb;
`ifdef BARS
    reg  [10:0] bx = 11'd0;
    reg         de_b = 1'b0;
    always @(posedge pclk) begin
        de_b <= de;
        bx <= (de_b && !de) ? 11'd0 : (de ? bx + 1'b1 : bx);
    end
    wire [2:0] bar = bx[10:7] < 10 ? 3'd7 - bx[9:7] : 3'd0;
    assign tx_rgb = {{8{bar[2]}}, {8{bar[1]}}, {8{bar[0]}}};
`else
    assign tx_rgb = rgb24;
`endif
    dvi_tx #(.HDMI(1), .AUDIO(HDMI_AUDIO)) tx (.pclk(pclk), .sclk(sclk), .rst(!plocked), .de(de), .hsync(hs), .vsync(vs),
               .r(tx_rgb[23:16]), .g(tx_rgb[15:8]), .b(tx_rgb[7:0]),
               .a_l(a_l), .a_r(a_r), .a_pop(tx_pop), .tmds_p(tmds_p), .tmds_n(tmds_n));
`ifdef PKTCAP
    dvi_tx #(.HDMI(1), .AUDIO(1), .NOSER(1)) sh (.pclk(pclk), .sclk(sclk), .rst(!plocked), .de(de), .hsync(hs), .vsync(vs),
               .r(tx_rgb[23:16]), .g(tx_rgb[15:8]), .b(tx_rgb[7:0]),
               .a_l(a_l), .a_r(a_r), .a_pop(sh_pop), .tmds_p(), .tmds_n(), .isl(sh_isl), .wout(sh_w));
`endif
`ifdef PKTCAP
    // The capture: from the first data island after a vsync, every island
    // word of the copy's output, {2'b0x, ch2, ch1, ch0} (x: the island's
    // first word), and after each island {2'b10, 6'd0, pixels from the
    // previous island's start to this one's}, until 8192 words are taken.
    // Armed from fclk (25 s after configuration, then every 12 s); when
    // full it goes out as "#P xxxxxxxx" lines (below).
    localparam CAPN = 8192;
    reg  [31:0] cap_m [0:CAPN-1];
    reg  [12:0] cap_wa = 13'd0;
    reg         cap_arm_t = 1'b0, cap_done_t = 1'b0;     // toggles: fclk -> pclk, pclk -> fclk
    reg  [2:0]  cap_arm_s = 3'd0;
    reg  [1:0]  cap_st = 2'd0;                           // 0 idle, 1 wait vsync, 2 wait island, 3 on
    reg  [2:0]  cap_skip = 3'd0, cap_vn = 3'd0;
    reg         isl_p = 1'b0, vs_cp = 1'b0;
    reg  [23:0] cap_gap = 24'd0, cap_gl = 24'd0;
    reg         cap_we = 1'b0;
    reg  [31:0] cap_wd = 32'd0;
    always @(posedge pclk) begin
        cap_arm_s <= {cap_arm_s[1:0], cap_arm_t};
        isl_p <= sh_isl;
        vs_cp <= vs;
        cap_gap <= (sh_isl && !isl_p) ? 24'd1 : cap_gap + 1'b1;
        if (sh_isl && !isl_p) cap_gl <= cap_gap;
        cap_we <= 1'b0;
        case (cap_st)
            2'd0: if (cap_arm_s[2] != cap_arm_s[1]) begin cap_st <= 2'd1; cap_vn <= cap_skip; end
            2'd1: if (vs && !vs_cp) begin   // capture n starts n frames later (n mod 8): the
                      // audio's 192-sample blocks sit differently in each
                      if (cap_vn == 3'd0) begin cap_st <= 2'd2; cap_skip <= cap_skip + 1'b1; end
                      else cap_vn <= cap_vn - 1'b1;
                  end
            2'd2: if (sh_isl && !isl_p) begin
                      cap_st <= 2'd3; cap_wa <= 13'd0;
                      cap_we <= 1'b1; cap_wd <= {2'b01, sh_w};
                  end
            default: begin
                if (cap_we && cap_wa == CAPN - 1) begin
                    cap_st <= 2'd0; cap_done_t <= ~cap_done_t;
                end else begin
                    if (cap_we) cap_wa <= cap_wa + 1'b1;
                    if (sh_isl) begin cap_we <= 1'b1; cap_wd <= {1'b0, !isl_p, sh_w}; end
                    else if (isl_p) begin cap_we <= 1'b1; cap_wd <= {2'b10, 6'd0, cap_gl}; end
                end
            end
        endcase
        if (cap_we) cap_m[cap_wa] <= cap_wd;
    end
    reg  [12:0] pk_a = 13'd0;
    reg  [31:0] pk_rd = 32'd0, pk_w = 32'd0;
    always @(posedge fclk) pk_rd <= cap_m[pk_a];
`endif

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
    localparam L_IDLE = 3'd0, L_ADDR = 3'd1, L_LEN = 3'd2, L_DATA = 3'd3, L_GO = 3'd4, L_CAP = 3'd5;
    reg        bt_go_r = 1'b0, bt_pr_r = 1'b0;
    assign     bt_go = bt_go_r;
    assign     bt_pr = bt_pr_r;
    reg [2:0]  ls = L_IDLE;
    reg [1:0]  lk = 2'd0;
    reg [23:0] lv = 24'd0;
    reg [23:0] ln = 24'd0;
    reg        lgo = 1'b0;
    reg [7:0]  code = 8'd0;
    reg [31:0] sync = 32'd0;

    // T1 halted while running (a fault, or SYS): "\nHALT cc @aaaaaa\n" (the
    // halt code and the address on T1's bus) on the console, then 0x04 and
    // the code, as EXIT ends a run (tools/run.py prints it)
    reg  [1:0]  h_s = 2'b00;
    reg  [4:0]  h_k = 5'd0;                 // the message's next character, 0 = none
    reg  [31:0] h_v = 32'd0;                // {code, address}
    wire        h_rise = h_s[0] && !h_s[1];
    function [7:0] hexc;
        input [3:0] n;
        hexc = n < 4'd10 ? 8'h30 + n : 8'h37 + n;
    endfunction
    reg  [7:0]  h_c;
    always @* begin
        case (h_k)
            5'd1: h_c = 8'h0A;  5'd2: h_c = "H";  5'd3: h_c = "A";  5'd4: h_c = "L";  5'd5: h_c = "T";
            5'd6: h_c = " ";    5'd7: h_c = hexc(h_v[31:28]);       5'd8: h_c = hexc(h_v[27:24]);
            5'd9: h_c = " ";    5'd10: h_c = "@";
            5'd11: h_c = hexc(h_v[23:20]); 5'd12: h_c = hexc(h_v[19:16]); 5'd13: h_c = hexc(h_v[15:12]);
            5'd14: h_c = hexc(h_v[11:8]);  5'd15: h_c = hexc(h_v[7:4]);   5'd16: h_c = hexc(h_v[3:0]);
            5'd17: h_c = 8'h0A; 5'd18: h_c = 8'h04;
            default: h_c = h_v[31:24];      // 19: the code
        endcase
    end

`ifdef DIAG
    // -DDIAG: once a second, "\n#L llll U uuuu F ffff\n" on the console:
    // video lines fetched late, HDMI audio FIFO underruns (a pop that found
    // it empty), frames (vsync); hex, 16 bits, counted from the start
    reg  [15:0] dg_l = 16'd0, dg_u = 16'd0, dg_f = 16'd0;
    reg  [2:0]  dg_us = 3'd0, dg_vs = 3'd0;
    reg         dg_ut = 1'b0, dg_vt = 1'b0;    // pclk toggles
    reg         vs_p = 1'b0;
    always @(posedge pclk) begin
        vs_p <= vs;
        if (a_under) dg_ut <= ~dg_ut;
        if (vs && !vs_p) dg_vt <= ~dg_vt;
    end
    reg  [26:0] dg_t = 27'd0;
    reg  [4:0]  dg_k = 5'd0;
    reg  [47:0] dg_v = 48'd0;
    reg  [7:0]  dg_c;
    always @* begin
        case (dg_k)
            5'd1: dg_c = 8'h0A; 5'd2: dg_c = "#"; 5'd3: dg_c = "L"; 5'd4: dg_c = " ";
            5'd5: dg_c = hexc(dg_v[47:44]); 5'd6: dg_c = hexc(dg_v[43:40]); 5'd7: dg_c = hexc(dg_v[39:36]); 5'd8: dg_c = hexc(dg_v[35:32]);
            5'd9: dg_c = " "; 5'd10: dg_c = "U"; 5'd11: dg_c = " ";
            5'd12: dg_c = hexc(dg_v[31:28]); 5'd13: dg_c = hexc(dg_v[27:24]); 5'd14: dg_c = hexc(dg_v[23:20]); 5'd15: dg_c = hexc(dg_v[19:16]);
            5'd16: dg_c = " "; 5'd17: dg_c = "F"; 5'd18: dg_c = " ";
            5'd19: dg_c = hexc(dg_v[15:12]); 5'd20: dg_c = hexc(dg_v[11:8]); 5'd21: dg_c = hexc(dg_v[7:4]); 5'd22: dg_c = hexc(dg_v[3:0]);
            default: dg_c = 8'h0A;
        endcase
    end
`endif

`ifdef PKTCAP
    reg  [2:0]  pk_ds = 3'd0;
    reg  [26:0] pk_t = 27'd0;
    reg  [4:0]  pk_s = 5'd0;
    reg         pk_first = 1'b1, pk_on = 1'b0;
    reg  [3:0]  pk_k = 4'd0;
`endif
    always @(posedge fclk) begin
        txq_we <= 1'b0;
        ld_we  <= 1'b0;
`ifdef DIAG
        dg_us <= {dg_us[1:0], dg_ut};
        dg_vs <= {dg_vs[1:0], dg_vt};
        if (v_late) dg_l <= dg_l + 1'b1;
        if (dg_us[2] != dg_us[1]) dg_u <= dg_u + 1'b1;
        if (dg_vs[2] != dg_vs[1]) dg_f <= dg_f + 1'b1;
        dg_t <= (dg_t == CLK_HZ - 1) ? 27'd0 : dg_t + 1'b1;
        if (dg_t == 27'd0 && dg_k == 5'd0) begin dg_k <= 5'd1; dg_v <= {dg_l, dg_u, dg_f}; end
        else if (dg_k != 5'd0 && h_k == 5'd0 && !con_we && !exit_we && !lgo) begin
            txq_we <= 1'b1; txq_d <= dg_c;
            dg_k <= (dg_k == 5'd23) ? 5'd0 : dg_k + 1'b1;
        end
`endif

`ifdef PKTCAP
        pk_ds <= {pk_ds[1:0], cap_done_t};
`ifdef SIM
        pk_t  <= (pk_t == CLK_HZ / 1000 - 1) ? 27'd0 : pk_t + 1'b1;   // a "second" of 1 ms
`else
        pk_t  <= (pk_t == CLK_HZ - 1) ? 27'd0 : pk_t + 1'b1;
`endif
        if (pk_t == 27'd0 && !pk_on) begin
            if (pk_s == (pk_first ? 5'd24 : 5'd11)) begin pk_s <= 5'd0; pk_first <= 1'b0; cap_arm_t <= ~cap_arm_t; end
            else pk_s <= pk_s + 1'b1;
        end
        if (pk_ds[2] != pk_ds[1]) begin pk_on <= 1'b1; pk_a <= 13'd0; pk_k <= 4'd0; end
        else if (pk_on && pk_k == 4'd0) pk_k <= 4'd1;
        else if (pk_on && h_k == 5'd0 && !con_we && !exit_we && !lgo && (txq_w - txq_r) < 8'd200) begin
            txq_we <= 1'b1;
            case (pk_k)
                4'd1: begin txq_d <= "#"; pk_w <= pk_rd; end
                4'd2: txq_d <= "P";
                4'd3: txq_d <= " ";
                4'd12: txq_d <= 8'h0A;
                default: txq_d <= hexc(pk_w[31 - 4 * (pk_k - 4) -: 4]);
            endcase
            if (pk_k == 4'd12) begin
                pk_k <= 4'd0; pk_a <= pk_a + 1'b1;
                if (pk_a == CAPN - 1) pk_on <= 1'b0;
            end else
                pk_k <= pk_k + 1'b1;
        end
`endif
        h_s <= {h_s[0], halted && run && !t1_rst};
        if (h_rise && h_k == 5'd0) begin h_k <= 5'd1; h_v <= {halt_code, addr}; end
        else if (h_k != 5'd0 && !con_we && !exit_we && !lgo) begin
            txq_we <= 1'b1; txq_d <= h_c;
            h_k <= (h_k == 5'd19) ? 5'd0 : h_k + 1'b1;
        end
        // the self-test's report, when nothing else is being sent
        bt_pr_r <= 1'b0;
        if (bt_pv && !bt_pr_r && h_k == 5'd0 && !h_rise && !con_we && !exit_we && !lgo
`ifdef DIAG
            && dg_k == 5'd0 && dg_t != 27'd0
`endif
            ) begin
            txq_we <= 1'b1; txq_d <= bt_pc; bt_pr_r <= 1'b1;
        end

        // CON_OUT, and EXIT: 0x04 then the code
        if (con_we) begin txq_we <= 1'b1; txq_d <= wvalue[7:0]; end
        if (exit_we) begin txq_we <= 1'b1; txq_d <= 8'h04; lgo <= 1'b1; code <= wvalue[7:0]; end
        if (lgo && !txq_we && !exit_we) begin
            txq_we <= 1'b1; txq_d <= code; lgo <= 1'b0;
        end

        if (con_take) con_in[8] <= 1'b1;   // a read takes it

        // serial input: the sync word, the loader, or CON_IN
        bt_go_r <= 1'b0;
        if (bt_capw) sd_cap <= bt_cap;
        if (rx_ok) begin
            sync <= {sync[23:0], rx_byte};
            if ({sync[23:0], rx_byte} == 32'hDEADBEEF) begin
                run_req <= 1'b0; ls <= L_IDLE;
            end else if (run)
                con_in <= {1'b0, rx_byte};
            else case (ls)
                L_IDLE: begin
                    lk <= 2'd0;
                    if (rx_byte == "L") ls <= L_ADDR;
                    if (rx_byte == "G") ls <= L_GO;
                    if (rx_byte == "C" || rx_byte == "D" || rx_byte == "E" || rx_byte == "B") begin
                        bt_go_r  <= 1'b1;
                        bt_rlen  <= rx_byte == "D" ? 5'd4 : rx_byte == "E" ? 5'd1 : 5'd8;
                        bt_bytew <= rx_byte == "B";
                    end
                    if (rx_byte == "K") ls <= L_CAP;
                end
                L_CAP: begin
                    if (rx_byte >= 8'd2 && rx_byte <= 8'd4) sd_cap <= rx_byte[2:0];
                    ls <= L_IDLE;
                end
                L_ADDR, L_GO: begin
                    lv <= {lv[15:0], rx_byte};
                    lk <= lk + 1'b1;
                    if (lk == 2'd2) begin
                        if (ls == L_GO) begin
                            entry <= {lv[15:0], rx_byte};
                            run_req <= 1'b1;
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


// A simple dual-port block RAM: a write port and a registered read.
module cram #(
    parameter AW = 11,
    parameter DW = 9
) (
    input  wire          clk,
    input  wire          we,
    input  wire [AW-1:0] wa,
    input  wire [DW-1:0] wd,
    input  wire          re,
    input  wire [AW-1:0] ra,
    output reg  [DW-1:0] q
);
    reg [DW-1:0] m [0:(1 << AW) - 1];
    integer i;
    initial for (i = 0; i < (1 << AW); i = i + 1) m[i] = {DW{1'b0}};
    always @(posedge clk) begin
        if (we) m[wa] <= wd;
        if (re) q <= m[ra];
    end
endmodule


// The SDRAM self-test (system.v, serial 'C'): 1024 words written at word
// 0x38000 (byte 0E0000), each different, then read back in runs of 8 (SDRAM
// bursts) with the read capture at 2, 3 and 4 clocks; after each, the
// report "\nCAP c ERR eeee" (the words that came back wrong, hex) goes out
// one character at a time (pv/pc, pr when taken). The capture is left at 3.
module sdbist (
    input  wire        clk,
    input  wire        go,
    input  wire [4:0]  rlen,                // read runs: 8, 4 or 1 words
    input  wire        bytew,               // write each word as four byte writes
    output reg         req = 1'b0,
    output reg         we = 1'b0,
    output reg  [20:0] addr = 21'd0,
    output wire [4:0]  len,
    output wire [31:0] wd,
    output wire [3:0]  wbe,
    input  wire        ack,
    input  wire        rvalid,
    input  wire [31:0] rdata,
    output reg  [2:0]  cap = 3'd3,
    output reg         cap_we = 1'b0,
    output reg  [7:0]  pc = 8'd0,
    output wire        pv,
    input  wire        pr
);
    localparam [20:0] BASE = 21'h38000;
    localparam S_IDLE = 3'd0, S_W = 3'd1, S_R = 3'd2, S_RW = 3'd3, S_P = 3'd4, S_SET = 3'd5;
    reg  [2:0]  st = S_IDLE;
    reg  [9:0]  i = 10'd0;                  // the next word to write or ask for
    reg  [9:0]  j = 10'd0;                  // the next word to come back
    reg  [3:0]  n = 4'd0;                   // words of this run still to come
    reg  [15:0] err = 16'd0;
    reg  [3:0]  k = 4'd0;                   // the report's character
    reg  [1:0]  bl = 2'd0;                  // byte writes: the lane
    assign wbe = bytew ? (4'b0001 << bl) : 4'hF;
    function [31:0] pat;
        input [9:0] x;
        pat = {~x, 6'b101101, x, 6'b010011} ^ {x[3:0], 28'h5A3C96E};
    endfunction
    assign wd  = pat(i);
    assign len = we ? 5'd1 : rlen;
    assign pv  = st == S_P;
    function [7:0] hexc;
        input [3:0] h;
        hexc = h < 4'd10 ? 8'h30 + h : 8'h37 + h;
    endfunction
    always @* begin
        case (k)
            4'd0: pc = 8'h0A; 4'd1: pc = "C"; 4'd2: pc = "A"; 4'd3: pc = "P"; 4'd4: pc = " ";
            4'd5: pc = 8'h30 + cap; 4'd6: pc = " "; 4'd7: pc = "E"; 4'd8: pc = "R"; 4'd9: pc = "R";
            4'd10: pc = " "; 4'd11: pc = hexc(err[15:12]); 4'd12: pc = hexc(err[11:8]);
            4'd13: pc = hexc(err[7:4]); default: pc = hexc(err[3:0]);
        endcase
    end
    always @(posedge clk) begin
        cap_we <= 1'b0;
        if (rvalid && st == S_RW) begin
            if (rdata != pat(j)) err <= err + 1'b1;
            j <= j + 1'b1;
            n <= n - 1'b1;
        end
        case (st)
            S_IDLE: if (go) begin st <= S_W; i <= 10'd0; bl <= 2'd0; we <= 1'b1; req <= 1'b1; addr <= BASE; cap <= 3'd2; cap_we <= 1'b1; end
            S_W: if (ack) begin
                    bl <= bl + 1'b1;
                    if (!bytew || bl == 2'd3) begin
                        bl <= 2'd0;
                        i <= i + 1'b1; addr <= addr + 1'b1;
                        if (i == 10'd1023) begin req <= 1'b0; we <= 1'b0; st <= S_R; end
                    end
                 end
            S_R: begin                      // a run of 8
                    if (i == 10'd1023 || st == S_R) begin end
                    i <= 10'd0; j <= 10'd0; err <= 16'd0; addr <= BASE;
                    req <= 1'b1; n <= rlen[3:0]; st <= S_RW;
                 end
            S_RW: begin
                    if (ack) begin req <= 1'b0; i <= i + rlen; addr <= addr + rlen; end
                    if (!req && !ack && n == 4'd0) begin
                        if (i == 10'd0) begin st <= S_P; k <= 4'd0; end   // all 1024 (i wrapped)
                        else begin req <= 1'b1; n <= rlen[3:0]; end
                    end
                 end
            S_P: if (pr) begin
                    k <= k + 1'b1;
                    if (k == 4'd14) begin
                        if (cap == 3'd4) begin cap <= 3'd3; cap_we <= 1'b1; st <= S_IDLE; end
                        else begin cap <= cap + 1'b1; cap_we <= 1'b1; st <= S_SET; end
                    end
                 end
            S_SET: st <= S_R;               // (the new capture in place)
            default: st <= S_IDLE;
        endcase
    end
endmodule
