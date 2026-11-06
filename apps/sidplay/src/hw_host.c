/* host build: the SID player on a PC, writing the engine register script for fpga/tools/audio_ref.py
 * and the raw SID write log. usage: sidplay_host file.sid song seconds script.txt [siddump.txt] [-nojit] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sid6502.h"
#include "sidplay.h"
#include "model.h"
#include "layout.h"

int call_routine(uint16_t addr, uint8_t a, uint8_t x, uint8_t y, uint32_t max_cycles, int use_jit)
{
    uint32_t until = cpu.cycles + max_cycles;
    (void)use_jit;
    cpu.a = a; cpu.x = x; cpu.y = y;
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) >> 8;
    cpu.s--;
    mem6502[0x100 + cpu.s] = (STOP_PC - 1) & 0xFF;
    cpu.s--;
    cpu.pc = addr;
    return interp_run(until, STOP_PC);
}

static FILE *script;
static uint32_t frame48;
void aud_raw(uint16_t addr, uint16_t val) { fprintf(script, "W %u %x %x\n", frame48, addr, val); }
uint32_t cyc_now(void) { return 0; }
static FILE *dump;
static uint32_t cur_frame;
static void hook(void)
{
    uint16_t i;
    for (i = 0; i < sid_n; i++)
        fprintf(dump, "%u %u %u %u\n", cur_frame, sidlog[i].cycle, sidlog[i].reg, sidlog[i].val);
}

int main(int argc, char **argv)
{
    FILE *f;
    static uint8_t buf[70000];
    psid_t p;
    uint32_t n, fr, nframes;
    int e, song;
    if (argc < 5) { fprintf(stderr, "usage\n"); return 1; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    e = psid_parse(buf, n, &p);
    if (e) { fprintf(stderr, "%s\n", psid_err(e)); return 1; }
    song = atoi(argv[2]);
    if (song < 1) song = p.start;
    script = fopen(argv[4], "w");
    if (argc > 5 && strcmp(argv[5], "-nojit")) dump = fopen(argv[5], "w");
    if (dump) pl_log_hook = hook;
    if (getenv("SM_MUTE")) sm_mute = (uint8_t)atoi(getenv("SM_MUTE"));
    pl_jit = 0;
    e = pl_start(&p, song);
    printf("%s / %s / %s  songs %d play rate %lu.%02lu Hz period %lu frames, init status %d\n", p.name, p.author, p.released, p.songs,
           (unsigned long)pl_rate_x100 / 100, (unsigned long)pl_rate_x100 % 100, (unsigned long)pl_period48, e);
    if (dump) fprintf(dump, "B %lu %lu\n", (unsigned long)cpu.cycles, (unsigned long)pl_cyc_per_call);
    sm_set_clock(pl_clk_hz);
    sm_init();
    model_init();
    nframes = (uint32_t)(atof(argv[3]) * pl_rate_x100 / 100);
    {
        static frame_t all[6000];
        if (nframes > 6000) nframes = 6000;
        for (fr = 0; fr < nframes; fr++) {
            cur_frame = fr;
            e = pl_frame(&all[fr]);
            if (e) { fprintf(stderr, "play status %d at frame %u\n", e, fr); break; }
        }
        model_detect(all, fr < 96 ? fr : 96);
        for (n = 0; n < fr; n++) {
            frame48 = (uint32_t)((uint64_t)n * 48000ULL * pl_cyc_per_call / pl_clk_hz);
            sm_apply(&all[n]);
            model_feed(&all[n], n);
        }
    }
    if (getenv("MODEL")) {
        uint16_t i, n = model_nrows();
        printf("speed %d, %d rows, %d instruments\n", model_speed(), n, model_ninst() - 1);
        for (i = 0; i < n; i++) {
            const row_t *r = model_row(i);
            int v;
            if (!r) continue;
            printf("%03X f%-4lu ", i & 0xFFF, (unsigned long)r->frame0);
            for (v = 0; v < 3; v++) {
                const cell_t *c = &r->c[v];
                printf("%s %02X %02X %c%02X | ", c->note == 0 ? "..." : c->note == 0xFE ? "---" : note_name(c->note), c->inst, c->wave, c->fxc ? c->fxc : '.', c->fxv);
            }
            printf("\n");
        }
    }
    fprintf(script, "E %lu\n", (unsigned long)(fr * pl_period48));
    fclose(script);
    return 0;
}
