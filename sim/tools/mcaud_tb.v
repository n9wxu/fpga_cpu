`timescale 1ns/1ps
// The microcoded audio engine (fpga/rtl/mcaudio, docs/mcaudio.md) with the real rammux over a
// behavioural RAM (read latency, and a hog on the higher-priority port), the control store loaded
// through the CPU port, the node list and samples from a RAM image; the output taken at 48 kHz and
// written one "L R" line a sample. fpga/tools/mctest.py writes the inputs and checks the output.
// Plusargs: +dir=DIR (cs.hex, ram.hex, head.txt; writes out.txt, ram_out.hex), +n=samples, +top=words.
// (A copy of fpga/sim/mc_tb.v for sim/tools/mcaud_check.py: after the samples it stops the engine, lets the
// block in progress finish, and dumps the RAM, the counters and the whole monitor for the C model.)
module mc_tb;
    localparam AW = 20;
    reg clk = 1'b0;
    always #7.4074 clk = ~clk;              // 67.5 MHz

    reg         io_wr = 1'b0;
    reg  [15:0] io_addr = 16'd0;
    reg  [7:0]  io_wdata = 8'd0;
    reg  [15:0] rd_addr = 16'd0;
    wire [15:0] rd_data;
    wire        irq;
    wire        a_req, a_we, a_ack, a_rvalid, a_wnext;
    wire [AW-1:0] a_addr;
    wire [4:0]  a_len;
    wire [31:0] a_wdata;
    wire [3:0]  a_wbe;
    wire        s_valid;
    wire [15:0] s_l, s_r;
    reg         s_ready = 1'b0;
    wire [31:0] p_rdata;

    mc_engine #(.AW(AW)) dut (
        .clk(clk), .io_wr(io_wr), .io_addr(io_addr), .io_wdata(io_wdata),
        .rd_addr(rd_addr), .rd_data(rd_data), .irq(irq),
        .p_req(a_req), .p_we(a_we), .p_addr(a_addr), .p_len(a_len), .p_ack(a_ack),
        .p_rvalid(a_rvalid), .p_rdata(p_rdata), .p_wdata(a_wdata), .p_wbe(a_wbe), .p_wnext(a_wnext),
        .s_valid(s_valid), .s_l(s_l), .s_r(s_r), .s_ready(s_ready));

    // a hog on port 0
    reg         h_req = 1'b0;
    reg  [AW-1:0] h_addr = 20'h80000;
    reg  [7:0]  h_cnt = 8'd0;
    wire        h_ack, h_rvalid;
    always @(posedge clk) begin
        h_cnt <= h_cnt + 8'd1;
        if (h_ack) h_req <= 1'b0;
        if (h_cnt == 8'd0 && !h_req) begin h_req <= 1'b1; h_addr <= h_addr + 20'd4099; end
    end

    wire phy_cmd, phy_we, phy_ready, phy_rvalid;
    wire [AW-1:0] phy_addr;
    wire [31:0] phy_wdata, phy_rdata;
    wire [3:0]  phy_be;
    wire [1:0]  ack, rv, wn;
    assign h_ack = ack[0]; assign a_ack = ack[1];
    assign h_rvalid = rv[0]; assign a_rvalid = rv[1]; assign a_wnext = wn[1];
    rammux #(.NP(2), .AW(AW), .LW(5), .WMASK(2'b10)) mux (
        .clk(clk), .p_req({a_req, h_req}), .p_we({a_we, 1'b0}),
        .p_addr({a_addr, h_addr}), .p_len({a_len, 5'd16}),
        .p_ack(ack), .p_wdata({a_wdata, 32'd0}), .p_wbe({a_wbe, 4'd0}), .p_wnext(wn),
        .p_rvalid(rv), .p_rdata(p_rdata),
        .phy_cmd(phy_cmd), .phy_we(phy_we), .phy_addr(phy_addr), .phy_wdata(phy_wdata), .phy_be(phy_be),
        .phy_ready(phy_ready), .phy_rvalid(phy_rvalid), .phy_rdata(phy_rdata));

    reg [31:0] ram [0:(1<<AW)-1];
    assign phy_ready = 1'b1;
    reg [AW-1:0] pa [0:15];
    reg [15:0]   pv = 16'd0;
    integer k;
    always @(posedge clk) begin
        for (k = 15; k > 0; k = k - 1) pa[k] <= pa[k-1];
        pa[0] <= phy_addr;
        pv <= {pv[14:0], phy_cmd && !phy_we};
        if (phy_cmd && phy_we) ram[phy_addr] <= phy_wdata;
    end
    assign phy_rvalid = pv[4];
    assign phy_rdata = ram[pa[4]];

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
            repeat (3) @(posedge clk);
            v = rd_data;
        end
    endtask

    reg [71:0] csm [0:511];
    reg [31:0] headv [0:1];
    reg [4095:0] dir;
    integer n, i, got, fo, tcnt, maxclk, top, ctrl;
    reg [15:0] v16, v16b, nnodes;
    initial begin
        if (!$value$plusargs("dir=%s", dir)) dir = ".";
        if (!$value$plusargs("n=%d", n)) n = 960;
        if (!$value$plusargs("ctrl=%d", ctrl)) ctrl = 1;
        for (i = 0; i < (1 << AW); i = i + 1) ram[i] = 32'd0;
        $readmemh({dir, "/cs.hex"}, csm);
        $readmemh({dir, "/ram.hex"}, ram);
        $readmemh({dir, "/head.txt"}, headv);
        fo = $fopen({dir, "/out.txt"}, "w");
        wr16(16'h1008, 16'd0);
        for (i = 0; i < 512; i = i + 1) begin
            wr16(16'h100A, csm[i][15:0]);  wr16(16'h100C, csm[i][31:16]);
            wr16(16'h100E, csm[i][47:32]); wr16(16'h1010, csm[i][63:48]);
            wr16(16'h1012, {8'd0, csm[i][71:64]});
        end
        wr16(16'h1000, headv[0][15:0]); wr16(16'h1002, headv[0][31:16]);
        wr16(16'h1006, headv[1][15:0]);     // tempo
        wr16(16'h1004, ctrl[15:0]);         // run, output shift (+ctrl=, default 1)
        got = 0; tcnt = 0; maxclk = 0;
        while (got < n) begin
            @(posedge clk);
            tcnt = tcnt + 1;
            s_ready <= 1'b0;
            if (dut.st_clk > maxclk) maxclk = dut.st_clk;
            if (tcnt >= 1406) begin
                s_ready <= 1'b1;
                if (s_valid && s_ready) begin
                    $fwrite(fo, "%0d %0d\n", $signed(s_l), $signed(s_r));
                    got = got + 1; tcnt = 0; s_ready <= 1'b0;
                end
            end
        end
        $fclose(fo);
        wr16(16'h1004, ctrl[15:0] & 16'hFFFE);   // stop (keeping the output shift); the block in progress finishes
        repeat (150000) @(posedge clk);
        rd16(16'h1014, v16); $display("IRQ %h", v16);
        rd16(16'h1016, v16); $display("ID %h", v16);
        rd16(16'h1018, v16); $display("BLOCKS %0d", v16);
        rd16(16'h101A, v16); $display("UNDERRUNS %0d", v16);
        rd16(16'h101C, v16); $display("CLOCKS %0d MAX %0d", v16, maxclk);
        rd16(16'h101E, v16); $display("BURSTS %0d", v16);
        rd16(16'h1020, v16); $display("WORDS %0d", v16);
        rd16(16'h1022, nnodes); $display("NODES %0d", nnodes);
        rd16(16'h1024, v16); $display("RING %0d", v16);
        for (i = 0; i < 288; i = i + 1) begin
            wr16(16'h1024, i[15:0]);
            rd16(16'h1026, v16); rd16(16'h1028, v16b);
            $display("MON %0d %h%h", i, v16b, v16);
        end
        if (!$value$plusargs("top=%d", top)) top = 1024;
        $writememh({dir, "/ram_out.hex"}, ram, 0, top - 1);
        $finish;
    end
endmodule
