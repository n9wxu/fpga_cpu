/* toccata: J. S. Bach (attr.), Toccata and Fugue in D minor, BWV 565, complete (the Mutopia
 * Project's public-domain edition, tools/score.py), on a pipe organ built from the microcoded audio
 * engine (docs/mcaudio.md), under the falling-notes visualizer (../sym40/src/viz.c). Needs the
 * -DMCAUDIO bitstream (cd tang && make prog-mc).
 *
 * The organ: every note sounds its manual's or the pedal's registration, its flue chorus one engine
 * node (a 16-bit sampled pipe, tools/pipes.py, in the range that keeps it alias-free) and at full
 * organ a reed another, from a pool of 64 nodes. A
 * released node rings on as the church's tail (a slow release) until the pool needs it; a node
 * silent long enough is skipped (w1 bit 31) so it costs the engine almost nothing. At start the
 * engine's cost a node is measured and the number sounding at once capped to fit a block.
 *
 * The score runs in the engine's block-done interrupt (node words are written only there, while the
 * engine waits between blocks); the main loop reads the score's state and draws. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "irq.h"
#include "mc.h"
#include "viz.h"
#include "score.h"
#include "pipes.h"

#define TICK_BLOCKS 32                  /* a tick: 32 blocks, 1/46.875 s (the score's unit) */
#define POOL        64
#define TAIL_TICKS  90                  /* a released node is silent (-60 dB) by then: skipped */
#define BLOCK_CLKS  45000               /* the engine's clocks a block (67.5 MHz / 1500) */

/* ---- the sections the visualizer shows, and the parts in them ---- */
enum { SEC_SOP, SEC_ALTO, SEC_TENOR, SEC_BASS, SEC_PEDAL, SEC__N };
static const char *const sec_name[SEC__N] = { "Soprano", "Alto", "Tenor", "Bass", "Pedal" };
static const uint8_t sec_rgb[SEC__N][3] = {
    { 255, 214, 90 },    /* soprano: gold */
    { 255, 120, 80 },    /* alto: coral */
    { 90, 230, 200 },    /* tenor: sea green */
    { 90, 150, 255 },    /* bass: blue */
    { 200, 100, 255 },   /* pedal: violet */
};
#define PX16 40                         /* 2.5 pixels a tick: about 117 a second */
#define VIZ_LOOK (76 * 16)              /* the falling notes listed: 190 pixels ahead */
static const viz_cfg_t viz_cfg = { SEC__N, sec_name, sec_rgb, PX16 };

/* ---- the registrations: a note's nodes, each a pipe sample (tools/pipes.py draws a registration's
 * flue ranks into one), its pitch from the key, its loudness ---- */
typedef struct { uint8_t timbre; int8_t semis; int8_t detune; uint16_t vol; uint16_t attack; } rank_t;
typedef struct { uint8_t n; rank_t r[2]; } reg_t;
/* the manuals: 0 soft (the fugue's opening), 1, 2 bright (2 2/3' and 2'), 3 full organ: and the trumpet */
static const reg_t manual[4] = {
    { 1, { { T_MAN_SOFT, 0, 0, 0x1080, 0x0500 } } },
    { 1, { { T_MAN_MID, 0, 0, 0x1080, 0x0500 } } },
    { 1, { { T_MAN_FULL, 0, 0, 0x1200, 0x0600 } } },
    { 2, { { T_MAN_FULL, 0, 0, 0x1080, 0x0600 }, { T_REED, 0, 6, 0x0A80, 0x0900 } } },
};
/* the pedals sound an octave down (16'); full: and the trombone */
static const reg_t pedal[4] = {
    { 1, { { T_PED, -12, 0, 0x1E00, 0x0300 } } },
    { 1, { { T_PED, -12, 0, 0x1E00, 0x0300 } } },
    { 1, { { T_PED_FULL, -12, 0, 0x1E00, 0x0300 } } },
    { 2, { { T_PED_FULL, -12, 0, 0x1C80, 0x0300 }, { T_REED, -12, 5, 0x0F00, 0x0500 } } },
};
#define REL_RATE 0x0060                 /* release: 96/32768 of what is left a block, -60 dB in ~1.6 s */

typedef struct {
    const sc_ev_t *ev;
    uint8_t sec, is_pedal;              /* the voice of its sounding note; on the pedals */
    uint16_t i, end;                    /* the next event; the tick its sounding note ends */
    uint8_t vel;
    uint8_t node[2], nn;                /* the nodes sounding its note */
} part_t;

