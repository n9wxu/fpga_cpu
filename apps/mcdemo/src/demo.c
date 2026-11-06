/* mcdemo: the microcoded audio engine (docs/mcaudio.md) feature by feature, with a live dataflow
 * diagram of how the sound gets from the SDRAM to the speaker (view.c). Needs the -DMCAUDIO
 * bitstream (cd tang && make mcaudio prog-mc). Seven scenes of 8 bars at 120 BPM, then again.
 *
 * The music runs in the engine's block-done interrupt (1500 a second): node words are written only
 * there, while the engine waits for room in its FIFO, so it never fetches a node half written. The
 * main loop reads the engine's monitor and draws. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "irq.h"
#include "mc.h"
#include "view.h"
#include "samples.h"

#define TICK_BLOCKS 188                 /* a 16th note at 120 BPM: 1500 blocks a second / 8 */
#define STEPS       128                 /* a scene: 8 bars of 16ths */

/* ---- voices ----------------------------------------------------------------------------------- */
typedef struct {
    uint16_t entry;
    uint16_t vl, vr;                    /* Q15 */
    uint16_t a, d, r, s;                /* attack (Q15 a block), decay and release rates, sustain */
    uint16_t width, lfo, pwm, vib;      /* pulse width, LFO rate, PWM depth, vibrato depth */
    uint8_t  route;                     /* 1: to the filter bus */
} patch_t;

static uint32_t w3[MC_NNODES];          /* each node's w3: trigger count, gate, route */
static uint8_t  kind[MC_NNODES];        /* for the screen: VK_* by node */

static void W(uint8_t n, uint8_t w, uint32_t v) { MC_NODE(n)[w] = v; }

static void env(uint8_t n, const patch_t *p)
{
    W(n, 4, (uint32_t)p->vr << 16 | p->vl);
    W(n, 9, (uint32_t)p->a << 16);
    W(n, 10, (uint32_t)p->r << 16 | p->d);
    W(n, 11, (uint32_t)p->s << 16);
    w3[n] = (uint32_t)p->route << 12;
    W(n, 3, w3[n]);
    W(n, 12, 0); W(n, 13, 0); W(n, 14, 0); W(n, 15, 0);
}

static void osc(uint8_t n, const patch_t *p)
{
    W(n, 1, p->entry);
    env(n, p);
    W(n, 5, p->width); W(n, 6, (uint32_t)p->lfo << 16); W(n, 7, p->pwm); W(n, 8, p->vib);
    if (p->entry == MC_OSC_NOISE) W(n, 15, 1);          /* the LFSR's seed */
    kind[n] = p->entry == MC_OSC_SAW ? VK_SAW : p->entry == MC_OSC_TRI ? VK_TRI :
              p->entry == MC_OSC_PULSE ? VK_PULSE : VK_NOISE;
}

typedef struct { const int8_t *d; uint16_t len, loop; uint8_t looped; } sample_t;
static const sample_t S_KICK  = { smp_kick, SMP_KICK_LEN, 0, 0 };
static const sample_t S_SNARE = { smp_snare, SMP_SNARE_LEN, 0, 0 };
static const sample_t S_HAT   = { smp_hat, SMP_HAT_LEN, 0, 0 };
static const sample_t S_PLUCK = { smp_pluck, SMP_PLUCK_LEN, SMP_PLUCK_LOOP, 1 };

static void smp(uint8_t n, const sample_t *s, const patch_t *p)
{
    uint32_t base = MC_WADDR(s->d);
    W(n, 1, MC_SMP);
    env(n, p);
    W(n, 5, base);
    W(n, 6, (uint32_t)s->len << 16);
    W(n, 7, s->looped ? (uint32_t)(s->len - s->loop) << 16 : 0);
    W(n, 8, base + s->loop / 4);
    kind[n] = VK_SAMPLE;
}

/* a lockstep group: master m (envelope, step), slave m + 1 */
static void sync_grp(uint8_t m, uint16_t mode, const patch_t *p, uint16_t svol)
{
    W(m, 1, MC_SYNC);
    env(m, p);
    W(m, 4, 0);                                         /* the master is heard only through the slave */
    W(m, 5, mode);
    W(m + 1, 1, 0); W(m + 1, 4, (uint32_t)svol << 16 | svol);
    W(m + 1, 12, 0); W(m + 1, 13, 0);
    kind[m] = mode & 2 ? VK_RING : VK_SYNC; kind[m + 1] = kind[m];
}

