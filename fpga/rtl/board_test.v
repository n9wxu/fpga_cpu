`timescale 1ns/1ps
// Board bring-up test for the Digilent Spartan-3 Starter Board.
//
//   display  seconds since reset, 0000-9999, counting at 1 Hz
//   VGA      1280x720 60 Hz: the fpga_cpu's 80x25 text layer, 640x350
//            shown 2x, with a demo screen and the same count
//   LD0-LD7  a Larson scanner: one bright LED sweeping back and forth,
//            with a fading trail (PWM)
//   BTN0     reset
//
// Both the anodes and the segments of the display are active low.
module board_test #(
    parameter CLK_HZ     = 50_000_000,
    parameter STEP_TICKS = CLK_HZ / 16,  // scanner step: 1/16 s, ~0.9 s per round trip
    parameter SCAN_BITS  = 18,           // all four digits every 2^SCAN_BITS clocks (190 Hz)
    parameter PWM_SHIFT  = 6             // PWM period 16 << PWM_SHIFT clocks (49 kHz)
) (
    input  wire       clk,               // 50 MHz oscillator
    input  wire       btn0,              // pushbutton, active high
    output wire [7:0] led,
    output reg  [3:0] an  = 4'b1111,     // digit anodes; an[3] is the leftmost digit
    output reg  [7:0] seg = 8'hFF,       // {dp, g, f, e, d, c, b, a}
    output wire       vga_r, vga_g, vga_b, vga_hs, vga_vs
);
    // clk50 drives everything here; clk_pix is the 1280x720 pixel clock.
    wire clk50, clk_pix, locked;
    clkgen clocks (.clk_in(clk), .clk50(clk50), .clk_pix(clk_pix), .locked(locked));

    // The button is asynchronous: synchronize it before using it as a reset.
    reg [1:0] btn_s = 2'b00;
    always @(posedge clk50) btn_s <= {btn_s[0], btn0};
    wire rst = btn_s[1];

    // ---- 1 Hz decimal counter -------------------------------------------
    reg [25:0] sec_div = 0;
    wire sec_tick = (sec_div == CLK_HZ - 1);
    always @(posedge clk50)
        if (rst || sec_tick) sec_div <= 0;
        else                 sec_div <= sec_div + 1'b1;

    reg [3:0] d0 = 0, d1 = 0, d2 = 0, d3 = 0;   // BCD, d0 is the ones digit
    always @(posedge clk50)
        if (rst) begin
            d0 <= 0; d1 <= 0; d2 <= 0; d3 <= 0;
        end else if (sec_tick) begin
            d0 <= (d0 == 9) ? 4'd0 : d0 + 1'b1;
            if (d0 == 9) begin
                d1 <= (d1 == 9) ? 4'd0 : d1 + 1'b1;
                if (d1 == 9) begin
                    d2 <= (d2 == 9) ? 4'd0 : d2 + 1'b1;
                    if (d2 == 9)
                        d3 <= (d3 == 9) ? 4'd0 : d3 + 1'b1;
                end
            end
        end

    // ---- display multiplexing ---------------------------------------------
    reg [SCAN_BITS-1:0] refresh = 0;
    always @(posedge clk50) refresh <= refresh + 1'b1;
    wire [1:0] sel = refresh[SCAN_BITS-1 -: 2];

    reg [3:0] digit;
    always @*
        case (sel)
            2'd0: digit = d0;
            2'd1: digit = d1;
            2'd2: digit = d2;
            default: digit = d3;
        endcase

    wire [6:0] glyph;                    // {g, f, e, d, c, b, a}, active high
    seg7 dec (.digit(digit), .glyph(glyph));

    // Registered outputs, so the pins change together and never glitch.
    always @(posedge clk50) begin
        an  <= ~(4'b0001 << sel);
        seg <= {1'b1, ~glyph};           // decimal point off
    end

    // ---- VGA --------------------------------------------------------------
    // The count is written into the text layer's cells the way the CPU will
    // write them: one digit per clock, round robin, through the RAM's second
    // port. COUNT_ADDR is row 17, column 52 (tools/mktext.py's COUNT_ROW/COL).
    localparam [10:0] COUNT_ADDR = 17 * 80 + 52;
    reg  [1:0] wk = 0;
    always @(posedge clk50) wk <= wk + 1'b1;
    wire [3:0] wdig = (wk == 0) ? d3 : (wk == 1) ? d2 : (wk == 2) ? d1 : d0;

    video720 vga (
        .clk(clk_pix),
        .wclk(clk50), .we(1'b1), .waddr(COUNT_ADDR + {9'd0, wk}),
        .wdata({2'b00, 3'd0, 3'd6, 4'h3, wdig}),   // '0'-'9', yellow on black
        .hsync(vga_hs), .vsync(vga_vs), .rgb({vga_r, vga_g, vga_b})
    );

    // ---- Larson scanner ---------------------------------------------------
    reg [22:0] step_div = 0;
    wire step = (step_div == STEP_TICKS - 1);
    always @(posedge clk50)
        if (rst || step) step_div <= 0;
        else             step_div <= step_div + 1'b1;

    reg [2:0] pos = 0;
    reg       down = 0;                  // moving towards LD0
    wire      turn = down ? (pos == 0) : (pos == 7);
    wire      ndown = turn ? ~down : down;
    wire [2:0] npos = ndown ? pos - 1'b1 : pos + 1'b1;

    // 4-bit brightness per LED; every step halves them all (15, 7, 3, 1, 0)
    // and lights the new position fully, leaving a fading trail.
    reg [31:0] bright = 0;
    integer i;
    always @(posedge clk50)
        if (rst) begin
            pos <= 0; down <= 0; bright <= 0;
        end else if (step) begin
            pos  <= npos;
            down <= ndown;
            for (i = 0; i < 8; i = i + 1)
                bright[4*i +: 4] <= (i == npos) ? 4'd15 : {1'b0, bright[4*i+1 +: 3]};
        end

    reg [PWM_SHIFT+3:0] pwm = 0;
    always @(posedge clk50) pwm <= pwm + 1'b1;
    wire [3:0] level = pwm[PWM_SHIFT+3 -: 4];

    genvar g;
    generate
        for (g = 0; g < 8; g = g + 1) begin : drive
            reg on = 0;
            always @(posedge clk50) on <= bright[4*g +: 4] > level;
            assign led[g] = on;
        end
    endgenerate
endmodule
