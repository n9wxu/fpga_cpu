/* fpga_cpu C runtime: clock() and time() from the cycle counter. */
#include <fcpu.h>
#include <stdint.h>
#include <time.h>

static uint32_t cycles(void)
{
    uint16_t hi, lo;
    do { /* read hi, lo, hi so a carry between the halves can't tear */
        hi = IO16(0xF00006);
        lo = IO16(0xF00004);
    } while (IO16(0xF00006) != hi);
    return (uint32_t)hi << 16 | lo;
}

clock_t clock(void) { return (clock_t)(cycles() >> 4); }

time_t time(time_t *t)
{
    time_t s = (time_t)(cycles() / 50000000UL);
    if (t)
        *t = s;
    return s;
}
