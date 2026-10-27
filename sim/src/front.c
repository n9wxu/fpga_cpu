/* fpga_cpu simulator: helpers shared by the front ends. */
#define _POSIX_C_SOURCE 200809L /* strdup under -std=c11 on glibc */
#include "front.h"

#include <stdlib.h>
#include <string.h>

void opts_usage(int gui)
{
    fprintf(stderr,
            "usage: %s [options] image.bin\n"
            "  -v A|B            encoding variant (default A)\n"
            "  --load ADDR       load address (default 0x1000)\n"
            "  --entry ADDR      start address (default = load address)\n"
            "  --icache BYTES    I-cache size, 0 = none (default 2048)\n"
            "  --line BYTES      I-cache line size (default 16)\n"
            "  --no-prefetch     disable the stream buffer\n"
            "  --fuse            apply every kind of macro-op fusion (candidates are always counted)\n"
            "  --fuse=LIST       apply only these kinds: cmpbr,addadc,leamem,ldpair,alubr\n"
            "  --dual[=banked|ports]  in-order dual issue; banked (default) = even/odd write banks,\n"
            "                    ports = a full second register write port\n"
            "  --branch-pen N    taken-branch penalty (default 2)\n"
            "  --video P:B       video steals B of every P port cycles (default 56:4 = 7%%, 0:0 = off)\n"
            "  --xram MB         simulator-only expansion RAM at 0x100000, up to 14 MB (default none)\n"
            "  --max-cycles N    stop after N cycles (default %s)\n"
            "  --disk DIR        host directory served by the file service (default ./disk)\n"
            "  --sym FILE        symbol file (lines of \"%%06x name\")\n"
            "  --keys STRING     preload the keyboard queue (\\n = Enter, \\e = Esc)\n"
            "  --true-color      show hi-color palette colors instead of the 3-bit dither\n"
            "  --screenshot FILE write the 1280x720 display as PPM at exit\n",
            gui ? "fsim-gui" : "fsim", gui ? "none" : "2000000000");
    if (gui)
        fprintf(stderr,
                "  --paused          start stopped (default: running)\n"
                "  --max-speed       run as fast as possible (default: real time, 50 MHz)\n"
                "  --frames N        run N display frames, then exit\n"
                "  --screenshot-window FILE  with --frames: save the whole window as BMP\n");
    else
        fprintf(stderr,
                "  --json FILE       write statistics as JSON\n"
                "  --dump A:N:FILE   write N bytes of memory from address A to FILE at exit\n"
                "  --trace           trace every instruction to stderr\n"
                "  -q                no summary on stderr\n");
    exit(2);
}

static uint32_t num(const char *s) { return (uint32_t)strtoul(s, NULL, 0); }

/* "cmpbr,alubr" -> FU_* bit mask */
static int parse_fuse(const char *s, int *mask)
{
    *mask = 0;
    while (*s) {
        size_t n = strcspn(s, ",");
        int k = 0;
        while (k < FU__COUNT && (strlen(fuse_keys[k]) != n || strncmp(s, fuse_keys[k], n)))
            k++;
        if (k == FU__COUNT) {
            fprintf(stderr, "--fuse: unknown kind '%.*s'\n", (int)n, s);
            return -1;
        }
        *mask |= 1 << k;
        s += n;
        if (*s == ',')
            s++;
    }
    return 0;
}

int opts_parse(opts_t *o, int argc, char **argv, int gui)
{
    memset(o, 0, sizeof *o);
    o->cfg = (tcfg_t){ 2048, 16, 1, 0, 2, 1, 2, 56, 4, DUAL_OFF };
    o->vname = "A";
    o->load = 0x1000;
    o->entry = 0xFFFFFFFF;
    o->max_cycles = gui ? 0 : 2000000000ULL;
    o->disk = "disk";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        int more = i + 1 < argc;
        if (!strcmp(a, "-v") && more) o->vname = argv[++i];
        else if (!strcmp(a, "--load") && more) o->load = num(argv[++i]);
        else if (!strcmp(a, "--entry") && more) o->entry = num(argv[++i]);
        else if (!strcmp(a, "--icache") && more) o->cfg.icache_bytes = num(argv[++i]);
        else if (!strcmp(a, "--line") && more) o->cfg.line_bytes = num(argv[++i]);
        else if (!strcmp(a, "--no-prefetch")) o->cfg.prefetch = 0;
        else if (!strcmp(a, "--xram") && more) o->cfg.xram_mb = num(argv[++i]);
        else if (!strcmp(a, "--fuse")) o->cfg.fuse = FU_ALL;
        else if (!strncmp(a, "--fuse=", 7)) {
            if (parse_fuse(a + 7, &o->cfg.fuse))
                return -1;
        } else if (!strcmp(a, "--dual") || !strcmp(a, "--dual=banked")) o->cfg.dual = DUAL_BANKED;
        else if (!strcmp(a, "--dual=ports")) o->cfg.dual = DUAL_PORTS;
        else if (!strcmp(a, "--branch-pen") && more) o->cfg.branch_pen = (int)num(argv[++i]);
        else if (!strcmp(a, "--video") && more) {
            char *colon;
            o->cfg.vperiod = (uint32_t)strtoul(argv[++i], &colon, 0);
            o->cfg.vburst = *colon == ':' ? (uint32_t)strtoul(colon + 1, NULL, 0) : 0;
        } else if (!strcmp(a, "--max-cycles") && more) o->max_cycles = strtoull(argv[++i], NULL, 0);
        else if (!strcmp(a, "--disk") && more) o->disk = argv[++i];
        else if (!strcmp(a, "--sym") && more) o->sym = argv[++i];
        else if (!strcmp(a, "--keys") && more) o->keys = argv[++i];
        else if (!strcmp(a, "--true-color")) o->true_color = 1;
        else if (!strcmp(a, "--screenshot") && more) o->screenshot = argv[++i];
        else if (!gui && !strcmp(a, "--json") && more) o->json = argv[++i];
        else if (!gui && !strcmp(a, "--trace")) o->trace = 1;
        else if (!gui && !strcmp(a, "--dump") && more) o->dump = argv[++i];
        else if (!gui && !strcmp(a, "-q")) o->quiet = 1;
        else if (gui && !strcmp(a, "--paused")) o->paused = 1;
        else if (gui && !strcmp(a, "--max-speed")) o->max_speed = 1;
        else if (gui && !strcmp(a, "--frames") && more) o->frames = (int)num(argv[++i]);
        else if (gui && !strcmp(a, "--screenshot-window") && more) o->shot_window = argv[++i];
        else if (a[0] == '-') return -1;
        else o->image = a;
    }
    if (!o->image)
        return -1;
    if (o->cfg.icache_bytes && (o->cfg.line_bytes < 4 || o->cfg.icache_bytes % o->cfg.line_bytes)) {
        fprintf(stderr, "bad I-cache geometry\n");
        exit(2);
    }
    if (!o->cfg.icache_bytes)
        o->cfg.prefetch = 0;
    return 0;
}

