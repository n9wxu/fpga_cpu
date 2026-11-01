`timescale 1ns/1ps
// The board's SRAM for simulation: two 256K x 16 asynchronous chips with a
// shared address, WE# and OE#, each with its own CE#, UB# and LB#
// (IS61LV25616AL-10). Reads are combinational; a write happens while WE#
// and CE# are low (level-sensitive, so a write that ends on the same edge
// as the data driver is still captured). Read access time is modelled as
// 10 ns.
//
// $readmemh preloads 32-bit words: +image=FILE (word address 0 up).
module sram_model (
    input  wire [17:0] a,
    inout  wire [31:0] d,
    input  wire        oe_n,
    input  wire        we_n,
    input  wire [1:0]  ce_n,
    input  wire [3:0]  be_n
);
    reg [31:0] mem [0:262143];
    integer i;
    reg [1023:0] image;
    initial begin
        for (i = 0; i < 262144; i = i + 1) mem[i] = 32'h0;
        if ($value$plusargs("image=%s", image)) $readmemh(image, mem);
    end

    wire rd0 = !ce_n[0] && !oe_n && we_n;
    wire rd1 = !ce_n[1] && !oe_n && we_n;
    wire [31:0] q = mem[a];
    assign #10 d[7:0]   = (rd0 && !be_n[0]) ? q[7:0]   : 8'bz;
    assign #10 d[15:8]  = (rd0 && !be_n[1]) ? q[15:8]  : 8'bz;
    assign #10 d[23:16] = (rd1 && !be_n[2]) ? q[23:16] : 8'bz;
    assign #10 d[31:24] = (rd1 && !be_n[3]) ? q[31:24] : 8'bz;

    always @* begin
        if (!we_n && !ce_n[0] && !be_n[0]) mem[a][7:0]   = d[7:0];
        if (!we_n && !ce_n[0] && !be_n[1]) mem[a][15:8]  = d[15:8];
        if (!we_n && !ce_n[1] && !be_n[2]) mem[a][23:16] = d[23:16];
        if (!we_n && !ce_n[1] && !be_n[3]) mem[a][31:24] = d[31:24];
    end
endmodule
