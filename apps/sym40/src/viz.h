/* The falling-notes visualizer, shared by sym40 and toccata (viz.c): notes fall on playfield B
 * (a scrolled 360-line ring) onto a keyboard C1-C7 on playfield A whose keys light in their
 * section's colour; where a note lands a white-hot flash (a sprite) and a spray of sparks; under the
 * keyboard each section's ball bounces on its attacks over a fire the notes feed. Memory: A at
 * 0x200000 / 0x240000 (double-buffered), B at 0x280000, sprite pixels at 0x2C0000.
 *
 *   viz_init(&cfg);                     the sections (up to VIZ_MAXSEC): names, colours; the speed
 *   for (;;) { fill a viz_t; viz_frame(&v); }
 *
 * Time is in ticks, the app's unit (an eighth note in sym40, a fixed 1/47 s in toccata); the notes
 * fall px16/16 pixels a tick, and the app lists them from a little before now to 190 pixels ahead.
 * viz_frame waits for the last frame's flip, then in vertical blank sends the sprites and palette,
 * draws the next frame into the hidden buffers and asks for the flip; the caller's work (and the
 * music interrupt) overlaps the wait. */
#ifndef APPS_VIZ_H
#define APPS_VIZ_H
#include <stdint.h>

#define VIZ_MAXSEC 10
typedef struct {
    uint8_t nsec;                 /* sections: a colour, a ball, a share of the fire each */
    const char *const *name;      /* short labels (up to 80 / nsec characters) */
    const uint8_t (*rgb)[3];
    uint16_t px16;                /* the falling speed: pixels a tick, 1/16 */
} viz_cfg_t;
typedef struct { uint8_t inst; uint8_t midi; uint8_t vel; } viz_note_t;     /* a note-on */
/* a note on its way down, or sounding: start and length in 1/16 ticks (an eighth note) from now */
typedef struct { uint8_t inst, midi; int16_t start; uint16_t len; } viz_fall_t;
typedef struct {
    uint32_t now16;               /* now, in 1/16 ticks since the start (never wraps: the notes' scroll) */
    viz_fall_t fall[192];         /* the notes from a little before now to VIZ_LOOK on */
    uint16_t nfall;
    uint16_t level[VIZ_MAXSEC];        /* each section's loudness now, 0..0x7FFF (envelope x volume) */
    int16_t  scope[256];          /* the last 256 output samples (mono), oldest first */
    viz_note_t notes[32];         /* note-ons since the last frame */
    uint8_t  nnotes;
    uint16_t bar, beat;           /* position: bar (from 1); beat's bit 0 blinks the HUD's marker */
    uint16_t progress;            /* through the piece, 0..65535: the bar along the bottom */
    uint8_t  voices;              /* engine nodes sounding */
    uint16_t blocks_busy;         /* engine load: clocks of the last block, of 45000 */
    uint16_t underruns;           /* since the start */
    const char *title;
    const char *section;
} viz_t;

void viz_init(const viz_cfg_t *c);
void viz_frame(const viz_t *v);
uint32_t viz_cycles(void);        /* CYCLE periods the last viz_frame spent (not counting its waits) */
#endif