static part_t parts[SC_PARTS];          /* the score's parts: each hand's chords dealt out (score.py) */
#define NPARTS SC_PARTS

/* ---- the node pool ---- */
static uint32_t w3[POOL];
static uint8_t  gated[POOL], skipped[POOL];
static uint32_t rel_at[POOL];           /* the tick it was released (or skipped) */
static uint8_t  max_active = 40, nactive, ngated, peak_active;

static void W(uint8_t n, uint8_t w, uint32_t v) { MC_NODE(n)[w] = v; }

static void node_init(uint8_t n)
{
    for (uint8_t k = 1; k < 16; k++) W(n, k, 0);
    W(n, 1, 0x80000000UL | MC_SMP16);   /* skipped */
    W(n, 10, (uint32_t)REL_RATE << 16 | 0x0100); W(n, 11, 0x7FFFUL << 16);
    w3[n] = 0; gated[n] = 0; skipped[n] = 1; rel_at[n] = 0;
}

static void skip(uint8_t n)
{
    W(n, 1, 0x80000000UL | MC_SMP16);
    skipped[n] = 1; nactive--;
}

/* a node for a new note: a skipped one released longest ago; else, if the engine has room, any
 * skipped; else the active one released longest ago (its tail cut); else the oldest gated */
static uint32_t abs_tick;
static uint8_t alloc(void)
{
    int16_t best = -1, oldest = -1;
    for (uint8_t n = 0; n < POOL; n++) {
        if (gated[n]) { if (oldest < 0 || rel_at[n] < rel_at[oldest]) oldest = (int16_t)n; continue; }
        if (skipped[n] && nactive < max_active) return n;
        if (!skipped[n] && (best < 0 || rel_at[n] < rel_at[best])) best = (int16_t)n;
    }
    if (best >= 0) return (uint8_t)best;
    return (uint8_t)(oldest >= 0 ? oldest : 0);
}

static void sound(uint8_t n, const rank_t *rk, uint8_t key, uint8_t vel, uint8_t is_pedal)
{
    int16_t m = (int16_t)(key + rk->semis);
    while (m > 108) m -= 12;            /* a rank too high breaks back an octave, as mixtures do */
    uint8_t range = 0;
    while (range < 3 && m > pipe_top[range]) range++;
    uint32_t step = mc_step(m) >> pipe_shift[range];
    step += (step >> 12) * (uint32_t)(int32_t)rk->detune;       /* a reed beats against the flues */
    uint32_t base = MC_WADDR(pipe[rk->timbre][range]);
    uint32_t v = ((uint32_t)rk->vol * (64 + vel / 2)) >> 7;
    /* the pipes stand on two chests, C and C# sides: a note's ranks lean left or right */
    uint16_t pan = is_pedal ? 128 : (key & 1) ? 168 : 88;
    uint32_t l = v * (256 - pan) >> 7, r = v * pan >> 7;
    if (l > 0x7FFF) l = 0x7FFF;
    if (r > 0x7FFF) r = 0x7FFF;
    if (skipped[n]) { skipped[n] = 0; nactive++; }
    if (!gated[n]) ngated++;
    W(n, 1, MC_SMP16);
    W(n, 2, step);
    W(n, 4, r << 16 | l);
    W(n, 5, base); W(n, 6, (uint32_t)PIPE_LEN << 16); W(n, 7, (uint32_t)PIPE_LEN << 16); W(n, 8, base);
    W(n, 9, (uint32_t)rk->attack << 16);
    w3[n] = ((w3[n] + 1) & 0xFF) | 0x100;
    W(n, 3, w3[n]);
    gated[n] = 1;
    rel_at[n] = abs_tick;
}

static void release(part_t *pt)
{
    for (uint8_t k = 0; k < pt->nn; k++) {
        uint8_t n = pt->node[k];
        if (!gated[n]) continue;
        w3[n] &= ~0x100UL;
        W(n, 3, w3[n]);
        gated[n] = 0; ngated--;
        rel_at[n] = abs_tick;
    }
    pt->nn = 0;
}

/* ---- the note-ons the visualizer has not seen: a ring the interrupt fills ---- */
static viz_note_t nring[64];
static volatile uint8_t nhead, ntail;
static uint8_t cur_reg;

