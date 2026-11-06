`timescale 1ns/1ps
// An SPI NOR flash for system_tb.v (as the Tang's XT25F64B, mode 0): READ 03, RDID 9F (0B 40 17),
// RDSR 05, WREN 06, PP 02 (within a 256-byte page), SE 20 (4 KB). A 1 MB window of the 8 MB
// starting at BASE holds data (reads elsewhere give FF); +flash=FILE loads it ($readmemh, bytes
// from BASE). Programming takes PROG_NS (WIP set), as a real part's does.
module spiflash_model #(
    parameter [23:0] BASE    = 24'h400000,
    parameter        PROG_NS = 20000
) (
    input  wire cs_n,
    input  wire sclk,
    input  wire mosi,
    output reg  miso = 1'b1
);
    reg [7:0]  mem [0:1048575];
    integer i;
    reg [1023:0] f;
    initial begin
        for (i = 0; i < 1048576; i = i + 1) mem[i] = 8'hFF;
        if ($value$plusargs("flash=%s", f)) $readmemh(f, mem);
    end
    function [7:0] rdb(input [23:0] a);
        rdb = (a >= BASE && a < BASE + 24'h100000) ? mem[a - BASE] : 8'hFF;
    endfunction

    reg [7:0]  sh = 8'd0, cmd = 8'd0, outb = 8'hFF;
    reg [2:0]  bc = 3'd0;
    integer    nb = 0;                     // bytes in this transaction
    reg [23:0] ad = 24'd0;
    reg        wel = 1'b0, wip = 1'b0;

    always @(negedge cs_n) begin bc = 0; nb = 0; miso = 1'b1; end
    always @(posedge cs_n) begin
        if (cmd == 8'h20 && nb >= 4 && wel) begin                      // sector erase
            wip = 1'b1; wel = 1'b0;
            for (i = 0; i < 4096; i = i + 1)
                if ({ad[23:12], 12'd0} + i >= BASE && {ad[23:12], 12'd0} + i < BASE + 24'h100000)
                    mem[{ad[23:12], 12'd0} + i - BASE] = 8'hFF;
            #(PROG_NS) wip = 1'b0;
        end else if (cmd == 8'h02 && nb > 4) begin
            wel = 1'b0; wip = 1'b1; #(PROG_NS) wip = 1'b0;
        end
        cmd = 8'd0;
    end
    always @(posedge sclk) if (!cs_n) begin
        sh = {sh[6:0], mosi};
        bc = bc + 1'b1;
        if (bc == 3'd0) begin                  // a whole byte
            if (nb == 0) begin
                cmd = sh;
                if (sh == 8'h06) wel = 1'b1;
            end else if (nb <= 3) ad = {ad[15:0], sh};
            else if (cmd == 8'h02 && wel) begin                         // program: AND into the page
                if (ad >= BASE && ad < BASE + 24'h100000) mem[ad - BASE] = mem[ad - BASE] & sh;
                ad = {ad[23:8], ad[7:0] + 8'd1};
            end
            nb = nb + 1;
            // the next byte out
            case (cmd)
                8'h03: outb = (nb >= 4) ? rdb(ad + (nb - 4)) : 8'hFF;
                8'h9F: outb = (nb == 1) ? 8'h0B : (nb == 2) ? 8'h40 : 8'h17;
                8'h05: outb = {6'd0, wel, wip};
                default: outb = 8'hFF;
            endcase
        end
    end
    always @(negedge sclk) if (!cs_n) begin
        miso = outb[7 - bc];
    end
endmodule
