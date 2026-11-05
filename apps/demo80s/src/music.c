/* demo80s: the music. Players on the audio engine (docs/audio.md), any of them at once, each paced by the
 * engine's 48 kHz frame counter (SCOUNT) from poll(), which the drawing calls while it waits and inside long
 * loops:
 *   beeper  the Apple II's speaker: one square voice (12), notes on and off, and key clicks (13)
 *   sid     three chip voices (8-10) as a C64's SID: PWM bass through the filter, 50 Hz chord arpeggios,
 *           a lead with vibrato, drums stolen from the voices (song data below: sid_song())
 *   mod     ../modplay/src/mod.c on voices 0-7 (the generated amiga/finale modules, music_mods.h)
 *   saw     voices 16-31: a supersaw chord (the finale), swelling through the filter
 * Without the engine (fsim) time comes from FRAME and the registers are written to nothing. */
#include "mod.h"
#include "music_mods.h"

#define VREG(v, w) IO(0x1800 + 64 * (v) + 2 * (w))
#define V_CTRL 0
#define V_STEP_LO 1
#define V_STEP_HI 2
#define V_VOL_L 3
#define V_VOL_R 4
#define V_DUTY 5
#define V_ATT 6
#define V_DEC 7
#define V_SUS 8
#define V_REL 9
#define V_NOISE 10
#define C_GATE 1
#define C_ENV 2
#define C_PULSE (1 << 2)
#define C_TRI (2 << 2)
#define C_SAW (3 << 2)
#define C_NOISE (4 << 2)
#define C_FILT (1 << 13)