static void note_on(part_t *pt, uint8_t key, uint8_t vel, uint8_t sec)
{
    pt->sec = sec;
    release(pt);
    const reg_t *rg = pt->is_pedal ? &pedal[cur_reg] : &manual[cur_reg];
    for (uint8_t k = 0; k < rg->n; k++) {
        uint8_t n = alloc();
        for (uint8_t p = 0; p < NPARTS; p++)          /* a stolen gated node: its part forgets it */
            for (uint8_t j = 0; j < parts[p].nn; j++)
                if (parts[p].node[j] == n) parts[p].node[j] = parts[p].node[--parts[p].nn];
        if (gated[n]) { gated[n] = 0; ngated--; }
        sound(n, &rg->r[k], key, vel, pt->is_pedal);
        pt->node[pt->nn++] = n;
    }
    pt->vel = vel;
    uint8_t h = nhead;
    if ((uint8_t)(h + 1 - ntail) < 64) {
        nring[h & 63].inst = pt->sec; nring[h & 63].midi = key; nring[h & 63].vel = vel;
        nhead = (uint8_t)(h + 1);
    }
}

/* ---- the interrupt: one a block ---- */
static uint16_t last_blocks, tick_wait, tick, under0;
static volatile uint16_t cur_tick;
static volatile uint32_t cur_abs;
static uint8_t sec_i;

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
        tick = 0; sec_i = 0;
        for (uint8_t p = 0; p < NPARTS; p++) parts[p].i = 0;
    }
    if (tick == 0) under0 = MC_UNDER;   /* underruns are counted from the start of each time through */
    while (sec_i + 1 < SC_SECTIONS && sc_sec_tick[sec_i + 1] <= tick) sec_i++;
    cur_reg = sc_sec_reg[sec_i];
    for (uint8_t p = 0; p < NPARTS; p++) {
        part_t *pt = &parts[p];
        if (pt->nn && pt->end == tick) release(pt);
        const sc_ev_t *e = &pt->ev[pt->i];
        if (e->t == tick) {
            note_on(pt, e->midi, e->vel, e->sec);
            pt->end = (uint16_t)(tick + e->len);
            pt->i++;
        }
    }
    for (uint8_t n = 0; n < POOL; n++)                   /* tails that have died away: skipped */
        if (!gated[n] && !skipped[n] && abs_tick - rel_at[n] > TAIL_TICKS) skip(n);
    cur_tick = tick;
    cur_abs = abs_tick++;
    tick++;
}

/* ---- the engine's cost a node, measured: k nodes sounding (silently), the block's clocks ---- */
static uint16_t clocks_with(uint8_t k)
{
    static const rank_t quiet = { T_REED, 0, 0, 0, 0x7FFF };
    for (uint8_t n = 0; n < POOL; n++) {
        if (n < k) { sound(n, &quiet, (uint8_t)(48 + n), 0, 0); W(n, 4, 0); }
        else if (!skipped[n]) skip(n);
    }
    uint16_t b = MC_BLOCKS;
    while ((uint16_t)(MC_BLOCKS - b) < 20) ;
    uint16_t mx = 0;
    for (uint8_t i = 0; i < 16; i++) {
        b = MC_BLOCKS;
        while (MC_BLOCKS == b) ;
        uint16_t c = MC_CLOCKS;
        if (c > mx) mx = c;
    }
    return mx;
}

static void calibrate(void)
{
    uint16_t c0 = clocks_with(0), c32 = clocks_with(32), c64 = clocks_with(64);
    uint16_t per = (uint16_t)((c64 - c0) / 64);
    uint32_t room = (BLOCK_CLKS * 85UL) / 100;           /* 85% of a block, the rest for the video's share of RAM */
    uint32_t m = per ? (room - c0) / per : POOL;
    max_active = (uint8_t)(m > POOL ? POOL : m);
    printf("engine: %u clocks a block idle, %u with 32 nodes, %u with 64: %u a node; up to %u at once\n",
           c0, c32, c64, per, max_active);
    for (uint8_t n = 0; n < POOL; n++) node_init(n);
    nactive = 0; ngated = 0;
}

