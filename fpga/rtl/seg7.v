`timescale 1ns/1ps
// Seven-segment decoder: a hex digit to segments {g, f, e, d, c, b, a},
// active high (A, b, C, d, E, F above 9).
module seg7 (
    input  wire [3:0] digit,
    output reg  [6:0] glyph
);
    always @*
        case (digit)
            4'd0: glyph = 7'h3F;
            4'd1: glyph = 7'h06;
            4'd2: glyph = 7'h5B;
            4'd3: glyph = 7'h4F;
            4'd4: glyph = 7'h66;
            4'd5: glyph = 7'h6D;
            4'd6: glyph = 7'h7D;
            4'd7: glyph = 7'h07;
            4'd8: glyph = 7'h7F;
            4'd9: glyph = 7'h6F;
            4'hA: glyph = 7'h77;
            4'hB: glyph = 7'h7C;
            4'hC: glyph = 7'h39;
            4'hD: glyph = 7'h5E;
            4'hE: glyph = 7'h79;
            default: glyph = 7'h71;
        endcase
endmodule
