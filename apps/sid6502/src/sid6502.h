/* sid6502: a 6502 interpreter and dynamic recompiler for fpga_cpu.
 * See ../README.md.  int is 16 bits on the target: all widths are explicit. */
#ifndef SID6502_H
#define SID6502_H
#include <stdint.h>

#ifndef SID_LOG_MAX
#define SID_LOG_MAX 4096        /* SID write log capacity (entries) */
#endif

#define STOP_PC   0xFFF0u       /* init/play return here: the "exit" address */

#define ATTR_CODE 1             /* byte belongs to translated code */
#define ATTR_IO   2             /* $D000-$DFFF */

/* 6502 P bits */
#define P_N 0x80
#define P_V 0x40
#define P_B 0x10
#define P_D 0x08
#define P_I 0x04
#define P_Z 0x02
#define P_C 0x01

typedef struct {
    uint8_t a, x, y, s;
    uint8_t n, v, d, i, z, c;   /* each 0 or 1 */
    uint16_t pc;
    uint32_t cycles;            /* 6502 cycles since reset */
} cpu6502_t;

typedef struct {
    uint32_t cycle;
    uint8_t reg;                /* 0..31 */
    uint8_t val;
} sidwrite_t;

#ifdef SID_MEM_ADDR
/* the big arrays at fixed addresses (e.g. in SDRAM, away from fast block RAM): -DSID_MEM_ADDR=0x10000
 * -DSID_ATTR_ADDR=0x20000 -DSID_LOG_ADDR=0x30000 (SID_LOG_MAX * 8 bytes); see jit.c for JIT_DATA_ADDR */
#define mem6502  ((uint8_t *)SID_MEM_ADDR)
#define attr6502 ((uint8_t *)SID_ATTR_ADDR)
#define sidlog   ((sidwrite_t *)SID_LOG_ADDR)
#else
extern uint8_t mem6502[65536];
extern uint8_t attr6502[65536];
extern sidwrite_t sidlog[SID_LOG_MAX];
#endif
extern cpu6502_t cpu;
extern uint16_t sid_n;          /* entries in sidlog */
extern uint16_t sid_dropped;    /* writes lost because the log was full */

/* I/O hooks (default: raster counter for $D011/$D012, otherwise memory) */
extern uint8_t (*io_read_hook)(uint16_t addr, uint32_t cycle);
extern void (*io_write_hook)(uint16_t addr, uint8_t val, uint32_t cycle);

/* results of a run */
#define RUN_STOP    0           /* reached stop_pc */
#define RUN_LIMIT   1           /* cycle limit */
#define RUN_ILLEGAL 2           /* undefined/unsupported opcode at cpu.pc */
#define RUN_MISMATCH 3          /* the stepping hook reported a difference */

void sid_reset(void);
void sid_load(uint16_t addr, const uint8_t *data, uint16_t len);
void sid_log_clear(void);
uint8_t cpu_getp(uint8_t b);
void cpu_setp(uint8_t p);
uint8_t io_read(uint16_t a, uint32_t cycle);
void io_write(uint16_t a, uint8_t v, uint32_t cycle);

/* interpreter */
int interp_step(void);                          /* one instruction; 0 on illegal opcode */
int interp_run(uint32_t until_cycles, uint16_t stop_pc);
extern uint16_t interp_nominal;                 /* cycles interp_step charged beyond the table minimum */

/* called when a store hits a translated code byte (jit.c) */
void smc_write(uint16_t a);

/* JIT engine */
#define JIT_CHAIN_OFF 1                         /* jit_flags: no chaining, return at every trace */
extern uint8_t jit_flags;
uint8_t jit_live_p(uint16_t pc);                /* P bits that are exact at pc (the JIT skips dead flags) */
int jit_run(uint32_t until_cycles, uint16_t stop_pc);
void jit_flush(void);
typedef struct {
    uint32_t traces, instrs, bytes;             /* translated: traces, 6502 instructions, native bytes */
    uint32_t events, assists, flushes, links;
    uint32_t xlate_cycles;                      /* native cycles spent translating (if counted) */
} jit_stats_t;
extern jit_stats_t jit_stats;
/* stepping hook for differential tests: called after each trace with state in cpu */
extern int (*jit_step_hook)(void);

/* player API */
int call_routine(uint16_t addr, uint8_t a, uint8_t x, uint8_t y, uint32_t max_cycles, int use_jit);

#endif
