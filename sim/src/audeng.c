/* fpga_cpu simulator: the Tang's audio engine (fpga/rtl/audio_engine.v, docs/audio.md), bit exact
 * with its reference model fpga/tools/audio_ref.py (sim/tools/audeng_check.py compares them).
 *
 * 32 voices, an output frame 48,000 times a second of simulated time. Frames are made up to the
 * CPU's cycle count (snd.h): a register write or a read first makes every frame due, so a write
 * takes effect from the next frame, as on the board. The voices read their samples straight from
 * RAM (the model assumes the engine's buffers are always ahead: no underruns). The old 3-voice
 * device at F00400 drives voices 0-2. Interrupt source 3 is pending after any frame that leaves
 * a voice's flag set. */
#include "cpu.h"
#include "snd.h"

#include <string.h>

enum { IDLE, ATK, DEC, SUS, REL };

typedef struct {
    uint16_t cfg[32];
    int64_t pos;                       /* sample: 24.16 frames (40 bits); chip: the 24-bit phase */
    uint32_t env, lfsr;
    uint8_t stage, run, lg, dir, half, done, irq, under, pw, wrap, msb;
} voice_t;

static struct {
    voice_t v[32];
    uint16_t master, fmode, fcut, fres, envdiv;
    int32_t low[2], band[2];
    uint16_t leg[3][3];                /* F00400: freq, vol, wave */
    uint64_t frames;                   /* made so far (SCOUNT) */
} E;

static int16_t s16(int64_t x) { return (int16_t)(uint16_t)x; }
static int64_t sat(int64_t x, int bits)
{
    int64_t lo = -((int64_t)1 << (bits - 1)), hi = ((int64_t)1 << (bits - 1)) - 1;
    return x < lo ? lo : x > hi ? hi : x;
}

void audeng_reset(void)
{
    memset(&E, 0, sizeof E);
    E.master = 0x1000;
    for (int i = 0; i < 32; i++)
        E.v[i].lfsr = 0x7FFFFF;
}

/* ---- memory: byte addresses, 24 bits, little endian */
static uint32_t rd(const cpu_t *c, uint32_t a, int n)
{
    uint32_t x = 0;
    for (int i = 0; i < n; i++) {
        uint32_t b = (a + (uint32_t)i) & 0xFFFFFF;
        x |= (uint32_t)(b < c->ram_size ? c->sram[b] : 0) << (8 * i);
    }
    return x;
}
static int16_t conv8(uint32_t b, int uns) { return s16((int64_t)(uns ? (b ^ 0x80) : b) << 8); }
static int16_t conv16(uint32_t h, int uns) { return s16(uns ? h ^ 0x8000 : h); }

static void getframe(const cpu_t *c, uint32_t ba, int b16, int st, int uns, int16_t *l, int16_t *r)
{
    if (!st && !b16) { *l = *r = conv8(rd(c, ba, 1), uns); return; }
    if (!st) { *l = *r = conv16(rd(c, ba & ~1u, 2), uns); return; }
    if (!b16) { uint32_t h = rd(c, ba & ~1u, 2); *l = conv8(h & 255, uns); *r = conv8(h >> 8, uns); return; }
    uint32_t w = rd(c, ba & ~3u, 4);
    *l = conv16(w & 0xFFFF, uns);
    *r = conv16(w >> 16, uns);
}

/* ---- one voice, one frame */
static const uint8_t NZ[4][2] = { { 15, 1 }, { 15, 6 }, { 17, 3 }, { 23, 5 } };

