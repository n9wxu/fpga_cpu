/* fpga_cpu simulator: WebAssembly front end.
 *
 * Runs BASIC or Doom in the browser.  The page (index.html) calls the
 * web_* functions below: web_run and web_render once per animation frame,
 * web_key / web_key_up / web_mod for keyboard input, web_sound from the
 * audio callback.  BASIC's disk directory is /disk, an IndexedDB-backed
 * folder the page mounts and fills with the example programs on first use;
 * Doom's is /doom, with the WAD the page downloads. */
#include <emscripten.h>
#include <stdio.h>
#include <string.h>

#include "front.h"

static cpu_t cpu;
static uint32_t px[SCREEN_W * SCREEN_H];
static int ready;

/* Boot an image: load it at `load` and start there. xram_mb > 0 adds the
 * simulator's expansion RAM (Doom needs 14 MB). disk is the file service's
 * folder. */
EMSCRIPTEN_KEEPALIVE int web_boot_image(const char *vname, const char *path, int load, int xram_mb,
                                        const char *disk)
{
    const variant_t *v = find_variant(vname);
    if (!v)
        return -1;
    if (ready)
        cpu_free(&cpu);
    ready = 0;
    tcfg_t cfg = { 2048, 16, 1, 0, 2, 1, 2, 56, 4, DUAL_OFF, 0, (uint32_t)xram_mb };
    cpu_init(&cpu, v, &cfg);
    cpu.dev.con_out = NULL;
    cpu.dev.con_stdin = 0;      /* stdin would be window.prompt() here */
    snprintf(cpu.dev.disk, sizeof cpu.dev.disk, "%s", disk);
    if (!load_image(&cpu, path, (uint32_t)load))
        return -1;
    cpu.pc = (uint32_t)load;
    ready = 1;
    return 0;
}

/* Boot BASIC for variant "A" or "B" from /rom/basic.<v>.bin. */
EMSCRIPTEN_KEEPALIVE int web_boot(const char *vname)
{
    char path[64];
    snprintf(path, sizeof path, "/rom/basic.%s.bin", vname);
    return web_boot_image(vname, path, 0x1000, 0, "/disk");
}

/* Run up to `cycles` CPU cycles (fewer if the program stops). */
EMSCRIPTEN_KEEPALIVE void web_run(double cycles)
{
    if (!ready)
        return;
    uint64_t end = cpu.st.cycles + (uint64_t)cycles;
    while (!cpu.halted && cpu.st.cycles < end)
        cpu_step(&cpu);
}

/* Render the display and return it as RGBA (1280x720), or 0 before boot.
 * true_color 0 shows the hi-color bitmap as the board's 3-bit VGA does
 * (2x2 dither), 1 shows the full 8-bit indexed palette. */
EMSCRIPTEN_KEEPALIVE uint32_t *web_render(int true_color)
{
    if (!ready)
        return 0;
    video_render(&cpu, px, true_color, (dev_frame(cpu.st.cycles) / 16) % 2 == 0);
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
/* queued only when the program turned releases on (KBD_CTRL bit 0) */
EMSCRIPTEN_KEEPALIVE void web_key_up(int code) { dev_key_up(&cpu.dev, (uint16_t)code); }
/* live modifiers (KBD_MOD): bit 0 shift, 1 ctrl, 2 alt */
EMSCRIPTEN_KEEPALIVE void web_mod(int m) { cpu.dev.kbd_mod = (uint16_t)m; }
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
