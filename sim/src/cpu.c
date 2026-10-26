/* fpga_cpu simulator core: decode, memory, timing model, run loop. */
#include "cpu.h"

#include <stdlib.h>
#include <string.h>

const char *const fuse_names[FU__COUNT] = { "cmp+branch", "add+adc pair", "lea+mem", "ldw/stw pair", "alu+branch" };
const char *const fuse_keys[FU__COUNT] = { "cmpbr", "addadc", "leamem", "ldpair", "alubr" };
const char *const pair_block_names[PB__COUNT] = { "window", "slot1", "slot2", "raw", "waw", "bank", "flags" };
const char *const pair_slot_names[PS__COUNT] = { "alu", "movi", "branch" };

/* ---- decoder (table-driven, tables from gen/isa_gen.c) ---- */

static void set_opnd(dinst_t *d, int o, int32_t v)
{
    switch (o) {
    case O_RD: d->rd = (uint8_t)v; break;
    case O_RS: d->rs = (uint8_t)v; break;
    case O_RT: d->rt = (uint8_t)v; break;
    case O_RX: d->rx = (uint8_t)v; break;
    case O_PD: d->pd = (uint8_t)v; break;
    case O_PS: d->ps = (uint8_t)v; break;
    case O_PT: d->pt = (uint8_t)v; break;
    case O_PN: d->pn = (uint8_t)v; break;
    case O_CC: d->cc = (uint8_t)v; break;
    case O_SR: d->sr = (uint8_t)v; break;
    case O_SCALE: d->scale = (uint8_t)v; break;
    case O_EXT: d->ext = (uint8_t)v; break;
    case O_IMM: case O_DISP: case O_TARGET: d->imm = v; break;
    case O_ABS: d->abs = (uint32_t)v; break;
    }
}

int decode(const variant_t *v, uint16_t p0, uint16_t p1, dinst_t *d)
{
    int lng = 0;
    for (int i = 0; i < v->nlong; i++)
        if ((p0 & v->lmask[i]) == v->lval[i])
            lng = 1;
    uint32_t w = lng ? ((uint32_t)p0 << 16) | p1 : p0;
    int lo = lng ? v->n16 : 0, hi = lng ? v->nforms : v->n16;
    for (int i = lo; i < hi; i++) {
        const form_t *f = &v->forms[i];
        if ((w & f->mask) != f->value)
            continue;
        memset(d, 0, sizeof *d);
        d->op = f->op;
        d->form = (uint16_t)i;
        d->len = f->len / 8;
        d->mode = f->mode;
        for (int k = 0; k < f->nf; k++) {
            const fld_t *fl = &f->f[k];
            uint32_t raw = 0;
            for (int r = 0; r < fl->nruns; r++)
                raw = (raw << fl->wd[r]) | ((w >> fl->sh[r]) & ((1u << fl->wd[r]) - 1));
            int32_t val = (int32_t)raw;
            if (fl->sign == 1 && (raw >> (fl->width - 1)) & 1)
                val -= (int32_t)(1u << fl->width);
            val = val / fl->div * fl->scale;
            set_opnd(d, fl->opnd, val);
            if (fl->opnd2 != 0xFF)
                set_opnd(d, fl->opnd2, val);
        }
        for (int k = 0; k < f->nfix; k++)
            set_opnd(d, f->fixo[k], f->fixv[k]);
        return 1;
    }
    return 0;
}

/* ---- predecode cache (functional speed only, not part of the timing model) ---- */

#define PDC_BITS 18
#define PDC_SIZE (1u << PDC_BITS)
static dinst_t pdc[PDC_SIZE];
static uint32_t pdc_tag[PDC_SIZE];

static void pdc_flush(void) { memset(pdc_tag, 0xFF, sizeof pdc_tag); }

static void pdc_inval(uint32_t a, int n)
{
    for (uint32_t x = a >= 2 ? (a & ~1u) - 2 : 0; x < a + n; x += 2)
        if (pdc_tag[(x >> 1) & (PDC_SIZE - 1)] == x)
            pdc_tag[(x >> 1) & (PDC_SIZE - 1)] = 0xFFFFFFFFu;
}

