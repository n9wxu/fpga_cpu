/*
 * Doom on the development machine, headless: the reference for the
 * fpga_cpu run (same engine, same WAD, same timedemo, so the same number
 * of gametics). DG_DrawFrame keeps the last frame and writes it as
 * LASTFRAME.PPM at exit. GPL v2 (see ../LICENSE).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doomgeneric.h"
#include "i_video.h"

static uint8_t rgb[256][3];

static void save(void)
{
    FILE *f = fopen("LASTFRAME.IDX", "wb");
    int x, y;
    if (f) {
        for (y = 0; y < 200; y++)
            fwrite(DG_ScreenBuffer + y * DOOMGENERIC_RESX + 160, 1, 320, f);
        fclose(f);
    }
    f = fopen("LASTFRAME.PPM", "wb");
    if (!f)
        return;
    fprintf(f, "P6\n320 200\n255\n");
    for (y = 0; y < 200; y++)
        for (x = 0; x < 320; x++)
            fwrite(rgb[DG_ScreenBuffer[y * DOOMGENERIC_RESX + 160 + x]], 1, 3, f);
    fclose(f);
}

void DG_Init(void) { atexit(save); }

void DG_DrawFrame(void)
{
    int i;
    if (palette_changed) {
        for (i = 0; i < 256; i++) {
            rgb[i][0] = colors[i].r;
            rgb[i][1] = colors[i].g;
            rgb[i][2] = colors[i].b;
        }
        palette_changed = false;
    }
}

uint32_t DG_GetTicksMs(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

void DG_SleepMs(uint32_t ms) { (void)ms; }
int DG_GetKey(int *pressed, unsigned char *key) { (void)pressed; (void)key; return 0; }
void DG_SetWindowTitle(const char *title) { (void)title; }

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    for (;;)
        doomgeneric_Tick();
}
