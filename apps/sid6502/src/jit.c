/* sid6502 dynamic recompiler: 6502 -> fpga_cpu native code.  See ../README.md for the design. */
#include <string.h>
#ifdef JIT_DEBUG
#include <stdio.h>
#endif
#include "sid6502.h"
#include "optable.h"

#define W16(lo, hi) ((uint16_t)((uint16_t)(hi) << 8 | (uint16_t)(lo)))

/* ------------------------------------------------------------------ native registers */
#define T0   0
#define T1   1
#define PMEM 1          /* pair p1: mem6502 + 0x8000 */
#define PATT 2          /* pair p2: attr6502 + 0x8000 */
#define RA   6
#define RX   7
#define RY   8
#define RS   9
#define RRES 10
#define RC   11
#define RV   12
#define RCYC 13

/* ALU function numbers */
#define F_ADD 0
#define F_ADC 1
#define F_SUB 2
#define F_SBC 3
#define F_AND 4
#define F_OR  5
#define F_XOR 6
#define F_MOV 7
#define F_CMP 8

/* condition codes */
#define CC_EQ 0
#define CC_NE 1
#define CC_CS 2
#define CC_CC 3
#define CC_AL 14

/* flag bits as the JIT tracks them */
#define NF 1
#define ZF 2
#define CF 4
#define VF 8
#define ALLF 15
#define NZF  3

/* ------------------------------------------------------------------ configuration */
#ifndef CODE_WORDS
#define CODE_WORDS 44000u       /* translation cache: 88 KB */
#endif
#ifndef MAX_EV
#define MAX_EV 2400
#endif
#ifndef HSIZE
#define HSIZE 2048
#endif
#define MAX_INS 32
#define MAX_STUBS 80
#define MAX_RANGES 256

/* ------------------------------------------------------------------ runtime state shared with jit_rt.s */
struct jst_s {
    uint32_t entry, memb, attrb;
    uint16_t a, x, y, s, res, c, v, cyc, t0, t1;
};
struct jst_s jst;
void jit_enter(void);
void jit_exit(void);
void jit_sync(void);

uint8_t jit_flags;
jit_stats_t jit_stats;
int (*jit_step_hook)(void);

static uint16_t cbuf[CODE_WORDS];
static uint16_t *cp;
static uint16_t *cend;

/* events: how translated code asks the dispatcher for something */
#define EV_CHAIN  1     /* leave to pc (static target); patchable */
#define EV_DYNPC  2     /* leave to pc in T0 */
#define EV_ASSIST 3     /* interpret one instruction */
#define EV_IOW    4     /* plain store to SID registers: log it, resume */
#define EV_BUDGET 5     /* cycle budget used up at trace entry */
#define EVF_RESUME 1    /* assist may resume natively */

typedef struct {
    uint8_t kind, di, flags, pad;
    uint16_t pc, pc2;
    int16_t kd;         /* cycles from the charged trace end back to the start of this instruction */
    uint16_t cyc;       /* instruction's base cycles (assist) / write offset (IOW) */
    uint32_t resume;    /* native resume address */
    uint32_t patch;     /* branch to patch (CHAIN) */
    uint8_t patch_is_bcc;
    uint8_t pad2;
} ev_t;
#ifdef JIT_DATA_ADDR
/* ev, hash, wsite and volb at a fixed address (SDRAM), leaving block RAM to the code cache */
#define JIT_HASH_ADDR  (JIT_DATA_ADDR + ((uint32_t)MAX_EV * sizeof(ev_t) + 3u) / 4u * 4u)
#define JIT_BITS_ADDR  (JIT_HASH_ADDR + (uint32_t)HSIZE * sizeof(hent_t))
static ev_t *const ev = (ev_t *)JIT_DATA_ADDR;
#else
static ev_t ev[MAX_EV];
#endif
static uint16_t ev_n;

typedef struct { uint16_t pc; uint8_t di, used; uint32_t addr; } hent_t;
#ifdef JIT_DATA_ADDR
static hent_t *const hash = (hent_t *)JIT_HASH_ADDR;
#else
static hent_t hash[HSIZE];
#endif
static uint16_t hash_n;

static uint16_t cranges[MAX_RANGES][2];
static uint16_t cr_n;
static uint8_t cr_over;

#ifdef JIT_DATA_ADDR
static uint8_t *const wsite = (uint8_t *)JIT_BITS_ADDR;
static uint8_t *const volb = (uint8_t *)(JIT_BITS_ADDR + 8192);
#else
static uint8_t wsite[8192], volb[8192];
#endif
#define BIT_GET(a, i) ((a)[(i) >> 3] >> ((i) & 7) & 1)
#define BIT_SET(a, i) ((a)[(i) >> 3] |= 1 << ((i) & 7))

static uint16_t flush_gen;      /* counts cache flushes: a link must not be made across one */
static uint8_t dirty;           /* code written since the last SYNC */
static uint8_t flushed;         /* the cache was flushed during an assist */

/* ------------------------------------------------------------------ emitter */
static struct {
    uint8_t nzr;        /* register holding the N/Z source */
    uint8_t resp;       /* if nzr is RES: it is a plain value (not the N=Z=1 special form) */
    uint8_t cst;        /* C: 0 register is valid, 1 known 0, 2 known 1 (register may be stale) */
    int8_t flr;         /* register whose value the FCPU Z flag reflects, -1 none, FLR_CMP: CMP result */
} S;
#define FLR_CMP 99

static inline void E(uint16_t w) { *cp++ = w; }

static void alu3(int fn, int d, int s, int t)
{
    if (fn != F_MOV)
        S.flr = -1;
    if (d == s && fn != F_MOV && fn != F_CMP) {
        E(0x0000 | d << 8 | t << 4 | fn);
    } else if (d == t && s != t && (fn == F_ADD || fn == F_AND || fn == F_OR || fn == F_XOR)) {
        E(0x0000 | d << 8 | s << 4 | fn);
    } else if (fn == F_MOV) {
        E(0x0000 | d << 8 | s << 4 | F_MOV);
    } else {
        E(0xF800 | d << 4 | s);
        E(t << 12 | fn << 8);
    }
}

static void e_mov(int d, int s) { if (d != s) E(0x0000 | d << 8 | s << 4 | F_MOV); }

static void e_cmp(int s, int t)
{
    S.flr = -1;
    E(0x0000 | s << 8 | t << 4 | F_CMP);
}

/* ALUI: f 0 ADDI 1 ANDI 2 ORI 3 XORI */
static void alui(int f, int d, int s, uint16_t imm)
{
    S.flr = -1;
    if (f == 0 && d == s && (int16_t)imm >= -128 && (int16_t)imm <= 127) {
        E(0x1000 | d << 8 | (imm & 0xFF));
        return;
    }
    E(0xE800 | f << 8 | d << 4 | s);
    E(imm);
}

static void e_movi(int d, uint16_t imm)
{
    if ((int16_t)imm >= -128 && (int16_t)imm <= 127) {
        E(0x2000 | d << 8 | (imm & 0xFF));
        return;
    }
    E(0xED00 | d << 4);
    E(imm);
}

