/* fpga_cpu simulator: CPU state, memory map, timing model, statistics. */
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>

#include "dev.h"
#include "gen/isa_gen.h"

/* ---- memory map (24-bit) ---- */
#define SRAM_SIZE 0x100000u /* 000000-0FFFFF  1 MB, 32-bit wide */
#define XRAM_MAX  14u       /* simulator-only expansion RAM from 100000, in MB */
#define IO_BASE   0xF00000u /* F00000-F0FFFF  I/O */
#define IO_SIZE   0x10000u
#define ROM_BASE  0xFF0000u /* FF0000-FFFFFF  boot ROM (block RAM) */
#define ROM_SIZE  0x10000u

typedef struct {
    uint16_t op, form;
    uint8_t len, mode; /* len in bytes */
    uint8_t rd, rs, rt, rx, pd, ps, pt, pn, cc, sr, scale, ext;
    int32_t imm; /* immediate, displacement or PC offset */
    uint32_t abs;
} dinst_t;

/* Timing model configuration.  CPU and SRAM share one 50 MHz clock. */
typedef struct {
    uint32_t icache_bytes; /* 0 = no I-cache: every fetch goes to SRAM */
    uint32_t line_bytes;
    int prefetch;          /* one-line stream buffer, fills in idle port cycles */
    int fuse;              /* bit mask of FU_* kinds to apply (candidates are always counted) */
    int branch_pen;        /* cycles lost on a taken branch or jump */
    int loaduse_pen;       /* cycles lost when an instruction uses the previous load */
    int io_pen;            /* extra cycles per I/O access */
    uint32_t vperiod, vburst; /* video steals vburst port cycles every vperiod */
    int dual;              /* in-order dual issue: DUAL_OFF, DUAL_BANKED or DUAL_PORTS */
    int ext;               /* extension X instructions allowed (isa.yaml: DSTEP, MULPU, FSHR/FSHL, CLZ) */
    uint32_t xram_mb;      /* simulator-only RAM after the SRAM (100000-), same timing; 0 = none */
    unsigned dual_ops;     /* slot-2 instruction kinds, 1 << PS_*; 0 = alu, movi and branch */
    int dual_16;           /* slot 2 takes only 16-bit encodings */
    int dual_any;          /* the slot-2 instruction may be the older one of the pair */
} tcfg_t;

/* Macro-op fusion kinds.  ALUBR is a flag-setting ALU op (not CMP) followed
 * by a conditional branch: DEC+BNE as a loop counter, i.e. DBNZ by fusion. */
enum { FU_CMPBR, FU_ADDADC, FU_LEAMEM, FU_LDPAIR, FU_ALUBR, FU__COUNT };
#define FU_ALL ((1 << FU__COUNT) - 1)
extern const char *const fuse_names[FU__COUNT];
extern const char *const fuse_keys[FU__COUNT]; /* short names for --fuse LIST */

/* Dual issue.  Slot 2 is a small pipe: simple ALU ops (no shifts, no
 * multiply, no pointer ops), MOVI, or a branch.  BANKED gives the register
 * file one write port per even/odd bank, so the two instructions must write
 * opposite banks; PORTS models a full second write port. */
enum { DUAL_OFF, DUAL_BANKED, DUAL_PORTS };

/* Why an adjacent sequential pair did not dual-issue (first reason found). */
enum { PB_WINDOW, PB_SLOT1, PB_SLOT2, PB_RAW, PB_WAW, PB_BANK, PB_FLAGS, PB__COUNT };
extern const char *const pair_block_names[PB__COUNT];
/* What slot 2 held when a pair issued. */
enum { PS_ALU, PS_MOVI, PS_BRANCH, PS_SHIFT, PS_MUL, PS_PTR, PS__COUNT };
extern const char *const pair_slot_names[PS__COUNT];

