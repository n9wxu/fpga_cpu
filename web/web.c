/* fpga_cpu simulator: WebAssembly front end.
 *
 * Runs the BASIC image in the browser.  The page (index.html) calls the
 * web_* functions below: one web_frame per animation frame, web_key for
 * keyboard input, web_sound from the audio callback.  The file service's
 * disk directory is /disk, an IndexedDB-backed folder the page mounts and
 * fills with the example programs on first use. */
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "front.h"

static cpu_t cpu;
static uint32_t px[SCREEN_W * SCREEN_H];
static int ready;

/* Boot the image for variant "A" or "B" from /rom/basic.<v>.bin. */
EMSCRIPTEN_KEEPALIVE int web_boot(const char *vname)
{
    const variant_t *v = find_variant(vname);
    if (!v)
        return -1;
    if (ready)
        cpu_free(&cpu);
    tcfg_t cfg = { 2048, 16, 1, 0, 2, 1, 2, 56, 4 };
    cpu_init(&cpu, v, &cfg);
    cpu.dev.con_out = NULL;
    cpu.dev.con_stdin = 0;      /* stdin would be window.prompt() here */
    snprintf(cpu.dev.disk, sizeof cpu.dev.disk, "/disk");
    char path[64];
    snprintf(path, sizeof path, "/rom/basic.%s.bin", vname);
    if (!load_image(&cpu, path, 0x1000))
        return -1;
    cpu.pc = 0x1000;
    ready = 1;
    return 0;
}

/* Run up to `cycles` CPU cycles, then render.  Returns the RGBA frame
 * (1280x720), or 0 once the program has stopped. */
EMSCRIPTEN_KEEPALIVE uint32_t *web_frame(double cycles)
{
    if (!ready)
        return 0;
    uint64_t end = cpu.st.cycles + (uint64_t)cycles;
    while (!cpu.halted && cpu.st.cycles < end)
        cpu_step(&cpu);
    video_render(&cpu, px, 0, (dev_frame(cpu.st.cycles) / 16) % 2 == 0);
    /* 0x00RRGGBB -> canvas byte order R,G,B,A */
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
        uint32_t p = px[i];
        px[i] = 0xFF000000u | (p & 0xFF) << 16 | (p & 0xFF00) | (p >> 16 & 0xFF);
    }
    return px;
}

EMSCRIPTEN_KEEPALIVE int web_halted(void) { return cpu.halted; }
EMSCRIPTEN_KEEPALIVE const char *web_halt_reason(void) { return cpu.halt_reason; }
EMSCRIPTEN_KEEPALIVE double web_cycles(void) { return (double)cpu.st.cycles; }
EMSCRIPTEN_KEEPALIVE double web_insts(void) { return (double)cpu.st.insts; }
EMSCRIPTEN_KEEPALIVE void web_key(int code) { dev_key(&cpu.dev, (uint16_t)code); }
EMSCRIPTEN_KEEPALIVE int web_leds(void) { return cpu.dev.leds; }
EMSCRIPTEN_KEEPALIVE void web_sound(float *out, int n, int rate) { sound_fill(&cpu.dev, out, n, rate); }

/* 1 while a file is open, so the page holds off saving the disk to IndexedDB */
EMSCRIPTEN_KEEPALIVE int web_files_open(void)
{
    for (int i = 0; i < FS_HANDLES; i++)
        if (cpu.dev.fh[i])
            return 1;
    return 0;
}

int main(void) { return 0; }
