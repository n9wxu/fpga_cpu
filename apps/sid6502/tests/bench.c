/* Speed of the interpreter and the JIT on the synthetic music routines (tools/mkmusic.py).
 * Native cycles come from the CPU's cycle counter, 6502 cycles from the engine's own count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sid6502.h"

#define FRAME_CYCLES 19656u
#define NFRAMES 100

static uint32_t rdcycle(void)
{
    volatile uint16_t *hi = (volatile uint16_t *)0xF00006, *lo = (volatile uint16_t *)0xF00004;
    uint16_t h, l, h2;
    do {
        h = *hi;
        l = *lo;
        h2 = *hi;
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}

static uint8_t buf[4096];

static int load_music(const char *fn, uint16_t *init, uint16_t *play)
{
    FILE *f = fopen(fn, "rb");
    uint8_t hdr[6];
    uint16_t nseg, i;
    if (!f) { printf("cannot open %s\n", fn); return 0; }
    fread(hdr, 1, 6, f);
    *init = hdr[0] | hdr[1] << 8;
    *play = hdr[2] | hdr[3] << 8;
    nseg = hdr[4] | hdr[5] << 8;
    sid_reset();
    for (i = 0; i < nseg; i++) {
        uint8_t h[4];
        uint16_t a, l;
        fread(h, 1, 4, f);
        a = h[0] | h[1] << 8;
        l = h[2] | h[3] << 8;
        fread(buf, 1, l, f);
        sid_load(a, buf, l);
    }
    fclose(f);
    return 1;
}

static void run(const char *fn, int jit)
{
    uint16_t init, play, f;
    uint32_t n0, n1, cold = 0, native = 0, c6502 = 0, cold6502 = 0, cold_native;
    int st = 0;
    if (!load_music(fn, &init, &play))
        return;
    cpu.s = 0xFF;
    cpu.i = 1;
    st = call_routine(init, 0, 0, 0, 100000, jit);
    for (f = 1; f <= NFRAMES && st == RUN_STOP; f++) {
        uint32_t t0;
        if (cpu.cycles < (uint32_t)f * FRAME_CYCLES)
            cpu.cycles = (uint32_t)f * FRAME_CYCLES;
        cpu.s = 0xFF;
        t0 = cpu.cycles;
        n0 = rdcycle();
        st = call_routine(play, 0, 0, 0, 200000, jit);
        n1 = rdcycle();
        if (f == 1) {
            cold = n1 - n0;
            cold6502 = cpu.cycles - t0;
        } else {
            native += n1 - n0;
            c6502 += cpu.cycles - t0;
        }
    }
    cold_native = cold;
    {
        uint32_t per6502 = c6502 / (NFRAMES - 1);
        uint32_t pernative = native / (NFRAMES - 1);
        /* ratio in hundredths */
        uint32_t ratio100 = native / (c6502 / 100u);
        /* percent of a 50 MHz CPU at 50 frames/s: pernative * 50 / 50e6 = pernative / 1e6, in tenths of a percent */
        uint32_t pct10 = pernative / 1000u;
        printf("%-12s %-6s st=%d  6502 cyc/frame %5lu  native cyc/frame %7lu  native/6502 %lu.%02lu  CPU %lu.%lu%%  sid writes %u",
               fn, jit ? "jit" : "interp", st, (unsigned long)per6502, (unsigned long)pernative,
               (unsigned long)(ratio100 / 100), (unsigned long)(ratio100 % 100), (unsigned long)(pct10 / 10),
               (unsigned long)(pct10 % 10), sid_n);
        if (jit)
            printf("  first frame %lu native (xlate ~%lu), %lu traces %lu instrs %lu bytes, %lu events",
                   (unsigned long)cold_native, (unsigned long)(cold_native > pernative ? cold_native - pernative : 0),
                   (unsigned long)jit_stats.traces, (unsigned long)jit_stats.instrs, (unsigned long)jit_stats.bytes,
                   (unsigned long)jit_stats.events);
        printf("\n");
        (void)cold6502;
    }
}

int main(void)
{
    const char *files[4] = { "MUSIC0.BIN", "MUSIC2K.BIN", "MUSIC5K.BIN", "DIGI.BIN" };
    int i;
    for (i = 0; i < 4; i++) {
        memset(&jit_stats, 0, sizeof jit_stats);
#ifndef JIT_ONLY
        run(files[i], 0);
#endif
        memset(&jit_stats, 0, sizeof jit_stats);
        run(files[i], 1);
    }
    return 0;
}
