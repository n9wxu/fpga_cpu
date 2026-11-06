/* demo80s: the 1980s home computers, then the Tang Nano 20K pulling ahead (see ../README.md).
 *   1 Apple II   2 Commodore 64   3 Amiga   4 the finale
 * One compilation unit with mod.c beside it. Test builds: -DONLY=n runs section n alone, -DSTOP_AT=t stops
 * (fsim: EXIT) t 50 Hz ticks after the start, for a screenshot of that moment. */
#include "hw.h"
#include "music.c"
#ifdef STOP_AT
#undef STOP_HOOK
#endif
#include "sec_apple.c"
#ifndef ONLY_APPLE
#include "sec_c64.c"
#include "sec_amiga.c"
#include "sec_finale.c"
#endif

int main(void)
{
    music_init();
    for (;;) {
#if !defined(ONLY) || ONLY == 1
        sec_apple();
#endif
#if !defined(ONLY_APPLE) && (!defined(ONLY) || ONLY == 2)
        sec_c64();
#endif
#if !defined(ONLY_APPLE) && (!defined(ONLY) || ONLY == 3)
        sec_amiga();
#endif
#if !defined(ONLY_APPLE) && (!defined(ONLY) || ONLY == 4)
        sec_finale();
#endif
        music_all_off();
    }
}
