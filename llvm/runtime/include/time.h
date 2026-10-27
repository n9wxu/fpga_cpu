/* fpga_cpu C runtime: <time.h>. clock() counts CPU cycles / 16 (the CPU
 * and SRAM run at 50 MHz), so it wraps after about 86 seconds; time()
 * counts seconds since reset. There is no calendar. */
#ifndef _TIME_H
#define _TIME_H
typedef long clock_t;
typedef long time_t;
#define CLOCKS_PER_SEC 3125000L
#define CLK_TCK CLOCKS_PER_SEC
clock_t clock(void);
time_t time(time_t *t);
#endif
