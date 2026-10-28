/* fpga_cpu simulator: debugger run control (see dbg.h). */
#include "dbg.h"

#include <stdlib.h>
#include <string.h>

#include "front.h"

#define BP_SLOTS (1u << 23)

void dbg_init(dbg_t *g, cpu_t *c)
{
    memset(g, 0, sizeof *g);
    g->cpu = c;
    g->mode = DBG_RUNNING;
    g->bp = calloc(BP_SLOTS, 1);
}

void dbg_free(dbg_t *g)
{
    free(g->bp);
    g->bp = NULL;
}

void dbg_clear_bps(dbg_t *g) { memset(g->bp, 0, BP_SLOTS); }

uint32_t dbg_sp(const cpu_t *c) { return c->r[14] | (uint32_t)(c->r[15] & 0xFF) << 16; }

int dbg_decode_at(const cpu_t *c, uint32_t pc, dinst_t *d)
{
    return decode(c->var, cpu_peek16(c, pc), cpu_peek16(c, (pc + 2) & 0xFFFFFF), d) != 0;
}

void dbg_step(dbg_t *g) { cpu_step(g->cpu); }

void dbg_step_over(dbg_t *g)
{
    cpu_t *c = g->cpu;
    dinst_t d;
    if (dbg_decode_at(c, c->pc, &d) && (d.op == OP_JAL || d.op == OP_JALR)) {
        g->over_target = (c->pc + d.len) & 0xFFFFFF;
        g->over_sp = dbg_sp(c);
        g->mode = DBG_STEP_OVER;
    }
    cpu_step(c); /* also leaves the breakpoint we may be sitting on */
}

void dbg_step_out(dbg_t *g)
{
    g->out_sp = dbg_sp(g->cpu);
    g->out_depth = 0;
    g->mode = DBG_STEP_OUT;
}

void dbg_resume(dbg_t *g)
{
    g->mode = DBG_RUNNING;
    dbg_run(g, 1);
}

/* Breakpoints are checked before each instruction except the first, so
 * resuming can leave a breakpoint. */
void dbg_run(dbg_t *g, uint64_t budget)
{
    cpu_t *c = g->cpu;
    uint64_t end = c->st.cycles + budget;
    int first = 1;
    while (g->mode != DBG_STOPPED && c->st.cycles < end) {
        if (c->halted) {
            g->mode = DBG_STOPPED;
            return;
        }
        uint32_t pc = c->pc;
        if (!first && g->bp[pc >> 1]) {
            g->mode = DBG_STOPPED;
            return;
        }
        first = 0;
        if (g->mode == DBG_STEP_OVER && pc == g->over_target && dbg_sp(c) >= g->over_sp) {
            g->mode = DBG_STOPPED;
            return;
        }
        /* step out ends at a return (JR) that is not from a call made since
         * the step began; the stack check covers recursion */
        int is_ret = 0;
        if (g->mode == DBG_STEP_OUT) {
            dinst_t d;
            if (dbg_decode_at(c, pc, &d)) {
                if (d.op == OP_JAL || d.op == OP_JALR)
                    g->out_depth++;
                else if (d.op == OP_JR && g->out_depth > 0)
                    g->out_depth--;
                else if (d.op == OP_JR)
                    is_ret = 1;
            }
        }
        cpu_step(c);
        if (is_ret && dbg_sp(c) >= g->out_sp) {
            g->mode = DBG_STOPPED;
            return;
        }
    }
}

int dbg_disasm(const cpu_t *c, uint32_t a, char *buf, size_t n)
{
    dinst_t d;
    if (!dbg_decode_at(c, a, &d)) {
        snprintf(buf, n, "??");
        return 2;
    }
    disasm(&d, a, buf, n);
    /* annotate branch targets with symbols */
    int cls = op_info[d.op].cls;
    if (nsyms && (cls == CLS_BR || cls == CLS_JAL || cls == CLS_LEAPC)) {
        uint32_t t = (a + (uint32_t)d.imm) & 0xFFFFFF, off;
        const char *s = sym_near(t, &off);
        size_t len = strlen(buf);
        if (s && off)
            snprintf(buf + len, n - len, " <%s+%u>", s, off);
        else if (s)
            snprintf(buf + len, n - len, " <%s>", s);
    }
    return d.len;
}

int dbg_parse_addr(const char *s, uint32_t *a)
{
    while (*s == ' ')
        s++;
    if (!*s)
        return 0;
    if (sym_find(s, a))
        return 1;
    char *e;
    unsigned long v = strtoul(s[0] == '$' ? s + 1 : s, &e, 16);
    if (*e)
        return 0;
    *a = (uint32_t)v & 0xFFFFFF;
    return 1;
}
