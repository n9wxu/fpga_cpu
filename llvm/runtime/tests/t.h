/* Shared helpers for the compiler tests. Every test runs on the host and
 * on fpga_cpu and must print the same thing: keep int-size differences out
 * of the output by casting results to fixed-width types. */
#include <stdint.h>
#include <stdio.h>

static uint32_t rs = 12345;
static uint32_t rnd(void)
{
    rs = rs * (uint32_t)1103515245 + (uint32_t)12345;
    return rs;
}
static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * (uint32_t)16777619 + (uint32_t)1; }

#define PH(name, h) printf("%-12s %08lx\n", name, (unsigned long)(uint32_t)(h))
#define PI(name, v) printf("%-12s %ld\n", name, (long)(v))
/* approximate results: compared with a tolerance by the runner */
#define PF(name, v) printf("~ %-10s %.6e\n", name, (double)(v))

/* defeat constant folding */
static volatile int16_t vzero = 0;
#define OPAQUE(x) ((x) + vzero)
