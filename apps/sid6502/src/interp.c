/* 6502 interpreter: the reference engine and the JIT's fallback. */
#include <string.h>
#ifdef SHADOW
#include "shadow.h"
#endif
#include "sid6502.h"
#include "optable.h"

#define W16(lo, hi) ((uint16_t)((uint16_t)(hi) << 8 | (uint16_t)(lo)))

#ifndef SID_MEM_ADDR
uint8_t mem6502[65536];
uint8_t attr6502[65536];
sidwrite_t sidlog[SID_LOG_MAX];
#endif
cpu6502_t cpu;
uint16_t sid_n, sid_dropped;
uint8_t (*io_read_hook)(uint16_t, uint32_t);
void (*io_write_hook)(uint16_t, uint8_t, uint32_t);
uint16_t interp_nominal;

uint8_t cpu_getp(uint8_t b)
{
    return (uint8_t)(cpu.n << 7 | cpu.v << 6 | 0x20 | (b ? P_B : 0) | cpu.d << 3 | cpu.i << 2 | cpu.z << 1 | cpu.c);
}

void cpu_setp(uint8_t p)
{
    cpu.n = p >> 7 & 1;
    cpu.v = p >> 6 & 1;
    cpu.d = p >> 3 & 1;
    cpu.i = p >> 2 & 1;
    cpu.z = p >> 1 & 1;
    cpu.c = p & 1;
}

void sid_log_clear(void)
{
    sid_n = 0;
    sid_dropped = 0;
}

void sid_reset(void)
{
    memset(mem6502, 0, 65536);
    memset(attr6502, 0, 65536);
    memset(attr6502 + 0xD000, ATTR_IO, 0x1000);
    memset(&cpu, 0, sizeof cpu);
    cpu.s = 0xFF;
    cpu.i = 1;
    sid_log_clear();
}

void sid_load(uint16_t addr, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
        mem6502[(uint16_t)(addr + i)] = data[i];
    jit_flush();
}

uint8_t io_read(uint16_t a, uint32_t cycle)
{
    if (io_read_hook)
        return io_read_hook(a, cycle);
    if (a == 0xD012 || a == 0xD011) {
        uint32_t line = (cycle / 63) % 312;
        if (a == 0xD012)
            return (uint8_t)line;
        return (uint8_t)((mem6502[a] & 0x7F) | ((line >> 8) << 7));
    }
    return mem6502[a];
}

void io_write(uint16_t a, uint8_t v, uint32_t cycle)
{
    if (a >= 0xD400 && a < 0xD800) {
        if (sid_n < SID_LOG_MAX) {
            sidlog[sid_n].cycle = cycle;
            sidlog[sid_n].reg = a & 31;
            sidlog[sid_n].val = v;
            sid_n++;
        } else {
            sid_dropped++;
        }
    }
    if (io_write_hook)
        io_write_hook(a, v, cycle);
}

static inline uint8_t rd(uint16_t a, uint32_t t)
{
    if (attr6502[a] & ATTR_IO)
        return io_read(a, t);
    return mem6502[a];
}

static inline void wr(uint16_t a, uint8_t v, uint32_t t)
{
    uint8_t at = attr6502[a];
    mem6502[a] = v;
    if (at) {
        if (at & ATTR_IO)
            io_write(a, v, t);
        if (at & ATTR_CODE)
            smc_write(a);
    }
}

static inline void push(uint8_t v)
{
    mem6502[0x100 + cpu.s] = v;     /* the stack page is never code or I/O in practice; checked anyway */
    if (attr6502[0x100 + cpu.s] & ATTR_CODE)
        smc_write(0x100 + cpu.s);
    cpu.s--;
}

static inline uint8_t pull(void)
{
    cpu.s++;
    return mem6502[0x100 + cpu.s];
}

static inline uint8_t nz(uint16_t v)
{
    v &= 0xFF;
    cpu.n = v >> 7;
    cpu.z = (v == 0);
    return (uint8_t)v;
}