static void e_movi32(int d, uint16_t imm)
{
    E(0xED00 | d << 4);
    E(imm);
}

static void e_cmpi(int s, uint16_t imm)
{
    S.flr = -1;
    if ((int16_t)imm >= -128 && (int16_t)imm <= 127) {
        E(0x3000 | s << 8 | (imm & 0xFF));
        return;
    }
    E(0xEC00 | s);
    E(imm);
}

static void e_addi(int d, int s, int16_t imm)
{
    if (d == s && imm == 0)
        return;
    alui(0, d, s, (uint16_t)imm);
}
static void e_andi(int d, int s, uint16_t imm) { alui(1, d, s, imm); }
static void e_ori(int d, int s, uint16_t imm) { alui(2, d, s, imm); }
static void e_xori(int d, int s, uint16_t imm) { alui(3, d, s, imm); }

static void e_shli(int d, int s, int n)
{
    S.flr = -1;
    if (d == s) { E(0xA700 | d << 4 | n); return; }
    E(0xF900 | d << 4 | s);
    E(n);
}

static void e_shri(int d, int s, int n)
{
    S.flr = -1;
    if (d == s) { E(0xA800 | d << 4 | n); return; }
    E(0xF900 | d << 4 | s);
    E(0x0100 | n);
}

/* byte load/store: [pair + disp] */
static void ld_d(int d, int p, int16_t disp) { E(0xE000 | d << 4 | p << 1); E((uint16_t)disp); }
static void st_d(int d, int p, int16_t disp) { E(0xE400 | d << 4 | p << 1); E((uint16_t)disp); }
/* [pair + reg] sign-extended index, scale 1 */
static void ld_x(int d, int p, int x) { E(0xE000 | d << 4 | p << 1 | 1); E(x << 12); }
static void st_x(int d, int p, int x) { E(0xE400 | d << 4 | p << 1 | 1); E(x << 12); }

/* 32-bit branch; returns the address of the instruction */
static uint16_t *e_br(int cc, uint32_t target)
{
    uint16_t *site = cp;
    int32_t off = (int32_t)(target - (uint32_t)(uintptr_t)site) >> 1;
    E(0xF000 | cc << 7 | ((off >> 16) & 0x7F));
    E((uint16_t)off);
    return site;
}

static void patch_br(uint16_t *site, uint32_t target)
{
    int32_t off = (int32_t)(target - (uint32_t)(uintptr_t)site) >> 1;
    site[0] = (site[0] & 0xFF80) | ((off >> 16) & 0x7F);
    site[1] = (uint16_t)off;
}

static void make_br_al(uint16_t *site, uint32_t target)
{
    int32_t off = (int32_t)(target - (uint32_t)(uintptr_t)site) >> 1;
    site[0] = 0xF000 | CC_AL << 7 | ((off >> 16) & 0x7F);
    site[1] = (uint16_t)off;
}

#define ADDR(p) ((uint32_t)(uintptr_t)(p))

/* ------------------------------------------------------------------ translation state */
typedef struct {
    uint16_t pc, tgt, kb;
    uint8_t opc, mn, mode, len, b1, b2, cyc;
    uint8_t kind;       /* K_* */
    uint8_t fold;       /* 0, 1 = branch never taken, 2 = always taken */
    uint8_t cst;        /* C static state before: 0 unknown, 1 known 0, 2 known 1 */
    uint8_t di;         /* D bit 0, I bit 1, before */
    uint8_t live;       /* flags live after (NF|ZF|CF|VF) */
    uint8_t iow;        /* plain store to SID: log it */
} ins_t;

#define K_NATIVE 0
#define K_RESUME 1      /* interpret this instruction, then continue natively */
#define K_END    2      /* interpret this instruction, then dispatch */

#define END_FALL 0      /* leave to pc2 */
#define END_JUMP 1      /* leave to tgt */
#define END_BR   2      /* conditional branch: taken -> tgt, not taken -> pc2 */
#define END_DYN  3      /* RTS: pc from the stack */
#define END_ASSIST 4    /* last instruction is an assist (K_END) */

static ins_t tr[MAX_INS + 2];
static int ntr;
static uint16_t tr_pc0;
static uint8_t tr_di0;
static uint8_t tr_end;
static uint16_t tr_pc2, tr_tgt;
static uint16_t K_total;

static uint8_t mapflags(uint8_t m)
{
    return (m & 0x80 ? NF : 0) | (m & 0x02 ? ZF : 0) | (m & 0x01 ? CF : 0) | (m & 0x40 ? VF : 0);
}

static int is_branch(uint8_t mn)
{
    return mn == N_BPL || mn == N_BMI || mn == N_BVC || mn == N_BVS || mn == N_BCC || mn == N_BCS || mn == N_BNE || mn == N_BEQ;
}

static int is_vol(uint16_t pc, uint8_t len)
{
    uint8_t i;
    if ((pc >> 8) == 1 || (pc >= 0xD000 && pc < 0xE000))
        return 1;
    for (i = 0; i < len; i++) {
        uint16_t a = pc + i;
        if (BIT_GET(volb, a))
            return 1;
        if (BIT_GET(wsite, a)) {
            BIT_SET(volb, a);
            return 1;
        }
    }
    return 0;
}

static void note_range(uint16_t pc, uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
        attr6502[(uint16_t)(pc + i)] |= ATTR_CODE;
    if (cr_n < MAX_RANGES) {
        cranges[cr_n][0] = pc;
        cranges[cr_n][1] = len;
        cr_n++;
    } else {
        cr_over = 1;
    }
}

/* flags possibly read before being rewritten, starting at pc (a conservative scan) */
static uint8_t live_at(uint16_t pc)
{
    uint8_t live = 0, known = 0;
    uint8_t n;
    for (n = 0; n < 12; n++) {
        uint8_t opc, mn, u, df, len;
        if (pc == STOP_PC)
            break;
        opc = mem6502[pc];
        mn = op_mn[opc];
        len = op_len[opc];
        if (mn == N_ILL || is_vol(pc, len))
            break;
        u = mapflags(op_fr[opc]);
        df = mapflags(op_fw[opc]);
        live |= u & ~known;
        known |= df;
        if ((known & ALLF) == ALLF)
            return live & ALLF;
        if (!(attr6502[pc] & ATTR_CODE))
            note_range(pc, len);
        if (mn == N_JMP && op_mode[opc] == AM_ABS) {
            pc = W16(mem6502[(uint16_t)(pc + 1)], mem6502[(uint16_t)(pc + 2)]);
            continue;
        }
        if (is_branch(mn) || mn == N_JSR || mn == N_RTS || mn == N_RTI || mn == N_BRK || mn == N_JMP || mn == N_PLP
            || mn == N_PHP)
            break;
        pc += len;
    }
    return (live | (ALLF & ~known)) & ALLF;
}

static int visited(uint16_t pc)
{
    int i;
    for (i = 0; i < ntr; i++)
        if (tr[i].pc == pc)
            return 1;
    return 0;
}

