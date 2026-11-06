/* The demo's soundtrack: an original chiptune for the 3-voice device at
 * F00400 (docs/io.md "Sound": on the Spartan-3 audio_lite.v, on the Tang
 * the audio engine's voices 0-2), in A minor over Am - F - C - G.
 *
 *   voice 0  lead: square, a pluck that settles, and a slow vibrato
 *   voice 1  bass: saw, eighth notes, root and octave
 *   voice 2  drums: kick (a triangle falling from 160 Hz), snare and hat
 *            (noise)
 *
 * A row is a sixteenth note, 7 frames at 60 Hz (128 BPM); a pattern is four
 * bars (64 rows). music_tick(n) runs n frames: frame_wait() calls it, so
 * dropped frames do not slow the music. SW1 silences it. On the Spartan-3
 * the voices are made in software (fpga/lib/sid.c) into the sound stream. */

#define FRAMES_PER_ROW 7

#ifdef TANG
static void snd_freq(int v, uint16_t hz) { REG(0xF00400 + 8 * v) = hz; }
static void snd_vol(int v, uint16_t vol) { REG(0xF00402 + 8 * v) = vol; }
static void snd_wave(int v, uint16_t w)  { REG(0xF00404 + 8 * v) = w; }
static void music_fill(void) {}
static void music_pause(void) {}
#else
/* The Spartan-3 has no hardware voices (docs/io.md "Sound"): the software
 * SID (fpga/lib/sid.c) makes them, into the stream, as the same simple
 * device: a frequency in Hz, a volume 0-15, a waveform. Its ring sits in
 * the free RAM above the text buffer. */
#include "../lib/sid.h"
#define MU_RING     0x0FD000UL
#define MU_RING_LEN 2048u                    /* 168 ms */
static void snd_freq(int v, uint16_t hz)
{
    uint16_t f = (uint16_t)(((uint32_t)hz * 17437u) >> 10);   /* x 16,777,216 / 985,248 */
    sid_write((uint8_t)(7 * v), (uint8_t)f);
    sid_write((uint8_t)(7 * v + 1), (uint8_t)(f >> 8));
}
static void snd_vol(int v, uint16_t vol) { sid_level(v, (uint8_t)(vol * 17)); }
static void snd_wave(int v, uint16_t w)
{
    static const uint8_t ctl[4] = { 0x40, 0x10, 0x20, 0x80 };   /* pulse (square), triangle, saw, noise */
    sid_write((uint8_t)(7 * v + 4), ctl[w & 3]);
}
static uint8_t mu_paused = 1;
/* keep the ring filled (once a frame); after a pause, restart it ahead of the player */
static void music_fill(void)
{
    if (mu_paused) { sid_mute(0); mu_paused = 0; }
    uint16_t room = sid_room();
    if (room >= 128) sid_render(room);
}
/* a section's set-up is coming (no frames for a while): mute the stream */
static void music_pause(void)
{
    sid_mute(1);
    mu_paused = 1;
}
#endif

/* notes: MIDI numbers; 0 = hold, 1 = off */
static uint16_t mu_hz(uint8_t n)
{
    static const uint16_t top[12] = { 4186, 4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459, 7902 };
    return (uint16_t)(top[n % 12] >> (9 - n / 12));       /* octave 8 is n / 12 = 9 */
}

/* the lead, a bar at a time: (row, note) pairs, 0xFF ends the bar */
static const uint8_t mu_lead[][18] = {
    /* A: the theme */
    { 0, 76, 2, 69, 4, 72, 6, 76, 8, 74, 10, 72, 12, 69, 15, 1, 0xFF },
    { 0, 77, 3, 76, 6, 72, 8, 69, 10, 72, 12, 77, 14, 76, 0xFF },
    { 0, 79, 2, 76, 4, 72, 6, 67, 8, 72, 10, 76, 12, 79, 14, 81, 0xFF },
    { 0, 79, 4, 77, 6, 76, 8, 74, 12, 71, 15, 1, 0xFF },
    /* B: higher, answering */
    { 0, 81, 4, 79, 6, 76, 8, 72, 10, 74, 12, 76, 0xFF },
    { 0, 77, 2, 81, 4, 84, 8, 81, 12, 77, 14, 79, 0xFF },
    { 0, 76, 4, 79, 8, 84, 12, 79, 0xFF },
    { 0, 74, 2, 76, 4, 77, 6, 79, 8, 83, 12, 79, 14, 1, 0xFF },
};
/* the chords' notes for the arpeggio pattern, and the bass roots */
static const uint8_t mu_chord[4][4] = { { 69, 72, 76, 81 }, { 65, 69, 72, 77 }, { 72, 76, 79, 84 }, { 67, 71, 74, 79 } };
static const uint8_t mu_root[4] = { 45, 41, 48, 43 };

/* the song: each entry a pattern: lead 0 none, 1 theme, 2 answer, 3 arpeggios */
static const uint8_t mu_song[] = { 0, 1, 2, 1, 3, 2, 1, 3 };
#define MU_LEN ((int)sizeof mu_song)

