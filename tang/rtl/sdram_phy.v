`timescale 1ns/1ps
// The Tang Nano 20K's SDRAM (in the GW2AR-18's package: 64 Mbit, 32 bits
// wide, 4 banks x 2048 rows x 256 columns) as a burst PHY for
// ../fpga/rtl/rammux.v, at 67.5 MHz (14.8 ns). The SDRAM's clock is ours
// from the same PLL, in phase by default (system.v SD_PSDA): commands and
// write data change at our rising edge and the SDRAM takes them at its next
// one. (At 180 degrees, the earlier setting, burst reads put the word at the
// edge of the capture window and failed on some placements.) CAS latency 2. Each
// bank's row stays open until a different row or a refresh needs it closed.
// Refresh every 7.4 us (8192 rows' worth per 64 ms).
//
// Reads are SDRAM bursts of 8 (sequential, within an 8-word block); writes
// are single words (mode register A9). rammux hands over a run one word a
// clock; the PHY issues a READ for the run's first word and at each 8-word
// boundary, and takes the words between as the burst brings them (one a
// clock: 270 MB/s). Words of a burst nobody asked for are masked with DQM,
// so the SDRAM does not drive them; a later command (READ, WRITE, PRE) ends
// the burst.
//
// A write's bank and column go on the pins a clock before its WRITE (a NOP
// with the address): the address lines and the data lines switch at
// different edges, which halves the current of the largest step.
//
// The data bus is driven whenever no read data can be on it, and holds the
// last value it carried (written, or read: the read data becomes the value
// held, so driving the bus again after a read changes nothing): a write
// changes only its enabled byte lanes, and the bus is let go only for reads.
// Fewer of the 32 lines switch: their switching, in the package, upset the
// HDMI output (the TV dropped audio and picture during SDRAM traffic).
//
// rammux's side: a command (in_cmd, in_we, in_addr, in_wdata, in_be) is
// taken at each edge where ready is high (the input queue has room). The
// head of the queue goes out when it can: not while the PHY opens its row
// (PRECHARGE, ACTIVE), refreshes, puts a write's address out, or waits for
// read data to clear the bus before a write. A read's word comes back with
// rvalid, in order.
//
// cap (2, 3 or 4) is where read data is taken from the pins: the cap'th
// rising edge after the word's slot. It is an input so it can be found on
// the board (with the SDRAM clock's phase, constraints/system: SD_PSDA).
module sdram_phy #(
    parameter INIT_WAIT = 15000             // 222 us of power-up wait
) (
    input  wire        clk,
    input  wire [2:0]  cap,                 // read capture: 2, 3 or 4 clocks
    // from rammux
    input  wire        in_cmd,
    input  wire        in_we,
    input  wire [20:0] in_addr,             // word address
    input  wire [31:0] in_wdata,
    input  wire [3:0]  in_be,
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

    // ---- the input queue -------------------------------------------------------------
    // Two commands deep: rammux's ready is "not full", from a register, and
    // the decisions below start from the queue's registers, so rammux's
    // grant and the PHY's compares are not in one clock. (A clock of
    // latency; a run still goes a word a clock.)
    reg  [57:0] q0 = 58'd0, q1 = 58'd0;     // {we, addr, wdata, be}
    reg  [1:0]  qn = 2'd0;
    assign ready = qn != 2'd2;
    wire        push = in_cmd && ready;
    wire        cmd = qn != 2'd0;
    wire        we = q0[57];
    wire [20:0] addr = q0[56:36];
    wire [31:0] wdata = q0[35:4];
    wire [3:0]  be = q0[3:0];
    wire        pop;
    always @(posedge clk) begin
        case ({push, pop})
            2'b10: begin
                if (qn == 2'd0) q0 <= {in_we, in_addr, in_wdata, in_be};
                else q1 <= {in_we, in_addr, in_wdata, in_be};
                qn <= qn + 1'b1;
            end
            2'b01: begin q0 <= q1; qn <= qn - 1'b1; end
            2'b11: begin
                if (qn == 2'd1) q0 <= {in_we, in_addr, in_wdata, in_be};
                else begin q0 <= q1; q1 <= {in_we, in_addr, in_wdata, in_be}; end
            end
            default: ;
        endcase
    end

    // ---- the command at the head of the queue ------------------------------------------
    wire [7:0]  col = addr[7:0];
    wire [1:0]  bnk = addr[9:8];
    wire [10:0] row = addr[20:10];

    // the open rows, in registers: the four banks' hits are compared with
    // the row at once and the bank picks one (this is on rammux's grant ->
    // ready -> ack path, so no RAM read and compare in series)
    reg  [3:0]  open = 4'b0000;
    reg  [10:0] orow0 = 11'd0, orow1 = 11'd0, orow2 = 11'd0, orow3 = 11'd0;
    wire [3:0]  hits = open & {orow3 == row, orow2 == row, orow1 == row, orow0 == row};
    wire        hit = hits[bnk];

    // the read burst under way: the next word it brings, and how many are left
    reg         b_on = 1'b0;
    reg  [20:0] b_next = 21'd0;
    reg  [2:0]  b_left = 3'd0;
    wire        cont = b_on && cmd && !we && addr == b_next;

    // ---- timing ------------------------------------------------------------------------
    reg  [2:0]  wait_ = 3'd0;               // clocks to wait before the next command
    reg  [2:0]  since_act = 3'd7, since_wr = 3'd7, since_rd = 3'd7;   // since_rd: since the last word asked for
    wire        can_pre = since_act >= 3'd3 && since_wr >= 3'd2;
    reg  [9:0]  ref_t = 10'd0;
    reg         ref_due = 1'b0;
    wire        pre_ok = sd_ba == bnk && sd_a == {3'b000, col};   // the write's address is out

    // ---- initialization -----------------------------------------------------------------
    reg  [15:0] init_t = INIT_WAIT;
    reg  [3:0]  init_s = 4'd0;              // 0 wait, 1 precharge, 2-9 refresh, 10 mode, 11 done

    // ---- decide ------------------------------------------------------------------------
    wire go      = init_done && wait_ == 3'd0 && !ref_due;
    wire   take  = !cmd ? 1'b0
                 : cont ? init_done && !ref_due
                 : !we  ? go && hit
                 :        go && hit && pre_ok && since_rd >= 3'd4;
    wire issue   = cmd && take;
    assign pop   = issue;
    wire rd_word = issue && !we;            // a word asked for: its slot is this clock
    wire new_rd  = rd_word && !cont;        // ... and a READ goes out for it

    // words asked for, one bit a clock; the word is taken from the pins cap
    // clocks after its slot
    reg  [3:0]  t_v = 4'd0;
    wire        cap_now = cap == 3'd2 ? t_v[1] : cap == 3'd4 ? t_v[3] : t_v[2];
    // the pins, sampled every clock by a register with nothing in front of
    // it (so it can be the I/O block's own: nextpnr --vopt ioreg_in_iob),
    // whose delay from the pin then does not depend on placement; the word
    // is taken from it a clock later
    reg  [31:0] dq_in = 32'd0;
    reg         cap_d = 1'b0;
    always @(posedge clk) dq_in <= sd_dq;

    always @(posedge clk) begin
        sc     <= C_NOP;
        sd_dqm <= 4'hF;
        if (wait_ != 0) wait_ <= wait_ - 1'b1;
        if (since_act != 3'd7) since_act <= since_act + 1'b1;
        if (since_wr  != 3'd7) since_wr  <= since_wr + 1'b1;
        if (since_rd  != 3'd7) since_rd  <= since_rd + 1'b1;
        if (init_done && since_rd >= 3'd4 && !new_rd) dq_oe <= 1'b1;   // when a write may drive it

        // refresh timer
        if (ref_t == 10'd499) begin ref_t <= 0; ref_due <= 1'b1; end
        else ref_t <= ref_t + 1'b1;

        // read data
        t_v    <= {t_v[2:0], rd_word};
        cap_d  <= cap_now;
        rvalid <= cap_d;
        if (cap_d) begin rdata <= dq_in; dq_o <= dq_in; end   // (a write cannot fall between)

        // the burst brings its next word at each edge with no command (a
        // command ends it)
        if (b_on) begin
            b_next <= b_next + 1'b1;
            b_left <= b_left - 1'b1;
            if (b_left == 3'd1) b_on <= 1'b0;
        end

        if (!init_done) begin
            // power-up: wait, precharge all, 8 refreshes, mode register
            if (init_t != 0) init_t <= init_t - 1'b1;
            else if (wait_ == 0) begin
                case (init_s)
                    4'd0: begin cke <= 1'b1; init_s <= 4'd1; wait_ <= 3'd2; end
                    4'd1: begin sc <= C_PRE; sd_a[10] <= 1'b1; wait_ <= T_RP - 1; init_s <= 4'd2; end
                    4'd10: begin
                        sc <= C_MRS; sd_ba <= 2'b00;
                        sd_a <= 11'b0_1_00_010_0_011;   // single writes, CAS 2, sequential, burst 8
                        wait_ <= T_MRD - 1; init_s <= 4'd11;
                    end
                    4'd11: init_done <= 1'b1;
                    default: begin sc <= C_REF; wait_ <= T_RFC - 1; init_s <= init_s + 1'b1; end
                endcase
            end
        end else if (issue) begin
            if (we) begin
                sc     <= C_WRITE;          // (its bank and column are out already: pre_ok)
                dq_o   <= {be[3] ? wdata[31:24] : dq_o[31:24], be[2] ? wdata[23:16] : dq_o[23:16],
                           be[1] ? wdata[15:8] : dq_o[15:8], be[0] ? wdata[7:0] : dq_o[7:0]};
                dq_oe  <= 1'b1;
                sd_dqm <= ~be;
                since_wr <= 3'd0;
                b_on   <= 1'b0;
            end else begin
                sd_dqm   <= 4'h0;           // this word out of the burst
                since_rd <= 3'd0;
                if (!cont) begin
                    sc     <= C_READ;
                    sd_ba  <= bnk;
                    sd_a   <= {3'b000, col};    // A10 low: no auto-precharge
                    dq_oe  <= 1'b0;
                    b_on   <= col[2:0] != 3'd7;
                    b_next <= addr + 1'b1;
                    b_left <= 3'd7 - col[2:0];
                end
            end
        end else if (wait_ != 0) begin
            // tRP, tRCD, tRFC
        end else if (ref_due) begin
            if (open != 0) begin
                if (can_pre) begin
                    sc <= C_PRE; sd_a[10] <= 1'b1; open <= 4'b0000; wait_ <= T_RP - 1; b_on <= 1'b0;
                end
            end else begin
                sc <= C_REF; wait_ <= T_RFC - 1; ref_due <= 1'b0;
            end
        end else if (cmd && !cont && !hit) begin
            if (open[bnk]) begin
                if (can_pre) begin
                    sc <= C_PRE; sd_ba <= bnk; sd_a[10] <= 1'b0;
                    open[bnk] <= 1'b0; wait_ <= T_RP - 1; b_on <= 1'b0;
                end
            end else begin
                sc <= C_ACT; sd_ba <= bnk; sd_a <= row;
                open[bnk] <= 1'b1;
                case (bnk)
                    2'd0: orow0 <= row;
                    2'd1: orow1 <= row;
                    2'd2: orow2 <= row;
                    default: orow3 <= row;
                endcase
                since_act <= 3'd0; wait_ <= T_RCD - 1;
            end
        end else if (cmd && we && !pre_ok) begin
            sd_ba <= bnk; sd_a <= {3'b000, col};   // the write's address, a clock early
        end
    end
endmodule