static void flt(uint8_t n, uint16_t f, uint16_t damp, uint16_t vol, uint16_t lfo, uint16_t depth, uint16_t mode)
{
    W(n, 1, MC_FLT);
    W(n, 2, f); W(n, 3, damp); W(n, 4, (uint32_t)vol << 16 | vol);
    W(n, 5, (uint32_t)lfo << 16); W(n, 6, depth); W(n, 7, mode);
    W(n, 12, 0); W(n, 13, 0); W(n, 14, 0); W(n, 15, 0);
    kind[n] = VK_FILTER;
}

static void note(uint8_t n, uint32_t step)
{
    W(n, 2, step);
    w3[n] = (w3[n] & 0x1000UL) | ((w3[n] + 1) & 0xFF) | 0x100;
    W(n, 3, w3[n]);
}
static void off(uint8_t n) { w3[n] &= ~0x100UL; W(n, 3, w3[n]); }
static uint32_t sstep(int16_t midi, int16_t base) { return mc_ratio((int16_t)(midi - base)); }

/* the list and its picture */
static uint8_t order[MC_NNODES], linked[MC_NNODES], nlist;
static void list(const uint8_t *o, const uint8_t *lk, uint8_t n)
{
    for (uint8_t i = 0; i < n; i++) { order[i] = o[i]; linked[i] = lk ? lk[i] : 0; }
    nlist = n;
    mc_list(order, linked, n);
}

/* ---- the music: A minor, Am F C G -------------------------------------------------------------- */
static const uint8_t root[4] = { 45, 41, 48, 43 };
static const uint8_t chord[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 60, 64, 67 }, { 55, 59, 62 } };
/* 0 hold, 1 off, else a MIDI note */
static const uint8_t melody[64] = {
    69, 0, 0, 72,  0, 76, 0, 0,  74, 0, 72, 0,  71, 0, 1, 0,
    65, 0, 0, 69,  0, 72, 0, 0,  77, 0, 76, 0,  72, 0, 1, 0,
    72, 0, 0, 76,  0, 79, 0, 0,  76, 0, 74, 0,  72, 0, 1, 0,
    71, 0, 74, 0,  79, 0, 78, 0, 79, 0, 76, 0,  74, 0, 1, 0 };
static const uint16_t kick_pat = 0x0505, snare_pat = 0x1010, hat_pat = 0x5555;   /* bit k: step k */
static uint8_t bar(uint16_t t) { return (uint8_t)((t >> 4) & 3); }
static uint8_t at(uint16_t pat, uint16_t t) { return (pat >> (t & 15)) & 1; }

/* patches */
static const patch_t P_LEAD  = { MC_OSC_PULSE, 0x5000, 0x5000, 0x2000, 0x0300, 0x0600, 0x5000, 2048, 0, 0, 0, 0 };
static const patch_t P_BASS  = { MC_OSC_TRI,   0x3000, 0x3000, 0x4000, 0x0100, 0x0800, 0x6000, 0, 0, 0, 0, 0 };
static const patch_t P_ARP   = { MC_OSC_SAW,   0x0C00, 0x0600, 0x7FFF, 0x0800, 0x0400, 0x2000, 0, 0, 0, 0, 0 };
static const patch_t P_STAB  = { MC_OSC_PULSE, 0x0600, 0x0C00, 0x7FFF, 0x0600, 0x0400, 0x3000, 1200, 0, 0, 0, 0 };
static const patch_t P_HAT   = { MC_OSC_NOISE, 0x0A00, 0x0A00, 0x7FFF, 0x2000, 0x2000, 0x0000, 0, 0, 0, 0, 0 };
static const patch_t P_PWM   = { MC_OSC_PULSE, 0x0E00, 0x0E00, 0x0080, 0x0040, 0x0080, 0x6000, 2048, 60, 1800, 0, 0 };
static const patch_t P_VIB   = { MC_OSC_TRI,   0x2400, 0x2400, 0x1000, 0x0200, 0x0300, 0x6000, 0, 240, 0, 520, 0 };
static const patch_t P_PAD   = { MC_OSC_SAW,   0x0900, 0x0900, 0x0100, 0x0080, 0x0100, 0x6000, 0, 0, 0, 0, 1 };
static const patch_t P_DRUM  = { MC_SMP,       0x5000, 0x5000, 0x7FFF, 0x0000, 0x0080, 0x7FFF, 0, 0, 0, 0, 0 };
static const patch_t P_PLUCK = { MC_SMP,       0x4800, 0x4800, 0x7FFF, 0x0040, 0x0400, 0x6000, 0, 0, 0, 0, 0 };
static const patch_t P_SYNC  = { MC_SYNC,      0, 0, 0x4000, 0x0080, 0x0500, 0x6000, 0, 0, 0, 0, 0 };
static const patch_t P_BELL  = { MC_SYNC,      0, 0, 0x7FFF, 0x0180, 0x0300, 0x1000, 0, 0, 0, 0, 0 };

