/* Test runner: reads the vectors made by gen_tests.py and runs each test on the interpreter
 * (host and target) and the JIT (target), comparing with the Python reference's results.
 * Build: host `make -C apps/sid6502 host`, target `make -C apps/sid6502 test`. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sid6502.h"

#define MAXSEG   24576u
#define MAXSID   3000
#define MAXDELTA 12000
#define MAXREG   24

typedef struct { uint16_t addr, len; } region_t;
#ifdef WITH_JIT
extern uint8_t smem6502[65536];
#endif

static char name[25];
static uint8_t flags, ta, tx, ty, ts, tp;
static uint16_t entry, play, nframes;
static uint32_t maxcyc, frame_cycles;
static uint16_t nseg_total;
static uint8_t segbuf[MAXSEG];          /* addr(2) len(2) data ... */
static uint16_t segbytes;
static uint8_t e_status, e_a, e_x, e_y, e_s, e_p;
static uint16_t e_pc;
static uint32_t e_cycles;
static uint16_t e_nsid;
static sidwrite_t e_sid[MAXSID];
static uint16_t e_nreg, e_ndelta;
static region_t regs[MAXREG];
static uint16_t d_addr[MAXDELTA];
static uint8_t d_val[MAXDELTA];
static uint8_t snap[MAXSEG];

static FILE *fp;
static int rd_ok;

static uint32_t rdn(int n)
{
    uint8_t b[4];
    uint32_t v = 0;
    int i;
    if (fread(b, 1, n, fp) != (size_t)n) {
        rd_ok = 0;
        return 0;
    }
    for (i = n - 1; i >= 0; i--)
        v = v << 8 | b[i];
    return v;
}

static int read_test(void)
{
    uint16_t i, n, pos = 0;
    rd_ok = 1;
    if (fread(name, 1, 24, fp) != 24)
        return 0;
    name[24] = 0;
    flags = rdn(1); entry = rdn(2); ta = rdn(1); tx = rdn(1); ty = rdn(1); ts = rdn(1); tp = rdn(1);
    maxcyc = rdn(4); play = rdn(2); nframes = rdn(2); frame_cycles = rdn(4);
    n = rdn(2);
    nseg_total = n;
    for (i = 0; i < n && rd_ok; i++) {
        uint16_t a = rdn(2), l = rdn(2);
        if (pos + 4u + l > MAXSEG) { printf("segment buffer full\n"); exit(2); }
        segbuf[pos] = a & 255; segbuf[pos + 1] = a >> 8; segbuf[pos + 2] = l & 255; segbuf[pos + 3] = l >> 8;
        if (fread(segbuf + pos + 4, 1, l, fp) != l) rd_ok = 0;
        pos += 4 + l;
    }
    segbytes = pos;
    e_status = rdn(1); e_a = rdn(1); e_x = rdn(1); e_y = rdn(1); e_s = rdn(1); e_p = rdn(1);
    e_pc = rdn(2); e_cycles = rdn(4);
    e_nsid = rdn(2);
    if (e_nsid > MAXSID) { printf("too many sid writes in %s\n", name); exit(2); }
    for (i = 0; i < e_nsid; i++) {
        e_sid[i].cycle = rdn(4); e_sid[i].reg = rdn(1); e_sid[i].val = rdn(1);
    }
    e_nreg = rdn(2);
    if (e_nreg > MAXREG) { printf("too many regions\n"); exit(2); }
    for (i = 0; i < e_nreg; i++) { regs[i].addr = rdn(2); regs[i].len = rdn(2); }
    e_ndelta = rdn(2);
    if (e_ndelta > MAXDELTA) { printf("too many deltas in %s\n", name); exit(2); }
    for (i = 0; i < e_ndelta; i++) { d_addr[i] = rdn(2); d_val[i] = rdn(1); }
    return rd_ok;
}

static void apply_segments_to(uint8_t *m)
{
    uint16_t pos = 0;
    while (pos < segbytes) {
        uint16_t a = segbuf[pos] | segbuf[pos + 1] << 8, l = segbuf[pos + 2] | segbuf[pos + 3] << 8;
        memcpy(m + a, segbuf + pos + 4, l);
        pos += 4 + l;
    }
}

static void apply_segments(void) { apply_segments_to(mem6502); }

static void clear_memory_of(uint8_t *m)
{
    uint16_t pos = 0, i;
    while (pos < segbytes) {
        uint16_t a = segbuf[pos] | segbuf[pos + 1] << 8, l = segbuf[pos + 2] | segbuf[pos + 3] << 8;
        memset(m + a, 0, l);
        pos += 4 + l;
    }
    for (i = 0; i < e_nreg; i++)
        memset(m + regs[i].addr, 0, regs[i].len);
    memset(m, 0, 0x200);
}

static void clear_memory(void) { clear_memory_of(mem6502); }

typedef int (*runner_t)(uint32_t, uint16_t);

static void push_sentinel(void)
{
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) >> 8;
    cpu.s--;
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) & 255;
    cpu.s--;
}

