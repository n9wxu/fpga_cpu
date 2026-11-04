`timescale 1ns/1ps
// Controller for the Tang Nano 20K's SDRAM (in the GW2AR-18's package):
// 64 Mbit, 32 bits wide, 4 banks x 2048 rows x 256 columns. At 67.5 MHz
// (14.8 ns), with the SDRAM's clock the controller's shifted 180 degrees:
// commands and write data change at our rising edge and the SDRAM samples
// them half a period later. CAS latency 2, burst length 1; each bank's row
// stays open until a different row or a refresh needs it closed.
//
// Two clients:
//   cpu   one byte at a time: c_req held until done. A write is done when
//         its command goes out: c_wdone is high in the cycle before that
//         edge. A read's byte is captured from the pins into c_q; c_rsoon
//         is high in the cycle before that edge.
//   video DMA, as fpga/rtl/video.v asks: d_left words from word d_addr.
//         d_take is high in a cycle whose edge issues a READ for the word
//         at d_addr; the words arrive in order with d_valid.
// The cpu goes first. Refresh every 7.4 us (8192 rows' worth per 64 ms).
//
// Read data: the READ leaves at edge e0 and the data is captured from the
// pins at e0 + CAP*T (CAP = 3). Found on the board (the clock tree delays
// our edges against the SDRAM's): reads one after another, a word apart,
// at 3T sometimes gave the next word; at 3.5T or with the SDRAM's clock
// earlier, many reads failed. Reads with a cycle between them, as here,
// had no errors. (Capture in the I/O block, nextpnr's ioreg_in_iob,
// failed at every phase.)
module sdram #(
    parameter CAP = 3,
    parameter INIT_WAIT = 15000             // 222 us of power-up wait
) (
    input  wire        clk,
    // cpu
    input  wire        c_req,
    input  wire        c_we,
    input  wire [22:0] c_addr,              // byte address
    input  wire [7:0]  c_wd,
    output wire        c_wdone,
    output wire        c_rsoon,
    output wire [7:0]  c_q,
    // video DMA
    input  wire [8:0]  d_left,
    input  wire [20:0] d_addr,              // word address
    output wire        d_take,
    output reg         d_valid = 1'b0,
    output reg  [31:0] d_data = 32'd0,
    // the SDRAM
    output wire        sd_cke,
    output wire        sd_cs_n,
    output wire        sd_ras_n,
    output wire        sd_cas_n,
    output wire        sd_we_n,
    output reg  [10:0] sd_a = 11'd0,
    output reg  [1:0]  sd_ba = 2'd0,
    output reg  [3:0]  sd_dqm = 4'hF,
    inout  wire [31:0] sd_dq,
    output reg         ready = 1'b0         // initialized
);
    localparam [3:0] C_NOP = 4'b0111, C_ACT = 4'b0011, C_READ = 4'b0101, C_WRITE = 4'b0100,
                     C_PRE = 4'b0010, C_REF = 4'b0001, C_MRS = 4'b0000;
    localparam T_RP = 2, T_RCD = 2, T_RFC = 6, T_MRD = 2;

    reg  [3:0]  cmd = C_NOP;
    reg         cke = 1'b0;
    assign {sd_cs_n, sd_ras_n, sd_cas_n, sd_we_n} = cmd;
    assign sd_cke = cke;

    reg  [31:0] dq_o = 32'd0;
    reg         dq_oe = 1'b0;
    assign sd_dq = dq_oe ? dq_o : 32'bz;

    // ---- the request this cycle ---------------------------------------------
    reg         c_issued = 1'b0;            // c_req served; cleared when c_req drops
    wire        c_want = c_req && !c_issued;
    wire        d_want = d_left != 9'd0;
    wire        use_c  = c_want;
    wire [20:0] w   = use_c ? c_addr[22:2] : d_addr;
    wire [7:0]  col = w[7:0];
    wire [1:0]  bnk = w[9:8];
    wire [10:0] row = w[20:10];
    wire        wr  = use_c && c_we;

    reg  [3:0]  open = 4'b0000;             // a row is open in the bank
    reg  [10:0] orow [0:3];
    integer k;

    // ---- timing --------------------------------------------------------------
    reg  [2:0]  wait_ = 3'd0;               // cycles to wait before the next command
    reg  [2:0]  since_act = 3'd7, since_wr = 3'd7, since_rd = 3'd7;
    wire        can_pre = since_act >= 3'd3 && since_wr >= 3'd2;
    reg  [9:0]  ref_t = 10'd0;
    reg         ref_due = 1'b0;

    // ---- initialization ----------------------------------------------------------
    reg  [15:0] init_t = INIT_WAIT;
    reg  [3:0]  init_s = 4'd0;              // 0 wait, 1 precharge, 2-9 refresh, 10 mode, 11 done

    // ---- decide ------------------------------------------------------------------
    // what goes out at this edge (the read/write cases also tell the clients)
    wire go      = ready && wait_ == 3'd0;
    wire do_ref  = go && ref_due;
    wire do_acc  = go && !ref_due && (use_c || d_want);
    wire hit     = open[bnk] && orow[bnk] == row;
    // a write waits for the bus to be free of read data; a read never follows
    // a read directly (back-to-back, the capture sometimes took the next word)
    wire issue   = do_acc && hit && (wr ? since_rd >= 3'd4 : since_rd != 3'd0);
    assign c_wdone = issue && use_c && c_we;
    assign d_take  = issue && !use_c;

    // reads in flight: {cpu, dma, byte lane} per stage; the data is taken
    // from the pins at the edge after the last stage
    localparam L = CAP;
    reg  [L-1:0] t_c = 0, t_d = 0;
    reg  [1:0]   t_lane [0:L-1];
    assign c_rsoon = t_c[L-1];
    reg  [31:0] c_word = 32'd0;
    reg  [1:0]  c_lane = 2'd0;
    assign c_q = c_word[8*c_lane +: 8];

    always @(posedge clk) begin
        cmd   <= C_NOP;
        dq_oe <= 1'b0;
        sd_dqm <= 4'hF;
        if (wait_ != 0) wait_ <= wait_ - 1'b1;
        if (since_act != 3'd7) since_act <= since_act + 1'b1;
        if (since_wr  != 3'd7) since_wr  <= since_wr + 1'b1;
        if (since_rd  != 3'd7) since_rd  <= since_rd + 1'b1;
        if (!c_req) c_issued <= 1'b0;

        // refresh timer
        if (ref_t == 10'd499) begin ref_t <= 0; ref_due <= 1'b1; end
        else ref_t <= ref_t + 1'b1;

        // read data
        t_c <= {t_c[L-2:0], 1'b0};
        t_d <= {t_d[L-2:0], 1'b0};
        for (k = L - 1; k > 0; k = k - 1) t_lane[k] <= t_lane[k - 1];
        d_valid <= t_d[L-1];
        if (t_d[L-1]) d_data <= sd_dq;
        if (t_c[L-1]) begin c_word <= sd_dq; c_lane <= t_lane[L-1]; end

        if (!ready) begin
            // power-up: wait, precharge all, 8 refreshes, mode register
            if (init_t != 0) init_t <= init_t - 1'b1;
            else if (wait_ == 0) begin
                case (init_s)
                    4'd0: begin cke <= 1'b1; init_s <= 4'd1; wait_ <= 3'd2; end
                    4'd1: begin cmd <= C_PRE; sd_a[10] <= 1'b1; wait_ <= T_RP - 1; init_s <= 4'd2; end
                    4'd10: begin
                        cmd <= C_MRS; sd_ba <= 2'b00;
                        sd_a <= 11'b0_0_00_010_0_000;   // burst 1, sequential, CAS 2
                        wait_ <= T_MRD - 1; init_s <= 4'd11;
                    end
                    4'd11: ready <= 1'b1;
                    default: begin cmd <= C_REF; wait_ <= T_RFC - 1; init_s <= init_s + 1'b1; end
                endcase
            end
        end else if (do_ref) begin
            if (open != 0) begin
                if (can_pre) begin
                    cmd <= C_PRE; sd_a[10] <= 1'b1; open <= 4'b0000; wait_ <= T_RP - 1;
                end
            end else begin
                cmd <= C_REF; wait_ <= T_RFC - 1; ref_due <= 1'b0;
            end
        end else if (do_acc) begin
            if (issue) begin
                cmd   <= wr ? C_WRITE : C_READ;
                sd_ba <= bnk;
                sd_a  <= {3'b000, col};     // A10 low: no auto-precharge
                if (wr) begin
                    dq_o   <= {4{c_wd}};
                    dq_oe  <= 1'b1;
                    sd_dqm <= ~(4'b0001 << c_addr[1:0]);
                    since_wr <= 3'd0;
                end else begin
                    sd_dqm   <= 4'h0;
                    since_rd <= 3'd0;
                    t_c[0]   <= use_c;
                    t_d[0]   <= !use_c;
                    t_lane[0] <= c_addr[1:0];
                end
                if (use_c) c_issued <= 1'b1;
            end else if (!hit && open[bnk]) begin
                if (can_pre) begin
                    cmd <= C_PRE; sd_ba <= bnk; sd_a[10] <= 1'b0;
                    open[bnk] <= 1'b0; wait_ <= T_RP - 1;
                end
            end else if (!open[bnk]) begin
                cmd <= C_ACT; sd_ba <= bnk; sd_a <= row;
                open[bnk] <= 1'b1; orow[bnk] <= row;
                since_act <= 3'd0; wait_ <= T_RCD - 1;
            end
        end
    end
endmodule
