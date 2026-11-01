`timescale 1ns/1ps
// Simulation stand-in for rtl/clkgen.v (Icarus has no DCM model).
module clkgen (
    input  wire clk_in,
    output wire clk50,
    output reg  clk_pix = 0,
    output wire locked
);
    assign clk50  = clk_in;
    assign locked = 1'b1;
    always #6.734 clk_pix = ~clk_pix;    // 74.25 MHz
endmodule