/* ---- the screen's view of the score ---- */
static viz_t v;
static uint16_t sec_lvl[SEC__N], out_peak;
static void monitor(void)
{
    uint16_t t = cur_tick;
    uint16_t tw = tick_wait;
    uint32_t ab = cur_abs;
    if (cur_tick != t) { t = cur_tick; ab = cur_abs; }      /* (an interrupt between the reads) */
    int16_t frac = (int16_t)(((uint32_t)(TICK_BLOCKS - (tw > TICK_BLOCKS ? TICK_BLOCKS : tw)) * 15) / TICK_BLOCKS);
    v.now16 = ab * 16 + (uint32_t)frac;

    for (uint8_t s = 0; s < SEC__N; s++) sec_lvl[s] = (uint16_t)(sec_lvl[s] - (sec_lvl[s] >> 3));
    for (uint8_t p = 0; p < NPARTS; p++)
        if (parts[p].nn) {
            uint16_t l = (uint16_t)(parts[p].vel << 8);
            if (l > sec_lvl[parts[p].sec]) sec_lvl[parts[p].sec] = l;
        }
    for (uint8_t s = 0; s < SEC__N; s++) v.level[s] = sec_lvl[s];

    uint16_t r = MC_MON_IDX;
    for (uint16_t i = 0; i < 256; i++) {
        MC_MON_IDX = (uint16_t)((r + i) & 255);
        int16_t l = (int16_t)MC_MON_LO, rr = (int16_t)MC_MON_HI;
        v.scope[i] = (int16_t)((l >> 1) + (rr >> 1));
        uint16_t al = (uint16_t)(l < 0 ? -l : l), ar = (uint16_t)(rr < 0 ? -rr : rr);
        if (al > out_peak) out_peak = al;
        if (ar > out_peak) out_peak = ar;
    }
    v.nnotes = 0;
    while (ntail != nhead && v.nnotes < 32) { v.notes[v.nnotes++] = nring[ntail & 63]; ntail = (uint8_t)(ntail + 1); }

    /* the falling notes: each part from its sounding note on, to VIZ_LOOK ahead (round the loop) */
    v.nfall = 0;
    for (uint8_t p = 0; p < NPARTS && v.nfall < 192; p++) {
        const sc_ev_t *ev = parts[p].ev;
        uint16_t i = parts[p].i;
        i = i ? (uint16_t)(i - 1) : 0;
        uint16_t wrap = 0;
        for (uint8_t guard = 0; guard < 64 && v.nfall < 192; guard++) {
            if (ev[i].t == 0xFFFF) { if (wrap) break; wrap = SC_TICKS; i = 0; continue; }
            int32_t rel = ((int32_t)ev[i].t + wrap - (int32_t)t) * 16 - frac;
            if (rel >= VIZ_LOOK) break;
            if (rel + (int32_t)ev[i].len * 16 > 0) {
                viz_fall_t *f = &v.fall[v.nfall++];
                f->inst = ev[i].sec; f->midi = ev[i].midi; f->start = (int16_t)rel; f->len = (uint16_t)(ev[i].len * 16);
            }
            i++;
        }
    }
    uint16_t b = 0;
    while (b + 1 < SC_BARS && sc_bar[b + 1] <= t) b++;
    v.bar = (uint16_t)(b + 1);
    v.beat = (uint16_t)((t / 12) & 1);
    v.voices = nactive;
    v.progress = (uint16_t)((uint32_t)t * 65535u / SC_TICKS);
    if (nactive > peak_active) peak_active = nactive;
    v.blocks_busy = MC_CLOCKS;
    v.underruns = (uint16_t)(MC_UNDER - under0);
    v.section = sc_sec_name[sec_i];
}

int main(void)
{
    printf("toccata: J. S. Bach, Toccata and Fugue in D minor, BWV 565, on a %u-node pipe organ\n", POOL);
    if (MC_ID != 0xA0C2) {
        printf("This needs the microcoded engine's bitstream (cd tang && make mcaudio prog-mc).\n");
        return 1;
    }
    mc_init();
    for (uint8_t p = 0; p < NPARTS; p++) { parts[p].ev = sc_part[p]; parts[p].is_pedal = sc_part_pedal[p]; }
    static uint8_t order[POOL];
    for (uint8_t n = 0; n < POOL; n++) { node_init(n); order[n] = n; }
    mc_list(order, 0, POOL);
    calibrate();
    v.title = "J. S. Bach: Toccata and Fugue in D minor, BWV 565";
    viz_init(&viz_cfg);
    last_blocks = MC_BLOCKS;
    tick = 0; tick_wait = 0;
    irq_on(IRQ_AUDIO, on_block);
    irq_enable(1);
    uint16_t f0 = FRAME;
    for (;;) {
        monitor();
        viz_frame(&v);
        if ((uint16_t)(FRAME - f0) >= 300) {             /* every 5 s: the most nodes sounding, the loudest */
            f0 = FRAME;
            printf("bar %u: up to %u nodes sounding (cap %u), output peak %u, underruns %u\n",
                   v.bar, peak_active, max_active, out_peak, v.underruns);
            peak_active = 0; out_peak = 0;
        }
        int16_t k = (int16_t)CON_IN;
        if (k == 27 || k == 'q') break;
    }
    irq_enable(0);
    MC_CTRL = 0;
    return 0;
}
