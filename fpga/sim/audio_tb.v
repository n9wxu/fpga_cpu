`timescale 1ns/1ps
// Audio engine testbench: the real rammux with a behavioural RAM behind it
// (read latency, ready gaps) and a hog on the higher-priority port. Plusargs:
//   +script=F +mem=F +out=F    the register script, memory image, sample output
//   +slow                      the RAM gives gaps and a longer latency
//   +turbo                     the sink always takes a sample: report the clocks a frame takes
// Default (exact) mode: the engine is parked at each frame's output while the
// frame's register writes are made, so audio_ref.py's frame-by-frame model
// matches bit for bit. Script format: see fpga/tools/audio_ref.py.
module audio_tb;
    localparam AW = 20;
    reg clk = 1'b0;
    always #7.4074 clk = ~clk;              // 67.5 MHz

    // ---- engine
    reg         io_wr = 1'b0;
    reg  [15:0] io_addr = 16'd0;
    reg  [7:0]  io_wdata = 8'd0;
    reg  [15:0] rd_addr = 16'd0;
    wire [15:0] rd_data;
    wire        irq;
    wire        a_req, a_we, a_ack, a_rvalid;
    wire [AW-1:0] a_addr;
    wire [4:0]  a_len;
    wire        s_valid;
    wire [15:0] s_l, s_r;
    reg         s_ready = 1'b0;
    wire [31:0] p_rdata;

    audio_engine #(.AW(AW)) dut (
        .clk(clk), .io_wr(io_wr), .io_addr(io_addr), .io_wdata(io_wdata),
        .rd_addr(rd_addr), .rd_data(rd_data), .irq(irq),
        .p_req(a_req), .p_we(a_we), .p_addr(a_addr), .p_len(a_len), .p_ack(a_ack),
        .p_rvalid(a_rvalid), .p_rdata(p_rdata),
        .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready));

    // ---- a hog on port 0: bursts of 16 words every so often
    reg         h_req = 1'b0;
    reg  [AW-1:0] h_addr = 0;
    reg  [7:0]  h_cnt = 8'd0;
    wire        h_ack, h_rvalid;
    always @(posedge clk) begin
        h_cnt <= h_cnt + 8'd1;
        if (h_ack) h_req <= 1'b0;
        if (h_cnt == 8'd0 && !h_req) begin h_req <= 1'b1; h_addr <= h_addr + 20'd4099; end
    end

    // ---- rammux and the RAM
    wire phy_cmd, phy_we, phy_ready, phy_rvalid;
    wire [AW-1:0] phy_addr;
    wire [31:0] phy_wdata, phy_rdata;
    wire [3:0]  phy_be;
    wire [1:0]  rq = {a_req, h_req};
    wire [1:0]  ack, rv;
    assign h_ack = ack[0]; assign a_ack = ack[1];
    assign h_rvalid = rv[0]; assign a_rvalid = rv[1];
    rammux #(.NP(2), .AW(AW), .LW(5), .WMASK(2'b00)) mux (
        .clk(clk), .p_req(rq), .p_we(2'b00),
        .p_addr({a_addr, h_addr}), .p_len({a_len, 5'd16}),
        .p_ack(ack), .p_wdata(64'd0), .p_wbe(8'd0), .p_wnext(),
        .p_rvalid(rv), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));

    reg [31:0] ram [0:(1<<AW)-1];
    reg        slow = 1'b0;
    reg [15:0] rnd = 16'hACE1;
    always @(posedge clk) rnd <= {rnd[14:0], rnd[15] ^ rnd[13] ^ rnd[12] ^ rnd[10]};
    assign phy_ready = !(slow && rnd[2:0] == 3'd0);
    localparam LAT = 6;
    reg [AW-1:0] pa [0:15];
    reg [15:0]   pv = 16'd0;
    integer k;
    always @(posedge clk) begin
        for (k = 15; k > 0; k = k - 1) pa[k] <= pa[k-1];
        pa[0] <= phy_addr;
        pv <= {pv[14:0], phy_cmd && phy_ready && !phy_we};
    end
    wire [3:0] lat = slow ? 4'd6 : 4'd4;           // at most 8 reads out (rammux's tag queue)
    assign phy_rvalid = pv[lat];
    assign phy_rdata = ram[pa[lat]];

    // ---- the bus
    task wr16(input [15:0] a, input [15:0] v);
        begin
            @(posedge clk); io_addr <= a;     io_wdata <= v[7:0];  io_wr <= 1'b1;
            @(posedge clk); io_addr <= a | 1; io_wdata <= v[15:8];
            @(posedge clk); io_wr <= 1'b0;
        end
    endtask
    task rd16(input [15:0] a, output [15:0] v);
        begin
            @(posedge clk); rd_addr <= a;
            repeat (4) @(posedge clk);
            v = rd_data;
        end
    endtask

    // ---- the script
    reg [31:0] ev_f [0:262143];
    reg [31:0] ev_a [0:262143];
    reg [31:0] ev_v [0:262143];
    reg        ev_m [0:262143];
    integer nev = 0, nframes = 0, fd, r, f, a, v, fr, e, fo;
    real t0, t1;
    reg [7:0] tok;
    reg [4095:0] sname, mname, oname;
    reg [15:0] rb;
    integer pushes = 0, kk;
    always @(posedge clk) if (s_valid && s_ready) pushes <= pushes + 1;
    initial begin
        if (!$value$plusargs("script=%s", sname)) begin $display("no +script"); $finish; end
        if (!$value$plusargs("mem=%s", mname)) begin $display("no +mem"); $finish; end
        if (!$value$plusargs("out=%s", oname)) oname = "out.txt";
        slow = $test$plusargs("slow");
        $readmemh(mname, ram);
        fd = $fopen(sname, "r");
        while (!$feof(fd)) begin
            r = $fscanf(fd, " %s", tok);
            if (r == 1) begin
                if (tok == "W") begin
                    r = $fscanf(fd, " %d %h %h", f, a, v);
                    ev_f[nev] = f; ev_a[nev] = a; ev_v[nev] = v; ev_m[nev] = 1'b0; nev = nev + 1;
                end else if (tok == "M") begin
                    r = $fscanf(fd, " %d %h %h", f, a, v);
                    ev_f[nev] = f; ev_a[nev] = a; ev_v[nev] = v; ev_m[nev] = 1'b1; nev = nev + 1;
                end else if (tok == "E") r = $fscanf(fd, " %d", nframes);
            end
        end
        $fclose(fd);
        fo = $fopen(oname, "w");
        if ($test$plusargs("turbo")) begin
            // free running: clocks a frame takes
            s_ready = 1'b1;
            e = 0;
            t0 = 0;
            for (fr = 0; fr < nframes; fr = fr + 1) begin
                while (e < nev && ev_f[e] <= fr) begin if (ev_m[e]) ram[ev_a[e]] = ev_v[e]; else wr16(ev_a[e], ev_v[e]); e = e + 1; end
                while (pushes <= fr) @(posedge clk);
                if (fr == 8) t0 = $realtime;
            end
            t1 = $realtime;
            $display("turbo: %0d frames, %0.1f clocks a frame (limit 1406.25), underruns %0d",
                     nframes - 9, (t1 - t0) / 14.8148 / (nframes - 9), dut.ucount);
            $finish;
        end
        // exact mode
        @(posedge clk);
        while (!s_valid) @(posedge clk);
        e = 0;
        for (fr = 0; fr < nframes; fr = fr + 1) begin
            while (e < nev && ev_f[e] <= fr + 1) begin if (ev_m[e]) ram[ev_a[e]] = ev_v[e]; else wr16(ev_a[e], ev_v[e]); e = e + 1; end
            repeat (60) @(posedge clk);
            if (fr == nframes - 1) begin
                for (kk = 11; kk < 16; kk = kk + 1) begin rd16(16'h1800 + 2 * kk, rb); $display("RD %0d %0d", kk, rb); end
                rd16(16'h100C, rb); $display("RD g6 %0d", rb);
                rd16(16'h1010, rb); $display("RD g8 %0d", rb);
                rd16(16'h1014, rb); $display("RD g10 %0h", rb);
                rd16(16'h1018, rb); $display("RD g12 %0d", rb);
            end
            $fwrite(fo, "%0d %0d\n", $signed(s_l), $signed(s_r)); $fflush(fo);
            s_ready <= 1'b1; @(posedge clk); s_ready <= 1'b0;
            @(posedge clk);
            while (!s_valid) @(posedge clk);
        end
        $fclose(fo);
        $display("done: %0d frames, underruns %0d, irq %b", nframes, dut.ucount, irq);
        $finish;
    end
    initial begin #2000000000; $display("timeout"); $finish; end
endmodule
