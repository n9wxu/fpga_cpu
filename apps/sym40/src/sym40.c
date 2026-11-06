/* sym40: the stress test. Mozart's Symphony No. 40 in G minor, first movement (arranged,
 * tools/score.py), played by a 23-node orchestra on the microcoded audio engine (docs/mcaudio.md),
 * under a music visualizer that works the video hardware: sprites, the second playfield, line
 * scroll, the blitter (viz.c). Needs the -DMCAUDIO bitstream (cd tang && make prog-mc).
 *
 * The score runs in the engine's block-done interrupt (node words are written only there, while the
 * engine waits between blocks); the main loop reads the engine's monitor and draws. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "irq.h"
#include "mc.h"
#include "viz.h"
#include "score.h"
#include "instruments.h"

enum { VI_VIOLIN1, VI_VIOLIN2, VI_VIOLA, VI_CELLO, VI_BASS, VI_FLUTE, VI_OBOE, VI_CLARINET, VI_BASSOON, VI_HORN, VI__N };
static const char *const sec_name[VI__N] = { "Vln I", "Vln II", "Viola", "Cello", "Bass", "Flute", "Oboe", "Clar", "Bsn", "Horn" };
static const uint8_t sec_rgb[VI__N][3] = {
    { 255, 214, 90 }, { 255, 160, 60 }, { 255, 105, 80 }, { 230, 60, 110 }, { 170, 90, 230 },   /* strings: gold to violet */
    { 90, 230, 255 }, { 110, 240, 140 }, { 80, 160, 255 }, { 200, 220, 90 }, { 255, 250, 210 },  /* winds, horn */
};
#define VIZ_LOOK (16 * 16)              /* the falling notes listed: 16 eighths ahead (12 pixels each) */
static const viz_cfg_t viz_cfg = { VI__N, sec_name, sec_rgb, 12 * 16 };

#define TICK_BLOCKS 208                 /* an eighth note at half note = 108: 1500 * 60 / 432 */

typedef struct {
    const int16_t *smp;                 /* a sampled instrument (instruments.h, 16-bit), or 0: an oscillator */
    uint16_t entry, vol;                /* routine; volume at velocity 127 (Q15) */
    uint16_t a, d, r, s;                /* envelope */
    uint16_t width, lfo, vib;           /* pulse width, LFO rate, vibrato depth */
    uint8_t  route;                     /* 1: through the filter bus */
} patch_t;

/* The balance: the first violins (the melody) on top; the accompaniment (violas' eighths, cellos,
 * horns, bassoons) well under it. Strings and horns are sampled (instruments.h); the winds are
 * oscillators through the filter bus, a low-pass that takes the edge off the pulses.
 *                                              entry         vol     a       d       r       s       width lfo vib route */
static const patch_t P_VN1  = { ins_violin, MC_SMP16,     0x1C00, 0x0300, 0x0100, 0x0500, 0x6800, 0,    0,  0,  0 };
static const patch_t P_VN2  = { ins_violin, MC_SMP16,     0x0B00, 0x0300, 0x0100, 0x0500, 0x6000, 0,    0,  0,  0 };
static const patch_t P_VA   = { ins_lowstr, MC_SMP16,     0x0900, 0x0400, 0x0100, 0x0400, 0x6000, 0,    0,  0,  0 };
static const patch_t P_VC   = { ins_lowstr, MC_SMP16,     0x0D00, 0x0300, 0x0100, 0x0400, 0x6000, 0,    0,  0,  0 };
static const patch_t P_BASS = { ins_lowstr, MC_SMP16,     0x1000, 0x0400, 0x0100, 0x0300, 0x6800, 0,    0,  0,  0 };
static const patch_t P_HN   = { ins_horn,   MC_SMP16,     0x0A00, 0x0180, 0x0080, 0x0300, 0x6800, 0,    0,  0,  0 };
static const patch_t P_FL   = { 0,          MC_OSC_TRI,   0x1800, 0x0500, 0x0080, 0x0400, 0x6000, 0,  260, 420, 0 };
static const patch_t P_OB   = { 0,          MC_OSC_PULSE, 0x0A00, 0x0800, 0x0100, 0x0600, 0x5800, 1400, 250, 250, 1 };
static const patch_t P_CL   = { 0,          MC_OSC_PULSE, 0x0C00, 0x0600, 0x0100, 0x0500, 0x6000, 2048, 0,  0,  1 };
static const patch_t P_BN   = { 0,          MC_OSC_PULSE, 0x0900, 0x0600, 0x0100, 0x0500, 0x6000, 1400, 0,  0,  1 };