enum { DR_NONE, DR_KICK, DR_SNARE, DR_HAT };
static uint8_t mu_drum(int row, int fill)
{
    int r = row & 15;
    if (fill && r >= 12) return DR_SNARE;              /* a roll into the next pattern */
    if (r == 0 || r == 8 || r == 10 && (row & 32)) return DR_KICK;
    if (r == 4 || r == 12) return DR_SNARE;
    if (r & 1) return DR_NONE;
    return DR_HAT;
}

static uint8_t  mu_pos, mu_row, mu_sub;               /* pattern, row, frame in the row */
static uint8_t  mu_note[2];                           /* lead, bass: the note sounding (0 none) */
static uint8_t  mu_age[3];                            /* frames since each voice's note began */
static uint8_t  mu_dr;                                /* the drum sounding */
static uint8_t  mu_muted;

static void music_silence(void)
{
    for (int v = 0; v < 3; v++) snd_vol(v, 0);
}

static COLD void music_init(void)
{
#ifndef TANG
    sid_init(MU_RING, MU_RING_LEN);
    sid_write(2, 0x00); sid_write(3, 0x08);          /* voice 0's pulse: a square */
    sid_write(24, 15);
#endif
    mu_pos = mu_row = mu_sub = 0;
    mu_note[0] = mu_note[1] = 0;
    mu_dr = DR_NONE;
    snd_wave(0, 0);                                  /* square */
    snd_wave(1, 2);                                  /* saw */
    music_silence();
}

/* a new row: the notes that start on it */
static void mu_row_start(void)
{
    uint8_t pat = mu_song[mu_pos];
    int bar = mu_row >> 4, r = mu_row & 15;
    /* lead */
    uint8_t n = 0;
    if (pat == 1 || pat == 2) {
        const uint8_t *e = mu_lead[(pat - 1) * 4 + bar];
        for (; *e != 0xFF; e += 2)
            if (e[0] == r) { n = e[1]; break; }
    } else if (pat == 3)
        n = mu_chord[bar][r & 3];
    if (n == 1) { mu_note[0] = 0; }
    else if (n) { mu_note[0] = n; mu_age[0] = 0; snd_freq(0, mu_hz(n)); }
    /* bass: eighths, root and octave */
    if (!(r & 1)) {
        mu_note[1] = (uint8_t)(mu_root[bar] + ((r & 2) ? 12 : 0));
        mu_age[1] = 0;
        snd_freq(1, mu_hz(mu_note[1]));
    }
    /* drums */
    uint8_t d = mu_drum(mu_row, bar == 3 && ((mu_pos & 1) || pat == 0));
    if (d != DR_NONE) {
        mu_dr = d;
        mu_age[2] = 0;
        snd_wave(2, d == DR_KICK ? 1 : 3);
    }
}

/* every frame: the envelopes */
static void mu_frame(void)
{
    /* lead: 13, falling to 8; vibrato after a quarter second */
    if (mu_note[0]) {
        uint8_t a = mu_age[0];
        int vol = 13 - (a >> 2);
        if (vol < 8) vol = 8;
        if (mu_song[mu_pos] == 3) vol -= 3;           /* the arpeggios softer */
        snd_vol(0, (uint16_t)vol);
        if (a >= 15) {
            uint16_t f = mu_hz(mu_note[0]);
            int8_t w = SIN(a * 10) >> 5;              /* -3 .. 3 */
            snd_freq(0, (uint16_t)(f + ((int16_t)(f >> 7) * w >> 2)));
        }
        if (a < 255) mu_age[0]++;
    } else
        snd_vol(0, 0);
    /* bass: 11, short */
    if (mu_note[1]) {
        uint8_t a = mu_age[1];
        snd_vol(1, (uint16_t)(a < 9 ? 11 - (a >> 1) : 0));
        if (a < 255) mu_age[1]++;
    }
    /* drums */
    uint8_t a = mu_age[2];
    switch (mu_dr) {
    case DR_KICK:
        snd_freq(2, (uint16_t)(a < 8 ? 160 - 15 * a : 45));
        snd_vol(2, (uint16_t)(a < 8 ? 15 - 2 * a : 0));
        break;
    case DR_SNARE:
        snd_freq(2, 1500);
        snd_vol(2, (uint16_t)(a < 6 ? 11 - 2 * a : 0));
        break;
    case DR_HAT:
        snd_freq(2, 3000);
        snd_vol(2, (uint16_t)(a < 2 ? 5 - 2 * a : 0));
        break;
    default:
        snd_vol(2, 0);
    }
    if (a < 255) mu_age[2]++;
}

static void music_tick(uint16_t n)
{
    if (SWITCHES & 2) {
        if (!mu_muted) { music_silence(); music_pause(); mu_muted = 1; }
        return;
    }
    mu_muted = 0;
    if (n > 8) n = 8;                                 /* after a long set-up: carry on, do not race */
    while (n--) {
        if (mu_sub == 0) mu_row_start();
        mu_frame();
        if (++mu_sub == FRAMES_PER_ROW) {
            mu_sub = 0;
            if (++mu_row == 64) {
                mu_row = 0;
                if (++mu_pos == MU_LEN) mu_pos = 1;   /* the intro once */
            }
        }
    }
    music_fill();
}