/* ---- scenes ----------------------------------------------------------------------------------- */
typedef struct {
    const char *title, *notes;
    uint16_t flags;
    void (*start)(void);
    void (*tick)(uint16_t t);
    void (*block)(uint16_t b);           /* every block (b: blocks into the scene), or 0 */
} scene_t;

static void lead_tick(uint8_t n, uint16_t t, int8_t transpose)
{
    uint8_t m = melody[t & 63];
    if (m == 1) off(n);
    else if (m) note(n, mc_step((int16_t)(m + transpose)));
}

/* 1 */
static void s1_start(void) { static const uint8_t o[] = { 0 }; osc(0, &P_LEAD); list(o, 0, 1); }
static void s1_tick(uint16_t t) { lead_tick(0, t, 0); }

/* 2 */
static void s2_start(void)
{
    static const uint8_t o[] = { 0, 1, 2, 3 };
    osc(0, &P_BASS); osc(1, &P_ARP); osc(2, &P_STAB); osc(3, &P_HAT);
    list(o, 0, 4);
}
static void s2_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (k == 0 || k == 8) note(0, mc_step(root[b]));
    if (k == 6 || k == 14) off(0);
    if (t >= 16) note(1, mc_step((int16_t)(chord[b][k % 3] + 12 * ((k / 3) & 1))));
    if (t >= 32 && (k == 2 || k == 10)) note(2, mc_step(chord[b][1]));
    if (t >= 32 && (k == 4 || k == 12)) off(2);
    if (t >= 48 && at(hat_pat, t)) note(3, 0x60000000UL);
}

/* 3 */
static void s3_start(void)
{
    static const uint8_t o[] = { 0, 1, 2, 3 };
    osc(0, &P_PWM); osc(1, &P_PWM); osc(2, &P_PWM); osc(3, &P_VIB);
    W(1, 6, (uint32_t)47 << 16); W(2, 6, (uint32_t)73 << 16);          /* LFOs a little apart */
    list(o, 0, 4);
}
static void s3_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (k == 0) for (uint8_t i = 0; i < 3; i++) note(i, mc_step((int16_t)(chord[b][i] - 12)));
    if (t >= 32) lead_tick(3, t, 0);
}

/* 4 */
static void s4_start(void)
{
    static const uint8_t o[] = { 0, 1, 2, 3, 4 };
    smp(0, &S_KICK, &P_DRUM); smp(1, &S_SNARE, &P_DRUM); smp(2, &S_HAT, &P_DRUM);
    smp(3, &S_PLUCK, &P_PLUCK); smp(4, &S_PLUCK, &P_PLUCK);
    W(2, 4, 0x18001800UL);
    W(4, 4, 0x10002400UL);
    list(o, 0, 5);
}
static void s4_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (at(kick_pat, t)) note(0, 0x10000);
    if (at(snare_pat, t)) note(1, 0x10000);
    if (at(hat_pat, t) || (t >= 64 && (k & 1))) note(2, 0x10000);
    if ((k & 1) == 0) note(3, sstep((int16_t)(root[b] + ((k & 4) ? 12 : 0)), SMP_PLUCK_MIDI));
    if (t >= 32) { uint8_t m = melody[t & 63]; if (m > 1) note(4, sstep(m, SMP_PLUCK_MIDI)); }
}