static int is_store_mn(uint8_t mn) { return mn == N_STA || mn == N_STX || mn == N_STY; }
static int is_rmw_mn(uint8_t mn)
{
    return mn == N_INC || mn == N_DEC || ((mn == N_ASL || mn == N_LSR || mn == N_ROL || mn == N_ROR));
}

/* does a load/store/RMW with this decoded operand reach the I/O area? (static addresses only) */
static int touches_io(const ins_t *in)
{
    uint16_t base = W16(in->b1, in->b2);
    switch (in->mode) {
    case AM_ABS: return base >= 0xD000 && base < 0xE000;
    case AM_ABX: case AM_ABY: return base >= 0xCF01 && base < 0xE000;
    default: return 0;
    }
}

/* ------------------------------------------------------------------ pass 1: decode the trace */
static void build_trace(uint16_t pc, uint8_t di)
{
    uint8_t cst = 0;
    uint16_t k = 0;
    ntr = 0;
    tr_pc0 = pc;
    tr_di0 = di;
    tr_end = END_FALL;
    for (;;) {
        ins_t *in = &tr[ntr];
        uint8_t opc, mn, mode;
        if (ntr >= MAX_INS || pc == STOP_PC) {
            tr_end = END_FALL;
            tr_pc2 = pc;
            break;
        }
        opc = mem6502[pc];
        mn = op_mn[opc];
        mode = op_mode[opc];
        in->pc = pc;
        in->opc = opc;
        in->mn = mn;
        in->mode = mode;
        in->len = op_len[opc];
        in->b1 = mem6502[(uint16_t)(pc + 1)];
        in->b2 = mem6502[(uint16_t)(pc + 2)];
        in->cyc = op_cyc[opc];
        in->cst = cst;
        in->di = di;
        in->kind = K_NATIVE;
        in->fold = 0;
        in->iow = 0;
        in->kb = k;
        in->tgt = 0;
        if (mn == N_ILL) {
            in->len = 1;
            in->kind = K_END;
            ntr++;
            tr_end = END_ASSIST;
            break;
        }
        if (is_vol(pc, in->len)) {
            /* the interpreter reads these bytes fresh each time, and the dispatcher continues from where
             * it ended up, so nothing about this instruction (length, flow) is assumed */
            in->kind = K_END;
        } else {
            /* static store sites: remember them so that code translated later at the same address is interpreted */
            if ((is_store_mn(mn) || is_rmw_mn(mn)) && (mode == AM_ZP || mode == AM_ABS))
                BIT_SET(wsite, mode == AM_ZP ? in->b1 : W16(in->b1, in->b2));
            if (mn == N_PLP || mn == N_RTI || mn == N_BRK || (mn == N_JMP && mode == AM_IND))
                in->kind = K_END;
            else if (mn == N_PHP || (mn == N_NOP && opc != 0xEA))
                in->kind = K_RESUME;
        }
        /* mnemonics only the interpreter implements */
        switch (mn) {
        case N_ALR: case N_ANC: case N_ARR: case N_DCP: case N_ISC: case N_LAX: case N_RLA: case N_RRA:
        case N_SAX: case N_SBX: case N_SLO: case N_SRE:
            if (in->kind == K_NATIVE) in->kind = K_RESUME;
            break;
        default: break;
        }
        if (in->kind == K_NATIVE) {
            if ((mn == N_ADC || mn == N_SBC) && (di & 1))
                in->kind = K_RESUME;
            else if (touches_io(in)) {
                if (is_store_mn(mn) && mode == AM_ABS && W16(in->b1, in->b2) >= 0xD400 && W16(in->b1, in->b2) < 0xD800)
                    in->iow = 1;
                else if (mode == AM_ABS || !is_store_mn(mn)) {
                    /* loads and read-modify-write of I/O: let the interpreter do them (it knows the hooks) */
                    in->kind = K_RESUME;
                }
            }
        }
        /* cycles and the C flag's static state */
        k += in->cyc;
        if (in->kind == K_NATIVE) {
            if (mn == N_CLC) cst = 1;
            else if (mn == N_SEC) cst = 2;
            else if (op_fw[opc] & 0x01) cst = 0;
        } else if (op_fw[opc] & 0x01 || in->kind == K_END) {
            cst = 0;
        }
        if (mn == N_SED) di |= 1;
        else if (mn == N_CLD) di &= ~1;
        else if (mn == N_SEI) di |= 2;
        else if (mn == N_CLI) di &= ~2;
        if (in->kind == K_END) {
            ntr++;
            tr_end = END_ASSIST;
            tr_pc2 = (uint16_t)(pc + in->len);
            break;
        }
        ntr++;
        /* control flow */
        if (in->kind == K_NATIVE || in->kind == K_RESUME) {
            if (is_branch(mn)) {
                uint16_t npc = pc + 2;
                uint16_t t = npc + (int8_t)in->b1;
                uint8_t take;
                in->tgt = t;
                /* branches on a known C fold to a fixed direction */
                if (in->cst && (mn == N_BCC || mn == N_BCS) && in->kind == K_NATIVE) {
                    take = (mn == N_BCS) == (in->cst == 2);
                    in->fold = take ? 2 : 1;
                    if (take) {
                        k += 1 + (((t ^ npc) & 0xFF00) != 0);
                        if (t > pc && t != STOP_PC && !visited(t) && ntr < MAX_INS) {
                            pc = t;
                            continue;
                        }
                        tr_end = END_JUMP;
                        tr_tgt = t;
                        break;
                    }
                    pc = npc;
                    continue;
                }
                if (t > pc && t != pc && ntr < MAX_INS) {
                    pc = npc;           /* forward: stay on the fall-through path */
                    continue;
                }
                /* backward (or at the size limit): both edges leave; assume taken */
                tr_end = END_BR;
                tr_tgt = t;
                tr_pc2 = npc;
                k += 1 + (((t ^ npc) & 0xFF00) != 0);
                break;
            }
            if (mn == N_JMP && in->kind == K_NATIVE) {
                uint16_t t = W16(in->b1, in->b2);
                in->tgt = t;
                if (t != STOP_PC && !visited(t) && ntr < MAX_INS - 1 && t != tr_pc0) {
                    pc = t;
                    continue;
                }
                tr_end = END_JUMP;
                tr_tgt = t;
                break;
            }
            if (mn == N_JSR && in->kind == K_NATIVE) {
                uint16_t t = W16(in->b1, in->b2);
                in->tgt = t;
                if (t != STOP_PC && !visited(t) && ntr < MAX_INS - 1 && t != tr_pc0) {
                    pc = t;
                    continue;
                }
                /* push the return address, then leave to the subroutine */
                tr_end = END_JUMP;
                tr_tgt = t;
                break;
            }
            if (mn == N_RTS && in->kind == K_NATIVE) {
                tr_end = END_DYN;
                break;
            }
        }
        pc += in->len;
    }
    K_total = k;
}