/* ---- memory ---- */

void cpu_trap(cpu_t *c, int code)
{
    if (c->halted)
        return;
    c->halted = 1;
    c->exit_code = 128 + (code & 0x7F);
    if (code >= 0)
        snprintf(c->halt_reason, sizeof c->halt_reason, "SYS %d at %06x", code, c->pc);
    else if (code == -1)
        snprintf(c->halt_reason, sizeof c->halt_reason, "RETI at %06x (no interrupts yet)", c->pc);
    else if (code == -2)
        snprintf(c->halt_reason, sizeof c->halt_reason, "illegal instruction at %06x", c->pc);
    else
        snprintf(c->halt_reason, sizeof c->halt_reason, "fault %d at %06x", code, c->pc);
}

static void fault(cpu_t *c, const char *what, uint32_t a)
{
    if (c->halted)
        return;
    c->halted = 1;
    c->exit_code = 3;
    snprintf(c->halt_reason, sizeof c->halt_reason, "%s at address %06x (pc %06x)", what, a, c->pc);
}

static uint16_t io_read(cpu_t *c, uint32_t a)
{
    c->io_acc++;
    return dev_read(c, a);
}

static void io_write(cpu_t *c, uint32_t a, uint32_t v)
{
    c->io_acc++;
    dev_write(c, a, v);
}

#define IN_SRAM(a) ((a) < SRAM_SIZE)
#define IN_ROM(a)  ((a) >= ROM_BASE)
#define IN_IO(a)   ((a) >= IO_BASE && (a) < IO_BASE + IO_SIZE)

uint16_t mem_rd8(cpu_t *c, uint32_t a)
{
    if (IN_SRAM(a)) { c->dwords++; return c->sram[a]; }
    if (IN_ROM(a)) return c->rom[a - ROM_BASE];
    if (IN_IO(a)) return io_read(c, a) >> ((a & 1) * 8) & 0xFF;
    fault(c, "read from unmapped", a);
    return 0;
}

uint16_t mem_rd16(cpu_t *c, uint32_t a)
{
    if (a & 1) { fault(c, "unaligned 16-bit read", a); return 0; }
    if (IN_SRAM(a)) { c->dwords++; return c->sram[a] | c->sram[a + 1] << 8; }
    if (IN_ROM(a)) return c->rom[a - ROM_BASE] | c->rom[a - ROM_BASE + 1] << 8;
    if (IN_IO(a)) return io_read(c, a);
    fault(c, "read from unmapped", a);
    return 0;
}

uint32_t mem_rd32(cpu_t *c, uint32_t a)
{
    if (a & 1) { fault(c, "unaligned 32-bit read", a); return 0; }
    if (IN_SRAM(a) && (a & 2)) {
        c->pair_split = 1; /* spans two 32-bit SRAM words */
        c->dwords++;
    }
    uint32_t lo = mem_rd16(c, a);
    if (IN_SRAM(a))
        c->dwords--; /* one SRAM access delivers both halves */
    return lo | (uint32_t)mem_rd16(c, a + 2) << 16;
}

void mem_wr8(cpu_t *c, uint32_t a, uint32_t v)
{
    if (IN_SRAM(a)) { c->dwords++; c->sram[a] = (uint8_t)v; pdc_inval(a, 1); return; }
    if (IN_IO(a)) { io_write(c, a, v); return; }
    fault(c, IN_ROM(a) ? "write to boot ROM" : "write to unmapped", a);
}

void mem_wr16(cpu_t *c, uint32_t a, uint32_t v)
{
    if (a & 1) { fault(c, "unaligned 16-bit write", a); return; }
    if (IN_SRAM(a)) {
        c->dwords++;
        c->sram[a] = (uint8_t)v;
        c->sram[a + 1] = (uint8_t)(v >> 8);
        pdc_inval(a, 2);
        return;
    }
    if (IN_IO(a)) { io_write(c, a, v); return; }
    fault(c, IN_ROM(a) ? "write to boot ROM" : "write to unmapped", a);
}

