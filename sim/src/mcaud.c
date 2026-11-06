/* fpga_cpu simulator: the Tang's microcoded audio engine (fpga/rtl/mcaudio, docs/mcaudio.md), bit
 * exact: the output samples, the nodes' write-back words, the monitor and the counters match the
 * RTL (sim/tools/mcaud_check.py runs both on the same RAM image and control store).
 *
 * The engine is a block renderer: a block is 32 output frames, made from the node list in RAM as
 * it is when the block falls due (the 48 kHz frame clock, snd_frame_cycle(32 n)). One block is:
 * walk the list (a node, or up to 4 linked nodes fetched into the node buffer), run the head's
 * routine on the micro-sequencer, write words 12-15 of each node back, then the main mix bus,
 * saturated, goes out and every bus is cleared (mc_engine.v). The micro-sequencer (mc_core.v) is
 * run clock by clock from whatever control store the program loaded, since its pipeline rules
 * decide the results: MDR is the read issued two words before, a product reaches the accumulator
 * two words after its MAC, a bus add lands two clocks after its word, flags steer the next word.
 * Only the walker's and the RAM's waits are not simulated (they cannot change a result: a DMA
 * stalls the sequencer, which just lets its pipelines drain). */
#include "cpu.h"
#include "snd.h"

#include <string.h>

#define RATE     48000u
#define BLK      32u                   /* frames in a block */
#define AMASK    0x3FFFFFu             /* RAM word addresses are 22 bits (AW = 22) */
#define DMA_CAP  4000000               /* micro-words in a routine before it is abandoned (a runaway) */

/* a control-store word, decoded when the CPU commits it */
typedef struct {
    uint16_t nxt, imm;
    uint8_t bc, aop, rd, ra, rb, bsel, setf, mop, masel, mbsel, memop, sh, sfmt, pred;
} uw_t;

/* the datapath's registers that live across words (and blocks); a routine runs on a copy in locals */
typedef struct {
    int64_t acc, mp;                   /* 40 bits, 36 bits */
    uint32_t mdr, pend, fres, accp_d, bus_wd, bq;
    uint32_t mop_p, fc, cnt, route, badd, bus_we, bus_wa, bwa;
} core_t;

static struct {
    /* the CPU port (mc_regs.v) */
    uint8_t run, oshift;
    uint32_t head;
    uint16_t tempo, cs_a, cs_d[4];
    uint8_t cs_d4;
    uint16_t mon_idx, st_blocks, st_clk, st_bursts, st_words, st_nodes;
    uint8_t irq, mring;
    uint32_t mon[512];                 /* 0-255 the output ring {R, L}, 256-287 the nodes' meters */
    uint16_t tacc;                     /* the tempo accumulator, and the tick it carried */
    uint8_t tick;
    /* the buffers */
    uint32_t nb[64], sb[512], bus[128], rf[16];
    uw_t cs[512];
    uint16_t tb[256];                  /* the sine table */
    core_t k;
    /* the schedule */
    uint64_t n, due;                   /* the next block's index, and the cycle it is due */
    uint16_t c_bursts, c_words;
} M;

