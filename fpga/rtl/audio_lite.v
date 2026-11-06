`timescale 1ns/1ps
// The low tier's sound (the Spartan-3): the 3-voice chip device of
// docs/io.md (F00400) and a streaming player, mixed to stereo and sent to
// an I2S DAC (e.g. a PCM5102A, which makes its own master clock). The
// Spartan-3 builds it with VOICES = 0: the stream only, software
// synthesizing voices into it (fpga/lib/sid.c).
//
// Rates: an output frame every 1024 clocks of 50 MHz (48,828.125 Hz); the
// bit clock is 50 MHz / 16 (3.125 MHz), 32 bits a channel (16 data bits,
// MSB first, a bit clock after the word clock changes: I2S); the word
// clock is low for the left channel.
//
// Registers (F00400 + a; writes are 16 bits, low byte then high, as in
// iosys.v; only STR_POS reads back):
//   voice v (0-2) at 0x400 + 8v:
//     +0 FREQ  Hz, 0 = silent        +2 VOL  0-15        +4 WAVE  0 square,
//     1 triangle, 2 saw, 3 noise
//   0x420 STR_CTRL  bit 0 play (0 -> 1 starts at the buffer's first frame);
//                   bits 3:1 n - 1: each buffer frame plays for n output
//                   frames (48,828 / n Hz)
//   0x422 STR_VOL   0x100 = 1.0 (bits 8:0)
//   0x424/6 STR_ADDR  the buffer's byte address (a multiple of 4), bits 19:0
//   0x428 STR_LEN   the buffer's length in frames (32-bit words: the left
//                   sample in the low half, the right in the high; 16-bit
//                   signed), 1-65535; the player loops around it
//   0x42A STR_POS   (read) the frame being fetched, 0 .. LEN - 1 (it is
//                   heard two output frames later)
// The program keeps the buffer filled behind STR_POS.
//
// Each voice: a 24-bit phase advancing by FREQ x 2^24 / 48828 a frame;
// square (+-full), triangle (half scale), saw, or noise (a 15-bit LFSR,
// x^15 + x^14 + 1, shifted when phase bit 19 rises; +-full); times
// VOL x 5 / 256 (three voices at full volume do not clip). The mix is
// sat16(voices + stream x STR_VOL / 256). fsim models the same.
//
// Built small for the XC3S200: the registers live in a block RAM (written
// by the CPU, read back once a frame by a 22-clock sequence); the voices'
// phases and LFSRs rotate through three registers instead of being chosen
// by multiplexers; the products are MULT18X18s.
(* shreg_extract = "no" *)     // flip-flops are plentiful here, LUTs are not
module audio_lite #(
    parameter AW = 18,                      // the RAM's word address bits
    parameter VOICES = 1                    // 0: no voices (their registers do nothing): the stream only
) (
    input  wire        clk,                 // 50 MHz
    // the CPU's I/O writes (as iosys.v gets them) and reads
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    input  wire [15:0] rd_addr,
    output wire [15:0] rd_data,             // zero but for STR_POS
    // rammux port: reads of one word
    output reg         p_req = 1'b0,
    output reg  [AW-1:0] p_addr = {AW{1'b0}},
    input  wire        p_ack,
    input  wire        p_rvalid,
    input  wire [31:0] p_rdata,
    // I2S
    output reg         i2s_bck = 1'b0,
    output reg         i2s_lrck = 1'b0,
    output reg         i2s_data = 1'b0
);
    // ---- the frame: 1024 clocks, and a token for each of its first 32 ----
    reg  [9:0]  fc = 10'd0;
    reg  [31:0] tok = 32'd0;
    always @(posedge clk) begin
        fc  <= fc + 10'd1;
        tok <= {tok[30:0], fc == 10'd1023};
    end

    // ---- register writes into the block RAM: words 0x00-0x1F of F00400 ----
    reg  [7:0]  lo = 8'd0;
    wire        ours = io_addr[15:6] == 10'h010;               // 0x0400-0x043F
    (* ram_style = "block" *)
    reg  [15:0] regs [0:31];
    integer k;
    initial for (k = 0; k < 32; k = k + 1) regs[k] = 16'd0;   // as the block RAM powers up
    reg  [15:0] dob = 16'd0;
    always @(posedge clk) begin
        if (io_wr && !io_addr[0]) lo <= io_wdata;
        if (io_wr && io_addr[0] && ours) regs[io_addr[5:1]] <= {io_wdata, lo};
        dob <= regs[fc[4:0]];                // word t, read at clock t, here at t + 1
    end

    // ---- the voices ----------------------------------------------------------
    // word 4v FREQ at t = 4v + 1 in dob, 4v + 1 VOL at 4v + 2, 4v + 2 WAVE at 4v + 3
    wire [35:0] stepp = $signed({2'b00, dob}) * $signed(18'd21990);   // FREQ x 343.59 x 64
    reg  [23:0] step = 24'd0;
    reg  [23:0] ph0 = 24'd0, ph1 = 24'd0, ph2 = 24'd0;
    reg  [14:0] nz0 = 15'd1, nz1 = 15'd1, nz2 = 15'd1;
    reg  [6:0]  vol5 = 7'd0, vol5d = 7'd0;
    reg  [15:0] s = 16'd0;
    wire        rot = tok[2] | tok[6] | tok[10];                   // t = 4v + 2
    wire [23:0] phn = ph0 + step;
    // the sample from the voice just stepped (in ph2, nz2), its WAVE in dob
    wire        sqn = dob[1] ? dob[0] : !dob[0];                     // square or noise
    wire        sgn = dob[0] ? nz2[0] : ph2[23];                     // ... its sign
    // triangle: t = phase[22:8], mirrored in the second half, less 16384
    wire        tt = ph2[22] ^ ph2[23];
    wire [15:0] wav = sqn ? {sgn, {14{!sgn}}, 1'b1} :
                      dob[1:0] == 2'd1 ? {!tt, !tt, ph2[21:8] ^ {14{ph2[23]}}}
                                       : {!ph2[23], ph2[22:8]};
    always @(posedge clk) begin
        step  <= stepp[29:6];
        vol5  <= {1'b0, dob[3:0], 2'b00} + {3'b000, dob[3:0]};
        vol5d <= vol5;
        s     <= wav;
        if (rot) begin
            ph0 <= ph1; ph1 <= ph2; ph2 <= phn;
            nz0 <= nz1; nz1 <= nz2;
            nz2 <= (!ph0[19] && phn[19]) ? {nz0[13:0], nz0[14] ^ nz0[13]} : nz0;
        end
    end
    wire [22:0] m = $signed(s) * $signed({1'b0, vol5d});
    reg  [22:0] mr = 23'd0;
    reg  [17:0] acc = 18'd0;
    always @(posedge clk) begin
        mr <= m;                                                     // at t = 4v + 5
        if (tok[5] | tok[9] | tok[13])
            acc <= (tok[5] ? 18'd0 : acc) + {{3{mr[22]}}, mr[22:8]};
    end

    // ---- the stream: its registers (words 16-20) at t = 17..21 ----
    reg         play = 1'b0;
    reg  [2:0]  rdiv = 3'd0, div = 3'd0;
    reg  [8:0]  svol = 9'd0;
    reg  [17:0] sbase = 18'd0;
    reg  [15:0] slen = 16'd0, pos = 16'd0;
    reg  [31:0] word = 32'd0, wordh = 32'd0;
    always @(posedge clk) begin
        if (tok[17]) begin
            play <= dob[0];
            rdiv <= dob[3:1];
            if (dob[0] && !play) begin pos <= 16'd0; div <= 3'd0; end
            if (!dob[0]) word <= 32'd0;
        end
        if (tok[18]) svol <= dob[8:0];
        if (tok[19]) sbase[13:0] <= dob[15:2];
        if (tok[20]) sbase[17:14] <= dob[3:0];
        if (tok[21]) slen <= dob;
        // fetch the frame at pos (heard next frame), then move on
        if (tok[22] && play) begin
            p_req  <= 1'b1;
            p_addr <= sbase + {2'b00, pos};
        end
        if (p_ack) p_req <= 1'b0;
        if (p_rvalid && play) word <= p_rdata;
        if (tok[23] && play) begin
            if (div == rdiv) begin
                div <= 3'd0;
                pos <= (pos + 16'd1 == slen) ? 16'd0 : pos + 16'd1;
            end else
                div <= div + 3'd1;
        end
    end

    // ---- the mix: left at t = 25, right at 26, from the word fetched last frame
    reg         selr = 1'b0;
    // (the word fetched last frame, held from the frame's start: a fixed delay)
    wire [24:0] sm = $signed(selr ? wordh[31:16] : wordh[15:0]) * $signed({1'b0, svol});
    reg  [24:0] smr = 25'd0;
    wire [17:0] accv = VOICES ? acc : 18'd0;   // (VOICES = 0: the voices' logic is trimmed away)
    wire [17:0] sum = accv + {smr[24], smr[24:8]};                  // (sample x STR_VOL) / 256: 17 bits and the sign
    wire        ovf = sum[17:15] != {3{sum[17]}};
    wire [15:0] sat = ovf ? {sum[17], {15{!sum[17]}}} : sum[15:0];
    reg  [15:0] mixl = 16'd0, mixr = 16'd0, outl = 16'd0, outr = 16'd0;
    always @(posedge clk) begin
        selr <= tok[24];
        smr  <= sm;                                                   // left at 25, right at 26
        if (tok[25]) mixl <= sat;
        if (tok[26]) mixr <= sat;
        if (tok[0]) begin outl <= mixl; outr <= mixr; wordh <= word; end
    end

    // ---- I2S: slot k of a channel (fc[8:4]) carries bit 16 - k for k = 1..16
    wire [4:0]  slot = fc[8:4];
    wire [15:0] ch = fc[9] ? outr : outl;
    wire [3:0]  bi = 4'd0 - slot[3:0];                                // 16 - k
    always @(posedge clk) begin
        i2s_bck  <= fc[3];
        i2s_lrck <= fc[9];
        i2s_data <= (slot != 5'd0 && slot <= 5'd16) ? ch[bi] : 1'b0;
    end

    assign rd_data = ({rd_addr[15:1], 1'b0} == 16'h042A) ? pos : 16'd0;
endmodule
