`timescale 1ns/1ps
// RAM-machine mock: tang/rtl/system.v (real cache, rammux, sdram_phy, video, audio engine, I/O)
// with the T1 core replaced by t1f_stub.v (replays sidplay/Glass Bells' bus accesses), on the
// real sdram_model, plus a scoreboard on the PHY interface and latency/lateness monitors.
//   +trace=FILE +image=HEXFILE +frames=N [+idle=N] [+tick=NS]
module mock_tb;
    reg clk = 0;
    always #7.407 clk = ~clk;                       // fclk 67.5 MHz
    wire        sd_clk, sd_cke, sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n;
    wire [10:0] sd_a;  wire [1:0] sd_ba;  wire [3:0] sd_dqm;  wire [31:0] sd_dq;
    wire        tx;  wire [5:0] led;
    system dut (.clk(clk), .s1(1'b0), .s2(1'b0), .uart_rx(1'b1), .uart_tx(tx), .led(led), .tmds_p(), .tmds_n(),
                .O_sdram_clk(sd_clk), .O_sdram_cke(sd_cke), .O_sdram_cs_n(sd_cs_n),
                .O_sdram_ras_n(sd_ras_n), .O_sdram_cas_n(sd_cas_n), .O_sdram_wen_n(sd_we_n),
                .O_sdram_addr(sd_a), .O_sdram_ba(sd_ba), .O_sdram_dqm(sd_dqm), .IO_sdram_dq(sd_dq));
    sdram_model sdm (.clk(sd_clk), .cke(sd_cke), .cs_n(sd_cs_n), .ras_n(sd_ras_n), .cas_n(sd_cas_n),
                     .we_n(sd_we_n), .a(sd_a), .ba(sd_ba), .dqm(sd_dqm), .dq(sd_dq));

    // ---- start: the SDRAM holds the program's memory at the window's start; T1 is released
    reg [31:0] shadow [0:2097151];
    reg [1023:0] imgf;
    integer k, frames_want = 10;
    initial begin
        for (k = 0; k < 2097152; k = k + 1) shadow[k] = 32'd0;
        if (!$value$plusargs("image=%s", imgf)) begin $display("need +image=FILE"); $finish; end
        if (!$value$plusargs("frames=%d", frames_want)) frames_want = 10;
        #10;
        $readmemh(imgf, sdm.mem);
        $readmemh(imgf, shadow);
        #1000 dut.run_req = 1'b1;                   // as the loader's 'G' would (the cache clears first)
    end

    // ---- scoreboard on the PHY's input: every read word, every port
    // A write updates the shadow when its command is taken; a read queues the shadow's word
    // and the issuing port; the word that comes back is compared.
    reg [31:0] q_exp [0:63];  reg [3:0] q_port [0:63];  reg [20:0] q_adr [0:63];
    integer qw = 0, qr = 0;
    integer words_chk [0:3];
    integer data_err = 0, first_err_cyc = 0;
    integer cyc = 0;
    wire [3:0] sel_oh = dut.mux.sel;
    initial for (k = 0; k < 4; k = k + 1) words_chk[k] = 0;
    wire [31:0] wmask_w = {{8{dut.phy_be[3]}}, {8{dut.phy_be[2]}}, {8{dut.phy_be[1]}}, {8{dut.phy_be[0]}}};
    always @(posedge clk) begin
        cyc <= cyc + 1;
        if (dut.mux.issue) begin
            if (dut.phy_we) shadow[dut.phy_addr] = (shadow[dut.phy_addr] & ~wmask_w) | (dut.phy_wdata & wmask_w);
            else begin q_exp[qw % 64] = shadow[dut.phy_addr]; q_port[qw % 64] = sel_oh; q_adr[qw % 64] = dut.phy_addr; qw = qw + 1; end
        end
        if (dut.phy_rvalid) begin
            if (dut.phy_rdata !== q_exp[qr % 64]) begin
                data_err = data_err + 1;
                if (data_err <= 10) $display("[mock] cycle %0d: port %b word %06x gave %08x expected %08x",
                                             cyc, q_port[qr % 64], q_adr[qr % 64], dut.phy_rdata, q_exp[qr % 64]);
                if (data_err == 1) first_err_cyc = cyc;
            end
            for (k = 0; k < 4; k = k + 1) if (q_port[qr % 64][k]) words_chk[k] = words_chk[k] + 1;
            qr = qr + 1;
        end
    end

    // ---- grant latency per port: request seen to ack (fclk cycles, 1 = granted at once)
    integer wst [0:3], lat_max [0:3], lat_n [0:3], lat_sum [0:3];
    reg [3:0] w = 4'b0;
    integer p;
    initial for (p = 0; p < 4; p = p + 1) begin lat_max[p] = 0; lat_n[p] = 0; lat_sum[p] = 0; wst[p] = 0; end
    always @(posedge clk) for (p = 0; p < 4; p = p + 1) begin
        if (dut.mux.p_req[p]) begin
            if (!w[p]) begin w[p] = 1'b1; wst[p] = cyc; end
            if (dut.p_ack[p]) begin
                lat_n[p] = lat_n[p] + 1; lat_sum[p] = lat_sum[p] + (cyc - wst[p] + 1);
                if (cyc - wst[p] + 1 > lat_max[p]) lat_max[p] = cyc - wst[p] + 1;
                w[p] = 1'b0;
            end
        end else w[p] = 1'b0;
    end

    // ---- video: late lines, and how long each line's fetch takes (rq -> idle), vs a line's 3000 clocks
    integer late_n = 0, vstart = 0, vmax = 0, vlines = 0, vsum = 0, vseen = 0;
    reg vact = 1'b0;
    always @(posedge clk) begin
        if (dut.v_late) begin late_n = late_n + 1; if (late_n <= 5) $display("[mock] cycle %0d: video LATE", cyc); end
        if (dut.vid.rq) begin vact = 1'b1; vseen = 0; vstart = cyc; end
        else if (vact) begin
            if (dut.vid.st != 2'd0) vseen = 1;
            else if (vseen) begin
                vact = 1'b0; vlines = vlines + 1; vsum = vsum + (cyc - vstart);
                if (cyc - vstart > vmax) vmax = cyc - vstart;
            end
        end
    end
    // frames (pixel clock) and audio FIFO underruns (pixel clock)
    integer frames = 0, under_n = 0;
    reg under_d = 1'b0;
    always @(posedge dut.pclk) begin
        if (dut.vid.hc == 0 && dut.vid.vc == 0) frames = frames + 1;
        under_d <= dut.a_under;
        if (dut.a_under && !under_d) under_n = under_n + 1;
    end
    // audio engine: its DMA port and frame rate
    integer aud_req_n = 0;
    always @(posedge clk) if (dut.p_ack[2]) aud_req_n = aud_req_n + 1;

    // ---- run and report
    integer last_frame = 0;
    always @(posedge dut.pclk)
        if (frames != last_frame) begin
            last_frame = frames;
            $display("[mock] frame %0d at %0t: cpu errors %0d, data errors %0d, late %0d, sdram errors %0d, cpu reads %0d writes %0d insts %0d",
                     frames, $time, dut.cpu.errors, data_err, late_n, sdm.errors, dut.cpu.reads, dut.cpu.writes, dut.cpu.insts);
            if (frames > frames_want) report;
        end
    task report;
        integer i;
        begin
            $display("==== mock result: %0d video frames (%0t), %0d fclk cycles", frames - 1, $time, cyc);
            $display("CPU stream: %0d insts, %0d reads checked, %0d writes, tick waits passed %0d; CPU data errors %0d",
                     dut.cpu.insts, dut.cpu.reads, dut.cpu.writes, dut.cpu.ticks_seen, dut.cpu.errors);
            $display("SDRAM words checked by the scoreboard: port0(cpu) %0d  port1(video) %0d  port2(audio) %0d  port3(bist) %0d; data errors %0d",
                     words_chk[0], words_chk[1], words_chk[2], words_chk[3], data_err);
            for (i = 0; i < 3; i = i + 1)
                $display("port %0d: %0d bursts, grant latency worst %0d avg %0d fclk", i, lat_n[i], lat_max[i], lat_n[i] ? lat_sum[i] / lat_n[i] : 0);
            $display("video: late %0d, %0d line fetches, fetch time worst %0d avg %0d fclk (a line is ~3000)",
                     late_n, vlines, vmax, vlines ? vsum / vlines : 0);
            $display("audio: DMA bursts %0d, HDMI audio FIFO underruns %0d", aud_req_n, under_n);
            $display("SDRAM model errors %0d", sdm.errors);
            $finish;
        end
    endtask
    initial begin #4000000000; $display("[mock] timeout"); report; end
    // +words=FILE: dvi_tx's three 10-bit channel words, one line a pixel clock
    // ("ch0 ch1 ch2", hex), for tools/hdmi_audio_check.py (NOTONE=1)
    reg [1023:0] wfile;
    integer wf = 0;
    initial if ($value$plusargs("words=%s", wfile)) wf = $fopen(wfile, "w");
    always @(posedge dut.pclk)
        if (wf != 0) $fwrite(wf, "%h %h %h\n", dut.tx.words[0], dut.tx.words[1], dut.tx.words[2]);
    // +con=FILE: the console's bytes as they enter the transmit queue
    reg [1023:0] cfile;
    integer cf = 0;
    initial if ($value$plusargs("con=%s", cfile)) cf = $fopen(cfile, "w");
    always @(posedge dut.fclk)
        if (cf != 0 && dut.txq_we && !dut.txq_full) $fwrite(cf, "%c", dut.txq_d);
endmodule
