/* trace2mock: turn an fsim --log of a program into the access stream the mock's
 * CPU stand-in (t1f_stub.v) replays, plus the SDRAM image at the start of the window.
 *
 *   trace2mock LOG IMAGE.bin --tick0 K --ticks N --out PREFIX [--ui ADDR --gen ADDR]
 *
 * LOG is fsim's binary log (sim/src/cpu.c log_rec): 8-byte records, an instruction
 * (pc | len<<24, start cycle) followed by its data accesses (kind<<28 | size<<24 | addr, value).
 * IMAGE.bin is what fsim loaded at 0x1000 (the memory before the program runs).
 * A "tick" is one pass of sidplay's main loop: it starts at the gen_one call that precedes
 * a ui_frame call (--gen, --ui are those symbols' addresses).
 *
 * Outputs: PREFIX.trace  one 10-hex-digit entry per bus access (a T1 period each):
 *                         [39:32] = {last-of-instruction, 3'b0, kind}  kind 0 fetch, 1 data read,
 *                                   2 write, 3 wait for tick (addr = n), 4 end, 5 prologue I/O write
 *                         [31:8] address, [7:0] data (writes: the byte; reads: the expected byte)
 *          PREFIX.hex    the SDRAM contents (32-bit words, hex) at the window's start
 * Instruction fetches are len bytes at pc, data accesses are expanded to bytes (T1 has an
 * 8-bit bus). I/O reads are dropped (they never reach the RAM); the I/O writes before the
 * window are replayed first (video, palette, audio engine setup) except the console/EXIT.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMSZ (1u << 23)
static uint8_t mem[MEMSZ], snap[MEMSZ];
typedef struct { uint32_t w0, w1; } rec_t;

static uint32_t hex(const char *s) { return (uint32_t)strtoul(s, NULL, 0); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage\n"); return 2; }
    const char *logf = argv[1], *imgf = argv[2], *out = "mock";
    int tick0 = 8, nticks = 9;
    uint32_t ui = 0x26fe, gen = 0x1ad8;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--tick0")) tick0 = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ticks")) nticks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out")) out = argv[++i];
        else if (!strcmp(argv[i], "--ui")) ui = hex(argv[++i]);
        else if (!strcmp(argv[i], "--gen")) gen = hex(argv[++i]);
    }
    FILE *f = fopen(imgf, "rb");
    if (!f) { perror(imgf); return 1; }
    size_t n = fread(mem + 0x1000, 1, MEMSZ - 0x1000, f);
    fclose(f);
    (void)n;
    f = fopen(logf, "rb");
    if (!f) { perror(logf); return 1; }
    fseek(f, 0, SEEK_END);
    long nrec = ftell(f) / 8;
    fseek(f, 0, SEEK_SET);
    rec_t *r = malloc(nrec * sizeof *r);
    if (fread(r, 8, nrec, f) != (size_t)nrec) { perror("read"); return 1; }
    fclose(f);

    /* pass A: tick starts (record index of the gen_one call before each ui_frame) */
    long *ts = malloc(100000 * sizeof *ts);
    int nt = 0;
    long lastgen = -1;
    for (long i = 0; i < nrec; i++) {
        if ((r[i].w0 >> 28) != 0) continue;
        uint32_t pc = r[i].w0 & 0xFFFFFF;
        if (pc == gen) lastgen = i;
        else if (pc == ui && nt < 100000) ts[nt++] = lastgen;
    }
    if (tick0 + nticks + 1 > nt) { fprintf(stderr, "only %d ticks in the log\n", nt); return 1; }
    long wstart = ts[tick0], wend = ts[tick0 + nticks];   /* the window: ticks tick0 .. tick0+nticks-1 */

    char name[512];
    snprintf(name, sizeof name, "%s.trace", out);
    FILE *tf = fopen(name, "w");
    /* prologue buffer: I/O writes before the window (collected in pass B) */
    long cap = 1 << 20, np = 0;
    uint64_t *pro = malloc(cap * sizeof *pro);

    long nent = 0, nmis = 0, nfetch = 0, nrd = 0, nwr = 0, nio = 0, ninst = 0;
    uint32_t maxa = 0;
    int in_win = 0, tk = 0;
    /* an entry buffer for the current instruction */
    uint64_t eb[64];
    int ne;
