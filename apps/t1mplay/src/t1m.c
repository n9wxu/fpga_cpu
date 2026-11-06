/* T1M player on the microcoded audio engine: see t1m.h, docs/t1m.md and docs/mcaudio.md.
 *
 * How the features land on the engine:
 *   saw, triangle, pulse, noise   one node each, the osc_* routines: the engine runs the envelope,
 *                                 the LFO (vibrato, PWM) and the pitch within a tick
 *   samples                       one node, smp (8-bit) or smp16; vibrato in software, a tick at a time
 *   pairs                         a linked group of two nodes, the sync routine (hard sync, ring);
 *                                 vibrato in software
 *   unison                        that many nodes (or groups), detuned and panned apart
 *   filters                       one flt node each, after every voice in the list; a voice routed to
 *                                 the filters adds into the filter buses
 * Each channel keeps the nodes its most expensive instrument needs; the ones a note does not use
 * are skipped. Arithmetic is 32-bit wherever a product can pass 16 bits (fcc's int is 16). */
#include "t1m.h"
#include "mc.h"

#include <string.h>

volatile t1m_state_t t1m;

/* ---- reading the module (little endian, any alignment) */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }

static const uint8_t *m_ordr, *m_patt, *m_inst, *m_smpl, *m_sdat, *m_filt, *m_seqs;
static uint8_t m_nch, m_speed0, m_gvol0, m_restart, m_nord, m_ninst, m_nsmp, m_nflt, m_nseq;
static uint16_t m_npat, m_tick0, m_mix;

/* ---- pitch: units of 1/64 semitone from MIDI note 0 */
static const uint16_t semi_q16[12] = { 0, 3897, 8026, 12400, 17034, 21944, 27146, 32657, 38496, 44682, 51236, 58179 };
static const uint16_t frac_q16[64] = {
    0, 59, 118, 178, 237, 296, 356, 415, 475, 535, 594, 654, 714, 773, 833, 893, 953, 1013, 1073, 1134, 1194, 1254,
    1314, 1375, 1435, 1496, 1556, 1617, 1677, 1738, 1799, 1859, 1920, 1981, 2042, 2103, 2164, 2225, 2287, 2348, 2409,
    2471, 2532, 2593, 2655, 2716, 2778, 2840, 2902, 2963, 3025, 3087, 3149, 3211, 3273, 3335, 3397, 3460, 3522, 3584,
    3647, 3709, 3772, 3834 };
static const int16_t sin64[64] = {
    0, 3212, 6393, 9512, 12539, 15446, 18204, 20787, 23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329, 23170, 20787, 18204, 15446, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15446, -18204, -20787, -23170, -25329, -27245, -28898, -30273, -31356, -32137,
    -32609, -32767, -32609, -32137, -31356, -30273, -28898, -27245, -25329, -23170, -20787, -18204, -15446, -12539,
    -9512, -6393, -3212 };
#define STEP0 731558UL                  /* MIDI 0 (8.18 Hz): its phase step a 48 kHz sample, 0.32 */

/* 2^(u/768) as an in-octave ratio (Q16, 65536-131071) and the octaves */
static uint32_t oct_ratio(int32_t u, int16_t *oct)
{
    int16_t o = 0;
    while (u < 0) { u += 768; o--; }
    while (u >= 768) { u -= 768; o++; }
    *oct = o;
    uint32_t a = (0x10000UL + semi_q16[u >> 6]) >> 1, b = (0x10000UL + frac_q16[u & 63]) >> 1;
    return (a * b) >> 14;
}
static uint32_t ratio16(int32_t u)      /* 2^(u/768), 16.16 (u up to 15 octaves) */
{
    int16_t o;
    uint32_t r = oct_ratio(u, &o);
    return o >= 0 ? r << o : r >> -o;
}
static uint32_t osc_step(int32_t u)     /* the oscillators' step, 0.32 */
{
    int16_t o;
    uint32_t r = oct_ratio(u, &o);
    uint32_t s = (STEP0 >> 8) * (r >> 8);  /* STEP0 x r, >> 16 */
    if (o >= 0) {
        if (o > 10) return 0x7FFFFFFFUL;    /* (above the audible range) */
        s = s << o;
        return s > 0x7FFFFFFFUL ? 0x7FFFFFFFUL : s;
    }
    return s >> -o;
}
static int16_t cents_u(int16_t c) { return (int16_t)(((int32_t)c * 64) / 100); }

/* ---- instruments, as the engine wants them */
typedef struct {
    uint8_t src, flags, level, pan, unison, spread, nodes, b16;
    int16_t tune;                       /* transpose and fine tune, units */
    int16_t detune;                     /* the unison spread, units */
    int16_t poff;                       /* pair: the second oscillator's offset, units */
    uint8_t partner;
    uint16_t entry;                     /* the routine */
    uint32_t w9, w10, w11;              /* envelope */
    uint16_t width, pwm;                /* 0-4095 */
    uint16_t lfo256, vibc;              /* LFO rate (Hz x 256), vibrato cents */
    uint32_t saddr, sframes, sloop, sloopaddr, sstep;   /* sample: word address, frames, loop frames, loop word address, 16.16 step at the base note */
    int16_t sbase;                      /* the base note, units */
    uint8_t seq[4];
    const char *name;
} ins_t;
static ins_t ins[255];

