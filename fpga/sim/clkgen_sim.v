`timescale 1ns/1ps
// Simulation stand-in for rtl/clkgen.v (Icarus has no DCM model).
module clkgen (
    input  wire clk_in,
    output wire clk50,
    output reg  clk_pix = 0,
    output wire locked
);
    // clk50 lags the pin by 1 ns, so that T1's gated clock (made from the
    // pin in simulation, see system.v) samples before the 50 MHz logic
    // changes, as it does in hardware, where clock-to-output delays decide.
    assign #1 clk50 = clk_in;
    assign locked = 1'b1;
    always #6.734 clk_pix = ~clk_pix;    // 74.25 MHz
endmodule
