/* SID registers -> audio engine registers (docs/audio.md). See ../README.md "Mapping".
 *
 * Engine voices: SID voice v (0..2) plays on engine voice 2v+1. The engine's ring/sync source for a
 * voice is the voice before it, so engine voice 2v is a silent copy of SID voice (v+2)%3's oscillator
 * (same step, saw, full level, volume 0), enabled only while voice v uses ring or sync.
 */
#include <string.h>
#include "sidplay.h"
#include "tables.h"

#define W_CTRL 0
#define W_STEP_LO 1
#define W_STEP_HI 2
#define W_VOLL 3
#define W_VOLR 4
#define W_DUTY 5
#define W_ATTACK 6
#define W_DECAY 7
#define W_SUSTAIN 8
#define W_RELEASE 9
#define W_NOISE 10
#define W_POSF 11
#define W_POSL 12
#define W_POSH 13
#define W_ENV 15

#define G_MASTER 0x1000
#define G_FMODE  0x1002
#define G_FCUT   0x1004
#define G_FRES   0x1006
#define G_ENVDIV 0x1008
#define G_ID     0x1014
#define ENVDIV   3                              /* envelopes tick every 8 frames: 6000 ticks a second */

uint8_t sm_mute;
uint8_t sm_spread = 128;

static uint32_t vcache[6][11];                  /* what the engine holds (0xFFFFFFFF = unknown) */
static uint32_t gcache[5];
static uint16_t atk_tab[16], dec_tab[16];
static uint32_t step_k = 20526;                 /* clock / 48 (the step is Fn x clock / 48000) */
static uint8_t eg[3];                           /* engine gate now */
static int8_t pend[3];                          /* gate to set at the next frame, or -1 */
static uint8_t prev_test[3];
static uint8_t present;
static uint16_t last_fn[3] = {0xFFFF, 0xFFFF, 0xFFFF};
static uint32_t last_step[3], last_k;

static const uint16_t atk_ms[16] = {2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000};
static const uint16_t dec_ms[16] = {6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000};

#ifdef HOST
static uint16_t fake_regs[0x2000];
static volatile uint16_t *regp(uint16_t a) { return &fake_regs[a >> 1]; }
#else
static volatile uint16_t *regp(uint16_t a) { return (volatile uint16_t *)(0xF00000UL + a); }
#endif

static void vw(uint8_t ev, uint8_t w, uint16_t val)
{
    if (vcache[ev][w] != val) {
        vcache[ev][w] = val;
        aud_raw(0x1800 + ev * 64 + w * 2, val);
    }
}

static void gw(uint8_t i, uint16_t addr, uint16_t val)
{
    if (gcache[i] != val) {
        gcache[i] = val;
        aud_raw(addr, val);
    }
}

uint8_t sm_engine_present(void) { return present; }

/* A software copy of the engine's envelope (for the display where there is no engine, e.g. fsim) */
static uint32_t env_lvl[3];
static uint8_t env_st[3];                       /* 0 idle, 1 attack, 2 decay/sustain, 3 release */
static uint16_t e_atk[3], e_dec[3], e_sus[3], e_rel[3];
static uint8_t e_gate[3], e_gprev[3];
static uint16_t tick_acc;

static uint32_t decay_pow(uint16_t d, uint16_t n)   /* (1 - d/65536)^n in 1.16 fixed point */
{
    uint32_t r = 65536UL, f = 65536UL - d;
    while (n) {
        if (n & 1)
            r = (r * f) >> 16;
        f = (f * f) >> 16;
        n >>= 1;
    }
    return r;
}

static void env_sim(void)
{
    uint8_t v;
    uint16_t nt;
    tick_acc += (uint16_t)pl_period48;
    nt = tick_acc >> ENVDIV;
    tick_acc &= (1 << ENVDIV) - 1;
    for (v = 0; v < 3; v++) {
        if (e_gate[v] && !e_gprev[v]) env_st[v] = 1;
        if (!e_gate[v] && e_gprev[v]) env_st[v] = 3;
        e_gprev[v] = e_gate[v];
        if (env_st[v] == 1) {
            env_lvl[v] += 16UL * e_atk[v] * nt;
            if (env_lvl[v] >= 0xFFFFFFUL) {
                env_lvl[v] = 0xFFFFFFUL;
                env_st[v] = 2;
            }
        } else if (env_st[v] == 2) {
            uint32_t sus = (uint32_t)e_sus[v] << 8;
            if (env_lvl[v] > sus) {
                env_lvl[v] = (env_lvl[v] >> 8) * decay_pow(e_dec[v], nt) >> 8;
                if (env_lvl[v] < sus) env_lvl[v] = sus;
            }
        } else if (env_st[v] == 3) {
            env_lvl[v] = (env_lvl[v] >> 8) * decay_pow(e_rel[v], nt) >> 8;
            if (env_lvl[v] == 0) env_st[v] = 0;
        }
    }
}