#ifdef WITH_JIT
extern cpu6502_t scpu;
extern uint8_t smem6502[65536];
extern uint8_t sattr6502[65536];
extern uint16_t ssid_n;
extern sidwrite_t ssidlog[SID_LOG_MAX];
int sinterp_step(void);
void ssid_reset(void);
void sjit_flush(void) {}
void ssmc_write(uint16_t a) { (void)a; }
static uint16_t last_pc;
static int step_count;

static int step_hook(void)
{
    uint16_t r, k;
    uint8_t sp, jp;
    while (scpu.cycles < cpu.cycles && sinterp_step())
        ;
    step_count++;
#define SMIS(...) do { printf("  STEP %d: trace from %04X to %04X: ", step_count, last_pc, cpu.pc); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)
    if (scpu.cycles != cpu.cycles) SMIS("cycles %lu want %lu", (unsigned long)cpu.cycles, (unsigned long)scpu.cycles);
    if (scpu.pc != cpu.pc) SMIS("pc %04X want %04X", cpu.pc, scpu.pc);
    if (scpu.a != cpu.a) SMIS("A %02X want %02X", cpu.a, scpu.a);
    if (scpu.x != cpu.x) SMIS("X %02X want %02X", cpu.x, scpu.x);
    if (scpu.y != cpu.y) SMIS("Y %02X want %02X", cpu.y, scpu.y);
    if (scpu.s != cpu.s) SMIS("S %02X want %02X", cpu.s, scpu.s);
    sp = (scpu.n << 7 | scpu.v << 6 | scpu.d << 3 | scpu.i << 2 | scpu.z << 1 | scpu.c);
    jp = (cpu.n << 7 | cpu.v << 6 | cpu.d << 3 | cpu.i << 2 | cpu.z << 1 | cpu.c);
    {
        uint8_t lm = jit_live_p(cpu.pc) | P_D | P_I;
        sp &= lm;
        jp &= lm;
    }
    if (sp != jp) SMIS("P %02X want %02X", jp, sp);
    if (ssid_n != sid_n) SMIS("sid writes %u want %u", sid_n, ssid_n);
    for (r = 0; r < e_nreg; r++)
        for (k = 0; k < regs[r].len; k++) {
            uint16_t a = regs[r].addr + k;
            if (smem6502[a] != mem6502[a]) SMIS("mem[%04X] %02X want %02X", a, mem6502[a], smem6502[a]);
        }
    last_pc = cpu.pc;
    return 0;
}
#endif

static int run_engine(runner_t run)
{
    int st;
    uint16_t f;
    uint32_t limit;
    sid_log_clear();
    cpu.a = ta; cpu.x = tx; cpu.y = ty; cpu.s = ts; cpu_setp(tp);
    cpu.cycles = 0;
    push_sentinel();
    cpu.pc = entry;
    limit = maxcyc;
#ifdef WITH_JIT
    if (run == jit_run && (jit_flags & JIT_CHAIN_OFF)) {
        clear_memory_of(smem6502);
        apply_segments_to(smem6502);
        smem6502[0x100 + ts] = mem6502[0x100 + ts];
        smem6502[0x100 + (uint8_t)(ts - 1)] = mem6502[0x100 + (uint8_t)(ts - 1)];
        memset(sattr6502, 0, 65536);
        memset(sattr6502 + 0xD000, ATTR_IO, 0x1000);
        scpu = cpu;
        ssid_n = 0;
        last_pc = entry;
        step_count = 0;
    }
#endif
    st = run(limit, STOP_PC);
    for (f = 1; f <= nframes && st == RUN_STOP; f++) {
        if (cpu.cycles < (uint32_t)f * frame_cycles)
            cpu.cycles = (uint32_t)f * frame_cycles;
        cpu.a = cpu.x = cpu.y = 0;
        cpu.s = ts;
        push_sentinel();
        cpu.pc = play;
        st = run(cpu.cycles + maxcyc, STOP_PC);
    }
    return st;
}

static uint8_t expmem[MAXSEG];          /* expected contents of the checked regions, back to back */

static void build_expected(void)
{
    uint16_t r, i, k;
    uint16_t off = 0;
    for (r = 0; r < e_nreg; r++) {
        if (off + regs[r].len > MAXSEG) { printf("regions too big\n"); exit(2); }
        memcpy(expmem + off, mem6502 + regs[r].addr, regs[r].len);
        off += regs[r].len;
    }
    for (i = 0; i < e_ndelta; i++) {
        off = 0;
        for (r = 0; r < e_nreg; r++) {
            if (d_addr[i] >= regs[r].addr && (uint16_t)(d_addr[i] - regs[r].addr) < regs[r].len) {
                k = d_addr[i] - regs[r].addr;
                expmem[off + k] = d_val[i];
                break;
            }
            off += regs[r].len;
        }
    }
}

