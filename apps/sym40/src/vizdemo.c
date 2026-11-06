/* vizdemo: the visualizer (viz.c) driven by made-up orchestra data: the opening of Symphony No. 40
 * as note-ons by section (the violins' sighing motif, the violas' eighths, the bass line, wind
 * chords), with section levels and a scope from the notes sounding. Prints the CYCLE periods per
 * viz_frame every 64 frames. -DFRAMES=n stops after n frames (fsim writes --screenshot then).
 *   fcc -O2 vizdemo.c viz.c -o vizdemo.bin
 *   fsim --xram 7 --true-color --screenshot shot.ppm vizdemo.bin */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "viz.h"

static viz_t v;
static uint16_t lv[VI__N];                      /* envelope per section */
static uint8_t sound[VI__N];                    /* each section's latest note */

/* the first theme (G minor), eighths: Eb D D | Eb D D | Eb D D Bb' ... as MIDI, 0 = rest */
static const uint8_t theme[32] = { 75, 74, 74, 0, 75, 74, 74, 0, 75, 74, 74, 82, 0, 82, 81, 79,
                                   79, 77, 75, 75, 74, 72, 70, 70, 69, 67, 67, 0, 0, 0, 0, 0 };
static const uint8_t bassln[8] = { 43, 43, 43, 43, 45, 45, 38, 38 };
static const uint8_t chords[4][3] = { { 67, 70, 74 }, { 66, 69, 72 }, { 67, 70, 74 }, { 62, 66, 69 } };

static void on(uint8_t inst, uint8_t midi, uint8_t vel)
{
    if (v.nnotes < 32) { v.notes[v.nnotes].inst = inst; v.notes[v.nnotes].midi = midi; v.notes[v.nnotes].vel = vel; v.nnotes++; }
    if (lv[inst] < (uint16_t)(vel * 200)) lv[inst] = (uint16_t)(vel * 200);
    sound[inst] = midi;
}

static int16_t sn[64];

int main(void)
{
    for (uint16_t i = 0; i < 64; i++) {             /* a coarse sine for the scope */
        int32_t x = (int32_t)i * 1024 - 32768;          /* -32768..31744 over the cycle: a parabola pair */
        int32_t t = x < 0 ? -x : x;
        sn[i] = (int16_t)((32768 - t) * x / 16384);
    }
    viz_init();
    v.title = "W. A. Mozart: Symphony No. 40 in G minor, K. 550 - I. Molto allegro (test data)";
    uint16_t f0 = FRAME, last_e = 0xFFFF;
    uint32_t sum = 0, n = 0;
    for (uint32_t k = 0;; k++) {
        uint16_t f = (uint16_t)(FRAME - f0);
        uint16_t e = (uint16_t)(f / 8);                 /* an eighth every 8 video frames (~112 bpm) */
        v.nnotes = 0;
        if (e != last_e) {
            last_e = e;
            uint8_t m = theme[e & 31];
            if (m) { on(VI_VIOLIN1, m, 100); on(VI_VIOLIN2, (uint8_t)(m - 12), 80); on(VI_FLUTE, (uint8_t)(m + 12), (e & 32) ? 70 : 0 + 20); }
            on(VI_VIOLA, (uint8_t)(62 + ((e & 1) ? 5 : 0)), 60);
            if ((e & 1) == 0) { on(VI_BASS, bassln[(e >> 1) & 7], 90); on(VI_CELLO, (uint8_t)(bassln[(e >> 1) & 7] + 12), 80); }
            if ((e & 7) == 0) {
                const uint8_t *c = chords[(e >> 3) & 3];
                on(VI_OBOE, c[2], 70); on(VI_CLARINET, c[1], 70); on(VI_BASSOON, (uint8_t)(c[0] - 12), 70);
                if ((e & 15) == 0) { on(VI_HORN, c[0], 90); on(VI_HORN, (uint8_t)(c[0] - 12), 90); }
            }
        }
        for (uint8_t i = 0; i < VI__N; i++) {
            lv[i] = (uint16_t)(lv[i] - (lv[i] >> 4));
            v.level[i] = lv[i];
        }
        /* the scope: the sections' latest notes, summed */
        for (uint16_t s = 0; s < 256; s++) {
            int32_t acc = 0;
            for (uint8_t i = 0; i < VI__N; i += 2)
                if (sound[i]) acc += (int32_t)sn[(uint8_t)(s * (sound[i] - 30) / 8 + f * 7) & 63] * (lv[i] >> 6) >> 9;
            v.scope[s] = (int16_t)(acc > 32767 ? 32767 : acc < -32767 ? -32767 : acc);
        }
        v.bar = (uint16_t)(1 + e / 8); v.beat = (uint16_t)((e / 4) & 1);
        v.blocks_busy = (uint16_t)(14000 + (f * 37) % 6000);
        v.section = e < 64 ? "Exposition: first theme (violins, piano)" : "Exposition: tutti";
        viz_frame(&v);
        sum += viz_cycles(); n++;
        if ((k & 63) == 63) { printf("frames %lu: %lu CYCLE periods a frame (avg)\n", (unsigned long)(k + 1), (unsigned long)(sum / n)); sum = n = 0; }
#ifdef FRAMES
        if (k + 1 >= FRAMES) return 0;
#endif
    }
}
