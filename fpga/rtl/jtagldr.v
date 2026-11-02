`timescale 1ns/1ps
// Program loading and status over the FPGA's JTAG port (BSCAN_SPARTAN3).
//
//   USER2 (IR 0x03), 32-bit data register: control, when bit 30 is set.
//     Bit 31 = 0: hold the CPU in reset and set the load pointer to bits
//     19:0 (a byte address, multiple of 4).
//     Bit 31 = 1: release the CPU from reset at bits 23:0.
//     Bit 29 = 1 (with bit 31 = 0): also read the SRAM word at bits 19:0;
//     the next USER2 scan shifts it out. With bit 28 also set, read entry
//     bits 10:2 of the bus trace (system.v) instead.
//     It shifts out the last word read; a scan with bit 30 clear does
//     nothing else.
//   USER1 (IR 0x02): data. Every 32 bits shifted in (bit 0 first) are
//     written to SRAM at the load pointer, which then advances by 4. A
//     shift of any multiple of 32 bits streams many words at once.
//   USER1 shifts out the status word captured at Capture-DR:
//     {0, halted, running, 5'b0, halt_code[7:0], instructions[15:0]}.
//     A 31-bit USER1 scan reads it without writing anything.
//
// fpga/tools/jtagload.py writes the OpenOCD commands for an image.
// In simulation (SIM defined) there is no JTAG: a testbench sets run and
// entry directly.
module jtagldr (
    input  wire        clk,                 // 50 MHz
    input  wire [31:0] status,
    output reg         run = 1'b0,
    output reg  [23:0] entry = 24'h001000,
    output reg         ldr_req = 1'b0,
    output reg         ldr_rd = 1'b0,
    input  wire [31:0] ldr_q,
    output reg         trace_rd = 1'b0,
    output reg  [17:0] ldr_addr = 18'd0,
    output reg  [31:0] ldr_data = 32'd0,
    input  wire        ldr_done
);
`ifndef SIM
    wire capture, drck1_l, drck2_l, drck1, drck2, sel1, sel2, shift, tdi, update, jreset;
    reg  [31:0] sr1 = 32'd0, sr2 = 32'd0;
    reg  [4:0]  bc = 5'd0;
    reg  [31:0] word = 32'd0;
    reg         wtgl = 1'b0;

    BSCAN_SPARTAN3 bscan (
        .CAPTURE(capture), .DRCK1(drck1_l), .DRCK2(drck2_l), .RESET(jreset),
        .SEL1(sel1), .SEL2(sel2), .SHIFT(shift), .TDI(tdi), .UPDATE(update),
        .TDO1(sr1[0]), .TDO2(sr2[0])
    );

    // DRCK1 and DRCK2 on global buffers: on local routing their skew lets
    // shift-register bits race (seen as bits read back out of place).
    BUFG bd1 (.I(drck1_l), .O(drck1));
    BUFG bd2 (.I(drck2_l), .O(drck2));

    // USER1: the data stream (DRCK1 runs only in Capture-DR and Shift-DR).
    // On this board a scan's bits arrive one position late relative to this
    // count: word n is bits 32n-1 .. 32n+30 of the scan, and the first word's
    // bit 0 is not from the scan at all. tools/jtagload.py allows for it.
    always @(posedge drck1)
        if (capture) begin
            sr1 <= status;
            bc  <= 5'd0;
        end else if (shift) begin
            sr1 <= {tdi, sr1[31:1]};
            bc  <= bc + 1'b1;
            if (bc == 5'd31) begin
                word <= {tdi, sr1[31:1]};
                wtgl <= ~wtgl;
            end
        end

    // USER2: control
    always @(posedge drck2)
        if (capture)    sr2 <= ldr_q;
        else if (shift) sr2 <= {tdi, sr2[31:1]};

    // into the 50 MHz domain: word and sr2 are stable long after their toggles
    reg [2:0] w_s = 3'b000, u_s = 3'b000;
    reg [19:2] ptr = 18'd0;
    always @(posedge clk) begin
        w_s <= {w_s[1:0], wtgl};
        u_s <= {u_s[1:0], update & sel2};
        if (w_s[2] != w_s[1]) begin
            ldr_addr <= ptr;
            ldr_data <= word;
            ldr_req  <= 1'b1;
            ldr_rd   <= 1'b0;
            trace_rd <= 1'b0;
            ptr      <= ptr + 1'b1;
        end else if (ldr_done)
            ldr_req <= 1'b0;
        if (u_s[2:1] == 2'b01) begin
            if (sr2[30]) begin
                if (sr2[31]) begin
                    entry <= sr2[23:0];
                    run   <= 1'b1;
                    trace_rd <= 1'b0;
                end else begin
                    run <= 1'b0;
                    ptr <= sr2[19:2];
                    if (sr2[29]) begin
                        ldr_addr <= sr2[19:2];
                        ldr_req  <= !sr2[28];
                        ldr_rd   <= 1'b1;
                        trace_rd <= sr2[28];
                    end
                end
            end
        end
    end
`endif
endmodule
