`timescale 1ns/1ps
// The Tang Nano 20K's SPI flash (the configuration flash, an XT25F64B, 8 MB, on the MSPI pins
// after configuration: gowin_pack --mspi_as_gpio) for docs/rom.md:
//
//   - the boot copier: after power-up (start), it reads the ROM image's 32-byte header at
//     ROM_OFS in the flash: "FCRM", the body's length, its load address and its entry (32-bit,
//     little endian; the rest is the BIOS's). With the magic right it copies the body (the bytes
//     after the header) to
//     the load address through the loader's write port (wr/wr_a/wr_d, one byte each time
//     wr_ready allows) and then asks T1 to run from the entry (go). With anything else, or
//     abort (the serial loader's sync word), it does nothing: the serial loader works as before.
//   - the CPU's port, once the copier is done (docs/io.md "Flash (F00900)"):
//       F00900  FL_DATA  write (low byte): send the byte and receive one; read: the byte received
//       F00902  FL_CTRL  write bit 0: 1 = select the flash (CS low); read: bit 0 busy,
//                        bit 1 the ROM was copied from the flash at power-up
//
// SPI mode 0, MSB first, one bit every 2*HALF clocks (HALF 2: fclk/4, 16.9 MHz), the flash's
// bit sampled at our next falling edge (it holds until then); the copier uses READ (03), which
// the flash serves at any clock up to 50 MHz.
module spiflash #(
    parameter [23:0] ROM_OFS = 24'h400000,
    parameter        HALF    = 2
) (
    input  wire        clk,
    input  wire        start,               // a pulse: begin the copy (at power-up, or a reboot)
    input  wire        abort,               // a pulse: the serial loader took over
    output reg         booted = 1'b0,       // the copy finished and T1 was started
    input  wire        wr_ready,            // the loader's write port is free
    output reg         wr = 1'b0,
    output reg  [23:0] wr_a = 24'd0,
    output reg  [7:0]  wr_d = 8'd0,
    output reg         go = 1'b0,           // a pulse: run T1 from go_entry
    output reg  [23:0] go_entry = 24'd0,
    // the CPU's port
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    output wire [15:0] rd,
    // the pins
    output reg         f_cs_n = 1'b1,
    output reg         f_clk = 1'b0,
    output reg         f_mosi = 1'b0,
    input  wire        f_miso
);
    // ---- the byte engine
    reg  [7:0]  tx = 8'd0, rx = 8'd0;
    reg  [3:0]  bit_n = 4'd0;               // bits left
    reg  [3:0]  hc = 4'd0;                  // clocks in this half period
    reg         bsy = 1'b0;
    reg         bstart = 1'b0;              // set with tx to send a byte
    reg  [7:0]  b_tx = 8'd0;
    reg         miso_q = 1'b0;
    wire        bdone;
    reg         bdone_r = 1'b0;
    assign bdone = bdone_r;
    always @(posedge clk) begin
        miso_q  <= f_miso;
        bdone_r <= 1'b0;
        if (bstart && !bsy) begin
            bsy <= 1'b1; tx <= b_tx; bit_n <= 4'd8; hc <= 4'd0;
            f_mosi <= b_tx[7]; f_clk <= 1'b0;
        end else if (bsy) begin
            hc <= hc + 1'b1;
            if (hc == HALF - 1) begin
                hc <= 4'd0;
                if (!f_clk) begin               // rising edge: the flash takes our bit
                    f_clk <= 1'b1;
                end else begin                  // falling edge: the flash's bit, held since the last
                    f_clk <= 1'b0;              // falling edge (2*HALF-1 clocks to settle), and ours out
                    rx <= {rx[6:0], miso_q};
                    tx <= {tx[6:0], 1'b0};
                    f_mosi <= tx[6];
                    if (bit_n == 4'd1) begin bsy <= 1'b0; bdone_r <= 1'b1; end
                    bit_n <= bit_n - 1'b1;
                end
            end
        end
    end

    // ---- the copier
    localparam C_IDLE = 3'd0, C_CMD = 3'd1, C_HDR = 3'd2, C_BODY = 3'd3, C_WRITE = 3'd4, C_DONE = 3'd5, C_DESEL = 3'd6;
    reg  [2:0]  cs = C_IDLE;
    reg  [4:0]  k = 5'd0;                   // command and header bytes
    reg  [31:0] magic = 32'd0;
    reg  [23:0] len = 24'd0, entry = 24'd0;
    reg         owned = 1'b0;               // the copier owns the engine
    wire [7:0]  cmd_b = (k == 5'd0) ? 8'h03 : (k == 5'd1) ? ROM_OFS[23:16] : (k == 5'd2) ? ROM_OFS[15:8] : ROM_OFS[7:0];

    // ---- the CPU's port
    wire        sel = io_addr[15:2] == 14'h0240;          // F00900-F00903
    assign rd = io_addr[1] ? {14'd0, booted, bsy | owned} : {8'd0, rx};

    always @(posedge clk) begin
        go <= 1'b0;
        wr <= 1'b0;
        bstart <= 1'b0;
        if (start && !owned && !bsy) begin  // (again: a reboot, T1 held) a deselect, then the read
            cs <= C_DESEL; owned <= 1'b1; f_cs_n <= 1'b1; k <= 5'd0; booted <= 1'b0;
        end else case (cs)
            C_DESEL: begin
                k <= k + 1'b1;
                if (k == 5'd15) begin cs <= C_CMD; f_cs_n <= 1'b0; k <= 5'd0; end
            end
            C_CMD: if (!bsy && !bstart && !bdone) begin
                if (k == 5'd4) begin cs <= C_HDR; k <= 5'd0; b_tx <= 8'h00; bstart <= 1'b1; end
                else begin b_tx <= cmd_b; bstart <= 1'b1; k <= k + 1'b1; end
            end
            C_HDR: if (bdone) begin         // header bytes 0-15, little endian words (16-31: for the BIOS)
                if (!k[4]) case (k[3:2])
                    2'd0: magic <= {rx, magic[31:8]};
                    2'd1: len   <= k[1:0] == 2'd3 ? len : {rx, len[23:8]};
                    2'd2: wr_a  <= k[1:0] == 2'd3 ? wr_a : {rx, wr_a[23:8]};
                    default: entry <= k[1:0] == 2'd3 ? entry : {rx, entry[23:8]};
                endcase
                k <= k + 1'b1;
                if (k == 5'd31) begin
                    if (magic == 32'h4D524346 && len != 24'd0) begin   // "FCRM", 'F' first
                        cs <= C_BODY; wr_a <= wr_a - 1'b1;            // pre-decremented, as the serial loader's
                        b_tx <= 8'h00; bstart <= 1'b1;
                    end else begin
                        cs <= C_DONE; f_cs_n <= 1'b1;
                    end
                end else begin b_tx <= 8'h00; bstart <= 1'b1; end
            end
            C_BODY: if (bdone) begin wr_d <= rx; cs <= C_WRITE; end
            C_WRITE: if (wr_ready && !wr) begin
                wr <= 1'b1; wr_a <= wr_a + 1'b1;
                len <= len - 1'b1;
                if (len == 24'd1) begin
                    cs <= C_DONE; f_cs_n <= 1'b1; go <= !abort; go_entry <= entry; booted <= !abort;
                end else begin
                    cs <= C_BODY; b_tx <= 8'h00; bstart <= 1'b1;
                end
            end
            default: owned <= 1'b0;         // C_IDLE, C_DONE: the port is the CPU's
        endcase
        if (abort && owned) begin cs <= C_DONE; f_cs_n <= 1'b1; end

        // the CPU's port (T1 runs only after the copier is done)
        if (!owned && io_wr && sel) begin
            if (io_addr[1:0] == 2'd0 && !bsy) begin b_tx <= io_wdata; bstart <= 1'b1; end
            if (io_addr[1:0] == 2'd2) f_cs_n <= !io_wdata[0];
        end
    end
endmodule
