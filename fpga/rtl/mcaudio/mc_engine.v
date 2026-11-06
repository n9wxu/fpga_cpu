`timescale 1ns/1ps
// Microcoded audio engine (issue #23, docs/mcaudio.md): the list walker, the RAM client, the
// buffers, the block renderer, the output and the monitor; mc_core.v (the micro-sequencer and its
// datapath), mc_regs.v (the CPU port). The ports match audio_engine.v's, plus the write side of
// the rammux port (state write-back).
//
// A block is 32 output samples. For each block: walk the list from HEAD; per element (a group of
// up to 4 nodes flagged linked-with-next, fetched one after another into the node buffer) run the
// head's routine (its per-block code, which may ask for sample bursts into the sample buffer, then
// its per-sample loop into the mix buses), write each node's state (words 12-15) back; then move
// the main bus into the output FIFO (saturated to 16 bits) and clear the buses. A block starts when
// the FIFO has room for it.
//
// Node (16 words): w0 [21:0] next (word address, 0 = end), [31] linked with next;
//   w1 [8:0] the routine's entry (the head's), [31] skip; w2-w11 parameters; w12-w15 state,
//   written back (w13: the node's meter, mirrored into the monitor).
module mc_engine #(
    parameter AW = 22,
    parameter [15:0] BASE = 16'h1000,
    parameter SINE = "mc_sine.mem"          // the sine table ($readmemh, from where the tool runs)
) (
    input  wire        clk,
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    input  wire [15:0] rd_addr,
    output wire [15:0] rd_data,
    output wire        irq,
    output reg         p_req = 1'b0,
    output reg         p_we = 1'b0,
    output reg  [AW-1:0] p_addr = 0,
    output reg  [4:0]  p_len = 5'd0,
    input  wire        p_ack,
    input  wire        p_rvalid,
    input  wire [31:0] p_rdata,
    output wire [31:0] p_wdata,
    output wire [3:0]  p_wbe,
    input  wire        p_wnext,
    output wire        s_valid,
    output wire [15:0] s_l,
    output wire [15:0] s_r,
    input  wire        s_ready
);
    // ---- CPU port
    wire        run, cs_we, irq_clr_we;
    wire [8:0]  cs_wa, mon_idx;
    wire [71:0] cs_wd;
    wire [23:0] head;
    wire [3:0]  oshift;
    wire [15:0] tempo;
    wire [7:0]  irq_clr;
    reg  [15:0] st_blocks = 16'd0, st_under = 16'd0, st_clk = 16'd0;
    reg  [15:0] st_bursts = 16'd0, st_words = 16'd0, st_nodes = 16'd0;
    reg  [15:0] c_bursts = 16'd0, c_words = 16'd0, c_nodes = 16'd0;   // this block's
    reg  [7:0]  irq_pend = 8'd0;
    reg  [31:0] mon_q = 32'd0;
    reg  [7:0]  mring = 8'd0;
    mc_regs #(.BASE(BASE)) regs (
        .clk(clk), .io_wr(io_wr), .io_addr(io_addr), .io_wdata(io_wdata),
        .rd_addr(rd_addr), .rd_data(rd_data),
        .run(run), .head(head), .oshift(oshift), .tempo(tempo),
        .cs_we(cs_we), .cs_wa(cs_wa), .cs_wd(cs_wd), .irq_clr_we(irq_clr_we), .irq_clr(irq_clr),
        .mon_idx(mon_idx), .mon_q(mon_q), .mon_ring(mring),
        .st_blocks(st_blocks), .st_under(st_under), .st_clk(st_clk),
        .st_bursts(st_bursts), .st_words(st_words), .st_nodes(st_nodes), .st_irq(irq_pend));
    assign irq = irq_pend != 8'd0;

    // ---- buffers
    (* syn_ramstyle = "block_ram" *) reg [31:0] nb [0:63];      // node buffer: 4 nodes
    (* syn_ramstyle = "block_ram" *) reg [31:0] sb [0:511];     // sample buffer
    (* syn_ramstyle = "block_ram" *) reg [31:0] bus [0:127];    // mix buses
    (* syn_ramstyle = "block_ram" *) reg [31:0] mon [0:511];    // the monitor: output ring, meters
    reg [31:0] nb_q, sb_q, bus_q;
    integer ii;
    initial for (ii = 0; ii < 128; ii = ii + 1) bus[ii] = 32'd0;     // (block RAM powers up 0)
    reg [5:0]  nb_ra;  reg nb_we; reg [5:0] nb_wa; reg [31:0] nb_wd;
    reg [6:0]  bus_ra; reg bus_we; reg [6:0] bus_wa; reg [31:0] bus_wd;
    wire [8:0] sb_ra;
    reg        sb_we = 1'b0; reg [8:0] sb_wa = 9'd0; reg [31:0] sb_wd = 32'd0;
    reg        mon_we = 1'b0; reg [8:0] mon_wa = 9'd0; reg [31:0] mon_wd = 32'd0;
    always @(posedge clk) begin
        nb_q <= nb[nb_ra];  if (nb_we) nb[nb_wa] <= nb_wd;
        sb_q <= sb[sb_ra];  if (sb_we) sb[sb_wa] <= sb_wd;
        bus_q <= bus[bus_ra]; if (bus_we) bus[bus_wa] <= bus_wd;
        mon_q <= mon[mon_idx]; if (mon_we) mon[mon_wa] <= mon_wd;
    end

    // ---- the micro-sequencer
    reg        k_start = 1'b0;
    reg  [8:0] k_entry = 9'd0;
    wire       k_busy;
    wire [5:0] k_nb_ra, k_nb_wa;
    wire       k_nb_we;
    wire [31:0] k_nb_wd;
    wire [6:0] k_bus_ra, k_bus_wa;
    wire       k_bus_we;
    wire [31:0] k_bus_wd;
    wire       dma_req;
    wire [23:0] dma_addr;
    wire [4:0] dma_len;
    wire [8:0] dma_dst;
    reg        dma_done = 1'b0;
    wire [7:0] k_irq;
    reg        tick = 1'b0;
    mc_core #(.SINE(SINE)) core (
        .clk(clk), .cs_we(cs_we), .cs_wa(cs_wa), .cs_wd(cs_wd),
        .start(k_start), .entry(k_entry), .busy(k_busy), .tick(tick), .irq_set(k_irq),
        .dma_req(dma_req), .dma_addr(dma_addr), .dma_len(dma_len), .dma_dst(dma_dst), .dma_done(dma_done),
        .nb_ra(k_nb_ra), .nb_rd(nb_q), .nb_we(k_nb_we), .nb_wa(k_nb_wa), .nb_wd(k_nb_wd),
        .sb_ra(sb_ra), .sb_rd(sb_q),
        .bus_ra(k_bus_ra), .bus_rd(bus_q), .bus_we(k_bus_we), .bus_wa(k_bus_wa), .bus_wd(k_bus_wd));

    // ---- the walker
    localparam W_IDLE = 4'd0, W_NREQ = 4'd1, W_NRCV = 4'd2, W_START = 4'd3, W_DREQ = 4'd4,
               W_DRCV = 4'd5, W_KWAIT = 4'd6, W_WREQ = 4'd7, W_NEXT = 4'd8, W_DRAIN = 4'd9;
    reg [3:0]  ws = W_IDLE;
    reg [1:0]  gs = 2'd0, gn = 2'd0;        // the node being fetched / written back, nodes in the group
    reg [AW-1:0] nptr [0:3];                 // each node's address, for the write-back
    reg [AW-1:0] ptr = 0;                    // the next element
    reg        lnk = 1'b0;
    reg [3:0]  wc = 4'd0;
    reg [4:0]  dc = 5'd0;
    reg [15:0] bclk = 16'd0;
    reg [15:0] tacc = 16'd0;
    reg [1:0]  wk = 2'd0;                    // write-back word 12 + wk
    reg [7:0]  dwr = 8'd0;                   // drain: the read index, the write follows
    reg [4:0]  nord = 5'd0;                  // the group's first node's place in the list
    // drain: index j (main L/R interleaved) is read at dwr = j and processed (and cleared) at
    // dwr = j + 1; then dwr 65-128 clear the filter buses (entries 64-127)
    wire       dact = ws == W_DRAIN && dwr >= 8'd1 && dwr <= 8'd64;
    wire       dclr = ws == W_DRAIN && dwr >= 8'd65 && dwr <= 8'd128;
    wire [7:0] dj = dwr - 8'd1;
    wire [6:0] dl = dact ? {1'b0, dj[0], dj[5:1]} : dj[6:0];
    reg        wsent = 1'b0;
    // the output FIFO: 64 stereo samples, block RAM (first-word-fall-through by a registered read)
    (* syn_ramstyle = "block_ram" *) reg [31:0] of [0:63];
    reg [6:0]  ofw = 7'd0, ofr = 7'd0, ofw_d = 7'd0;
    reg [31:0] of_q = 32'd0;
    reg        of_we = 1'b0;
    reg [31:0] of_wd = 32'd0;
    wire       pop = s_valid && s_ready;
    wire [6:0] ofr_n = ofr + {6'd0, pop};
    always @(posedge clk) begin
        if (of_we) of[ofw[5:0]] <= of_wd;
        of_q <= of[ofr_n[5:0]];
        ofw_d <= ofw;
        ofr <= ofr_n;
        if (of_we) ofw <= ofw + 7'd1;
        if (s_ready && !s_valid && st_blocks != 16'd0) st_under <= st_under + 16'd1;
    end
    assign s_valid = ofw_d != ofr;
    assign s_l = of_q[15:0];
    assign s_r = of_q[31:16];
    wire [6:0] of_fill = ofw - ofr;

    function [15:0] sat16(input [31:0] x, input [3:0] s);
        reg signed [31:0] y;
        begin
            y = $signed(x) >>> s;
            sat16 = (y > 32'sd32767) ? 16'h7FFF : (y < -32'sd32768) ? 16'h8000 : y[15:0];
        end
    endfunction

    // port sharing: the walker (fetch, write-back, drain) and the micro-sequencer
    reg [5:0] w_nb_ra = 6'd0;
    always @* begin
        nb_ra = k_busy ? k_nb_ra : (ws == W_WREQ) ? {gs, 2'b11, wk + {1'b0, p_wnext}} : w_nb_ra;
        nb_we = k_nb_we | (ws == W_NRCV && p_rvalid);
        nb_wa = k_nb_we ? k_nb_wa : {gs, wc};
        nb_wd = k_nb_we ? k_nb_wd : p_rdata;
        bus_ra = (ws == W_DRAIN) ? {1'b0, dwr[0], dwr[5:1]} : k_bus_ra;
        bus_we = k_bus_we | dact | dclr;
        bus_wa = (dact || dclr) ? dl : k_bus_wa;
        bus_wd = (dact || dclr) ? 32'd0 : k_bus_wd;
    end
    assign p_wdata = nb_q;
    assign p_wbe = 4'hF;

    reg [15:0] lsamp = 16'd0;
    always @(posedge clk) begin
        k_start <= 1'b0; dma_done <= 1'b0; sb_we <= 1'b0; of_we <= 1'b0; mon_we <= 1'b0;
        if (k_irq != 8'd0) irq_pend <= irq_pend | k_irq;
        if (irq_clr_we) irq_pend <= irq_pend & ~irq_clr;
        if (ws != W_IDLE) bclk <= bclk + 16'd1;
        if (p_req && p_ack) c_bursts <= c_bursts + 16'd1;
        if (p_rvalid || p_wnext) c_words <= c_words + 16'd1;
        case (ws)
            W_IDLE: if (run && head != 0 && of_fill <= 7'd32) begin
                ptr <= head[AW-1:0]; bclk <= 16'd0; gs <= 2'd0; nord <= 5'd0; ws <= W_NREQ;
                c_bursts <= 16'd0; c_words <= 16'd0; c_nodes <= 16'd0;
                {tick, tacc} <= {1'b0, tacc} + {1'b0, tempo};
            end
            W_NREQ: begin
                p_req <= 1'b1; p_we <= 1'b0; p_addr <= ptr; p_len <= 5'd16;
                nptr[gs] <= ptr; wc <= 4'd0;
                if (p_req && p_ack) begin p_req <= 1'b0; ws <= W_NRCV; end
            end
            W_NRCV: if (p_rvalid) begin
                if (wc == 4'd0) begin ptr <= p_rdata[AW-1:0]; lnk <= p_rdata[31]; end
                if (wc == 4'd15) begin
                    c_nodes <= c_nodes + 16'd1;
                    if (lnk && gs != 2'd3) begin gs <= gs + 2'd1; ws <= W_NREQ; end
                    else begin gn <= gs; w_nb_ra <= 6'd1; dc <= 5'd0; ws <= W_START; end
                end
                wc <= wc + 4'd1;
            end
            // the head's w1: the routine's entry (bit 31: skip it)
            W_START: begin
                dc <= dc + 5'd1;
                if (dc == 5'd1) begin
                    gs <= 2'd0;
                    if (!nb_q[31]) begin k_entry <= nb_q[8:0]; k_start <= 1'b1; end
                    ws <= W_KWAIT;
                end
            end
            W_KWAIT: if (dma_req) ws <= W_DREQ;
                     else if (!k_start && !k_busy) begin wk <= 2'd0; wsent <= 1'b0; ws <= W_WREQ; end
            W_DREQ: begin
                p_req <= 1'b1; p_we <= 1'b0; p_addr <= dma_addr[AW-1:0]; p_len <= dma_len;
                sb_wa <= dma_dst - 9'd1;
                if (p_req && p_ack) begin p_req <= 1'b0; ws <= W_DRCV; dc <= dma_len; end
            end
            W_DRCV: if (p_rvalid) begin
                sb_we <= 1'b1; sb_wa <= sb_wa + 9'd1; sb_wd <= p_rdata;
                dc <= dc - 5'd1;
                if (dc == 5'd1) begin dma_done <= 1'b1; ws <= W_KWAIT; end
            end
            // the state, words 12-15 of each node in the group; word 13 is its meter
            W_WREQ: begin
                if (!wsent) begin p_req <= 1'b1; p_we <= 1'b1; p_addr <= nptr[gs] + 12; p_len <= 5'd4; wsent <= 1'b1; end
                if (p_req && p_ack) p_req <= 1'b0;
                if (p_wnext) begin
                    if (wk == 2'd1) begin mon_we <= 1'b1; mon_wa <= {4'b1000, nord + {3'd0, gs}}; mon_wd <= p_wdata; end
                    wk <= wk + 2'd1;
                    if (wk == 2'd3) ws <= W_NEXT;
                end
            end
            W_NEXT: begin
                p_we <= 1'b0;
                if (gs != gn) begin gs <= gs + 2'd1; wk <= 2'd0; wsent <= 1'b0; ws <= W_WREQ; end
                else if (ptr != 0) begin gs <= 2'd0; nord <= nord + {3'd0, gn} + 5'd1; ws <= W_NREQ; end
                else begin dwr <= 8'd0; ws <= W_DRAIN; end
            end
            // the main bus to the FIFO (L, R interleaved) and the monitor's ring; every entry cleared
            W_DRAIN: begin
                dwr <= dwr + 8'd1;
                if (dact) begin
                    if (dwr[0]) lsamp <= sat16(bus_q, oshift);
                    else begin
                        of_we <= 1'b1; of_wd <= {sat16(bus_q, oshift), lsamp};
                        mon_we <= 1'b1; mon_wa <= {1'b0, mring}; mon_wd <= {sat16(bus_q, oshift), lsamp};
                        mring <= mring + 8'd1;
                    end
                end
                if (dwr == 8'd128) begin
                    dwr <= 8'd0; ws <= W_IDLE; st_blocks <= st_blocks + 16'd1; st_clk <= bclk; irq_pend[7] <= 1'b1;
                    st_bursts <= c_bursts; st_words <= c_words; st_nodes <= c_nodes;
                end
            end
            default: ws <= W_IDLE;
        endcase
    end
endmodule