/* 5 */
static uint8_t s5_root;
static void s5_start(void)
{
    static const uint8_t o[] = { 0, 1, 2, 3, 4 }, lk[] = { 1, 0, 1, 0, 0 };
    sync_grp(0, 1, &P_SYNC, 0x2400);
    sync_grp(2, 2, &P_BELL, 0x2400);
    osc(4, &P_HAT);
    list(o, lk, 5);
}
static void s5_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (k == 0) { s5_root = root[b]; note(0, mc_step(s5_root)); }
    if (k == 12) off(0);
    if (t >= 32) {
        uint8_t m = melody[t & 63];
        if (m == 1) off(2);
        else if (m) { uint32_t s = mc_step((int16_t)(m + 12)); note(2, s); W(3, 2, (s >> 8) * 361UL); }   /* ratio 1.41 */
    }
    if (t >= 64 && at(hat_pat, t)) note(4, 0x60000000UL);
}
static void s5_block(uint16_t b)
{
    /* the slave's step: from 1x to 4.5x the master's and back, every 2 seconds */
    uint16_t ph = (uint16_t)(b % 3000);
    uint16_t x = ph < 1500 ? ph : (uint16_t)(3000 - ph);
    uint32_t r = 256 + (uint32_t)x * 896 / 1500;        /* 8.8: 1.0 - 4.5 */
    if ((b & 3) == 0) W(1, 2, (mc_step(s5_root) >> 8) * r);
}

/* 6 */
static void s6_start(void)
{
    static const uint8_t o[] = { 0, 1, 2, 3, 4, 5, 6 };
    for (uint8_t i = 0; i < 5; i++) osc(i, &P_PAD);
    osc(5, &P_BASS);
    flt(6, 0x0900, 0x1400, 0x7000, 22, 0x0700, 0);
    list(o, 0, 7);
}
static void s6_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (k == 0) {
        for (uint8_t i = 0; i < 3; i++) note(i, mc_step(chord[b][i]));
        note(3, mc_step((int16_t)(chord[b][0] + 12)) + 0x8000UL);
        note(4, mc_step((int16_t)(chord[b][2] - 12)) - 0x8000UL);
    }
    if (k == 0 || k == 6 || k == 10) note(5, mc_step(root[b]));
    if (k == 4 || k == 8 || k == 14) off(5);
}

/* 7: everything */
enum { E_KICK, E_SNARE, E_HAT, E_BASS, E_LEAD, E_ARP, E_SYM, E_SYS, E_PAD0 = 8, E_NPAD = 12, E_FLT = E_PAD0 + E_NPAD };
static void s7_start(void)
{
    static uint8_t o[24], lk[24];
    uint8_t n = 0;
    smp(E_KICK, &S_KICK, &P_DRUM); smp(E_SNARE, &S_SNARE, &P_DRUM); smp(E_HAT, &S_HAT, &P_DRUM);
    W(E_HAT, 4, 0x18001800UL);
    smp(E_BASS, &S_PLUCK, &P_PLUCK);
    osc(E_LEAD, &P_VIB); W(E_LEAD, 4, 0x18001C00UL);
    osc(E_ARP, &P_ARP);
    sync_grp(E_SYM, 1, &P_SYNC, 0x1400);
    for (uint8_t i = 0; i < E_NPAD; i++) { osc((uint8_t)(E_PAD0 + i), &P_PAD); W((uint8_t)(E_PAD0 + i), 4, 0x05000500UL); }
    flt(E_FLT, 0x0900, 0x1200, 0x7000, 22, 0x0800, 0);
    o[n] = E_KICK; lk[n++] = 0; o[n] = E_SNARE; lk[n++] = 0; o[n] = E_HAT; lk[n++] = 0; o[n] = E_BASS; lk[n++] = 0;
    o[n] = E_LEAD; lk[n++] = 0; o[n] = E_ARP; lk[n++] = 0; o[n] = E_SYM; lk[n++] = 1; o[n] = E_SYS; lk[n++] = 0;
    for (uint8_t i = 0; i < E_NPAD; i++) { o[n] = (uint8_t)(E_PAD0 + i); lk[n++] = 0; }
    o[n] = E_FLT; lk[n++] = 0;
    list(o, lk, n);
}
static void s7_tick(uint16_t t)
{
    uint8_t b = bar(t), k = t & 15;
    if (at(kick_pat, t)) note(E_KICK, 0x10000);
    if (at(snare_pat, t)) note(E_SNARE, 0x10000);
    if (k & 1 ? t >= 64 : 1) note(E_HAT, 0x10000);
    if ((k & 1) == 0) note(E_BASS, sstep((int16_t)(root[b] + ((k & 4) ? 12 : 0)), SMP_PLUCK_MIDI));
    if (k == 0)
        for (uint8_t i = 0; i < E_NPAD; i++) {
            uint32_t s = mc_step((int16_t)(chord[b][i % 3] + 12 * (i / 6) - 12));
            note((uint8_t)(E_PAD0 + i), (i & 1) ? s + (s >> 9) : s - (s >> 9));
        }
    lead_tick(E_LEAD, t, 12);
    if (t >= 32) note(E_ARP, mc_step((int16_t)(chord[b][k % 3] + 12 * (1 + ((k / 3) & 1)))));
    if (k == 2 || k == 7 || k == 11) { s5_root = (uint8_t)(root[b] + 12); note(E_SYM, mc_step(s5_root)); }
    if (k == 4 || k == 9 || k == 13) off(E_SYM);
}
static void s7_block(uint16_t b)
{
    uint16_t ph = (uint16_t)(b % 1500);
    uint16_t x = ph < 750 ? ph : (uint16_t)(1500 - ph);
    if ((b & 3) == 0) W(E_SYS, 2, (mc_step(s5_root) >> 8) * (256 + (uint32_t)x * 768 / 750));
}

