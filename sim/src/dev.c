/* fpga_cpu simulator: I/O devices.  The register map is docs/io.md. */
#define _POSIX_C_SOURCE 200809L /* strdup under -std=c11 on glibc */
#include "cpu.h"

#include <ctype.h>
#include <dirent.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

void dev_init(iodev_t *d)
{
    memset(d, 0, sizeof *d);
    d->con_out = stdout;
    d->con_stdin = 1;
    snprintf(d->disk, sizeof d->disk, "disk");
    dev_reset(d);
}

static void dir_free(iodev_t *d)
{
    for (int i = 0; i < d->dir_n; i++)
        free(d->dir_list[i]);
    free(d->dir_list);
    d->dir_list = NULL;
    d->dir_n = d->dir_pos = 0;
}

void dev_reset(iodev_t *d)
{
    memset(d->vid, 0, sizeof d->vid);
    memset(d->pal, 0, sizeof d->pal);
    for (int v = 0; v < 3; v++) {
        for (int r = 0; r < 4; r++)
            d->snd[v][r] = 0;
        d->snd_lfsr[v] = 0xACE1u + (uint32_t)v;
    }
    for (int i = 0; i < FS_HANDLES; i++)
        if (d->fh[i]) {
            fclose(d->fh[i]);
            d->fh[i] = NULL;
        }
    dir_free(d);
    d->fs_handle = d->fs_mode = d->fs_len = 0;
    d->fs_result = 0;
    d->fs_name = d->fs_buf = d->fs_size = d->fs_pos = 0;
    d->leds = d->seg7 = d->seg7_ctrl = 0;
    d->kbd_head = d->kbd_tail = 0;
    d->kbd_ctrl = 0;
}

void dev_free(iodev_t *d)
{
    dev_reset(d);
}

void dev_key(iodev_t *d, uint16_t code)
{
    unsigned n = (d->kbd_tail + 1) % KBD_QLEN;
    if (n == d->kbd_head)
        return; /* full: drop, like a real keyboard buffer */
    d->kbd_q[d->kbd_tail] = code;
    d->kbd_tail = n;
}

void dev_key_up(iodev_t *d, uint16_t code)
{
    if (d->kbd_ctrl & 1)
        dev_key(d, (uint16_t)(code | KBD_RELEASE));
}

void dev_con_in(iodev_t *d, uint16_t ch)
{
    unsigned n = (d->con_tail + 1) % CON_QLEN;
    if (n == d->con_head)
        return;
    d->con_q[d->con_tail] = ch;
    d->con_tail = n;
}

/* Frame timing.  A line is 50e6 / (60 * 750) = 1111.1 cycles, so
 * lines = cycles * 9 / 10000. */
static uint64_t total_lines(uint64_t cycles) { return cycles * 9 / 10000; }
uint32_t dev_line(uint64_t cycles) { return (uint32_t)(total_lines(cycles) % FRAME_LINES); }
uint32_t dev_frame(uint64_t cycles)
{
    return (uint32_t)((total_lines(cycles) + (FRAME_LINES - VISIBLE_LINES)) / FRAME_LINES);
}

/* ---- file service ---- */

static int valid_name(const char *n)
{
    const char *dot = strchr(n, '.');
    size_t len = strlen(n), base = dot ? (size_t)(dot - n) : len;
    if (len == 0 || base == 0 || base > 8 || (dot && (len - base - 1 > 3 || strchr(dot + 1, '.'))))
        return 0;
    for (const char *p = n; *p; p++)
        if (!(isalnum((unsigned char)*p) || strchr("._-$~!#%&()@^{}'`", *p)))
            return 0;
    return 1;
}

/* Read the NUL-terminated name at FS_NAME; -5 if it is not a valid 8.3 name. */
static int fs_getname(cpu_t *c, char *out, size_t n)
{
    size_t i = 0;
    uint32_t a = c->dev.fs_name & 0xFFFFFF;
    for (; i < n - 1; i++, a++) {
        if (a >= c->ram_size)
            return -5;
        if (!(out[i] = (char)c->sram[a]))
            break;
    }
    out[i] = 0;
    return valid_name(out) ? 0 : -5;
}

