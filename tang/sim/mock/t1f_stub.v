`timescale 1ns/1ps
// Stand-in for t1f_core (same ports): replays the access stream made by trace2mock.c, one bus
// access per T1 period, and checks every byte read against the value the program saw.
// Compiled instead of verilog/rtl/t1f_core.v, so tang/rtl/system.v runs unchanged around it:
// its cache, rammux, SDRAM PHY, video, audio engine and I/O are the real ones.
//   +trace=FILE  the stream      +idle=N  extra idle periods after each instruction (default 0)
//   +tick=NS     tick period (default 19950000: PAL play rate)
module t1f_core #(parameter EXT_X = 1, parameter TRSIZE = 2097152) (
    input  wire        clk, ce, rst,
    input  wire [23:0] reset_pc,
    output reg  [23:0] addr = 24'd0,
    output reg         mem_rd = 1'b0,
    output reg         mem_wr = 1'b0,
    output reg  [7:0]  dout = 8'd0,
    input  wire [7:0]  din,
    output wire [1:0]  sr_sel,
    input  wire [15:0] sr_in,
    output reg         halted = 1'b0,
    output reg  [7:0]  halt_code = 8'd0,
    output reg         retire = 1'b0
);
    assign sr_sel = 2'd0;
    reg [39:0] tr [0:TRSIZE-1];
    integer idx = 0, idle_n = 0, idle_cfg = 0, tick_ns = 19950000;
    integer errors = 0, reads = 0, writes = 0, insts = 0, ticks_seen = 0, first_err_t = 0;
    reg done = 1'b0, started = 1'b0, pend = 1'b0;
    reg [7:0] pexp = 8'd0;
    reg [23:0] paddr = 24'd0;
    reg [1023:0] fn;
    real t0 = 0.0;
    reg [39:0] e;
    integer n;
    initial begin
        if (!$value$plusargs("trace=%s", fn)) begin $display("t1f_stub: need +trace=FILE"); $finish; end
        if (!$value$plusargs("idle=%d", idle_cfg)) idle_cfg = 0;
        if (!$value$plusargs("tick=%d", tick_ns)) tick_ns = 19950000;
        for (n = 0; n < TRSIZE; n = n + 1) tr[n] = 40'h0400000000;
        $readmemh(fn, tr);
    end
    always @(posedge clk) begin
        retire <= 1'b0;
        if (rst) begin
            idx = 0; idle_n = 0; pend = 1'b0; mem_rd <= 1'b0; mem_wr <= 1'b0; addr <= 24'd0;
        end else begin
            if (!started) begin started = 1'b1; t0 = $realtime; end
            if (pend) begin
                if (din !== pexp) begin
                    errors = errors + 1;
                    if (errors <= 10) $display("[cpu] %0t: read %06x gave %02x, expected %02x", $time, paddr, din, pexp);
                    if (errors == 1) first_err_t = $time;
                end
                pend = 1'b0;
            end
            mem_rd <= 1'b0; mem_wr <= 1'b0; addr <= 24'd0;
            e = tr[idx];
            if (done) ;
            else if (idle_n > 0) idle_n = idle_n - 1;
            else if (e[35:32] == 4'd3) begin       // wait for tick e[31:8]
                if ($realtime - t0 >= 1.0 * tick_ns * e[31:8] + 20000.0) begin idx = idx + 1; ticks_seen = ticks_seen + 1; end
            end else if (e[35:32] == 4'd4) done = 1'b1;
            else begin
                addr <= e[31:8];
                if (e[35:32] == 4'd0 || e[35:32] == 4'd1) begin
                    mem_rd <= 1'b1; pend = 1'b1; pexp = e[7:0]; paddr = e[31:8]; reads = reads + 1;
                end else begin
                    mem_wr <= 1'b1; dout <= e[7:0]; writes = writes + 1;
                end
                if (e[36]) begin idle_n = idle_cfg; insts = insts + 1; retire <= 1'b1; end
                idx = idx + 1;
                // a prologue I/O write ends no instruction: no idle
            end
        end
    end
endmodule
