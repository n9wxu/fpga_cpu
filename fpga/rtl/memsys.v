`timescale 1ns/1ps
// The SRAM, shared by the CPU, the video DMA and the JTAG loader.
//
// Time is divided into periods, each owned by one user; e0 is the edge a
// period starts on, e1, e2, ... the edges after it.
//
//   CPU     PER cycles. T1 runs one clock per CPU period, on a gated copy of
//           the 50 MHz clock (BUFGCE) whose edge is the period's last edge.
//           cpu_ce is high in the period's last cycle; BUFGCE passes the
//           edge that ends it (measured on the board with a register on
//           T1's clock sampling ph). T1's bus
//           outputs (address, data, strobes) come through its instruction
//           decode and settle up to ~40 ns after its edge, so they are
//           registered into the I/O blocks at e(LAT). A read is at the SRAM
//           pins by e(LAT) + 15 ns (5 ns clock to pin, 10 ns SRAM) and goes
//           to T1 combinationally (up to ~25 ns), which samples it at the
//           period's end: with PER = 5, LAT = 2 that leaves ~20 ns. A write pulses WE# from half a cycle after e(LAT)
//           to half a cycle later (a falling-edge register), so address and
//           data are stable 10 ns either side of it. I/O accesses (F0xxxx)
//           go to the I/O block at e(LAT).
//   DMA     2n cycles for n words (n <= 4). Each address is held for two
//           cycles (at e1, e3, ...) and its data sampled at the second edge:
//           the data reaches the input registers 20.4 ns after the address,
//           too late for one cycle. Handed to the video DMA one edge later.
//           The last word is sampled at the next period's e1; that period
//           is always a CPU one, which leaves the SRAM reading until e(LAT).
//   LOADER  4 cycles: one word written (address and data at e1, WE# from
//           e1.5 to e2.5) or read (address at e1, sampled at e3).
//
// T1's gated clock reaches its registers later than the 50 MHz clock
// reaches the I/O blocks (a second global buffer), so nothing on the SRAM
// bus changes at the edge that ends a CPU period: DMA and loader periods
// start driving it at their e1. (ISE does not check hold on pad paths.)
//
// With PER = 5, T1 runs at 10 MHz. The video DMA takes every other period
// while it has words to fetch, so the CPU never waits more than one DMA
// period at a time; the loader takes periods the DMA leaves.
//
// The SRAM is 256K x 32: IC10 holds bytes 0 and 1 of each word (lanes 0, 1),
// IC11 bytes 2 and 3. All of its signals are registered in the I/O blocks.
module memsys #(
    parameter PER = 5,                      // 50 MHz cycles per CPU clock
    parameter LAT = 2                       // T1's bus is registered at e(LAT)
) (
    input  wire        clk,                 // 50 MHz
    output reg         cpu_ce = 1'b0,       // T1's clock enable: high in the cycle whose closing
                                            // edge is T1's (measured on the board: BUFGCE passes
                                            // the edge that ends a cycle with CE high)
    // T1's bus (combinational outputs of the core)
    input  wire [23:0] cpu_addr,
    input  wire        cpu_rd,
    input  wire        cpu_wr,
    input  wire [7:0]  cpu_dout,
    output wire [7:0]  cpu_din,
    // I/O block: a CPU access to F0xxxx, registered at e(LAT)
    output reg         io_wr = 1'b0,        // one-cycle pulses
    output reg         io_rd = 1'b0,
    output reg  [15:0] io_addr = 16'd0,
    output reg  [7:0]  io_wdata = 8'd0,
    output wire        cpu_lat,             // this edge is e(LAT) of a CPU period
    input  wire [7:0]  io_rdata,            // read data for cpu_addr, registered at e(LAT)
    // video DMA: read more words from dma_addr (word address)
    input  wire        dma_req,
    input  wire [17:0] dma_addr,
    input  wire [8:0]  dma_left,            // words still to issue
    output reg  [2:0]  dma_take = 3'd0,     // words issued this period (pulse at e0)
    output reg         dma_valid = 1'b0,    // a word is on dma_data
    output reg  [31:0] dma_data = 32'd0,
    // JTAG loader: write ldr_data at ldr_addr (word address), or read it
    input  wire        ldr_req,
    input  wire        ldr_rd,
    input  wire [17:0] ldr_addr,
    input  wire [31:0] ldr_data,
    output reg         ldr_done = 1'b0,     // pulse: the access has been made
    output reg  [31:0] ldr_q = 32'd0,       // the word read
    // SRAM pins
    output reg  [17:0] sram_a = 18'd0,
    inout  wire [31:0] sram_d,
    output reg         sram_oe_n = 1'b1,
    output reg         sram_we_n = 1'b1,
    output reg  [1:0]  sram_ce_n = 2'b11,   // [0] IC10, [1] IC11
    output reg  [3:0]  sram_be_n = 4'b1111  // lanes: {UB2, LB2, UB1, LB1}
);
    localparam S_CPU = 2'd0, S_DMA = 2'd1, S_LDR = 2'd2;
    localparam [3:0] CPU_LAST = PER - 1;
    localparam [3:0] LATP = LAT - 1;        // ph at the edge e(LAT)
    localparam [2:0] DMA_MAX = 3'd4;        // words per DMA period

    reg [3:0] ph = 4'd0;                    // cycles since e0
    reg [3:0] last = CPU_LAST;              // ph at the edge that ends this period
    reg [1:0] slot = S_CPU;
    reg       last_dma = 1'b0;

    // ---- data pins -------------------------------------------------------------
    // The write data and output enables stay out of the I/O blocks: a CPU write
    // fills dq with four copies of a byte, and when those equivalent registers
    // were replicated and packed into I/O blocks, the board wrote every byte
    // shifted right one bit (pin k took bit k+1). Write data is set a cycle
    // before WE# falls, so it does not need the I/O block's timing.
    (* IOB = "FALSE", equivalent_register_removal = "no" *) reg [31:0] dq = 32'd0;
    (* IOB = "FALSE", equivalent_register_removal = "no" *) reg [31:0] drive = 32'd0;
    genvar gi;
    generate
        for (gi = 0; gi < 32; gi = gi + 1) begin : pins
            assign sram_d[gi] = drive[gi] ? dq[gi] : 1'bz;
        end
    endgenerate

    (* IOB = "TRUE", shreg_extract = "no" *) reg [31:0] din_q = 32'd0;
    always @(posedge clk) din_q <= sram_d;

    // ---- CPU data in (combinational from the pins, or I/O) -------------------
    wire cpu_io = cpu_addr[23:16] == 8'hF0;
    reg  [7:0] lane;
    always @*
        case (cpu_addr[1:0])
            2'd0: lane = sram_d[7:0];
            2'd1: lane = sram_d[15:8];
            2'd2: lane = sram_d[23:16];
            default: lane = sram_d[31:24];
        endcase
    assign cpu_din = cpu_io ? io_rdata : lane;

    wire cpu_ram = cpu_addr[23:20] == 4'h0;
    assign cpu_lat = slot == S_CPU && ph == LATP;

    // ---- DMA bookkeeping -----------------------------------------------------
    wire [2:0] dma_n = (dma_left >= DMA_MAX) ? DMA_MAX : dma_left[2:0];
    reg  [2:0] dma_k = 3'd0;                // reads still to issue in this period
    reg  [17:0] dma_a = 18'd0;
    (* shreg_extract = "no" *) reg [2:0] issued = 3'b000;  // a read was issued 1, 2, 3 edges ago
    reg        cpu_write = 1'b0;
    reg        ldr_read = 1'b0;

    always @(posedge clk) begin
        io_wr    <= 1'b0;
        io_rd    <= 1'b0;
        dma_take <= 3'd0;
        ldr_done <= 1'b0;
        cpu_ce   <= 1'b0;

        // data returned by DMA reads: an address set at edge E is sampled
        // into din_q at E+2 and handed over at E+3
        issued    <= {issued[1:0], 1'b0};
        dma_valid <= issued[2];
        dma_data  <= din_q;

        if (ph == last) begin
            // e0: the next period starts. Nothing on the SRAM bus changes
            // here except the data drivers of a CPU write turning off.
            ph       <= 4'd0;
            drive    <= 32'd0;
            if (dma_req && dma_left != 0 && !last_dma) begin
                slot     <= S_DMA;
                last_dma <= 1'b1;
                last     <= {dma_n, 1'b0} - 1'b1;     // 2n cycles
                dma_take <= dma_n;
                dma_k    <= dma_n;
                dma_a    <= dma_addr;
            end else if (ldr_req && !last_dma) begin
                slot     <= S_LDR;
                last_dma <= 1'b1;
                last     <= 4'd3;
                ldr_read <= ldr_rd;
            end else begin
                // CPU: its access starts at e(LAT). Until then the SRAM is
                // left reading, so a DMA period's last word is still valid
                // when it is sampled, at e1.
                slot      <= S_CPU;
                last_dma  <= 1'b0;
                last      <= CPU_LAST;
                cpu_write <= 1'b0;
            end
        end else begin
            ph <= ph + 1'b1;
            case (slot)
            S_DMA:
                if (!ph[0] && dma_k != 0) begin             // e1, e3, e5, e7
                    sram_a    <= dma_a;
                    sram_ce_n <= 2'b00;
                    sram_be_n <= 4'b0000;
                    sram_oe_n <= 1'b0;
                    dma_a     <= dma_a + 1'b1;
                    dma_k     <= dma_k - 1'b1;
                    issued[0] <= 1'b1;
                end
            S_LDR: begin
                if (ph == 4'd0) begin                        // e1
                    sram_a    <= ldr_addr;
                    dq        <= ldr_data;
                    drive     <= {32{!ldr_read}};
                    sram_ce_n <= 2'b00;
                    sram_be_n <= 4'b0000;
                    sram_oe_n <= !ldr_read;
                end
                if (ph == 4'd2) begin                        // e3: done
                    ldr_done <= 1'b1;
                    ldr_q    <= din_q;                       // a read: sampled at e3, 40 ns after its address
                end
            end
            default: begin                                   // CPU
                if (ph == LATP) begin                        // e(LAT): T1's bus has settled
                    if (cpu_ram && (cpu_rd || cpu_wr)) begin
                        // a read enables the whole word (the byte is picked
                        // here); a write only its own chip and byte lane
                        sram_a    <= cpu_addr[19:2];
                        sram_ce_n <= cpu_wr ? {cpu_addr[1] == 1'b0, cpu_addr[1] == 1'b1} : 2'b00;
                        sram_be_n <= cpu_wr ? ~(4'b0001 << cpu_addr[1:0]) : 4'b0000;
                        sram_oe_n <= ~cpu_rd;
                        dq        <= {4{cpu_dout}};
                        drive     <= {32{cpu_wr}};
                        cpu_write <= cpu_wr;
                    end
                    if (cpu_io && (cpu_rd || cpu_wr)) begin
                        io_addr  <= cpu_addr[15:0];
                        io_wdata <= cpu_dout;
                        io_wr    <= cpu_wr;
                        io_rd    <= cpu_rd;
                    end
                end
                if (ph == CPU_LAST - 1'b1) cpu_ce <= 1'b1;   // high in the last cycle: T1's edge is e(PER)
            end
            endcase
        end
    end

    // WE#: half a cycle into the write's cycle until half a cycle after it
    always @(negedge clk)
        sram_we_n <= !((slot == S_CPU && cpu_write && ph == LATP + 1'b1) ||
                       (slot == S_LDR && !ldr_read && ph == 4'd1));
endmodule