static void voice(const cpu_t *c, int vi, uint64_t n, int64_t acc[2][2])
{
    voice_t *v = &E.v[vi];
    const uint16_t *k = v->cfg;
    int ctrl = k[0];
    int gate = ctrl & 1, enven = ctrl >> 1 & 1, src = ctrl >> 2 & 7, ring = ctrl >> 5 & 1, sync = ctrl >> 6 & 1;
    int interp = ctrl >> 7 & 1, loopm = ctrl >> 8 & 3, b16 = ctrl >> 10 & 1, uns = ctrl >> 11 & 1;
    int stereo = ctrl >> 12 & 1, filt = ctrl >> 13 & 1, irqen = ctrl >> 14 & 1;
    int fshift = b16 + stereo;
    int64_t step = (int64_t)(k[2] & 0xFF) << 16 | k[1];
    int vl = k[3] & 0x1FF, vr = k[4] & 0x1FF;
    uint32_t duty = k[5] & 0xFFF, atk = k[6], dec = k[7], sus = k[8], rel = k[9], noise = k[10];
    uint32_t addr = (uint32_t)(k[17] & 0xFF) << 16 | k[16];
    int64_t ls = (int64_t)(k[19] & 0xFF) << 16 | k[18], end = (int64_t)(k[21] & 0xFF) << 16 | k[20];
    int pw = v->pw;
    if (!gate && !v->lg && v->env == 0 && !pw && !v->run) {
        v->wrap = 0;                                     /* idle: skipped */
        return;
    }
    v->pw = 0;
    const voice_t *sv = &E.v[(vi - 1) & 31];             /* the previous voice: ring and sync */
    int64_t pos;
    if (pw)
        pos = src == 0 ? (int64_t)k[11] | (int64_t)k[12] << 16 | (int64_t)(k[13] & 0xFF) << 32
                       : (int64_t)k[11] | (int64_t)(k[12] & 0xFF) << 16;
    else
        pos = src == 0 ? v->pos : v->pos & 0xFFFFFF;
    int rise = gate && !v->lg, fall = !gate && v->lg;
    int stage = rise ? ATK : (fall && enven) ? REL : v->stage;
    int run = rise ? 1 : v->run;
    int d = loopm == 2 ? (pw ? 0 : v->dir) : 0;
    int64_t env = v->env;
    /* the envelope */
    if (!enven)
        env = gate ? 0xFFFFFF : 0;
    else if ((n & ((1u << E.envdiv) - 1)) == 0) {
        int64_t delta = ((((int64_t)v->env >> 8) * (stage == REL ? rel : dec)) >> 8) & 0xFFFFFF;
        if (delta < 1)
            delta = 1;
        if (stage == ATK) {
            int64_t e = env + ((int64_t)atk << 4);
            if (e >= 0xFFFFFF) { env = 0xFFFFFF; stage = DEC; } else env = e;
        } else if (stage == DEC) {
            int64_t e = env - delta;
            if (e <= (int64_t)sus << 8) { env = (int64_t)sus << 8; stage = SUS; } else env = e;
        } else if (stage == REL) {
            int64_t e = env - delta;
            if (e <= 0) { env = 0; stage = IDLE; } else env = e;
        }
    }
    int audible = enven ? env != 0 : gate;
    int16_t xL, xR;
    if (src == 0) {                                      /* a sample from RAM */
        int act = run && audible, run_n = run && (gate || audible);
        int64_t npos = pos;
        int nd = d, done = 0;
        if (act) {
            uint32_t ba = (addr + (uint32_t)((pos >> 16) << fshift)) & 0xFFFFFF;
            int16_t aL, aR, bL, bR;
            getframe(c, ba, b16, stereo, uns, &aL, &aR);
            getframe(c, (ba + (1u << fshift)) & 0xFFFFFF, b16, stereo, uns, &bL, &bR);
            int64_t fr = interp ? (pos >> 8) & 0xFF : 0;
            xL = s16(aL + (((int64_t)(bL - aL) * fr) >> 8));
            xR = s16(aR + (((int64_t)(bR - aR) * fr) >> 8));
            int64_t ends = end << 16, lsf = ls << 16;
            int64_t pn = d ? pos - step : pos + step;
            int64_t dE = pn - ends, dS = pn - lsf, ln = ends - lsf;
            int64_t rf = 2 * ends - pn - 1, rb = 2 * lsf - pn, pn2 = (pn - ends) + lsf;
            if (!d) {
                if (dE >= 0) {
                    if (loopm == 1 && ln > 0) npos = pn2 >= ends ? lsf : pn2;
                    else if (loopm == 2 && ln > 0) { npos = rf < 0 ? lsf : rf; nd = 1; }
                    else { npos = ends; done = 1; }
                } else
                    npos = pn;
            } else if (dS < 0) {
                npos = rb;
                nd = 0;
            } else
                npos = pn;
            npos &= ((int64_t)1 << 40) - 1;
        } else
            xL = xR = 0;
        run_n = run_n && !done;
        int64_t mid = (ls + end) >> 1;
        int ih = (npos >> 16) >= mid;
        if (irqen && (done || (loopm != 0 && ih != v->half && !pw)))
            v->irq = 1;
        if (done)
            v->done = 1;
        v->half = (uint8_t)ih;
        v->wrap = v->msb = 0;
        v->pos = npos;
        v->dir = (uint8_t)nd;
        v->run = (uint8_t)run_n;
    } else {                                             /* a chip voice: the phase */
        uint32_t ph_old = (uint32_t)pos & 0xFFFFFF, s = ph_old + (uint32_t)step;
        int syn = sync && sv->wrap;
        uint32_t ph = syn ? 0 : s & 0xFFFFFF, lf = v->lfsr;
        if (src == 4) {
            int nb = NZ[noise & 3][0], tp = NZ[noise & 3][1];
            uint32_t mask = (1u << nb) - 1, l = lf & mask;
            if (l == 0)
                l = mask;
            if (!(ph_old >> 19 & 1) && (s >> 19 & 1) && !syn)
                l = (l >> 1) | (((l ^ (l >> tp)) & 1) << (nb - 1));
            lf = l;
        }
        int wm = (int)(ph >> 23 & 1) ^ (ring && sv->msb);
        int64_t w;
        switch (src) {
        case 1: w = (ph >> 12) < duty ? 0x7FFF : -0x7FFF; break;
        case 2: w = s16((int64_t)((((ph >> 8) & 0x7FFF) ^ (wm ? 0x7FFF : 0)) << 1 ^ 0x8000)); break;
        case 3: w = s16((ph >> 8) ^ 0x8000); break;
        case 4:
            if (noise >> 2 & 1) {
                static const uint8_t bit[8] = { 22, 20, 16, 13, 11, 7, 4, 2 };
                uint32_t u = 0;
                for (int i = 0; i < 8; i++)
                    u |= (lf >> bit[i] & 1) << (7 - i);
                w = s16((u << 8) ^ 0x8000);
            } else
                w = lf & 1 ? 0x7FFF : -0x7FFF;
            break;
        default: w = 0;
        }
        xL = xR = (int16_t)w;
        v->lfsr = lf;
        v->pos = ph;
        v->wrap = s >> 24 & 1;
        v->msb = ph >> 23 & 1;
        v->half = v->run = 0;
        v->dir = (uint8_t)d;
    }
    int64_t e16 = env >> 8;
    int64_t mL = s16(((int64_t)xL * e16) >> 16), mR = s16(((int64_t)xR * e16) >> 16);
    acc[filt][0] += mL * vl;
    acc[filt][1] += mR * vr;
    v->lg = (uint8_t)gate;
    v->env = (uint32_t)env;
    v->stage = (uint8_t)stage;
}