const variant_t *find_variant(const char *name)
{
    for (int i = 0; i < NUM_VARIANTS; i++)
        if (!strcmp(variants[i]->name, name))
            return variants[i];
    return NULL;
}

int load_image(cpu_t *c, const char *path, uint32_t load)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return 0;
    }
    if (load < ROM_BASE && load >= c->ram_size) {
        fprintf(stderr, "%s: load address %06x is not RAM (expansion RAM needs --xram)\n", path, load);
        return 0;
    }
    uint8_t *dst = load >= ROM_BASE ? c->rom + (load - ROM_BASE) : c->sram + load;
    size_t room = load >= ROM_BASE ? ROM_SIZE - (load - ROM_BASE) : c->ram_size - load;
    size_t n = fread(dst, 1, room, f);
    fclose(f);
    if (!n)
        fprintf(stderr, "%s: empty image\n", path);
    cpu_mem_written(c, load, (uint32_t)n);
    return (int)n;
}

void push_keys(iodev_t *d, const char *s)
{
    for (; s && *s; s++) {
        if (*s == '\\' && s[1]) {
            s++;
            dev_key(d, *s == 'n' ? 13 : *s == 'e' ? 27 : *s == 't' ? 9 : *s == 'b' ? 8 : (uint16_t)*s);
        } else {
            dev_key(d, *s == '\n' ? 13 : (uint16_t)(unsigned char)*s);
        }
    }
}

int setup_cpu(cpu_t *c, const opts_t *o)
{
    const variant_t *v = find_variant(o->vname);
    if (!v) {
        fprintf(stderr, "unknown variant %s\n", o->vname);
        return -1;
    }
    cpu_init(c, v, &o->cfg);
    c->trace = o->trace;
    snprintf(c->dev.disk, sizeof c->dev.disk, "%s", o->disk);
    if (o->sym && sym_load(o->sym) < 0) {
        perror(o->sym);
        return -1;
    }
    if (!load_image(c, o->image, o->load))
        return -1;
    c->pc = o->entry == 0xFFFFFFFF ? o->load : o->entry;
    push_keys(&c->dev, o->keys);
    return 0;
}

/* ---- symbols ---- */

sym_t *syms;
int nsyms;

static int sym_cmp(const void *a, const void *b)
{
    const sym_t *x = a, *y = b;
    return x->addr < y->addr ? -1 : x->addr > y->addr;
}

int sym_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    char line[256], name[200];
    unsigned a;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%x %199s", &a, name) == 2) {
            syms = realloc(syms, sizeof *syms * (size_t)(nsyms + 1));
            syms[nsyms].addr = a & 0xFFFFFF;
            syms[nsyms++].name = strdup(name);
        }
    fclose(f);
    qsort(syms, (size_t)nsyms, sizeof *syms, sym_cmp);
    return nsyms;
}

/* index of the last symbol at or below addr, -1 if none */
static int sym_floor(uint32_t addr)
{
    int lo = 0, hi = nsyms - 1, r = -1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (syms[m].addr <= addr) {
            r = m;
            lo = m + 1;
        } else {
            hi = m - 1;
        }
    }
    return r;
}

const char *sym_at(uint32_t addr)
{
    int i = sym_floor(addr);
    if (i < 0 || syms[i].addr != addr)
        return NULL;
    /* several names can share an address: prefer one that is not a local '.L' label */
    for (int j = i; j >= 0 && syms[j].addr == addr; j--)
        if (syms[j].name[0] != '.')
            return syms[j].name;
    return syms[i].name;
}

const char *sym_near(uint32_t addr, uint32_t *off)
{
    int i = sym_floor(addr);
    if (i < 0)
        return NULL;
    *off = addr - syms[i].addr;
    return syms[i].name;
}

int sym_find(const char *name, uint32_t *addr)
{
    for (int i = 0; i < nsyms; i++)
        if (!strcmp(syms[i].name, name)) {
            *addr = syms[i].addr;
            return 1;
        }
    return 0;
}