/* Host path for a name, matched without regard to case.  New files are
 * created in upper case, the way FAT shows 8.3 names. */
static void fs_path(iodev_t *d, const char *name, char *path, size_t n)
{
    DIR *dir = opendir(d->disk);
    if (dir) {
        struct dirent *e;
        while ((e = readdir(dir)))
            if (!strcasecmp(e->d_name, name)) {
                snprintf(path, n, "%s/%s", d->disk, e->d_name);
                closedir(dir);
                return;
            }
        closedir(dir);
    }
    char up[16];
    size_t i = 0;
    for (; name[i] && i < sizeof up - 1; i++)
        up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    snprintf(path, n, "%s/%s", d->disk, up);
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int16_t fs_command(cpu_t *c, int cmd)
{
    iodev_t *d = &c->dev;
    char name[16], path[1200];
    int h = d->fs_handle;
    switch (cmd) {
    case 1: { /* OPEN */
        if (fs_getname(c, name, sizeof name))
            return -5;
        static const char *const modes[] = { "rb", "wb", "ab" };
        if (d->fs_mode > 2)
            return -4;
        int slot = -1;
        for (int i = 0; i < FS_HANDLES && slot < 0; i++)
            if (!d->fh[i])
                slot = i;
        if (slot < 0)
            return -3;
        mkdir(d->disk, 0755);
        fs_path(d, name, path, sizeof path);
        if (!(d->fh[slot] = fopen(path, modes[d->fs_mode])))
            return d->fs_mode ? -4 : -1;
        return (int16_t)slot;
    }
    case 2: /* CLOSE */
        if (h >= FS_HANDLES || !d->fh[h])
            return -2;
        fclose(d->fh[h]);
        d->fh[h] = NULL;
        return 0;
    case 3: case 4: { /* READ, WRITE */
        if (h >= FS_HANDLES || !d->fh[h])
            return -2;
        uint32_t a = d->fs_buf & 0xFFFFFF, n = d->fs_len;
        if (a >= c->ram_size || a + n > c->ram_size)
            return -4;
        size_t got;
        if (cmd == 3) {
            got = fread(c->sram + a, 1, n, d->fh[h]);
            cpu_mem_written(c, a, (uint32_t)got);
        } else {
            got = fwrite(c->sram + a, 1, n, d->fh[h]);
            fflush(d->fh[h]);
        }
        if (got < n && ferror(d->fh[h]))
            return -4;
        return (int16_t)got;
    }
    case 5: /* DELETE */
        if (fs_getname(c, name, sizeof name))
            return -5;
        fs_path(d, name, path, sizeof path);
        return remove(path) ? -1 : 0;
    case 6: { /* DIR_OPEN */
        dir_free(d);
        DIR *dir = opendir(d->disk);
        if (!dir)
            return 0; /* no disk directory yet: empty listing */
        struct dirent *e;
        while ((e = readdir(dir))) {
            struct stat sb;
            snprintf(path, sizeof path, "%s/%s", d->disk, e->d_name);
            if (!valid_name(e->d_name) || stat(path, &sb) || !S_ISREG(sb.st_mode))
                continue;
            d->dir_list = realloc(d->dir_list, sizeof(char *) * (size_t)(d->dir_n + 1));
            d->dir_list[d->dir_n++] = strdup(e->d_name);
        }
        closedir(dir);
        qsort(d->dir_list, (size_t)d->dir_n, sizeof(char *), cmp_str);
        return 0;
    }
    case 7: { /* DIR_NEXT */
        if (d->dir_pos >= d->dir_n)
            return 0;
        const char *nm = d->dir_list[d->dir_pos++];
        uint32_t a = d->fs_buf & 0xFFFFFF;
        size_t len = strlen(nm);
        if (a + len + 1 > c->ram_size)
            return -4;
        for (size_t i = 0; i <= len; i++)
            c->sram[a + i] = (uint8_t)toupper((unsigned char)nm[i]);
        cpu_mem_written(c, a, (uint32_t)len + 1);
        struct stat sb;
        snprintf(path, sizeof path, "%s/%s", d->disk, nm);
        d->fs_size = stat(path, &sb) ? 0 : (uint32_t)sb.st_size;
        return 1;
    }
    case 8: /* SEEK: to FS_POS from the start */
        if (h >= FS_HANDLES || !d->fh[h])
            return -2;
        return fseek(d->fh[h], (long)d->fs_pos, SEEK_SET) ? -4 : 0;
    case 9: { /* SIZE: of an open file, into FS_SIZE */
        if (h >= FS_HANDLES || !d->fh[h])
            return -2;
        struct stat sb;
        fflush(d->fh[h]);
        if (fstat(fileno(d->fh[h]), &sb))
            return -4;
        d->fs_size = (uint32_t)sb.st_size;
        return 0;
    }
    }
    return -4;
}

/* ---- register access ---- */

uint16_t dev_read(cpu_t *c, uint32_t a)
{
    iodev_t *d = &c->dev;
    a &= ~1u;
    switch (a) {
    case IO_CYCLO: return (uint16_t)c->st.cycles;
    case IO_CYCHI: return (uint16_t)(c->st.cycles >> 16);
    case IO_GETC:
        if (d->con_stdin) {
            /* Non-blocking: 0xFFFF when nothing is waiting, 0xFFFE once
             * stdin has ended. */
            if (d->con_eof)
                return 0xFFFE;
            struct pollfd p = {0, POLLIN, 0};
            if (poll(&p, 1, 0) <= 0)
                return 0xFFFF;
            unsigned char ch;
            if (read(0, &ch, 1) != 1) {
                d->con_eof = 1;
                return 0xFFFE;
            }
            return ch;
        }
        if (d->con_head == d->con_tail)
            return 0xFFFF;
        {
            uint16_t ch = d->con_q[d->con_head];
            d->con_head = (d->con_head + 1) % CON_QLEN;
            return ch;
        }
    case IO_FRAME: return (uint16_t)dev_frame(c->st.cycles);
    case IO_VSTAT: {
        uint32_t line = dev_line(c->st.cycles);
        return (uint16_t)(line << 4 | (line >= VISIBLE_LINES));
    }
    case IO_KBD_DATA:
        if (d->kbd_head == d->kbd_tail)
            return 0xFFFF;
        {
            uint16_t k = d->kbd_q[d->kbd_head];
            d->kbd_head = (d->kbd_head + 1) % KBD_QLEN;
            return k;
        }
    case IO_KBD_MOD: return d->kbd_mod;
    case IO_KBD_CTRL: return d->kbd_ctrl;
    }
    if (a >= IO_VID && a < IO_VID + 2 * V__COUNT) {
        int r = (int)(a - IO_VID) / 2;
        return r == V_PAL_DATA ? 0 : d->vid[r];
    }
    if (a >= IO_SND && a < IO_SND + 24)
        return (a & 7) < 6 ? d->snd[(a - IO_SND) / 8][(a & 7) / 2] : 0;
    switch (a) {
    case IO_FS + 0x02: return 0; /* never busy */
    case IO_FS + 0x04: return d->fs_handle;
    case IO_FS + 0x06: return d->fs_mode;
    case IO_FS + 0x08: return (uint16_t)d->fs_name;
    case IO_FS + 0x0A: return (uint16_t)(d->fs_name >> 16);
    case IO_FS + 0x0C: return (uint16_t)d->fs_buf;
    case IO_FS + 0x0E: return (uint16_t)(d->fs_buf >> 16);
    case IO_FS + 0x10: return d->fs_len;
    case IO_FS + 0x12: return (uint16_t)d->fs_result;
    case IO_FS + 0x14: return (uint16_t)d->fs_size;
    case IO_FS + 0x16: return (uint16_t)(d->fs_size >> 16);
    case IO_FS + 0x18: return (uint16_t)d->fs_pos;
    case IO_FS + 0x1A: return (uint16_t)(d->fs_pos >> 16);
    case IO_BOARD + 0x0: return d->leds;
    case IO_BOARD + 0x2: return d->switches;
    case IO_BOARD + 0x4: return d->buttons;
    case IO_BOARD + 0x6: return d->seg7;
    case IO_BOARD + 0x8: return d->seg7_ctrl;
    }
    return 0;
}

static void set_lo(uint32_t *r, uint32_t v) { *r = (*r & 0xFFFF0000u) | (v & 0xFFFF); }
static void set_hi(uint32_t *r, uint32_t v) { *r = (*r & 0xFFFF) | (v & 0xFFFF) << 16; }

void dev_write(cpu_t *c, uint32_t a, uint32_t v)
{
    iodev_t *d = &c->dev;
    a &= ~1u;
    v &= 0xFFFF;
    switch (a) {
    case IO_CONSOLE:
        if (d->con_out)
            fputc((int)(v & 0xFF), d->con_out);
        if (d->con_hook)
            d->con_hook(d->con_hook_arg, (int)(v & 0xFF));
        return;
    case IO_EXIT:
        c->halted = 1;
        c->exit_code = (int)v;
        snprintf(c->halt_reason, sizeof c->halt_reason, "exit %d", c->exit_code);
        return;
    }
    if (a >= IO_VID && a < IO_VID + 2 * V__COUNT) {
        int r = (int)(a - IO_VID) / 2;
        if (r == V_PAL_DATA) {
            d->pal[d->vid[V_PAL_INDEX] & 0xFF] = (uint16_t)v;
            d->vid[V_PAL_INDEX] = (uint16_t)((d->vid[V_PAL_INDEX] + 1) & 0xFF);
        } else {
            d->vid[r] = (uint16_t)v;
        }
        return;
    }
    if (a >= IO_SND && a < IO_SND + 24) {
        if ((a & 7) < 6)
            d->snd[(a - IO_SND) / 8][(a & 7) / 2] = (uint16_t)v;
        return;
    }
    switch (a) {
    case IO_FS + 0x00: d->fs_result = fs_command(c, (int)v); return;
    case IO_FS + 0x04: d->fs_handle = (uint16_t)v; return;
    case IO_FS + 0x06: d->fs_mode = (uint16_t)v; return;
    case IO_FS + 0x08: set_lo(&d->fs_name, v); return;
    case IO_FS + 0x0A: set_hi(&d->fs_name, v); return;
    case IO_FS + 0x0C: set_lo(&d->fs_buf, v); return;
    case IO_FS + 0x0E: set_hi(&d->fs_buf, v); return;
    case IO_FS + 0x10: d->fs_len = (uint16_t)v; return;
    case IO_FS + 0x18: set_lo(&d->fs_pos, v); return;
    case IO_FS + 0x1A: set_hi(&d->fs_pos, v); return;
    case IO_KBD_CTRL: d->kbd_ctrl = (uint16_t)(v & 1); return;
    case IO_BOARD + 0x0: d->leds = (uint16_t)(v & 0xFF); return;
    case IO_BOARD + 0x6: d->seg7 = (uint16_t)v; return;
    case IO_BOARD + 0x8: d->seg7_ctrl = (uint16_t)(v & 0xFF); return;
    }
}

/* ---- sound ---- */

void sound_fill(iodev_t *d, float *out, int n, int rate)
{
    for (int i = 0; i < n; i++)
        out[i] = 0;
    for (int v = 0; v < 3; v++) {
        uint16_t f = d->snd[v][0], vol = d->snd[v][1] & 15, wave = d->snd[v][2] & 3;
        if (!f || !vol)
            continue;
        double ph = d->snd_phase[v], step = (double)f / rate;
        float amp = 0.25f * (float)vol / 15.0f;
        for (int i = 0; i < n; i++) {
            float s;
            switch (wave) {
            case 0: s = ph < 0.5 ? 1.0f : -1.0f; break;
            case 1: s = (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph); break;
            case 2: s = (float)(2 * ph - 1); break;
            default: s = d->snd_noise[v]; break;
            }
            out[i] += amp * s;
            ph += step;
            if (ph >= 1.0) {
                ph -= (double)(int)ph;
                /* noise: a new random level every period, from a 16-bit LFSR */
                uint32_t l = d->snd_lfsr[v];
                l = (l >> 1) ^ (-(l & 1u) & 0xB400u);
                d->snd_lfsr[v] = l;
                d->snd_noise[v] = (l & 1) ? 1.0f : -1.0f;
            }
        }
        d->snd_phase[v] = ph;
    }
}