typedef struct {                        /* a sequence */
    const uint8_t *step;
    uint8_t kind, len, loop, rel;
} seq_t;
static seq_t seqs[255];

/* the time constant (ms) of an exponential segment as the engine's rate: Q15 of the distance a block */
static uint16_t exp_rate(uint16_t ms)
{
    if (!ms) return 32767;
    uint32_t k = 65536UL / (3UL * ms);  /* 32768 / (ms x 1.5 blocks) */
    return (uint16_t)(k < 1 ? 1 : k > 32767 ? 32767 : k);
}
static uint32_t lfo_word(uint16_t hz256)   /* an LFO rate, Hz x 256, as phase a block (<< 16) */
{
    uint32_t r = ((uint32_t)hz256 << 8) / 1500;
    return (r > 0xFFFF ? 0xFFFF : r) << 16;
}
static uint16_t vib_q15(uint16_t cents) { return (uint16_t)((ratio16(cents_u((int16_t)cents)) - 0x10000UL) >> 1); }

/* ---- filters */
typedef struct {
    uint8_t mode, pan, level, node;
    int32_t cut;                        /* cutoff, units above 20 Hz */
    uint16_t q256, lfo256, sweepc;
    uint8_t dirty;
} flt_t;
static flt_t flt[4];

static uint16_t hz_of(int32_t cut)      /* cutoff units -> Hz */
{
    if (cut < 0) cut = 0;
    uint32_t r = ratio16(cut);
    uint32_t f = (20UL * (r >> 8)) >> 8;
    return (uint16_t)(f > 20000 ? 20000 : f);
}
static int32_t cut_of_hz(uint16_t hz)   /* Hz -> units above 20 Hz */
{
    int32_t u = 0;
    if (hz <= 20) return 0;
    for (int16_t step = 4096; step; step >>= 1)
        if (hz_of(u + step) <= hz) u += step;
    return u;
}
static uint16_t flt_F(uint16_t hz)      /* 2 sin(pi fc / 48000), Q15 */
{
    uint32_t x = ((uint32_t)hz * 102944UL) / 48000UL;     /* pi fc / 48000, Q15 */
    uint32_t x2 = (x * x) >> 15, x3 = (x2 * x) >> 15, x5 = (x3 * x2) >> 15;
    int32_t s = (int32_t)x - (int32_t)(x3 / 6) + (int32_t)(x5 / 120);
    s *= 2;
    return (uint16_t)(s < 0x100 ? 0x100 : s > 0x7000 ? 0x7000 : s);
}

/* ---- channels */
typedef struct {
    uint8_t base, nres;                 /* its nodes */
    uint8_t inst, cfg;                  /* the instrument; the one its nodes are set up for (0xFF none) */
    uint8_t note, gate, on, vol, pan, route, trig, mute;
    int16_t pitch;                      /* the note's pitch with tuning, units */
    int16_t slide;                      /* slides and portamento so far, units */
    int16_t porta_to; uint8_t porta;
    int16_t fine;                       /* 0E, units */
    int16_t poff;                       /* pair offset, units */
    int8_t pslide;
    uint8_t fx[2][2];                   /* this row's effects */
    uint8_t arp;
    uint8_t vslide, wslide;
    uint16_t width;                     /* 0-65535 */
    uint16_t vib_lfo256, vibc, pwm;     /* LFO rate, vibrato cents, PWM depth (width units 0-4095) */
    uint16_t vph;                       /* software vibrato's phase (1/65536 cycle) */
    uint8_t cut_at, off_at, delay_at, retrig;
    uint8_t dnote, dinst, dvol, dflags; /* a delayed cell */
    uint32_t soff;                      /* sample offset, frames */
    uint8_t spos[4], released;
    int16_t spitch;                     /* the pitch sequence's sum, units */
    uint8_t dirty;                      /* engine parameters to write (LFO, width depth) */
} ch_t;
static ch_t ch[T1M_MAXCH];

static uint8_t nnodes;                  /* in the list: voices, then filters */
static uint32_t next_w[MC_NNODES];      /* each node's link to the next (word address) */

/* ---- play position */
static const uint8_t *pp;               /* the row stream */
static uint8_t skip_rows, speed, tick, gvol, ord, row, rows, paused, jump_to, brk_row, flow;
static uint16_t tick_hz256;
static uint32_t period, acc;            /* blocks a tick (16.16), and the count toward it */
static uint16_t last_blocks;

static void W(uint8_t n, uint8_t w, uint32_t v) { MC_NODE(n)[w] = v; }

const char *t1m_inst_name(uint8_t i) { return i < m_ninst ? ins[i].name : ""; }

const char *t1m_error(int e)
{
    static const char *const s[] = { "ok", "not a T1M module", "a newer T1M version", "a chunk is missing or damaged",
                                     "more voices than the engine has", "too big" };
    return e >= 0 && e <= T1M_ESIZE ? s[e] : "?";
}

/* ---- loading */
static uint8_t inst_nodes(const ins_t *in) { return (uint8_t)(in->unison * (in->src == 5 ? 2 : 1)); }

