`timescale 1ns/1ps
// The RAM's clients, prioritized, in front of a burst PHY: the one memory
// interface everything else builds on (the CPU's buffers, video, audio,
// sprites, the loader). The same on every board; the PHY behind it is the
// board's (sram_phy.v on the Spartan-3, an SDRAM PHY on the Tang).
//
// A client port, all on clk:
//   p_req    a burst wanted: held, with p_we, p_addr and p_len, until p_ack
//   p_we     write (else read)
//   p_addr   word address of the first word
//   p_len    words, 1 up
//   p_ack    the burst starts: its first word goes to the PHY this clock
//            (the client may change its request at this edge)
//   p_wdata, p_wbe   a write's current word: p_wnext is high in each clock
//            that takes it; the next word is wanted at the following clock
//   p_rvalid a read's word is on p_rdata (shared by all ports); a burst's
//            words come back in order
// Port 0 has the highest priority, then 1, and so on. A burst is not
// interrupted: the longest burst a port asks for is how long a higher one
// may wait. Bursts follow one another with no gap.
//
// The PHY takes a command (phy_cmd, with phy_we, phy_addr, phy_wdata,
// phy_be) at each edge where phy_ready is high, and returns read data in
// the order it took the reads (phy_rvalid, phy_rdata).
// Built for the FPGA's LUTs: the grant is one-hot (the lowest request,
// by the carry chain: req & -req), every field is chosen by AND-OR of the
// one-hot grant (no binary index, which XST makes into multipliers and
// shifters), and the read tags are one-hot too, so a returning word needs
// no decode. WMASK names the ports that write: only they reach the write
// data and byte enables.
module rammux #(
    parameter NP = 5,                       // ports
    parameter AW = 18,                      // word address bits
    parameter LW = 5,                       // burst length bits
    parameter TQ = 8,                       // reads the PHY may have out (tag queue; 8 here)
    parameter [NP-1:0] WMASK = {NP{1'b1}}   // the ports that write
) (
    input  wire                 clk,
    input  wire [NP-1:0]        p_req,
    input  wire [NP-1:0]        p_we,
    input  wire [NP*AW-1:0]     p_addr,
    input  wire [NP*LW-1:0]     p_len,
    output wire [NP-1:0]        p_ack,
    input  wire [NP*32-1:0]     p_wdata,
    input  wire [NP*4-1:0]      p_wbe,
    output wire [NP-1:0]        p_wnext,
    output wire [NP-1:0]        p_rvalid,
    output wire [31:0]          p_rdata,
    // the PHY
    output wire                 phy_cmd,
    output wire                 phy_we,
    output wire [AW-1:0]        phy_addr,
    output reg  [31:0]          phy_wdata,
    output reg  [3:0]           phy_be,
    input  wire                 phy_ready,
    input  wire                 phy_rvalid,
    input  wire [31:0]          phy_rdata
);
    // the burst under way
    reg              cur_v = 1'b0;
    reg  [NP-1:0]    cur_g = {NP{1'b0}};    // its port, one-hot
    reg              cur_we = 1'b0;
    reg  [AW-1:0]    cur_a = {AW{1'b0}};
    reg  [LW-1:0]    cur_n = {LW{1'b0}};    // words still to issue

    // the port that would start now: the lowest-numbered request, one-hot
    wire [NP-1:0]    gnt = p_req & (~p_req + 1'b1);
    wire             any = p_req != {NP{1'b0}};
    wire [NP-1:0]    sel = cur_v ? cur_g : gnt;

    // a new burst's fields, by AND-OR of the grant
    reg              n_we;
    reg  [AW-1:0]    n_addr;
    reg  [LW-1:0]    n_len;
    integer i;
    always @* begin
        n_we = 1'b0; n_addr = {AW{1'b0}}; n_len = {LW{1'b0}};
        phy_wdata = 32'd0; phy_be = 4'd0;
        for (i = 0; i < NP; i = i + 1) begin
            n_addr = n_addr | (p_addr[i*AW +: AW] & {AW{gnt[i]}});
            n_len  = n_len  | (p_len[i*LW +: LW] & {LW{gnt[i]}});
            if (WMASK[i]) begin
                n_we      = n_we | (p_we[i] & gnt[i]);
                phy_wdata = phy_wdata | (p_wdata[i*32 +: 32] & {32{sel[i]}});
                phy_be    = phy_be | (p_wbe[i*4 +: 4] & {4{sel[i]}});
            end
        end
    end

    // this clock's command: the burst under way, or a new one's first word
    assign phy_cmd  = cur_v || any;
    assign phy_we   = cur_v ? cur_we : n_we;
    assign phy_addr = cur_v ? cur_a : n_addr;
    wire             issue = phy_cmd && phy_ready;
    assign p_ack   = (cur_v || !phy_ready) ? {NP{1'b0}} : gnt;
    assign p_wnext = (issue && phy_we) ? sel : {NP{1'b0}};
    wire [LW-1:0]    left = cur_v ? cur_n : n_len;

    always @(posedge clk)
        if (issue) begin
            cur_v  <= left != {{LW-1{1'b0}}, 1'b1};
            cur_g  <= sel;
            cur_we <= phy_we;
            cur_a  <= phy_addr + 1'b1;
            cur_n  <= left - 1'b1;
        end

    // read tags: the port (one-hot) of each read the PHY has out
    reg  [NP-1:0] tq [0:7];
    reg  [2:0]    tq_w = 3'd0, tq_r = 3'd0;
    always @(posedge clk) begin
        if (issue && !phy_we) begin tq[tq_w] <= sel; tq_w <= tq_w + 3'd1; end
        if (phy_rvalid) tq_r <= tq_r + 3'd1;
    end
    assign p_rvalid = phy_rvalid ? tq[tq_r] : {NP{1'b0}};
    assign p_rdata = phy_rdata;
endmodule