/* ------------------------------------------------------------------ pass 2: marks and flag liveness */
static void analyze(void)
{
    int i;
    uint8_t live;
    for (i = 0; i < ntr; i++)
        if (tr[i].kind != K_END || (tr[i].mn == N_PLP || tr[i].mn == N_RTI || tr[i].mn == N_BRK || tr[i].mn == N_JMP) )
            if (!is_vol(tr[i].pc, tr[i].len) && tr[i].mn != N_ILL)
                note_range(tr[i].pc, tr[i].len);
    /* a store to a byte that is now translated code is a self-modifying write: let the interpreter do it */
    for (i = 0; i < ntr; i++) {
        ins_t *in = &tr[i];
        if (in->kind == K_NATIVE && (is_store_mn(in->mn) || is_rmw_mn(in->mn)) && (in->mode == AM_ZP || in->mode == AM_ABS)) {
            uint16_t a = in->mode == AM_ZP ? in->b1 : W16(in->b1, in->b2);
            if (attr6502[a] & ATTR_CODE) {
                in->kind = K_RESUME;
                in->iow = 0;
            }
        }
    }
    switch (tr_end) {
    case END_FALL: live = live_at(tr_pc2); break;
    case END_JUMP: live = live_at(tr_tgt); break;
    case END_BR:   live = live_at(tr_tgt) | live_at(tr_pc2); break;
    default:       live = ALLF; break;
    }
    for (i = ntr - 1; i >= 0; i--) {
        ins_t *in = &tr[i];
        uint8_t out = live, u, d;
        if (is_branch(in->mn) && in->kind == K_NATIVE && !in->fold && !(i == ntr - 1 && tr_end == END_BR))
            out |= live_at(in->tgt);
        in->live = out;
        u = mapflags(op_fr[in->opc]);
        d = mapflags(op_fw[in->opc]);
        if (in->kind == K_NATIVE) {
            if (in->cst && (in->mn == N_ADC || in->mn == N_SBC || in->mn == N_ROL || in->mn == N_ROR))
                u &= ~CF;
            if (in->fold)
                u = 0;
        }
        live = (out & ~d) | u;
    }
}

/* ------------------------------------------------------------------ emission state */

typedef struct { uint8_t nzr, resp, cst; } snap_t;
static snap_t snap_get(void) { snap_t s; s.nzr = S.nzr; s.resp = S.resp; s.cst = S.cst; return s; }
static void snap_set(snap_t s) { S.nzr = s.nzr; S.resp = s.resp; S.cst = s.cst; S.flr = -1; }

/* emit the work that makes the flags in `mask` canonical (N/Z in RES, C in the C register) */
static int canon_work(uint8_t mask, int emit)
{
    int work = 0;
    if ((mask & NZF) && (S.nzr != RRES)) {
        work = 1;
        if (emit) e_mov(RRES, S.nzr);
    }
    if ((mask & CF) && S.cst) {
        work = 1;
        if (emit) e_movi(RC, S.cst == 2);
    }
    return work;
}

static void canon(uint8_t mask)
{
    if (canon_work(mask, 1)) {
        if ((mask & NZF) && S.nzr != RRES) { S.nzr = RRES; S.resp = 1; }
        if ((mask & CF) && S.cst) S.cst = 0;
    }
}

static ev_t *new_ev(uint8_t kind, uint8_t di, uint16_t pc, uint16_t pc2, int16_t kd, uint16_t cyc, uint16_t *idx)
{
    ev_t *e = &ev[ev_n];
    memset(e, 0, sizeof *e);
    e->kind = kind;
    e->di = di;
    e->pc = pc;
    e->pc2 = pc2;
    e->kd = kd;
    e->cyc = cyc;
    *idx = ev_n++;
    return e;
}

/* ------------------------------------------------------------------ stubs (cold code emitted after the trace) */
#define ST_CHAIN  1
#define ST_ASSIST 2
#define ST_BUDGET 3

typedef struct {
    uint16_t *bsite;
    uint8_t kind, di, live, flags;
    uint16_t pc, pc2, cyc;
    int16_t adjust, kd;
    snap_t snap;
    uint16_t evx;           /* ST_ASSIST: event already created (resume address is filled in later) */
} stub_t;
static stub_t stubs[MAX_STUBS];
static int nstubs;
static uint16_t *entry_addr;

static hent_t *hash_find(uint16_t pc, uint8_t di)
{
    uint16_t h = (pc * 7u + di * 1031u) & (HSIZE - 1);
    uint16_t n;
    for (n = 0; n < HSIZE; n++, h = (h + 1) & (HSIZE - 1)) {
        if (!hash[h].used)
            return 0;
        if (hash[h].pc == pc && hash[h].di == di)
            return &hash[h];
    }
    return 0;
}

static void hash_add(uint16_t pc, uint8_t di, uint32_t addr)
{
    uint16_t h = (pc * 7u + di * 1031u) & (HSIZE - 1);
    while (hash[h].used)
        h = (h + 1) & (HSIZE - 1);
    hash[h].used = 1;
    hash[h].pc = pc;
    hash[h].di = di;
    hash[h].addr = addr;
    hash_n++;
}

/* A chain stub: [canon work] [cycle adjustment] MOVI T1,ev ; B jit_exit.  Linking replaces the MOVI
 * with a branch to the target, or (when there is no work) retargets the conditional branch itself. */
static void link_ev(ev_t *e, uint32_t target)
{
    if (e->patch_is_bcc) {
        patch_br((uint16_t *)(uintptr_t)e->patch, target);
    } else {
        make_br_al((uint16_t *)(uintptr_t)e->patch, target);
    }
    dirty = 1;
    jit_stats.links++;
}

static void emit_chain_tail(uint8_t di, uint16_t pc, int16_t adjust, uint8_t live, uint16_t *bsite)
{
    uint16_t idx;
    ev_t *e;
    uint16_t *start = cp;
    hent_t *h;
    canon(live);
    if (adjust)
        e_addi(RCYC, RCYC, adjust);
    e = new_ev(EV_CHAIN, di, pc, pc, 0, 0, &idx);
    if (bsite && cp == start) {
        /* no work: the branch itself will be retargeted; keep a normal exit behind it for the first run */
        e->patch = ADDR(bsite);
        e->patch_is_bcc = 1;
    } else {
        e->patch = ADDR(cp);
    }
    e_movi32(T1, idx);
    e_br(CC_AL, ADDR(jit_exit));
    h = hash_find(pc, di);
    if (h)
        link_ev(e, h->addr);
}

/* ------------------------------------------------------------------ pass 3: instruction emission */
typedef struct { uint8_t kind; uint16_t addr; } ea_t;      /* kind 1: static address, 2: index in T0 */

static uint16_t pend[8];
static uint8_t npend;
static uint8_t curdi;

static void set_nz(int r) { S.nzr = r; S.resp = 1; S.flr = -1; }
/* the last emitted instruction set the FCPU flags from r */
static void set_nz_f(int r) { S.nzr = r; S.resp = 1; S.flr = r; }

static void ldb(int dst, ea_t e)
{
    if (e.kind == 1) ld_d(dst, PMEM, (int16_t)(e.addr - 0x8000u));
    else ld_x(dst, PMEM, T0);
}

static void stb(int src, ea_t e)
{
    if (e.kind == 1) st_d(src, PMEM, (int16_t)(e.addr - 0x8000u));
    else st_x(src, PMEM, T0);
}

