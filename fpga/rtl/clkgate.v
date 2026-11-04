`timescale 1ns/1ps
// T1's clock: clk with only the rising edges that end a cycle in which ce
// was high (BUFGCE switches while the clock is low; measured on the board).
module clkgate (
    input  wire clk,
    input  wire ce,
    output wire gclk
);
`ifdef SIM
    reg en = 1'b0;
    always @(clk or ce) if (!clk) en = ce;
    assign gclk = clk & en;
`else
    BUFGCE b (.I(clk), .CE(ce), .O(gclk));
`endif
endmodule