static void load_inst(uint8_t i)
{
    const uint8_t *r = m_inst + 64 * (uint16_t)i;
    ins_t *in = &ins[i];
    in->name = (const char *)r;
    in->src = r[16]; in->flags = r[17]; in->level = r[19]; in->pan = r[20];
    in->unison = r[39] ? (r[39] > 4 ? 4 : r[39]) : 1;
    in->spread = r[42];
    in->tune = (int16_t)((int8_t)r[43] * 64 + cents_u((int8_t)r[44]));
    in->detune = cents_u((int16_t)rd16(r + 40));
    in->poff = cents_u((int16_t)rd16(r + 36));
    in->partner = r[38];
    in->w9 = (uint32_t)(rd16(r + 22) ? 32767UL / (3UL * rd16(r + 22) / 2 + 1) : 32767UL) << 16;
    in->w10 = (uint32_t)exp_rate(rd16(r + 26)) << 16 | exp_rate(rd16(r + 24));
    in->w11 = (uint32_t)(r[21] == 255 ? 32767 : (uint16_t)r[21] << 7) << 16;
    in->width = rd16(r + 28) >> 4;
    in->lfo256 = rd16(r + 30);
    in->vibc = rd16(r + 32);
    in->pwm = rd16(r + 34) >> 4;
    for (uint8_t k = 0; k < 4; k++) in->seq[k] = r[48 + k] < m_nseq ? r[48 + k] : 0xFF;
    in->b16 = 0;
    switch (in->src) {
    case 0: in->entry = MC_OSC_SAW; break;
    case 1: in->entry = MC_OSC_TRI; break;
    case 2: in->entry = MC_OSC_PULSE; break;
    case 3: in->entry = MC_OSC_NOISE; break;
    case 5: in->entry = MC_SYNC; break;
    default: {                          /* a sample */
        const uint8_t *s = m_smpl + 20 * (uint16_t)(r[18] < m_nsmp ? r[18] : 0);
        in->b16 = s[19] & 1;
        in->entry = in->b16 ? MC_SMP16 : MC_SMP;
        const uint8_t *data = m_sdat + rd32(s);
        uint8_t fpw = in->b16 ? 1 : 2;  /* frames to words: >> 1 (16-bit) or >> 2 (8-bit) */
        in->saddr = MC_WADDR(data);
        in->sframes = rd32(s + 4);
        in->sloop = rd32(s + 12);
        in->sloopaddr = MC_WADDR(data) + (rd32(s + 8) >> fpw);
        in->sstep = ((uint32_t)rd16(s + 16) << 12) / 3000;  /* rate / 48000, 16.16 */
        in->sbase = (int16_t)(s[18] * 64);
        if (m_nsmp == 0) in->src = 0, in->entry = MC_OSC_SAW;
    }
    }
    in->nodes = inst_nodes(in);
}

int t1m_load(const void *module)
{
    const uint8_t *m = (const uint8_t *)module;
    if (rd32(m) != 0x1A4D3154UL) return T1M_EMAGIC;
    if (rd16(m + 4) != 1) return T1M_EVERSION;
    uint32_t total = rd32(m + 12), p = 16;
    const uint8_t *song = 0, *name = 0;
    m_ordr = m_patt = m_inst = m_smpl = m_sdat = m_filt = m_seqs = 0;
    while (p + 8 <= total) {
        const uint8_t *c = m + p;
        uint32_t n = rd32(c + 4);
        const uint8_t *d = c + 8;
        if (!memcmp(c, "SONG", 4)) song = d;
        else if (!memcmp(c, "NAME", 4)) name = d;
        else if (!memcmp(c, "ORDR", 4)) m_ordr = d;
        else if (!memcmp(c, "PATT", 4)) m_patt = d;
        else if (!memcmp(c, "INST", 4)) m_inst = d;
        else if (!memcmp(c, "SMPL", 4)) m_smpl = d;
        else if (!memcmp(c, "SDAT", 4)) m_sdat = d;
        else if (!memcmp(c, "FILT", 4)) m_filt = d;
        else if (!memcmp(c, "SEQS", 4)) m_seqs = d;
        p += 8 + ((n + 3) & ~3UL);
    }
    if (!song || !m_ordr || !m_patt || !m_inst) return T1M_ECHUNK;
    m_nch = song[0]; m_speed0 = song[1]; m_tick0 = rd16(song + 2); m_mix = rd16(song + 4);
    m_gvol0 = song[6]; m_restart = song[7]; m_nord = (uint8_t)rd16(song + 8); m_npat = rd16(song + 10);
    m_ninst = song[12]; m_nsmp = song[13]; m_nflt = song[14] > 4 ? 4 : song[14]; m_nseq = song[15];
    if (!m_nch || m_nch > T1M_MAXCH || !m_nord) return T1M_ECHUNK;
    if ((m_nsmp && (!m_smpl || !m_sdat)) || (m_nflt && !m_filt) || (m_nseq && !m_seqs)) return T1M_ECHUNK;
    for (uint8_t i = 0; i < m_nseq; i++) {
        const uint8_t *s = m_seqs + rd16(m_seqs + 2 * (uint16_t)i);
        seqs[i].kind = s[0]; seqs[i].len = s[1]; seqs[i].loop = s[2]; seqs[i].rel = s[3]; seqs[i].step = s + 4;
    }
    for (uint8_t i = 0; i < m_ninst; i++) load_inst(i);
    /* each channel's nodes: what its most expensive instrument needs */
    uint8_t need[T1M_MAXCH];
    memset(need, 0, sizeof need);
    for (uint16_t pt = 0; pt < m_npat; pt++) {
        const uint8_t *s = m_patt + rd32(m_patt + 4 * (uint32_t)pt);
        uint16_t nr = rd16(s), r = 0;
        s += 2;
        while (r < nr) {
            uint8_t b = *s++;
            if (b == 0) r++;
            else if (b >= 0x80) r = (uint16_t)(r + b - 0x7F);
            else {
                uint8_t c = (uint8_t)(b - 1), fl = *s++;
                if (fl & 1) s++;
                if (fl & 2) {
                    uint8_t i = *s++;
                    if (c < m_nch && i < m_ninst && ins[i].nodes > need[c]) need[c] = ins[i].nodes;
                }
                if (fl & 4) s++;
                if (fl & 8) s += 2;
                if (fl & 16) s += 2;
            }
        }
    }
    nnodes = 0;
    for (uint8_t c = 0; c < m_nch; c++) {
        ch[c].base = nnodes;
        ch[c].nres = need[c];
        nnodes = (uint8_t)(nnodes + need[c]);
        if (nnodes > MC_NNODES) return T1M_ENODES;
    }
    for (uint8_t f = 0; f < m_nflt; f++) {
        const uint8_t *r = m_filt + 16 * (uint16_t)f;
        flt[f].mode = r[0]; flt[f].pan = r[1]; flt[f].cut = cut_of_hz(rd16(r + 2)); flt[f].q256 = rd16(r + 4);
        flt[f].lfo256 = rd16(r + 6); flt[f].sweepc = rd16(r + 8); flt[f].level = r[10];
        flt[f].node = nnodes++;
        if (nnodes > MC_NNODES) return T1M_ENODES;
    }
    t1m.title = name ? (const char *)name : "";
    t1m.author = t1m.title + strlen(t1m.title) + 1;
    t1m.comment = t1m.author + strlen(t1m.author) + 1;
    if (!name) t1m.author = t1m.comment = "";
    t1m.channels = m_nch; t1m.orders = m_nord; t1m.ninst = m_ninst;
    t1m.needs = rd16(m + 6); t1m.voices = rd16(m + 8); t1m.nodes = nnodes;
    return T1M_OK;
}