static const scene_t scenes[] = {
    { "1. One node, one routine",
      "The list holds one node. Each 32-sample block the walker fetches its 16 words in one\n"
      "burst; its routine runs the envelope once, then the 32-pass sample loop (9 micro-words a\n"
      "sample), then writes the phase and envelope back to SDRAM.", 0, s1_start, s1_tick, 0 },
    { "2. The wave unit: triangle, saw, pulse, noise",
      "Four nodes, four routines from the same control store. One wave-unit operation makes each\n"
      "waveform from the phase; the noise routine steps an LFSR whenever its phase wraps.\n"
      "Bass (triangle), arpeggio (saw), stabs (pulse), then the hi-hat (noise).", 0, s2_start, s2_tick, 0 },
    { "3. Scripts as microcode: PWM and vibrato",
      "Each routine's per-block part reads a sine LFO from the table ROM: it sweeps the three\n"
      "pulse widths (PWM: the chorusing pad) and bends the lead's step (vibrato). No CPU work:\n"
      "there is no script processor any more, the same datapath runs it.", 0, s3_start, s3_tick, 0 },
    { "4. Samples from SDRAM",
      "Drums and a plucked string are 8-bit samples in SDRAM. Each block a sample node's routine\n"
      "DMAs its next 128 frames (and its loop start) into the sample buffer, then reads them with\n"
      "linear interpolation. Note-on restarts the position in the engine.", VF_SAMPLES, s4_start, s4_tick, 0 },
    { "5. Linked nodes: hard sync and ring modulation",
      "Nodes flagged linked are fetched together and stepped in lockstep. Bass: a saw restarted\n"
      "each time its master wraps while its pitch sweeps (hard sync). Bells: a triangle whose\n"
      "sign flips with its master's top bit (ring modulation).", VF_GROUP, s5_start, s5_tick, s5_block },
    { "6. The filter bus",
      "Five pad voices add into the filter buses instead of the main mix; the filter node, last\n"
      "in the list, runs a resonant state-variable filter over them and adds the result to the\n"
      "mix. Its cutoff sweeps from its own LFO. The bass goes straight to the main mix.",
      VF_FILTER, s6_start, s6_tick, 0 },
    { "7. Everything: 21 nodes",
      "Drums and bass from samples, a 12-voice pad through the filter, a lead with vibrato, an\n"
      "arpeggio and synced stabs. The busy bar shows how much of each block the engine needs:\n"
      "one 18x18 multiplier for all of it.", VF_SAMPLES | VF_FILTER | VF_GROUP, s7_start, s7_tick, s7_block },
};
#define NSCENES (sizeof scenes / sizeof scenes[0])

