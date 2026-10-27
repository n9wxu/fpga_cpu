/* Host HAL: runs BASIC in a terminal on the development machine.
 *
 * The target's SRAM and I/O registers are emulated in memory, so the
 * shared console and graphics code write the same video layout as on the
 * target. Files live in a host directory (BASIC_DISK, default ./disk).
 *
 * Environment:
 *   BASIC_DISK=dir      file directory
 *   BASIC_PPM=file      on exit, write the bitmap + sprite layers as a PPM
 *   BASIC_FASTFRAMES=1  the frame counter advances on every read (tests)
 *   BASIC_ARENA=bytes   a smaller workspace (garbage-collector tests)
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "basic.h"

#define SRAM 0x100000UL

static uint8_t sram[SRAM + 4];
static uint16_t ioreg[0x8000];
static uint16_t palette[256];
static uint8_t arena_mem[256 * 1024] __attribute__((aligned(16)));
static volatile sig_atomic_t got_int;
static int fast_frames, echo_input;
static uint32_t frame_count;
static const char *disk = "disk";
static FILE *files[8];
static DIR *dir;

static void on_int(int s)
{
    (void)s;
    got_int = 1;
}

void hal_init(void)
{
    const char *e = getenv("BASIC_DISK");
    if (e && *e)
        disk = e;
    fast_frames = getenv("BASIC_FASTFRAMES") != NULL;
    echo_input = !isatty(0);
    signal(SIGINT, on_int);
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
}

static void pal_rgb(uint8_t i, uint8_t *px)
{
    uint16_t c = palette[i];
    px[0] = (uint8_t)((c >> 11) << 3);
    px[1] = (uint8_t)(((c >> 5) & 63) << 2);
    px[2] = (uint8_t)((c & 31) << 3);
}

static void write_ppm(const char *path);

void hal_exit(int code)
{
    const char *p = getenv("BASIC_PPM");
    fflush(stdout);
    if (p && *p)
        write_ppm(p);
    exit(code);
}

uint8_t *hal_arena(uint32_t *size)
{
    const char *e = getenv("BASIC_ARENA");
    *size = sizeof arena_mem;
    if (e && atol(e) > 1024 && (size_t)atol(e) < sizeof arena_mem)
        *size = (uint32_t)atol(e);
    return arena_mem;
}

void hal_serial_putc(char c) { putchar(c); }

int hal_getkey(int wait)
{
    int c;
    if (!wait) {
        struct pollfd p = { 0, POLLIN, 0 };
        if (poll(&p, 1, 0) <= 0 || !(p.revents & (POLLIN | POLLHUP)))
            return -1;
    }
    fflush(stdout);
    c = getchar();
    if (c == EOF)
        return wait ? HAL_EOF : -1;
    return c == '\n' ? 13 : c;
}

int hal_readline(char *buf, int max)
{
    int n;
    fflush(stdout);
    if (!fgets(buf, max, stdin))
        return HAL_EOF;
    n = (int)strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = 0;
    if (echo_input) {
        con_puts(buf);
        con_putc('\n');
    }
    return n;
}

int hal_break(void)
{
    if (got_int) {
        got_int = 0;
        return 1;
    }
    return 0;
}

uint8_t *hal_mem(uint32_t addr) { return sram + (addr & (SRAM - 1)); }

uint32_t hal_frame(void)
{
    struct timespec t;
    if (fast_frames)
        return ++frame_count;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 60u + (uint64_t)t.tv_nsec * 60u / 1000000000u);
}

static int is_io(uint32_t a) { return a >= 0xF00000UL && a < 0xF10000UL; }

uint16_t hal_peek16(uint32_t a)
{
    if (a < SRAM)
        return (uint16_t)(sram[a] | (sram[a + 1] << 8));
    if (is_io(a)) {
        switch (a) {
        case 0xF0000AUL: return (uint16_t)hal_frame();
        case 0xF00008UL:
        case 0xF00100UL: return 0xFFFF;
        }
        return ioreg[(a - 0xF00000UL) >> 1];
    }
    return 0;
}

uint8_t hal_peek8(uint32_t a)
{
    if (a < SRAM)
        return sram[a];
    return (uint8_t)(hal_peek16(a & ~1UL) >> ((a & 1) * 8));
}

void hal_poke16(uint32_t a, uint16_t v)
{
    if (a < SRAM) {
        sram[a] = (uint8_t)v;
        sram[a + 1] = (uint8_t)(v >> 8);
        return;
    }
    if (!is_io(a))
        return;
    if (a == PAL_DATA) {
        palette[ioreg[(PAL_INDEX - 0xF00000UL) >> 1] & 255] = v;
        ioreg[(PAL_INDEX - 0xF00000UL) >> 1]++;
        return;
    }
    if (a == CON_OUT) {
        putchar((char)v);
        return;
    }
    if (a == 0xF00002UL)
        hal_exit(v);
    ioreg[(a - 0xF00000UL) >> 1] = v;
}

void hal_poke8(uint32_t a, uint8_t v)
{
    if (a < SRAM) {
        sram[a] = v;
        return;
    }
    if (is_io(a)) {
        uint16_t w = hal_peek16(a & ~1UL);
        if (a & 1)
            w = (uint16_t)((w & 0x00FF) | (v << 8));
        else
            w = (uint16_t)((w & 0xFF00) | v);
        hal_poke16(a & ~1UL, w);
    }
}

void hal_call(uint32_t addr) { (void)addr; }

/* ---- files ---- */