typedef struct {
    const sc_ev_t *ev;
    uint8_t first, n;                   /* its nodes */
    uint8_t inst;                       /* VI_* */
    const patch_t *p;
    int8_t transpose;
    uint16_t i, end;                    /* the next event; the tick the sounding note ends */
} part_t;

static part_t parts[] = {
    { sc_vn1, 0, 4, VI_VIOLIN1, &P_VN1, 0 },  { sc_vn2, 4, 4, VI_VIOLIN2, &P_VN2, 0 },
    { sc_va1, 8, 1, VI_VIOLA, &P_VA, 0 },     { sc_va2, 9, 1, VI_VIOLA, &P_VA, 0 },
    { sc_vc, 10, 2, VI_CELLO, &P_VC, 0 },     { sc_cb, 12, 1, VI_BASS, &P_BASS, 0 },
    { sc_fl, 13, 1, VI_FLUTE, &P_FL, 0 },     { sc_ob1, 14, 1, VI_OBOE, &P_OB, 0 },
    { sc_ob2, 15, 1, VI_OBOE, &P_OB, 0 },     { sc_cl1, 16, 1, VI_CLARINET, &P_CL, 0 },
    { sc_cl2, 17, 1, VI_CLARINET, &P_CL, 0 }, { sc_bn1, 18, 1, VI_BASSOON, &P_BN, 0 },
    { sc_bn2, 19, 1, VI_BASSOON, &P_BN, 0 },  { sc_hn1, 20, 1, VI_HORN, &P_HN, 0 },
    { sc_hn2, 21, 1, VI_HORN, &P_HN, 0 },
};
#define NPARTS (sizeof parts / sizeof parts[0])
#define FLT_NODE 22
#define NNODES 23

static uint32_t w3[NNODES];
static uint8_t  node_inst[NNODES], node_vel[NNODES];

static void W(uint8_t n, uint8_t w, uint32_t v) { MC_NODE(n)[w] = v; }

static void setup(uint8_t n, const patch_t *p)
{
    W(n, 1, p->entry);
    W(n, 4, 0);
    if (p->smp) {                       /* the sample: one 8192-frame loop */
        uint32_t base = MC_WADDR(p->smp);
        W(n, 5, base); W(n, 6, (uint32_t)INS_LEN << 16); W(n, 7, (uint32_t)INS_LEN << 16); W(n, 8, base);   /* the whole sample loops */
    } else {
        W(n, 5, p->width); W(n, 6, (uint32_t)p->lfo << 16); W(n, 7, 0); W(n, 8, p->vib);
    }
    W(n, 9, (uint32_t)p->a << 16); W(n, 10, (uint32_t)p->r << 16 | p->d); W(n, 11, (uint32_t)p->s << 16);
    w3[n] = (uint32_t)p->route << 12;
    W(n, 3, w3[n]);
    W(n, 12, 0); W(n, 13, 0); W(n, 14, 0); W(n, 15, (uint32_t)(n * 977u) << 16);   /* LFOs apart */
}

/* ---- the note-ons the visualizer has not seen: a ring the interrupt fills */
static viz_note_t nring[64];
static volatile uint8_t nhead, ntail;

