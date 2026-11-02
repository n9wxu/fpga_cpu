`timescale 1ns/1ps
// Tang Nano 20K flow check: a binary count on the six LEDs, about 3 Hz on
// LED0; S1 holds it.
module blinky (
    input  wire       clk,          // 27 MHz
    input  wire       s1,           // button, high when pressed
    output wire [5:0] led           // active low
);
    reg [27:0] n = 0;
    always @(posedge clk) if (!s1) n <= n + 1'b1;
    assign led = ~n[27:22];
endmodule
