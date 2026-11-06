`timescale 1ns/1ps
// Microcoded audio engine (issue #23, docs/mcaudio.md): the micro-sequencer and its horizontal
// datapath. One 72-bit micro-word a clock from a CPU-written control store (512 x 72, two block
// RAMs). A node's routine is its per-block control code (envelope, vibrato, arpeggio, sample DMA:
// the "script", as microcode on this datapath) followed by its per-sample loop (32 samples a
// block, counted by CNT) and the write-back of its state words.
//
// Micro-word fields:
//   [71:63] nxt    branch target
//   [62:60] bc     0 next, 1 jump, 2 jz, 3 jnz, 4 jn, 5 jnn, 6 sample loop (CNT<31: CNT++ and jump;
//                  else CNT=0 and fall through), 7 end of the routine
//   [59:56] aop    0 none, 1 add, 2 sub (C: carry, or no borrow), 3 and, 4 or, 5 xor, 6 shr, 7 sar,
//                  8/9 B, 10 min, 11 max (signed), 12 acc at binary point sh[1:0] (0, 8, 15, 16),
//                  13 SYS (sh[2:0]: 0 DMA, 1 TICK, 2 IRQ, 3 ROUTE; below), 14 LFSR step of A,
//                  15 wave of A (sh[1:0]: 0 saw, 1 triangle, 2 pulse with width B[11:0], 3 noise;
//                  sh[2]: ring, the triangle XORed with B's MSB)
//   [55:52] rd  [51:48] ra  [47:44] rb
//   [43:42] bsel   ALU B: 0 RF[rb], 1 imm sign-extended, 2 imm << 16, 3 MDR (the read issued two
//                  words before)
//   [41]    setf   flags N Z C from this ALU result
//   [40:38] mop    MAC: 0 none, 1 acc = a*b, 2 acc += a*b, 3 acc -= a*b, 4 acc = 0
//   [37:36] masel  0 RF[ra][15:0], 1 RF[ra][31:16], 2 MDR[15:0], 3 MDR[31:16]   (signed 16)
//   [35:34] mbsel  0 RF[rb][15:0] signed, 1 RF[rb][31:16] signed, 2 imm, 3 RF[rb][15:0] unsigned
//   [33:31] memop  0 none, 1 node read [imm], 2 node write [imm] = RF[ra], 3 sample read (frame
//                  RF[ra][31:16] + imm base, format sfmt), 4 table read (sine, RF[ra][31:24]),
//                  5 bus add (bus imm[1:0]^ROUTE, entry CNT) += acc at point sh[1:0], 6 bus read
//                  (bus imm[1:0], entry CNT), 7 bus write (bus imm[1:0], entry CNT) = RF[ra]
//   [30:26] sh     shift amount / sub-op
//   [25:24] sfmt   sample format: 0 8-bit, 1 16-bit, 2 16-bit stereo (L low, R high), 3 8-bit unsigned
//   [23:22] pred   write rd only: 0 always, 1 if C, 2 if not C, 3 if N
//   [15:0]  imm
// SYS: 0 DMA: a burst of imm[4:0] words (1-16) from RAM word address RF[ra][23:0] into the sample
//        buffer at word RF[rb][8:0]; the sequencer waits until it has arrived (a word of its own:
//        no other operation, bc 0)
//      1 TICK: rd = 1 in the first block of a tempo tick, else 0
//      2 IRQ: raise the engine's interrupt imm[2:0]
//      3 ROUTE: bus adds go to bus imm[1:0] XOR RF[ra][1:0] (2: the filter buses); reset per routine
// Hazards (left to the microcode): the product reaches acc two words after the MAC word (an acc read
// or a bus add sees acc as it is at its own word); a bus add reads its entry and writes it two clocks
// later (do not add to the same entry within two words); MDR is the read issued two words before.
module mc_core #(parameter SINE = "mc_sine.mem") (
    input  wire        clk,
    input  wire        cs_we,
    input  wire [8:0]  cs_wa,
    input  wire [71:0] cs_wd,
    input  wire        start,
    input  wire [8:0]  entry,
    output reg         busy = 1'b0,
    input  wire        tick,
    output reg  [7:0]  irq_set = 8'd0,      // pulses
    // DMA, served by the walker
    output reg         dma_req = 1'b0,      // a pulse with the fields below
    output reg  [23:0] dma_addr = 24'd0,
    output reg  [4:0]  dma_len = 5'd0,
    output reg  [8:0]  dma_dst = 9'd0,
    input  wire        dma_done,
    // node buffer
    output wire [5:0]  nb_ra,
    input  wire [31:0] nb_rd,
    output wire        nb_we,
    output wire [5:0]  nb_wa,
    output wire [31:0] nb_wd,
    // sample buffer
    output wire [8:0]  sb_ra,
    input  wire [31:0] sb_rd,
    // mix buses: 4 x 32 entries (main L, main R, filter L, filter R)
    output wire [6:0]  bus_ra,
    input  wire [31:0] bus_rd,
    output reg         bus_we = 1'b0,
    output reg  [6:0]  bus_wa = 7'd0,
    output reg  [31:0] bus_wd = 32'd0
);
    // ---- control store
    reg [71:0] cs [0:511];
    reg [71:0] uw = 72'd0;                  // the word now
    reg [8:0]  upc = 9'd0;
    wire [8:0]  nxt   = uw[71:63];
    wire [2:0]  bc    = uw[62:60];
    wire [3:0]  aop   = uw[59:56];
    wire [3:0]  rd    = uw[55:52];
    wire [3:0]  ra    = uw[51:48];
    wire [3:0]  rb    = uw[47:44];
    wire [1:0]  bsel  = uw[43:42];
    wire        setf  = uw[41];
    wire [2:0]  mop   = uw[40:38];
    wire [1:0]  masel = uw[37:36];
    wire [1:0]  mbsel = uw[35:34];
    wire [2:0]  memop = uw[33:31];
    wire [4:0]  sh    = uw[30:26];
    wire [1:0]  sfmt  = uw[25:24];
    wire [1:0]  pred  = uw[23:22];
    wire [15:0] imm   = uw[15:0];
    wire        sys   = aop == 4'd13;
    wire        is_dma = busy && sys && sh[1:0] == 2'd0;

    // the DMA wait: requested, then arrived; the word advances once it has
    reg  [1:0] dst = 2'd0;
    wire       stall = is_dma && dst != 2'd2;
    wire       run = busy && !stall;

    // flags: the flag-setting result is kept, and Z and N are decoded from it (the zero test off the
    // ALU's path); C is kept as it is
    reg  [31:0] fres = 32'd1;
    reg         fc = 1'b0;
    wire        fz = fres == 32'd0;
    wire        fn = fres[31];
    reg [4:0]  cnt = 5'd0;
    reg [8:0]  pc_n;
    always @* begin
        pc_n = upc + 9'd1;
        case (bc)
            3'd1: pc_n = nxt;
            3'd2: if (fz)  pc_n = nxt;
            3'd3: if (!fz) pc_n = nxt;
            3'd4: if (fn)  pc_n = nxt;
            3'd5: if (!fn) pc_n = nxt;
            3'd6: if (cnt != 5'd31) pc_n = nxt;
            default: ;
        endcase
        if (start) pc_n = entry;
    end
    always @(posedge clk) begin
        if (cs_we) cs[cs_wa] <= cs_wd;
        if (start || (run && bc != 3'd7)) begin uw <= cs[pc_n]; upc <= pc_n; end
    end
    // ---- register file: 16 x 32, LUT RAM, two read ports
    reg [31:0] rf [0:15];
    integer i;
    initial for (i = 0; i < 16; i = i + 1) rf[i] = 32'd0;
    wire [31:0] A = rf[ra];
    wire [31:0] B0 = rf[rb];

    reg [1:0] route = 2'd0;
    always @(posedge clk) begin
        dma_req <= 1'b0;
        irq_set <= 8'd0;
        if (start) begin busy <= 1'b1; cnt <= 5'd0; route <= 2'd0; dst <= 2'd0; end
        else if (busy) begin
            if (run && bc == 3'd7) busy <= 1'b0;
            if (run && bc == 3'd6) cnt <= (cnt == 5'd31) ? 5'd0 : cnt + 5'd1;
            if (is_dma) case (dst)
                2'd0: begin dma_req <= 1'b1; dma_addr <= A[23:0]; dma_dst <= B0[8:0];
                            dma_len <= imm[4:0]; dst <= 2'd1; end
                2'd1: if (dma_done) dst <= 2'd2;
                default: dst <= 2'd0;       // the word advances now
            endcase
            if (run && sys && sh[1:0] == 2'd2) irq_set[imm[2:0]] <= 1'b1;
            if (run && sys && sh[1:0] == 2'd3) route <= imm[1:0] ^ A[1:0];
        end
    end

    // ---- memory results (MDR): the read issued by the last word
    reg  [2:0]  mop_d = 3'd0;
    reg  [1:0]  sfmt_d = 2'd0;
    reg  [1:0]  lane_d = 2'd0;
    reg  [15:0] tb [0:255];                 // sine table, 16-bit signed
    reg  [15:0] tb_q = 16'd0;
    initial $readmemh(SINE, tb);
    always @(posedge clk) tb_q <= tb[A[31:24]];
    wire [15:0] fidx = A[31:16];            // frame index, from the position's integer part
    assign sb_ra = imm[8:0] + ((sfmt == 2'd0 || sfmt == 2'd3) ? {2'b00, fidx[15:2]} :
                               (sfmt == 2'd1) ? {1'b0, fidx[15:1]} : fidx[8:0]);
    always @(posedge clk) begin
        mop_d <= run ? memop : 3'd0;
        sfmt_d <= sfmt;
        lane_d <= (sfmt == 2'd1) ? {fidx[0], 1'b0} : fidx[1:0];
    end
    reg [31:0] smp;
    always @* begin
        case (sfmt_d)
            2'd0: begin smp[15:0] = {sb_rd[{lane_d, 3'b000} +: 8], 8'h00}; smp[31:16] = smp[15:0]; end
            2'd3: begin smp[15:0] = {~sb_rd[{lane_d, 3'b111}], sb_rd[{lane_d, 3'b000} +: 7], 8'h00}; smp[31:16] = smp[15:0]; end
            2'd1: begin smp[15:0] = lane_d[1] ? sb_rd[31:16] : sb_rd[15:0]; smp[31:16] = smp[15:0]; end
            default: smp = sb_rd;
        endcase
    end
    // MDR: registered (a read's data reaches the datapath two words after the read: the block RAM's
    // output and the format select get a clock of their own)
    reg [31:0] mdr_c, mdr = 32'd0;
    always @* case (mop_d)
        3'd1: mdr_c = nb_rd;
        3'd3: mdr_c = smp;
        3'd4: mdr_c = {{16{tb_q[15]}}, tb_q};
        3'd6: mdr_c = bus_rd;
        default: mdr_c = nb_rd;
    endcase
    always @(posedge clk) if (!stall) mdr <= mdr_c;

    // ---- ALU
    reg  [31:0] Bv;
    always @* case (bsel)
        2'd0: Bv = B0;
        2'd1: Bv = {{16{imm[15]}}, imm};
        2'd2: Bv = {imm, 16'd0};
        default: Bv = mdr;
    endcase
    reg  signed [39:0] acc = 40'sd0;
    // the ALU in four classes, chosen at the end: arithmetic (one adder: add, sub, min, max), logic
    // (and, or, xor, B: one LUT a bit), shift (one shifter), special (acc, TICK, LFSR, wave)
    wire        sub = aop == 4'd2 || aop == 4'd10 || aop == 4'd11;
    wire [32:0] as = {1'b0, A} + {1'b0, Bv ^ {32{sub}}} + {32'd0, sub};   // C: carry, or no borrow
    wire        lt = (A[31] ^ Bv[31]) ? A[31] : as[31];                   // signed A < B (sub)
    wire [31:0] ar = (aop == 4'd1 || aop == 4'd2) ? as[31:0] : ((lt ^ (aop == 4'd11)) ? A : Bv);
    reg  [1:0]  lop;
    always @* case (aop)
        4'd3: lop = 2'd0;  4'd4: lop = 2'd1;  4'd5: lop = 2'd2;  default: lop = 2'd3;
    endcase
    reg  [31:0] lg;
    always @* case (lop)
        2'd0: lg = A & Bv;  2'd1: lg = A | Bv;  2'd2: lg = A ^ Bv;  default: lg = Bv;
    endcase
    // one shifter, right: logical or arithmetic (a left shift is ADD A,A or a MAC by 2^n)
    wire [31:0] sho = $signed({aop == 4'd7 && A[31], A}) >>> sh;
    // the accumulator at one of four binary points (a finer scale goes through the multiplier)
    // (a continuous assignment: an always @* here never runs in simulation while acc and sh keep
    // their initial values, leaving it X)
    wire [31:0] accp = (sh[1:0] == 2'd0) ? acc[31:0] : (sh[1:0] == 2'd1) ? acc[39:8] :
                       (sh[1:0] == 2'd2) ? {{7{acc[39]}}, acc[39:15]} : {{8{acc[39]}}, acc[39:16]};
    // the wave unit: saw, triangle (ring: XOR with B's MSB), pulse (A's top 12 bits against B), noise
    reg  [15:0] wave;
    always @* case (sh[1:0])
        2'd0: wave = {~A[31], A[30:16]};                                          // saw
        2'd1: wave = {A[30:16] ^ {15{A[31] ^ (sh[2] & B0[31])}}, 1'b0} ^ 16'h8000; // triangle
        2'd2: wave = (A[31:20] < B0[11:0]) ? 16'h7FFF : 16'h8001;                 // pulse
        default: wave = {A[22], A[20], A[16], A[13], A[11], A[7], A[4], A[2], 8'h00};  // noise (SID taps)
    endcase
    wire [31:0] sp = (aop == 4'd12) ? accp : (aop == 4'd14) ? {A[30:0], A[22] ^ A[17]} :
                     (aop == 4'd13) ? {31'd0, tick} : {{16{wave[15]}}, wave};
    reg  [31:0] res;
    always @* case (aop)
        4'd1, 4'd2, 4'd10, 4'd11:   res = ar;
        4'd6, 4'd7:                 res = sho;
        4'd12, 4'd13, 4'd14, 4'd15: res = sp;
        default:                    res = lg;
    endcase
    wire        cout = as[32];
    wire        pass = (pred == 2'd1) ? fc : (pred == 2'd2) ? !fc : (pred == 2'd3) ? fn : 1'b1;
    wire        wr_rf = aop != 4'd0 && (!sys || sh[1:0] == 2'd1);
    always @(posedge clk) if (run && wr_rf && pass) rf[rd] <= res;
    always @(posedge clk) if (run && setf) begin fres <= res; fc <= cout; end

    // ---- the multiplier (one 18 x 18) and the accumulator
    reg  signed [17:0] ma, mb;
    always @* begin
        case (masel)
            2'd0: ma = {{2{A[15]}}, A[15:0]};
            2'd1: ma = {{2{A[31]}}, A[31:16]};
            2'd2: ma = {{2{mdr[15]}}, mdr[15:0]};
            default: ma = {{2{mdr[31]}}, mdr[31:16]};
        endcase
        case (mbsel)
            2'd0: mb = {{2{B0[15]}}, B0[15:0]};
            2'd1: mb = {{2{B0[31]}}, B0[31:16]};
            2'd2: mb = {{2{imm[15]}}, imm};
            default: mb = {2'b00, B0[15:0]};
        endcase
    end
    reg signed [35:0] mp = 36'sd0;
    reg [2:0] mop_p = 3'd0;
    always @(posedge clk) begin
        mp <= ma * mb;
        mop_p <= run ? mop : 3'd0;
        case (mop_p)
            3'd1: acc <= mp;
            3'd2: acc <= acc + mp;
            3'd3: acc <= acc - mp;
            3'd4: acc <= 40'sd0;
            default: ;
        endcase
    end

    // ---- memory ports
    assign nb_ra  = imm[5:0];
    assign nb_we  = run && memop == 3'd2;
    assign nb_wa  = imm[5:0];
    assign nb_wd  = A;
    assign bus_ra = {imm[1:0] ^ (memop == 3'd5 ? route : 2'd0), cnt};
    reg        badd = 1'b0;
    reg [6:0]  bwa = 7'd0;
    reg [31:0] accp_d = 32'd0;
    always @(posedge clk) begin
        badd <= run && memop == 3'd5;
        bwa  <= {imm[1:0] ^ route, cnt};
        accp_d <= accp;
        bus_we <= 1'b0;
        if (badd) begin bus_we <= 1'b1; bus_wa <= bwa; bus_wd <= bus_rd + accp_d; end
        else if (run && memop == 3'd7) begin bus_we <= 1'b1; bus_wa <= {imm[1:0], cnt}; bus_wd <= A; end
    end
endmodule