static void note_on(part_t *pt, uint8_t midi, uint8_t vel)
{
    uint32_t s = mc_step((int16_t)(midi + pt->transpose));
    if (pt->p->smp) s >>= 10;           /* a sample: 64 frames a cycle at 48 kHz, 16.16 */
    uint16_t v = (uint16_t)(((uint32_t)pt->p->vol * vel) >> 7);
    for (uint8_t k = 0; k < pt->n; k++) {
        uint8_t n = (uint8_t)(pt->first + k);
        int32_t det = (int32_t)(s >> 10) * (2 * (int16_t)k - (int16_t)pt->n + 1);   /* +-0.1% a node */
        W(n, 2, (uint32_t)((int32_t)s + det));
        W(n, 4, (uint32_t)v << 16 | v);
        w3[n] = (w3[n] & 0x1000UL) | ((w3[n] + 1) & 0xFF) | 0x100;
        W(n, 3, w3[n]);
        node_vel[n] = vel;
    }
    uint8_t h = nhead;
    if ((uint8_t)(h + 1 - ntail) < 64) {
        nring[h & 63].inst = pt->inst; nring[h & 63].midi = midi; nring[h & 63].vel = vel;
        nhead = (uint8_t)(h + 1);
    }
}

static void note_off(part_t *pt)
{
    for (uint8_t k = 0; k < pt->n; k++) {
        uint8_t n = (uint8_t)(pt->first + k);
        w3[n] &= ~0x100UL;
        W(n, 3, w3[n]);
    }
}

/* ---- the interrupt: one a block */
static uint16_t last_blocks, tick_wait, tick, under0;
static volatile uint16_t cur_tick;
static volatile uint32_t cur_abs;       /* ticks since the start, for the falling notes' scroll */
static uint32_t abs_tick;

static void on_block(void)
{
    MC_IRQ = 0xFF;
    IRQ_PEND = (uint16_t)(1u << IRQ_AUDIO);
    uint16_t now = MC_BLOCKS;
    uint16_t d = (uint16_t)(now - last_blocks);
    last_blocks = now;
    if (tick_wait > d) { tick_wait = (uint16_t)(tick_wait - d); return; }
    tick_wait = TICK_BLOCKS;
    if (tick >= SC_TICKS) {
        tick = 0;
        for (uint8_t p = 0; p < NPARTS; p++) parts[p].i = 0;
    }
    if (tick == 0) under0 = MC_UNDER;   /* underruns are counted from the start of each time through */
    for (uint8_t p = 0; p < NPARTS; p++) {
        part_t *pt = &parts[p];
        if (pt->end == tick) note_off(pt);
        const sc_ev_t *e = &pt->ev[pt->i];
        if (e->t == tick) {
            note_on(pt, e->midi, e->vel);
            pt->end = (uint16_t)(tick + e->len);
            pt->i++;
        }
    }
    cur_tick = tick;
    cur_abs = abs_tick++;
    tick++;
}

static void start(void)
{
    static uint8_t order[NNODES];
    for (uint8_t p = 0; p < NPARTS; p++)
        for (uint8_t k = 0; k < parts[p].n; k++) {
            uint8_t n = (uint8_t)(parts[p].first + k);
            setup(n, parts[p].p);
            node_inst[n] = parts[p].inst;
        }
    /* the winds' filter: a gentle low-pass at ~2.4 kHz, little resonance */
    W(FLT_NODE, 1, MC_FLT); W(FLT_NODE, 2, 0x2800); W(FLT_NODE, 3, 0x5800); W(FLT_NODE, 4, 0x7FFF7FFFUL);
    W(FLT_NODE, 5, 0); W(FLT_NODE, 6, 0); W(FLT_NODE, 7, 0);
    W(FLT_NODE, 12, 0); W(FLT_NODE, 13, 0); W(FLT_NODE, 14, 0); W(FLT_NODE, 15, 0);
    for (uint8_t i = 0; i < NNODES; i++) order[i] = i;
    mc_list(order, 0, NNODES);
    tick = 0; tick_wait = 0;
}