void mem_wr32(cpu_t *c, uint32_t a, uint32_t v)
{
    if (a & 1) { fault(c, "unaligned 32-bit write", a); return; }
    if (IN_SRAM(a) && (a & 2)) {
        c->pair_split = 1;
        c->dwords++;
    }
    mem_wr16(c, a, v & 0xFFFF);
    if (IN_SRAM(a))
        c->dwords--;
    mem_wr16(c, a + 2, v >> 16);
}

static uint16_t fetch16(cpu_t *c, uint32_t a)
{
    if (IN_SRAM(a)) return c->sram[a] | c->sram[a + 1] << 8;
    if (IN_ROM(a)) return c->rom[a - ROM_BASE] | c->rom[a - ROM_BASE + 1] << 8;
    fault(c, "instruction fetch from non-code memory", a);
    return 0;
}

/* ---- architectural helpers ---- */

int cond(cpu_t *c, int cc)
{
    switch (cc) {
    case 0: return c->z;
    case 1: return !c->z;
    case 2: return c->cf;
    case 3: return !c->cf;
    case 4: return c->n;
    case 5: return !c->n;
    case 6: return c->v;
    case 7: return !c->v;
    case 8: return c->cf && !c->z;
    case 9: return !c->cf || c->z;
    case 10: return c->n == c->v;
    case 11: return c->n != c->v;
    case 12: return !c->z && c->n == c->v;
    case 13: return c->z || c->n != c->v;
    case 14: return 1;
    }
    cpu_trap(c, -2);
    return 0;
}

uint32_t ea(cpu_t *c, const dinst_t *d, int sz)
{
    uint32_t a = 0;
    switch (d->mode) {
    case M_D: a = pair24(c, d->pn) + (uint32_t)d->imm; break;
    case M_X: {
        uint32_t x = c->r[d->rx];
        if (!d->ext)
            x = (uint32_t)(int32_t)(int16_t)x;
        a = pair24(c, d->pn) + x * d->scale;
        break;
    }
    case M_PI: a = pair24(c, d->pn); wpair32(c, d->pn, pair32(c, d->pn) + sz); break;
    case M_PD: wpair32(c, d->pn, pair32(c, d->pn) - sz); a = pair24(c, d->pn); break;
    case M_A: a = d->abs; break;
    default: cpu_trap(c, -2);
    }
    a &= 0xFFFFFF;
    c->ea = a;
    return a;
}

/* LEA result: the full 32-bit sum, not truncated to an address. */
uint32_t lea32(cpu_t *c, const dinst_t *d)
{
    switch (d->mode) {
    case M_D: return pair32(c, d->pn) + (uint32_t)d->imm;
    case M_X: {
        uint32_t x = c->r[d->rx];
        if (!d->ext)
            x = (uint32_t)(int32_t)(int16_t)x;
        return pair32(c, d->pn) + x * d->scale;
    }
    case M_A: return d->abs;
    }
    cpu_trap(c, -2);
    return 0;
}

uint16_t sr_read(cpu_t *c, int n)
{
    switch (n) {
    case 0: return (uint16_t)(c->n << 3 | c->z << 2 | c->cf << 1 | c->v);
    case 1: return (uint16_t)c->st.cycles;
    case 2: return (uint16_t)(c->st.cycles >> 16);
    case 3: return (uint16_t)c->st.insts;
    }
    return 0;
}

void sr_write(cpu_t *c, int n, uint16_t v)
{
    if (n == 0) {
        c->n = v >> 3 & 1;
        c->z = v >> 2 & 1;
        c->cf = v >> 1 & 1;
        c->v = v & 1;
    }
}

void cpu_sync(cpu_t *c)
{
    if (c->ic_lines)
        memset(c->ic_valid, 0, c->ic_lines);
    c->sb_valid = 0;
    pdc_flush();
}

/* ---- timing model ---- */

/* Finish time of n single-cycle SRAM port transfers starting at t,
 * skipping the cycles taken by video scan-out. */
static uint64_t port_time(const cpu_t *c, uint64_t t, int n)
{
    while (n > 0) {
        if (c->cfg.vperiod && (t % c->cfg.vperiod) < c->cfg.vburst) {
            t += c->cfg.vburst - t % c->cfg.vperiod;
            continue;
        }
        t++;
        n--;
    }
    return t;
}

