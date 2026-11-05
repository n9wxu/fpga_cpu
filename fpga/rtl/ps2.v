`timescale 1ns/1ps
// One PS/2 port (docs/io.md "PS/2 ports"): bytes received in hardware;
// sending is software's, on the lines. Clock and data are open drain:
// clk_lo / dat_lo pull the line low, the pin's pull-up (or the device's)
// takes it high.
//
// Receive: the clock line, filtered (three equal samples at samp), shifts
// the data line into sr at each falling edge; sr starts all ones and the
// frame is whole when its start bit (0) reaches sr[0]. A frame with a bad
// stop bit or parity is dropped, and so is a part frame when the clock has
// been still for 7 line ticks (about 155 us). A whole frame stays in sr[8:1]
// (valid) until a read takes it, and the clock is held low meanwhile, so the
// device waits with the next one (PS/2 inhibit) and nothing is overrun.
//
// Send (software, fpga/lib/ps2.c): ctl (a STAT write) bit 0 holds the clock
// low, bit 1 the data line, and bit 2 turns the receiver off (it would take
// the host's own frame); line reads the filtered clock and the data line.
module ps2port (
    input  wire       clk,
    input  wire       samp,                 // the filter's sample strobe (about 1.3 us)
    input  wire       ltick,                // a scan line (22.2 us)
    input  wire       clk_in, dat_in,       // the lines
    output wire       clk_lo, dat_lo,       // pull them low
    input  wire       ctl_wr,               // set ctl from wd
    input  wire [2:0] wd,
    input  wire       take,                 // the byte was read
    output wire       valid,                // a byte is waiting: rd
    output wire [7:0] rd,
    output wire [1:0] line,                 // {data, clock}
    output reg        rx = 1'b0             // a byte arrived (a pulse: the interrupt)
);
    reg [1:0] cs = 2'b11, ds = 2'b11;
    reg [2:0] cf = 3'b111;
    reg       cl = 1'b1, cl_d = 1'b1;
    always @(posedge clk) begin
        cs <= {cs[0], clk_in};
        ds <= {ds[0], dat_in};
        if (samp) begin
            cf <= {cf[1:0], cs[1]};
            if (cf == 3'b111) cl <= 1'b1;
            if (cf == 3'b000) cl <= 1'b0;
        end
        cl_d <= cl;
    end
    wire fall = cl_d && !cl;

    reg [2:0]  ctl = 3'b000;
    reg [10:0] sr = 11'h7FF;
    reg [2:0]  cnt = 3'd0;
    wire good = sr[10] && ^sr[9:1];         // the stop bit, odd parity

    assign valid  = !sr[0] && good;
    assign rd     = sr[8:1];
    assign line   = {ds[1], cl};
    assign clk_lo = ctl[0] || valid;
    assign dat_lo = ctl[1];

    reg valid_d = 1'b0;
    always @(posedge clk) begin
        valid_d <= valid;
        rx <= valid && !valid_d;
        if (ctl_wr) ctl <= wd;
        if (fall) cnt <= 3'd0;
        else if (ltick && cnt != 3'd7) cnt <= cnt + 1'b1;
        if (ctl[2]) begin
            sr <= 11'h7FF;
        end else if (!sr[0]) begin
            if (take || !good) sr <= 11'h7FF;
        end else if (fall) begin
            sr <= {ds[1], sr[10:1]};
        end else if (cnt == 3'd7)
            sr <= 11'h7FF;
    end
endmodule