/* ---- the monitor, for the visualizer */
static viz_t v;
static void monitor(void)
{
    for (uint8_t k = 0; k < VI__N; k++) v.level[k] = 0;
    for (uint8_t n = 0; n < FLT_NODE; n++) {
        MC_MON_IDX = (uint16_t)(256 + n);
        uint16_t lvl = MC_MON_HI;                               /* the envelope, Q15 */
        uint16_t l = (uint16_t)(((uint32_t)lvl * node_vel[n]) >> 7);
        if (l > v.level[node_inst[n]]) v.level[node_inst[n]] = l;
    }
    uint16_t r = MC_MON_IDX;
    for (uint16_t i = 0; i < 256; i++) {
        MC_MON_IDX = (uint16_t)((r + i) & 255);
        int16_t l = (int16_t)MC_MON_LO, rr = (int16_t)MC_MON_HI;
        v.scope[i] = (int16_t)((l >> 1) + (rr >> 1));
    }
    v.nnotes = 0;
    while (ntail != nhead && v.nnotes < 32) { v.notes[v.nnotes++] = nring[ntail & 63]; ntail = (uint8_t)(ntail + 1); }
    uint16_t t = cur_tick;
    /* the falling notes: each part from its sounding note on, to VIZ_LOOK ahead (round the loop) */
    {
        uint16_t tw = tick_wait;
        uint32_t ab = cur_abs;
        if (cur_tick != t) { t = cur_tick; ab = cur_abs; }      /* (an interrupt between the reads) */
        int16_t frac = (int16_t)(((uint32_t)(TICK_BLOCKS - (tw > TICK_BLOCKS ? TICK_BLOCKS : tw)) * 15) / TICK_BLOCKS);
        v.now16 = ab * 16 + (uint32_t)frac;
        v.nfall = 0;
        for (uint8_t p = 0; p < NPARTS && v.nfall < 192; p++) {
            const sc_ev_t *ev = parts[p].ev;
            uint16_t i = parts[p].i;
            i = i ? (uint16_t)(i - 1) : 0;
            uint16_t wrap = 0;
            for (uint8_t guard = 0; guard < 64 && v.nfall < 192; guard++) {
                if (ev[i].t == 0xFFFF) { if (wrap) break; wrap = SC_TICKS; i = 0; continue; }
                int16_t rel = (int16_t)(((int16_t)(ev[i].t + wrap) - (int16_t)t) * 16 - frac);
                if (rel >= VIZ_LOOK) break;
                if (rel + (int16_t)ev[i].len * 16 > 0) {
                    viz_fall_t *f = &v.fall[v.nfall++];
                    f->inst = parts[p].inst; f->midi = ev[i].midi; f->start = rel; f->len = (uint16_t)(ev[i].len * 16);
                }
                i++;
            }
        }
    }
    v.bar = (uint16_t)(t / 8 + 1);
    v.beat = (uint16_t)((t / 4) & 1);
    v.voices = NNODES;
    v.progress = (uint16_t)((uint32_t)t * 65535u / SC_TICKS);
    v.blocks_busy = MC_CLOCKS;
    v.underruns = (uint16_t)(MC_UNDER - under0);
    v.section = sc_section_name[sc_bar_section[t / 8 < SC_BARS ? t / 8 : SC_BARS - 1]];
}

int main(void)
{
    printf("sym40: Mozart, Symphony No. 40 in G minor, I - a stress test for the engine and the video\n");
    if (MC_ID != 0xA0C2) {
        printf("This needs the microcoded engine's bitstream (cd tang && make mcaudio prog-mc).\n");
        return 1;
    }
    mc_init();
    v.title = "W. A. Mozart: Symphony No. 40 in G minor, K. 550 - I. Molto allegro";
    viz_init(&viz_cfg);
    last_blocks = MC_BLOCKS;
    start();                            /* the list, before the engine has one: it renders only then */
    irq_on(IRQ_AUDIO, on_block);
    irq_enable(1);
    for (;;) {
        monitor();
        viz_frame(&v);
        int16_t k = (int16_t)CON_IN;
        if (k == 27 || k == 'q') break;
    }
    irq_enable(0);
    MC_CTRL = 0;
    return 0;
}
