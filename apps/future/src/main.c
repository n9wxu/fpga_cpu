/* The Future: humans fly jets, then rockets, land on the Moon, build a space station and a Moon base, cross
 * the solar system and leave for the stars; the computers drawing it grow from a one-bit 128 x 72 screen and
 * a beeper to everything the Tang has. Ten minutes, 40 phrases of music (seq.c); each scene runs from its
 * first phrase to its last. -DSTART=p starts at phrase p; -DSTOP_AT=s stops fsim at 16th s of the song. */
#include "hw.h"
#include "assets.h"
#include "seq.c"
#include "cv.h"
#include "s_jets.c"
#include "s_rocket.c"
#include "s_moon.c"
#include "s_station.c"
#include "g3d.h"
#include "s_base.c"
#include "s_planets.c"
#include "s_stars.c"
#include "s_credits.c"
#ifndef START
#define START 0
#endif

static void sc_debug(uint8_t pend)
{
    char s[24];
    screen_reset();
    vid(VC_TEXT);
    while (cur_phrase() < pend && !song_done) {
        uint8_t p = cur_phrase();
        uint16_t k = since_kick();
        s[0] = 'P'; s[1] = (char)('0' + p / 10); s[2] = (char)('0' + p % 10); s[3] = ' ';
        s[4] = 'L'; s[5] = (char)('0' + song[p].lvl); s[6] = ' ';
        s[7] = (char)('0' + ((q_cur >> 4) & 7)); s[8] = '.'; s[9] = (char)('1' + ((q_cur >> 2) & 3)); s[10] = 0;
        tstr(35, 12, s, TA(k < 512 ? 6 : 7, 1));
        vsync();
    }
}

typedef void (*scene_fn)(uint8_t pend);
static const struct { uint8_t pend; scene_fn f; } scenes[] = {
    {5, sc_jets},
    {10, sc_rocket},
    {16, sc_moon},
    {22, sc_station},
    {27, sc_base},
    {33, sc_planets},
    {37, sc_stars},
    {40, sc_credits},
};

int main(void)
{
    uint8_t i;
    IO(0x1000) = 0x0A00;
    for (;;) {
        song_start(START);
        for (i = 0; i < sizeof scenes / sizeof scenes[0]; i++)
            if (scenes[i].pend > cur_phrase() && !song_done) scenes[i].f(scenes[i].pend);
        while (!song_done) poll();
    }
}
