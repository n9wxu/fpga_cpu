/* fpga_cpu simulator: headless front end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "front.h"

static double pct(uint64_t a, uint64_t b) { return b ? 100.0 * (double)a / (double)b : 0.0; }

static void write_json(const cpu_t *c, const char *path, const char *image)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror(path);
        return;
    }
    const stats_t *s = &c->st;
    const tcfg_t *g = &c->cfg;
    fprintf(f, "{\n  \"image\": \"%s\", \"variant\": \"%s\",\n", image, c->var->name);
    fprintf(f, "  \"config\": {\"icache\": %u, \"line\": %u, \"prefetch\": %d, \"fuse\": %d, "
               "\"branch_pen\": %d, \"loaduse_pen\": %d, \"vperiod\": %u, \"vburst\": %u, \"dual\": %d},\n",
            g->icache_bytes, g->line_bytes, g->prefetch, g->fuse, g->branch_pen, g->loaduse_pen,
            g->vperiod, g->vburst, g->dual);
    fprintf(f, "  \"exit\": %d, \"halt\": \"%s\",\n", c->exit_code, c->halt_reason);
    fprintf(f, "  \"insts\": %llu, \"cycles\": %llu, \"len16\": %llu, \"len32\": %llu,\n",
            (unsigned long long)s->insts, (unsigned long long)s->cycles,
            (unsigned long long)s->len16, (unsigned long long)s->len32);
    fprintf(f, "  \"icache\": {\"access\": %llu, \"hit\": %llu, \"miss\": %llu, \"sb_hit\": %llu, \"sb_partial\": %llu},\n",
            (unsigned long long)s->ic_access, (unsigned long long)s->ic_hit, (unsigned long long)s->ic_miss,
            (unsigned long long)s->sb_hit, (unsigned long long)s->sb_partial);
    fprintf(f, "  \"stalls\": {\"ifetch\": %llu, \"video\": %llu, \"branch\": %llu, \"loaduse\": %llu, "
               "\"io\": %llu, \"pair_split\": %llu},\n",
            (unsigned long long)s->st_ifetch, (unsigned long long)s->st_video, (unsigned long long)s->st_branch,
            (unsigned long long)s->st_loaduse, (unsigned long long)s->st_io, (unsigned long long)s->st_pair);
    fprintf(f, "  \"data_words\": %llu, \"branches\": %llu, \"taken\": %llu,\n",
            (unsigned long long)s->data_words, (unsigned long long)s->branches, (unsigned long long)s->taken);
    fprintf(f, "  \"fusion\": {");
    for (int i = 0; i < FU__COUNT; i++)
        fprintf(f, "%s\"%s\": %llu", i ? ", " : "", fuse_names[i], (unsigned long long)s->fuse_cand[i]);
    fprintf(f, ", \"applied\": %llu},\n", (unsigned long long)s->fused);
    fprintf(f, "  \"dual\": {\"paired\": %llu, \"slot2\": {", (unsigned long long)s->paired);
    for (int i = 0; i < PS__COUNT; i++)
        fprintf(f, "%s\"%s\": %llu", i ? ", " : "", pair_slot_names[i], (unsigned long long)s->pair_slot[i]);
    fprintf(f, "}, \"blocked\": {");
    for (int i = 0; i < PB__COUNT; i++)
        fprintf(f, "%s\"%s\": %llu", i ? ", " : "", pair_block_names[i], (unsigned long long)s->pair_block[i]);
    fprintf(f, "}},\n");
    fprintf(f, "  \"port\": {\"fetch_bytes\": %llu, \"ifetch_words\": %llu},\n",
            (unsigned long long)s->fetch_bytes, (unsigned long long)s->port_ifetch);
    fprintf(f, "  \"ops\": {");
    int first = 1;
    for (int i = 0; i < OP__COUNT; i++)
        if (s->op[i]) {
            fprintf(f, "%s\"%s\": %llu", first ? "" : ", ", op_info[i].name, (unsigned long long)s->op[i]);
            first = 0;
        }
    fprintf(f, "},\n  \"forms\": {");
    first = 1;
    for (int i = 0; i < c->var->nforms; i++)
        if (s->form[i]) {
            fprintf(f, "%s\"%s\": %llu", first ? "" : ", ", c->var->forms[i].name, (unsigned long long)s->form[i]);
            first = 0;
        }
    fprintf(f, "}\n}\n");
    fclose(f);
}

static void summary(const cpu_t *c)
{
    const stats_t *s = &c->st;
    fprintf(stderr, "\n--- %s (variant %s) ---\n", c->halt_reason, c->var->name);
    fprintf(stderr, "insts %llu  cycles %llu  CPI %.3f  16-bit %.1f%%\n", (unsigned long long)s->insts,
            (unsigned long long)s->cycles, s->insts ? (double)s->cycles / (double)s->insts : 0.0,
            pct(s->len16, s->insts));
    fprintf(stderr, "icache: %llu misses, %.2f%% hit, stream-buffer hits %llu (+%llu partial)\n",
            (unsigned long long)s->ic_miss, pct(s->ic_hit, s->ic_access), (unsigned long long)s->sb_hit,
            (unsigned long long)s->sb_partial);
    fprintf(stderr, "stalls: ifetch %llu  branch %llu  load-use %llu  video %llu  io %llu  pair-split %llu\n",
            (unsigned long long)s->st_ifetch, (unsigned long long)s->st_branch, (unsigned long long)s->st_loaduse,
            (unsigned long long)s->st_video, (unsigned long long)s->st_io, (unsigned long long)s->st_pair);
    fprintf(stderr, "fusion candidates:");
    for (int i = 0; i < FU__COUNT; i++)
        fprintf(stderr, "  %s %llu", fuse_names[i], (unsigned long long)s->fuse_cand[i]);
    fprintf(stderr, "%s\n", c->cfg.fuse ? "  (applied)" : "");
    if (c->cfg.dual)
        fprintf(stderr, "dual issue (%s): %llu paired, %.1f%% of instructions\n",
                c->cfg.dual == DUAL_BANKED ? "banked" : "ports", (unsigned long long)s->paired,
                pct(s->paired, s->insts));
    fprintf(stderr, "SRAM port: ifetch %.1f%%  data %.1f%%  of cycles; core consumes %.2f instruction bytes/cycle\n",
            pct(s->port_ifetch, s->cycles), pct(s->data_words, s->cycles),
            s->cycles ? (double)s->fetch_bytes / (double)s->cycles : 0.0);
}

int main(int argc, char **argv)
{
    opts_t o;
    if (opts_parse(&o, argc, argv, 0))
        opts_usage(0);
    static cpu_t c;
    if (setup_cpu(&c, &o))
        return 2;
    if (o.profile && !(c.prof = calloc((size_t)1 << 24, sizeof *c.prof))) {
        fprintf(stderr, "--profile: out of memory\n");
        return 2;
    }
    fflush(stdout);
    cpu_run(&c, o.max_cycles);
    fflush(stdout);
    if (!o.quiet)
        summary(&c);
    if (o.json)
        write_json(&c, o.json, o.image);
    if (o.screenshot) {
        static uint32_t px[SCREEN_W * SCREEN_H];
        video_render(&c, px, o.true_color, 1);
        if (write_ppm(o.screenshot, px, SCREEN_W, SCREEN_H))
            perror(o.screenshot);
    }
    if (o.dump) {
        char *e;
        uint32_t a = (uint32_t)strtoul(o.dump, &e, 0), n = *e == ':' ? (uint32_t)strtoul(e + 1, &e, 0) : 0;
        FILE *f = *e == ':' ? fopen(e + 1, "wb") : NULL;
        if (!f || a >= c.ram_size) {
            fprintf(stderr, "--dump: need ADDR:LEN:FILE within RAM\n");
        } else {
            fwrite(c.sram + a, 1, a + n > c.ram_size ? c.ram_size - a : n, f);
            fclose(f);
        }
    }
    if (o.profile) {
        FILE *f = fopen(o.profile, "w");
        if (!f) {
            perror(o.profile);
        } else {
            fprintf(f, "# address instructions cycles\n");
            for (uint32_t i = 0; i < (1u << 23); i++)
                if (c.prof[i * 2])
                    fprintf(f, "%06x %u %u\n", i * 2, c.prof[i * 2], c.prof[i * 2 + 1]);
            fclose(f);
        }
        free(c.prof);
    }
    int rc = c.exit_code;
    cpu_free(&c);
    return rc;
}
