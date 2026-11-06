// A Sega Genesis (Mega Drive) pad on the console's DE9, for system_tb.v: its
// six lines from the select line TH (active low; 1 = not pressed). BUTTONS
// as fpga/lib/pad.h: bit 0 Up, 1 Down, 2 Left, 3 Right, 4 B, 5 C, 6 A,
// 7 Start, 8 Z, 9 Y, 10 X, 11 Mode (1 = pressed).
//
//   TH  TR     TL  RIGHT LEFT DOWN UP
//   1   C      B   Right Left Down Up
//   0   Start  A   0     0    Down Up       (TH's 1st and 2nd low)
//   0   Start  A   0     0    0    0        6-button: the 3rd low
//   1   C      B   Mode  X    Y    Z        6-button: the high after it
//   0   Start  A   1     1    1    1        6-button: the 4th low
//
// A 6-button pad counts TH's falling edges until TH has stayed still for
// 1.5 ms. The lines follow TH after DELAY ns.
`timescale 1ns/1ps
module genesis_pad #(
    parameter SIX = 1,
    parameter [11:0] BUTTONS = 12'd0,
    parameter DELAY = 300
) (
    input  wire       th,
    output wire [5:0] lines                 // {TR, TL, RIGHT, LEFT, DOWN, UP}
);
    reg [2:0] falls = 3'd0;
    time      last = 0;
    always @(th) begin
        if ($time - last > 1500000) falls = 3'd0;
        last = $time;
        if (!th && falls != 3'd7) falls = falls + 3'd1;
    end
    // (a timeout while TH stays put: the next edge sees it)
    wire [11:0] p = ~BUTTONS;               // active low
    reg  [5:0]  v;
    always @* begin
        if (th) begin
            if (SIX && falls == 3'd3) v = {p[5], p[4], p[11], p[10], p[9], p[8]};
            else                      v = {p[5], p[4], p[3], p[2], p[1], p[0]};
        end else begin
            if (SIX && falls == 3'd3)      v = {p[7], p[6], 4'b0000};
            else if (SIX && falls == 3'd4) v = {p[7], p[6], 4'b1111};
            else                           v = {p[7], p[6], 2'b00, p[1], p[0]};
        end
    end
    assign #DELAY lines = v;
endmodule
