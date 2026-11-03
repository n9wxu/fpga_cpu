`timescale 1ns/1ps
// The Tang Nano 20K's SDRAM (in the GW2AR-18's package: 64 Mbit, 32 bits
// wide, 4 banks x 2048 rows x 256 columns) as a burst PHY for
// ../fpga/rtl/rammux.v, at 67.5 MHz (14.8 ns). The SDRAM's clock is ours
// shifted 180 degrees: commands and write data change at our rising edge
// and the SDRAM samples them half a period later. CAS latency 2, burst
// length 1; each bank's row stays open until a different row or a refresh
// needs it closed. Refresh every 7.4 us (8192 rows' worth per 64 ms).
//
// rammux's side: a command (cmd, we, addr, wdata, be) is taken at each
// edge where ready is high. ready looks at this clock's command: it is low
// while the PHY opens the command's row (PRECHARGE, ACTIVE), refreshes, or
// spaces a read (below), and high when the command's READ or WRITE goes out
// at this edge. A read's word comes back with rvalid, CAP clocks after its
// READ, in order.
//
// The timing found on the board (sdram.v had it first): read data is taken
// from the pins at the CAP'th (3rd) rising edge after the READ. Reads one
// after another, a clock apart, sometimes gave the next word, so a read
// never follows a read directly: reads stream at one every two clocks
// (135 MB/s). Writes stream at one a clock; a write waits until the bus is
// clear of read data. Capture at the falling edge, an earlier SDRAM clock,
// or the input register in the I/O block (nextpnr ioreg_in_iob) all failed.
//
// rdata holds from rvalid until the next read's rvalid, at least two
// clocks later (the reads' spacing): a client may use it in the clock
// after rvalid.
module sdram_phy #(
    parameter CAP = 3,
    parameter INIT_WAIT = 15000             // 222 us of power-up wait
) (
    input  wire        clk,
    // from rammux
    input  wire        cmd,
    input  wire        we,
    input  wire [20:0] addr,                // word address
    input  wire [31:0] wdata,
    input  wire [3:0]  be,
    output wire        ready,
    output reg         rvalid = 1'b0,
    output reg  [31:0] rdata = 32'd0,
    output reg         init_done = 1'b0,
    // the SDRAM
    output wire        sd_cke,
    output wire        sd_cs_n,
    output wire        sd_ras_n,
    output wire        sd_cas_n,
    output wire        sd_we_n,
    output reg  [10:0] sd_a = 11'd0,
    output reg  [1:0]  sd_ba = 2'd0,
    output reg  [3:0]  sd_dqm = 4'hF,
    inout  wire [31:0] sd_dq
);
    localparam [3:0] C_NOP = 4'b0111, C_ACT = 4'b0011, C_READ = 4'b0101, C_WRITE = 4'b0100,
                     C_PRE = 4'b0010, C_REF = 4'b0001, C_MRS = 4'b0000;
    localparam T_RP = 2, T_RCD = 2, T_RFC = 6, T_MRD = 2;

    reg  [3:0]  sc = C_NOP;
    reg         cke = 1'b0;
    assign {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} = sc;
    assign sd_cke = cke;

    reg  [31:0] dq_o = 32'd0;
    reg         dq_oe = 1'b0;
    assign sd_dq = dq_oe ? dq_o : 32'bz;

    // ---- this clock's command ---------------------------------------------------------
    wire [7:0]  col = addr[7:0];
    wire [1:0]  bnk = addr[9:8];
    wire [10:0] row = addr[20:10];

    reg  [3:0]  open = 4'b0000;             // a row is open in the bank
    reg  [10:0] orow [0:3];
    wire        hit = open[bnk] && orow[bnk] == row;

    // ---- timing ------------------------------------------------------------------------
    reg  [2:0]  wait_ = 3'd0;               // clocks to wait before the next command
    reg  [2:0]  since_act = 3'd7, since_wr = 3'd7, since_rd = 3'd7;
    wire        can_pre = since_act >= 3'd3 && since_wr >= 3'd2;
    reg  [9:0]  ref_t = 10'd0;
    reg         ref_due = 1'b0;

    // ---- initialization -----------------------------------------------------------------
    reg  [15:0] init_t = INIT_WAIT;
    reg  [3:0]  init_s = 4'd0;              // 0 wait, 1 precharge, 2-9 refresh, 10 mode, 11 done

    // ---- decide ------------------------------------------------------------------------
    wire go      = init_done && wait_ == 3'd0 && !ref_due;
    // a write waits for the bus to be free of read data; a read never
    // follows a read directly
    wire spaced  = we ? since_rd >= 3'd4 : since_rd != 3'd0;
    assign ready = go && hit && spaced;
    wire issue   = cmd && ready;

    // reads in flight, one bit a stage; the data is taken from the pins at
    // the edge after the last stage
    reg  [CAP-1:0] t_v = {CAP{1'b0}};

    always @(posedge clk) begin
        sc     <= C_NOP;
        dq_oe  <= 1'b0;
        sd_dqm <= 4'hF;
        if (wait_ != 0) wait_ <= wait_ - 1'b1;
        if (since_act != 3'd7) since_act <= since_act + 1'b1;
        if (since_wr  != 3'd7) since_wr  <= since_wr + 1'b1;
        if (since_rd  != 3'd7) since_rd  <= since_rd + 1'b1;

        // refresh timer
        if (ref_t == 10'd499) begin ref_t <= 0; ref_due <= 1'b1; end
        else ref_t <= ref_t + 1'b1;

        // read data
        t_v    <= {t_v[CAP-2:0], issue && !we};
        rvalid <= t_v[CAP-1];
        if (t_v[CAP-1]) rdata <= sd_dq;

        if (!init_done) begin
            // power-up: wait, precharge all, 8 refreshes, mode register
            if (init_t != 0) init_t <= init_t - 1'b1;
            else if (wait_ == 0) begin
                case (init_s)
                    4'd0: begin cke <= 1'b1; init_s <= 4'd1; wait_ <= 3'd2; end
                    4'd1: begin sc <= C_PRE; sd_a[10] <= 1'b1; wait_ <= T_RP - 1; init_s <= 4'd2; end
                    4'd10: begin
                        sc <= C_MRS; sd_ba <= 2'b00;
                        sd_a <= 11'b0_0_00_010_0_000;   // burst 1, sequential, CAS 2
                        wait_ <= T_MRD - 1; init_s <= 4'd11;
                    end
                    4'd11: init_done <= 1'b1;
                    default: begin sc <= C_REF; wait_ <= T_RFC - 1; init_s <= init_s + 1'b1; end
                endcase
            end
        end else if (wait_ != 0) begin
            // tRP, tRCD, tRFC
        end else if (ref_due) begin
            if (open != 0) begin
                if (can_pre) begin
                    sc <= C_PRE; sd_a[10] <= 1'b1; open <= 4'b0000; wait_ <= T_RP - 1;
                end
            end else begin
                sc <= C_REF; wait_ <= T_RFC - 1; ref_due <= 1'b0;
            end
        end else if (issue) begin
            sc    <= we ? C_WRITE : C_READ;
            sd_ba <= bnk;
            sd_a  <= {3'b000, col};         // A10 low: no auto-precharge
            if (we) begin
                dq_o   <= wdata;
                dq_oe  <= 1'b1;
                sd_dqm <= ~be;
                since_wr <= 3'd0;
            end else begin
                sd_dqm   <= 4'h0;
                since_rd <= 3'd0;
            end
        end else if (cmd && !hit) begin
            if (open[bnk]) begin
                if (can_pre) begin
                    sc <= C_PRE; sd_ba <= bnk; sd_a[10] <= 1'b0;
                    open[bnk] <= 1'b0; wait_ <= T_RP - 1;
                end
            end else begin
                sc <= C_ACT; sd_ba <= bnk; sd_a <= row;
                open[bnk] <= 1'b1; orow[bnk] <= row;
                since_act <= 3'd0; wait_ <= T_RCD - 1;
            end
        end
    end
endmodule