static int ic_present(const cpu_t *c, uint32_t line)
{
    uint32_t i = line % c->ic_lines;
    return c->ic_valid[i] && c->ic_tag[i] == line;
}

static void timing_fetch(cpu_t *c, uint32_t pc, int len)
{
    if (IN_ROM(pc))
        return; /* boot ROM is block RAM: no SRAM port use */
    stats_t *s = &c->st;
    if (!c->ic_lines) {
        int words = (int)(((pc + len - 1) >> 2) - (pc >> 2) + 1);
        s->port_ifetch += (uint64_t)words;
        uint64_t t = port_time(c, s->cycles, words) - s->cycles;
        uint64_t stall = t > 1 ? t - 1 : 0;
        s->cycles += stall;
        s->st_ifetch += stall;
        return;
    }
    uint32_t lb = c->cfg.line_bytes, words = lb / 4;
    for (uint32_t L = pc / lb; L <= (pc + len - 1) / lb; L++) {
        s->ic_access++;
        if (ic_present(c, L)) {
            s->ic_hit++;
            continue;
        }
        uint64_t stall;
        if (c->cfg.prefetch && c->sb_valid && c->sb_line == L) {
            if (c->sb_left <= 0) {
                s->sb_hit++;
                stall = 1;
            } else {
                s->sb_partial++;
                s->port_ifetch += (uint64_t)c->sb_left;
                stall = port_time(c, s->cycles, c->sb_left) - s->cycles + 1;
            }
        } else {
            s->ic_miss++;
            s->port_ifetch += words;
            stall = port_time(c, s->cycles + 1, (int)words) - s->cycles;
        }
        c->sb_valid = 0;
        s->cycles += stall;
        s->st_ifetch += stall;
        c->ic_valid[L % c->ic_lines] = 1;
        c->ic_tag[L % c->ic_lines] = L;
        if (c->cfg.prefetch && !ic_present(c, L + 1)) {
            c->sb_line = L + 1;
            c->sb_valid = 1;
            c->sb_left = (int)words;
        }
    }
}

static uint16_t pair_bits(int p) { return (uint16_t)(3u << (2 * p)); }

static uint16_t role_regs(const dinst_t *d, uint16_t roles)
{
    uint16_t m = 0;
    if (roles & ROLE_RD) m |= 1u << d->rd;
    if (roles & ROLE_RS) m |= 1u << d->rs;
    if (roles & ROLE_RT) m |= 1u << d->rt;
    if (roles & ROLE_RX) m |= 1u << d->rx;
    if (roles & ROLE_PD) m |= pair_bits(d->pd);
    if (roles & ROLE_PS) m |= pair_bits(d->ps);
    if (roles & ROLE_PT) m |= pair_bits(d->pt);
    if (roles & ROLE_PN) m |= pair_bits(d->pn);
    if (roles & ROLE_LR) m |= pair_bits(6);
    return m;
}

static uint16_t reads_mask(const dinst_t *d)
{
    const opinfo_t *oi = &op_info[d->op];
    uint16_t m = role_regs(d, oi->reads);
    if (oi->flags & F_MEM) {
        if (d->mode != M_A)
            m |= pair_bits(d->pn);
        if (d->mode == M_X)
            m |= 1u << d->rx;
    }
    return m;
}

static int is_cond_branch(const dinst_t *d) { return d->op == OP_B && d->cc != 14; }

/* Flags written / read.  Arithmetic, logic and shifts write flags; ADC,
 * SBC and CPC read C (and SBC/CPC chain Z); a conditional branch reads them. */
static int writes_flags(const dinst_t *d)
{
    switch (d->op) {
    case OP_ADD: case OP_ADC: case OP_SUB: case OP_SBC: case OP_CMP: case OP_CPC: case OP_NEG:
    case OP_ADDI: case OP_CMPI:
    case OP_AND: case OP_OR: case OP_XOR: case OP_NOT: case OP_ANDI: case OP_ORI: case OP_XORI:
    case OP_SHL: case OP_SHR: case OP_SAR: case OP_SHLI: case OP_SHRI: case OP_SARI:
        return 1;
    case OP_MTSR:
        return d->sr == 0;
    }
    return 0;
}