/* effective address; dynamic results are an index in T0 for the sign-extended [PMEM + T0] */
static ea_t emit_ea(const ins_t *in, int pen)
{
    ea_t e;
    uint16_t base = W16(in->b1, in->b2);
    int reg = (in->mode == AM_ZPX || in->mode == AM_ABX || in->mode == AM_IZX) ? RX : RY;
    e.kind = 1;
    e.addr = 0;
    switch (in->mode) {
    case AM_ZP:
        e.addr = in->b1;
        break;
    case AM_ABS:
        e.addr = base;
        break;
    case AM_ZPX: case AM_ZPY:
        e_addi(T0, reg, in->b1);
        e_andi(T0, T0, 0xFF);
        e_xori(T0, T0, 0x8000);
        e.kind = 2;
        break;
    case AM_ABX: case AM_ABY:
        e_addi(T0, reg, (int16_t)(base + 0x8000u));
        if (pen && (base & 0xFF)) {
            e_addi(T1, reg, base & 0xFF);
            e_shri(T1, T1, 8);
            alu3(F_SUB, RCYC, RCYC, T1);
        }
        e.kind = 2;
        break;
    case AM_IZX:
        e_addi(T0, RX, in->b1);
        e_andi(T0, T0, 0xFF);
        e_xori(T1, T0, 0x8000);
        ld_x(T1, PMEM, T1);                 /* lo */
        e_addi(T0, T0, 1);
        e_andi(T0, T0, 0xFF);
        e_xori(T0, T0, 0x8000);
        ld_x(T0, PMEM, T0);                 /* hi */
        e_shli(T0, T0, 8);
        alu3(F_OR, T0, T0, T1);
        e_xori(T0, T0, 0x8000);
        e.kind = 2;
        break;
    case AM_IZY:
        ld_d(T0, PMEM, (int16_t)(in->b1 - 0x8000));
        if (pen) {
            alu3(F_ADD, T1, T0, RY);
            e_shri(T1, T1, 8);
            alu3(F_SUB, RCYC, RCYC, T1);
        }
        ld_d(T1, PMEM, (int16_t)((uint8_t)(in->b1 + 1) - 0x8000));
        e_shli(T1, T1, 8);
        alu3(F_OR, T0, T0, T1);
        alu3(F_ADD, T0, T0, RY);
        e_xori(T0, T0, 0x8000);
        e.kind = 2;
        break;
    default:
        break;
    }
    return e;
}

static void add_stub(uint16_t *bsite, uint8_t kind, uint8_t di, uint16_t pc, uint16_t pc2, uint16_t cyc,
                     int16_t adjust, int16_t kd, uint8_t live, uint16_t evx)
{
    stub_t *st = &stubs[nstubs++];
    st->bsite = bsite;
    st->kind = kind;
    st->di = di;
    st->pc = pc;
    st->pc2 = pc2;
    st->cyc = cyc;
    st->adjust = adjust;
    st->kd = kd;
    st->live = live;
    st->snap = snap_get();
    st->evx = evx;
}

/* branch (condition cc) to a cold stub that has the interpreter run this whole instruction */
static void assist_branch(const ins_t *in, int cc)
{
    uint16_t idx;
    ev_t *e = new_ev(EV_ASSIST, in->di, in->pc, in->pc + in->len, (int16_t)(in->kb - K_total), in->cyc, &idx);
    uint16_t *site;
    e->flags = EVF_RESUME;
    pend[npend++] = idx;
    site = e_br(cc, 0);
    add_stub(site, ST_ASSIST, in->di, in->pc, 0, 0, 0, 0, in->live | mapflags(op_fr[in->opc]), idx);
}

static void emit_event_inline(uint16_t idx)
{
    e_movi32(T1, idx);
    e_br(CC_AL, ADDR(jit_exit));
}

/* plain store to a SID register: log it and carry on */
static void emit_iow(const ins_t *in)
{
    uint16_t idx;
    ev_t *e = new_ev(EV_IOW, in->di, in->pc, in->pc + in->len, (int16_t)(in->kb - K_total), in->cyc - 1, &idx);
    emit_event_inline(idx);
    e->resume = ADDR(cp);
}

static void emit_store(const ins_t *in, int reg)
{
    ea_t e = emit_ea(in, 0);
    if (e.kind == 1) {
        stb(reg, e);
        if (in->iow)
            emit_iow(in);
    } else {
        ld_x(T1, PATT, T0);
        e_cmpi(T1, 0);
        assist_branch(in, CC_NE);
        stb(reg, e);
    }
}

/* interpret this instruction and come back (K_RESUME) or dispatch (K_END) */
static void emit_assist(const ins_t *in)
{
    uint16_t idx;
    ev_t *e;
    uint8_t m = in->live | mapflags(op_fr[in->opc]);
    if (in->kind == K_END)
        m = ALLF;
    canon(m);
    e = new_ev(EV_ASSIST, in->di, in->pc, in->pc + in->len, (int16_t)(in->kb - K_total), in->cyc, &idx);
    e->flags = in->kind == K_RESUME ? EVF_RESUME : 0;
    emit_event_inline(idx);
    e->resume = ADDR(cp);
    if (in->kind == K_RESUME) {
        uint8_t fw = mapflags(op_fw[in->opc]);
        if (fw & NZF) { S.nzr = RRES; S.resp = 0; }
        if (fw & CF) S.cst = 0;
        S.flr = -1;
    }
}

static void push_byte_imm(uint8_t v)
{
    e_addi(T0, RS, 0x8100);
    e_movi(T1, v);
    st_x(T1, PMEM, T0);
    e_addi(RS, RS, -1);
    e_andi(RS, RS, 0xFF);
}

/* condition test for a branch; returns the condition code */
static int emit_cond(const ins_t *in)
{
    switch (in->mn) {
    case N_BEQ: case N_BNE: {
        int eq = in->mn == N_BEQ;
        if (S.flr == FLR_CMP || (S.flr >= 0 && S.flr == S.nzr))
            return eq ? CC_EQ : CC_NE;
        if (S.nzr == RRES && !S.resp) {
            e_addi(T0, RRES, -1);
            e_cmpi(T0, 0xFF);
            return eq ? CC_CS : CC_CC;
        }
        e_cmpi(S.nzr, 0);
        return eq ? CC_EQ : CC_NE;
    }
    case N_BMI: case N_BPL:
        e_andi(T0, S.nzr, 0x80);
        return in->mn == N_BMI ? CC_NE : CC_EQ;
    case N_BCS: case N_BCC:
        if (S.flr == FLR_CMP)
            return in->mn == N_BCS ? CC_CS : CC_CC;
        e_cmpi(RC, 1);
        return in->mn == N_BCS ? CC_CS : CC_CC;
    default:    /* BVS BVC */
        e_andi(T0, RV, 0x80);
        return in->mn == N_BVS ? CC_NE : CC_EQ;
    }
}

