`timescale 1ns/1ps
// Seven-segment decoder: a BCD digit to segments {g, f, e, d, c, b, a},
// active high. Anything above 9 shows a dash.
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
            default: glyph = 7'h40;
        endcase
endmodule
