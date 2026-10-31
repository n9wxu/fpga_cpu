// Decode check for t1_core: reads "ir p2" pairs (hex) from +vec=FILE, forces
// them into the core's instruction registers and prints "ir p2 illegal"
// for each, where illegal=1 means the decoder sends it to the FE trap.
// t1dec.py compares that with the ISA tables (cpu/isa.yaml).

`timescale 1ns / 1ns
`default_nettype none

module t1_dec_tb;
    parameter EXT_X = 1;

    reg clk = 1'b0, rst = 1'b1;
    wire [23:0] addr;
    wire        mem_rd, mem_wr, halted, retire;
    wire [7:0]  dout, halt_code;
    wire [1:0]  sr_sel;

    t1_core #(.EXT_X(EXT_X)) core (.clk(clk), .rst(rst), .reset_pc(24'h001000), .addr(addr), .mem_rd(mem_rd),
                  .mem_wr(mem_wr), .dout(dout), .din(8'h00), .sr_sel(sr_sel), .sr_in(16'h0000),
                  .halted(halted), .halt_code(halt_code), .retire(retire));

    reg [8*512-1:0] path;
    integer f, n;
    reg [15:0] a, b;
    initial begin
        if (!$value$plusargs("vec=%s", path)) begin $display("usage: +vec=FILE"); $finish; end
        f = $fopen(path, "r");
        while (!$feof(f)) begin
            n = $fscanf(f, "%h %h\n", a, b);
            if (n == 2) begin
                core.ir = a; core.p2 = b;
                #1;
                $display("%04x %04x %0d", a, b, core.illegal ? 1 : 0);
            end
        end
        $finish;
    end
endmodule

`default_nettype wire