static uint8_t eng_ok;
static uint32_t scount(void)
{
    uint16_t h, l, h2;
    if (!eng_ok) {                              /* fsim: 800 frames a video frame */
        static uint16_t lastf;
        static uint32_t t;
        uint16_t f = FRAME;
        t += (uint32_t)(uint16_t)(f - lastf) * 800UL;
        lastf = f;
        return t;
    }
    do {
        h = IO(0x1012);
        l = IO(0x1010);
        h2 = IO(0x1012);
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}

/* a chip voice's step for MIDI note m (C7 = 96 and an octave down per right shift) */
static uint32_t mstep(int16_t m)
{
    int16_t o = 0;
    while (m < 96) { m = (int16_t)(m + 12); o++; }
    while (m > 107) { m = (int16_t)(m - 12); o--; }
    return o >= 0 ? note_step7[m - 96] >> o : note_step7[m - 96] << -o;
}
static void v_step(uint8_t v, uint32_t s) { VREG(v, V_STEP_LO) = (uint16_t)s; VREG(v, V_STEP_HI) = (uint16_t)(s >> 16); }
static void v_vol(uint8_t v, uint16_t l, uint16_t r) { VREG(v, V_VOL_L) = l; VREG(v, V_VOL_R) = r; }
static void v_adsr(uint8_t v, uint16_t a, uint16_t d, uint16_t s, uint16_t r)
{
    VREG(v, V_ATT) = a; VREG(v, V_DEC) = d; VREG(v, V_SUS) = s; VREG(v, V_REL) = r;
}
static void v_off(uint8_t v) { VREG(v, V_CTRL) = 0; }
static void filter(uint16_t cut, uint16_t res, uint16_t mode) { IO(0x1004) = cut; IO(0x1006) = res; IO(0x1002) = mode; }

/* ---------------------------------------------------------------- the beeper */
/* the tune: MIDI notes (0 rest) and lengths in 50 Hz ticks; the speaker is on for all but the last 2 */
static const uint8_t beep_tune[] = {
    72, 6, 76, 6, 79, 6, 84, 6, 83, 6, 79, 6, 76, 6, 79, 6,
    81, 6, 77, 6, 74, 6, 77, 6, 81, 6, 84, 6, 83, 6, 81, 6,
    79, 6, 76, 6, 72, 6, 76, 6, 79, 6, 83, 6, 81, 6, 79, 6,
    77, 6, 74, 6, 71, 6, 74, 6, 79, 6, 77, 6, 76, 12, 0, 6,
    84, 4, 83, 4, 81, 4, 79, 6, 81, 6, 79, 6, 76, 6, 72, 12,
    74, 4, 76, 4, 77, 4, 79, 6, 77, 6, 76, 6, 74, 6, 71, 12,
    72, 6, 79, 6, 76, 6, 84, 6, 83, 3, 84, 3, 86, 6, 88, 12, 84, 18, 0, 12,
};
static uint8_t b_on, b_pos, b_left;
static void beep_tick(void)
{
    if (b_left == 1) VREG(12, V_CTRL) = C_PULSE;              /* staccato: the speaker stops */
    if (b_left) { b_left--; return; }
    if (b_pos >= sizeof beep_tune) b_pos = 0;
    {
        uint8_t n = beep_tune[b_pos], len = beep_tune[b_pos + 1];
        b_pos = (uint8_t)(b_pos + 2);
        b_left = (uint8_t)(len - 1);
        if (n) {
            v_step(12, mstep(n));
            VREG(12, V_CTRL) = C_PULSE | C_GATE;
        }
    }
}
static void beep_start(void)
{
    VREG(12, V_DUTY) = 0x800;                   /* a square: the speaker toggled */
    v_vol(12, 0xB0, 0xB0);
    b_pos = 0; b_left = 0; b_on = 1;
}
static void click(void)                         /* a key: a few ms of noise */
{
    VREG(13, V_NOISE) = 0;
    v_step(13, 0x200000);
    v_vol(13, 0x90, 0x90);
    v_adsr(13, 0xFFFF, 900, 0, 900);
    VREG(13, V_CTRL) = C_NOISE | C_ENV;
    VREG(13, V_CTRL) = C_NOISE | C_ENV | C_GATE;
}

/* ---------------------------------------------------------------- the SID */
/* the song: E minor, Em C G D, a row is 6 ticks (50 Hz). Per row, per voice: a MIDI note (0 hold, 1 off),
 * written as bars of 16 rows. Voice 0 bass, voice 1 chord (root; the arpeggio adds its shape), voice 2 lead. */
static const uint8_t sid_bass[64] = {
    40, 0, 52, 0, 40, 52, 40, 0,  40, 0, 52, 0, 40, 52, 43, 45,     /* Em */
    36, 0, 48, 0, 36, 48, 36, 0,  36, 0, 48, 0, 36, 48, 38, 40,     /* C */
    43, 0, 55, 0, 43, 55, 43, 0,  43, 0, 55, 0, 43, 55, 45, 47,     /* G */
    38, 0, 50, 0, 38, 50, 38, 0,  38, 0, 50, 0, 38, 50, 40, 42,     /* D */
};
static const uint8_t sid_chord[4] = {64, 60, 67, 62};           /* E C G D (a bar each) */
static const int8_t sid_shape[4][3] = {{0, 3, 7}, {0, 4, 7}, {0, 4, 7}, {0, 4, 7}};
static const uint8_t sid_lead_a[64] = {
    76, 0, 0, 79, 0, 0, 83, 0,  81, 0, 79, 0, 78, 0, 76, 0,
    72, 0, 0, 76, 0, 0, 79, 0,  81, 0, 79, 0, 76, 0, 72, 0,
    74, 0, 0, 79, 0, 0, 83, 0,  86, 0, 83, 0, 81, 0, 79, 0,
    78, 0, 0, 74, 0, 0, 78, 0,  81, 0, 0, 0, 79, 0, 78, 0,
};
static const uint8_t sid_lead_b[64] = {
    88, 0, 86, 0, 83, 0, 79, 0,  83, 0, 0, 0, 1, 0, 76, 79,
    84, 0, 83, 0, 79, 0, 76, 0,  79, 0, 0, 0, 1, 0, 72, 76,
    86, 0, 83, 0, 79, 0, 74, 0,  79, 0, 83, 0, 86, 0, 88, 0,
    90, 0, 0, 0, 86, 0, 0, 0,  81, 0, 78, 0, 74, 0, 1, 0,
};
static uint8_t s_on, s_row, s_tick, s_bar, s_pat, s_mute_lead;
static uint8_t s_base;                          /* the first engine voice */
static uint16_t s_pwm = 0x300, s_cut = 0x900;
static int8_t s_pwd = 24;
static uint8_t s_ln;                            /* the lead's note, for its vibrato */
static uint8_t s_vib;
static uint8_t s_drum;                          /* ticks of drum left on the chord voice */

static void sid_start(uint8_t base)
{
    s_base = base;
    VREG(base + 0, V_DUTY) = 0x300; v_vol(base + 0, 0xD0, 0xD0); v_adsr(base + 0, 0x4000, 60, 0xA000, 300);
    VREG(base + 1, V_DUTY) = 0x400; v_vol(base + 1, 0x70, 0x60); v_adsr(base + 1, 0x8000, 30, 0x9000, 200);
    VREG(base + 2, V_DUTY) = 0x800; v_vol(base + 2, 0x60, 0x80); v_adsr(base + 2, 0x1800, 20, 0xC000, 120);
    filter(0x900, 0x2000, 1);
    s_row = 0; s_tick = 0; s_bar = 0; s_pat = 0; s_on = 1; s_mute_lead = 0;
}

static void sid_tick(void)
{
    uint8_t b = s_base, k = s_tick;
    /* every tick: PWM on the bass, the filter sweeping, the arpeggio, the lead's vibrato */
    s_pwm = (uint16_t)(s_pwm + s_pwd);
    if (s_pwm > 0xE00 || s_pwm < 0x200) s_pwd = (int8_t)-s_pwd;
    VREG(b, V_DUTY) = s_pwm;
    s_cut = (uint16_t)(0x700 + ((SIN(s_pat * 64 + s_row * 4 + k) + 128) << 4));
    IO(0x1004) = s_cut;
    if (s_drum) {
        s_drum--;
        if (!s_drum) VREG(b + 1, V_CTRL) = C_PULSE | C_ENV | C_GATE;
    } else {
        uint8_t ch = sid_chord[s_bar];
        int8_t sh = sid_shape[s_bar][(uint8_t)(k % 3)];
        v_step(b + 1, mstep((int16_t)(ch + sh)));
    }
    if (s_ln && !s_mute_lead) {
        s_vib++;
        {
            uint32_t st = mstep(s_ln), d = st >> 7;
            v_step(b + 2, (s_vib & 4) ? st + d : st - d);
        }
    }
    if (k == 0) {
        uint8_t r = (uint8_t)(s_bar * 16 + s_row);
        uint8_t bn = sid_bass[r];
        const uint8_t *lead = (s_pat & 1) ? sid_lead_b : sid_lead_a;
        uint8_t ln = lead[r];
        if (bn) {
            v_step(b, mstep(bn));
            VREG(b, V_CTRL) = C_PULSE | C_ENV | C_FILT;
            VREG(b, V_CTRL) = C_PULSE | C_ENV | C_FILT | C_GATE;
        }
        /* drums: noise on the chord voice on the backbeat, the arpeggio back two ticks later */
        if ((s_row & 7) == 4) {
            VREG(b + 1, V_NOISE) = 0;
            v_step(b + 1, 0x300000);
            VREG(b + 1, V_CTRL) = C_NOISE | C_ENV;
            VREG(b + 1, V_CTRL) = C_NOISE | C_ENV | C_GATE;
            s_drum = 3;
        } else if (s_row == 0) {
            VREG(b + 1, V_CTRL) = C_PULSE | C_ENV;
            VREG(b + 1, V_CTRL) = C_PULSE | C_ENV | C_GATE;
        }
        if (!s_mute_lead && s_pat >= 1) {
            if (ln == 1) { VREG(b + 2, V_CTRL) = C_SAW | C_ENV; s_ln = 0; }
            else if (ln) {
                s_ln = ln;
                s_vib = 0;
                v_step(b + 2, mstep(ln));
                VREG(b + 2, V_CTRL) = C_SAW | C_ENV;
                VREG(b + 2, V_CTRL) = C_SAW | C_ENV | C_GATE;
            }
        }
    }
    if (++s_tick >= 6) {
        s_tick = 0;
        if (++s_row >= 16) {
            s_row = 0;
            if (++s_bar >= 4) { s_bar = 0; s_pat++; }
        }
    }
}
static void sid_stop(void)
{
    uint8_t i;
    s_on = 0;
    for (i = 0; i < 3; i++) v_off((uint8_t)(s_base + i));
}

/* ---------------------------------------------------------------- SID effects over the MOD (finale) */
/* three chip voices following the finale module's chords (Em C D B, a bar of 16 rows each): a PWM arpeggio
 * at 50 Hz, a laser zap at each bar, a filtered saw riser in each pattern's last bar. The flood: up to 24
 * more voices (8-31 less the three) arpeggiating the same chord in staggered octaves and phases. */
static const uint8_t fx_ch[4][3] = {{76, 79, 83}, {72, 76, 79}, {74, 78, 81}, {71, 75, 78}};
static uint8_t fl_n;                            /* flood voices (11 ..) */
static uint8_t x_on, x_k, x_lastbar = 0xFF;
static uint16_t x_pwm = 0x200, x_zap;
static void sidfx_start(void)
{
    VREG(8, V_DUTY) = 0x200; v_vol(8, 0x38, 0x58); v_adsr(8, 0xFFFF, 6, 0xC000, 200);
    VREG(8, V_CTRL) = C_PULSE | C_ENV; VREG(8, V_CTRL) = C_PULSE | C_ENV | C_GATE;
    v_vol(9, 0x40, 0x30); v_adsr(9, 0xFFFF, 60, 0, 60);
    v_vol(10, 0x30, 0x30); v_adsr(10, 0x0100, 4, 0xF000, 100);
    x_on = 1; x_lastbar = 0xFF;
}
static void sidfx_tick(void)
{
    uint8_t bar = (uint8_t)((pl.prow >> 4) & 3);
    x_pwm = (uint16_t)(x_pwm + 37);
    if (x_pwm > 0xD00) x_pwm = 0x200;
    VREG(8, V_DUTY) = x_pwm;
    v_step(8, mstep(fx_ch[bar][x_k % 3]));
    x_k++;
    if (bar != x_lastbar) {                     /* a zap: a falling square */
        x_lastbar = bar;
        x_zap = 0xFFFF;
        VREG(9, V_DUTY) = 0x800;
        VREG(9, V_CTRL) = C_PULSE | C_ENV; VREG(9, V_CTRL) = C_PULSE | C_ENV | C_GATE;
        if (bar == 3) {                         /* the riser: through the filter, up an octave over the bar */
            v_step(10, mstep(50));
            VREG(10, V_CTRL) = C_SAW | C_ENV | C_FILT; VREG(10, V_CTRL) = C_SAW | C_ENV | C_FILT | C_GATE;
            filter(0x300, 0x1800, 1);
        } else if (bar == 0)
            VREG(10, V_CTRL) = C_SAW | C_ENV | C_FILT;
    }
    if (x_zap > 0x3000) {
        x_zap = (uint16_t)(x_zap - (x_zap >> 3));
        v_step(9, (uint32_t)x_zap * 12);
    }
    if (bar == 3) {
        uint16_t c = IO(0x1004);
        if (c < 0x3000) IO(0x1004) = (uint16_t)(c + 0x50);
    }
    {
        uint8_t i;
        for (i = 0; i < fl_n; i++) {
            uint8_t v = (uint8_t)(11 + i);
            v_step(v, mstep((int16_t)(fx_ch[bar][(uint8_t)(x_k + i) % 3] - 24 + 12 * (i % 4))));
        }
    }
}
static void flood(uint8_t n)                    /* n flood voices on (0: off) */
{
    uint8_t i;
    for (i = 0; i < 21; i++) {
        uint8_t v = (uint8_t)(11 + i);
        if (i < n && i >= fl_n) {
            VREG(v, V_DUTY) = (uint16_t)(0x300 + i * 0x60);
            v_vol(v, (uint16_t)(i & 1 ? 0x10 : 0x1C), (uint16_t)(i & 1 ? 0x1C : 0x10));
            v_adsr(v, 0x2000, 4, 0xFFFF, 60);
            VREG(v, V_CTRL) = (uint16_t)((i % 3 ? C_PULSE : C_SAW) | C_ENV);
            VREG(v, V_CTRL) = (uint16_t)((i % 3 ? C_PULSE : C_SAW) | C_ENV | C_GATE);
        } else if (i >= n && i < fl_n)
            VREG(v, V_CTRL) = (uint16_t)((i % 3 ? C_PULSE : C_SAW) | C_ENV);
    }
    fl_n = n;
}
static void sidfx_stop(void)
{
    x_on = 0;
    v_off(8); v_off(9); v_off(10);
    flood(0);
}

/* ---------------------------------------------------------------- the supersaw (finale) */
static uint8_t w_on;
static uint16_t w_lvl, w_cut;
static void saw_chord(const uint8_t *notes, uint8_t n)          /* n notes, 3 detuned voices each, from 16 */
{
    uint8_t i, j, v = 16;
    for (i = 0; i < n && v < 32; i++)
        for (j = 0; j < 3 && v < 32; j++, v++) {
            uint32_t st = mstep(notes[i]);
            int32_t det = (int32_t)(st >> 9) * (j - 1);
            v_step(v, (uint32_t)((int32_t)st + det));
            v_vol(v, (uint16_t)((i + j) & 1 ? 0x18 : 0x28), (uint16_t)((i + j) & 1 ? 0x28 : 0x18));
            v_adsr(v, 0x0400, 4, 0xFFFF, 40);
            VREG(v, V_CTRL) = C_SAW | C_ENV | C_FILT;
            VREG(v, V_CTRL) = C_SAW | C_ENV | C_FILT | C_GATE;
        }
    w_on = 1;
    w_cut = 0x200;
}
static void saw_tick(void)
{
    if (w_cut < 0x5000) w_cut = (uint16_t)(w_cut + 0x60);         /* the filter opens */
    IO(0x1004) = w_cut;
}
static void saw_release(void)
{
    uint8_t v;
    for (v = 16; v < 32; v++) VREG(v, V_CTRL) = C_SAW | C_ENV | C_FILT;
}

/* ---------------------------------------------------------------- the MOD */
static uint8_t m_on;
static uint32_t m_next, m_acc;
static void mod_play(const uint8_t *file)
{
    if (mod_parse((uint32_t)file, &mod) == 0) {
        pl.sep = 80;
        mod_start();
        m_on = 1;
        m_next = scount() + 480;
        m_acc = 0;
    }
}
static void mod_stop(void)
{
    m_on = 0;
    mod_silence();
}

/* ---------------------------------------------------------------- the clock */
static uint32_t c_next;                          /* the 50 Hz players' next tick */
uint32_t ticks50;                                /* 50 Hz ticks since start (sections time themselves by it) */
void poll(void)
{
    uint32_t t = scount();
    while ((int32_t)(t - c_next) >= 0) {
        c_next += 960;
        ticks50++;
#ifdef STOP_AT
        if (ticks50 >= STOP_AT) IO(2) = 0;    /* fsim: stop here */
#endif
        if (b_on) beep_tick();
        if (s_on) sid_tick();
        if (w_on) saw_tick();
        if (x_on) sidfx_tick();
    }
    while (m_on && (int32_t)(t - m_next) >= 0) {
        uint32_t f;
        mod_tick();
        m_acc += mod_tick_frames_x100();
        f = m_acc / 100;
        m_acc -= f * 100;
        m_next += f;
    }
}
static void music_init(void)
{
    eng_ok = IO(0x1014) == 0xA032;
    IO(0x1000) = 0x0A00;                        /* MASTER: 0.625 */
    IO(0x1008) = 0;                             /* ENVDIV */
    c_next = scount() + 480;
}
static void music_all_off(void)
{
    uint8_t v;
    b_on = 0; s_on = 0; w_on = 0; x_on = 0;
    mod_stop();
    for (v = 0; v < 32; v++) v_off(v);
}
