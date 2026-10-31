// T1 register file: r0-r15 plus two hidden temporaries (the multiplier's
// accumulator), stored as 36 bytes. Byte address = register * 2 + byte, so
// pair pN byte k is 4N + k and the temporaries are bytes 32-35.
//
// Two read ports and one write port, one byte each. Reads are asynchronous;
// the write happens at the clock edge, so a byte read in the same cycle
// sees the old value. In silicon this is an 18-row by 16-bit RAM with
// a byte column select (docs/t1.md counts it as a macro); here it is
// behavioural, and the transistor count script treats it as a black box.

`default_nettype none

module t1_rf (
    input  wire       clk,
    input  wire [5:0] ra,
    input  wire [5:0] rb,
    output wire [7:0] qa,
    output wire [7:0] qb,
    input  wire       we,
    input  wire [5:0] wa,
    input  wire [7:0] wd
);
    reg [7:0] mem [0:35];

    integer i;
    initial for (i = 0; i < 36; i = i + 1) mem[i] = 8'h00;

    assign qa = mem[ra];
    assign qb = mem[rb];

    always @(posedge clk)
        if (we)
            mem[wa] <= wd;
endmodule

`default_nettype wire
