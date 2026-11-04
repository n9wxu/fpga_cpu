`timescale 1ns/1ps
// The memory system of fcsys.v: one RAM state machine running the SRAM at
// its full speed, with each client behind a buffer.
//
// The RAM state machine (50 MHz) chooses an access every clock it is free,
// by priority:
//   1. video DMA                (video.v's line buffers: bursts of reads)
//   2. other media              (audio, sprites: none yet)
//   3. CPU writes               (from the write buffer, WB entries)
//   4. CPU data reads           (one at a time; the CPU waits for it)
//   5. instruction prefetch     (into the prefetch buffer, PF words)
//   6. the JTAG loader
// Reads go out one a clock: a run of them is a burst. Each address leaves
// at a 50 MHz edge E; its data, on the pins from ~20 ns, is taken at
// E + 25 ns (clk90: the 50 MHz clock 90 degrees later, in the middle of
// the data's window, which closes at ~29 ns when the next address has
// arrived) and reaches its client at E + 40 ns. A write takes three clocks:
// the address; the data, with WE# falling mid-clock; WE# rising mid-clock,
// address and data held to the end. A read after a write waits a clock
// while the data bus turns around.
//
// The CPU side. FC runs on the 50 MHz clock gated to every other edge
// (25 MHz). Its requests are taken at its edges; what it reads (grants,
// rvalid, data) changes only at the 50 MHz edge halfway between, since its
// gated clock reaches its registers later than the 50 MHz clock reaches
// these.
//   - A store goes into the write buffer and FC carries on; it waits only
//     when the buffer is full. Writes go to RAM before any CPU read, so a
//     read always sees them.
//   - A load waits for its word.
//   - Fetch: FC takes words from the prefetch buffer, which reads ahead in
//     bursts. A fetch that is not the buffer's next word (a branch), or
//     FC's i_flush, empties it and starts a new burst there. It is not a
//     cache: a loop is fetched again each time round. A store into the
//     words it holds or has asked for empties it too.
//   - I/O (F0xxxx) goes to iosys a byte at a time, once the write buffer
//     has drained.
module fcmem #(
    parameter WB = 4,                       // write buffer entries
    parameter PF = 8                        // prefetch buffer words (a burst)
) (
    input  wire        clk,                 // 50 MHz
    input  wire        clk90,               // 50 MHz, 90 degrees later: read data capture
    output reg         cpu_ce = 1'b0,       // high in the cycle whose closing edge is FC's
    // FC's ports
    input  wire        i_req,
    input  wire [23:2] i_addr,
    output wire        i_gnt,
    output reg         i_rvalid = 1'b0,
    output reg  [31:0] i_rdata = 32'd0,
    input  wire        i_flush,
    input  wire        d_req,
    input  wire        d_we,
    input  wire [23:0] d_addr,
    input  wire [3:0]  d_be,
    input  wire [31:0] d_wdata,
    output wire        d_gnt,
    output reg         d_rvalid = 1'b0,
    output reg  [31:0] d_rdata = 32'd0,
    // iosys, a byte at a time (its 8-bit bus, as T1's)
    output reg         io_wr = 1'b0,        // one-cycle pulses
    output reg         io_lat = 1'b0,       // read: iosys registers io_rdata for io_addr
    output reg  [15:0] io_addr = 16'd0,
    output reg  [7:0]  io_wdata = 8'd0,
    input  wire [7:0]  io_rdata,
    // video DMA (video.v)
    input  wire [17:0] dma_addr,
    input  wire [8:0]  dma_left,
    output wire [2:0]  dma_take,
    output reg         dma_valid = 1'b0,
    output reg  [31:0] dma_data = 32'd0,
    // JTAG loader
    input  wire        ldr_req,
    input  wire        ldr_rd,
    input  wire [17:0] ldr_addr,
    input  wire [31:0] ldr_data,
    output reg         ldr_done = 1'b0,
    output reg  [31:0] ldr_q = 32'd0,
    // SRAM pins
    output reg  [17:0] sram_a = 18'd0,
    inout  wire [31:0] sram_d,
    output reg         sram_oe_n = 1'b1,
    output reg         sram_we_n = 1'b1,
    output wire [1:0]  sram_ce_n,
    output reg  [3:0]  sram_be_n = 4'b0000
);
    assign sram_ce_n = 2'b00;

    // FC's edge every other 50 MHz edge
    always @(posedge clk) cpu_ce <= ~cpu_ce;
    wire fce = cpu_ce;                      // this edge is FC's
    wire mid = !cpu_ce;                     // this edge is halfway between two of FC's

    // ================================================================ pins
    // write data and its drive stay out of the I/O blocks (as memsys.v)
    (* IOB = "FALSE", equivalent_register_removal = "no" *) reg [31:0] dq = 32'd0;
    (* IOB = "FALSE", equivalent_register_removal = "no" *) reg [31:0] drive = 32'd0;
    genvar gi;
    generate
        for (gi = 0; gi < 32; gi = gi + 1) begin : pins
            assign sram_d[gi] = drive[gi] ? dq[gi] : 1'bz;
        end
    endgenerate
    // read data: taken at E + 25 ns, then into the 50 MHz domain at E + 40 ns
    (* IOB = "TRUE" *) reg [31:0] rq90 = 32'd0;
    always @(posedge clk90) rq90 <= sram_d;

    // ================================================================ clients
    // ---- write buffer (filled at FC's edges)
    reg  [17:0] wb_a [0:WB-1];
    reg  [31:0] wb_d [0:WB-1];
    reg  [3:0]  wb_be [0:WB-1];
    reg  [1:0]  wb_w = 2'd0, wb_r = 2'd0;
    reg  [2:0]  wb_n = 3'd0;                // entries
    reg         wb_full_v = 1'b0, wb_empty_v = 1'b1;    // as FC sees them (mid-clock)

    // ---- CPU data read (one at a time)
    reg         rd_pend = 1'b0;             // asked, not yet read from RAM
    reg         rd_busy_v = 1'b0;           // as FC sees it: a read is out
    reg  [17:0] rd_a = 18'd0;
    reg         rd_back = 1'b0;             // the word is in rd_word, for FC at the next mid-clock
    reg  [31:0] rd_word = 32'd0;

    // ---- prefetch buffer
    reg  [31:0] pf_mem [0:PF-1];
    reg  [2:0]  pf_rp = 3'd0, pf_wp = 3'd0;
    reg  [3:0]  pf_n = 4'd0;                // words in it
    reg  [3:0]  pf_out = 4'd0;              // reads issued, not yet back
    reg  [21:0] pf_head = 22'd0;            // the word address of its first word
    reg  [21:0] pf_next = 22'd0;            // the next word to read
    reg         pf_on = 1'b0;               // reading ahead
    reg         pf_ep = 1'b0;               // epoch: reads from before an emptying are dropped
    reg         hv_v = 1'b0;                // as FC sees it: the head word is there
    reg  [21:0] ha_v = 22'd0;               //   and its address
    reg  [31:0] i_staged = 32'd0;           // the word FC took, shown at the next mid-clock
    reg         i_have = 1'b0;

    // ---- I/O sequencer
    wire        d_io = d_addr[23:16] == 8'hF0;
    reg  [1:0]  io_k = 2'd0;
    reg         io_busy = 1'b0, io_done = 1'b0, io_done_v = 1'b0, io_ans = 1'b0;
    reg  [31:0] io_word = 32'd0;
    reg         io_rdwait = 1'b0;
    reg  [1:0]  io_rdlane = 2'd0;
    function [1:0] lowest(input [3:0] m);
        casez (m)
            4'b???1: lowest = 2'd0;
            4'b??10: lowest = 2'd1;
            4'b?100: lowest = 2'd2;
            default: lowest = 2'd3;
        endcase
    endfunction
    wire [3:0]  io_rest = io_busy ? (d_be & ~((4'b0010 << io_k) - 4'b0001)) : d_be;   // lanes still to do
    wire [1:0]  io_lane = lowest(io_rest);
    wire        io_lastb = (io_rest & ~(4'b0001 << io_lane)) == 4'b0000;

    // ---- FC's grants (from what FC sees, and its request)
    assign d_gnt = d_req && (d_io ? io_done_v
                                  : d_we ? !wb_full_v
                                         : !rd_busy_v && !rd_back);
    assign i_gnt = i_req && hv_v && ha_v == i_addr && !i_flush;

    // ================================================================ the RAM state machine
    localparam T_NONE = 3'd0, T_VID = 3'd1, T_CPU = 3'd2, T_PF0 = 3'd3, T_PF1 = 3'd4, T_LDR = 3'd5;
    reg  [1:0]  wph = 2'd0;                 // write: 1 address out, 2 data on (WE# low), 3 hold; 0 free
    reg         after_wr = 1'b0;            // the last access was a write: a read waits a clock
    reg  [2:0]  tg_a = T_NONE, tg_b = T_NONE;   // the read issued one, two edges ago
    reg         ldr_wait = 1'b0;            // a loader access is out, or its request not yet dropped

    // this clock's choice
    wire        free = wph == 2'd0;
    wire        want_v = dma_left != 9'd0;
    wire        want_w = wb_n != 3'd0;
    wire        want_c = rd_pend;
    wire        pf_room = ({1'b0, pf_n} + {1'b0, pf_out}) < PF;
    wire        want_p = pf_on && pf_room && pf_next[21:18] == 4'h0;   // in RAM
    wire        want_l = ldr_req && !ldr_wait;
    wire        go_v = free && want_v && !after_wr;
    wire        go_w = free && !want_v && want_w;
    wire        go_c = free && !want_v && !want_w && want_c && !after_wr;
    wire        go_p = free && !want_v && !want_w && !want_c && want_p && !after_wr;
    wire        go_l = free && !want_v && !want_w && !want_c && !want_p && want_l && (ldr_rd ? !after_wr : 1'b1);
    assign dma_take = {2'b00, go_v};

    // prefetch reads coming back this clock
    wire        pf_push = tg_b == (pf_ep ? T_PF1 : T_PF0);
    wire        pf_ret  = tg_b == T_PF0 || tg_b == T_PF1;

    // FC's fetch at this edge
    wire        f_take = fce && i_gnt;
    wire        f_restart = fce && (i_flush || (i_req && (!pf_on || pf_head != i_addr)));
    // a store into the words the prefetch buffer holds or has asked for
    wire        wb_in = fce && d_req && d_we && !d_io && d_gnt;
    wire        st_hit = wb_in && d_addr[23:2] >= pf_head && d_addr[23:2] < pf_next;

    always @(posedge clk) begin
        dma_valid <= 1'b0; ldr_done <= 1'b0; io_wr <= 1'b0; io_lat <= 1'b0;

        // ---- reads coming back (issued two edges ago)
        tg_b <= tg_a; tg_a <= T_NONE;
        if (tg_b == T_VID) begin dma_valid <= 1'b1; dma_data <= rq90; end
        if (tg_b == T_CPU) begin rd_word <= rq90; rd_back <= 1'b1; end
        if (tg_b == T_LDR) begin ldr_q <= rq90; ldr_done <= 1'b1; end
        if (ldr_wait && !ldr_req) ldr_wait <= 1'b0;

        // ---- the RAM's next access
        case (wph)
            2'd1: begin drive <= 32'hFFFFFFFF; wph <= 2'd2; end   // data on; WE# falls mid-clock
            2'd2: wph <= 2'd3;                                    // WE# rises mid-clock
            2'd3: begin wph <= 2'd0; after_wr <= 1'b1;            // address and data held to here
                        if (ldr_wait) ldr_done <= 1'b1; end
            default: begin
                drive <= 32'd0;
                if (go_v) begin
                    sram_a <= dma_addr; sram_oe_n <= 1'b0; sram_be_n <= 4'b0000; tg_a <= T_VID;
                end else if (go_w) begin
                    sram_a <= wb_a[wb_r]; dq <= wb_d[wb_r]; sram_be_n <= ~wb_be[wb_r]; sram_oe_n <= 1'b1;
                    wb_r <= wb_r + 2'd1; wph <= 2'd1;
                end else if (go_c) begin
                    sram_a <= rd_a; sram_oe_n <= 1'b0; sram_be_n <= 4'b0000; tg_a <= T_CPU; rd_pend <= 1'b0;
                end else if (go_p) begin
                    sram_a <= pf_next[17:0]; sram_oe_n <= 1'b0; sram_be_n <= 4'b0000;
                    tg_a <= pf_ep ? T_PF1 : T_PF0;
                end else if (go_l) begin
                    sram_a <= ldr_addr; ldr_wait <= 1'b1;
                    if (ldr_rd) begin sram_oe_n <= 1'b0; sram_be_n <= 4'b0000; tg_a <= T_LDR; end
                    else begin dq <= ldr_data; sram_be_n <= 4'b0000; sram_oe_n <= 1'b1; wph <= 2'd1; end
                end else if (after_wr) begin
                    sram_oe_n <= 1'b1;              // the turnaround clock
                end
                if (!go_w && !(go_l && !ldr_rd)) after_wr <= 1'b0;
            end
        endcase

        // ---- write buffer
        if (wb_in) begin
            wb_a[wb_w] <= d_addr[19:2]; wb_d[wb_w] <= d_wdata; wb_be[wb_w] <= d_be;
            wb_w <= wb_w + 2'd1;
        end
        wb_n <= wb_n + (wb_in ? 3'd1 : 3'd0) - (go_w ? 3'd1 : 3'd0);

        // ---- CPU data read
        if (fce && d_req && !d_we && !d_io && d_gnt) begin rd_pend <= 1'b1; rd_a <= d_addr[19:2]; end

        // ---- prefetch buffer
        if (pf_push) begin pf_mem[pf_wp] <= rq90; pf_wp <= pf_wp + 3'd1; end
        if (f_take) begin i_staged <= pf_mem[pf_rp]; i_have <= 1'b1; end
        if (f_restart || st_hit) begin
            // empty it; reads still out come back with the old epoch and are dropped
            pf_n <= 4'd0; pf_rp <= 3'd0; pf_wp <= 3'd0; pf_ep <= ~pf_ep;
            pf_out <= pf_out + (go_p ? 4'd1 : 4'd0) - (pf_ret ? 4'd1 : 4'd0);
            if (f_restart && !i_flush) begin           // FC wants a word that is not next: read from there
                pf_head <= i_addr; pf_next <= i_addr; pf_on <= 1'b1;
            end else if (f_restart) pf_on <= 1'b0;     // a flush: FC asks where next
            else begin                                 // a store: read again from the head
                pf_head <= pf_head + (f_take ? 22'd1 : 22'd0);
                pf_next <= pf_head + (f_take ? 22'd1 : 22'd0);
            end
        end else begin
            if (go_p) pf_next <= pf_next + 22'd1;
            pf_out <= pf_out + (go_p ? 4'd1 : 4'd0) - (pf_ret ? 4'd1 : 4'd0);
            pf_n <= pf_n + (pf_push ? 4'd1 : 4'd0) - (f_take ? 4'd1 : 4'd0);
            if (f_take) begin pf_rp <= pf_rp + 3'd1; pf_head <= pf_head + 22'd1; end
        end

        // ---- I/O, a byte per FC clock, once the write buffer is empty
        if (fce) begin
            if (io_rdwait) io_word[8 * io_rdlane +: 8] <= io_rdata;
            io_rdwait <= 1'b0;
            io_done <= 1'b0;
            if (d_req && d_io && wb_empty_v && !rd_busy_v) begin
                if (io_done_v) begin
                    io_busy <= 1'b0;                    // FC takes it at this edge
                    if (!d_we) io_ans <= 1'b1;
                end else if (!io_done) begin
                    io_k <= io_lane; io_addr <= {d_addr[15:2], io_lane};
                    io_wdata <= d_wdata[8 * io_lane +: 8];
                    if (d_we) io_wr <= 1'b1;
                    else begin io_lat <= 1'b1; io_rdwait <= 1'b1; io_rdlane <= io_lane; end
                    io_busy <= 1'b1;
                    if (io_lastb) io_done <= 1'b1;
                end
            end
        end

        // ---- mid-clock: what FC will see at its next edge
        if (mid) begin
            wb_full_v  <= wb_n == WB;
            wb_empty_v <= wb_n == 3'd0;
            hv_v <= pf_n != 4'd0;
            ha_v <= pf_head;
            i_rvalid <= i_have; i_have <= 1'b0;
            if (i_have) i_rdata <= i_staged;
            io_done_v <= io_done;
            d_rvalid <= 1'b0;
            if (rd_back) begin d_rvalid <= 1'b1; d_rdata <= rd_word; rd_back <= 1'b0; rd_busy_v <= 1'b0; end
            else if (io_ans) begin d_rvalid <= 1'b1; d_rdata <= io_word; io_ans <= 1'b0; end
            if (rd_pend) rd_busy_v <= 1'b1;
        end
    end

    // WE#: low from the middle of a write's second clock to the middle of its third
    always @(negedge clk) sram_we_n <= !(wph == 2'd2);
endmodule