/* ---- the engine side of a voice */
static void link(uint8_t n, uint8_t linked) { W(n, 0, next_w[n] | (linked ? 0x80000000UL : 0)); }

static void configure(ch_t *c, uint8_t i)  /* set the channel's nodes up for instrument i */
{
    const ins_t *in = &ins[i];
    for (uint8_t k = 0; k < c->nres; k++) {
        uint8_t n = (uint8_t)(c->base + k);
        uint8_t used = k < in->nodes;
        uint8_t slave = in->src == 5 && (k & 1);
        link(n, used && in->src == 5 && !slave);
        W(n, 1, slave ? (uint32_t)MC_SILENT : used ? in->entry : (uint32_t)MC_SILENT | 0x80000000UL);   /* (a group runs its head's routine) */
        if (!used || slave) continue;
        W(n, 9, in->w9); W(n, 10, in->w10); W(n, 11, in->w11);
        W(n, 12, 0); W(n, 13, 0); W(n, 14, 0);
        if (in->src <= 3) {
            W(n, 5, in->width); W(n, 6, lfo_word(in->lfo256)); W(n, 7, in->pwm); W(n, 8, vib_q15(in->vibc));
            W(n, 15, in->src == 3 ? 1 : (uint32_t)(k * 16384u) << 16);   /* noise: the LFSR's seed */
        } else if (in->src == 4) {
            W(n, 5, in->saddr); W(n, 6, in->sframes << 16); W(n, 7, in->sloop << 16); W(n, 8, in->sloopaddr);
        } else {
            W(n, 5, (in->flags & 2 ? 1u : 0u) | (in->flags & 4 ? 2u : 0u));
        }
    }
    c->cfg = i;
}

static int16_t clampi(int32_t v, int16_t lo, int16_t hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }

static uint8_t seq_val(ch_t *c, const ins_t *in, uint8_t kind, int16_t *val)
{
    uint8_t s = in->seq[kind];
    if (s == 0xFF || seqs[s].kind != kind) return 0;
    const seq_t *q = &seqs[s];
    uint8_t pos = c->spos[kind];
    if (pos >= q->len) pos = (uint8_t)(q->len - 1);
    *val = kind == 1 || kind == 2 ? (int16_t)(int8_t)q->step[pos] : (int16_t)q->step[pos];
    return 1;
}

static void seq_advance(ch_t *c, const ins_t *in)
{
    for (uint8_t kind = 0; kind < 4; kind++) {
        uint8_t s = in->seq[kind];
        if (s == 0xFF) continue;
        const seq_t *q = &seqs[s];
        uint8_t end = (!c->released && q->rel != 0xFF) ? q->rel : q->len;   /* held: loop before the release point */
        if (kind == 2)                  /* pitch steps accumulate, one a tick */
            c->spitch = (int16_t)(c->spitch + (int8_t)q->step[c->spos[2] < q->len ? c->spos[2] : q->len - 1] * 4);
        uint8_t pos = (uint8_t)(c->spos[kind] + 1);
        if (pos >= end) pos = (!c->released && q->loop != 0xFF && q->loop < end) ? q->loop : (uint8_t)(end - 1);
        c->spos[kind] = pos;
    }
}

