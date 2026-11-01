`timescale 1ns/1ps
// Clocks from the 50 MHz oscillator: the oscillator itself through a global
// buffer, and the 1280x720 pixel clock, exactly 74.25 MHz.
//
// A Spartan-3 DCM multiplies by at most 32, so 50 x 297/200 takes two in a
// row: 50 x 27/20 = 67.5 MHz, then x 11/10 = 74.25 MHz. (One DCM's closest,
// 50 x 3/2 = 75 MHz, is 1% fast; the Samsung 2343BWX takes it but the
// picture bounces vertically.) The second DCM is held in reset until the
// first has locked, as Xilinx asks for cascaded DCMs.
//
// sim/clkgen_sim.v stands in for this in Icarus.
module clkgen (
    input  wire clk_in,                  // 50 MHz pin
    output wire clk50,
    output wire clk_pix,                 // 74.25 MHz
    output wire locked
);
    wire clk_ibuf, fx1, fx1_buf, fx2, locked1, locked2;

    IBUFG ibuf (.I(clk_in), .O(clk_ibuf));
    BUFG  b50  (.I(clk_ibuf), .O(clk50));

    DCM #(
        .CLKIN_PERIOD(20.0),
        .CLKFX_MULTIPLY(27),
        .CLKFX_DIVIDE(20),
        .CLK_FEEDBACK("NONE"),
        .DFS_FREQUENCY_MODE("LOW"),
        .STARTUP_WAIT("FALSE")
    ) dcm1 (
        .CLKIN(clk_ibuf), .CLKFB(1'b0), .RST(1'b0),
        .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0), .DSSEN(1'b0),
        .CLKFX(fx1), .LOCKED(locked1)
    );
    BUFG  b1   (.I(fx1), .O(fx1_buf));

    // Hold the second DCM in reset until the first has been locked for
    // 16 clocks.
    reg [15:0] rst2 = 16'hFFFF;
    always @(posedge clk50) rst2 <= locked1 ? {rst2[14:0], 1'b0} : 16'hFFFF;

    DCM #(
        .CLKIN_PERIOD(14.815),
        .CLKFX_MULTIPLY(11),
        .CLKFX_DIVIDE(10),
        .CLK_FEEDBACK("NONE"),
        .DFS_FREQUENCY_MODE("LOW"),
        .STARTUP_WAIT("FALSE")
    ) dcm2 (
        .CLKIN(fx1_buf), .CLKFB(1'b0), .RST(rst2[15]),
        .PSCLK(1'b0), .PSEN(1'b0), .PSINCDEC(1'b0), .DSSEN(1'b0),
        .CLKFX(fx2), .LOCKED(locked2)
    );
    BUFG  bpix (.I(fx2), .O(clk_pix));

    assign locked = locked1 && locked2;
endmodule