static int fusable_cmp(int i)
{
    const ins_t *b;
    if (i + 1 >= ntr)
        return 0;
    b = &tr[i + 1];
    if (b->kind != K_NATIVE || b->fold || !(b->mn == N_BEQ || b->mn == N_BNE || b->mn == N_BCS || b->mn == N_BCC))
        return 0;
    return (b->live & (NF | ZF | CF)) == 0;
}

static void emit_cmp(const ins_t *in, int i, int r)
{
    int fuse = fusable_cmp(i);
    uint8_t L = in->live;
    if (fuse) {
        if (in->mode == AM_IMM) {
            e_cmpi(r, in->b1);
        } else {
            ea_t e = emit_ea(in, 1);
            ldb(T0, e);
            e_cmp(r, T0);
        }
        S.flr = FLR_CMP;
        return;
    }
    if (!(L & (NZF | CF))) {
        /* nothing observes the result; only the page-crossing cycle remains */
        if (in->mode != AM_IMM)
            (void)emit_ea(in, 1);
        return;
    }
    if (in->mode == AM_IMM) {
        e_addi(RRES, r, (int16_t)(-(int)in->b1));
    } else {
        ea_t e = emit_ea(in, 1);
        ldb(T0, e);
        alu3(F_SUB, RRES, r, T0);
    }
    if (L & CF) {
        e_shri(RC, RRES, 15);
        e_xori(RC, RC, 1);
    }
    if (L & NZF) {
        e_andi(RRES, RRES, 0xFF);
        set_nz_f(RRES);
    } else {
        set_nz(RRES);
    }
    S.cst = 0;
}

static void emit_rmw(const ins_t *in)
{
    int acc = in->mode == AM_ACC;
    int v = acc ? RA : RRES;
    int fl = 0;
    uint8_t L = in->live;
    ea_t e;
    e.kind = 0;
    e.addr = 0;
    if (!acc) {
        e = emit_ea(in, 0);
        if (e.kind == 2) {
            ld_x(T1, PATT, T0);
            e_cmpi(T1, 0);
            assist_branch(in, CC_NE);
        }
        ldb(RRES, e);
    }
    switch (in->mn) {
    case N_INC:
        e_addi(v, v, 1);
        e_andi(v, v, 0xFF);
        fl = 1;
        break;
    case N_DEC:
        e_addi(v, v, -1);
        e_andi(v, v, 0xFF);
        fl = 1;
        break;
    case N_ASL:
        e_shli(v, v, 1);
        if (L & CF) e_shri(RC, v, 8);
        e_andi(v, v, 0xFF);
        fl = 1;
        S.cst = 0;
        break;
    case N_LSR:
        if (L & CF) e_andi(RC, v, 1);
        e_shri(v, v, 1);
        S.cst = 0;
        break;
    case N_ROL:
        e_shli(v, v, 1);
        if (S.cst == 0) alu3(F_ADD, v, v, RC);
        else if (S.cst == 2) e_addi(v, v, 1);
        if (L & CF) e_shri(RC, v, 8);
        e_andi(v, v, 0xFF);
        fl = 1;
        S.cst = 0;
        break;
    default:    /* ROR */
        if (L & CF) e_andi(T1, v, 1);
        e_shri(v, v, 1);
        if (S.cst == 0) {
            e_shli(RC, RC, 7);
            alu3(F_OR, v, v, RC);
        } else if (S.cst == 2) {
            e_ori(v, v, 0x80);
        }
        if (L & CF) e_mov(RC, T1);
        S.cst = 0;
        break;
    }
    if (fl) set_nz_f(v); else set_nz(v);
    if (!acc)
        stb(RRES, e);
}

static void emit_bit(const ins_t *in)
{
    uint8_t L = in->live;
    ea_t e = emit_ea(in, 0);
    ldb(T0, e);
    if (L & VF)
        e_shli(RV, T0, 1);
    if ((L & NZF) == NZF) {
        alu3(F_AND, T1, RA, T0);
        e_addi(T1, T1, 255);
        e_shri(T1, T1, 8);
        e_andi(RRES, T0, 0x80);
        alu3(F_OR, RRES, RRES, T1);
        e_xori(T1, T1, 1);
        e_shli(T1, T1, 8);
        alu3(F_OR, RRES, RRES, T1);
        S.nzr = RRES;
        S.resp = 0;
    } else if (L & ZF) {
        alu3(F_AND, RRES, RA, T0);
        set_nz(RRES);
    } else if (L & NF) {
        e_mov(RRES, T0);
        set_nz(RRES);
    }
}

static void emit_adc(const ins_t *in)
{
    int imm = -1;
    uint8_t L = in->live;
    uint8_t cs = S.cst;
    if (in->mode == AM_IMM) {
        imm = in->b1;
    } else {
        ea_t e = emit_ea(in, 1);
        ldb(T0, e);
    }
    if (in->mn == N_SBC) {
        if (imm >= 0) imm ^= 0xFF;
        else e_xori(T0, T0, 0xFF);
    }
    if (imm >= 0) e_addi(T1, RA, (int16_t)(imm + (cs == 2)));
    else alu3(F_ADD, T1, RA, T0);
    if (cs == 0) alu3(F_ADD, T1, T1, RC);
    else if (cs == 2 && imm < 0) e_addi(T1, T1, 1);
    if (L & VF) {
        alu3(F_XOR, RV, RA, T1);
        if (imm >= 0) e_xori(T0, T1, (uint16_t)imm);
        else alu3(F_XOR, T0, T0, T1);
        alu3(F_AND, RV, RV, T0);
    }
    if (L & CF) e_shri(RC, T1, 8);
    e_andi(RA, T1, 0xFF);
    set_nz_f(RA);
    S.cst = 0;
}

