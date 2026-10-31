/*
 * Doom on fpga_cpu: the doomgeneric platform layer.
 *
 * Built with a 32-bit int (fcc --int32) and run from simulator expansion
 * RAM (fsim --xram 14): the engine needs several megabytes, more than the
 * base board's 1 MB SRAM. See benchmarks/doom/README.md.
 *
 * Video: doomgeneric's 8-bit mode (CMAP256) with a 640x360 "frame
 * buffer", which is exactly the hi-color bitmap's layout, so the engine's
 * picture is copied straight into video memory, centered, and Doom's
 * palette goes into the hardware palette. The picture is 320x200, or the
 * full 640x350 when built with -DDOOM_HIRES (src/i_video.h).
 * Input: the keyboard queue, with key releases switched on (KBD_CTRL bit 0:
 * a release arrives as its code | 0x8000), so held keys stay held. Ctrl,
 * Shift and Alt (fire, run, strafe) come from the live modifier state.
 * Time: the CPU cycle counter at 50 MHz.
 * Files: the C runtime's stdio on the file service (8.3 names).
 *
 * GPL v2, like the rest of doomgeneric (see ../LICENSE).
 */
#include <fcpu.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_video.h"

#define VID_CTRL     IO16(0xF00200)
#define VID_BACKDROP IO16(0xF00202)
#define VID_BMP_LO   IO16(0xF00208)
#define VID_BMP_HI   IO16(0xF0020A)
#define PAL_INDEX    IO16(0xF00210)
#define PAL_DATA     IO16(0xF00212)
#define KBD_CTRL     IO16(0xF00104)
#define KBD_RELEASE  0x8000
#define BMP_ADDR     0x0C3C00UL
#define SCREEN_W     640
#define SCREEN_H     360

/* ---- time ---- */

static uint32_t cycles32(void)
{
    uint16_t hi, lo;
    do {
        hi = IO16(0xF00006);
        lo = IO16(0xF00004);
    } while (IO16(0xF00006) != hi);
    return (uint32_t)hi << 16 | lo;
}

/* The 32-bit counter wraps every 86 s; count the wraps. Called often
 * enough (every frame) that no wrap is missed. */
static uint64_t cycles64(void)
{
    static uint32_t prev, wraps;
    uint32_t now = cycles32();
    if (now < prev)
        wraps++;
    prev = now;
    return (uint64_t)wraps << 32 | now;
}

uint32_t DG_GetTicksMs(void) { return (uint32_t)(cycles64() / 50000u); }

void DG_SleepMs(uint32_t ms)
{
    uint32_t end = DG_GetTicksMs() + ms;
    while ((int32_t)(DG_GetTicksMs() - end) < 0)
        ;
}

/* ---- video ---- */

void DG_Init(void)
{
    VID_BMP_LO = (uint16_t)BMP_ADDR;
    VID_BMP_HI = (uint16_t)(BMP_ADDR >> 16);
    memset((void *)BMP_ADDR, 0, (size_t)SCREEN_W * SCREEN_H);
    VID_BACKDROP = 0;  /* palette index 0 is transparent: let black show through */
    VID_CTRL = 0x06;   /* hi-color bitmap on, text off */
    KBD_CTRL = 1;      /* queue key releases too */
    /* Draw straight into video memory, centered vertically: 80 lines down
     * for 320x200, 5 for 640x350. (doomgeneric_Create's own buffer goes
     * unused.) */
#if SCREENHEIGHT <= SCREEN_H
    DG_ScreenBuffer = (pixel_t *)(BMP_ADDR + (unsigned long)((SCREEN_H - SCREENHEIGHT) / 2) * SCREEN_W);
#endif  /* else (DOOM_640x480) doomgeneric's own buffer, not shown */
}

void DG_DrawFrame(void)
{
    if (palette_changed) {
        int i;
        PAL_INDEX = 0;
        for (i = 0; i < 256; i++)
            PAL_DATA = (uint16_t)((colors[i].r >> 3) << 11 | (colors[i].g >> 2) << 5 | colors[i].b >> 3);
        palette_changed = false;
    }
    cycles64(); /* keep the wrap count up to date */
}

void DG_SetWindowTitle(const char *title) { (void)title; }

/* ---- keyboard ---- */

static uint16_t mods;

static unsigned char map_key(uint16_t k)
{
    switch (k) {
    case 0x80: return KEY_UPARROW;
    case 0x81: return KEY_DOWNARROW;
    case 0x82: return KEY_LEFTARROW;
    case 0x83: return KEY_RIGHTARROW;
    case 13: return KEY_ENTER;
    case 27: return KEY_ESCAPE;
    case 8: return KEY_BACKSPACE;
    case 9: return KEY_TAB;
    case 0x7F: return KEY_DEL;
    }
    if (k >= 0x90 && k <= 0x9B)
        return (unsigned char)(KEY_F1 + (k - 0x90));
    if (k >= 'A' && k <= 'Z')
        return (unsigned char)(k + 32);
    if (k >= 32 && k < 127)
        return (unsigned char)k;
    return 0;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    static const uint16_t bits[3] = { 2, 1, 4 };   /* ctrl, shift, alt (KBD_MOD) */
    static const unsigned char keys[3] = { KEY_FIRE, KEY_RSHIFT, KEY_RALT };
    uint16_t m = KBD_MOD, k;
    int i;
    for (i = 0; i < 3; i++)
        if ((m ^ mods) & bits[i]) {
            mods ^= bits[i];
            *pressed = (m & bits[i]) != 0;
            *key = keys[i];
            return 1;
        }
    while ((k = KBD_DATA) != 0xFFFF) {
        unsigned char d = map_key(k & ~KBD_RELEASE);
        if (!d)
            continue;
        *pressed = !(k & KBD_RELEASE);
        *key = d;
        return 1;
    }
    return 0;
}

/* ---- POSIX bits the engine calls ---- */

int mkdir(const char *path, mode_t mode)
{
    (void)path;
    (void)mode;
    return 0; /* the file service has no directories */
}

int access(const char *path, int mode)
{
    FILE *f = fopen(path, "rb");
    (void)mode;
    if (!f)
        return -1;
    fclose(f);
    return 0;
}

/* ---- main ----
 * Arguments come from DOOMARGS.TXT on the disk if it exists (the
 * benchmark puts "-timedemo demo1" there), after an -iwad for DOOM1.WAD
 * (the shareware IWAD, if you have it) or else DOOM.WAD (Freedoom). */

static char argbuf[256];

int main(void)
{
    static char *argv[24];
    int argc = 0;
    FILE *f;
    argv[argc++] = "doom";
    argv[argc++] = "-iwad";
    argv[argc++] = access("DOOM1.WAD", 0) == 0 ? "DOOM1.WAD" : "DOOM.WAD";
    if ((f = fopen("DOOMARGS.TXT", "rb")) != NULL) {
        size_t n = fread(argbuf, 1, sizeof argbuf - 1, f);
        char *p = argbuf;
        fclose(f);
        argbuf[n] = 0;
        while (argc < 23) {
            while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
                p++;
            if (!*p)
                break;
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
                p++;
            if (*p)
                *p++ = 0;
        }
    }
    argv[argc] = NULL;
    doomgeneric_Create(argc, argv);
    for (;;)
        doomgeneric_Tick();
}
