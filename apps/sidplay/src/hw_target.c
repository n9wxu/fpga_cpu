/* The Tang / fsim side: engine registers at F01000, the cycle counter, and (with -DAUDLOG) a log of
 * every engine register write in SDRAM for tools/log2script.py. */
#include "sidplay.h"
#include "layout.h"

#ifdef AUDLOG
#define LOG_BASE SDR_AUDLOG
volatile uint32_t aud_tick;                     /* set by main: the play frame being applied */
static uint32_t log_n;
#endif

void aud_raw(uint16_t addr, uint16_t val)
{
    *(volatile uint16_t *)(0xF00000UL + addr) = val;
#ifdef AUDLOG
    {
        volatile uint16_t *e = (volatile uint16_t *)(LOG_BASE + 16 + log_n * 8);
        if (log_n < 3500UL) {
            e[0] = (uint16_t)aud_tick;
            e[1] = (uint16_t)(aud_tick >> 16);
            e[2] = (uint16_t)(addr);
            e[3] = val;
            log_n++;
            *(volatile uint32_t *)LOG_BASE = log_n;
        }
    }
#endif
}

uint32_t cyc_now(void)
{
    volatile uint16_t *hi = (volatile uint16_t *)0xF00006UL, *lo = (volatile uint16_t *)0xF00004UL;
    uint16_t h, l, h2;
    do {
        h = *hi;
        l = *lo;
        h2 = *hi;
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}
