`timescale 1ns/1ps
// The Spartan-3 Starter Board's SRAM (2 x IS61LV25616AL-10: 256K x 32,
// asynchronous, 10 ns) as a burst PHY for rammux.v: one access a 50 MHz
// clock, reads and writes, at what the chips and the pins allow.
//
// A command taken at edge E puts its address (and a write's data) out
// through the I/O block registers at E.
//   read:  the data is on the pins from ~20 ns until ~29 ns (the next
//          address, ~5 ns after the next edge, plus the SRAM's 3 ns hold).
//          It is taken on clk_cap's rising edge after E + 20 ns (its DCM
//          phase sets where: aim ~25 ns, the middle) and passed to clk at
//          E + 40 ns: rvalid three clocks after the command's edge.
//   write (wmode 0, one a clock): WE# is low from a rising edge of clk_we
//          to its falling edge, 10 ns later: aim from ~8 ns to ~18 ns, inside
//          the address's 20 ns and clear of the next one. Address and data
//          hold to the next edge.
//   write (wmode 1, two clocks, the safe one): WE# is low from the clock's
//          middle to the next middle; the address and data hold two clocks.
// When the data bus changes direction (a read after a write, or a write
// after a read) the PHY takes one clock off (phy_ready low).
//
// FLASH = 1: a parallel NOR flash (SST39VF1601C, 70 ns) on the A1 expansion
// connector shares the address, OE#, WE# and the low data byte (IC10
// IO0-IO7) with the SRAM, with its own CE# and A18. A command with xa[1]
// set is a flash cycle on the flash word at {xa[0], addr}, its low byte on
// data bits 7:0 (the rest of the bus is undefined on a read). Both SRAMs
// are deselected for it. It takes a clock off first (the last SRAM read's
// capture is clear of the flash's outputs), then at E0:
//   read:  CE#, OE# low at E0; captured on clk_cap after E5 (~100 ns),
//          rvalid at E6, as an SRAM read's three clocks later
//   write: address and data from E0, WE# low from the middle of E0 to the
//          middle of E3 (60 ns; 40 needed), the address 90 ns past WE#'s
//          fall (30 needed), the data 60 ns before its rise (30 needed)
// and lets go at E6. The next command goes out at E8: the flash takes up
// to 20 ns to let go of the bus.
module sram_phy #(
    parameter FLASH = 0
) (
    input  wire        clk,                 // 50 MHz
    input  wire        clk_cap,             // read capture (phase set by its DCM)
    input  wire        clk_we,              // write strobe (phase set by its DCM)
    input  wire        wmode,               // 0: a write a clock; 1: two clocks each
    // from rammux
    input  wire        cmd,
    input  wire        we,
    input  wire [17:0] addr,
    input  wire [1:0]  xa,                  // FLASH: {a flash cycle, its A18}
    input  wire [31:0] wdata,
    input  wire [3:0]  be,
    output wire        ready,
    output reg         rvalid = 1'b0,
    output reg  [31:0] rdata = 32'd0,
    // pins
    output reg  [17:0] sram_a = 18'd0,
    inout  wire [31:0] sram_d,
    output reg         sram_oe_n = 1'b0,
    output wire        sram_we_n,
    output reg  [1:0]  sram_ce_n = 2'b00,
    output reg  [3:0]  sram_be_n = 4'b0000,
    output reg         flash_ce_n = 1'b1,
    output reg         flash_a18 = 1'b0
);
    wire fl = FLASH && xa[1];               // this command is a flash cycle
    reg  [2:0] fc = 3'd0;                   // a flash cycle's clocks still to go
    reg        fwr = 1'b0;                  // it is a write
    wire       fbusy = FLASH && fc != 3'd0;

    // write data and its output enable in each pin's I/O block registers
    // (no slices; the enable kept per pin, as the I/O block's tristate
    // register, 1 = off, so the 32 copies must not be merged)
    (* IOB = "TRUE" *) reg [31:0] dq = 32'd0;
    (* IOB = "TRUE", equivalent_register_removal = "no" *) reg [31:0] tri_n = 32'hFFFFFFFF;
    genvar gi;
    generate
        for (gi = 0; gi < 32; gi = gi + 1) begin : pins
            assign sram_d[gi] = tri_n[gi] ? 1'bz : dq[gi];
        end
    endgenerate

    // ---- direction and the clock off
    reg  last_wr = 1'b0;                    // the bus's last use was a write
    reg  second = 1'b0;                     // wmode 1: the write's second clock
    reg  turn = 1'b0;                       // this clock is the turnaround
    wire flip = cmd && (we != last_wr || fl);   // a flash cycle: always a clock off first
    assign ready = !second && !(flip && !turn) && !fbusy;

    // ---- issuing
    reg  wr_now = 1'b0;                     // a write was put out at the last edge
    reg  v1 = 1'b0;                         // a read put out at the last edge
    always @(posedge clk) begin
        v1 <= 1'b0; wr_now <= 1'b0; turn <= 1'b0; second <= 1'b0;
        if (fbusy) begin
            // a flash cycle under way (E1-E7): the data at E4 (v1), let go at E6
            fc <= fc - 3'd1;
            if (fc == 3'd4 && !fwr) v1 <= 1'b1;
            if (fc == 3'd2) begin
                flash_ce_n <= 1'b1; sram_ce_n <= 2'b00; sram_oe_n <= 1'b1;
            end
        end else if (cmd && flip && !turn && !second) begin
            // the turnaround: neither side drives the bus
            turn <= 1'b1; sram_oe_n <= 1'b1; tri_n <= 32'hFFFFFFFF;
        end else if (second) begin
            // wmode 1: hold the write
        end else if (cmd && fl) begin
            // a flash cycle's E0
            sram_a <= addr; flash_a18 <= xa[0]; last_wr <= we;
            flash_ce_n <= 1'b0; sram_ce_n <= 2'b11; fc <= 3'd7; fwr <= we;
            if (we) begin
                sram_oe_n <= 1'b1; dq <= wdata; tri_n <= 32'd0;
            end else begin
                sram_oe_n <= 1'b0; tri_n <= 32'hFFFFFFFF;
            end
        end else if (cmd) begin
            sram_a <= addr;
            last_wr <= we;
            if (we) begin
                sram_oe_n <= 1'b1; sram_be_n <= ~be; dq <= wdata; tri_n <= 32'd0;
                wr_now <= 1'b1;
                if (wmode) second <= 1'b1;
            end else begin
                sram_oe_n <= 1'b0; sram_be_n <= 4'b0000; tri_n <= 32'hFFFFFFFF; v1 <= 1'b1;
            end
        end else if (!last_wr) begin
            tri_n <= 32'hFFFFFFFF;
        end
    end

    // ---- read data: on clk_cap, then on clk two edges after the command
    // (the read put out at E: captured at E + 20 ns + clk_cap's phase,
    // handed over at E + 40 ns)
    (* IOB = "TRUE" *) reg [31:0] cap = 32'd0;
    always @(posedge clk_cap) cap <= sram_d;
    reg  v2 = 1'b0;
    always @(posedge clk) begin
        v2     <= v1;
        rvalid <= v2;
        rdata  <= cap;
    end

    // ---- WE#
    // wmode 0: a pulse from clk_we's rising edge to its falling edge, for
    // each write (two toggles, compared: no glitch, back-to-back writes work)
    reg  ta = 1'b0, tb = 1'b0;
    always @(posedge clk_we) if (wr_now && !wmode) ta <= ~ta;
    always @(negedge clk_we) tb <= ta;
    // wmode 1: low from the write's first middle to its second
    reg  we1_n = 1'b1;
    always @(negedge clk) we1_n <= !(wmode && wr_now);
    // a flash write: low from the middle of E0 to the middle of E3
    reg  fwe_n = 1'b1;
    always @(negedge clk) fwe_n <= !(FLASH && fwr && fc >= 3'd5);
    assign sram_we_n = (wmode ? we1_n : !(ta ^ tb)) && fwe_n;
endmodule
