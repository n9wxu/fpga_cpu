`timescale 1ns/1ps
// The audio engine's CPU register port (docs/audio.md), in iosys.v's style:
// 16-bit registers at even addresses on an 8-bit bus; the write to the low
// byte is held, the write to the high byte commits both (STW writes low then
// high).
//
//   BASE+0000..00FF   global registers
//   BASE+0800..0FFF   32 voices, 64 bytes each (word w of voice v at BASE+0800+64v+2w)
//   LEG +0..0017      the old 3-voice sound device (docs/io.md F00400), kept
//                     working: its writes are turned into voice 0-2 settings
//
// A voice's configuration goes to the engine through cfg_we/cfg_wa/cfg_wd
// (one write, registered). Everything the engine owns and the CPU may read
// or clear (position, envelope, flags) comes back through the st_* vectors
// and the snapshot port. Reads: rd_addr is the CPU's address; rd_data is
// valid one clock after rd_addr is stable (registered address, then a mux) (0 if the address is not ours).
module audio_regs #(
    parameter [15:0] BASE = 16'h1000,
    parameter [15:0] LEG  = 16'h0400
) (
    input  wire        clk,
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    input  wire [15:0] rd_addr,
    output reg  [15:0] rd_data,
    // voice configuration writes, to the engine
    output reg         cfg_we = 1'b0,
    output reg  [9:0]  cfg_wa = 10'd0,      // {voice, word}
    output reg  [15:0] cfg_wd = 16'd0,
    output reg         ev_pw = 1'b0,        // with the write: the position was written
    // events to the engine
    output reg         ev_clr_st = 1'b0,    // STATUS written (voice = cfg_wa[9:5])
    output reg  [2:0]  ev_clr_bits = 3'd0,  // 1 = clear done, irq flag, underrun
    output reg         ev_clr_irq = 1'b0,
    output reg  [31:0] ev_clr_mask = 32'd0,
    // global registers
    output reg  [15:0] master = 16'h1000,
    output reg  [2:0]  fmode = 3'd0,
    output reg  [15:0] fcut = 16'd0,
    output reg  [15:0] fres = 16'd0,
    output reg  [2:0]  envdiv = 3'd0,
    // from the engine
    input  wire [31:0] st_run, st_done, st_irq, st_half, st_under, st_arm,
    input  wire [31:0] scount,
    input  wire [15:0] ucount,
    input  wire        sn_we,               // snapshot write: pos, envelope
    input  wire [6:0]  sn_wa,               // {voice, 0:POSF 1:POSL 2:POSH 3:ENV}
    input  wire [15:0] sn_wd
);
    // ---------------------------------------------------------------- bus writes
    reg  [7:0]  lo = 8'd0;
    wire [15:0] wa = {io_addr[15:1], 1'b0};
    wire [15:0] wv = {io_wdata, lo};
    wire        commit = io_wr && io_addr[0];
    always @(posedge clk) if (io_wr && !io_addr[0]) lo <= io_wdata;

    wire w_glob = commit && (wa[15:8] == BASE[15:8]);
    wire w_voic = commit && (wa[15:11] == (BASE[15:11] + 5'd1));  // BASE+0800
    wire w_leg  = commit && (wa[15:5] == LEG[15:5]) && (wa[4:3] != 2'd3);

    // legacy shadow
    reg [15:0] lf [0:2];
    reg [3:0]  lv [0:2];
    reg [1:0]  lw [0:2];
    reg [2:0]  ldirty = 3'd0;
    integer k;
    initial for (k = 0; k < 3; k = k + 1) begin lf[k] = 16'd0; lv[k] = 4'd0; lw[k] = 2'd0; end

    // legacy sequencer: six writes per dirty voice
    reg [2:0]  lk = 3'd0;
    reg [1:0]  lvo = 2'd0;
    reg        lbusy = 1'b0;
    wire [31:0] lstep = lf[lvo] * 32'd349 + ((lf[lvo] * 32'd134) >> 8);   // Hz * 2^24/48000
    reg [15:0] lwd;
    wire       lgate = (lf[lvo] != 16'd0) && (lv[lvo] != 4'd0);
    wire [2:0] lsrc  = {1'b0, lw[lvo]} + 3'd1;      // 1 pulse, 2 triangle, 3 saw, 4 noise
    always @* begin
        case (lk)
            3'd0: lwd = {11'd0, lsrc, 1'b0, lgate};
            3'd1: lwd = lstep[15:0];
            3'd2: lwd = {8'd0, lstep[23:16]};
            3'd3: lwd = {10'd0, lv[lvo], 2'b00} + {12'd0, lv[lvo]};   // vol * 5
            3'd4: lwd = {10'd0, lv[lvo], 2'b00} + {12'd0, lv[lvo]};
            default: lwd = 16'h0800;                                 // duty 50%
        endcase
    end

    wire [4:0] w_v = wa[10:6];
    wire [4:0] w_w = wa[5:1];
    always @(posedge clk) begin
        cfg_we <= 1'b0; ev_pw <= 1'b0; ev_clr_st <= 1'b0; ev_clr_irq <= 1'b0;
        if (w_voic) begin
            cfg_we <= 1'b1; cfg_wa <= {w_v, w_w}; cfg_wd <= wv;
            ev_pw <= (w_w == 5'd13);
            ev_clr_st <= (w_w == 5'd14);
            ev_clr_bits <= {wv[4], wv[2], wv[1]};
        end else if (lbusy) begin
            cfg_we <= 1'b1;
            cfg_wa <= {3'd0, lvo, 5'd0} | {7'd0, lk};
            cfg_wd <= lwd;
            if (lk == 3'd5) lbusy <= 1'b0; else lk <= lk + 3'd1;
        end
        if (!lbusy && ldirty != 3'd0 && !w_voic) begin
            lbusy <= 1'b1; lk <= 3'd0;
            lvo <= ldirty[0] ? 2'd0 : ldirty[1] ? 2'd1 : 2'd2;
            ldirty[ldirty[0] ? 0 : ldirty[1] ? 1 : 2] <= 1'b0;
        end
        if (w_leg && wa[4:3] != 2'd3) begin
            case (wa[2:1])
                2'd0: lf[wa[4:3]] <= wv;
                2'd1: lv[wa[4:3]] <= wv[3:0];
                2'd2: lw[wa[4:3]] <= wv[1:0];
                default: ;
            endcase
            ldirty[wa[4:3]] <= 1'b1;
        end
        if (w_glob) begin
            case (wa[7:1])
                7'd0: master <= wv;
                7'd1: fmode  <= wv[2:0];
                7'd2: fcut   <= wv;
                7'd3: fres   <= wv;
                7'd4: envdiv <= wv[2:0];
                7'd6: begin ev_clr_irq <= 1'b1; ev_clr_mask <= {16'd0, wv}; end
                7'd7: begin ev_clr_irq <= 1'b1; ev_clr_mask <= {wv, 16'd0}; end
                default: ;
            endcase
        end
    end

    // ---------------------------------------------------------------- readback
    // (a voice's configuration words, the global settings and the legacy registers are
    // write-only: they read 0; only what the engine owns reads back)
    reg [15:0] snap [0:127];
    initial for (k = 0; k < 128; k = k + 1) snap[k] = 16'd0;
    always @(posedge clk) if (sn_we) snap[sn_wa] <= sn_wd;

    wire [15:0] ra   = {rd_addr[15:1], 1'b0};
    wire [4:0]  r_v  = ra[10:6];
    wire [4:0]  r_w  = ra[5:1];
    reg  [15:0] sq = 16'd0, stq = 16'd0, gq = 16'd0;
    reg  [15:0] ra_d = 16'd0;
    wire [1:0]  sk = (r_w == 5'd11) ? 2'd0 : (r_w == 5'd12) ? 2'd1 : (r_w == 5'd13) ? 2'd2 : 2'd3;
    always @(posedge clk) begin
        ra_d <= ra;
        sq <= snap[{r_v, sk}];
        stq <= {10'd0, st_arm[r_v], st_under[r_v], st_half[r_v], st_irq[r_v], st_done[r_v], st_run[r_v]};
        case (ra[7:1])
            7'd6: gq <= st_irq[15:0];
            7'd7: gq <= st_irq[31:16];
            7'd8: gq <= scount[15:0];
            7'd9: gq <= scount[31:16];
            7'd10: gq <= 16'hA032;           // id: audio engine, 32 voices
            7'd12: gq <= ucount;
            default: gq <= 16'd0;
        endcase
    end
    wire h_glob = ra_d[15:8] == BASE[15:8];
    wire h_voic = ra_d[15:11] == (BASE[15:11] + 5'd1);
    wire [4:0] d_w = ra_d[5:1];
    always @* rd_data = h_glob ? gq :
                        h_voic ? ((d_w == 5'd11 || d_w == 5'd12 || d_w == 5'd13 || d_w == 5'd15) ? sq :
                                  (d_w == 5'd14) ? stq : 16'd0) : 16'd0;
endmodule
