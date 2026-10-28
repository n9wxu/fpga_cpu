/* fpga_cpu simulator: helpers shared by the front ends (options, image
 * loading, symbols). */
#ifndef FRONT_H
#define FRONT_H

#include "cpu.h"

typedef struct {
    tcfg_t cfg;
    const char *vname, *image, *json, *disk, *sym, *screenshot, *keys, *dump;
    uint32_t load, entry;
    uint64_t max_cycles;
    int quiet, trace, true_color;
    /* GUI only */
    int frames;             /* run this many frames, then exit (0 = interactive) */
    const char *shot_window; /* with --frames: save the whole window here (BMP) */
    int max_speed, paused;
} opts_t;

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