/* write the voice's pitch, volume and width for this tick */
static void update(uint8_t ci)
{
    ch_t *c = &ch[ci];
    if (c->cfg == 0xFF) return;
    const ins_t *in = &ins[c->cfg];
    int16_t v;
    int32_t u = (int32_t)c->pitch + c->slide + c->fine + c->spitch;
    if (seq_val(c, in, 1, &v)) u += (int32_t)v * 64;
    if (c->arp) {
        uint8_t a = (uint8_t)(tick % 3);
        u += a == 1 ? (int32_t)(c->arp >> 4) * 64 : a == 2 ? (int32_t)(c->arp & 15) * 64 : 0;
    }
    if (in->src >= 4 && c->vibc) {      /* software vibrato: samples and pairs have no LFO in the engine */
        int16_t s = sin64[c->vph >> 10];
        u += ((int32_t)cents_u((int16_t)c->vibc) * s) >> 15;
    }
    /* the level: instrument x channel x sequence x global x mix, Q15 */
    uint32_t lv = (uint32_t)in->level * c->vol;
    if (seq_val(c, in, 0, &v)) lv = (lv * (uint16_t)v) >> 8;
    lv = (lv * gvol) >> 8;
    lv = (lv * m_mix) >> 8;
    lv >>= 1;
    if (lv > 32767) lv = 32767;
    if (c->mute || !c->on) lv = 0;
    uint32_t wd = c->width >> 4;
    if (seq_val(c, in, 3, &v)) wd = (uint32_t)v << 4;
    uint8_t n = in->unison;
    for (uint8_t k = 0; k < n; k++) {
        int16_t sp = n > 1 ? (int16_t)(2 * k - (n - 1)) : 0;    /* -(n-1) .. n-1 */
        int32_t uk = u + (n > 1 ? (int32_t)in->detune * sp / (2 * (n - 1)) : 0);
        int32_t pk = (int32_t)c->pan + (n > 1 ? (int32_t)in->spread * sp / (2 * (n - 1)) : 0);
        pk = clampi(pk, 0, 255);
        uint32_t lf = pk < 128 ? 255 : (uint32_t)(255 - pk) * 2, rf = pk > 128 ? 255 : (uint32_t)pk * 2;
        if (lf > 255) lf = 255;
        if (rf > 255) rf = 255;
        uint32_t L = (lv * lf) / 255, R = (lv * rf) / 255;
        uint8_t node = (uint8_t)(c->base + (in->src == 5 ? 2 * k : k));
        if (in->src == 4) {
            int16_t o;
            uint32_t r = oct_ratio(uk - in->sbase, &o);
            uint32_t s = ((in->sstep >> 2) * (r >> 8)) >> 6;    /* sstep x r, >> 16 */
            s = o >= 0 ? s << (o > 4 ? 4 : o) : s >> -o;
            uint32_t lim = in->b16 ? 0x2E000UL : 0x3E000UL;     /* the engine reads 96 or 128 frames a block */
            W(node, 2, s > lim ? lim : s);
        } else {
            W(node, 2, osc_step(uk));
        }
        W(node, 4, R << 16 | L);
        if (in->src == 2) W(node, 5, wd > 4095 ? 4095 : wd);
        if (c->dirty && in->src <= 3) {
            W(node, 6, lfo_word(c->vib_lfo256)); W(node, 7, c->pwm); W(node, 8, vib_q15(c->vibc));
        }
        if (in->src == 5) {             /* the pair's second oscillator, in the group's second node */
            W((uint8_t)(node + 1), 2, osc_step(uk + c->poff));
            uint32_t pl = (L * in->partner) / 255, pr = (R * in->partner) / 255;
            W((uint8_t)(node + 1), 4, pr << 16 | pl);
        }
        uint32_t w3 = c->trig | (c->gate ? 0x100u : 0) | (c->route ? 0x1000u : 0);
        W(node, 3, w3);
    }
    c->dirty = 0;
}

static void note_on(ch_t *c, uint8_t note)
{
    const ins_t *in = &ins[c->inst];
    if (c->cfg != c->inst) configure(c, c->inst);
    c->note = note;
    c->pitch = (int16_t)(note * 64 + in->tune);
    c->slide = 0; c->spitch = 0;
    c->gate = 1; c->on = 1; c->released = 0;
    memset(c->spos, 0, sizeof c->spos);
    c->trig = (uint8_t)(c->trig + 1);
    if (in->src == 4) {                 /* the sample offset: start the sample later */
        uint32_t off = c->soff < in->sframes ? c->soff & ~3UL : 0;
        uint8_t fpw = in->b16 ? 1 : 2;
        for (uint8_t k = 0; k < in->unison; k++) {
            W((uint8_t)(c->base + k), 5, in->saddr + (off >> fpw));
            W((uint8_t)(c->base + k), 6, (in->sframes - off) << 16);
        }
    }
    if (in->src <= 2 && !(in->flags & 8))
        for (uint8_t k = 0; k < in->unison; k++) W((uint8_t)(c->base + k), 15, (uint32_t)(k * 16384u) << 16);
    c->vph = 0;
    c->soff = 0;
}

static void release(ch_t *c) { c->gate = 0; c->released = 1; }

static void cut(ch_t *c)
{
    c->gate = 0; c->on = 0; c->released = 1;
    if (c->cfg == 0xFF) return;
    for (uint8_t k = 0; k < c->nres; k++) W((uint8_t)(c->base + k), 14, c->trig | 0x200u);   /* the level to 0 */
}

