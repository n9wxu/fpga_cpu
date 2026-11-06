/* The Symphony No. 40 stress test's screen: a music visualizer that works the Tang's video hard.
 *   playfield A (8 bpp, double-buffered at 0x200000 / 0x240000): a starfield in parallax bands
 *       (line scroll), the scope ribbon and the sections' level bars (blitter fills)
 *   playfield B (8 bpp at 0x280000, in front): a piano roll scrolling left (VID_SCROLLXB), each
 *       note a streak in its section's colour, the whole roll waving with the loudness (line scroll)
 *   64 hardware sprites: a glowing particle per note-on, coloured by section, fading as it drifts
 *   palette: the background pulses on each beat; text layer: title, section, bar, a HUD
 *   line-scroll tables double-buffered at 0x2B9000 / 0x2BA000; sprite pixels at 0x2C0000
 *
 *   viz_init();
 *   for (;;) { fill a viz_t; viz_frame(&v); }
 *
 * viz_frame waits for the last frame's flip, then in vertical blank sends the sprites, scroll and
 * palette, draws the next frame into the hidden buffers and asks for the flip; the caller's work
 * (and the music interrupt) overlaps the wait. */
#ifndef SYM40_VIZ_H
#define SYM40_VIZ_H
#include <stdint.h>

enum { VI_VIOLIN1, VI_VIOLIN2, VI_VIOLA, VI_CELLO, VI_BASS, VI_FLUTE, VI_OBOE, VI_CLARINET, VI_BASSOON, VI_HORN, VI__N };
typedef struct { uint8_t inst; uint8_t midi; uint8_t vel; } viz_note_t;     /* a note-on */
/* a note on its way down, or sounding: start and length in 1/16 ticks (an eighth note) from now */
typedef struct { uint8_t inst, midi; int16_t start; uint16_t len; } viz_fall_t;
#define VIZ_LOOK (16 * 16)              /* how far ahead the falling notes reach: 16 eighths */
typedef struct {
    uint32_t now16;               /* now, in 1/16 ticks since the start (never wraps: the notes' scroll) */
    viz_fall_t fall[192];         /* the notes from a little before now to VIZ_LOOK on */
    uint16_t nfall;
    uint16_t level[VI__N];        /* each section's loudness now, 0..0x7FFF (envelope x volume) */
    int16_t  scope[256];          /* the last 256 output samples (mono), oldest first */
    viz_note_t notes[32];         /* note-ons since the last frame */
    uint8_t  nnotes;
    uint16_t bar, beat;           /* position: bar (from 1), beat 0-1 (the piece is in 2/2) */
    uint16_t blocks_busy;         /* engine load: clocks of the last block, of 45000 */
    uint16_t underruns;           /* since the start */
    const char *title;
    const char *section;
} viz_t;

void viz_init(void);
void viz_frame(const viz_t *v);
uint32_t viz_cycles(void);        /* CYCLE periods the last viz_frame spent (not counting its waits) */
#endif
