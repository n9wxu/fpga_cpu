/* audeng_check SCRIPT MEM.HEX OUT.TXT: render an audio_ref.py register script with fsim's audio
 * engine (sim/src/audeng.c), one "left right" line a frame, as audio_ref.py writes them.
 * sim/tools/audeng_check.py builds this and compares the two. */
#include "cpu.h"
#include "snd.h"

#include <stdlib.h>
#include <string.h>

uint32_t dev_cpu_hz = 48000;           /* a cycle a frame */
uint64_t snd_next;
static FILE *out;
void snd_out(int16_t l, int16_t r) { fprintf(out, "%d %d\n", l, r); }

typedef struct { long f; int mem; uint32_t a, v; } ev_t;

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: audeng_check SCRIPT MEM.HEX OUT.TXT\n");
        return 2;
    }
    static cpu_t c;
    c.ram_size = 16u << 20;
    c.sram = calloc(c.ram_size, 1);
    FILE *f = fopen(argv[2], "r");
    if (!f) { perror(argv[2]); return 2; }
    unsigned w;
    for (uint32_t i = 0; fscanf(f, "%x", &w) == 1 && i + 4 <= c.ram_size; i += 4)
        memcpy(c.sram + i, &w, 4);                       /* little endian, as the hex's words */
    fclose(f);
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    ev_t *ev = NULL;
    size_t n = 0, cap = 0;
    long frames = 0;
    char line[128], k;
    while (fgets(line, sizeof line, f)) {
        long fr;
        unsigned a, v;
        if (sscanf(line, " %c %ld %x %x", &k, &fr, &a, &v) == 4 && (k == 'W' || k == 'M')) {
            if (n == cap) ev = realloc(ev, (cap = cap ? 2 * cap : 256) * sizeof *ev);
            ev[n++] = (ev_t){ fr, k == 'M', a, v };
        } else if (sscanf(line, " E %ld", &fr) == 1)
            frames = fr;
    }
    fclose(f);
    out = fopen(argv[3], "w");
    if (!out) { perror(argv[3]); return 2; }
    audeng_reset();
    size_t e = 0;
    for (long fr = 0; fr < frames; fr++) {
        c.st.cycles = (uint64_t)fr;                      /* frames 0 .. fr-1 made: the writes for frame fr */
        audeng_sync(&c);
        for (; e < n && ev[e].f <= fr; e++) {
            if (ev[e].f < fr)
                continue;
            if (ev[e].mem)
                memcpy(c.sram + 4 * ev[e].a, &ev[e].v, 4);
            else
                audeng_write(&c, 0xF00000u + ev[e].a, (uint16_t)ev[e].v);
        }
    }
    c.st.cycles = (uint64_t)frames;
    audeng_sync(&c);
    fclose(out);
    return 0;
}