static int reads_flags(const dinst_t *d)
{
    switch (d->op) {
    case OP_ADC: case OP_SBC: case OP_CPC: return 1;
    case OP_B: return d->cc != 14;
    case OP_MFSR: return d->sr == 0;
    }
    return 0;
}

static uint16_t writes_mask(const dinst_t *d)
{
    const opinfo_t *oi = &op_info[d->op];
    uint16_t m = role_regs(d, oi->writes);
    if ((oi->flags & F_MEM) && (d->mode == M_PI || d->mode == M_PD))
        m |= pair_bits(d->pn);
    return m;
}

/* Bytes of the 64-bit fetch window left after the instruction at pc. */
static int window_fits(const cpu_t *c, const dinst_t *d, uint32_t pc)
{
    const dinst_t *p = &c->prev;
    if (((c->prev_pc + p->len) & 0xFFFFFF) != pc)
        return 0; /* not sequential */
    int avail = (c->prev_pc & 2) ? 6 : 8;
    return p->len + d->len <= avail;
}

static int check_fusion(cpu_t *c, const dinst_t *d, uint32_t pc)
{
    if (!c->prev_valid || c->prev_grouped || !window_fits(c, d, pc))
        return -1;
    const dinst_t *p = &c->prev;
    if ((p->op == OP_CMP || p->op == OP_CMPI || p->op == OP_CPC) && is_cond_branch(d))
        return FU_CMPBR;
    if (((p->op == OP_ADD && d->op == OP_ADC) || (p->op == OP_SUB && d->op == OP_SBC)) &&
        !(p->rd & 1) && !(p->rs & 1) && !(p->rt & 1) &&
        d->rd == p->rd + 1 && d->rs == p->rs + 1 && d->rt == p->rt + 1)
        return FU_ADDADC;
    if (p->op == OP_LEA && (op_info[d->op].flags & F_MEM) && d->op != OP_LEA &&
        (d->mode == M_D || d->mode == M_X) && d->pn == p->pd)
        return FU_LEAMEM;
    if ((p->op == OP_LDW || p->op == OP_STW) && d->op == p->op && p->mode == M_D && d->mode == M_D &&
        d->pn == p->pn && d->imm == p->imm + 2 && (c->prev_ea & 3) == 0 &&
        !(p->op == OP_LDW && (p->rd >> 1) == p->pn))
        return FU_LDPAIR;
    /* A flag-setting ALU op whose result the branch tests: DEC r; BNE loop.
     * Same datapath as CMP+Bcc, plus the register write. */
    if (is_cond_branch(d) && writes_flags(p) && p->op != OP_MTSR &&
        op_info[p->op].cls != CLS_CMP && op_info[p->op].cls != CLS_CMPI)
        return FU_ALUBR;
    return -1;
}

/* Slot-2 pipe: a 16-bit adder and logic unit, no shifter, multiplier or
 * pointer adder, no memory port.  Returns the PS_* class or -1. */
static int slot2_class(const dinst_t *d)
{
    switch (d->op) {
    case OP_ADD: case OP_ADC: case OP_SUB: case OP_SBC: case OP_AND: case OP_OR: case OP_XOR:
    case OP_MOV: case OP_NEG: case OP_NOT: case OP_CMP: case OP_CPC:
    case OP_ADDI: case OP_ANDI: case OP_ORI: case OP_XORI: case OP_CMPI:
        return PS_ALU;
    case OP_MOVI:
        return PS_MOVI;
    case OP_B:
        return PS_BRANCH;
    }
    return -1;
}

static int bank_set(uint16_t regs) { return ((regs & 0x5555) ? 1 : 0) | ((regs & 0xAAAA) ? 2 : 0); }

/* Can d issue in slot 2 beside the previous instruction?  Returns the PS_*
 * class, or -1 after counting why not.  In-order: slot 1 is the older one. */
