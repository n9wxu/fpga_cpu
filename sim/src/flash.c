/* The Tang Nano 20K's SPI flash and its port (docs/io.md "Flash (F00900)"), and the boot
 * copier's job for fsim --rom (docs/rom.md): an 8 MB XT25F64B with READ 03, RDID 9F, RDSR 05,
 * WREN 06, WRDI 04, PP 02, SE 20 (4 KB), BE C7. Programming is instant (WIP never shows). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

#define FLASH_SIZE 0x800000u
#define ROM_OFS    0x400000u

static uint8_t *fl;          /* NULL: no flash (reads 0) */
static const char *fl_path;  /* the image, written back at exit when changed (--rom-save) */
static int fl_save, fl_dirty, fl_booted;
static int sel, n;           /* CS low; bytes in this transaction */
static uint8_t cmd, rx, wel;
static uint32_t ad;

int flash_handles(uint32_t a) { return (a & ~3u) == IO_FLASH; }

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* load the image (flash bytes from 0x400000) and do the copier's job: the body at its load
 * address, through *mem (the CPU's memory); returns the entry, or 0xFFFFFFFF without a ROM */
uint32_t flash_load(cpu_t *c, const char *path, int save)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 0xFFFFFFFE; }
    fl = malloc(FLASH_SIZE);
    memset(fl, 0xFF, FLASH_SIZE);
    size_t got = fread(fl + ROM_OFS, 1, FLASH_SIZE - ROM_OFS, f);
    fclose(f);
    fl_path = path; fl_save = save;
    if (got < 32 || memcmp(fl + ROM_OFS, "FCRM", 4)) {
        fprintf(stderr, "%s: no ROM header (FCRM)\n", path);
        return 0xFFFFFFFE;
    }
    uint32_t len = le32(fl + ROM_OFS + 4), load = le32(fl + ROM_OFS + 8), entry = le32(fl + ROM_OFS + 12);
    for (uint32_t i = 0; i < len; i++)
        cpu_poke8(c, (load + i) & 0xFFFFFF, fl[ROM_OFS + 32 + i]);   /* RAM or the ROM region */
    cpu_mem_written(c, load, len);
    fl_booted = 1;
    return entry & 0xFFFFFF;
}

void flash_exit(void)
{
    if (fl && fl_save && fl_dirty) {
        FILE *f = fopen(fl_path, "wb");
        if (!f) { perror(fl_path); return; }
        uint32_t end = FLASH_SIZE;
        while (end > ROM_OFS && fl[end - 1] == 0xFF) end--;
        fwrite(fl + ROM_OFS, 1, end - ROM_OFS, f);
        fclose(f);
    }
}

uint16_t flash_read(cpu_t *c, uint32_t a)
{
    (void)c;
    if (a & 2) return (uint16_t)(fl_booted << 1);   /* never busy */
    return rx;
}

static void byte(uint8_t b)
{
    uint8_t out = 0xFF;
    int nb = n;                                /* this byte's place in the transaction */
    if (n == 0) {
        cmd = b;
        if (b == 0x06) wel = 1;
        if (b == 0x04) wel = 0;
        if (b == 0xC7 && wel && fl) { memset(fl, 0xFF, FLASH_SIZE); wel = 0; fl_dirty = 1; }
    } else if (n <= 3) {
        ad = (ad << 8 | b) & 0xFFFFFF;
    } else if (cmd == 0x02 && wel && fl) {
        fl[ad] &= b;
        ad = (ad & ~0xFFu) | ((ad + 1) & 0xFFu);
        fl_dirty = 1;
    }
    n++;
    switch (cmd) {
    case 0x03: if (nb >= 4 && fl) out = fl[(ad + (uint32_t)(nb - 4)) & (FLASH_SIZE - 1)]; break;
    case 0x9F: out = nb == 1 ? 0x0B : nb == 2 ? 0x40 : nb == 3 ? 0x17 : 0xFF; break;
    case 0x05: if (nb >= 1) out = (uint8_t)(wel << 1); break;
    }
    rx = out;
}

/* each transfer returns what the flash sent during it (as the hardware's): after READ and its
 * three address bytes, each dummy byte sent brings one data byte */
void flash_write(cpu_t *c, uint32_t a, uint16_t v)
{
    (void)c;
    if (a & 2) {
        int s = v & 1;
        if (sel && !s) {                       /* deselect: the command ends */
            if (cmd == 0x20 && n >= 4 && wel && fl) {
                memset(fl + (ad & ~0xFFFu & (FLASH_SIZE - 1)), 0xFF, 4096);
                fl_dirty = 1;
            }
            if ((cmd == 0x02 && n > 4) || cmd == 0x20) wel = 0;
        }
        if (!sel && s) n = 0;
        sel = s;
        return;
    }
    if (!(a & 1) && sel) byte((uint8_t)v);
    else if (!(a & 1)) rx = 0xFF;
}
