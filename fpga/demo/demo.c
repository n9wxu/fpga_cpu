/* fpga_cpu demo: six sections in a loop, each a small original piece
 *   1 a platform game        (sec_platform.c)
 *   2 a bullet-hell shooter  (sec_bullets.c)
 *   3 a plasma               (sec_plasma.c)
 *   4 a third-person racer   (sec_racing.c)
 *   5 an isometric flyer     (sec_iso.c)
 *   6 30 s of a paint program on a desktop (sec_desktop.c)
 * playing themselves. Everything is drawn by the CPU into 16-colour
 * bitmaps (gfx.c), so it runs on both video tiers: the Spartan-3 (low tier,
 * 16 direct colours) and the Tang Nano 20K, where the plasma uses the
 * 256-colour palette (high tier).
 *
 * The statistics line (row 24: frames a second, CPU busy, thousands of
 * instructions a frame, MIPS while busy) is hidden by SW0. A soundtrack
 * (music.c) plays on the 3-voice sound device (on the Spartan-3, made in
 * software into the sound stream: fpga/lib/sid.c); SW1 silences it.
 *
 * Build: llvm/fcc -O2 --stack 0x20000 -o demo.bin fpga/demo/demo.c
 *   (-DTANG for the Tang; fpga/Makefile and tang/Makefile do this)
 * Test one section in fsim: add -DFSIM -DONLY=n -DSECS=frames; it runs
 * section n for that many frames, prints its instructions a frame and
 * exits, so fsim --screenshot shows its last frame (--no-hicolor: the
 * Spartan-3's video).
 *
 * int is 16 bits: products that can exceed 32767 use 32-bit types. */
#include "gfx.c"
#ifdef FSIM
#include <stdio.h>
#endif

#include "sec_platform.c"
#include "sec_bullets.c"
#include "sec_plasma.c"
#include "sec_racing.c"
#include "sec_iso.c"
#include "sec_desktop.c"

typedef struct {
    const char *title;
    void (*run)(int32_t frames);
    int16_t seconds;
} section_t;

static const section_t sections[] = {
    { "a platform game",          pf_run, 25 },
    { "a bullet-hell shooter",    bh_run, 25 },
    { "plasma",                   pl_run, 20 },
    { "a third-person racer",     rc_run, 25 },
    { "an isometric flyer",       iso_run, 25 },
    { "a paint program",          dt_run, 30 },
};
#define NSECTIONS ((int)(sizeof sections / sizeof sections[0]))

/* the card between sections: the title on black, about a second */
static COLD void title_card(int n)
{
    gfx_mode16();
    gfx_target(BUF0);
    clear(BLACK);
    uint16_t f = FRAME;
    while (FRAME == f) ;
    gfx_show(BUF0);
    tclear();
    tfill(10, 20, 40, ' ', ATTR(WHITE, BLUE));
    tfill(11, 20, 40, ' ', ATTR(WHITE, BLUE));
    tfill(12, 20, 40, ' ', ATTR(WHITE, BLUE));
    tput(10, 22, "fpga_cpu demo", ATTR(YELLOW, BLUE));
    char b[3] = { (char)('1' + n), 0, 0 };
    tput(10, 52, b, ATTR(WHITE, BLUE));
    tput(10, 53, "/6", ATTR(WHITE, BLUE));
    tcenter(11, sections[n].title, ATTR(WHITE, BLUE));
    SEG7 = (uint16_t)(n + 1);
    for (int i = 0; i < 60; i++) {
        f = FRAME;
        while (FRAME == f) ;
        music_tick(1);
        LEDS = (uint16_t)(1u << ((i >> 3) & 7));
    }
    tclear();
    music_silence();                            /* (the section's set-up: no note left hanging) */
    music_pause();
}

int main(void)
{
    gfx_init();
    tclear();
    pl_setup();                                 /* the high tier's plasma is computed once */
    music_init();
#ifdef FSIM
#ifndef ONLY
#define ONLY 1
#endif
#ifndef SECS
#define SECS 120
#endif
    {
        int n = ONLY - 1;
        tclear();
        sections[n].run(SECS);
        printf("section %d: %lu frames drawn in %lu, %lu instructions a frame\n", ONLY,
               (unsigned long)frames_drawn, (unsigned long)frames_total,
               (unsigned long)(frames_drawn ? sec_insts_all / frames_drawn : 0));
        return 0;
    }
#else
    for (;;)
        for (int n = 0; n < NSECTIONS; n++) {
            title_card(n);
            sec_index = n;
            sections[n].run((int32_t)sections[n].seconds * 60);
            tclear();
        }
#endif
}