static int check_pair(cpu_t *c, const dinst_t *d, uint32_t pc)
{
    if (!c->prev_valid || c->prev_grouped)
        return -1;
    stats_t *s = &c->st;
    const dinst_t *p = &c->prev;
    if (!window_fits(c, d, pc)) {
        if (((c->prev_pc + p->len) & 0xFFFFFF) == pc)
            s->pair_block[PB_WINDOW]++;
        return -1;
    }
    const opinfo_t *po = &op_info[p->op];
    if ((po->flags & (F_BRANCH | F_JUMP)) || po->cls == CLS_SYS || po->cls == CLS_NONE || p->op == OP_MTSR) {
        s->pair_block[PB_SLOT1]++;
        return -1;
    }
    int k = slot2_class(d);
    if (k < 0) {
        s->pair_block[PB_SLOT2]++;
        return -1;
    }
    uint16_t pw = writes_mask(p), dw = writes_mask(d);
    if (reads_mask(d) & pw) {
        s->pair_block[PB_RAW]++;
        return -1;
    }
    if (dw & pw) {
        s->pair_block[PB_WAW]++;
        return -1;
    }
    if (c->cfg.dual == DUAL_BANKED && (bank_set(pw) & bank_set(dw))) {
        s->pair_block[PB_BANK]++;
        return -1;
    }
    if (reads_flags(d) && writes_flags(p)) {
        s->pair_block[PB_FLAGS]++;
        return -1;
    }
    return k;
}

static void timing_exec(cpu_t *c, const dinst_t *d, uint32_t pc)
{
    stats_t *s = &c->st;
    uint64_t cyc = 1;
    int fk = check_fusion(c, d, pc), grouped = 0;
    /* Candidates are counted without overlap (as if every kind were fused),
     * so the counts do not depend on which kinds --fuse applies. */
    int counted = fk >= 0 && !c->prev_cand;
    if (counted)
        s->fuse_cand[fk]++;
    if (fk >= 0) {
        if (c->cfg.fuse & (1 << fk)) {
            s->fused++;
            grouped = 1;
        }
    }
    if (!grouped && c->cfg.dual) {
        int k = check_pair(c, d, pc);
        if (k >= 0) {
            s->paired++;
            s->pair_slot[k]++;
            grouped = 1;
        }
    }
    if (grouped)
        cyc = 0; /* issues together with the previous instruction */
    s->fetch_bytes += d->len;
    if (c->prev_load_regs & reads_mask(d)) {
        cyc += c->cfg.loaduse_pen;
        s->st_loaduse += c->cfg.loaduse_pen;
    }
    if (c->dwords) {
        uint64_t t = port_time(c, s->cycles, c->dwords) - s->cycles;
        uint64_t wait = t - (uint64_t)c->dwords;
        s->st_video += wait;
        cyc += wait;
        if (c->pair_split) {
            cyc += 1;
            s->st_pair++;
        }
        s->data_words += c->dwords;
    }
    if (c->io_acc) {
        cyc += (uint64_t)c->cfg.io_pen * c->io_acc;
        s->st_io += (uint64_t)c->cfg.io_pen * c->io_acc;
    }
    if (c->taken) {
        cyc += c->cfg.branch_pen;
        s->st_branch += c->cfg.branch_pen;
    }
    if (c->cfg.prefetch && c->sb_valid && c->sb_left > 0) {
        int idle = (int)cyc - c->dwords;
        if (idle > c->sb_left)
            idle = c->sb_left;
        if (idle > 0) {
            c->sb_left -= idle;
            s->port_ifetch += (uint64_t)idle;
        }
    }
    s->cycles += cyc;
    uint16_t lr = (op_info[d->op].flags & F_LOAD) ? role_regs(d, op_info[d->op].writes) : 0;
    /* A group's results all arrive together: a load in slot 1 still delays
     * the next instruction that uses it. */
    c->prev_load_regs = grouped ? (uint16_t)(c->prev_load_regs | lr) : lr;
    c->prev = *d;
    c->prev_pc = pc;
    c->prev_ea = c->ea;
    c->prev_valid = 1;
    c->prev_grouped = grouped;
    c->prev_cand = counted;
}

/* ---- run loop ---- */