uint16_t sm_env(uint8_t v)
{
    if (present)
        return *regp(0x1800 + (2 * v + 1) * 64 + W_ENV * 2);
    return (uint16_t)(env_lvl[v] >> 8);
}

void sm_set_clock(uint32_t hz) { step_k = hz / 48; }

void sm_silence(void)
{
    uint8_t e;
    for (e = 0; e < 6; e++)
        vw(e, W_CTRL, 0);
    eg[0] = eg[1] = eg[2] = 0;
    pend[0] = pend[1] = pend[2] = -1;
    memset(env_lvl, 0, sizeof env_lvl);
    memset(env_st, 0, sizeof env_st);
    memset(e_gprev, 0, sizeof e_gprev);
}

void sm_init(void)
{
    uint8_t e, i;
    present = (*regp(G_ID) == 0xA032);
    memset(vcache, 0xFF, sizeof vcache);
    memset(gcache, 0xFF, sizeof gcache);
    for (i = 0; i < 16; i++) {
        /* attack: +16 x A of 2^24 per tick, 6000 ticks/s: A = 2^24 / (16 x 6 x ms) = 174763 / ms (<= 65535).
         * decay/release: exponential, time constant 0.62 x the datasheet time (the SID's 255 -> 0 takes
         * about 3x its linear time, and the first e-fold takes 161 of 757 steps): DECAY = 65536 / (6 x 0.62 x ms) */
        uint32_t a = 174763UL / atk_ms[i];
        uint32_t d = (17617UL + dec_ms[i] / 2) / dec_ms[i];
        atk_tab[i] = a > 65535 ? 65535 : (uint16_t)a;
        dec_tab[i] = d < 1 ? 1 : (uint16_t)d;
    }
    /* every engine voice we might hear from a previous program: off */
    for (e = 0; e < 32; e++)
        aud_raw(0x1800 + e * 64 + W_CTRL * 2, 0);
    for (e = 0; e < 6; e++)
        vw(e, W_CTRL, 0);
    for (e = 0; e < 3; e++) {
        vw(2 * e + 1, W_NOISE, 7);              /* 23-bit LFSR, multi-level */
        vw(2 * e, W_VOLL, 0);
        vw(2 * e, W_VOLR, 0);
    }
    gw(4, G_ENVDIV, ENVDIV);
    memset(eg, 0, sizeof eg);
    memset(prev_test, 0, sizeof prev_test);
    pend[0] = pend[1] = pend[2] = -1;
}

static uint16_t cutoff(uint16_t fc)             /* SID FC (11 bits) -> engine FCUT */
{
    uint16_t i = fc >> 4, fr = fc & 15;
    int32_t a = fcut_tab[i], b = fcut_tab[i + 1];
    return (uint16_t)(a + (((b - a) * fr) >> 4));
}

