/* fpga_cpu simulator: WebAssembly front end.
 *
 * Runs the BASIC image in the browser.  The page (index.html) calls the
 * web_* functions below: web_run and web_render once per animation frame, web_key for
 * keyboard input, web_sound from the audio callback.  The file service's
 * disk directory is /disk, an IndexedDB-backed folder the page mounts and
 * fills with the example programs on first use.
 *
 * The debugger panels use the same run control as fsim-gui (dbg.c).  State
 * goes to the page as JSON (web_state, web_disasm) or text (web_mem). */
#include <emscripten.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "front.h"

static cpu_t cpu;
static dbg_t dbg;
static uint32_t px[SCREEN_W * SCREEN_H];
static int ready;
static char booted[4];

/* serial console output since the page last asked for it */
static char serial[8192];
static size_t serial_len;

static void serial_hook(void *arg, int ch)
{
    if (ch == '\r')
        return;
    if (serial_len == sizeof serial - 1) {
        memmove(serial, serial + sizeof serial / 2, sizeof serial / 2);
        serial_len -= sizeof serial / 2;
    }
    serial[serial_len++] = (char)ch;
}

/* ---- JSON/text output buffer (valid until the next call) ---- */

static char out[65536];
static size_t out_len;

static void put(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out + out_len, sizeof out - out_len, fmt, ap);
    va_end(ap);
    if (n > 0)
        out_len += (size_t)n < sizeof out - out_len ? (size_t)n : sizeof out - out_len - 1;
}

/* a JSON string */
static void put_str(const char *s)
{
    put("\"");
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\')
            put("\\%c", ch);
        else if (ch < 32 || ch >= 127)
            put("\\u%04x", ch);
        else
            put("%c", ch);
    }
    put("\"");
}

/* Boot the image for variant "A" or "B" from /rom/basic.<v>.bin, with its
 * symbols.  Breakpoints survive a reset but not a change of variant. */
EMSCRIPTEN_KEEPALIVE int web_boot(const char *vname)
{
    const variant_t *v = find_variant(vname);
    if (!v)
        return -1;
    if (ready)
        cpu_free(&cpu);
    else
        dbg_init(&dbg, &cpu);
    tcfg_t cfg = { 2048, 16, 1, 0, 2, 1, 2, 56, 4 };
    cpu_init(&cpu, v, &cfg);
    cpu.dev.con_out = NULL;
    cpu.dev.con_stdin = 0;      /* stdin would be window.prompt() here */
    cpu.dev.con_hook = serial_hook;
    snprintf(cpu.dev.disk, sizeof cpu.dev.disk, "/disk");
    char path[64];
    snprintf(path, sizeof path, "/rom/basic.%s.bin", vname);
    if (!load_image(&cpu, path, 0x1000))
        return -1;
    cpu.pc = 0x1000;
    if (strcmp(booted, vname)) {
        dbg_clear_bps(&dbg);
        sym_free();
        snprintf(path, sizeof path, "/rom/basic.%s.sym", vname);
        sym_load(path);
        snprintf(booted, sizeof booted, "%s", vname);
    }
    if (dbg.mode != DBG_STOPPED)
        dbg.mode = DBG_RUNNING;
    serial_len = 0;
    ready = 1;
    return 0;
}

