`timescale 1ns/1ps
// T1F (8-bit bus, one access a clock, on a gated clock) as a client of the
// RAM machine (rammux.v), and the I/O block's CPU side (as memsys.v had it).
//
// Periods: T1F runs one clock per period, on a gated copy of the 50 MHz
// clock whose edge ends the period (cpu_ce high in the period's last cycle;
// BUFGCE passes that edge). T1F's bus settles ~40 ns after its edge, so it
// is looked at e2 (cpu_lat). A period is PER cycles or longer:
//   read, the word buffered          PERF: the byte comes from rbuf
//   read, not buffered               until the word is back from the RAM
//                                    (+ a clock: rbuf, then T1F's edge)
//   write                            PER: into the write buffer (one word);
//                                    longer only while it is still full
//   no access                        PERF
//   I/O (F0xxxx)                     PER
// rbuf holds the last word read (T1F fetches a byte at a time, so three
// fetches in four find their word there); a CPU write to its word goes
// into it too. Reads and writes leave through one rammux port in order, so
// a read after a write to the same word sees it. A write's data is taken at
// e(DLAT), later than its address: T1S's comes from its register file, a
// block RAM read a clock earlier, once the register's address has settled.
module cpumem #(
    parameter PER = 5,                      // 50 MHz cycles in a period, at least
    parameter PERF = 4,                     // and in one with no access or one from rbuf (T1S:
                                            // its register file reads two clocks before the step)
    parameter LAT = 2,                      // T1F's bus is looked at e(LAT)
    parameter DLAT = 4                      // and a write's data at e(DLAT) (T1S: from its
                                            // register file, a block RAM read a clock earlier)
) (
    input  wire        clk,
    input  wire        rst,                 // T1F held in reset (the loader may write): rbuf invalid
    output reg         cpu_ce = 1'b0,
    // T1F's bus (combinational from the core)
    input  wire [23:0] cpu_addr,
    input  wire        cpu_rd,
    input  wire        cpu_wr,
    input  wire [7:0]  cpu_dout,
    output wire [7:0]  cpu_din,
    // I/O block
    output reg         io_wr = 1'b0,
    output reg         io_rd = 1'b0,
    output reg  [15:0] io_addr = 16'd0,
    output reg  [7:0]  io_wdata = 8'd0,
    output wire        cpu_lat,
    input  wire [7:0]  io_rdata,
    // rammux port (single words)
    output wire        p_req,
    output wire        p_we,
    output wire [17:0] p_addr,
    output wire [31:0] p_wdata,
    output wire [3:0]  p_wbe,
    input  wire        p_ack,
    input  wire        p_rvalid,
    input  wire [31:0] p_rdata,
    // statistics (pulses): a period, a read that waited for the RAM, a
    // write that waited for the buffer
    output reg         s_period = 1'b0,
    output reg         s_miss = 1'b0,
    output reg         s_wstall = 1'b0
);
    localparam [3:0] LATP = LAT - 1;        // ph at the edge e(LAT)
    localparam [3:0] DLATP = DLAT - 1;      // ph at the edge e(DLAT)
    reg  [3:0]  ph = 4'd0;                  // cycles since the period began

    // the word buffer
    reg         rb_v = 1'b0;
    reg  [17:0] rb_a = 18'd0;
    reg  [31:0] rbuf = 32'd0;
    // the write buffer
    reg         wb_v = 1'b0;
    reg  [17:0] wb_a = 18'd0;
    reg  [31:0] wb_d = 32'd0;
    reg  [3:0]  wb_be = 4'd0;
    // a read for the RAM
    reg         rd_v = 1'b0, rd_out = 1'b0;
    reg  [17:0] rd_a = 18'd0;

    assign p_req   = wb_v || (rd_v && !rd_out);
    assign p_we    = wb_v;
    assign p_addr  = wb_v ? wb_a : rd_a;
    assign p_wdata = wb_d;
    assign p_wbe   = wb_be;

    // T1F's access now (settled from e2)
    wire        a_io  = cpu_addr[23:16] == 8'hF0;
    wire        a_ram = cpu_addr[23:20] == 4'h0;
    wire [17:0] a_w   = cpu_addr[19:2];
    wire [3:0]  a_be  = 4'b0001 << cpu_addr[1:0];
    wire        hit   = rb_v && rb_a == a_w;
    reg  [7:0]  lane;
    always @*
        case (cpu_addr[1:0])
            2'd0: lane = rbuf[7:0];
            2'd1: lane = rbuf[15:8];
            2'd2: lane = rbuf[23:16];
            default: lane = rbuf[31:24];
        endcase
    assign cpu_din = a_io ? io_rdata : lane;
    assign cpu_lat = ph == LATP;

    reg         resolved = 1'b0;            // the access is done: the period may end
    reg         quick = 1'b0;               // resolved at e(LAT) with nothing to wait for: PERF
    reg         wpend = 1'b0;               // a RAM write waits for its data (e(DLAT)) and the buffer
    reg         iowp = 1'b0;                // an I/O write waits for its data
    wire        wb_free = !wb_v || (p_ack && wb_v);
    wire        rd_back = p_rvalid && rd_out;
    wire        dtime = ph >= DLATP;        // the write data (cpu_dout) has settled
    wire        wnow = wpend && dtime && wb_free;
    wire [31:0] bmask = {{8{a_be[3]}}, {8{a_be[2]}}, {8{a_be[1]}}, {8{a_be[0]}}};

    always @(posedge clk) begin
        io_wr <= 1'b0; io_rd <= 1'b0;
        s_period <= 1'b0; s_miss <= 1'b0; s_wstall <= 1'b0;

        // the port
        if (p_ack && wb_v) wb_v <= 1'b0;
        else if (p_ack) rd_out <= 1'b1;
        if (rd_back) begin
            rd_v <= 1'b0; rd_out <= 1'b0;
            rbuf <= p_rdata; rb_v <= 1'b1; rb_a <= rd_a;
        end

        // the period
        if (cpu_ce) begin
            // this edge is the core's step: a new period
            ph <= 4'd0; cpu_ce <= 1'b0; resolved <= 1'b0; quick <= 1'b0; s_period <= 1'b1;
        end else begin
            if (ph != 4'd15) ph <= ph + 4'd1;
            if (ph == LATP) begin
                // e(LAT): the core's bus has settled (its write data not yet)
                if (a_io && cpu_rd) begin
                    io_addr <= cpu_addr[15:0]; io_rd <= 1'b1; resolved <= 1'b1;
                end else if (a_io && cpu_wr)
                    iowp <= 1'b1;
                else if (a_ram && cpu_wr)
                    wpend <= 1'b1;
                else if (a_ram && cpu_rd && !hit) begin
                    rd_v <= 1'b1; rd_a <= a_w; s_miss <= 1'b1;
                end else begin
                    resolved <= 1'b1; quick <= 1'b1;   // no access, or a read from rbuf
                end
            end
            // e(DLAT) or later: a write's data
            if (iowp && dtime) begin
                iowp <= 1'b0; resolved <= 1'b1;
                io_addr <= cpu_addr[15:0]; io_wdata <= cpu_dout; io_wr <= 1'b1;
            end
            if (wpend && dtime) begin
                if (wb_free) begin
                    wpend <= 1'b0; resolved <= 1'b1;
                    wb_v <= 1'b1; wb_a <= a_w; wb_d <= {4{cpu_dout}}; wb_be <= a_be;
                    if (hit) rbuf <= (rbuf & ~bmask) | ({4{cpu_dout}} & bmask);
                end else s_wstall <= 1'b1;
            end
            // the core's step ends the period: at least PER cycles, and the
            // access done (a read back: rbuf is loaded at this edge, the
            // step is the next; a write: its data is taken at this edge)
            if ((ph >= PER - 2 || (quick && ph >= PERF - 2)) && (resolved || rd_back || wnow || (iowp && dtime)))
                cpu_ce <= 1'b1;
        end

        if (rst) begin rb_v <= 1'b0; end
    end
endmodule