static int compare(const char *eng, int st)
{
    uint16_t i, r, k, off = 0;
#define FAIL(...) do { printf("FAIL %s [%s]: ", name, eng); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)
    if (st != e_status) FAIL("status %d want %d (pc %04X)", st, e_status, cpu.pc);
    if (cpu.a != e_a) FAIL("A %02X want %02X", cpu.a, e_a);
    if (cpu.x != e_x) FAIL("X %02X want %02X", cpu.x, e_x);
    if (cpu.y != e_y) FAIL("Y %02X want %02X", cpu.y, e_y);
    if (cpu.s != e_s) FAIL("S %02X want %02X", cpu.s, e_s);
    if ((cpu_getp(0) & 0xCF) != e_p) FAIL("P %02X want %02X", cpu_getp(0) & 0xCF, e_p);
    if (cpu.pc != e_pc) FAIL("PC %04X want %04X", cpu.pc, e_pc);
    if (cpu.cycles != e_cycles) FAIL("cycles %lu want %lu", (unsigned long)cpu.cycles, (unsigned long)e_cycles);
    if (sid_n != e_nsid) FAIL("sid writes %u want %u", sid_n, e_nsid);
    for (i = 0; i < sid_n; i++)
        if (sidlog[i].cycle != e_sid[i].cycle || sidlog[i].reg != e_sid[i].reg || sidlog[i].val != e_sid[i].val)
            FAIL("sid[%u] (%lu,%u,%02X) want (%lu,%u,%02X)", i, (unsigned long)sidlog[i].cycle, sidlog[i].reg,
                 sidlog[i].val, (unsigned long)e_sid[i].cycle, e_sid[i].reg, e_sid[i].val);
    for (r = 0; r < e_nreg; r++) {
        for (k = 0; k < regs[r].len; k++)
            if (mem6502[(uint16_t)(regs[r].addr + k)] != expmem[off + k])
                FAIL("mem[%04X] %02X want %02X", (uint16_t)(regs[r].addr + k), mem6502[(uint16_t)(regs[r].addr + k)], expmem[off + k]);
        off += regs[r].len;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *files[5] = { "TESTS1.BIN", "TESTS2.BIN", "TESTS3.BIN", "TESTS4.BIN", 0 };
    int fi, pass[3] = { 0, 0, 0 }, fail[3] = { 0, 0, 0 }, e;
    const char *only = 0;
    char path[128];
    const char *dir = "";
    int nengines = 1;
    runner_t engines[3];
    sid_reset();
    const char *enames[3] = { "interp", "jit", "jitstep" };
    engines[0] = interp_run;
#ifdef WITH_JIT
    engines[1] = jit_run;
    engines[2] = jit_run;
    nengines = 3;
#endif
#ifndef __fcpu__
    if (argc > 1) dir = argv[1];
    if (argc > 2) only = argv[2];
#endif
#ifdef ONLY
    only = ONLY;
#endif
    (void)argc; (void)argv;
    for (fi = 0; files[fi]; fi++) {
        uint16_t nt, t;
        snprintf(path, sizeof path, "%s%s", dir, files[fi]);
        fp = fopen(path, "rb");
        if (!fp) { printf("cannot open %s\n", path); continue; }
        if (rdn(4) != 0x54444953u) { printf("bad magic in %s\n", path); return 2; }
        nt = rdn(2);
        printf("%s: %u tests\n", files[fi], nt);
        for (t = 0; t < nt; t++) {
            if (!read_test()) { printf("short read in %s test %u\n", path, t); break; }
            if (only && !strstr(name, only)) continue;
            apply_segments();
            build_expected();
            for (e = 0; e < nengines; e++) {
                int st;
#ifdef WITH_JIT
                if (e == 2 && (nframes != 0 || (flags & 1))) continue;
#endif
                if (e) {
                    /* restore the initial memory for the next engine */
                    clear_memory();
                    apply_segments();
                    jit_flush();
                }
#ifdef WITH_JIT
                if (e == 2) {
                    jit_flags |= JIT_CHAIN_OFF;
                    jit_step_hook = step_hook;
                }
#endif
                st = run_engine(engines[e]);
#ifdef WITH_JIT
                if (e == 2) {
                    jit_step_hook = 0;
                    jit_flags &= ~JIT_CHAIN_OFF;
                    clear_memory_of(smem6502);
                    if (st == RUN_MISMATCH) { fail[e]++; printf("FAIL %s [jitstep]\n", name); continue; }
                }
#endif
                if (compare(enames[e], st)) {
                    fail[e]++;
#ifdef WITH_JIT
                    if (e == 1 && nframes == 0) {
                        clear_memory();
                        apply_segments();
                        jit_flush();
                        jit_flags |= JIT_CHAIN_OFF;
                        jit_step_hook = step_hook;
                        run_engine(jit_run);
                        jit_step_hook = 0;
                        jit_flags &= ~JIT_CHAIN_OFF;
                    }
#endif
                } else {
                    pass[e]++;
                }
            }
            clear_memory();
#ifdef WITH_JIT
            jit_flush();
#endif
        }
        fclose(fp);
    }
    for (e = 0; e < nengines; e++)
        printf("%s: %d passed, %d failed\n", enames[e], pass[e], fail[e]);
    {
        int bad = 0;
        for (e = 0; e < nengines; e++) bad += fail[e];
        printf(bad ? "TESTS FAILED\n" : "ALL TESTS PASSED\n");
        return bad != 0;
    }
}
