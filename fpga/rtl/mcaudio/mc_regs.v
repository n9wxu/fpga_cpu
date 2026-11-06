`timescale 1ns/1ps
// Microcoded audio engine (docs/mcaudio.md): the CPU register port, in iosys.v's style (16-bit
// registers on the 8-bit bus: the low byte held, the high byte commits). Configuration is
// write-only; the status and the monitor read back.
//   BASE+00/02  HEAD lo/hi    the list head (word address; 0 = nothing)
//   BASE+04     CTRL          bit 0 run, bits 7:4 output shift (mix bus >> n to 16 bits)
//   BASE+06     TEMPO         blocks per tick, a 0.16 fraction (the routines' TICK)
//   BASE+08     CS_ADDR       control-store address for the next CS_DATA word
//   BASE+0A-12  CS_DATA0-4    a 72-bit micro-word, low first; writing CS_DATA4 (bits 71:64) commits
//                             it and increments CS_ADDR
//   BASE+14     IRQ           read: pending (bit 7 block done, 0-6 SYS IRQ n); write 1s to clear
//   BASE+16     ID (0xA0C2)   BASE+18 BLOCKS   BASE+1A UNDERRUNS   BASE+1C CLOCKS of the last block
//   BASE+1E     BURSTS        RAM bursts in the last block (node reads, DMA, write-backs)
//   BASE+20     WORDS         RAM words in the last block
//   BASE+22     NODES         nodes walked in the last block
//   BASE+24     MON_IDX (W) / MON_RING (R: the output ring's next position, 0-255)
//   BASE+26/28  MON_LO/HI (R) the monitor word at MON_IDX: 0-255 the output ring {R, L},
//                             256-287 each node's meter (its word 13, list order)
module mc_regs #(parameter [15:0] BASE = 16'h1000) (
    input  wire        clk,
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    input  wire [15:0] rd_addr,
    output reg  [15:0] rd_data = 16'd0,
    output reg         run = 1'b0,
    output reg  [23:0] head = 24'd0,
    output reg  [3:0]  oshift = 4'd0,
    output reg  [15:0] tempo = 16'd0,
    output reg         cs_we = 1'b0,
    output reg  [8:0]  cs_wa = 9'd0,
    output reg  [71:0] cs_wd = 72'd0,
    output reg         irq_clr_we = 1'b0,
    output reg  [7:0]  irq_clr = 8'd0,
    output reg  [8:0]  mon_idx = 9'd0,
    input  wire [31:0] mon_q,
    input  wire [7:0]  mon_ring,
    input  wire [15:0] st_blocks,
    input  wire [15:0] st_under,
    input  wire [15:0] st_clk,
    input  wire [15:0] st_bursts,
    input  wire [15:0] st_words,
    input  wire [15:0] st_nodes,
    input  wire [7:0]  st_irq
);
    reg  [7:0]  lo = 8'd0;
    wire [15:0] w = {io_wdata, lo};
    wire        mine = io_addr[15:6] == BASE[15:6];
    reg  [8:0]  cs_a = 9'd0;
    always @(posedge clk) begin
        cs_we <= 1'b0; irq_clr_we <= 1'b0;
        if (cs_we) cs_a <= cs_a + 9'd1;
        if (io_wr && !io_addr[0]) lo <= io_wdata;
        if (io_wr && io_addr[0] && mine) case (io_addr[5:1])
            5'd0: head[15:0] <= w;
            5'd1: head[23:16] <= w[7:0];
            5'd2: begin run <= w[0]; oshift <= w[7:4]; end
            5'd3: tempo <= w;
            5'd4: cs_a <= w[8:0];
            5'd5: cs_wd[15:0] <= w;
            5'd6: cs_wd[31:16] <= w;
            5'd7: cs_wd[47:32] <= w;
            5'd8: cs_wd[63:48] <= w;
            5'd9: begin cs_wd[71:64] <= w[7:0]; cs_we <= 1'b1; cs_wa <= cs_a; end
            5'd10: begin irq_clr_we <= 1'b1; irq_clr <= w[7:0]; end
            5'd18: mon_idx <= w[8:0];
            default: ;
        endcase
        if (rd_addr[15:6] != BASE[15:6]) rd_data <= 16'd0;
        else case (rd_addr[5:1])
            5'd10: rd_data <= {8'd0, st_irq};
            5'd11: rd_data <= 16'hA0C2;
            5'd12: rd_data <= st_blocks;
            5'd13: rd_data <= st_under;
            5'd14: rd_data <= st_clk;
            5'd15: rd_data <= st_bursts;
            5'd16: rd_data <= st_words;
            5'd17: rd_data <= st_nodes;
            5'd18: rd_data <= {8'd0, mon_ring};
            5'd19: rd_data <= mon_q[15:0];
            5'd20: rd_data <= mon_q[31:16];
            default: rd_data <= 16'd0;
        endcase
    end
endmodule