static void frame(cpu_t *c)
{
    int64_t acc[2][2] = { { 0, 0 }, { 0, 0 } };          /* direct, filtered; L, R */
    for (int vi = 0; vi < 32; vi++)
        voice(c, vi, E.frames, acc);
    int16_t out[2];
    for (int ch = 0; ch < 2; ch++) {                     /* the Chamberlin filter, then the mix */
        int64_t fin = sat(acc[1][ch] >> 8, 24), dl = acc[0][ch] >> 8;
        int64_t low = sat(E.low[ch] + (((int64_t)E.band[ch] * E.fcut) >> 15), 24);
        int64_t high = sat(fin - low - (((int64_t)E.band[ch] * E.fres) >> 15), 24);
        int64_t band = sat(E.band[ch] + ((high * E.fcut) >> 15), 24);
        E.low[ch] = (int32_t)low;
        E.band[ch] = (int32_t)band;
        int64_t tot = dl + (E.fmode & 1 ? low : 0) + (E.fmode & 2 ? band : 0) + (E.fmode & 4 ? high : 0);
        out[ch] = (int16_t)sat((tot * E.master) >> 12, 16);
    }
    E.frames++;
    snd_out(out[0], out[1]);
    for (int i = 0; i < 32; i++)
        if (E.v[i].irq) {
            c->dev.irq_pend |= IRQ_AUDIO;
            break;
        }
}

