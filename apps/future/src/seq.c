/* The Future: the music. An uplifting techno track at 128 BPM, 40 phrases of 8 bars (15 s each, 600 s),
 * played by a 16th-note sequencer whose instruments grow with the eras: each phrase has a level and the
 * parts that play, and the level says how a part sounds:
 *   0  one square voice, no envelope (a beeper); the kick a click on it
 *   1  two square voices (lead, bass), the kick a click on the bass
 *   2  a sound chip: pulse lead and arpeggio, triangle bass, a noise drum voice, volumes stepped by the CPU
 *   3  a SID: ADSR on every voice, pulse-width modulation, the bass through the resonant filter
 *   4  4 sample channels, Amiga style: drums on one channel, bass, lead, arpeggio; hard stereo; no interpolation
 *   5  8 channels: kick, snare, hats on their own, echoes on the lead and arpeggio, interpolation, a string pad
 *   6  16: a sub bass, a supersaw pad through the filter with sidechain pumping, a 3-saw lead, crashes, risers
 *   7  all 32 voices: a 5-saw lead, a 16-voice pad
 * Time is the audio engine's 48 kHz frame counter: a 16th note is 5625 frames exactly. */
#define VREG(v, w) IO(0x1800 + 64 * (v) + 2 * (w))
#define C_GATE 1
#define C_ENV 2
#define C_PULSE (1 << 2)
#define C_TRI (2 << 2)
#define C_SAW (3 << 2)
#define C_NOISE (4 << 2)
#define C_INTERP 0x80
#define C_LOOP 0x100
#define C_FILT 0x2000
#define STEP_FR 5625UL
#define SONG_STEPS 5120                         /* 40 phrases x 128 */

#define SMP 0x0E9A00UL                           /* the samples, copied from assets.h (the engine reads SDRAM) */

