/* fpga_cpu simulator: helpers shared by the front ends (options, image
 * loading, symbols). */
#ifndef FRONT_H
#define FRONT_H

#include "cpu.h"

typedef struct {
    tcfg_t cfg;
    const char *vname, *image, *json, *disk, *sym, *screenshot, *keys, *dump, *profile, *log, *rom;
    int rom_save;
    uint32_t load, entry;
    uint64_t max_cycles;
    int quiet, trace, true_color;
    /* GUI only */
    int frames;             /* run this many frames, then exit (0 = interactive) */
    const char *shot_window; /* with --frames: save the whole window here (BMP) */
    int max_speed, paused;
    const char *programs;   /* GUI: the catalogue (default: programs/catalog.tsv beside fsim-gui) */
    const char *hw;         /* --hw NAME, as given */
} opts_t;

/* core configurations: fsim --core NAME; cpu = the matching fcc --cpu */
typedef struct { const char *name, *cpu, *about; } core_t;
#define NUM_CORES 4
extern const core_t cores[NUM_CORES];
int apply_core(tcfg_t *cfg, const char *name); /* index, or -1 if unknown */

/* Hardware profiles (fsim --hw NAME): the machine a program was made for. Each sets the video tier,
 * the RAM, how the hi-colour palette is shown and the clock (dev_cpu_hz), so that a program runs at
 * about the speed of the board's CPU (its MIPS, at fsim's cycles per instruction). */
typedef struct {
    const char *name, *title, *about;
    int hicolor;            /* 1: the high tier (palette, video extensions, blitter); 0: the low tier */
    uint32_t xram_mb;       /* RAM above the first MB */
    uint32_t hz;            /* the simulated clock */
    int true_color;         /* show the palette as the board's display does */
    int audio;              /* its sound device on the high tier (snd.h: SND_NONE, SND_ENGINE, SND_MC) */
} hw_t;
#define NUM_HW 5
extern const hw_t hw_profiles[NUM_HW];
const hw_t *find_hw(const char *name);
void apply_hw(opts_t *o, const hw_t *h);   /* the globals, o->cfg.xram_mb, o->true_color */

/* The programs the simulators offer (sim/tools/programs.py writes catalog.tsv beside their images). */
typedef struct {
    char name[48], kind[8], hw[16], variant[4], title[64], about[160], image[512], disk[512], keys[128];
    uint32_t load, entry;   /* entry 0xFFFFFFFF = the load address */
    int xram;               /* -1 = the hardware's */
} prog_t;
int catalog_load(const char *path, prog_t **out);   /* the number of programs (images found), -1 on error */

void opts_usage(int gui);
int opts_parse(opts_t *o, int argc, char **argv, int gui); /* 0 = ok */
const variant_t *find_variant(const char *name);
int load_image(cpu_t *c, const char *path, uint32_t load); /* bytes loaded, 0 on error */
void push_keys(iodev_t *d, const char *s); /* --keys: "\n" -> Enter (13), "\e" -> Esc */
int setup_cpu(cpu_t *c, const opts_t *o); /* init, load image, set PC; 0 = ok */

typedef struct { uint32_t addr; char *name; } sym_t;
extern sym_t *syms;
extern int nsyms;
int sym_load(const char *path);
void sym_free(void);
const char *sym_at(uint32_t addr);                  /* exact match or NULL */
const char *sym_near(uint32_t addr, uint32_t *off); /* nearest symbol at or below */
int sym_find(const char *name, uint32_t *addr);

#endif
