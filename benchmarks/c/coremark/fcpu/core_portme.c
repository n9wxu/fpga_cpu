/*
 * CoreMark port for fpga_cpu (benchmarks/c/coremark), from EEMBC's
 * barebones template. Apache License 2.0 (see ../LICENSE.md).
 */
#include "coremark.h"
#include "core_portme.h"

#if VALIDATION_RUN
volatile ee_s32 seed1_volatile = 0x3415;
volatile ee_s32 seed2_volatile = 0x3415;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PERFORMANCE_RUN
volatile ee_s32 seed1_volatile = 0x0;
volatile ee_s32 seed2_volatile = 0x0;
volatile ee_s32 seed3_volatile = 0x66;
#endif
#if PROFILE_RUN
volatile ee_s32 seed1_volatile = 0x8;
volatile ee_s32 seed2_volatile = 0x8;
volatile ee_s32 seed3_volatile = 0x8;
#endif
volatile ee_s32 seed4_volatile = ITERATIONS;
volatile ee_s32 seed5_volatile = 0;

/* The CPU cycle counter (docs/io.md CYCLE_LO/HI), read so that a carry
 * between the halves can't tear the value. 50 MHz: wraps after 85 s. */
static CORETIMETYPE cycles(void)
{
    ee_u16 hi, lo;
    do {
        hi = *(volatile ee_u16 *)0xF00006;
        lo = *(volatile ee_u16 *)0xF00004;
    } while (*(volatile ee_u16 *)0xF00006 != hi);
    return (CORETIMETYPE)hi << 16 | lo;
}

#define EE_TICKS_PER_SEC 50000000UL

static CORETIMETYPE start_time_val, stop_time_val;

void start_time(void) { start_time_val = cycles(); }
void stop_time(void) { stop_time_val = cycles(); }
CORE_TICKS get_time(void) { return (CORE_TICKS)(stop_time_val - start_time_val); }

secs_ret time_in_secs(CORE_TICKS ticks)
{
    return ((secs_ret)ticks) / (secs_ret)EE_TICKS_PER_SEC;
}

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *))
        ee_printf("ERROR! Please define ee_ptr_int to a type that holds a pointer!\n");
    if (sizeof(ee_u32) != 4)
        ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
    p->portable_id = 1;
}

void portable_fini(core_portable *p) { p->portable_id = 0; }