static uint8_t eng_ok;
static uint32_t scount(void)
{
    uint16_t h, l, h2;
    if (!eng_ok) {                              /* fsim: no engine; 800 frames a video frame */
        static uint16_t lastf;
        static uint32_t t;
        uint16_t f = FRAME;
        t += (uint32_t)(uint16_t)(f - lastf) * 800UL;
        lastf = f;
        return t;
    }
    do {
        h = IO(0x1012);
        l = IO(0x1010);
        h2 = IO(0x1012);
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}

static uint32_t mstep(int16_t m)                /* a chip voice's step for MIDI note m */
{
    int16_t o = 0;
    while (m < 96) { m = (int16_t)(m + 12); o++; }
    while (m > 107) { m = (int16_t)(m - 12); o--; }
    return o >= 0 ? note_step7[m - 96] >> o : note_step7[m - 96] << -o;
}
static void v_step(uint8_t v, uint32_t s) { VREG(v, 1) = (uint16_t)s; VREG(v, 2) = (uint16_t)(s >> 16); }
static void v_vol(uint8_t v, uint16_t l, uint16_t r) { VREG(v, 3) = l; VREG(v, 4) = r; }
static void v_adsr(uint8_t v, uint16_t a, uint16_t d, uint16_t s, uint16_t r)
{
    VREG(v, 6) = a; VREG(v, 7) = d; VREG(v, 8) = s; VREG(v, 9) = r;
}
static void v_off(uint8_t v) { VREG(v, 0) = 0; }
static void v_pos0(uint8_t v) { VREG(v, 11) = 0; VREG(v, 12) = 0; VREG(v, 13) = 0; }
static void v_smp(uint8_t v, uint16_t off, uint16_t len)
{
    uint32_t a = SMP + off;
    VREG(v, 16) = (uint16_t)a; VREG(v, 17) = (uint16_t)(a >> 16);
    VREG(v, 18) = 0; VREG(v, 19) = 0;
    VREG(v, 20) = len; VREG(v, 21) = 0;
}
/* a one-shot sample */
static void hit(uint8_t v, uint16_t off, uint16_t len, uint16_t step, uint16_t vl, uint16_t vr, uint16_t fl)
{
    VREG(v, 0) = 0;
    v_smp(v, off, len);
    v_step(v, step);
    v_vol(v, vl, vr);
    v_pos0(v);
    VREG(v, 0) = (uint16_t)(fl | C_GATE);
}
/* a looped 256-frame waveform at MIDI note n, with the envelope */
static void wave_on(uint8_t v, uint16_t off, int16_t n, uint16_t fl)
{
    VREG(v, 0) = (uint16_t)(C_LOOP | C_ENV | fl);
    v_smp(v, off, 256);
    v_step(v, mstep(n));
    v_pos0(v);
    VREG(v, 0) = (uint16_t)(C_LOOP | C_ENV | fl | C_GATE);
}
static void chip_on(uint8_t v, uint16_t src, uint32_t step)
{
    VREG(v, 0) = src;
    v_step(v, step);
    VREG(v, 0) = (uint16_t)(src | C_GATE);
}

/* ---------------------------------------------------------------- the song */
#define P_KICK 0x0001
#define P_HAT 0x0002
#define P_CLAP 0x0004
#define P_BASS 0x0008
#define P_ARP 0x0010
#define P_LEAD 0x0020
#define P_PAD 0x0040
#define P_RISER 0x0080                          /* noise rising over the last two bars */
#define P_FILL 0x0100                           /* a snare roll in the last bar */
#define P_CRASH 0x0200                          /* a crash on the first beat */
#define P_ROLL 0x0400                           /* rolling 16th bass */
#define P_HAT16 0x0800                          /* 16th hats */
#define P_FADE 0x1000                           /* everything fades over the phrase */
#define P_HALF 0x2000                           /* the drums come in half way */
#define ALL (P_KICK | P_HAT | P_CLAP | P_BASS | P_ARP | P_LEAD)

typedef struct { uint8_t lvl, prog, mel; uint16_t parts; } phrase_t;
static const phrase_t song[40] = {
    {0, 0, 1, P_LEAD},                                              /*  0 jets: the theme on a beeper */
    {0, 0, 1, P_LEAD | P_KICK},
    {1, 2, 2, P_LEAD | P_KICK | P_BASS},                            /*  2 a second voice */
    {1, 0, 1, P_LEAD | P_KICK | P_BASS},
    {2, 0, 1, ALL | P_FILL},                                        /*  4 the sound chip */
    {2, 1, 0, P_ARP | P_BASS | P_HAT | P_RISER},                    /*  5 rockets: the countdown */
    {2, 0, 1, ALL},                                                 /*  6 lift-off */
    {2, 2, 2, ALL | P_FILL},
    {3, 0, 1, ALL},                                                 /*  8 the SID: staging */
    {3, 3, 3, P_ARP | P_LEAD | P_BASS | P_RISER},                   /*  9 coast to the Moon */
    {3, 0, 1, ALL},                                                 /* 10 the Moon: descent */
    {3, 2, 2, ALL | P_FILL},
    {3, 0, 1, ALL},                                                 /* 12 touchdown */
    {3, 3, 3, P_ARP | P_LEAD},                                      /* 13 the walk */
    {3, 1, 0, P_ARP | P_BASS | P_HAT | P_RISER | P_HALF | P_KICK},  /* 14 earthrise */
    {3, 0, 1, ALL | P_FILL},                                        /* 15 lift-off */
    {4, 0, 1, ALL | P_CRASH},                                       /* 16 the station: 4 sample channels */
    {4, 2, 2, ALL},
    {4, 0, 1, ALL | P_ROLL},
    {4, 3, 3, P_ARP | P_LEAD | P_BASS},                             /* 19 the walk in space */
    {4, 1, 0, P_ARP | P_BASS | P_HAT | P_KICK | P_RISER | P_FILL},
    {4, 2, 2, ALL | P_CRASH},
    {5, 0, 1, ALL | P_PAD | P_CRASH},                               /* 22 the Moon base: 8 channels */
    {5, 2, 2, ALL | P_PAD | P_ROLL},
    {5, 0, 1, ALL | P_PAD | P_ROLL | P_HAT16 | P_CRASH},            /* 24 high resolution */
    {5, 3, 3, P_PAD | P_ARP | P_LEAD},                              /* 25 night */
    {5, 1, 0, P_ARP | P_BASS | P_HAT | P_PAD | P_RISER | P_FILL | P_HALF | P_KICK},
    {6, 0, 1, ALL | P_ROLL | P_PAD | P_CRASH | P_HAT16},            /* 27 the planets: 16 */
    {6, 2, 2, ALL | P_ROLL | P_PAD | P_HAT16},
    {6, 0, 1, ALL | P_ROLL | P_PAD | P_HAT16 | P_FILL},
    {6, 3, 3, P_PAD | P_LEAD | P_ARP},                              /* 30 Saturn */
    {6, 1, 0, P_PAD | P_ARP | P_BASS | P_HAT | P_RISER | P_FILL | P_HALF | P_KICK},
    {6, 2, 2, ALL | P_ROLL | P_PAD | P_CRASH},
    {7, 3, 3, P_PAD | P_ARP | P_LEAD | P_RISER | P_FILL},           /* 33 the starship: 32 */
    {7, 0, 1, ALL | P_ROLL | P_PAD | P_CRASH | P_HAT16},            /* 34 launch */
    {7, 2, 2, ALL | P_ROLL | P_PAD | P_HAT16},
    {7, 0, 1, ALL | P_ROLL | P_PAD | P_HAT16 | P_FILL},
    {7, 2, 2, ALL | P_ROLL | P_PAD | P_CRASH},                      /* 37 credits */
    {7, 0, 1, ALL | P_PAD},
    {7, 3, 3, P_PAD | P_LEAD | P_FADE},                             /* 39 the end */
};

/* chords: the bass's root (MIDI) and minor; progressions of four, two bars each (D major / B minor) */
static const uint8_t ch_root[6] = {47, 43, 50, 45, 40, 42};    /* Bm G D A Em F#m */
static const uint8_t ch_min[6] = {1, 0, 0, 0, 1, 1};
static const uint8_t progs[4][4] = {{0, 1, 2, 3}, {1, 3, 0, 0}, {2, 3, 0, 1}, {1, 2, 3, 0}};

/* melodies: (step, note, length) over 8 bars, 255 ends */
static const uint8_t mel_theme[] = {
    0, 78, 4, 4, 74, 2, 6, 76, 2, 8, 78, 6, 14, 83, 2,
    16, 81, 4, 20, 78, 4, 24, 76, 2, 26, 74, 2, 28, 76, 4,
    32, 74, 4, 36, 71, 2, 38, 74, 2, 40, 79, 6, 46, 83, 2,
    48, 81, 4, 52, 79, 4, 56, 78, 2, 58, 76, 2, 60, 74, 4,
    64, 69, 2, 66, 74, 2, 68, 78, 4, 72, 81, 4, 76, 78, 2, 78, 81, 2,
    80, 86, 6, 86, 85, 2, 88, 81, 4, 92, 78, 4,
    96, 76, 4, 100, 81, 4, 104, 85, 6, 110, 83, 2,
    112, 81, 8, 120, 76, 4, 124, 73, 4, 255};
static const uint8_t mel_theme2[] = {
    0, 74, 2, 2, 78, 2, 4, 81, 4, 8, 86, 6, 14, 85, 2,
    16, 86, 4, 20, 81, 4, 24, 78, 4, 28, 81, 4,
    32, 76, 2, 34, 81, 2, 36, 85, 4, 40, 88, 6, 46, 86, 2,
    48, 85, 4, 52, 81, 4, 56, 76, 4, 60, 81, 4,
    64, 86, 6, 70, 85, 2, 72, 83, 4, 76, 78, 4,
    80, 83, 4, 84, 86, 4, 88, 90, 8,
    96, 79, 2, 98, 83, 2, 100, 86, 4, 104, 88, 4, 108, 86, 4,
    112, 83, 6, 118, 81, 2, 120, 79, 4, 124, 81, 4, 255};
static const uint8_t mel_bd[] = {
    0, 86, 14, 16, 83, 14, 32, 81, 14, 48, 78, 14,
    64, 76, 8, 72, 78, 8, 80, 81, 14, 96, 78, 14, 112, 74, 8, 120, 76, 8, 255};
static const uint8_t *const mels[4] = {0, mel_theme, mel_theme2, mel_bd};
static const uint8_t arp_pat[16] = {0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 3};
static const uint16_t master_lvl[8] = {0x0C00, 0x0B00, 0x0A00, 0x0A00, 0x0A00, 0x0900, 0x0800, 0x0780};

/* ---------------------------------------------------------------- state */
uint16_t q_cur;                                 /* the step playing (16ths since the start) */
static uint16_t q_step;                         /* the next step */
static uint32_t q_next, q_t0;                   /* when the next step starts; when the current one did */
static uint8_t q_on, q_lvl = 0xFF;
uint8_t song_done;
static uint32_t e_next;                         /* the 50 Hz ticks */
uint32_t ticks50;
static const phrase_t *ph;
static const uint8_t *mp;                       /* the melody's next event */
static uint8_t chord;                           /* the chord playing (0-5) */
static uint8_t ld_note, ld_age, ld_left;        /* the lead's note, ticks since it started, steps left */
static uint8_t bs_note, bs_left;
static uint16_t ld_vol, ar_vol;                 /* level 2: volumes stepped down by the CPU */
static uint8_t eh_l[4], eh_a[4];                /* echoes: the lead's and arpeggio's notes 3 steps ago */
static uint8_t dk;                              /* drum ticks left (levels 0-3) */
static uint8_t dk_kind;                         /* 0 kick, 1 snare, 2 hat */
static uint8_t dk_v;                            /* levels 0-1: the voice the click is on */
static uint16_t fade_k = 256;
uint16_t kick_step = 0xFFFF;                    /* the last kick's step (the pictures flash on it) */

static uint8_t ld_n(void) { return q_lvl >= 6 ? (q_lvl == 7 ? 5 : 3) : 1; }   /* lead voices */
static const uint8_t ld_vo[5] = {5, 11, 12, 13, 14};
static const int8_t ld_det[5] = {0, -3, 3, -6, 6};
static uint8_t pd_n(void) { return q_lvl == 7 ? 16 : (q_lvl == 6 ? 6 : (q_lvl == 5 ? 2 : 0)); }

static void lead_step(uint32_t st)
{
    uint8_t i, n = ld_n();
    for (i = 0; i < n; i++)
        v_step(ld_vo[i], (uint32_t)((int32_t)st + (int32_t)(st >> 9) * ld_det[i]));
}

/* the instruments of each level */
static void set_level(uint8_t l)
{
    uint8_t v;
    for (v = 0; v < 32; v++) v_off(v);
    q_lvl = l;
    IO(0x1000) = (uint16_t)((uint32_t)master_lvl[l] * fade_k >> 8);
    for (v = 0; v < 32; v++) { v_vol(v, 0, 0); VREG(v, 5) = 0x800; v_adsr(v, 0xFFFF, 0, 0xFFFF, 0xFFFF); }
    switch (l) {
    case 0:
    case 1:
        v_vol(5, 0x70, 0x70);
        v_vol(3, 0x60, 0x60);
        break;
    case 2:
        VREG(5, 5) = 0x400; v_vol(5, 0x80, 0x60);
        VREG(4, 5) = 0x200; v_vol(4, 0x40, 0x58);
        v_vol(3, 0xA0, 0xA0);
        v_vol(2, 0x60, 0x60);
        break;
    case 3:
        VREG(5, 5) = 0x600; v_vol(5, 0x70, 0x58); v_adsr(5, 0x1800, 40, 0xB000, 150);
        VREG(4, 5) = 0x300; v_vol(4, 0x38, 0x50); v_adsr(4, 0xFFFF, 400, 0x1000, 300);
        VREG(3, 5) = 0x500; v_vol(3, 0xC0, 0xC0); v_adsr(3, 0xFFFF, 120, 0x4000, 200);
        v_vol(2, 0x70, 0x70); v_adsr(2, 0xFFFF, 500, 0, 500);
        IO(0x1006) = 0x1800; IO(0x1002) = 1;   /* the bass's filter: low pass, resonant */
        break;
    default:                                    /* 4-7: samples */
        {
            uint8_t am = l == 4;
            v_vol(0, 0xC0, am ? 0x30 : 0xC0);
            v_vol(1, am ? 0 : 0x90, am ? 0 : 0x80);
            v_vol(2, am ? 0 : 0x38, am ? 0 : 0x50);
            v_vol(3, am ? 0x30 : 0xA0, am ? 0xB0 : 0xA0); v_adsr(3, 0xFFFF, 160, 0x5000, 400);
            v_vol(5, am ? 0x20 : 0x60, am ? 0x90 : 0x50); v_adsr(5, 0x2000, 30, 0xC000, 220);
            v_vol(4, am ? 0x80 : 0x30, am ? 0x20 : 0x48); v_adsr(4, 0xFFFF, 500, 0x0800, 500);
            v_vol(7, 0x20, 0x0C); v_adsr(7, 0xFFFF, 500, 0x0800, 500);
            v_vol(8, 0x0C, 0x28); v_adsr(8, 0x2000, 30, 0xC000, 220);
            v_vol(6, 0x50, 0x50);
            v_vol(9, 0x70, 0x70); v_adsr(9, 0x4000, 0, 0xFFFF, 300);         /* the sub (6, 7) */
            v_vol(10, 0x30, 0x30); v_adsr(10, 0x0400, 20, 0xA000, 600);      /* level 5's pad */
            if (l >= 6) {
                uint8_t i;
                for (i = 0; i < 5; i++) {
                    v_vol(ld_vo[i], (uint16_t)(l == 7 ? 0x1C + (i & 1) * 8 : 0x26), (uint16_t)(l == 7 ? 0x24 - (i & 1) * 8 : 0x26));
                    v_adsr(ld_vo[i], 0x2000, 30, 0xC000, 220);
                }
                v_vol(8, 0x08, 0x20);
                for (v = 16; v < 32; v++) v_adsr(v, 0x0300, 0, 0xFFFF, 500);
                IO(0x1006) = 0x5000; IO(0x1002) = 1; IO(0x1004) = 0x1800;
            }
        }
        break;
    }
}

static uint8_t chord_tone(uint8_t i)            /* 0 root, 1 third, 2 fifth, 3 octave (from the bass root) */
{
    static const uint8_t iv[2][4] = {{0, 4, 7, 12}, {0, 3, 7, 12}};
    return (uint8_t)(ch_root[chord] + iv[ch_min[chord]][i]);
}

static void lead_on(uint8_t n)
{
    uint8_t l = q_lvl;
    ld_note = n; ld_age = 0;
    if (l <= 1) chip_on(5, C_PULSE, mstep(n));
    else if (l == 2) { ld_vol = 0x90; v_vol(5, ld_vol, ld_vol - 0x20); chip_on(5, C_PULSE, mstep(n)); }
    else if (l == 3) chip_on(5, C_PULSE | C_ENV, mstep(n));
    else if (l <= 5) wave_on(5, S_LEAD_OFF, n, l == 5 ? C_INTERP : 0);
    else {
        uint8_t i, k = ld_n();
        for (i = 0; i < k; i++) chip_on(ld_vo[i], C_SAW | C_ENV, mstep(n));
        lead_step(mstep(n));
    }
}
static void lead_off(void)
{
    uint8_t l = q_lvl;
    if (l <= 2) VREG(5, 0) = C_PULSE;
    else if (l == 3) VREG(5, 0) = C_PULSE | C_ENV;
    else if (l <= 5) VREG(5, 0) = C_LOOP | C_ENV | (l == 5 ? C_INTERP : 0);
    else {
        uint8_t i, k = ld_n();
        for (i = 0; i < k; i++) VREG(ld_vo[i], 0) = C_SAW | C_ENV;
    }
    ld_note = 0;
}
static void bass_on(uint8_t n, uint8_t len)
{
    uint8_t l = q_lvl;
    bs_note = n; bs_left = len;
    if (l <= 1) chip_on(3, C_PULSE, mstep(n));
    else if (l == 2) chip_on(3, C_TRI, mstep(n));
    else if (l == 3) chip_on(3, C_PULSE | C_ENV | C_FILT, mstep(n));
    else {
        wave_on(3, S_BASS_OFF, n, l >= 5 ? C_INTERP : 0);
        if (l >= 6) chip_on(9, C_TRI | C_ENV, mstep((int16_t)(n - 12)));
    }
}
static void bass_off(void)
{
    uint8_t l = q_lvl;
    if (l <= 1) VREG(3, 0) = C_PULSE;
    else if (l == 2) VREG(3, 0) = C_TRI;
    else if (l == 3) VREG(3, 0) = C_PULSE | C_ENV | C_FILT;
    else { VREG(3, 0) = C_LOOP | C_ENV | (l >= 5 ? C_INTERP : 0); VREG(9, 0) = C_TRI | C_ENV; }
    bs_note = 0;
}
static void arp_on(uint8_t v, uint8_t n)
{
    uint8_t l = q_lvl;
    if (l == 2) { ar_vol = 0x50; v_vol(4, ar_vol, ar_vol + 0x10); chip_on(4, C_PULSE, mstep(n)); }
    else if (l == 3) chip_on(4, C_PULSE | C_ENV, mstep(n));
    else if (l >= 4) wave_on(v, S_PLUCK_OFF, n, l >= 5 ? C_INTERP : 0);
}
static void pad_on(void)
{
    uint8_t n = pd_n(), i;
    if (!n) return;
    if (q_lvl == 5) {
        wave_on(10, S_STRING_OFF, (int16_t)(chord_tone(1) + 12), C_INTERP);
        return;
    }
    for (i = 0; i < n; i++) {
        uint8_t v = (uint8_t)(16 + i), k = q_lvl == 7 ? 4 : 2;
        uint8_t note = (uint8_t)(chord_tone((uint8_t)(i / k)) + 12);
        uint32_t st = mstep(note);
        int8_t d = (int8_t)((i % k) * 2 - (k - 1));
        VREG(v, 0) = C_SAW | C_ENV | C_FILT;
        v_step(v, (uint32_t)((int32_t)st + (int32_t)(st >> 10) * d));
        VREG(v, 0) = C_SAW | C_ENV | C_FILT | C_GATE;
    }
}
static void pad_retune(void)                    /* the chord changed: the pad glides to it (no retrigger) */
{
    uint8_t n = pd_n(), i;
    if (q_lvl == 5) { v_step(10, mstep((int16_t)(chord_tone(1) + 12))); return; }
    for (i = 0; i < n; i++) {
        uint8_t k = q_lvl == 7 ? 4 : 2;
        uint32_t st = mstep((int16_t)(chord_tone((uint8_t)(i / k)) + 12));
        int8_t d = (int8_t)((i % k) * 2 - (k - 1));
        v_step((uint8_t)(16 + i), (uint32_t)((int32_t)st + (int32_t)(st >> 10) * d));
    }
}

/* drums */
static void drum(uint8_t kind, uint8_t vel)     /* 0 kick, 1 snare, 2 closed hat, 3 open hat, 4 crash */
{
    uint8_t l = q_lvl;
    if (kind == 0) kick_step = q_cur;
    if (l <= 1) {
        if (kind) return;
        dk_v = l == 0 ? 5 : 3;
        dk = 3; dk_kind = 0;
        chip_on(dk_v, C_PULSE, mstep(64));
        return;
    }
    if (l <= 3) {
        static const uint32_t ns[5] = {0x7000, 0x48000, 0x80000, 0x80000, 0x60000};
        static const uint8_t nt[5] = {4, 7, 1, 5, 12};
        if (dk && dk_kind < kind) return;       /* the kick has priority */
        dk = nt[kind]; dk_kind = kind;
        VREG(2, 10) = kind == 0 ? 1 : 0;
        if (l == 2) v_vol(2, (uint16_t)(kind == 2 ? 0x30 : 0x90), (uint16_t)(kind == 2 ? 0x30 : 0x90));
        else {
            v_adsr(2, 0xFFFF, (uint16_t)(kind == 0 ? 2500 : (kind == 2 ? 3000 : 700)), 0, 900);
            v_vol(2, (uint16_t)(kind == 2 ? 0x40 : 0xB0), (uint16_t)(kind == 2 ? 0x50 : 0xB0));
        }
        chip_on(2, (uint16_t)(C_NOISE | (l == 3 ? C_ENV : 0)), ns[kind]);
        if (kind == 0) {                        /* the kick: a pitch drop on the noise voice */
            VREG(2, 0) = (uint16_t)(C_PULSE | (l == 3 ? C_ENV : 0));
            v_step(2, mstep(45));
            VREG(2, 0) = (uint16_t)(C_PULSE | (l == 3 ? C_ENV : 0) | C_GATE);
        }
        return;
    }
    {
        uint16_t fl = l >= 5 ? C_INTERP : 0;
        uint16_t g = (uint16_t)(vel << 1);      /* vel 0-128 -> 0-256 */
        uint8_t v = l == 4 ? 0 : (kind == 0 ? 0 : (kind == 1 ? 1 : (kind == 4 ? 6 : 2)));
        uint16_t vl = 0xC0, vr = 0xC0;
        if (l == 4) {
            static uint8_t busy;                /* one channel: the kick is not cut by a hat */
            if (kind >= 2 && busy && (uint8_t)(q_cur & 3) == 1) return;
            busy = kind == 0;
            vl = 0xD0; vr = 0x40;
        } else if (kind == 1) { vl = 0x90; vr = 0x80; }
        else if (kind == 2 || kind == 3) { vl = 0x38; vr = 0x58; }
        else if (kind == 4) { vl = 0x58; vr = 0x58; }
        vl = (uint16_t)(vl * g >> 8); vr = (uint16_t)(vr * g >> 8);
        switch (kind) {
        case 0: hit(v, S_KICK_OFF, S_KICK_LEN, S_KICK_STEP, vl, vr, fl); break;
        case 1: hit(v, S_SNARE_OFF, S_SNARE_LEN, S_SNARE_STEP, vl, vr, fl); break;
        case 2: hit(v, S_CHAT_OFF, S_CHAT_LEN, S_CHAT_STEP, vl, vr, fl); break;
        case 3: hit(v, S_OHAT_OFF, S_OHAT_LEN, S_OHAT_STEP, vl, vr, fl); break;
        default: hit(v, S_CRASH_OFF, S_CRASH_LEN, S_CRASH_STEP, vl, vr, fl); break;
        }
    }
}

/* ---------------------------------------------------------------- one 16th */
static void seq_step(uint16_t s)
{
    uint8_t p = (uint8_t)(s >> 7), r = (uint8_t)(s & 127), b16 = (uint8_t)(s & 15), bar = (uint8_t)(r >> 4);
    uint16_t pa;
    uint8_t drums;
    q_cur = s;
    ph = &song[p];
    pa = ph->parts;
    if (r == 0) {
        if (ph->lvl != q_lvl) set_level(ph->lvl);
        mp = mels[ph->mel];
        ld_left = 0;
        if (ld_note) lead_off();
        if (pa & P_FADE) fade_k = 256;
    }
    {
        uint8_t c = progs[ph->prog][r >> 5];
        if (c != chord || r == 0) {
            chord = c;
            if (pa & P_PAD) { if (r == 0) pad_on(); else pad_retune(); }
            else if (r == 0 && pd_n()) { uint8_t v; for (v = 16; v < 32; v++) VREG(v, 0) = C_SAW | C_ENV | C_FILT; VREG(10, 0) = C_LOOP | C_ENV | C_INTERP; }
        }
    }
    if ((pa & P_FADE) && fade_k > 2) fade_k = (uint16_t)(256 - (r << 1));
    if (pa & P_FADE) IO(0x1000) = (uint16_t)((uint32_t)master_lvl[q_lvl] * fade_k >> 8);

    /* the drums */
    drums = !(pa & P_HALF) || bar >= 4;
    if ((pa & P_FILL) && bar == 7) {            /* the roll: 8ths, then 16ths, louder */
        if (b16 < 8 ? !(b16 & 1) : 1) drum(1, (uint8_t)(48 + b16 * 5));
        if ((pa & P_KICK) && !(b16 & 3) && q_lvl >= 4) drum(0, 128);
    } else if (drums) {
        if ((pa & P_KICK) && !(b16 & 3)) drum(0, 128);
        if ((pa & P_CLAP) && (b16 == 4 || b16 == 12)) drum(1, 110);
        if ((pa & P_HAT) && (b16 & 3) == 2) drum(q_lvl >= 5 ? 3 : 2, 100);
        else if ((pa & P_HAT16) && (b16 & 1)) drum(2, 60);
    }
    if ((pa & P_CRASH) && r == 0 && q_lvl >= 4) drum(4, 128);

    /* the bass */
    if (bs_left && !--bs_left) bass_off();
    if (pa & P_BASS) {
        uint8_t root = ch_root[chord], kickless = !(pa & P_KICK) || !drums;
        if (kickless && q_lvl >= 3) {
            if (!(b16 & 7)) bass_on(root, 6);   /* half notes when there is no beat */
        } else if ((pa & P_ROLL) && (b16 & 3))
            bass_on((uint8_t)(root + ((b16 & 3) == 3 ? 12 : 0)), 1);
        else if ((b16 & 3) == 2)
            bass_on(root, q_lvl <= 2 ? 1 : 2);
    }

    /* the arpeggio (8ths on the chip levels, 16ths on the sample ones), and its echo */
    if (q_lvl >= 2 && (pa & P_ARP)) {
        uint8_t n = 0;
        if (q_lvl >= 4 || !(b16 & 1)) {
            n = (uint8_t)(chord_tone(arp_pat[b16]) + 24);
            if (bar & 1) n = (uint8_t)(n + (b16 >= 8 ? 12 : 0));
            arp_on(4, n);
        }
        if (q_lvl >= 5) {
            uint8_t e = eh_a[(s + 1) & 3];
            if (e) arp_on(7, e);
            eh_a[s & 3] = n;
        }
    }

    /* the lead */
    if (ld_left && !--ld_left) lead_off();
    if ((pa & P_LEAD) && mp) {
        uint8_t played = 0;
        while (*mp != 255 && *mp == r) {
            lead_on(mp[1]);
            ld_left = mp[2];
            played = mp[1];
            mp += 3;
        }
        if (q_lvl >= 5) {
            uint8_t e = eh_l[(s + 1) & 3];
            if (e) {
                if (q_lvl == 5) wave_on(8, S_LEAD_OFF, e, C_INTERP);
                else chip_on(8, C_SAW | C_ENV, mstep(e));
            }
            eh_l[s & 3] = played;
        }
    }

    /* the riser: noise climbing over the last two bars */
    if ((pa & P_RISER) && r == 96) {
        v_vol(15, 0, 0);
        VREG(15, 10) = 0;
        chip_on(15, C_NOISE, 0x8000);
    }
    if ((pa & P_RISER) && r >= 96) {
        uint16_t k = (uint16_t)(r - 96);
        v_step(15, 0x8000UL + (uint32_t)k * k * 480UL);
        v_vol(15, (uint16_t)(k * 3), (uint16_t)(k * 3));
    }
    if (r == 0) v_off(15);
}

/* ---------------------------------------------------------------- 50 Hz: drums, vibrato, pumping */
uint32_t mpos(void);
static void fx_tick(void)
{
    uint8_t l = q_lvl;
    if (dk) {                                   /* the chip levels' drums */
        dk--;
        if (l <= 1) {
            if (dk == 2) v_step(dk_v, mstep(52));
            else if (dk == 1) v_step(dk_v, mstep(40));
            else {
                uint8_t n = dk_v == 5 ? ld_note : bs_note;
                if (n) v_step(dk_v, mstep(n));
                else VREG(dk_v, 0) = C_PULSE;
            }
        } else {
            if (dk_kind == 0) v_step(2, mstep((int16_t)(33 + dk * 3)));
            if (l == 2) v_vol(2, (uint16_t)(dk * 0x14), (uint16_t)(dk * 0x14));
            if (!dk) VREG(2, 0) = (uint16_t)(VREG(2, 0) & ~C_GATE);
        }
    }
    if (ld_note) {
        ld_age++;
        if (l == 2 && ld_vol > 0x40) { ld_vol = (uint16_t)(ld_vol - 3); v_vol(5, ld_vol, (uint16_t)(ld_vol - 0x20)); }
        if (l >= 2 && ld_age > 10) {            /* vibrato after the attack */
            uint32_t st = mstep(ld_note);
            int8_t sv = SIN((uint8_t)(ld_age * 22));
            int32_t d = (int32_t)(st >> 12) * sv >> 5;
            if (l >= 6) lead_step((uint32_t)((int32_t)st + d));
            else v_step(5, (uint32_t)((int32_t)st + d));
        }
        if (l == 3) VREG(5, 5) = (uint16_t)(0x400 + ((SIN((uint8_t)(ld_age * 5)) + 128) << 2));
    }
    if (l == 2 && ar_vol > 0x10) { ar_vol = (uint16_t)(ar_vol - 6); v_vol(4, ar_vol, (uint16_t)(ar_vol + 0x10)); }
    if (l == 3) {                               /* the bass filter sweeps; the arpeggio's pulse width moves */
        IO(0x1004) = (uint16_t)(0x0900 + ((SIN((uint8_t)(ticks50 >> 1)) + 128) << 5));
        VREG(4, 5) = (uint16_t)(0x200 + ((SIN((uint8_t)(ticks50 * 3)) + 128) << 2));
    }
    if (l >= 6 && ph && (ph->parts & P_PAD)) {  /* the pad pumps with the kick, the filter breathes */
        uint32_t m = mpos();
        uint16_t bp = (uint16_t)(m & 1023);     /* the position in the beat */
        uint16_t k = (ph->parts & P_KICK) && !((ph->parts & P_HALF) && ((q_cur >> 4) & 7) < 4)
                         ? (uint16_t)(bp < 640 ? 64 + bp * 3 / 10 : 256) : 256;
        uint8_t n = pd_n(), i;
        uint16_t base = l == 7 ? 0x16 : 0x2A;
        uint16_t vv = (uint16_t)(base * k >> 8);
        for (i = 0; i < n; i++) v_vol((uint8_t)(16 + i), (i & 1) ? vv : (uint16_t)(vv * 3 >> 2), (i & 1) ? (uint16_t)(vv * 3 >> 2) : vv);
        IO(0x1004) = (uint16_t)(0x1400 + ((SIN((uint8_t)(m >> 9)) + 128) << 4));
    }
}

/* ---------------------------------------------------------------- the clock */
uint32_t mpos(void)                             /* the song position in 16ths x 256 */
{
    uint32_t t = scount() - q_t0;
    if (t > STEP_FR) t = STEP_FR;
    return ((uint32_t)q_cur << 8) + ((t * 373UL) >> 13);
}
void poll(void)
{
    uint32_t t = scount();
    while (q_on && (int32_t)(t - q_next) >= 0) {
        q_t0 = q_next;
        q_next += STEP_FR;
        if (q_step >= SONG_STEPS) {
            uint8_t v;
            q_on = 0; song_done = 1;
            for (v = 0; v < 32; v++) v_off(v);
            break;
        }
        seq_step(q_step++);
    }
    while ((int32_t)(t - e_next) >= 0) {
        e_next += 960;
        ticks50++;
#ifdef STOP_AT
        if (q_cur >= (uint16_t)(STOP_AT)) IO(2) = 0;    /* fsim: stop at this step */
#endif
        if (q_on) fx_tick();
    }
}
static void song_start(uint8_t phrase)
{
    uint16_t i;
    eng_ok = IO(0x1014) == 0xA032;
    for (i = 0; i < SAMPLES_LEN; i += 2)        /* the samples into SDRAM, where the engine reads */
        *(volatile uint16_t *)(SMP + i) = (uint16_t)(samples[i] | samples[i + 1] << 8);
    IO(0x1008) = 0;
    q_lvl = 0xFF; fade_k = 256; song_done = 0;
    q_step = (uint16_t)(phrase * 128);
    q_cur = q_step;
    q_t0 = scount();
    q_next = q_t0 + 480;
    e_next = q_next;
    q_on = 1;
}
