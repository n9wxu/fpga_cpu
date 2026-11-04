`timescale 1ns/1ps
// One PS/2 port (docs/io.md "PS/2 ports"): whole bytes each way, the device
// left to software. Clock and data are open drain: clk_lo / dat_lo pull the
// line low, the pin's pull-up (or the device's) takes it high.
//
// Receive: the clock line, filtered (three equal samples at samp), shifts
// the data line into sr at each falling edge; sr starts all ones and the
// frame is whole when its start bit (0) reaches sr[0]. A frame with a bad
// stop bit or parity is dropped, and so is a part frame when the clock has
// been still for 7 line ticks (about 155 us). A whole frame stays in sr[8:1]
// (valid) until a read takes it, and the clock is held low meanwhile, so the
// device waits with the next one (PS/2 inhibit) and nothing is overrun.
//
// Send (a write): the clock is held low for 7 line ticks with the data line
// low (the request to send), then released; at each of the device's falling
// edges the next bit goes out (data 0-7, odd parity, the stop bit); the
// device's eleventh edge is its acknowledge, and the port is idle again.
// A device that does not clock within two frame ticks (17-33 ms) ends the
// send with err set.
module ps2port (
    input  wire       clk,
    input  wire       samp,                 // the filter's sample strobe (about 1.3 us)
    input  wire       ltick,                // a scan line (22.2 us)
    input  wire       ftick,                // a frame (16.7 ms)
    input  wire       clk_in, dat_in,       // the lines
    output wire       clk_lo, dat_lo,       // pull them low
    input  wire       wr,                   // send wd
    input  wire [7:0] wd,
    input  wire       take,                 // the byte was read
    output wire       valid,                // a byte is waiting: rd
    output wire [7:0] rd,
    output wire       busy,                 // sending
    output reg        err = 1'b0,           // the last send found no device
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

    reg [10:0] sr = 11'h7FF;
    reg        tx = 1'b0, inh = 1'b0, q = 1'b1;
    reg [2:0]  cnt = 3'd0;
    wire tick = (tx && !inh) ? ftick : ltick;
    wire good = sr[10] && ^sr[9:1];         // the stop bit, odd parity

    assign valid  = !tx && !sr[0] && good;
    assign rd     = sr[8:1];
    assign busy   = tx;
    assign clk_lo = (tx && inh) || valid;
    assign dat_lo = !q;

    reg valid_d = 1'b0;
    always @(posedge clk) begin
        valid_d <= valid;
        rx <= valid && !valid_d;
        if (fall) cnt <= 3'd0;
        else if (tick && cnt != 3'd7) cnt <= cnt + 1'b1;
        if (wr) begin
            tx <= 1'b1; inh <= 1'b1; q <= 1'b0; err <= 1'b0; cnt <= 3'd0;
            sr <= {1'b1, 1'b1, ~^wd, wd};       // the end marker, stop, parity, data
        end else if (tx) begin
            if (inh) begin
                if (cnt == 3'd7) begin inh <= 1'b0; cnt <= 3'd0; end
            end else if (fall) begin
                if (sr == 11'd1) begin tx <= 1'b0; q <= 1'b1; sr <= 11'h7FF; end   // the acknowledge
                else begin q <= sr[0]; sr <= {1'b0, sr[10:1]}; end
            end else if (cnt == 3'd2) begin
                tx <= 1'b0; q <= 1'b1; err <= 1'b1; sr <= 11'h7FF;
            end
        end else if (!sr[0]) begin
            if (take || !good) sr <= 11'h7FF;
        end else if (fall) begin
            sr <= {ds[1], sr[10:1]};
        end else if (cnt == 3'd7)
            sr <= 11'h7FF;
    end
endmodule