/* the frames due by now: frame k is made at the cycle the 48 kHz clock passes k + 1 */
void audeng_sync(cpu_t *c)
{
    uint64_t want = c->st.cycles * 48000 / dev_cpu_hz;
    while (E.frames < want)
        frame(c);
    snd_next = audeng_next();
}

int audeng_irq(void)
{
    for (int i = 0; i < 32; i++)
        if (E.v[i].irq)
            return 1;
    return 0;
}

uint64_t audeng_next(void) { return ((E.frames + 1) * dev_cpu_hz + 47999) / 48000; }

/* ---- the registers (addresses F00400-F00417, F01000-F01FFF) */
static void leg_apply(int vo)
{
    uint32_t f = E.leg[vo][0], vol = E.leg[vo][1], wv = E.leg[vo][2];
    uint32_t step = f * 349 + ((f * 134) >> 8);
    uint16_t *k = E.v[vo].cfg;
    k[0] = (uint16_t)(((wv + 1) << 2) | (f && vol));
    k[1] = (uint16_t)step;
    k[2] = (step >> 16) & 0xFF;
    k[3] = k[4] = (uint16_t)(vol * 5);
    k[5] = 0x800;
}

void audeng_write(cpu_t *c, uint32_t a, uint16_t val)
{
    audeng_sync(c);
    a &= 0xFFFF;
    if (a >= 0x400 && a < 0x418) {
        int vo = (a - 0x400) >> 3, w = ((a - 0x400) >> 1) & 3;
        static const uint16_t mask[3] = { 0xFFFF, 0xF, 3 };
        if (w < 3) {
            E.leg[vo][w] = val & mask[w];
            leg_apply(vo);
        }
    } else if ((a >> 8) == 0x10) {
        switch ((a >> 1) & 0x7F) {
        case 0: E.master = val; break;
        case 1: E.fmode = val & 7; break;
        case 2: E.fcut = val; break;
        case 3: E.fres = val; break;
        case 4: E.envdiv = val & 7; break;
        case 6: case 7:
            for (int i = 0; i < 16; i++)
                if (val >> i & 1)
                    E.v[i + (((a >> 1) & 0x7F) == 7 ? 16 : 0)].irq = 0;
            break;
        }
    } else if ((a >> 11) == 3) {
        voice_t *v = &E.v[(a >> 6) & 31];
        int w = (a >> 1) & 31;
        v->cfg[w] = val;
        if (w == 13)
            v->pw = 1;                                   /* POS_H commits the position */
        if (w == 14) {
            if (val & 2) v->done = 0;
            if (val & 4) v->irq = 0;
            if (val & 16) v->under = 0;
        }
    }
}

uint16_t audeng_read(cpu_t *c, uint32_t a)
{
    audeng_sync(c);
    a &= 0xFFFF;
    if ((a >> 8) == 0x10) {
        uint32_t irq = 0;
        for (int i = 0; i < 32; i++)
            irq |= (uint32_t)E.v[i].irq << i;
        switch ((a >> 1) & 0x7F) {
        case 6: return (uint16_t)irq;
        case 7: return (uint16_t)(irq >> 16);
        case 8: return (uint16_t)E.frames;
        case 9: return (uint16_t)(E.frames >> 16);
        case 10: return 0xA032;
        default: return 0;                               /* write-only, and UNDERRUNS: never */
        }
    }
    if ((a >> 11) == 3) {                                /* the live state, as of the last frame */
        const voice_t *v = &E.v[(a >> 6) & 31];
        switch ((a >> 1) & 31) {
        case 11: return (uint16_t)v->pos;
        case 12: return (uint16_t)(v->pos >> 16);
        case 13: return (uint16_t)((v->pos >> 32) & 0xFF);
        case 14: return (uint16_t)(v->run | v->done << 1 | v->irq << 2 | v->half << 3 | v->under << 4);
        case 15: return (uint16_t)(v->env >> 8);
        }
    }
    return 0;
}