void cpu_init(cpu_t *c, const variant_t *v, const tcfg_t *cfg)
{
    memset(c, 0, sizeof *c);
    c->var = v;
    c->cfg = *cfg;
    c->sram = calloc(SRAM_SIZE + 4, 1);
    c->rom = calloc(ROM_SIZE + 4, 1);
    c->st.form = calloc((size_t)v->nforms, sizeof(uint64_t));
    dev_init(&c->dev);
    if (cfg->icache_bytes) {
        c->ic_lines = cfg->icache_bytes / cfg->line_bytes;
        c->ic_tag = calloc(c->ic_lines, sizeof(uint32_t));
        c->ic_valid = calloc(c->ic_lines, 1);
    }
    pdc_flush();
}

void cpu_free(cpu_t *c)
{
    dev_free(&c->dev);
    free(c->sram);
    free(c->rom);
    free(c->st.form);
    free(c->ic_tag);
    free(c->ic_valid);
}

static const dinst_t *predecode(cpu_t *c, uint32_t pc)
{
    uint32_t i = (pc >> 1) & (PDC_SIZE - 1);
    if (pdc_tag[i] == pc)
        return &pdc[i];
    uint16_t p0 = fetch16(c, pc), p1 = fetch16(c, (pc + 2) & 0xFFFFFF);
    if (c->halted)
        return NULL;
    if (!decode(c->var, p0, p1, &pdc[i])) {
        cpu_trap(c, -2);
        return NULL;
    }
    pdc_tag[i] = pc;
    return &pdc[i];
}

int cpu_step(cpu_t *c)
{
    char buf[96];
    if (c->halted)
        return 0;
    uint32_t pc = c->pc;
    if (pc & 1) {
        fault(c, "odd PC", pc);
        return 0;
    }
    const dinst_t *d = predecode(c, pc);
    if (!d)
        return 0;
    timing_fetch(c, pc, d->len);
    c->npc = (pc + d->len) & 0xFFFFFF;
    c->taken = 0;
    c->dwords = c->io_acc = c->pair_split = 0;
    if (c->trace) {
        disasm(d, pc, buf, sizeof buf);
        fprintf(stderr, "%06x  %-30s  ", pc, buf);
    }
    exec_inst(c, d);
    if (c->trace) {
        for (int r = 0; r < 16; r++)
            fprintf(stderr, "%04x ", c->r[r]);
        fprintf(stderr, "%c%c%c%c\n", c->n ? 'N' : '-', c->z ? 'Z' : '-', c->cf ? 'C' : '-', c->v ? 'V' : '-');
    }
    stats_t *s = &c->st;
    s->insts++;
    s->op[d->op]++;
    s->form[d->form]++;
    if (d->len == 2) s->len16++; else s->len32++;
    if (op_info[d->op].flags & F_BRANCH) {
        s->branches++;
        if (c->taken) s->taken++;
    }
    timing_exec(c, d, pc);
    c->pc = c->npc;
    return !c->halted;
}

void cpu_run(cpu_t *c, uint64_t max_cycles)
{
    while (!c->halted) {
        if (max_cycles && c->st.cycles >= max_cycles) {
            c->halted = 1;
            c->exit_code = 2;
            snprintf(c->halt_reason, sizeof c->halt_reason, "cycle limit at %06x", c->pc);
            break;
        }
        cpu_step(c);
    }
}

void cpu_reset(cpu_t *c)
{
    memset(c->r, 0, sizeof c->r);
    c->n = c->z = c->cf = c->v = 0;
    c->pc = c->npc = 0;
    c->taken = c->halted = c->exit_code = 0;
    c->halt_reason[0] = 0;
    uint64_t *form = c->st.form;
    memset(form, 0, sizeof(uint64_t) * (size_t)c->var->nforms);
    memset(&c->st, 0, sizeof c->st);
    c->st.form = form;
    if (c->ic_lines)
        memset(c->ic_valid, 0, c->ic_lines);
    c->sb_valid = c->sb_left = 0;
    c->sb_line = 0;
    c->prev_load_regs = 0;
    c->prev_valid = c->prev_grouped = c->prev_cand = 0;
    pdc_flush();
    dev_reset(&c->dev);
}

void cpu_mem_written(cpu_t *c, uint32_t a, uint32_t n)
{
    (void)c;
    if (n)
        pdc_inval(a, (int)n);
}

