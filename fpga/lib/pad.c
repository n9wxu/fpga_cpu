/* Sega Genesis pads (pad.h): select-line sequence as a Genesis game's.
 *
 *   TH  TR     TL  RIGHT LEFT DOWN UP
 *   1   C      B   Right Left Down Up
 *   0   Start  A   0     0    Down Up       the 1st and 2nd low
 *   0   Start  A   0     0    0    0        a 6-button pad's 3rd low
 *   1   C      B   Mode  X    Y    Z        ... and the high after it
 *   0   Start  A   1     1    1    1        ... its 4th low
 *
 * The lines are low for pressed. A 6-button pad counts the lows until TH
 * stays still for 1.5 ms. LEFT and RIGHT high while TH is low: no pad (the
 * expansion board's pull-ups). */
#include "pad.h"

#define T16 IO16(0xF00004)                /* CYCLE's low half: 10 MHz */

static uint16_t th(uint16_t v)
{
    PADS = v;
    uint16_t t0 = T16;                    /* the pad and the cable: 2 us */
    while ((uint16_t)(T16 - t0) < 20) ;
    return PADS;
}

void pad_read(uint16_t pad[2])
{
    uint16_t h0 = th(1);                  /* Up Down Left Right B C */
    uint16_t l1 = th(0);                  /* Up Down 0 0 A Start */
    th(1);
    th(0);
    th(1);
    uint16_t l3 = th(0);                  /* 6-button: 0 0 0 0 */
    uint16_t h3 = th(1);                  /* 6-button: Z Y X Mode */
    th(0);
    th(1);
    for (int p = 0; p < 2; p++) {
        int s = 8 * p;
        uint16_t h = ~h0 >> s, l = ~l1 >> s;          /* 1 = pressed */
        if ((l & 0x0C) != 0x0C) { pad[p] = 0; continue; }
        uint16_t b = (h & 0x3F) | ((l & 0x30) << 2) | PAD_PRESENT;
        if (((l3 >> s) & 0x0F) == 0)
            b |= ((~h3 >> s) & 0x0F) << 8 | PAD_SIX;
        pad[p] = b;
    }
}