static void set_inst(ch_t *c, uint8_t i)
{
    if (i >= m_ninst || ins[i].nodes > c->nres) return;
    c->inst = i;
    c->vol = 255;
    c->pan = ins[i].pan;
    c->route = ins[i].flags & 1;
    c->width = (uint16_t)(ins[i].width << 4);
    c->vib_lfo256 = ins[i].lfo256; c->vibc = ins[i].vibc; c->pwm = ins[i].pwm;
    c->poff = ins[i].poff; c->fine = 0;
    c->dirty = 1;
}

/* ---- filters */
static void flt_write(uint8_t f)
{
    flt_t *t = &flt[f];
    uint16_t F = flt_F(hz_of(t->cut));
    uint32_t damp = t->q256 ? 8388608UL / t->q256 : 32767;
    if (damp > 32767) damp = 32767;
    if (damp < 256) damp = 256;
    uint32_t depth = ((uint32_t)F * ((ratio16(cents_u((int16_t)t->sweepc)) - 0x10000UL) >> 4)) >> 12;
    uint32_t lv = (uint32_t)t->level << 7;
    uint32_t lf = t->pan < 128 ? 255 : (uint32_t)(255 - t->pan) * 2, rf = t->pan > 128 ? 255 : (uint32_t)t->pan * 2;
    if (lf > 255) lf = 255;
    if (rf > 255) rf = 255;
    W(t->node, 2, F); W(t->node, 3, damp);
    W(t->node, 4, ((lv * rf) / 255) << 16 | (lv * lf) / 255);
    W(t->node, 5, lfo_word(t->lfo256)); W(t->node, 6, depth > 0x7000 ? 0x7000 : depth); W(t->node, 7, t->mode);
    t->dirty = 0;
}

/* ---- the song */
static void pattern_at(uint8_t o, uint8_t r)
{
    if (o >= m_nord) o = m_restart < m_nord ? m_restart : 0;
    ord = o;
    uint8_t pt = m_ordr[o];
    const uint8_t *s = m_patt + rd32(m_patt + 4 * (uint32_t)pt);
    uint16_t nr = rd16(s);
    rows = (uint8_t)(nr - 1);           /* (up to 256: the last row's index) */
    pp = s + 2;
    skip_rows = 0;
    row = 0;
    while (row < r && row < rows) {     /* to row r: read past the rows before it */
        if (skip_rows) { skip_rows--; row++; continue; }
        for (;;) {
            uint8_t b = *pp++;
            if (b == 0) break;
            if (b >= 0x80) { skip_rows = (uint8_t)(b - 0x80); break; }
            uint8_t fl = *pp++;
            pp += (fl & 1) + (fl >> 1 & 1) + (fl >> 2 & 1) + 2 * (fl >> 3 & 1) + 2 * (fl >> 4 & 1);
        }
        row++;
    }
    t1m.order = ord; t1m.pattern = pt; t1m.rows = rows;
}

static void apply_cell(ch_t *c, uint8_t fl, uint8_t note, uint8_t inst, uint8_t vol)
{
    uint8_t porta = (c->fx[0][0] == 3 || c->fx[1][0] == 3);
    if (fl & 2) set_inst(c, inst);
    if (fl & 4) c->vol = vol;
    if (fl & 1) {
        if (note == 0x80) release(c);
        else if (note == 0x81) cut(c);
        else if (c->inst != 0xFF) {
            if (porta && c->on) {
                c->porta_to = (int16_t)(note * 64 + ins[c->inst].tune - c->pitch);
            } else {
                note_on(c, note);
            }
        }
    }
}

static void row_effects(ch_t *c, uint8_t ci)
{
    c->arp = 0; c->vslide = 0; c->wslide = 0; c->pslide = 0;
    c->cut_at = c->off_at = c->delay_at = c->retrig = 0xFF;
    for (uint8_t k = 0; k < 2; k++) {
        uint8_t cmd = c->fx[k][0], x = c->fx[k][1];
        if (cmd == 0xFF) continue;
        switch (cmd) {
        case 0x00: c->arp = x; break;
        case 0x03: if (x) c->porta = x; break;
        case 0x04:
            if (x >> 4) c->vib_lfo256 = (uint16_t)((x >> 4) * 128);
            if (x & 15) c->vibc = (uint16_t)((x & 15) * 8);
            c->dirty = 1;
            break;
        case 0x05: c->width = (uint16_t)x << 8; break;
        case 0x06:
            if (x >> 4) c->vib_lfo256 = (uint16_t)((x >> 4) * 128);
            if (x & 15) c->pwm = (uint16_t)((x & 15) * 16);
            c->dirty = 1;
            break;
        case 0x07: c->wslide = x; break;
        case 0x08: c->pan = x; break;
        case 0x09: c->soff = (uint32_t)x << 8; break;
        case 0x0A: c->vslide = x; break;
        case 0x0B: jump_to = x; flow |= 1; break;
        case 0x0C: c->vol = x; break;
        case 0x0D: brk_row = x; flow |= 2; break;
        case 0x0E: c->fine = cents_u((int8_t)x); break;
        case 0x0F: if (x) speed = x; break;
        case 0x10: if (x) { tick_hz256 = (uint16_t)x << 8; period = ((0x5DC0000UL) / tick_hz256) << 8; } break;   /* 1500 << 16 */
        case 0x11: gvol = x; break;
        case 0x12: c->cut_at = x; break;
        case 0x13: c->delay_at = x; break;
        case 0x14: c->retrig = x; break;
        case 0x15: c->off_at = x; break;
        case 0x16: c->route = x ? 1 : 0; break;
        case 0x17: c->poff = (int16_t)((int8_t)x * 16); break;
        case 0x18: c->pslide = (int8_t)x; break;
        default:
            if (cmd >= 0x20 && cmd < 0x3C) {
                uint8_t f = cmd & 3, op = (uint8_t)((cmd - 0x20) >> 2);
                if (f >= m_nflt) break;
                flt_t *t = &flt[f];
                switch (op) {
                case 0: t->cut = (int32_t)x * 32; break;
                case 2: { uint32_t q = (uint32_t)(ratio16((int32_t)x * 24) >> 8) * 128 >> 8; t->q256 = (uint16_t)(q > 65535 ? 65535 : q); } break;   /* 0.5 x 2^(x/32) */
                case 3: t->sweepc = (uint16_t)x * 8; break;
                case 4: t->lfo256 = (uint16_t)x * 16; break;
                case 5: t->level = x; break;
                case 6: t->mode = x ? 1 : 0; break;
                }
                t->dirty = 1;
            }
        }
    }
    (void)ci;
}