static void adc(uint8_t m)
{
    uint8_t a = cpu.a;
    uint16_t r;
    if (cpu.d) {
        uint16_t al = (a & 15) + (m & 15) + cpu.c;
        if (al >= 10)
            al = ((al + 6) & 15) + 16;
        r = (a & 0xF0) + (m & 0xF0) + al;
        cpu.z = (((a + m + cpu.c) & 0xFF) == 0);
        cpu.n = (r >> 7) & 1;
        cpu.v = ((~(a ^ m) & (a ^ r) & 0x80) != 0);
        if (r >= 0xA0)
            r += 0x60;
        cpu.c = (r >= 0x100);
        cpu.a = (uint8_t)r;
    } else {
        r = a + m + cpu.c;
        cpu.v = ((~(a ^ m) & (a ^ r) & 0x80) != 0);
        cpu.c = r >> 8;
        cpu.a = nz(r);
    }
}

static void sbc_dec(uint8_t m)
{
    uint8_t a = cpu.a;
    uint8_t cin = cpu.c;
    int16_t r = (int16_t)a - m - (1 - cin);
    int16_t al = (a & 15) - (m & 15) + cin - 1;
    int16_t rr;
    if (al < 0)
        al = ((al - 6) & 15) - 16;
    rr = (a & 0xF0) - (m & 0xF0) + al;
    if (rr < 0)
        rr -= 0x60;
    cpu.v = (((a ^ m) & (a ^ r) & 0x80) != 0);
    cpu.c = (r >= 0);
    nz((uint16_t)r);
    cpu.a = (uint8_t)rr;
}

static void do_sbc(uint8_t m)
{
    if (cpu.d) {
        sbc_dec(m);
    } else {
        uint8_t a = cpu.a;
        int16_t r = (int16_t)a - m - (1 - cpu.c);
        cpu.v = (((a ^ m) & (a ^ r) & 0x80) != 0);
        cpu.c = (r >= 0);
        cpu.a = nz((uint16_t)r);
    }
}

static void cmp(uint8_t r, uint8_t m)
{
    int16_t t = (int16_t)r - m;
    cpu.c = (t >= 0);
    nz((uint16_t)t);
}

static uint8_t asl(uint8_t v) { cpu.c = v >> 7; return nz((uint16_t)v << 1); }
static uint8_t lsr(uint8_t v) { cpu.c = v & 1; return nz(v >> 1); }
static uint8_t rol(uint8_t v) { uint8_t c = cpu.c; cpu.c = v >> 7; return nz(((uint16_t)v << 1) | c); }
static uint8_t ror(uint8_t v) { uint8_t c = cpu.c; cpu.c = v & 1; return nz((v >> 1) | (c << 7)); }

