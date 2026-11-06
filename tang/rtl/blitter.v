`timescale 1ns/1ps
// The blitter (docs/io.md, "Blitter"): copies or fills a rectangle of bytes
// in the SDRAM while the CPU runs, on a rammux port of its own.
//
// A row is done in chunks of up to 16 destination words (64 bytes). A copy
// reads the chunk's source words (up to 17: the source need not be aligned
// like the destination) into a small buffer, then writes the destination
// words, each made of two neighbouring source words shifted by the
// difference in alignment, with byte enables for the row's edges and (keyed)
// for source bytes equal to the key. A fill writes the fill byte. After each
// chunk's writes the CPU cache's lines that cover them are invalidated
// (inv_req / inv_line, taken by inv_ack), so the CPU reads what the blitter
// wrote.
//
// Registers (F00800-F00817; bytes as iosys takes them: the even byte is
// held, the odd one completes the word): SRC, DST (24 bits), SSTRIDE,
// DSTRIDE (signed), W (12 bits), H, KEY, FILL, CMD (write: bit 0 fill, bit 1
// keyed; starts the blit), STAT (bit 0 busy).
module blitter #(
    parameter AW = 21                       // word address bits (rammux)
) (
    input  wire          clk,
    // I/O (the low 16 bits of the address)
    input  wire          io_wr,
    input  wire [15:0]   io_addr,
    input  wire [7:0]    io_wdata,
    output reg  [15:0]   rd = 16'd0,        // the register at io_addr (a clock behind it)
    output wire          busy,
    // rammux
    output reg           p_req = 1'b0,
    output reg           p_we = 1'b0,
    output reg  [AW-1:0] p_addr = {AW{1'b0}},
    output reg  [4:0]    p_len = 5'd0,
    input  wire          p_ack,
    output wire [31:0]   p_wdata,
    output wire [3:0]    p_wbe,
    input  wire          p_wnext,
    input  wire          p_rvalid,
    input  wire [31:0]   p_rdata,
    // the CPU's cache
    output reg           inv_req = 1'b0,
    output reg  [10:0]   inv_line = 11'd0,
    input  wire          inv_ack
);
    // ---- registers --------------------------------------------------------------------
    reg [23:0] r_src = 0, r_dst = 0;
    reg [15:0] r_ss = 0, r_ds = 0, r_h = 0;
    reg [11:0] r_w = 0;
    reg [7:0]  r_key = 0, r_fill = 0;
    reg [7:0]  lo = 8'd0;
    wire [15:0] w = {io_wdata, lo};
    wire       sel = io_addr[15:5] == 11'h040;  // 0x0800-0x081F
    reg        go = 1'b0;
    reg  [1:0] go_cmd = 2'd0;
    always @(posedge clk) begin
        go <= 1'b0;
        if (io_wr && sel) begin
            if (!io_addr[0]) lo <= io_wdata;
            else case (io_addr[4:1])
                4'h0: r_src[15:0] <= w;
                4'h1: r_src[23:16] <= w[7:0];
                4'h2: r_dst[15:0] <= w;
                4'h3: r_dst[23:16] <= w[7:0];
                4'h4: r_ss <= w;
                4'h5: r_ds <= w;
                4'h6: r_w <= w[11:0];
                4'h7: r_h <= w;
                4'h8: r_key <= w[7:0];
                4'h9: r_fill <= w[7:0];
                4'hA: begin go <= 1'b1; go_cmd <= w[1:0]; end
                default: ;
            endcase
        end
        case (io_addr[4:1])                     // only BLT_STAT reads back: the rest are write-only
            4'hB: rd <= {15'd0, busy};
            default: rd <= 16'd0;
        endcase
    end

    // ---- the blit ---------------------------------------------------------------------
    localparam S_IDLE = 3'd0, S_CHUNK = 3'd1, S_READ = 3'd2, S_WRITE = 3'd3, S_INV = 3'd4, S_NEXT = 3'd5, S_LEN = 3'd6;
    reg  [2:0]  st = S_IDLE;
    assign busy = st != S_IDLE || go;
    reg         fill = 1'b0, keyed = 1'b0;
    reg  [23:0] rs = 0, rdst = 0;           // this row's source and destination
    reg  [15:0] rows = 0;                   // rows left after this one
    reg  [11:0] off = 0;                    // bytes of the row done
    // the chunk
    reg  [23:0] cd = 0, cs = 0;             // its first destination and source byte
    reg  [6:0]  clen = 0;                   // its bytes, 1-64
    reg  [4:0]  dn = 0, sn = 0;             // destination words, source words
    reg  [2:0]  e = 3'd0;                   // source alignment - destination alignment + 4 (1-7)
    reg  [31:0] sb [0:17];                  // the source words (sb[0] is never used for a written byte)
    reg  [4:0]  got = 0;                    // source words back
    reg  [4:0]  k = 0;                      // destination word being written
    reg  [10:0] ivl = 0;
    reg  [2:0]  ivc = 0;

    reg  [11:0] r_w_l = 0;
    reg  [7:0]  r_fill_l = 0, r_key_l = 0;
    wire [11:0] remain = r_w_l - off;
    wire [6:0]  window = 7'd64 - {5'd0, cd[1:0]};
    wire [7:0]  dsum = {6'd0, cd[1:0]} + {1'b0, clen} + 8'd3, ssum = {6'd0, cs[1:0]} + {1'b0, clen} + 8'd3;
    wire [4:0]  dn_c = dsum[6:2], sn_c = ssum[6:2];
    // destination word k: two source words, shifted
    wire [4:0]  q = k + {3'd0, e[2]};
    wire [63:0] pr = {sb[q + 5'd1], sb[q]};
    wire [31:0] dw = pr >> {e[1:0], 3'b000};
    wire [31:0] fw = {4{r_fill_l}};
    assign p_wdata = fill ? fw : dw;
    // byte j of word k is in the chunk when 4k + j - cd[1:0] is in [0, clen)
    wire [7:0]  pos0 = {1'b0, k, 2'b00} - {6'd0, cd[1:0]};
    genvar j;
    wire [3:0]  inr, kmatch;
    generate
        for (j = 0; j < 4; j = j + 1) begin : be
            wire [7:0] p = pos0 + j;
            assign inr[j] = !p[7] && p < {1'b0, clen};
            assign kmatch[j] = keyed && dw[8*j +: 8] == r_key_l;
        end
    endgenerate
    assign p_wbe = inr & ~(fill ? 4'd0 : kmatch);

    always @(posedge clk) begin
        if (p_rvalid && st == S_READ) begin
            sb[got + 5'd1] <= p_rdata;
            got <= got + 1'b1;
        end
        case (st)
            S_IDLE:
                if (go) begin
                    fill <= go_cmd[0]; keyed <= go_cmd[1] && !go_cmd[0];
                    rs <= r_src; rdst <= r_dst; r_w_l <= r_w; r_fill_l <= r_fill; r_key_l <= r_key;
                    rows <= r_h - 1'b1;
                    off <= 12'd0;
                    st <= (r_w == 12'd0 || r_h == 16'd0) ? S_IDLE : S_CHUNK;
                end
            S_CHUNK: begin
                // the chunk: from rdst + off, up to the end of 16 destination words
                cd <= rdst + {12'd0, off};
                cs <= rs + {12'd0, off};
                st <= S_NEXT;
            end
            S_NEXT: begin                   // (cd, cs are in place) the chunk's length
                clen <= remain > {5'd0, window} ? window : remain[6:0];
                e    <= 3'd4 + {1'b0, cs[1:0]} - {1'b0, cd[1:0]};
                got  <= 5'd0;
                k    <= 5'd0;
                st   <= S_LEN;
            end
            S_LEN: begin                    // its words, then read or write
                dn <= dn_c;
                sn <= sn_c;
                if (fill) begin
                    p_req <= 1'b1; p_we <= 1'b1; p_addr <= cd[AW+1:2]; p_len <= dn_c;
                    st <= S_WRITE;
                end else begin
                    p_req <= 1'b1; p_we <= 1'b0; p_addr <= cs[AW+1:2]; p_len <= sn_c;
                    st <= S_READ;
                end
            end
            S_READ: begin
                if (p_ack) p_req <= 1'b0;
                if (!p_req && got == sn) begin
                    p_req <= 1'b1; p_we <= 1'b1; p_addr <= cd[AW+1:2]; p_len <= dn;
                    st <= S_WRITE;
                end
            end
            S_WRITE: begin
                if (p_ack) p_req <= 1'b0;
                if (p_wnext) begin
                    k <= k + 1'b1;
                    if (k == dn - 1'b1) begin
                        // the lines it covered: from cd's line to its last word's
                        ivl <= cd[14:4];
                        ivc <= ({cd[3:2]} + dn - 1'b1) >> 2;   // lines after the first
                        st  <= S_INV;
                    end
                end
            end
            S_INV: begin
                if (!inv_req) begin
                    inv_req <= 1'b1; inv_line <= ivl;
                end else if (inv_ack) begin
                    inv_req <= 1'b0;
                    if (ivc == 3'd0) begin
                        if (off + {5'd0, clen} == r_w_l) begin
                            if (rows == 16'd0) st <= S_IDLE;
                            else begin
                                rows <= rows - 1'b1;
                                rs   <= rs + {{8{r_ss[15]}}, r_ss};
                                rdst <= rdst + {{8{r_ds[15]}}, r_ds};
                                off  <= 12'd0;
                                st   <= S_CHUNK;
                            end
                        end else begin
                            off <= off + {5'd0, clen};
                            st  <= S_CHUNK;
                        end
                    end else begin
                        ivc <= ivc - 1'b1;
                        ivl <= ivl + 1'b1;
                    end
                end
            end
            default: st <= S_IDLE;
        endcase
    end
endmodule