/* Run up to `cycles` CPU cycles (fewer if the program stops or a breakpoint hits). */
EMSCRIPTEN_KEEPALIVE void web_run(double cycles)
{
    if (ready)
        dbg_run(&dbg, (uint64_t)cycles);
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

/* ---- debugger ---- */

/* 0 stopped, 1 running, 2 stepping over, 3 stepping out */
EMSCRIPTEN_KEEPALIVE int web_mode(void) { return (int)dbg.mode; }
EMSCRIPTEN_KEEPALIVE void web_stop(void) { dbg.mode = DBG_STOPPED; }
EMSCRIPTEN_KEEPALIVE void web_resume(void) { if (!cpu.halted) dbg_resume(&dbg); }
EMSCRIPTEN_KEEPALIVE void web_step(void) { if (!cpu.halted) dbg_step(&dbg); }
EMSCRIPTEN_KEEPALIVE void web_step_over(void) { if (!cpu.halted) dbg_step_over(&dbg); }
EMSCRIPTEN_KEEPALIVE void web_step_out(void) { if (!cpu.halted) dbg_step_out(&dbg); }
EMSCRIPTEN_KEEPALIVE void web_toggle_bp(int a) { dbg_toggle_bp(&dbg, (uint32_t)a); }

/* Edits, allowed only while stopped: r0-r15, 16 = PC, 17 = flags (N Z C V in bits 3..0). */
EMSCRIPTEN_KEEPALIVE void web_set_reg(int i, int v)
{
    if (dbg.mode != DBG_STOPPED)
        return;
    if (i >= 0 && i < 16)
        cpu.r[i] = (uint16_t)v;
    else if (i == 16)
        cpu.pc = (uint32_t)v & 0xFFFFFE;
    else if (i == 17)
        cpu.n = v >> 3 & 1, cpu.z = v >> 2 & 1, cpu.cf = v >> 1 & 1, cpu.v = v & 1;
}

EMSCRIPTEN_KEEPALIVE void web_set_board(int switches, int buttons)
{
    cpu.dev.switches = (uint16_t)switches;
    cpu.dev.buttons = (uint16_t)buttons;
}

EMSCRIPTEN_KEEPALIVE void web_con_in(int ch) { dev_con_in(&cpu.dev, (uint16_t)ch); }

/* Address of a symbol or hex number, -1 if it is neither. */
EMSCRIPTEN_KEEPALIVE int web_find(const char *s)
{
    uint32_t a;
    return dbg_parse_addr(s, &a) ? (int)a : -1;
}

static void put_stat(const char *name, uint64_t v) { put("\"%s\":%llu,", name, (unsigned long long)v); }

/* Registers, flags, statistics, board I/O and new serial output, as JSON. */
EMSCRIPTEN_KEEPALIVE const char *web_state(void)
{
    const stats_t *s = &cpu.st;
    const iodev_t *d = &cpu.dev;
    out_len = 0;
    put("{\"mode\":%d,\"halted\":%d,\"reason\":", (int)dbg.mode, cpu.halted);
    put_str(cpu.halt_reason);
    put(",\"pc\":%u,\"flags\":%d,\"r\":[", cpu.pc, cpu.n << 3 | cpu.z << 2 | cpu.cf << 1 | cpu.v);
    for (int i = 0; i < 16; i++)
        put(i ? ",%u" : "%u", cpu.r[i]);
    put("],\"psym\":[");
    for (int p = 0; p < 8; p++) {
        uint32_t v = (cpu.r[2 * p] | (uint32_t)cpu.r[2 * p + 1] << 16) & 0xFFFFFF, off = 0;
        const char *n = nsyms && v < SRAM_SIZE ? sym_near(v, &off) : NULL;
        if (p)
            put(",");
        if (n && off < 0x10000) {
            put("[");
            put_str(n);
            put(",%u]", off);
        } else {
            put("null");
        }
    }
    put("],\"frame\":%u,\"line\":%u,\"st\":{", dev_frame(s->cycles), dev_line(s->cycles));
    put_stat("cycles", s->cycles);
    put_stat("insts", s->insts);
    put_stat("len16", s->len16);
    put_stat("ic_access", s->ic_access);
    put_stat("ic_hit", s->ic_hit);
    put_stat("ic_miss", s->ic_miss);
    put_stat("sb_hit", s->sb_hit);
    put_stat("sb_partial", s->sb_partial);
    put_stat("branches", s->branches);
    put_stat("taken", s->taken);
    put_stat("st_ifetch", s->st_ifetch);
    put_stat("st_branch", s->st_branch);
    put_stat("st_loaduse", s->st_loaduse);
    put_stat("st_video", s->st_video);
    put_stat("st_io", s->st_io);
    put_stat("st_pair", s->st_pair);
    put("\"fuse\":{");
    for (int i = 0; i < FU__COUNT; i++)
        put("%s\"%s\":%llu", i ? "," : "", fuse_names[i], (unsigned long long)s->fuse_cand[i]);
    put("}},\"board\":{\"leds\":%u,\"seg7\":%u,\"seg7_ctrl\":%u,\"switches\":%u,\"snd\":[%u,%u,%u]},\"serial\":",
        d->leds, d->seg7, d->seg7_ctrl, d->switches, d->snd[0][0], d->snd[1][0], d->snd[2][0]);
    serial[serial_len] = 0;
    put_str(serial);
    serial_len = 0;
    put("}");
    return out;
}

/* `rows` lines of disassembly from `top`.  With follow set, top moves so the
 * PC stays in view.  JSON: {"top":..,"lines":[[addr,label,hex,text,bp],..]}. */
EMSCRIPTEN_KEEPALIVE const char *web_disasm(int top_in, int rows, int follow)
{
    uint32_t top = (uint32_t)top_in & 0xFFFFFE;
    if (follow) {
        uint32_t a = top;
        int seen = 0;
        for (int i = 0; i < rows - 3 && !seen; i++) {
            dinst_t d;
            seen = a == cpu.pc;
            a = (a + (dbg_decode_at(&cpu, a, &d) ? d.len : 2)) & 0xFFFFFF;
        }
        if (!seen) {
            uint32_t off;
            const char *s = nsyms ? sym_near(cpu.pc, &off) : NULL;
            top = s && off < 24 ? cpu.pc - off : cpu.pc;
        }
    }
    out_len = 0;
    put("{\"top\":%u,\"lines\":[", top);
    uint32_t a = top;
    for (int i = 0; i < rows; i++) {
        const char *lab = nsyms ? sym_at(a) : NULL;
        if (lab && ++i >= rows)
            break;
        char text[128], hex[16];
        int len = dbg_disasm(&cpu, a, text, sizeof text);
        if (len == 4)
            snprintf(hex, sizeof hex, "%04X %04X", cpu_peek16(&cpu, a), cpu_peek16(&cpu, a + 2));
        else
            snprintf(hex, sizeof hex, "%04X", cpu_peek16(&cpu, a));
        put("%s[%u,", a == top ? "" : ",", a);
        if (lab)
            put_str(lab);
        else
            put("null");
        put(",\"%s\",", hex);
        put_str(text);
        put(",%d]", dbg_bp(&dbg, a));
        a = (a + (uint32_t)len) & 0xFFFFFF;
    }
    put("]}");
    return out;
}

/* `rows` lines of hex dump from addr (rounded down to 16), as text. */
EMSCRIPTEN_KEEPALIVE const char *web_mem(int addr, int rows)
{
    out_len = 0;
    uint32_t a = (uint32_t)addr & 0xFFFFF0;
    for (int row = 0; row < rows; row++, a = (a + 16) & 0xFFFFFF) {
        int mapped = a < SRAM_SIZE || a >= ROM_BASE;
        char asc[17];
        put("%06X ", a);
        for (int i = 0; i < 16; i++) {
            uint32_t x = a + (uint32_t)i;
            if (!mapped) {
                put(i == 8 ? "  --" : " --");
                asc[i] = ' ';
                continue;
            }
            uint8_t b = x < SRAM_SIZE ? cpu.sram[x] : cpu.rom[x - ROM_BASE];
            put(i == 8 ? "  %02X" : " %02X", b);
            asc[i] = b >= 32 && b < 127 ? (char)b : '.';
        }
        asc[16] = 0;
        put("  %s\n", asc);
    }
    return out;
}

int main(void) { return 0; }