static void emit_native(const ins_t *in, int i)
{
    uint8_t mn = in->mn;
    int r;
    switch (mn) {
    case N_LDA: case N_LDX: case N_LDY:
        r = mn == N_LDA ? RA : mn == N_LDX ? RX : RY;
        if (in->mode == AM_IMM) e_movi(r, in->b1);
        else ldb(r, emit_ea(in, 1));
        set_nz(r);
        break;
    case N_STA: emit_store(in, RA); break;
    case N_STX: emit_store(in, RX); break;
    case N_STY: emit_store(in, RY); break;
    case N_ADC: case N_SBC: emit_adc(in); break;
    case N_AND: case N_ORA: case N_EOR:
        if (in->mode == AM_IMM) {
            if (mn == N_AND) e_andi(RA, RA, in->b1);
            else if (mn == N_ORA) e_ori(RA, RA, in->b1);
            else e_xori(RA, RA, in->b1);
        } else {
            ldb(T0, emit_ea(in, 1));
            alu3(mn == N_AND ? F_AND : mn == N_ORA ? F_OR : F_XOR, RA, RA, T0);
        }
        set_nz_f(RA);
        break;
    case N_CMP: emit_cmp(in, i, RA); break;
    case N_CPX: emit_cmp(in, i, RX); break;
    case N_CPY: emit_cmp(in, i, RY); break;
    case N_BIT: emit_bit(in); break;
    case N_ASL: case N_LSR: case N_ROL: case N_ROR: case N_INC: case N_DEC:
        emit_rmw(in);
        break;
    case N_INX: case N_INY: case N_DEX: case N_DEY:
        r = (mn == N_INX || mn == N_DEX) ? RX : RY;
        e_addi(r, r, (mn == N_INX || mn == N_INY) ? 1 : -1);
        e_andi(r, r, 0xFF);
        set_nz_f(r);
        break;
    case N_TAX: e_mov(RX, RA); set_nz(RX); break;
    case N_TAY: e_mov(RY, RA); set_nz(RY); break;
    case N_TXA: e_mov(RA, RX); set_nz(RA); break;
    case N_TYA: e_mov(RA, RY); set_nz(RA); break;
    case N_TSX: e_mov(RX, RS); set_nz(RX); break;
    case N_TXS: e_mov(RS, RX); break;
    case N_PHA:
        e_addi(T0, RS, 0x8100);
        st_x(RA, PMEM, T0);
        e_addi(RS, RS, -1);
        e_andi(RS, RS, 0xFF);
        break;
    case N_PLA:
        e_addi(RS, RS, 1);
        e_andi(RS, RS, 0xFF);
        e_addi(T0, RS, 0x8100);
        ld_x(RA, PMEM, T0);
        set_nz(RA);
        break;
    case N_CLC: S.cst = 1; break;
    case N_SEC: S.cst = 2; break;
    case N_CLV: if (in->live & VF) e_movi(RV, 0); break;
    case N_SED: curdi |= 1; break;
    case N_CLD: curdi &= ~1; break;
    case N_SEI: curdi |= 2; break;
    case N_CLI: curdi &= ~2; break;
    case N_NOP: break;
    case N_JMP: break;
    case N_JSR: {
        uint16_t ret = in->pc + 2;
        push_byte_imm(ret >> 8);
        push_byte_imm(ret & 0xFF);
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------------------ cold stubs */
static void flush_stubs(void)
{
    int i;
    for (i = 0; i < nstubs; i++) {
        stub_t *st = &stubs[i];
        uint16_t idx;
        ev_t *e;
        patch_br(st->bsite, ADDR(cp));
        snap_set(st->snap);
        switch (st->kind) {
        case ST_CHAIN:
            emit_chain_tail(st->di, st->pc, st->adjust, st->live, st->bsite);
            break;
        case ST_ASSIST:
            canon(st->live);
            emit_event_inline(st->evx);
            break;
        default:        /* ST_BUDGET: give back the charge and ask the dispatcher */
            e_addi(RCYC, RCYC, K_total);
            e = new_ev(EV_BUDGET, st->di, st->pc, st->pc, 0, K_total, &idx);
            (void)e;
            emit_event_inline(idx);
            break;
        }
    }
    nstubs = 0;
}

static void flush_pend(void)
{
    while (npend)
        ev[pend[--npend]].resume = ADDR(cp);
}

/* branch with a chain stub */
static void chain_branch(int cc, uint8_t di, uint16_t pc, int16_t adjust, uint8_t live)
{
    uint16_t *site = e_br(cc, 0);
    add_stub(site, ST_CHAIN, di, pc, 0, 0, adjust, 0, live, 0);
}

static uint16_t extra_taken(uint16_t npc, uint16_t t) { return 1 + (((t ^ npc) & 0xFF00) != 0); }

static void emit_trace(void)
{
    int i;
    int last = ntr - 1;
    curdi = tr_di0;
    for (i = 0; i < ntr; i++) {
        const ins_t *in = &tr[i];
        npend = 0;
        if (in->kind == K_RESUME || in->kind == K_END) {
            emit_assist(in);
            flush_pend();
            continue;
        }
        if (is_branch(in->mn)) {
            uint16_t npc = in->pc + 2;
            if (in->fold)
                continue;
            if (i == last && tr_end == END_BR) {
                int cc = emit_cond(in);
                /* taken: charged as taken, no adjustment */
                chain_branch(cc, curdi, in->tgt, 0, live_at(in->tgt));
                /* not taken: give back the taken penalty */
                emit_chain_tail(curdi, npc, (int16_t)(K_total - (in->kb + 2)), live_at(npc), 0);
            } else {
                int cc = emit_cond(in);
                chain_branch(cc, curdi, in->tgt,
                             (int16_t)(K_total - (in->kb + 2 + extra_taken(npc, in->tgt))), live_at(in->tgt));
            }
            continue;
        }
        emit_native(in, i);
        flush_pend();
    }
    switch (tr_end) {
    case END_FALL:
        emit_chain_tail(curdi, tr_pc2, 0, live_at(tr_pc2), 0);
        break;
    case END_JUMP:
        emit_chain_tail(curdi, tr_tgt, 0, live_at(tr_tgt), 0);
        break;
    case END_BR:
        break;
    case END_DYN: {
        uint16_t idx;
        ev_t *e;
        /* RTS: pull the return address into T0 */
        e_addi(RS, RS, 1);
        e_andi(RS, RS, 0xFF);
        e_addi(T0, RS, 0x8100);
        ld_x(T1, PMEM, T0);
        e_addi(RS, RS, 1);
        e_andi(RS, RS, 0xFF);
        e_addi(T0, RS, 0x8100);
        ld_x(T0, PMEM, T0);
        e_shli(T0, T0, 8);
        alu3(F_OR, T0, T0, T1);
        e_addi(T0, T0, 1);
        canon(ALLF);
        e = new_ev(EV_DYNPC, curdi, 0, 0, 0, 0, &idx);
        (void)e;
        emit_event_inline(idx);
        break;
    }
    default:
        break;
    }
    flush_stubs();
}

/* ------------------------------------------------------------------ translate */
static void flush_cache(void)
{
    uint16_t i;
    if (cr_over) {
        memset(attr6502, 0, 0xD000);
        memset(attr6502 + 0xE000, 0, 0x2000);
        for (i = 0; i < 0x1000; i++)
            attr6502[0xD000 + i] = ATTR_IO;
    } else {
        for (i = 0; i < cr_n; i++) {
            uint16_t j;
            for (j = 0; j < cranges[i][1]; j++)
                attr6502[(uint16_t)(cranges[i][0] + j)] &= ~ATTR_CODE;
        }
    }
    cr_n = 0;
    cr_over = 0;
    memset(hash, 0, (uint16_t)HSIZE * sizeof(hent_t));
    hash_n = 0;
    ev_n = 0;
    cp = cbuf;
    cend = cbuf + CODE_WORDS;
    jit_stats.flushes++;
    flush_gen++;
    dirty = 1;
    flushed = 1;
}

void jit_flush(void)
{
    if (!cp) {
#ifdef JIT_DATA_ADDR
        memset(wsite, 0, 8192);                 /* fixed-address memory is not zeroed by the loader */
        memset(volb, 0, 8192);
#endif
        cp = cbuf;
        cend = cbuf + CODE_WORDS;
        jst.memb = ADDR(mem6502) + 0x8000;
        jst.attrb = ADDR(attr6502) + 0x8000;
    }
    flush_cache();
    flushed = 0;
}

void smc_write(uint16_t a)
{
    BIT_SET(volb, a);
    flush_cache();
}

static uint32_t translate(uint16_t pc, uint8_t di)
{
    uint16_t *start;
    uint16_t *bsite;
    if (cend - cp < 1400 || ev_n + 2 * MAX_INS + 8 > MAX_EV || hash_n > HSIZE * 3 / 4)
        flush_cache();
    start = cp;
    build_trace(pc, di);
    analyze();
    entry_addr = cp;
    hash_add(pc, di, ADDR(cp));
    S.nzr = RRES;
    S.resp = 0;
    S.cst = 0;
    S.flr = -1;
    nstubs = 0;
    npend = 0;
    e_addi(RCYC, RCYC, -(int16_t)K_total);
    bsite = e_br(4 /* MI */, 0);
    add_stub(bsite, ST_BUDGET, tr_di0, tr_pc0, 0, 0, 0, 0, 0, 0);
    emit_trace();
    jit_stats.traces++;
    jit_stats.instrs += ntr;
    jit_stats.bytes += (uint32_t)(cp - start) * 2;
    dirty = 1;
    return ADDR(start);
}

/* ------------------------------------------------------------------ dispatcher */
static uint32_t jbase;          /* absolute cycle at which CYC == jbudget */
static uint16_t jbudget;

#define NOW() (jbase + (uint32_t)jbudget - (uint32_t)(int32_t)(int16_t)jst.cyc)

static void to_jit(void)
{
    jst.a = cpu.a;
    jst.x = cpu.x;
    jst.y = cpu.y;
    jst.s = cpu.s;
    jst.res = cpu.z ? (cpu.n ? 0x180 : 0) : (cpu.n ? 0x80 : 1);
    jst.c = cpu.c;
    jst.v = cpu.v ? 0x80 : 0;
}

static void from_jit(uint8_t di)
{
    cpu.a = jst.a;
    cpu.x = jst.x;
    cpu.y = jst.y;
    cpu.s = jst.s;
    cpu.n = (jst.res & 0x80) != 0;
    cpu.z = (jst.res == 0 || jst.res >= 0x100);
    cpu.c = jst.c & 1;
    cpu.v = (jst.v & 0x80) != 0;
    cpu.d = di & 1;
    cpu.i = di >> 1 & 1;
    cpu.cycles = NOW();
}

static void refill(uint32_t until, uint16_t need)
{
    uint32_t now = NOW();
    uint32_t b = until > now ? until - now : 1;
    if (b > 0x4000)
        b = 0x4000;
    if (b < need)
        b = need;
    jbase = now;
    jbudget = (uint16_t)b;
    jst.cyc = (uint16_t)b;
}

static uint32_t lookup(uint16_t pc, uint8_t di)
{
    hent_t *h = hash_find(pc, di);
    if (h)
        return h->addr;
    return translate(pc, di);
}

int jit_run(uint32_t until, uint16_t stop_pc)
{
    uint16_t pc = cpu.pc;
    uint8_t di = cpu.d | cpu.i << 1;
    uint16_t need = 0;
    int st;
    if (!cp)
        jit_flush();
    jst.memb = ADDR(mem6502) + 0x8000;
    jst.attrb = ADDR(attr6502) + 0x8000;
    to_jit();
    jbase = cpu.cycles;
    jbudget = 0;
    jst.cyc = 0;
    for (;;) {
        uint32_t entry;
        /* ---- dispatch at (pc, di) */
        if (pc == stop_pc) {
            from_jit(di);
            cpu.pc = pc;
            return RUN_STOP;
        }
        if (NOW() >= until) {
            from_jit(di);
            cpu.pc = pc;
            return RUN_LIMIT;
        }
        refill(until, need);
        need = 0;
        entry = lookup(pc, di);
        jst.entry = entry;
        for (;;) {
            ev_t *e;
            if (dirty) {
                jit_sync();
                dirty = 0;
            }
            jit_enter();
            jit_stats.events++;
            e = &ev[jst.t1];
#ifdef JIT_DEBUG
            printf("  ev %u kind %u pc %04X pc2 %04X di %u kd %d cyc %u  now %lu A=%02X X=%02X Y=%02X S=%02X res %X c %X\n", jst.t1, e->kind, e->pc,
                   e->pc2, e->di, e->kd, e->cyc, (unsigned long)NOW(), jst.a, jst.x, jst.y, jst.s, jst.res, jst.c);
#endif
            switch (e->kind) {
            case EV_IOW: {
                uint16_t a = W16(mem6502[(uint16_t)(e->pc + 1)], mem6502[(uint16_t)(e->pc + 2)]);
                io_write(a, mem6502[a], NOW() + e->kd + e->cyc);
                jst.entry = e->resume;
                continue;
            }
            case EV_ASSIST: {
                uint32_t t0 = NOW() + e->kd;
                uint32_t delta;
                int ok;
                from_jit(e->di);
                cpu.cycles = t0;
                cpu.pc = e->pc;
                flushed = 0;
                ok = interp_step();
                jit_stats.assists++;
                if (!ok) {
                    return RUN_ILLEGAL;
                }
                delta = cpu.cycles - t0;
                if ((e->flags & EVF_RESUME) && !flushed) {
                    to_jit();
                    jst.cyc = (uint16_t)((int16_t)jst.cyc - (int16_t)(delta - e->cyc));
                    jst.entry = e->resume;
                    continue;
                }
                to_jit();
                pc = cpu.pc;
                di = cpu.d | cpu.i << 1;
                jbase = cpu.cycles;
                jbudget = 0;
                jst.cyc = 0;
                break;
            }
            case EV_BUDGET:
                pc = e->pc;
                di = e->di;
                need = e->cyc;
                break;
            case EV_DYNPC:
                pc = jst.t0;
                di = e->di;
                break;
            default:    /* EV_CHAIN */
                pc = e->pc;
                di = e->di;
                if (!(jit_flags & JIT_CHAIN_OFF) && pc != stop_pc) {
                    uint16_t gen0 = flush_gen;
                    uint32_t tgt = lookup(pc, di);
                    if (flush_gen == gen0)
                        link_ev(e, tgt);
                }
                break;
            }
            if (jit_step_hook) {
                from_jit(di);
                cpu.pc = pc;
                st = jit_step_hook();
                if (st)
                    return RUN_MISMATCH;
            }
            break;
        }
    }
}

int call_routine(uint16_t addr, uint8_t a, uint8_t x, uint8_t y, uint32_t max_cycles, int use_jit)
{
    uint32_t until = cpu.cycles + max_cycles;
    cpu.a = a;
    cpu.x = x;
    cpu.y = y;
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) >> 8;
    cpu.s--;
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) & 0xFF;
    cpu.s--;
    cpu.pc = addr;
    return use_jit ? jit_run(until, STOP_PC) : interp_run(until, STOP_PC);
}

/* flags (P bit positions) whose value matters at pc: the JIT does not keep dead flags exact */
uint8_t jit_live_p(uint16_t pc)
{
    uint8_t m = live_at(pc);
    return (m & NF ? P_N : 0) | (m & ZF ? P_Z : 0) | (m & CF ? P_C : 0) | (m & VF ? P_V : 0);
}