/* Names are compared without regard to case, as on the SD card. */
static int path_for(const char *name, char *out, size_t n)
{
    DIR *d;
    struct dirent *e;
    if (!name[0] || strchr(name, '/'))
        return -5;
    snprintf(out, n, "%s/%s", disk, name);
    if ((d = opendir(disk)) != NULL) {
        while ((e = readdir(d)) != NULL)
            if (!strcasecmp(e->d_name, name)) {
                snprintf(out, n, "%s/%s", disk, e->d_name);
                break;
            }
        closedir(d);
    }
    return 0;
}

int hal_fopen(const char *name, int mode)
{
    char p[512];
    int h;
    if (path_for(name, p, sizeof p) < 0)
        return -5;
    for (h = 0; h < 8 && files[h]; h++)
        ;
    if (h == 8)
        return -3;
    if (mode)
        mkdir(disk, 0777);
    files[h] = fopen(p, mode == 0 ? "rb" : mode == 1 ? "wb" : "ab");
    if (!files[h])
        return mode ? -4 : -1;
    return h;
}

int hal_fread(int h, void *buf, int len)
{
    if (h < 0 || h >= 8 || !files[h])
        return -2;
    return (int)fread(buf, 1, (size_t)len, files[h]);
}

int hal_fwrite(int h, const void *buf, int len)
{
    if (h < 0 || h >= 8 || !files[h])
        return -2;
    return (int)fwrite(buf, 1, (size_t)len, files[h]);
}

int hal_fclose(int h)
{
    if (h < 0 || h >= 8 || !files[h])
        return -2;
    fclose(files[h]);
    files[h] = NULL;
    return 0;
}

int hal_fdelete(const char *name)
{
    char p[512];
    if (path_for(name, p, sizeof p) < 0)
        return -5;
    return unlink(p) == 0 ? 0 : -1;
}

int hal_dir_open(void)
{
    if (dir)
        closedir(dir);
    dir = opendir(disk);
    return 0;
}

int hal_dir_next(char *name, uint32_t *size)
{
    struct dirent *e;
    if (!dir)
        return 0;
    while ((e = readdir(dir)) != NULL) {
        char p[512];
        struct stat st;
        if (e->d_name[0] == '.' || strlen(e->d_name) > 12)
            continue;
        snprintf(p, sizeof p, "%s/%s", disk, e->d_name);
        if (stat(p, &st) || !S_ISREG(st.st_mode))
            continue;
        strcpy(name, e->d_name);
        *size = (uint32_t)st.st_size;
        return 1;
    }
    closedir(dir);
    dir = NULL;
    return 0;
}

/* ---- PPM of the backdrop, bitmap and sprite layers ---- */

static void rgb8(int c, uint8_t *o)
{
    o[0] = (c & 4) ? 255 : 0;
    o[1] = (c & 2) ? 255 : 0;
    o[2] = (c & 1) ? 255 : 0;
}

static void write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint16_t ctrl = ioreg[(VID_CTRL - 0xF00000UL) >> 1];
    uint32_t bmpa = ioreg[(VID_BMP - 0xF00000UL) >> 1] | (uint32_t)ioreg[(VID_BMP + 2 - 0xF00000UL) >> 1] << 16;
    uint32_t spra = ioreg[(VID_SPR - 0xF00000UL) >> 1] | (uint32_t)ioreg[(VID_SPR + 2 - 0xF00000UL) >> 1] << 16;
    int bd = ioreg[(VID_BACKDROP - 0xF00000UL) >> 1] & 7, x, y;
    if (!f)
        return;
    if (ctrl & VC_SPRBUF)
        spra += (uint32_t)((ctrl & VC_SPR8) ? SPR_SIZE : SPR_SIZE / 2u);
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (y = 0; y < SCR_H; y++)
        for (x = 0; x < SCR_W; x++) {
            uint8_t px[3];
            rgb8(bd, px);
            if (ctrl & VC_BMP) {
                if (ctrl & VC_HICOL) {
                    uint8_t i = sram[(bmpa + (uint32_t)y * SCR_W + (uint32_t)x) & (SRAM - 1)];
                    if (i)
                        pal_rgb(i, px);
                } else {
                    uint8_t b = sram[(bmpa + (uint32_t)y * 320u + (uint32_t)(x >> 1)) & (SRAM - 1)];
                    int n = (x & 1) ? (b & 15) : (b >> 4);
                    if (!(n & 8))
                        rgb8(n, px);
                }
            }
            if ((ctrl & VC_SPR) && (ctrl & VC_SPR8)) {
                uint8_t i = sram[(spra + (uint32_t)y * SCR_W + (uint32_t)x) & (SRAM - 1)];
                if (i)
                    pal_rgb(i, px);
            } else if (ctrl & VC_SPR) {
                uint8_t b = sram[(spra + (uint32_t)y * 320u + (uint32_t)(x >> 1)) & (SRAM - 1)];
                int n = (x & 1) ? (b & 15) : (b >> 4);
                if (!(n & 8))
                    rgb8(n, px);
            }
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *file = NULL;
    int quiet = 0, i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-q"))
            quiet = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "usage: basic-host [-q] [program.bas]\n");
            return 2;
        } else
            file = argv[i];
    }
    hal_init();
    if (file) {
        /* Run a host file: point the disk at its directory. */
        static char d[512];
        const char *slash = strrchr(file, '/');
        if (slash) {
            snprintf(d, sizeof d, "%.*s", (int)(slash - file), file);
            disk = d;
            file = slash + 1;
        } else
            disk = ".";
    }
    basic_main(file, quiet);
    hal_exit(0);
    return 0;
}