/* ---- the interrupt: one a block ---------------------------------------------------------------- */
static volatile uint8_t scene = 0xFF;
static volatile uint16_t under0;        /* UNDERRUNS when this cycle of scenes began */
static uint16_t last_blocks, tick_wait, t, sblocks;

static void on_block(void)
{
    MC_IRQ = 0xFF;
    IRQ_PEND = (uint16_t)(1u << IRQ_AUDIO);
    uint16_t now = MC_BLOCKS;
    uint16_t d = (uint16_t)(now - last_blocks);
    last_blocks = now;
    if (scene == 0xFF || d == 0) { if (scene == 0xFF) { tick_wait = 0; t = STEPS; scene = NSCENES - 1; } }
    sblocks = (uint16_t)(sblocks + d);
    if (scenes[scene].block) scenes[scene].block(sblocks);
    if (tick_wait > d) { tick_wait = (uint16_t)(tick_wait - d); return; }
    tick_wait = TICK_BLOCKS;
    if (t >= STEPS) {
        scene = (uint8_t)((scene + 1) % NSCENES);
        t = 0; sblocks = 0;
        if (scene == 0) under0 = MC_UNDER;  /* the screen counts underruns from each cycle's start */
        for (uint8_t i = 0; i < MC_NNODES; i++) kind[i] = 0;
        scenes[scene].start();
    }
    scenes[scene].tick(t);
    t++;
}

/* ---- the monitor, for the screen --------------------------------------------------------------- */
static mon_t m;
static void monitor(void)
{
    m.blocks = MC_BLOCKS; m.underruns = (uint16_t)(MC_UNDER - under0); m.clocks = MC_CLOCKS;
    m.bursts = MC_BURSTS; m.words = MC_WORDS;
    uint8_t sc = scene;
    if (sc >= NSCENES) return;
    m.nnodes = nlist;
    for (uint16_t i = 0; i < nlist; i++) {
        uint8_t n = order[i];
        MC_MON_IDX = (uint16_t)(256 + i);
        uint16_t lo = MC_MON_LO, hi = MC_MON_HI;
        m.kind[i] = kind[n];
        m.group[i] = linked[i];
        m.meter[i] = kind[n] == VK_FILTER ? (uint16_t)(lo & 0x7FFF) : hi;
        if (i > 0 && linked[i - 1]) m.meter[i] = m.meter[i - 1];        /* a group: the master's */
    }
    uint16_t r = MC_MON_IDX;
    int16_t pk = 1;
    for (uint16_t i = 0; i < 256; i++) {
        MC_MON_IDX = (uint16_t)((r + i) & 255);
        int16_t l = (int16_t)MC_MON_LO, rr = (int16_t)MC_MON_HI;
        m.scope_l[i] = l; m.scope_r[i] = rr;
        if (l > pk) pk = l; if (-l > pk) pk = (int16_t)-l;
        if (rr > pk) pk = rr; if (-rr > pk) pk = (int16_t)-rr;
    }
    /* the scope at a readable size: the peak to ~85% of full scale (at most 16x) */
    uint16_t g = (uint16_t)(28000 / pk);
    if (g > 16) g = 16;
    if (g > 1)
        for (uint16_t i = 0; i < 256; i++) {
            m.scope_l[i] = (int16_t)(m.scope_l[i] * (int16_t)g);
            m.scope_r[i] = (int16_t)(m.scope_r[i] * (int16_t)g);
        }
    m.scene = scenes[sc].title;
    m.notes = scenes[sc].notes;
    m.flags = scenes[sc].flags;
    m.scene_no = sc;
    m.scene_count = NSCENES;
}

int main(void)
{
    printf("mcdemo: the microcoded audio engine, feature by feature\n");
    if (MC_ID != 0xA0C2) {
        printf("This needs the microcoded engine's bitstream (cd tang && make mcaudio prog-mc).\n");
        return 1;
    }
    mc_init();
    view_init();
    last_blocks = MC_BLOCKS;
    on_block();                         /* the first scene's list: the engine renders only once it has one */
    irq_on(IRQ_AUDIO, on_block);
    irq_enable(1);
    for (;;) {
        monitor();
        view_frame(&m);
        int16_t k = (int16_t)CON_IN;
        if (k == 27 || k == 'q') break;
    }
    irq_enable(0);
    MC_CTRL = 0;
    return 0;
}
