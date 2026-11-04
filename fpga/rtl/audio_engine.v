`timescale 1ns/1ps
// The audio engine (docs/audio.md): 32 voices through one sequencer, one
// state-variable filter, a stereo mix, 16-bit stereo out. A rammux client
// (read only) for the samples, a register port (audio_regs.v) for the CPU.
//
// One 48 kHz frame = the 32 voices in turn (3 clocks for an idle voice,
// 30 for a playing one), then the filter and the mix (~20 clocks), then the
// frame waits for s_ready (the FIFO has room). At 67.5 MHz a frame has 1406
// clocks; 32 playing voices take ~1000.
//
// Per voice: its configuration is read from two block RAMs (even and odd
// words of the voice's 32), its state from small RAMs; a sample voice reads
// its frame from a 16-word ring in block RAM that bursts of 8 words from RAM
// keep filled ahead of the position (or behind it, going backward in a
// ping-pong loop). A frame that is not there yet (underrun) holds the last
// one and sets the voice's underrun flag. Chip voices (pulse, triangle,
// saw, noise) run a 24-bit phase in the position register's low bits.
//
// Pipeline arithmetic (the Python model audio_ref.py does the same, bit for bit):
//   x    = frame (or oscillator), 16-bit signed
//   m    = (x * env16) >> 16                    env16 = envelope[23:8]
//   acc += m * vol                              vol 0x100 = 1.0 (9 bits)
//   filter (Chamberlin, per channel, 24-bit signed state):
//     low += (band*F)>>15;  high = in - low - (band*Q)>>15;  band += (high*F)>>15
//   out  = sat16(((acc_direct>>8) + filtered) * master >> 12)
module audio_engine #(
    parameter AW = 22,                      // word address bits of the rammux (22: 16 MB)
    parameter [15:0] BASE = 16'h1000,
    parameter [15:0] LEG  = 16'h0400
) (
    input  wire        clk,
    // CPU register port (see audio_regs.v)
    input  wire        io_wr,
    input  wire [15:0] io_addr,
    input  wire [7:0]  io_wdata,
    input  wire [15:0] rd_addr,
    output wire [15:0] rd_data,
    output wire        irq,
    // rammux client port
    output reg         p_req = 1'b0,
    output wire        p_we,
    output wire [AW-1:0] p_addr,
    output wire [4:0]  p_len,
    input  wire        p_ack,
    input  wire        p_rvalid,
    input  wire [31:0] p_rdata,
    // samples out
    output wire        s_valid,
    output reg  [15:0] s_l = 16'd0,
    output reg  [15:0] s_r = 16'd0,
    input  wire        s_ready
);
    assign p_we = 1'b0;
    assign p_len = 5'd8;

    // ------------------------------------------------------------ registers
    wire        cfg_we, ev_pw, ev_clr_st, ev_clr_irq;
    wire [9:0]  cfg_wa;
    wire [15:0] cfg_wd;
    wire [2:0]  ev_clr_bits;
    wire [31:0] ev_clr_mask;
    wire [15:0] master, fcut, fres;
    wire [2:0]  fmode, envdiv;
    reg  [31:0] v_run = 0, v_arm = 0, v_lg = 0, v_dir = 0, v_half = 0, v_done = 0, v_irq = 0,
                v_under = 0, v_pw = 0, v_fh = 0, v_hok = 0, v_hneed = 0, v_inf = 0, v_stale = 0, v_cmp = 0, v_fd = 0, v_msb = 0, v_wrap = 0;
    reg  [31:0] scount = 32'd0;
    reg  [15:0] ucount = 16'd0;
    reg         sn_we = 1'b0;
    reg  [6:0]  sn_wa = 7'd0;
    reg  [15:0] sn_wd = 16'd0;
    audio_regs #(.BASE(BASE), .LEG(LEG)) regs (
        .clk(clk), .io_wr(io_wr), .io_addr(io_addr), .io_wdata(io_wdata),
        .rd_addr(rd_addr), .rd_data(rd_data),
        .cfg_we(cfg_we), .cfg_wa(cfg_wa), .cfg_wd(cfg_wd), .ev_pw(ev_pw),
        .ev_clr_st(ev_clr_st), .ev_clr_bits(ev_clr_bits), .ev_clr_irq(ev_clr_irq), .ev_clr_mask(ev_clr_mask),
        .master(master), .fmode(fmode), .fcut(fcut), .fres(fres), .envdiv(envdiv),
        .st_run(v_run), .st_done(v_done), .st_irq(v_irq), .st_half(v_half), .st_under(v_under), .st_arm(v_arm),
        .scount(scount), .ucount(ucount), .sn_we(sn_we), .sn_wa(sn_wa), .sn_wd(sn_wd));
    assign irq = v_irq != 32'd0;

    // ------------------------------------------------------------ memories
    reg [15:0] cfge [0:511];
    reg [15:0] cfgo [0:511];
    reg [31:0] ring [0:511];
    reg [31:0] hring [0:255];               // loop heads: 8 words at the loop start, per voice
    reg [21:0] m_hdt [0:31];
    reg [39:0] m_pos [0:31];
    reg [26:0] m_env [0:31];                // {stage, level}
    reg [22:0] m_lfsr [0:31];
    reg [21:0] m_vlo [0:31];
    reg [21:0] m_vhi [0:31];
    reg [31:0] m_last [0:31];
    integer ii;
    initial begin
        for (ii = 0; ii < 512; ii = ii + 1) begin cfge[ii] = 16'd0; cfgo[ii] = 16'd0; ring[ii] = 32'd0; end
        for (ii = 0; ii < 256; ii = ii + 1) hring[ii] = 32'd0;
        for (ii = 0; ii < 32; ii = ii + 1) begin
            m_pos[ii] = 40'd0; m_env[ii] = 27'd0; m_lfsr[ii] = 23'h7FFFFF;
            m_hdt[ii] = 22'd0; m_vlo[ii] = 22'd0; m_vhi[ii] = 22'd0; m_last[ii] = 32'd0;
        end
    end
    always @(posedge clk) begin
        if (cfg_we && !cfg_wa[0]) cfge[{cfg_wa[9:5], cfg_wa[4:1]}] <= cfg_wd;
        if (cfg_we &&  cfg_wa[0]) cfgo[{cfg_wa[9:5], cfg_wa[4:1]}] <= cfg_wd;
    end

    // ------------------------------------------------------------ sequencer
    localparam K0 = 6'd11;
    localparam [2:0] S_IDLE = 3'd0, S_ATK = 3'd1, S_DEC = 3'd2, S_SUS = 3'd3, S_REL = 3'd4;
    localparam T0 = 6'd40;

    reg [4:0]  vi = 5'd0;
    reg [5:0]  st = 6'd0;
    wire [4:0] srcv = vi - 5'd1;

    // configuration of the voice now
    reg [15:0] qe, qo;
    reg [3:0]  pa_d = 4'd15;
    wire [3:0] pa = (st <= 6'd6) ? st[3:0] : (st <= 6'd9) ? st[3:0] + 4'd1 : 4'd15;
    always @(posedge clk) begin qe <= cfge[{vi, pa}]; qo <= cfgo[{vi, pa}]; pa_d <= pa; end
    reg [15:0] c_ctrl, c_stlo, c_sthi, c_vl, c_vr, c_duty, c_atk, c_dec, c_sus, c_rel, c_noise,
               c_pf, c_pl, c_ph, c_alo, c_ahi, c_lslo, c_lshi, c_elo, c_ehi;
    always @(posedge clk)
        case (pa_d)
            4'd0: {c_ctrl, c_stlo} <= {qe, qo};
            4'd1: {c_sthi, c_vl}   <= {qe, qo};
            4'd2: {c_vr, c_duty}   <= {qe, qo};
            4'd3: {c_atk, c_dec}   <= {qe, qo};
            4'd4: {c_sus, c_rel}   <= {qe, qo};
            4'd5: {c_noise, c_pf}  <= {qe, qo};
            4'd6: {c_pl, c_ph}     <= {qe, qo};
            4'd8: {c_alo, c_ahi}   <= {qe, qo};
            4'd9: {c_lslo, c_lshi} <= {qe, qo};
            4'd10: {c_elo, c_ehi}  <= {qe, qo};
            default: ;
        endcase
    wire        gate   = c_ctrl[0];
    wire        enven  = c_ctrl[1];
    wire [2:0]  src    = c_ctrl[4:2];
    wire        interp = c_ctrl[7];
    wire [1:0]  loopm  = c_ctrl[9:8];
    wire [1:0]  fshift = {1'b0, c_ctrl[10]} + {1'b0, c_ctrl[12]};
    wire [23:0] c_step = {c_sthi[7:0], c_stlo};
    wire [23:0] c_addr = {c_ahi[7:0], c_alo};
    wire [23:0] c_ls   = {c_lshi[7:0], c_lslo};
    wire [23:0] c_end  = {c_ehi[7:0], c_elo};

    // the voice's state
    reg [39:0] pos_q;
    reg [26:0] env_q;
    reg [22:0] lfsr_q;
    reg [21:0] vlo_q, vhi_q;
    reg [31:0] last_q;
    reg [21:0] hdt_q;
    always @(posedge clk) begin
        hdt_q <= m_hdt[vi];
        pos_q <= m_pos[vi]; env_q <= m_env[vi]; lfsr_q <= m_lfsr[vi];
        vlo_q <= m_vlo[vi]; vhi_q <= m_vhi[vi]; last_q <= m_last[vi];
    end

    // multiplier lanes: operands registered, product registered (2 clocks)
    reg signed [26:0] ma1 = 0, ma2 = 0;
    reg signed [17:0] mb1 = 0, mb2 = 0;
    reg signed [44:0] mp1 = 0, mp2 = 0;
    always @(posedge clk) begin mp1 <= ma1 * mb1; mp2 <= ma2 * mb2; end
    reg signed [17:0] ea = 0, eb = 0;       // the envelope's decay lane
    reg signed [35:0] ep = 0;
    always @(posedge clk) ep <= ea * eb;

    // working registers
    reg        pw_s, w_lg;
    reg [39:0] w_pos;
    reg [2:0]  w_stage;
    reg        w_run, w_dir, audible, inf_now, avail_r, act;
    reg [23:0] w_env;
    reg [23:0] baddr0, baddr1, baddrE, baddrL;
    reg [21:0] ew, lsw;
    reg        sel0m, sel1m, fq_h;
    reg [21:0] w0r, w1r, w_vlo, w_vhi;
    reg [1:0]  off0, off1;
    reg [31:0] rd0, rd1;
    reg signed [15:0] aL, aR, bL, bR, xL, xR, xo, hL, hR;
    reg signed [41:0] pn, dE, dS, ln, rf, rb, pn2;
    reg [39:0] n_pos;
    reg        n_dir, n_run, n_done;
    reg [24:0] ph_sum;
    reg [23:0] ph_n;
    reg [22:0] n_lfsr;
    reg        wrapev, wrap_n, msb_n, ih, n_armed, launch, headl, under_ev;
    reg [23:0] mid;
    reg [26:0] sn_env_r;
    reg [39:0] sn_pos_r;
    reg [4:0]  sn_v;
    reg [2:0]  sn_cnt = 3'd4;

    reg [31:0] ring_q, hring_q;
    wire [21:0] hoff0 = w0r - lsw, hoff1 = w1r - lsw;
    wire [8:0] ring_ra  = (st == K0 + 6'd6) ? {vi, w0r[3:0]} : {vi, w1r[3:0]};
    wire [7:0] hring_ra = (st == K0 + 6'd6) ? {vi, hoff0[2:0]} : {vi, hoff1[2:0]};
    always @(posedge clk) begin ring_q <= ring[ring_ra]; hring_q <= hring[hring_ra]; end

    // mix
    reg signed [31:0] accDL = 0, accDR = 0, accFL = 0, accFR = 0;
    reg signed [23:0] fin_l, fin_r, low_l = 0, low_r = 0, band_l = 0, band_r = 0, high_l, high_r;
    reg signed [25:0] dl_l, dl_r, tot_l, tot_r;
    reg signed [23:0] flt_l, flt_r;

    // fetch interface
    reg        fq_go = 1'b0;
    reg [4:0]  fq_v = 5'd0;
    reg [AW-1:0] fq_a = 0;
    reg        fbusy = 1'b0;
    reg        f_cmp = 1'b0;
    reg [4:0]  f_cv = 5'd0, f_v = 5'd0;
    reg [AW-1:0] f_a = 0;
    reg [3:0]  f_cnt = 4'd0;
    reg        f_h = 1'b0;

    assign s_valid = (st == T0 + 6'd16);
    wire [15:0] emask = (16'h0001 << envdiv) - 16'h0001;
    wire        etick = (scount[15:0] & emask) == 16'd0;

    // saturation
    function signed [23:0] sat24(input signed [31:0] x);
        sat24 = (x[31:23] == 9'h000 || x[31:23] == 9'h1FF) ? x[23:0] : x[31] ? 24'sh800000 : 24'sh7FFFFF;
    endfunction
    function signed [15:0] sat16(input signed [33:0] x);
        sat16 = (x[33:15] == 19'h00000 || x[33:15] == 19'h7FFFF) ? x[15:0] : x[33] ? 16'sh8000 : 16'sh7FFF;
    endfunction
    wire signed [29:0] sh15_1 = mp1[44:15], sh15_2 = mp2[44:15];
    wire signed [32:0] sh12_1 = mp1[44:12], sh12_2 = mp2[44:12];
    // sample bytes and halves to signed 16
    function [15:0] cv16(input [15:0] h, input uns);
        cv16 = uns ? {~h[15], h[14:0]} : h;
    endfunction
    function [15:0] cv8(input [7:0] b, input uns);
        cv8 = {uns ? ~b[7] : b[7], b[6:0], 8'h00};
    endfunction
    // frame at byte offset off of a word: {R, L}
    function [31:0] frame(input [31:0] w, input [1:0] off, input [2:0] f);   // f: {uns, st, 16}
        reg [7:0]  b;
        reg [15:0] h;
        reg [15:0] l, r;
        begin
            b = w >> {off, 3'b000};
            h = off[1] ? w[31:16] : w[15:0];
            case (f[1:0])
                2'b00: begin l = cv8(b, f[2]); r = l; end
                2'b01: begin l = cv16(h, f[2]); r = l; end
                2'b10: begin l = cv8(h[7:0], f[2]); r = cv8(h[15:8], f[2]); end
                default: begin l = cv16(w[15:0], f[2]); r = cv16(w[31:16], f[2]); end
            endcase
            frame = {r, l};
        end
    endfunction

    // window
    wire [21:0] wbot = w0r;
    wire [21:0] wtop = interp ? w1r : w0r;
    wire [21:0] top1 = wtop + 22'd1;
    wire [21:0] anchor = w_dir ? top1 : wbot;
    wire        inwin = w_dir ? ((w_vlo <= top1) && (top1 <= w_vhi)) : ((w_vlo <= wbot) && (wbot <= w_vhi));
    wire        hok_now = v_hok[vi] && (hdt_q == lsw);
    wire        in_m0 = (w_vlo <= w0r) && (w0r < w_vhi);
    wire        in_m1 = (w_vlo <= w1r) && (w1r < w_vhi);
    wire        in_h0 = hok_now && (hoff0 < 22'd8);
    wire        in_h1 = hok_now && (hoff1 < 22'd8);
    wire        availw = (in_m0 || in_h0) && (!interp || in_m1 || in_h1);
    wire [21:0] ahead = w_dir ? (top1 - w_vlo) : (w_vhi - wbot);
    wire        need = ahead < 22'd8;
    wire        isamp = (src == 3'd0);

    // the oscillators
    wire        sync_hit = c_ctrl[6] & v_wrap[srcv];
    wire        ringb = c_ctrl[5] & v_msb[srcv];
    reg [4:0]   nz_n;
    reg [4:0]   nz_t;
    reg [22:0]  nz_mask, nz_l;
    reg         nz_fb;
    always @* begin
        case (c_noise[1:0])
            2'd0: begin nz_n = 5'd15; nz_t = 5'd1; end
            2'd1: begin nz_n = 5'd15; nz_t = 5'd6; end
            2'd2: begin nz_n = 5'd17; nz_t = 5'd3; end
            default: begin nz_n = 5'd23; nz_t = 5'd5; end
        endcase
        nz_mask = (23'd1 << nz_n) - 23'd1;
        nz_l = lfsr_q & nz_mask;
        if (nz_l == 23'd0) nz_l = nz_mask;
        nz_fb = nz_l[0] ^ nz_l[nz_t];
    end
    reg [15:0] wave;
    reg [15:0] wu;
    reg [7:0]  nu8;
    reg        wm;
    always @* begin
        wu = 16'd0; nu8 = 8'd0; wm = ph_n[23] ^ ringb;
        case (src)
            3'd1: wave = (ph_n[23:12] < c_duty[11:0]) ? 16'h7FFF : 16'h8001;
            3'd2: begin wu = {ph_n[22:8] ^ {15{wm}}, 1'b0}; wave = {~wu[15], wu[14:0]}; end
            3'd3: begin wu = ph_n[23:8]; wave = {~wu[15], wu[14:0]}; end
            3'd4: begin
                nu8 = {n_lfsr[22], n_lfsr[20], n_lfsr[16], n_lfsr[13], n_lfsr[11], n_lfsr[7], n_lfsr[4], n_lfsr[2]};
                wave = c_noise[2] ? {~nu8[7], nu8[6:0], 8'h00} : (n_lfsr[0] ? 16'h7FFF : 16'h8001);
            end
            default: wave = 16'd0;
        endcase
    end

    // envelope arithmetic
    wire signed [25:0] e_l = {2'b00, env_q[23:0]};
    wire [23:0]  e_delta = (ep[31:8] == 24'd0) ? 24'd1 : ep[31:8];
    wire signed [25:0] e_dn = {2'b00, w_env} - {2'b00, e_delta};
    wire signed [25:0] e_up = {2'b00, w_env} + {6'd0, c_atk, 4'd0};
    wire signed [25:0] e_s24 = {2'b00, c_sus, 8'h00};

    // the pos arithmetic, 42-bit signed
    wire signed [41:0] posS  = {2'b00, w_pos};
    wire signed [41:0] stepS = {18'd0, c_step};
    wire signed [41:0] endfx = {2'b00, c_end, 16'd0};
    wire signed [41:0] lsfx  = {2'b00, c_ls, 16'd0};
    wire signed [41:0] pnow  = (w_dir ? posS - stepS : posS + stepS);

    wire        dead = !gate && !v_lg[vi] && (env_q[23:0] == 24'd0) && !v_pw[vi] && !v_arm[vi] && !v_run[vi];

    integer q;
    always @(posedge clk) begin
        fq_go <= 1'b0;
        sn_we <= 1'b0;
        // snapshot writer: four words after a voice
        if (sn_cnt != 3'd4) begin
            sn_we <= 1'b1; sn_wa <= {sn_v, sn_cnt[1:0]};
            case (sn_cnt[1:0])
                2'd0: sn_wd <= sn_pos_r[15:0];
                2'd1: sn_wd <= sn_pos_r[31:16];
                2'd2: sn_wd <= {8'd0, sn_pos_r[39:32]};
                default: sn_wd <= sn_env_r[23:8];
            endcase
            sn_cnt <= sn_cnt + 3'd1;
        end

        case (st)
        // ---- idle voice test
        6'd2: begin
            if (dead) begin
                v_wrap[vi] <= 1'b0;
                st <= 6'd0; vi <= vi + 5'd1;
                if (vi == 5'd31) st <= T0;
            end else st <= st + 6'd1;
        end
        6'd6: begin
            pw_s <= v_pw[vi]; v_pw[vi] <= 1'b0;
            st <= st + 6'd1;
        end
        // ---- K0: gate edge, position
        K0: begin
            w_pos <= pw_s ? ((src != 3'd0) ? {16'd0, c_pl[7:0], c_pf} : {c_ph[7:0], c_pl, c_pf}) :
                     (src != 3'd0) ? {16'd0, pos_q[23:0]} : pos_q;
            w_stage <= (gate & ~v_lg[vi]) ? S_ATK : (~gate & v_lg[vi] & enven) ? S_REL : env_q[26:24];
            w_run <= (gate & ~v_lg[vi]) ? 1'b1 : v_run[vi];
            w_dir <= (pw_s ? 1'b0 : v_dir[vi]) & (loopm == 2'd2);
            w_env <= env_q[23:0];
            st <= st + 6'd1;
        end
        // ---- K1: envelope multiply, byte address of the frame
        K0 + 6'd1: begin
            ea <= {2'b00, env_q[23:8]};
            eb <= {2'b00, (w_stage == S_REL) ? c_rel : c_dec};
            baddr0 <= c_addr + (w_pos[39:16] << fshift);
            baddrE <= c_addr + (c_end << fshift) + 24'd3;
            baddrL <= c_addr + (c_ls << fshift);
            st <= st + 6'd1;
        end
        K0 + 6'd2: begin
            w0r <= baddr0[23:2]; off0 <= baddr0[1:0];
            ew <= baddrE[23:2]; lsw <= baddrL[23:2];
            baddr1 <= baddr0 + (24'd1 << fshift);
            st <= st + 6'd1;
        end
        K0 + 6'd3: begin
            w1r <= baddr1[23:2]; off1 <= baddr1[1:0];
            // the envelope
            if (!enven) w_env <= gate ? 24'hFFFFFF : 24'h000000;
            else if (etick) begin
                case (w_stage)
                    S_ATK: if (e_up >= 26'sd16777215) begin w_env <= 24'hFFFFFF; w_stage <= S_DEC; end
                           else w_env <= e_up[23:0];
                    S_DEC: if (e_dn <= e_s24) begin w_env <= e_s24[23:0]; w_stage <= S_SUS; end
                           else w_env <= e_dn[23:0];
                    S_REL: if (e_dn <= 26'sd0) begin w_env <= 24'd0; w_stage <= S_IDLE; end
                           else w_env <= e_dn[23:0];
                    default: ;
                endcase
            end
            st <= st + 6'd1;
        end
        // ---- K4: a finished fetch joins the window
        K0 + 6'd4: begin
            audible <= enven ? (w_env != 24'd0) : gate;
            inf_now <= v_inf[vi] & ~v_cmp[vi];
            w_vlo <= vlo_q; w_vhi <= vhi_q;
            if (v_cmp[vi]) begin
                v_cmp[vi] <= 1'b0; v_inf[vi] <= 1'b0; v_stale[vi] <= 1'b0;
                if (v_fh[vi]) begin
                    v_hok[vi] <= 1'b1; v_hneed[vi] <= 1'b0; m_hdt[vi] <= lsw;
                end else if (!v_stale[vi]) begin
                    if (!v_fd[vi]) begin
                        w_vhi <= vhi_q + 22'd8;
                        w_vlo <= (vhi_q + 22'd8 - vlo_q > 22'd16) ? vhi_q - 22'd8 : vlo_q;
                    end else begin
                        w_vlo <= vlo_q - 22'd8;
                        w_vhi <= (vhi_q - vlo_q + 22'd8 > 22'd16) ? vlo_q + 22'd8 : vhi_q;
                    end
                end
            end
            st <= st + 6'd1;
        end
        // ---- K5: a jump (or a position write) empties the window
        K0 + 6'd5: begin
            if (isamp && (pw_s || !inwin)) begin
                w_vlo <= anchor; w_vhi <= anchor;
                if (inf_now && !v_fh[vi]) v_stale[vi] <= 1'b1;
            end
            if (isamp && pw_s) begin v_hok[vi] <= 1'b0; v_hneed[vi] <= 1'b1; end
            st <= st + 6'd1;
        end
        // ---- K6: is the frame there; fetch more?
        K0 + 6'd6: begin
            avail_r <= availw;
            sel0m <= in_m0; sel1m <= in_m1;
            act <= w_run & audible;
            launch = isamp && (w_run | v_arm[vi] | pw_s | gate) && !inf_now && !fbusy;
            headl = launch && !need && loopm == 2'd1 && v_hneed[vi] && (ew >= w0r) && (ew - w0r <= 22'd12);
            if (launch && (need || headl)) begin
                fq_go <= 1'b1; fq_v <= vi; fq_h <= headl;
                fq_a <= headl ? lsw : w_dir ? (w_vlo - 22'd8) : w_vhi;
                v_inf[vi] <= 1'b1; v_fd[vi] <= w_dir; v_fh[vi] <= headl;
            end
            st <= st + 6'd1;
        end
        K0 + 6'd7: begin
            rd0 <= sel0m ? ring_q : hring_q;
            w_run <= isamp ? (w_run & (gate | audible)) : 1'b0;
            n_armed <= isamp & (pw_s | v_arm[vi]) & ~(w_run & audible) & ~(avail_r & ~need);
            st <= st + 6'd1;
        end
        K0 + 6'd8: begin
            rd1 <= sel1m ? ring_q : hring_q;
            st <= st + 6'd1;
        end
        // ---- K9: pick the frame out of the words; advance; oscillator phase
        K0 + 6'd9: begin
            {aR, aL} <= frame(rd0, off0, {c_ctrl[11], c_ctrl[12], c_ctrl[10]});
            {bR, bL} <= frame((w1r == w0r) ? rd0 : rd1, off1, {c_ctrl[11], c_ctrl[12], c_ctrl[10]});
            pn <= pnow;
            ph_sum <= {1'b0, w_pos[23:0]} + {1'b0, c_step};
            st <= st + 6'd1;
        end
        K0 + 6'd10: begin
            ma1 <= bL - aL; ma2 <= bR - aR;
            mb1 <= interp ? {10'd0, w_pos[15:8]} : 18'd0;
            mb2 <= interp ? {10'd0, w_pos[15:8]} : 18'd0;
            dE <= pn - endfx; dS <= pn - lsfx; ln <= endfx - lsfx;
            rf <= (endfx <<< 1) - pn - 42'sd1;
            rb <= (lsfx <<< 1) - pn;
            // oscillator
            ph_n <= sync_hit ? 24'd0 : ph_sum[23:0];
            wrap_n <= ph_sum[24];
            if (src != 3'd4) n_lfsr <= lfsr_q;
            else if (~w_pos[19] & ph_sum[19] & ~sync_hit)
                n_lfsr <= (nz_l >> 1) | ((nz_fb ? 23'd1 : 23'd0) << (nz_n - 5'd1));
            else n_lfsr <= nz_l;
            st <= st + 6'd1;
        end
        K0 + 6'd11: begin
            pn2 <= dE + lsfx;
            xo <= wave;
            msb_n <= ph_n[23];
            st <= st + 6'd1;
        end
        // ---- K12: the frame, and where the position goes
        K0 + 6'd12: begin
            wrapev <= isamp && act && avail_r && !w_dir && dE >= 0 && loopm == 2'd1 && ln > 0;
            if (isamp) begin
                if (act && avail_r) begin
                    xL <= aL + (mp1 >>> 8);
                    xR <= aR + (mp2 >>> 8);
                    hL <= aL + (mp1 >>> 8);
                    hR <= aR + (mp2 >>> 8);
                end else if (act) begin
                    xL <= last_q[15:0]; xR <= last_q[31:16]; hL <= last_q[15:0]; hR <= last_q[31:16];
                end else begin
                    xL <= 16'd0; xR <= 16'd0; hL <= last_q[15:0]; hR <= last_q[31:16];
                end
            end else begin
                xL <= xo; xR <= xo; hL <= last_q[15:0]; hR <= last_q[31:16];
            end
            under_ev <= isamp && act && !avail_r;
            // position
            n_pos <= w_pos; n_dir <= w_dir; n_done <= 1'b0;
            if (!isamp) n_pos <= {16'd0, ph_n};
            else if (act && avail_r) begin
                if (!w_dir) begin
                    if (dE >= 0) begin
                        if (loopm == 2'd1 && ln > 0) n_pos <= (pn2 >= endfx) ? lsfx[39:0] : pn2[39:0];
                        else if (loopm == 2'd2 && ln > 0) begin n_pos <= (rf < 0) ? lsfx[39:0] : rf[39:0]; n_dir <= 1'b1; end
                        else begin n_pos <= endfx[39:0]; n_done <= 1'b1; end
                    end else n_pos <= pn[39:0];
                end else begin
                    if (dS < 0) begin n_pos <= rb[39:0]; n_dir <= 1'b0; end
                    else n_pos <= pn[39:0];
                end
            end
            st <= st + 6'd1;
        end
        K0 + 6'd13: begin
            // envelope gain
            ma1 <= xL; ma2 <= xR;
            mb1 <= {2'b00, w_env[23:8]}; mb2 <= {2'b00, w_env[23:8]};
            n_run <= isamp ? (w_run & ~n_done) : 1'b0;
            mid <= ({1'b0, c_ls} + {1'b0, c_end}) >> 1;
            st <= st + 6'd1;
        end
        K0 + 6'd14: begin
            ih <= n_pos[39:16] >= mid;
            st <= st + 6'd1;
        end
        K0 + 6'd15: begin
            ma1 <= $signed(mp1[31:16]); ma2 <= $signed(mp2[31:16]);
            mb1 <= {9'd0, c_vl[8:0]}; mb2 <= {9'd0, c_vr[8:0]};
            st <= st + 6'd1;
        end
        K0 + 6'd16: st <= st + 6'd1;
        // ---- K17: mix in, write the state back
        K0 + 6'd17: begin
            if (c_ctrl[13]) begin accFL <= accFL + mp1[31:0]; accFR <= accFR + mp2[31:0]; end
            else begin accDL <= accDL + mp1[31:0]; accDR <= accDR + mp2[31:0]; end
            m_pos[vi] <= n_pos;
            m_env[vi] <= {w_stage, w_env};
            m_lfsr[vi] <= n_lfsr;
            m_vlo[vi] <= w_vlo; m_vhi[vi] <= w_vhi;
            if (isamp && act) m_last[vi] <= {hR, hL};
            else if (pw_s) m_last[vi] <= 32'd0;       // a new start begins from silence
            v_lg[vi] <= gate;
            if (isamp && wrapev) v_hneed[vi] <= 1'b1;
            v_run[vi] <= n_run;
            v_dir[vi] <= n_dir;
            v_arm[vi] <= n_armed;
            v_wrap[vi] <= isamp ? 1'b0 : wrap_n;
            v_msb[vi] <= isamp ? 1'b0 : msb_n;
            v_half[vi] <= isamp ? ih : 1'b0;
            if (n_done) v_done[vi] <= 1'b1;
            if (under_ev) begin v_under[vi] <= 1'b1; ucount <= ucount + 16'd1; end
            if (c_ctrl[14] && ((n_done) || (isamp && loopm != 2'd0 && ih != v_half[vi] && !pw_s)))
                v_irq[vi] <= 1'b1;
            sn_pos_r <= n_pos; sn_env_r <= {w_stage, w_env}; sn_v <= vi; sn_cnt <= 3'd0;
            st <= 6'd0; vi <= vi + 5'd1;
            if (vi == 5'd31) st <= T0;
        end

        // ---- the filter and the mix
        T0: begin
            fin_l <= sat24($signed(accFL) >>> 8); fin_r <= sat24($signed(accFR) >>> 8);
            dl_l <= accDL >>> 8; dl_r <= accDR >>> 8;
            accDL <= 0; accDR <= 0; accFL <= 0; accFR <= 0;
            st <= st + 6'd1;
        end
        T0 + 6'd1: begin
            ma1 <= band_l; mb1 <= {2'b00, fcut}; ma2 <= band_r; mb2 <= {2'b00, fcut};
            st <= st + 6'd1;
        end
        T0 + 6'd2: st <= st + 6'd1;
        T0 + 6'd3: begin
            low_l <= sat24($signed(low_l) + sh15_1); low_r <= sat24($signed(low_r) + sh15_2);
            ma1 <= band_l; mb1 <= {2'b00, fres}; ma2 <= band_r; mb2 <= {2'b00, fres};
            st <= st + 6'd1;
        end
        T0 + 6'd4: st <= st + 6'd1;
        T0 + 6'd5: begin
            high_l <= sat24($signed(fin_l) - $signed(low_l) - sh15_1);
            high_r <= sat24($signed(fin_r) - $signed(low_r) - sh15_2);
            st <= st + 6'd1;
        end
        T0 + 6'd6: begin
            ma1 <= high_l; mb1 <= {2'b00, fcut}; ma2 <= high_r; mb2 <= {2'b00, fcut};
            st <= st + 6'd1;
        end
        T0 + 6'd7: st <= st + 6'd1;
        T0 + 6'd8: begin
            band_l <= sat24($signed(band_l) + sh15_1); band_r <= sat24($signed(band_r) + sh15_2);
            st <= st + 6'd1;
        end
        T0 + 6'd9: begin
            tot_l <= dl_l + (fmode[0] ? low_l : 0) + (fmode[1] ? band_l : 0) + (fmode[2] ? high_l : 0);
            tot_r <= dl_r + (fmode[0] ? low_r : 0) + (fmode[1] ? band_r : 0) + (fmode[2] ? high_r : 0);
            st <= st + 6'd1;
        end
        T0 + 6'd10: begin
            ma1 <= tot_l; ma2 <= tot_r; mb1 <= {2'b00, master}; mb2 <= {2'b00, master};
            st <= st + 6'd1;
        end
        T0 + 6'd11: st <= st + 6'd1;
        T0 + 6'd12: begin
            s_l <= sat16(sh12_1); s_r <= sat16(sh12_2);
            st <= T0 + 6'd16;
        end
        // output: valid until taken
        T0 + 6'd16: if (s_ready) begin
            scount <= scount + 32'd1;
            st <= 6'd0; vi <= 5'd0;
        end
        default: st <= st + 6'd1;
        endcase

        // events from the CPU port
        if (ev_clr_st) begin
            if (ev_clr_bits[0]) v_done[cfg_wa[9:5]] <= 1'b0;
            if (ev_clr_bits[1]) v_irq[cfg_wa[9:5]] <= 1'b0;
            if (ev_clr_bits[2]) v_under[cfg_wa[9:5]] <= 1'b0;
        end
        if (ev_clr_irq) v_irq <= v_irq & ~ev_clr_mask;
        if (f_cmp) v_cmp[f_cv] <= 1'b1;
        if (ev_pw) v_pw[cfg_wa[9:5]] <= 1'b1;
    end

    // ------------------------------------------------------------ the RAM client
    assign p_addr = f_a;
    always @(posedge clk) begin
        f_cmp <= 1'b0;
        if (fq_go) begin fbusy <= 1'b1; f_v <= fq_v; f_a <= fq_a; f_h <= fq_h; p_req <= 1'b1; f_cnt <= 4'd0; end
        if (p_req && p_ack) p_req <= 1'b0;
        if (p_rvalid) begin
            if (f_h) hring[{f_v, f_cnt[2:0]}] <= p_rdata;
            else ring[{f_v, f_a[3:0] + f_cnt[3:0]}] <= p_rdata;
            f_cnt <= f_cnt + 4'd1;
            if (f_cnt == 4'd7) begin fbusy <= 1'b0; f_cmp <= 1'b1; f_cv <= f_v; end
        end
    end
endmodule