#define E(kind, a, d) (((uint64_t)(kind) << 32) | ((uint64_t)((a) & 0xFFFFFF) << 8) | (uint64_t)((d) & 0xFF))
    long i = 0;
    while (i < nrec) {
        if (i == wstart && !in_win) {
            in_win = 1;
            memcpy(snap, mem, MEMSZ);
            for (long k = 0; k < np; k++) fprintf(tf, "%010llx\n", (unsigned long long)pro[k]);
        }
        if (in_win && i == ts[tick0 + tk] && tk < nticks) {
            fprintf(tf, "%010llx\n", (unsigned long long)E(3, tk, 0)); nent++;
            tk++;
        }
        if (i >= wend) break;
        if ((r[i].w0 >> 28) != 0) { i++; continue; }
        uint32_t pc = r[i].w0 & 0xFFFFFF, len = r[i].w0 >> 24;
        ne = 0;
        for (uint32_t j = 0; j < len; j++) eb[ne++] = E(0, pc + j, mem[(pc + j) & (MEMSZ - 1)]);
        long k = i + 1;
        for (; k < nrec && (r[k].w0 >> 28) != 0; k++) {
            uint32_t kind = r[k].w0 >> 28, sz = (r[k].w0 >> 24) & 15, a = r[k].w0 & 0xFFFFFF, v = r[k].w1;
            int io = (a >> 16) == 0xF0;
            for (uint32_t j = 0; j < sz; j++) {
                uint8_t b = (uint8_t)(v >> (8 * j));
                if (io) {
                    if (kind == 2 && (a + j) >= 0xF00004) {
                        uint64_t e = E(5, a + j, b);
                        if (!in_win) { if (np < cap) pro[np++] = e; }
                        else if (ne < 62) eb[ne++] = E(2, a + j, b);
                        nio++;
                    }
                    continue;
                }
                if (a + j >= MEMSZ) { fprintf(stderr, "address %06x out of range\n", a + j); return 1; }
                if (a + j > maxa) maxa = a + j;
                if (kind == 1) {
                    if (mem[a + j] != b) nmis++;
                    if (ne < 62) eb[ne++] = E(1, a + j, mem[a + j]);
                } else {
                    mem[a + j] = b;
                    if (ne < 62) eb[ne++] = E(2, a + j, b);
                }
            }
        }
        if (in_win) {
            eb[ne - 1] |= (uint64_t)0x10 << 32;
            for (int q = 0; q < ne; q++) {
                fprintf(tf, "%010llx\n", (unsigned long long)eb[q]);
                nent++;
                int kd = (eb[q] >> 32) & 15;
                nfetch += kd == 0; nrd += kd == 1; nwr += kd == 2;
            }
            ninst++;
        }
        i = k;
    }
    fprintf(tf, "%010llx\n", (unsigned long long)E(4, 0, 0));
    fclose(tf);
    snprintf(name, sizeof name, "%s.hex", out);
    f = fopen(name, "w");
    uint32_t words = ((maxa | 0xFFF) + 1) / 4;
    for (uint32_t w = 0; w < words; w++)
        fprintf(f, "%02x%02x%02x%02x\n", snap[4*w+3], snap[4*w+2], snap[4*w+1], snap[4*w]);
    fclose(f);
    printf("ticks in log %d; window ticks %d..%d (instructions %ld..%ld)\n", nt, tick0, tick0 + nticks - 1, wstart, wend);
    printf("window: %ld instructions, %ld bus accesses (%ld fetch bytes, %ld data reads, %ld writes), %ld I/O writes\n",
           ninst, nent, nfetch, nrd, nwr, nio);
    printf("prologue I/O writes %ld; max RAM address %06x (%u words in the image); read-value mismatches vs image: %ld\n",
           np, maxa, words, nmis);
    return 0;
}