static void tick_effects(ch_t *c)       /* the per-tick effects (ticks after the first) */
{
    for (uint8_t k = 0; k < 2; k++) {
        uint8_t cmd = c->fx[k][0], x = c->fx[k][1];
        if (cmd == 0x01) c->slide = (int16_t)(c->slide + x * 4);
        else if (cmd == 0x02) c->slide = (int16_t)(c->slide - x * 4);
        else if (cmd == 0x03 && c->porta) {
            int16_t d = (int16_t)(c->porta_to - c->slide), s = (int16_t)(c->porta * 4);
            c->slide = (int16_t)(c->slide + (d > s ? s : d < -s ? -s : d));
        } else if (cmd >= 0x24 && cmd < 0x28 && (cmd & 3) < m_nflt) {
            flt_t *t = &flt[cmd & 3];
            t->cut += (int32_t)(int8_t)x * 4;
            if (t->cut < 0) t->cut = 0;
            if (t->cut > 216L * 32) t->cut = 216L * 32;
            t->dirty = 1;
        }
    }
    if (c->vslide) c->vol = (uint8_t)clampi((int32_t)c->vol + (c->vslide >> 4) * 4 - (c->vslide & 15) * 4, 0, 255);
    if (c->wslide) c->width = (uint16_t)clampi(((int32_t)c->width >> 6) + (c->wslide >> 4) - (c->wslide & 15), 0, 1023) << 6;
    if (c->pslide) c->poff = (int16_t)(c->poff + c->pslide * 4);
    if (tick == c->cut_at) cut(c);
    if (tick == c->off_at) release(c);
    if (c->retrig != 0xFF && c->retrig && c->on && tick % c->retrig == 0) note_on(c, c->note);
}

static void read_row(void)
{
    for (uint8_t i = 0; i < m_nch; i++) { ch[i].fx[0][0] = ch[i].fx[1][0] = 0xFF; ch[i].dflags = 0; }
    static uint8_t fl_[T1M_MAXCH], n_[T1M_MAXCH], i_[T1M_MAXCH], v_[T1M_MAXCH];
    memset(fl_, 0, sizeof fl_);
    if (skip_rows) skip_rows--;
    else
        for (;;) {
            uint8_t b = *pp++;
            if (b == 0) break;
            if (b >= 0x80) { skip_rows = (uint8_t)(b - 0x80); break; }
            uint8_t c = (uint8_t)(b - 1), fl = *pp++, note = 0, in = 0, vol = 0, fx[2][2] = { { 0xFF, 0 }, { 0xFF, 0 } };
            if (fl & 1) note = *pp++;
            if (fl & 2) in = *pp++;
            if (fl & 4) vol = *pp++;
            for (uint8_t k = 0; k < 2; k++)
                if (fl & (8 << k)) { fx[k][0] = pp[0]; fx[k][1] = pp[1]; pp += 2; }
            if (fx[0][0] == 0xFF && fx[1][0] != 0xFF) { fx[0][0] = fx[1][0]; fx[0][1] = fx[1][1]; fx[1][0] = 0xFF; }
            if (c >= m_nch) continue;
            memcpy(ch[c].fx, fx, sizeof fx);
            fl_[c] = fl; n_[c] = note; i_[c] = in; v_[c] = vol;
        }
    for (uint8_t i = 0; i < m_nch; i++) {
        ch_t *c = &ch[i];
        row_effects(c, i);
        if (c->delay_at != 0xFF && c->delay_at) {   /* a delayed cell: later this row */
            c->dflags = fl_[i]; c->dnote = n_[i]; c->dinst = i_[i]; c->dvol = v_[i];
        } else if (fl_[i])
            apply_cell(c, fl_[i], n_[i], i_[i], v_[i]);
    }
}

static void next_row(void)
{
    if (flow) {
        uint8_t o = flow & 1 ? jump_to : (uint8_t)(ord + 1);
        pattern_at(o, flow & 2 ? brk_row : 0);
        flow = 0;
        return;
    }
    if (row >= rows) { pattern_at((uint8_t)(ord + 1), 0); return; }
    row++;
}

