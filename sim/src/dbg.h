/* fpga_cpu simulator: debugger run control shared by the front ends
 * (fsim-gui and the WebAssembly page): breakpoints, step, step over, step
 * out, and the disassembly line format. */
#ifndef DBG_H
#define DBG_H

#include "cpu.h"

typedef enum { DBG_STOPPED, DBG_RUNNING, DBG_STEP_OVER, DBG_STEP_OUT } dbg_mode_t;

typedef struct {
    cpu_t *cpu;
    dbg_mode_t mode;
    uint32_t over_target, over_sp, out_sp;
    int out_depth; /* step out: calls entered and not yet returned from */
    uint8_t *bp; /* one flag per 2-byte address */
} dbg_t;

void dbg_init(dbg_t *g, cpu_t *c);
void dbg_free(dbg_t *g);
void dbg_clear_bps(dbg_t *g);
static inline int dbg_bp(const dbg_t *g, uint32_t a) { return g->bp[(a & 0xFFFFFF) >> 1]; }
static inline void dbg_toggle_bp(dbg_t *g, uint32_t a) { g->bp[(a & 0xFFFFFF) >> 1] ^= 1; }

void dbg_step(dbg_t *g);       /* one instruction */
void dbg_step_over(dbg_t *g);  /* a call runs to its return; anything else steps */
void dbg_step_out(dbg_t *g);   /* run until the current function returns */
void dbg_resume(dbg_t *g);     /* run, leaving a breakpoint we may be sitting on */
/* Run up to `budget` cycles in the current mode.  Stops (mode becomes
 * DBG_STOPPED) at breakpoints, step-over/out targets and when the CPU halts. */
void dbg_run(dbg_t *g, uint64_t budget);

uint32_t dbg_sp(const cpu_t *c);
int dbg_decode_at(const cpu_t *c, uint32_t pc, dinst_t *d); /* 0 if not an instruction */
/* Disassemble at a: "text <sym+off>" into buf.  Returns the length in bytes (2 if undecodable). */
int dbg_disasm(const cpu_t *c, uint32_t a, char *buf, size_t n);
int dbg_parse_addr(const char *s, uint32_t *a); /* symbol or hex ("$" optional); 0 if neither */

#endif