typedef struct {
    uint64_t insts, cycles, len16, len32;
    uint64_t op[OP__COUNT];
    uint64_t *form;
    uint64_t ic_access, ic_hit, ic_miss, sb_hit, sb_partial;
    uint64_t st_ifetch, st_video, st_branch, st_loaduse, st_io, st_pair;
    uint64_t data_words, branches, taken;
    uint64_t fuse_cand[FU__COUNT], fused;
    uint64_t paired, pair_slot[PS__COUNT], pair_block[PB__COUNT];
    uint64_t fetch_bytes;   /* instruction bytes consumed by the core (demand) */
    uint64_t port_ifetch;   /* SRAM port words used by I-cache fills and the stream buffer */
} stats_t;

typedef struct cpu {
    uint16_t r[16];
    int n, z, cf, v;
    uint32_t pc, npc;
    int taken;
    int halted, exit_code;
    char halt_reason[96];
    const variant_t *var;
    tcfg_t cfg;
    stats_t st;
    uint8_t *sram, *rom;
    uint32_t ram_size;     /* SRAM plus expansion RAM, contiguous from 0 */
    iodev_t dev;
    int trace;
    uint32_t *prof;        /* --profile: [pc/2] -> {instructions, cycles}, or NULL */
    FILE *log;             /* --log: every instruction and data access, or NULL */
    int logging;           /* set while an instruction executes, so only its accesses are logged */
    /* per-instruction scratch */
    int dwords, io_acc, pair_split;
    uint32_t ea;
    /* timing state */
    uint32_t *ic_tag;
    uint8_t *ic_valid;
    uint32_t ic_lines;
    uint32_t sb_line;
    int sb_valid, sb_left;
    uint16_t prev_load_regs;
    dinst_t prev;
    uint32_t prev_pc, prev_ea;
    int prev_valid, prev_grouped; /* prev issued together with the one before it */
    int prev_cand;                /* prev completed a counted fusion candidate */
} cpu_t;

/* ---- helpers used by the generated semantics (exec_gen.c) ---- */
static inline uint32_t pair24(cpu_t *c, int n) { return c->r[2 * n] | ((uint32_t)(c->r[2 * n + 1] & 0xFF) << 16); }
static inline uint32_t pair32(cpu_t *c, int n) { return c->r[2 * n] | ((uint32_t)c->r[2 * n + 1] << 16); }
static inline void wpair24(cpu_t *c, int n, uint32_t v) { c->r[2 * n] = (uint16_t)v; c->r[2 * n + 1] = (v >> 16) & 0xFF; }
static inline void wpair32(cpu_t *c, int n, uint32_t v) { c->r[2 * n] = (uint16_t)v; c->r[2 * n + 1] = (uint16_t)(v >> 16); }

static inline uint16_t setnz(cpu_t *c, uint32_t r)
{
    r &= 0xFFFF;
    c->n = r >> 15;
    c->z = r == 0;
    return (uint16_t)r;
}

static inline uint16_t add16(cpu_t *c, uint32_t a, uint32_t b, int ci)
{
    a &= 0xFFFF;
    b &= 0xFFFF;
    uint32_t r = a + b + (ci ? 1 : 0);
    c->cf = (r >> 16) & 1;
    c->v = ((~(a ^ b) & (a ^ r)) >> 15) & 1;
    return setnz(c, r);
}

static inline uint16_t sub16(cpu_t *c, uint32_t a, uint32_t b, int ci, int chain)
{
    int z0 = c->z;
    uint16_t r = add16(c, a, ~b & 0xFFFF, ci);
    if (chain)
        c->z = z0 && c->z;
    return r;
}

static inline uint16_t logic16(cpu_t *c, uint32_t r)
{
    c->v = 0;
    return setnz(c, r);
}

static inline uint16_t shl16(cpu_t *c, uint32_t a, int n)
{
    a &= 0xFFFF;
    if (n)
        c->cf = (a >> (16 - n)) & 1;
    c->v = 0;
    return setnz(c, a << n);
}

static inline uint16_t shr16(cpu_t *c, uint32_t a, int n)
{
    a &= 0xFFFF;
    if (n)
        c->cf = (a >> (n - 1)) & 1;
    c->v = 0;
    return setnz(c, a >> n);
}