int interp_step(void)
{
    uint16_t pc = cpu.pc;
    uint8_t opc = mem6502[pc];
    uint8_t mn = op_mn[opc];
    uint8_t mode = op_mode[opc];
    uint8_t cyc = op_cyc[opc];
    uint8_t b1 = mem6502[(uint16_t)(pc + 1)];
    uint8_t b2 = mem6502[(uint16_t)(pc + 2)];
    uint16_t npc = pc + op_len[opc];
    uint16_t nxt = npc;
    uint16_t ea = 0, base;
    uint8_t extra = 0, v;
    uint32_t t0 = cpu.cycles;
    uint32_t tr = t0 + cyc - 1;     /* cycle of the last access */

    if (mn == N_ILL || (mn == N_ARR && cpu.d))
        return 0;
    switch (mode) {
    case AM_ZP:  ea = b1; break;
    case AM_ZPX: ea = (uint8_t)(b1 + cpu.x); break;
    case AM_ZPY: ea = (uint8_t)(b1 + cpu.y); break;
    case AM_ABS: ea = W16(b1, b2); break;
    case AM_ABX:
        base = W16(b1, b2);
        ea = base + cpu.x;
        extra = ((base ^ ea) & 0xFF00) != 0;
        break;
    case AM_ABY:
        base = W16(b1, b2);
        ea = base + cpu.y;
        extra = ((base ^ ea) & 0xFF00) != 0;
        break;
    case AM_IZX: {
        uint8_t p = b1 + cpu.x;
        ea = W16(mem6502[p], mem6502[(uint8_t)(p + 1)]);
        break;
    }
    case AM_IZY:
        base = W16(mem6502[b1], mem6502[(uint8_t)(b1 + 1)]);
        ea = base + cpu.y;
        extra = ((base ^ ea) & 0xFF00) != 0;
        break;
    default: break;
    }
    if (!op_pen[opc])
        extra = 0;

#define GET()  (mode == AM_IMM ? b1 : rd(ea, tr))
#define PUT(x) wr(ea, (x), tr)

    switch (mn) {
    case N_LDA: cpu.a = nz(GET()); break;
    case N_LDX: cpu.x = nz(GET()); break;
    case N_LDY: cpu.y = nz(GET()); break;
    case N_STA: PUT(cpu.a); break;
    case N_STX: PUT(cpu.x); break;
    case N_STY: PUT(cpu.y); break;
    case N_ADC: adc(GET()); break;
    case N_SBC: do_sbc(GET()); break;
    case N_AND: cpu.a = nz(cpu.a & GET()); break;
    case N_ORA: cpu.a = nz(cpu.a | GET()); break;
    case N_EOR: cpu.a = nz(cpu.a ^ GET()); break;
    case N_CMP: cmp(cpu.a, GET()); break;
    case N_CPX: cmp(cpu.x, GET()); break;
    case N_CPY: cmp(cpu.y, GET()); break;
    case N_BIT:
        v = GET();
        cpu.z = ((cpu.a & v) == 0);
        cpu.n = v >> 7;
        cpu.v = v >> 6 & 1;
        break;
    case N_ASL: if (mode == AM_ACC) cpu.a = asl(cpu.a); else PUT(asl(GET())); break;
    case N_LSR: if (mode == AM_ACC) cpu.a = lsr(cpu.a); else PUT(lsr(GET())); break;
    case N_ROL: if (mode == AM_ACC) cpu.a = rol(cpu.a); else PUT(rol(GET())); break;
    case N_ROR: if (mode == AM_ACC) cpu.a = ror(cpu.a); else PUT(ror(GET())); break;
    case N_INC: PUT(nz(GET() + 1)); break;
    case N_DEC: PUT(nz(GET() - 1)); break;
    case N_INX: cpu.x = nz(cpu.x + 1); break;
    case N_INY: cpu.y = nz(cpu.y + 1); break;
    case N_DEX: cpu.x = nz(cpu.x - 1); break;
    case N_DEY: cpu.y = nz(cpu.y - 1); break;
    case N_TAX: cpu.x = nz(cpu.a); break;
    case N_TAY: cpu.y = nz(cpu.a); break;
    case N_TXA: cpu.a = nz(cpu.x); break;
    case N_TYA: cpu.a = nz(cpu.y); break;
    case N_TSX: cpu.x = nz(cpu.s); break;
    case N_TXS: cpu.s = cpu.x; break;
    case N_PHA: push(cpu.a); break;
    case N_PLA: cpu.a = nz(pull()); break;
    case N_PHP: push(cpu_getp(1)); break;
    case N_PLP: cpu_setp(pull()); break;
    case N_CLC: cpu.c = 0; break;
    case N_SEC: cpu.c = 1; break;
    case N_CLI: cpu.i = 0; break;
    case N_SEI: cpu.i = 1; break;
    case N_CLD: cpu.d = 0; break;
    case N_SED: cpu.d = 1; break;
    case N_CLV: cpu.v = 0; break;
    case N_NOP: if (mode != AM_IMP && mode != AM_IMM) (void)GET(); break;
    case N_JMP:
        if (mode == AM_ABS) {
            nxt = W16(b1, b2);
        } else {
            uint16_t p = W16(b1, b2);
            nxt = W16(mem6502[p], mem6502[(p & 0xFF00) | ((p + 1) & 0xFF)]);
        }
        break;
    case N_JSR:
        push((uint16_t)(npc - 1) >> 8);
        push((npc - 1) & 0xFF);
        nxt = W16(b1, b2);
        break;
    case N_RTS: {
        uint8_t lo = pull();
        uint8_t hi = pull();
        nxt = (uint16_t)(W16(lo, hi) + 1);
        break;
    }
    case N_RTI: {
        uint8_t lo, hi;
        cpu_setp(pull());
        lo = pull();
        hi = pull();
        nxt = W16(lo, hi);
        break;
    }
    case N_BRK:
        push((uint16_t)(pc + 2) >> 8);
        push((pc + 2) & 0xFF);
        push(cpu_getp(1));
        cpu.i = 1;
        nxt = W16(mem6502[0xFFFE], mem6502[0xFFFF]);
        break;
    case N_BPL: case N_BMI: case N_BVC: case N_BVS: case N_BCC: case N_BCS: case N_BNE: case N_BEQ: {
        uint8_t take;
        switch (mn) {
        case N_BPL: take = !cpu.n; break;
        case N_BMI: take = cpu.n; break;
        case N_BVC: take = !cpu.v; break;
        case N_BVS: take = cpu.v; break;
        case N_BCC: take = !cpu.c; break;
        case N_BCS: take = cpu.c; break;
        case N_BNE: take = !cpu.z; break;
        default:    take = cpu.z; break;
        }
        if (take) {
            nxt = npc + (int8_t)b1;
            extra = 1 + (((nxt ^ npc) & 0xFF00) != 0);
        }
        break;
    }
    /* undocumented, stable */
    case N_LAX: cpu.a = cpu.x = nz(GET()); break;
    case N_SAX: PUT(cpu.a & cpu.x); break;
    case N_SLO: v = asl(GET()); PUT(v); cpu.a = nz(cpu.a | v); break;
    case N_RLA: v = rol(GET()); PUT(v); cpu.a = nz(cpu.a & v); break;
    case N_SRE: v = lsr(GET()); PUT(v); cpu.a = nz(cpu.a ^ v); break;
    case N_RRA: v = ror(GET()); PUT(v); adc(v); break;
    case N_DCP: v = GET() - 1; PUT(v); cmp(cpu.a, v); break;
    case N_ISC: v = GET() + 1; PUT(v); do_sbc(v); break;
    case N_ANC: cpu.a = nz(cpu.a & b1); cpu.c = cpu.n; break;
    case N_ALR: cpu.a = lsr(cpu.a & b1); break;
    case N_ARR: {
        uint8_t r = ((cpu.a & b1) >> 1) | (cpu.c << 7);
        cpu.a = nz(r);
        cpu.c = r >> 6 & 1;
        cpu.v = (r >> 6 & 1) ^ (r >> 5 & 1);
        break;
    }
    case N_SBX: {
        int16_t t = (int16_t)(cpu.a & cpu.x) - b1;
        cpu.c = (t >= 0);
        cpu.x = nz((uint16_t)t);
        break;
    }
    default:
        return 0;
    }
    cpu.pc = nxt;
    cpu.cycles = t0 + cyc + extra;
    interp_nominal = extra;
    return 1;
}

int interp_run(uint32_t until, uint16_t stop_pc)
{
    for (;;) {
        if (cpu.pc == stop_pc)
            return RUN_STOP;
        if (cpu.cycles >= until)
            return RUN_LIMIT;
        if (!interp_step())
            return RUN_ILLEGAL;
    }
}