static const uint16_t sine_tab[256] = {   /* fpga/rtl/mcaudio/mc_sine.mem */
    0x0000, 0x0324, 0x0648, 0x096a, 0x0c8c, 0x0fab, 0x12c8, 0x15e2, 0x18f9, 0x1c0b, 0x1f1a, 0x2223,
    0x2528, 0x2826, 0x2b1f, 0x2e11, 0x30fb, 0x33df, 0x36ba, 0x398c, 0x3c56, 0x3f17, 0x41ce, 0x447a,
    0x471c, 0x49b4, 0x4c3f, 0x4ebf, 0x5133, 0x539b, 0x55f5, 0x5842, 0x5a82, 0x5cb3, 0x5ed7, 0x60eb,
    0x62f1, 0x64e8, 0x66cf, 0x68a6, 0x6a6d, 0x6c23, 0x6dc9, 0x6f5e, 0x70e2, 0x7254, 0x73b5, 0x7504,
    0x7641, 0x776b, 0x7884, 0x7989, 0x7a7c, 0x7b5c, 0x7c29, 0x7ce3, 0x7d89, 0x7e1d, 0x7e9c, 0x7f09,
    0x7f61, 0x7fa6, 0x7fd8, 0x7ff5, 0x7fff, 0x7ff5, 0x7fd8, 0x7fa6, 0x7f61, 0x7f09, 0x7e9c, 0x7e1d,
    0x7d89, 0x7ce3, 0x7c29, 0x7b5c, 0x7a7c, 0x7989, 0x7884, 0x776b, 0x7641, 0x7504, 0x73b5, 0x7254,
    0x70e2, 0x6f5e, 0x6dc9, 0x6c23, 0x6a6d, 0x68a6, 0x66cf, 0x64e8, 0x62f1, 0x60eb, 0x5ed7, 0x5cb3,
    0x5a82, 0x5842, 0x55f5, 0x539b, 0x5133, 0x4ebf, 0x4c3f, 0x49b4, 0x471c, 0x447a, 0x41ce, 0x3f17,
    0x3c56, 0x398c, 0x36ba, 0x33df, 0x30fb, 0x2e11, 0x2b1f, 0x2826, 0x2528, 0x2223, 0x1f1a, 0x1c0b,
    0x18f9, 0x15e2, 0x12c8, 0x0fab, 0x0c8c, 0x096a, 0x0648, 0x0324, 0x0000, 0xfcdc, 0xf9b8, 0xf696,
    0xf374, 0xf055, 0xed38, 0xea1e, 0xe707, 0xe3f5, 0xe0e6, 0xdddd, 0xdad8, 0xd7da, 0xd4e1, 0xd1ef,
    0xcf05, 0xcc21, 0xc946, 0xc674, 0xc3aa, 0xc0e9, 0xbe32, 0xbb86, 0xb8e4, 0xb64c, 0xb3c1, 0xb141,
    0xaecd, 0xac65, 0xaa0b, 0xa7be, 0xa57e, 0xa34d, 0xa129, 0x9f15, 0x9d0f, 0x9b18, 0x9931, 0x975a,
    0x9593, 0x93dd, 0x9237, 0x90a2, 0x8f1e, 0x8dac, 0x8c4b, 0x8afc, 0x89bf, 0x8895, 0x877c, 0x8677,
    0x8584, 0x84a4, 0x83d7, 0x831d, 0x8277, 0x81e3, 0x8164, 0x80f7, 0x809f, 0x805a, 0x8028, 0x800b,
    0x8001, 0x800b, 0x8028, 0x805a, 0x809f, 0x80f7, 0x8164, 0x81e3, 0x8277, 0x831d, 0x83d7, 0x84a4,
    0x8584, 0x8677, 0x877c, 0x8895, 0x89bf, 0x8afc, 0x8c4b, 0x8dac, 0x8f1e, 0x90a2, 0x9237, 0x93dd,
    0x9593, 0x975a, 0x9931, 0x9b18, 0x9d0f, 0x9f15, 0xa129, 0xa34d, 0xa57e, 0xa7be, 0xaa0b, 0xac65,
    0xaecd, 0xb141, 0xb3c1, 0xb64c, 0xb8e4, 0xbb86, 0xbe32, 0xc0e9, 0xc3aa, 0xc674, 0xc946, 0xcc21,
    0xcf05, 0xd1ef, 0xd4e1, 0xd7da, 0xdad8, 0xdddd, 0xe0e6, 0xe3f5, 0xe707, 0xea1e, 0xed38, 0xf055,
    0xf374, 0xf696, 0xf9b8, 0xfcdc,
};

static inline uint64_t due_of(uint64_t n) { return snd_frame_cycle(BLK * n, RATE); }

void mcaud_reset(void)
{
    memset(&M, 0, sizeof M);
    memcpy(M.tb, sine_tab, sizeof M.tb);
    M.k.fres = 1;                        /* flags: Z clear, C clear, N clear */
    M.due = UINT64_MAX;
}

