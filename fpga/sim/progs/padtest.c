/* Game pad test (docs/io.md "Game pads", fpga/lib/pad.c): reads both pads
 * twice, 3 frames apart. Expected: a 6-button pad in 1 holding A, Start and
 * X, a 3-button pad in 2 holding Right and C, as system_tb.v's pads
 * (fpga/sim/genesis_pad.v) and
 *
 *   fsim --no-hicolor --pad 1,6,0x4C0 --pad 2,3,0x028 padtest.bin
 *
 * or no pads at all (PADS_NONE; fsim without --pad). Exit code 0 when the
 * reads are one of those. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "../../lib/pad.h"

int main(void)
{
    uint16_t a[2], b[2];
    pad_read(a);
    uint16_t f0 = FRAME;
    while ((uint16_t)(FRAME - f0) < 3) ;
    pad_read(b);
    printf("pad 1: %04x %04x, pad 2: %04x %04x\n", a[0], b[0], a[1], b[1]);
    int ok = a[0] == b[0] && a[1] == b[1] &&
             ((a[0] == (PAD_PRESENT | PAD_SIX | PAD_A | PAD_START | PAD_X) &&
               a[1] == (PAD_PRESENT | PAD_RIGHT | PAD_C)) ||
              (a[0] == 0 && a[1] == 0));
    printf(ok ? "ok\n" : "FAIL\n");
    return !ok;
}
