/* fpga_cpu simulator: two Sega Genesis pads on the Spartan-3's expansion
 * board (docs/io.md "Game pads", fpga/rtl/iosys.v PADS, fpga/sim/genesis_pad.v).
 * PADS (F00900): write bit 0 = the select line TH; read pad 1's lines
 * {TR, TL, RIGHT, LEFT, DOWN, UP} in bits 5:0, pad 2's in 13:8 (low =
 * pressed). No pad reads as the pull-ups, all 1s.
 *
 * --pad N,TYPE,BUTTONS: pad N (1 or 2), TYPE 3 or 6 buttons, BUTTONS held as
 * fpga/lib/pad.h (bit 0 Up ... 11 Mode). */
#include <stdlib.h>
#include <string.h>
#include "cpu.h"
#include "dev.h"

#define PADS_ADDR 0xF00900u
#define TIMEOUT   75000u                   /* 1.5 ms at 50 MHz: a 6-button pad starts over */

static struct { int type; uint16_t buttons; } P[2];
static int th = 1, falls;
static uint64_t last;

int pads_handles(uint32_t a) { return a == PADS_ADDR || a == PADS_ADDR + 1; }

static uint16_t lines(int i)
{
    if (!P[i].type)
        return 0x3F;
    uint16_t p = (uint16_t)~P[i].buttons;   /* active low */
    int six = P[i].type == 6;
    if (th)
        return six && falls == 3 ? (uint16_t)((p & 0x30) | (p >> 8 & 0x0F)) : (p & 0x3F);
    uint16_t ts = (p >> 2) & 0x30;           /* TR = Start, TL = A */
    if (six && falls == 3) return ts;
    if (six && falls == 4) return ts | 0x0F;
    return ts | (p & 0x03);
}

uint16_t pads_read(cpu_t *c, uint32_t a)
{
    (void)c;
    (void)a;
    return (uint16_t)(lines(0) | lines(1) << 8);
}

void pads_write(cpu_t *c, uint32_t a, uint16_t v)
{
    if (a != PADS_ADDR)
        return;
    int n = v & 1;
    if (n == th)
        return;
    uint64_t now = c->st.cycles;
    if (now - last > TIMEOUT)
        falls = 0;
    last = now;
    th = n;
    if (!th && falls < 7)
        falls++;
}

void pads_reset(void)
{
    th = 1;
    falls = 0;
    last = 0;
}

/* --pad N,TYPE,BUTTONS */
int pads_config(const char *s)
{
    char *e;
    long n = strtol(s, &e, 0);
    if ((n != 1 && n != 2) || *e != ',')
        return -1;
    long t = strtol(e + 1, &e, 0);
    if ((t != 3 && t != 6) || *e != ',')
        return -1;
    long b = strtol(e + 1, &e, 0);
    if (*e || b < 0 || b > 0xFFF)
        return -1;
    P[n - 1].type = (int)t;
    P[n - 1].buttons = (uint16_t)(t == 3 ? b & 0xFF : b);
    return 0;
}