static void do_tick(void)
{
    if (tick == 0) read_row();
    else
        for (uint8_t i = 0; i < m_nch; i++) {
            ch_t *c = &ch[i];
            tick_effects(c);
            if (c->dflags && tick == c->delay_at) { apply_cell(c, c->dflags, c->dnote, c->dinst, c->dvol); c->dflags = 0; }
        }
    for (uint8_t i = 0; i < m_nch; i++) {
        ch_t *c = &ch[i];
        if (c->cfg == 0xFF) continue;
        update(i);
        if (c->vibc && ins[c->cfg].src >= 4)
            c->vph = (uint16_t)(c->vph + (uint16_t)(((uint32_t)c->vib_lfo256 << 8) / (tick_hz256 >> 8 ? tick_hz256 >> 8 : 1)));
        seq_advance(c, &ins[c->cfg]);
        t1m.ch[i].note = c->on ? c->note : 0xFF;
        t1m.ch[i].inst = c->inst;
        t1m.ch[i].vol = c->vol;
        t1m.ch[i].gate = c->gate;
        c->mute = t1m.ch[i].mute;
    }
    for (uint8_t f = 0; f < m_nflt; f++)
        if (flt[f].dirty) flt_write(f);
    t1m.row = row; t1m.speed = speed; t1m.gvol = gvol; t1m.tick_hz256 = tick_hz256;
    t1m.ticks++;
    if (++tick >= speed) { tick = 0; next_row(); }
}

uint16_t t1m_decode(uint8_t pat, uint8_t *note, uint8_t *inst, uint8_t nch, uint16_t max_rows)
{
    if (pat >= m_npat) return 0;
    const uint8_t *s = m_patt + rd32(m_patt + 4 * (uint32_t)pat);
    uint16_t nr = rd16(s), r = 0;
    if (nr > max_rows) nr = max_rows;
    memset(note, 0xFF, (uint32_t)nr * nch);
    memset(inst, 0xFF, (uint32_t)nr * nch);
    s += 2;
    while (r < nr) {
        uint8_t b = *s++;
        if (b == 0) { r++; continue; }
        if (b >= 0x80) { r = (uint16_t)(r + b - 0x7F); continue; }
        uint8_t c = (uint8_t)(b - 1), fl = *s++;
        uint32_t at = (uint32_t)r * nch + c;
        if (fl & 1) { if (c < nch) note[at] = *s; s++; }
        if (fl & 2) { if (c < nch) inst[at] = *s; s++; }
        s += (fl >> 2 & 1) + 2 * (fl >> 3 & 1) + 2 * (fl >> 4 & 1);
    }
    return nr;
}

void t1m_goto(uint8_t o) { jump_to = o; flow = 1; }
void t1m_pause(uint8_t on)
{
    paused = on;
    for (uint8_t i = 0; i < m_nch; i++)
        if (on) cut(&ch[i]);
}

void t1m_start(void)
{
    mc_init();
    MC_CTRL = 0;
    static uint8_t order[MC_NNODES];
    for (uint8_t i = 0; i < nnodes; i++) order[i] = i;
    for (uint8_t i = 0; i < nnodes; i++)
        next_w[i] = i + 1 < nnodes ? MC_WADDR(MC_NODE(i + 1)) : 0;
    for (uint8_t i = 0; i < nnodes; i++) {
        link(i, 0);
        W(i, 1, (uint32_t)MC_SILENT | 0x80000000UL);
    }
    for (uint8_t c = 0; c < m_nch; c++) {
        memset(&ch[c].inst, 0, sizeof(ch_t) - 2);   /* (all but base and nres) */
        ch[c].inst = ch[c].cfg = 0xFF;
        ch[c].note = 0xFF;
        t1m.ch[c].node = ch[c].base; t1m.ch[c].nodes = ch[c].nres;
        t1m.ch[c].note = t1m.ch[c].inst = 0xFF;
    }
    for (uint8_t f = 0; f < m_nflt; f++) {
        W(flt[f].node, 1, MC_FLT);
        W(flt[f].node, 12, 0); W(flt[f].node, 13, 0); W(flt[f].node, 14, 0); W(flt[f].node, 15, (uint32_t)f << 30);
        flt_write(f);
    }
    uint32_t h = nnodes ? MC_WADDR(MC_NODE(0)) : 0;
    MC_HEAD_LO = (uint16_t)h;
    MC_HEAD_HI = (uint16_t)(h >> 16);
    speed = m_speed0 ? m_speed0 : 6;
    gvol = m_gvol0;
    tick_hz256 = m_tick0 ? m_tick0 : 50 * 256;
    period = ((0x5DC0000UL) / tick_hz256) << 8;  /* (1500 << 16) / Hz, << 8: blocks a tick, 16.16 */
    acc = period;                       /* the first tick at the first block */
    tick = 0; flow = 0; paused = 0;
    t1m.ticks = 0;
    pattern_at(0, 0);
    last_blocks = MC_BLOCKS;
    MC_IRQ = 0xFF;
    MC_CTRL = 0x0001;                   /* run, output shift 0 */
}

void t1m_block(void)
{
    uint16_t now = MC_BLOCKS;
    uint16_t d = (uint16_t)(now - last_blocks);
    last_blocks = now;
    if (paused) return;
    acc += (uint32_t)d << 16;
    if (acc >= period) {
        acc -= period;
        if (acc >= period) acc = 0;     /* far behind: do not race to catch up */
        do_tick();
    }
}