uint16_t cpu_peek16(const cpu_t *c, uint32_t a)
{
    a &= 0xFFFFFE;
    if (IN_SRAM(a)) return c->sram[a] | c->sram[a + 1] << 8;
    if (IN_ROM(a)) return c->rom[a - ROM_BASE] | c->rom[a - ROM_BASE + 1] << 8;
    return 0;
}

/* ---- disassembler ---- */

static void fmt_mem(const dinst_t *d, char *b, size_t n)
{
    switch (d->mode) {
    case M_D:
        if (d->imm) snprintf(b, n, "[p%d %c %d]", d->pn, d->imm < 0 ? '-' : '+', abs(d->imm));
        else snprintf(b, n, "[p%d]", d->pn);
        break;
    case M_X: snprintf(b, n, "[p%d + r%d%s*%d]", d->pn, d->rx, d->ext ? ".z" : "", d->scale); break;
    case M_PI: snprintf(b, n, "[p%d]+", d->pn); break;
    case M_PD: snprintf(b, n, "-[p%d]", d->pn); break;
    case M_A: snprintf(b, n, "[0x%06x]", d->abs); break;
    default: snprintf(b, n, "?"); break;
    }
}

void disasm(const dinst_t *d, uint32_t pc, char *b, size_t n)
{
    const opinfo_t *oi = &op_info[d->op];
    char m[40];
    uint32_t tgt = (pc + (uint32_t)d->imm) & 0xFFFFFF;
    switch (oi->cls) {
    case CLS_BIN: snprintf(b, n, "%s r%d, r%d, r%d", oi->name, d->rd, d->rs, d->rt); break;
    case CLS_UN: snprintf(b, n, "%s r%d, r%d", oi->name, d->rd, d->rs); break;
    case CLS_CMP: snprintf(b, n, "%s r%d, r%d", oi->name, d->rs, d->rt); break;
    case CLS_BINI: snprintf(b, n, "%s r%d, r%d, %d", oi->name, d->rd, d->rs, d->imm); break;
    case CLS_MOVI: snprintf(b, n, "%s r%d, %d", oi->name, d->rd, d->imm); break;
    case CLS_CMPI: snprintf(b, n, "%s r%d, %d", oi->name, d->rs, d->imm); break;
    case CLS_LD: case CLS_ST: fmt_mem(d, m, sizeof m); snprintf(b, n, "%s r%d, %s", oi->name, d->rd, m); break;
    case CLS_LDP: case CLS_STP: case CLS_LEA:
        fmt_mem(d, m, sizeof m);
        snprintf(b, n, "%s p%d, %s", oi->name, d->pd, m);
        break;
    case CLS_LEAPC: snprintf(b, n, "%s p%d, 0x%06x", oi->name, d->pd, tgt); break;
    case CLS_MOVP: snprintf(b, n, "%s p%d, p%d", oi->name, d->pd, d->ps); break;
    case CLS_ADDP: snprintf(b, n, "%s p%d, p%d, r%d", oi->name, d->pd, d->ps, d->rx); break;
    case CLS_ADDPP: snprintf(b, n, "%s p%d, p%d, p%d", oi->name, d->pd, d->ps, d->pt); break;
    case CLS_ADDPI: snprintf(b, n, "%s p%d, p%d, %d", oi->name, d->pd, d->ps, d->imm); break;
    case CLS_BR:
        snprintf(b, n, "B%s 0x%06x", d->cc < NUM_CONDITIONS ? cond_names[d->cc] : "??", tgt);
        break;
    case CLS_JAL: snprintf(b, n, "%s 0x%06x", oi->name, tgt); break;
    case CLS_JR: case CLS_JALR: snprintf(b, n, "%s p%d", oi->name, d->pn); break;
    case CLS_SYS: snprintf(b, n, "%s %d", oi->name, d->imm); break;
    case CLS_MFSR: snprintf(b, n, "%s r%d, sr%d", oi->name, d->rd, d->sr); break;
    case CLS_MTSR: snprintf(b, n, "%s sr%d, r%d", oi->name, d->sr, d->rs); break;
    default: snprintf(b, n, "%s", oi->name); break;
    }
}