static void cs_decode(int a, uint64_t lo, uint8_t hi)
{
    uw_t *w = &M.cs[a];
    w->nxt = (uint16_t)(((unsigned)hi << 1) | (unsigned)(lo >> 63));
    w->bc = (lo >> 60) & 7;    w->aop = (lo >> 56) & 15;   w->rd = (lo >> 52) & 15;
    w->ra = (lo >> 48) & 15;   w->rb = (lo >> 44) & 15;    w->bsel = (lo >> 42) & 3;
    w->setf = (lo >> 41) & 1;  w->mop = (lo >> 38) & 7;    w->masel = (lo >> 36) & 3;
    w->mbsel = (lo >> 34) & 3; w->memop = (lo >> 31) & 7;  w->sh = (lo >> 26) & 31;
    w->sfmt = (lo >> 24) & 3;  w->pred = (lo >> 22) & 3;   w->imm = (uint16_t)lo;
}

/* ---- RAM, word addressed (byte address 4 w, little endian) */
static inline uint32_t ram_rd(const cpu_t *c, uint32_t w)
{
    uint32_t a = (w & AMASK) << 2;
    if (a + 4 > c->ram_size) return 0;
    const uint8_t *p = c->sram + a;
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}
static inline void ram_wr(cpu_t *c, uint32_t w, uint32_t v)
{
    uint32_t a = (w & AMASK) << 2;
    if (a + 4 > c->ram_size) return;
    uint8_t *p = c->sram + a;
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* ---- one clock of the datapath.  run: the word executes (else a DMA wait or idle: only the
 * pipelines move).  mdr is loaded from what the last word's read left (pend), unless stalled. */
#define SX16(x) ((int32_t)(int16_t)(x))

static inline void pipes(core_t *k, const uw_t *w, int run, int stall)
{
    /* the accumulator takes the product of the word before (mop_p), and a bus add lands */
    if (k->mop_p) {
        switch (k->mop_p) {
        case 1: k->acc = k->mp; break;
        case 2: k->acc += k->mp; break;
        case 3: k->acc -= k->mp; break;
        default: k->acc = 0;
        }
        k->acc = (int64_t)((uint64_t)k->acc << 24) >> 24;
    }
    if (!stall) k->mdr = k->pend;
    if (k->bus_we) M.bus[k->bus_wa] = k->bus_wd;
    if (!run) {
        if (k->badd) { k->bus_we = 1; k->bus_wa = k->bwa; k->bus_wd = k->bq + k->accp_d; } else k->bus_we = 0;
        k->badd = 0; k->mop_p = 0;
        k->pend = M.nb[w->imm & 63];    /* mop_d is 0: the default, the node buffer */
    }
}

/* the word executes */
static inline void step(core_t *k, const uw_t *w, unsigned *upc, int *busy)
{
    const uint32_t A = M.rf[w->ra], B0 = M.rf[w->rb];
    const unsigned aop = w->aop, sh = w->sh, imm = w->imm, memop = w->memop;
    uint32_t Bv;
    switch (w->bsel) {
    case 0: Bv = B0; break;
    case 1: Bv = (uint32_t)SX16(imm); break;
    case 2: Bv = imm << 16; break;
    default: Bv = k->mdr;
    }
    /* the ALU */
    const int sub = aop == 2 || aop == 10 || aop == 11;
    uint64_t as = 0;
    int lt = 0;
    if (aop <= 2 || aop == 10 || aop == 11 || w->setf) {     /* (the carry out is kept by any flag-setting word) */
        as = (uint64_t)A + (uint32_t)(Bv ^ (sub ? ~0u : 0)) + (unsigned)sub;
        lt = ((A ^ Bv) >> 31) ? (int)(A >> 31) : (int)((as >> 31) & 1);
    }
    uint32_t res;
    switch (aop) {
    case 1: case 2: res = (uint32_t)as; break;
    case 10: res = lt ? A : Bv; break;
    case 11: res = lt ? Bv : A; break;
    case 3: res = A & Bv; break;
    case 4: res = A | Bv; break;
    case 5: res = A ^ Bv; break;
    case 6: res = A >> sh; break;
    case 7: res = (uint32_t)((int32_t)A >> sh); break;
    case 12: {
        int64_t a = k->acc;
        switch (sh & 3) {
        case 0: res = (uint32_t)a; break;
        case 1: res = (uint32_t)(a >> 8); break;
        case 2: res = (uint32_t)(a >> 15); break;
        default: res = (uint32_t)(a >> 16);
        }
        break; }
    case 13: res = M.tick; break;
    case 14: res = (A << 1) | (((A >> 22) ^ (A >> 17)) & 1); break;
    case 15: {                         /* the wave unit */
        uint32_t v;
        switch (sh & 3) {
        case 0: v = ((~A >> 31) << 15) | ((A >> 16) & 0x7FFF); break;
        case 1: v = ((((A >> 16) & 0x7FFF) ^ ((((A >> 31) ^ ((sh >> 2) & (B0 >> 31))) & 1) ? 0x7FFFu : 0)) << 1) ^ 0x8000; break;
        case 2: v = (A >> 20) < (B0 & 0xFFF) ? 0x7FFF : 0x8001; break;
        default: v = (((A >> 22) & 1) << 15) | (((A >> 20) & 1) << 14) | (((A >> 16) & 1) << 13) | (((A >> 13) & 1) << 12) |
                     (((A >> 11) & 1) << 11) | (((A >> 7) & 1) << 10) | (((A >> 4) & 1) << 9) | (((A >> 2) & 1) << 8);
        }
        res = (uint32_t)SX16(v);
        break; }
    default: res = Bv;                 /* 8, 9, and 0 */
    }
    const int sys = aop == 13;
    const int pass = w->pred == 1 ? k->fc : w->pred == 2 ? !k->fc : w->pred == 3 ? (int)(k->fres >> 31) : 1;
    /* the multiplier */
    int64_t prod = 0;
    if (w->mop) {
        int32_t ma, mb;
        switch (w->masel) {
        case 0: ma = SX16(A); break;
        case 1: ma = (int32_t)A >> 16; break;
        case 2: ma = SX16(k->mdr); break;
        default: ma = (int32_t)k->mdr >> 16;
        }
        switch (w->mbsel) {
        case 0: mb = SX16(B0); break;
        case 1: mb = (int32_t)B0 >> 16; break;
        case 2: mb = SX16(imm); break;
        default: mb = (int32_t)(B0 & 0xFFFF);
        }
        prod = (int64_t)ma * mb;
    }
    /* the reads this word issues (their data is registered: MDR takes it a word later) */
    const unsigned cnt = k->cnt;
    uint32_t pend;
    switch (memop) {
    case 3: {
        uint32_t fidx = A >> 16, q, v;
        unsigned wa = w->sfmt == 0 || w->sfmt == 3 ? fidx >> 2 : w->sfmt == 1 ? fidx >> 1 : fidx;
        q = M.sb[(imm + wa) & 511];
        switch (w->sfmt) {
        case 0: v = ((q >> (8 * (fidx & 3))) & 0xFF) << 8; pend = v | v << 16; break;
        case 3: v = (((q >> (8 * (fidx & 3))) & 0xFF) ^ 0x80) << 8; pend = v | v << 16; break;
        case 1: v = (fidx & 1) ? q >> 16 : q & 0xFFFF; pend = v | v << 16; break;
        default: pend = q;
        }
        break; }
    case 4: pend = (uint32_t)SX16(M.tb[A >> 24]); break;
    case 6: pend = M.bus[((imm & 3) << 5) | cnt]; break;
    default: pend = M.nb[imm & 63];
    }
    /* the next word: flags steer it */
    unsigned pc = (*upc + 1) & 511;
    const int fz = k->fres == 0, fn = (int)(k->fres >> 31);
    switch (w->bc) {
    case 1: pc = w->nxt; break;
    case 2: if (fz) pc = w->nxt; break;
    case 3: if (!fz) pc = w->nxt; break;
    case 4: if (fn) pc = w->nxt; break;
    case 5: if (!fn) pc = w->nxt; break;
    case 6: if (cnt != 31) pc = w->nxt; break;
    }
    /* bus adds: the entry is read now (before this clock's write), written two clocks on */
    uint32_t bq = 0, accp = 0;
    unsigned bwa = 0;
    if (memop == 5) {
        bwa = (((imm ^ k->route) & 3) << 5) | cnt;
        bq = M.bus[bwa];
        accp = (uint32_t)(k->acc >> ((sh & 3) == 0 ? 0 : (sh & 3) == 1 ? 8 : (sh & 3) == 2 ? 15 : 16));
    }
    /* the clock edge */
    pipes(k, w, 1, 0);                    /* (moves acc, loads MDR, lands the bus write registered before) */
    if (k->badd) { k->bus_we = 1; k->bus_wa = k->bwa; k->bus_wd = k->bq + k->accp_d; }
    else if (memop == 7) { k->bus_we = 1; k->bus_wa = (uint8_t)(((imm & 3) << 5) | cnt); k->bus_wd = A; }
    else k->bus_we = 0;
    k->badd = memop == 5;
    if (memop == 5) { k->bwa = (uint8_t)bwa; k->accp_d = accp; k->bq = bq; }
    k->mp = prod;
    k->mop_p = w->mop;
    k->pend = pend;
    if (aop && (!sys || (sh & 3) == 1) && pass) M.rf[w->rd] = res;
    if (w->setf) { k->fres = res; k->fc = (uint8_t)((as >> 32) & 1); }
    if (memop == 2) M.nb[imm & 63] = A;
    if (sys && (sh & 3) == 2) M.irq |= (uint8_t)(1u << (imm & 7));
    if (sys && (sh & 3) == 3) k->route = (uint8_t)((imm ^ A) & 3);
    if (w->bc == 6) k->cnt = cnt == 31 ? 0 : (uint8_t)(cnt + 1);
    if (w->bc == 7) *busy = 0; else *upc = pc;
}

/* a DMA word (SYS 0): a burst of imm[4:0] words from RAM word RF[ra] into the sample buffer at RF[rb] */
static void dma(cpu_t *c, const uw_t *w)
{
    uint32_t a = M.rf[w->ra] & 0xFFFFFF, d = M.rf[w->rb] & 511;
    unsigned n = w->imm & 31;
    for (unsigned i = 0; i < n; i++) M.sb[(d + i) & 511] = ram_rd(c, a + i);
    M.c_bursts++; M.c_words += (uint16_t)n;
}

/* the head's routine, from its entry to its end word; then the pipelines settle */
static uint64_t routine(cpu_t *c, unsigned entry)
{
    unsigned upc = entry & 511;
    int busy = 1;
    uint64_t n = 0;
    core_t k = M.k;
    k.cnt = 0; k.route = 0;
    k.mdr = k.pend = M.nb[1];          /* (the walker holds the node buffer's word 1 on the port) */
    while (busy && n < DMA_CAP) {
        const uw_t *w = &M.cs[upc];
        if (w->aop == 13 && (w->sh & 3) == 0) {
            dma(c, w);
            pipes(&k, w, 0, 1); pipes(&k, w, 0, 1); pipes(&k, w, 0, 1);     /* the waits */
            n += 9 + (w->imm & 31);       /* (clocks: a burst's latency and its words) */
        }
        step(&k, w, &upc, &busy);
        n++;
    }
    const uw_t *z = &M.cs[0];
    pipes(&k, z, 0, 0); pipes(&k, z, 0, 0); pipes(&k, z, 0, 0);
    M.k = k;
    return n;
}

static inline int16_t sat16(int32_t x, unsigned s)
{
    x >>= s;
    return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
}

/* one block: the list walker (mc_engine.v) */
static void block(cpu_t *c)
{
    uint32_t ptr = M.head & AMASK, nptr[4];
    unsigned nord = 0, nodes = 0;
    uint64_t clk = 0;
    M.c_bursts = M.c_words = 0;
    {
        uint32_t t = (uint32_t)M.tacc + M.tempo;
        M.tacc = (uint16_t)t; M.tick = (uint8_t)(t >> 16);
    }
    for (;;) {
        unsigned gs = 0, gn;
        for (;;) {                     /* fetch the group: nodes while linked, up to 4 */
            nptr[gs] = ptr;
            for (unsigned i = 0; i < 16; i++) M.nb[gs * 16 + i] = ram_rd(c, ptr + i);
            ptr = M.nb[gs * 16] & AMASK;
            nodes++; M.c_bursts++; M.c_words += 16;
            clk += 16;                  /* (clocks, for CLOCKS: fitted to the RTL testbench) */
            if ((M.nb[gs * 16] >> 31) && gs != 3) gs++; else break;
        }
        gn = gs;
        if (!(M.nb[1] >> 31)) clk += routine(c, M.nb[1] & 511);
        for (gs = 0; gs <= gn; gs++) { /* the state, words 12-15; word 13 is the meter */
            for (unsigned i = 12; i < 16; i++) ram_wr(c, nptr[gs] + i, M.nb[gs * 16 + i]);
            M.mon[256 + ((nord + gs) & 31)] = M.nb[gs * 16 + 13];
            M.c_bursts++; M.c_words += 4;
            clk += 15;
        }
        if (!ptr) break;
        nord = (nord + gn + 1) & 31;
    }
    /* the main bus out (saturated, shifted), every bus cleared */
    for (unsigned k = 0; k < BLK; k++) {
        int16_t l = sat16((int32_t)M.bus[k], M.oshift), r = sat16((int32_t)M.bus[32 + k], M.oshift);
        M.mon[M.mring++] = (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)r << 16;
        snd_out(l, r);
    }
    memset(M.bus, 0, sizeof M.bus);
    M.st_blocks++; M.st_clk = (uint16_t)(clk + 130); M.irq |= 0x80;
    M.st_bursts = M.c_bursts; M.st_words = M.c_words; M.st_nodes = (uint16_t)nodes;
}

/* the interrupt line is the engine's pending bits, a level; the system latches it as source 3 */
static inline void raise(cpu_t *c) { if (M.irq) c->dev.irq_pend |= IRQ_AUDIO; }

/* the block-time schedule: block n falls due at the cycle of output frame 32 n */
void mcaud_sync(cpu_t *c)
{
    while (M.run && c->st.cycles >= M.due) {
        if (M.head) { block(c); raise(c); }
        else for (unsigned k = 0; k < BLK; k++) snd_out(0, 0);   /* nothing to walk: silence */
        M.due = due_of(++M.n);
    }
}

uint64_t mcaud_next(void) { return M.run ? M.due : UINT64_MAX; }

/* is the engine's interrupt line high (for the system to hold source 3 up) */
int mcaud_irq(void) { return M.irq != 0; }

void mcaud_write(cpu_t *c, uint32_t a, uint16_t v)
{
    mcaud_sync(c);
    switch ((a & 0x3F) >> 1) {
    case 0: M.head = (M.head & 0xFF0000u) | v; break;
    case 1: M.head = (M.head & 0xFFFFu) | (uint32_t)(v & 0xFF) << 16; break;
    case 2: {
        int was = M.run;
        M.run = v & 1; M.oshift = (v >> 4) & 15;
        if (M.run && !was) {           /* a start: the first block is the next frame boundary */
            uint64_t n = c->st.cycles * RATE / ((uint64_t)dev_cpu_hz * BLK);
            while (n && due_of(n - 1) >= c->st.cycles) n--;
            while (due_of(n) < c->st.cycles) n++;
            M.n = n; M.due = due_of(n);
        }
        break; }
    case 3: M.tempo = v; break;
    case 4: M.cs_a = v & 511; break;
    case 5: case 6: case 7: case 8: M.cs_d[((a & 0x3F) >> 1) - 5] = v; break;
    case 9:
        M.cs_d4 = (uint8_t)v;
        cs_decode(M.cs_a, (uint64_t)M.cs_d[0] | (uint64_t)M.cs_d[1] << 16 | (uint64_t)M.cs_d[2] << 32 | (uint64_t)M.cs_d[3] << 48, M.cs_d4);
        M.cs_a = (M.cs_a + 1) & 511;
        break;
    case 10: M.irq &= (uint8_t)~v; break;
    case 18: M.mon_idx = v & 511; break;
    }
}

uint16_t mcaud_read(cpu_t *c, uint32_t a)
{
    mcaud_sync(c);
    switch ((a & 0x3F) >> 1) {
    case 10: return M.irq;
    case 11: return 0xA0C2;
    case 12: return M.st_blocks;
    case 13: return 0;                 /* underruns: none, the ring is never starved */
    case 14: return M.st_clk;
    case 15: return M.st_bursts;
    case 16: return M.st_words;
    case 17: return M.st_nodes;
    case 18: return M.mring;
    case 19: return (uint16_t)M.mon[M.mon_idx];
    case 20: return (uint16_t)(M.mon[M.mon_idx] >> 16);
    }
    return 0;
}
