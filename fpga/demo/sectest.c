/* Test one demo section alone in fsim (it includes only that section, so
 * the others may be half-written):
 *
 *   llvm/fcc -O2 --stack 0x20000 -DSEC=4 -DSECS=300 -o t.bin fpga/demo/sectest.c
 *   sim/build/fsim --no-hicolor --screenshot t.ppm t.bin
 *
 * SEC: 1 platform, 2 bullets, 3 plasma, 4 racing, 5 iso, 6 desktop.
 * SECS: frames (60 a second) to run. It prints how many frames were drawn
 * and the instructions a drawn frame (the board does about 70,000 in a
 * 60 Hz frame), then exits; --screenshot is the last frame shown.
 * Add -DTANG for the high-tier build (then run fsim without --no-hicolor). */
#include "gfx.c"
#include <stdio.h>

#ifndef SECS
#define SECS 120
#endif

#if SEC == 1
#include "sec_platform.c"
#define RUN pf_run
#elif SEC == 2
#include "sec_bullets.c"
#define RUN bh_run
#elif SEC == 3
#include "sec_plasma.c"
#define RUN pl_run
#elif SEC == 4
#include "sec_racing.c"
#define RUN rc_run
#elif SEC == 5
#include "sec_iso.c"
#define RUN iso_run
#elif SEC == 6
#include "sec_desktop.c"
#define RUN dt_run
#endif

int main(void)
{
    gfx_init();
    tclear();
    music_init();
#if SEC == 3
    pl_setup();
#endif
    RUN(SECS);
    printf("section %d: %lu frames drawn in %lu, %lu instructions a drawn frame\n", SEC,
           (unsigned long)frames_drawn, (unsigned long)frames_total,
           (unsigned long)(frames_drawn ? sec_insts_all / frames_drawn : 0));
    return 0;
}