static inline uint16_t sar16(cpu_t *c, uint32_t a, int n)
{
    int32_t s = (int16_t)a;
    if (n)
        c->cf = (s >> (n - 1)) & 1;
    c->v = 0;
    return setnz(c, (uint32_t)(s >> n));
}

int cond(cpu_t *c, int cc);
uint32_t ea(cpu_t *c, const dinst_t *d, int size);
uint32_t lea32(cpu_t *c, const dinst_t *d);
uint16_t mem_rd8(cpu_t *c, uint32_t a);
uint16_t mem_rd16(cpu_t *c, uint32_t a);
uint32_t mem_rd32(cpu_t *c, uint32_t a);
void mem_wr8(cpu_t *c, uint32_t a, uint32_t v);
void mem_wr16(cpu_t *c, uint32_t a, uint32_t v);
void mem_wr32(cpu_t *c, uint32_t a, uint32_t v);
void cpu_trap(cpu_t *c, int code);
void cpu_sync(cpu_t *c);
uint16_t sr_read(cpu_t *c, int n);
void sr_write(cpu_t *c, int n, uint16_t v);

#define R(n)        (c->r[(n) & 15])
#define W(n, v)     (c->r[(n) & 15] = (uint16_t)(v))
#define P(n)        pair24(c, (n))
#define P32(n)      pair32(c, (n))
#define WP(n, v)    wpair24(c, (n), (v))
#define WP32(n, v)  wpair32(c, (n), (v))
#define CF          (c->cf)
#define ADD16(a, b, ci)     add16(c, (a), (b), (ci))
#define SUB16(a, b, ci, ch) sub16(c, (a), (b), (ci), (ch))
#define LOGIC(v)    logic16(c, (v))
#define SHL16(a, n) shl16(c, (a), (n))
#define SHR16(a, n) shr16(c, (a), (n))
#define SAR16(a, n) sar16(c, (a), (n))
#define SEXT16(v)   ((uint32_t)(int32_t)(int16_t)(v))
#define EA(sz)      ea(c, d, (sz))
#define LEA32()     lea32(c, d)
#define LDU8(a)     mem_rd8(c, (a))
#define LDS8(a)     ((uint16_t)(int16_t)(int8_t)mem_rd8(c, (a)))
#define LD16(a)     mem_rd16(c, (a))
#define LD32(a)     mem_rd32(c, (a))
#define ST8(a, v)   mem_wr8(c, (a), (v))
#define ST16(a, v)  mem_wr16(c, (a), (v))
#define ST32(a, v)  mem_wr32(c, (a), (v))
#define COND(x)     cond(c, (x))
#define PC          (c->pc)
#define NPC         ((c->pc + d->len) & 0xFFFFFF)
#define JUMP(t)     do { c->npc = (t) & 0xFFFFFE; c->taken = 1; } while (0)
#define TRAP(n)     cpu_trap(c, (n))
#define SYNC()      cpu_sync(c)
#define SRR(n)      sr_read(c, (n))
#define SRW(n, v)   sr_write(c, (n), (v))

/* ---- API ---- */
void exec_inst(cpu_t *c, const dinst_t *d);
int decode_is_long(const variant_t *v, uint16_t p0);
int decode(const variant_t *v, uint16_t p0, uint16_t p1, dinst_t *d);
void disasm(const dinst_t *d, uint32_t pc, char *buf, size_t n);
void cpu_init(cpu_t *c, const variant_t *v, const tcfg_t *cfg);
void cpu_run(cpu_t *c, uint64_t max_cycles);
int cpu_step(cpu_t *c);          /* one instruction; returns 0 if the CPU is halted */
void cpu_reset(cpu_t *c);        /* registers, timing state, statistics and devices; keeps memory */
void cpu_mem_written(cpu_t *c, uint32_t a, uint32_t n); /* memory changed behind the CPU's back */
uint16_t cpu_peek16(const cpu_t *c, uint32_t a);        /* side-effect-free read (0 for I/O) */
void cpu_free(cpu_t *c);

#endif
