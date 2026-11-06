/* Host harness for sim/tools/mcaud_check.py: runs sim/src/mcaud.c on the RAM image and control store
 * the RTL testbench (mcaud_tb.v) was given, through the same register writes the CPU makes, for as
 * many blocks as the RTL rendered, and prints what the testbench prints.
 *   mcaud_check DIR NBLOCKS CTRL [time]   (time: print the host microseconds a block took)  DIR: cs.hex ram.hex head.txt; writes DIR/out_m.txt, DIR/ram_m.hex */
#include "cpu.h"
#include "snd.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

uint32_t dev_cpu_hz = 67500000u;       /* the Tang's clock: 1406.25 cycles a frame */
int snd_device;
static FILE *out;
static uint64_t frames;
void snd_out(int16_t l, int16_t r) { frames++; fprintf(out, "%d %d\n", l, r); }
uint64_t snd_frame_cycle(uint64_t k, uint32_t rate) { return k * dev_cpu_hz / rate; }

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: mcaud_check DIR NBLOCKS CTRL\n"); return 2; }
    const char *dir = argv[1];
    unsigned nblk = (unsigned)atoi(argv[2]), ctrl = (unsigned)atoi(argv[3]);
    char p[4096], line[256];
    static cpu_t c;
    c.ram_size = 4u << 20;
    c.sram = calloc(1, c.ram_size);
    snprintf(p, sizeof p, "%s/out_m.txt", dir);
    out = fopen(p, "w");
    mcaud_reset();
    /* RAM image: one word a line, from word 0 */
    snprintf(p, sizeof p, "%s/ram.hex", dir);
    FILE *f = fopen(p, "r");
    uint32_t top = 0, v;
    while (f && fgets(line, sizeof line, f)) {
        v = (uint32_t)strtoul(line, NULL, 16);
        for (int i = 0; i < 4; i++) c.sram[top * 4 + i] = (uint8_t)(v >> (8 * i));
        top++;
    }
    if (f) fclose(f);
    /* the control store, 72 bits a line: five 16-bit writes, the last commits */
    snprintf(p, sizeof p, "%s/cs.hex", dir);
    f = fopen(p, "r");
    mcaud_write(&c, 0xF01008, 0);
    for (int a = 0; a < 512 && f && fgets(line, sizeof line, f); a++) {
        char *e = line + strlen(line);
        while (e > line && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
        char *q = e;                   /* hex digits from the right: 4 a field */
        uint16_t d[5];
        for (int i = 0; i < 5; i++) {
            char t[5] = { 0 };
            int n = (int)(q - line) < 4 ? (int)(q - line) : 4;
            memcpy(t, q - n, (size_t)n);
            d[i] = (uint16_t)strtoul(t, NULL, 16);
            q -= n;
        }
        for (int i = 0; i < 5; i++) mcaud_write(&c, 0xF0100A + 2 * (uint32_t)i, i == 4 ? (uint16_t)(d[i] & 0xFF) : d[i]);
    }
    if (f) fclose(f);
    snprintf(p, sizeof p, "%s/head.txt", dir);
    f = fopen(p, "r");
    uint32_t head = 0, tempo = 0;
    if (f) { if (fgets(line, sizeof line, f)) head = (uint32_t)strtoul(line, NULL, 16);
             if (fgets(line, sizeof line, f)) tempo = (uint32_t)strtoul(line, NULL, 16); fclose(f); }
    mcaud_write(&c, 0xF01000, (uint16_t)head); mcaud_write(&c, 0xF01002, (uint16_t)(head >> 16));
    mcaud_write(&c, 0xF01006, (uint16_t)tempo);
    mcaud_write(&c, 0xF01004, (uint16_t)ctrl);
    clock_t t0 = clock();
    while (mcaud_read(&c, 0xF01018) < nblk) {
        c.st.cycles = mcaud_next();
        mcaud_sync(&c);
    }
    if (argc > 4) fprintf(stderr, "%.1f us a block (%u blocks)\n", 1e6 * (double)(clock() - t0) / CLOCKS_PER_SEC / nblk, nblk);
    fclose(out);
    mcaud_write(&c, 0xF01004, (uint16_t)(ctrl & ~1u));
    printf("IRQ %x\nID %x\nBLOCKS %u\nUNDERRUNS %u\nCLOCKS %u MAX 0\nBURSTS %u\nWORDS %u\nNODES %u\nRING %u\n",
           mcaud_read(&c, 0xF01014), mcaud_read(&c, 0xF01016), mcaud_read(&c, 0xF01018), mcaud_read(&c, 0xF0101A), mcaud_read(&c, 0xF0101C),
           mcaud_read(&c, 0xF0101E), mcaud_read(&c, 0xF01020), mcaud_read(&c, 0xF01022), mcaud_read(&c, 0xF01024));
    for (unsigned i = 0; i < 288; i++) {
        mcaud_write(&c, 0xF01024, (uint16_t)i);
        printf("MON %u %04x%04x\n", i, mcaud_read(&c, 0xF01028), mcaud_read(&c, 0xF01026));
    }
    snprintf(p, sizeof p, "%s/ram_m.hex", dir);
    f = fopen(p, "w");
    for (uint32_t i = 0; i < top; i++) {
        const uint8_t *b = c.sram + i * 4;
        fprintf(f, "%08x\n", b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
    }
    fclose(f);
    return 0;
}