void sm_apply(const frame_t *f)
{
    const uint8_t *r = f->r;
    uint8_t v;
    uint8_t filt = r[23] & 7, vol = r[24] & 15;
    uint16_t fcut = cutoff((uint16_t)((r[21] & 7) + r[22] * 8u));
    uint16_t fres = fres_tab[r[23] >> 4];
    uint32_t room = 65536UL - fres;
    uint32_t fmax = (room >> 1) + (room >> 2) + (room >> 3);   /* Chamberlin stability: F < 2 - damping, with a margin (0.875) */
    if (fcut > fmax)
        fcut = (uint16_t)fmax;
    gw(0, G_MASTER, (uint16_t)(vol * 205u));       /* 0xC03 at 15: 0.75, headroom for three voices */
    gw(1, G_FMODE, (r[24] >> 4) & 7);
    gw(2, G_FCUT, fcut);
    gw(3, G_FRES, fres);
    for (v = 0; v < 3; v++) {                   /* steps: Fn x clock / 48000, only when Fn changed */
        uint16_t fn = (uint16_t)(r[v * 7] + r[v * 7 + 1] * 256u);
        if (fn != last_fn[v] || last_k != step_k) {
            last_fn[v] = fn;
            last_step[v] = (uint32_t)fn * step_k / 1000;
        }
    }
    last_k = step_k;
    for (v = 0; v < 3; v++) {
        const uint8_t *q = r + v * 7;
        uint8_t ctrl = q[4], wave = ctrl >> 4, src, ring = 0, sync = (ctrl >> 1) & 1, g, e = 2 * v + 1;
        uint16_t pw = (uint16_t)(q[2] + (q[3] & 15) * 256u);
        uint32_t step = last_step[v];
        uint16_t duty = pw == 0 ? 4095 : 4096 - pw, vl, vr, c;
        int16_t pan = (int16_t)v - 1, dl;
        uint8_t silent = 0, test = (ctrl >> 3) & 1, usecopy;
        /* waveform: noise > pulse > saw > triangle for combined waves */
        if (wave & 8) src = 4;
        else if (wave & 4) src = 1;
        else if (wave & 2) src = 3;
        else if (wave & 1) src = 2;
        else src = 5;
        if (test) src = 5;
        if (src == 2 && (ctrl & 4))
            ring = 1;
        usecopy = ring || sync;
        if (v == 2 && (r[24] & 0x80) && !((filt >> 2) & 1))
            silent = 1;                         /* voice 3 off (it still reaches the filter) */
        if (sm_mute & (1 << v))
            silent = 1;
        /* gate sequencing: the engine sees one level per frame, so a gate 0->1 or 1->0 inside one play call
         * becomes a frame of the opposite level first */
        {
            uint8_t want = ctrl & 1, rf = (f->rise >> v) & 1, ff = (f->fall >> v) & 1;
            if (pend[v] >= 0) {
                g = (uint8_t)pend[v];
                pend[v] = -1;
            } else if (rf && ff && eg[v] == 1 && want == 1) {
                g = 0;
                pend[v] = 1;
            } else if (rf && ff && eg[v] == 0 && want == 0) {
                g = 1;
                pend[v] = 0;
            } else {
                g = want;
            }
            eg[v] = g;
#ifdef AUDDBG
            aud_raw(0xFE00 + v, rf | ff << 1 | want << 2 | g << 3 | (pend[v] + 1) << 4);
#endif
        }
        /* envelope */
        e_gate[v] = g;
        e_atk[v] = atk_tab[q[5] >> 4];
        e_dec[v] = dec_tab[q[5] & 15];
        e_sus[v] = (uint16_t)((q[6] >> 4) * 0x1111u);
        e_rel[v] = dec_tab[q[6] & 15];
        vw(e, W_ATTACK, atk_tab[q[5] >> 4]);
        vw(e, W_DECAY, dec_tab[q[5] & 15]);
        vw(e, W_SUSTAIN, (uint16_t)((q[6] >> 4) * 0x1111u));
        vw(e, W_RELEASE, dec_tab[q[6] & 15]);
        /* pitch, pulse width */
        vw(e, W_STEP_LO, (uint16_t)step);
        vw(e, W_STEP_HI, (uint16_t)(step >> 16) & 0xFF);
        vw(e, W_DUTY, duty);
        /* stereo: voice 1 left, 2 centre, 3 right by sm_spread/512 of the gain */
        dl = (int16_t)((int32_t)0x40 * sm_spread * pan / 512);
        vl = silent ? 0 : (uint16_t)(0x40 - dl);
        vr = silent ? 0 : (uint16_t)(0x40 + dl);
        vw(e, W_VOLL, vl);
        vw(e, W_VOLR, vr);
        if (test && !prev_test[v]) {            /* test bit: the oscillator is held at zero */
            aud_raw(0x1800 + e * 64 + W_POSF * 2, 0);
            aud_raw(0x1800 + e * 64 + W_POSL * 2, 0);
            aud_raw(0x1800 + e * 64 + W_POSH * 2, 0);
        }
        prev_test[v] = test;
        c = (uint16_t)(g | 2 | src << 2 | ring << 5 | sync << 6 | ((filt >> v) & 1) << 13);
        vw(e, W_CTRL, c);
        /* the modulator copy: SID voice (v+2)%3's pitch, silent */
        if (usecopy) {
            uint8_t s = (v + 2) % 3;
            uint32_t ms = last_step[s];
            vw(2 * v, W_STEP_LO, (uint16_t)ms);
            vw(2 * v, W_STEP_HI, (uint16_t)(ms >> 16) & 0xFF);
            vw(2 * v, W_CTRL, 0x0D);            /* gate, no envelope (full level), saw */
        } else {
            vw(2 * v, W_CTRL, 0);
        }
    }
    env_sim();
}
