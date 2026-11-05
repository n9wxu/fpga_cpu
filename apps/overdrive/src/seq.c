/* Overdrive: the music. A rave track at 150 BPM in A minor, 16 phrases of 8 bars (12.8 s each), played by
 * a 16th-note sequencer on the audio engine. The voices:
 *   0 kick  1 clap/snare  2 closed hat  3 open hat  4 crash  5 bass (sample wave)  6 sub (triangle)
 *   7 acid (saw through the resonant filter)  8 the lead's echo  9 the piano arpeggio  10-14 a 5-saw lead
 *   15 the riser (noise)  16-23 rave stabs (4 notes x 2 saws)  24-31 the pad (4 notes x 2 saws)
 * Time is the engine's 48 kHz frame counter: a 16th at 150 BPM is 4800 frames exactly. */
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
#define STEP_FR 4800UL
#define N_PHRASE 16
#define SONG_STEPS (15 * 128 + 96)              /* the last phrase stops after six bars */

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
static void hit(uint8_t v, uint16_t off, uint16_t len, uint16_t step, uint16_t vl, uint16_t vr)
{
    VREG(v, 0) = 0;
    v_smp(v, off, len);
    v_step(v, step);
    v_vol(v, vl, vr);
    v_pos0(v);
    VREG(v, 0) = (uint16_t)(C_INTERP | C_GATE);
}
static void wave_on(uint8_t v, uint16_t off, int16_t n)
{
    VREG(v, 0) = (uint16_t)(C_LOOP | C_ENV | C_INTERP);
    v_smp(v, off, 256);
    v_step(v, mstep(n));
    v_pos0(v);
    VREG(v, 0) = (uint16_t)(C_LOOP | C_ENV | C_INTERP | C_GATE);
}
static void wave_off(uint8_t v) { VREG(v, 0) = (uint16_t)(C_LOOP | C_ENV | C_INTERP); }
static void chip_on(uint8_t v, uint16_t src, uint32_t step)
{
    VREG(v, 0) = src;
    v_step(v, step);
    VREG(v, 0) = (uint16_t)(src | C_GATE);
}

/* ---------------------------------------------------------------- the song */
#define P_KICK 0x0001
#define P_CLAP 0x0002
#define P_HAT 0x0004                            /* open hats on the off-beats */
#define P_HAT16 0x0008                          /* closed 16ths */
#define P_BASS 0x0010                           /* the galloping bass between the kicks */
#define P_ACID 0x0020
#define P_STAB 0x0040
#define P_LEAD 0x0080
#define P_ARP 0x0100
#define P_PAD 0x0200
#define P_RISER 0x0400                          /* noise rising over the last two bars */
#define P_FILL 0x0800                           /* a snare roll in the last bar */
#define P_CRASH 0x1000
#define P_BUILD 0x2000                          /* the snare roll speeding up over the phrase; the kick stops */
#define P_HALF 0x4000                           /* hats, clap and bass come in half way */
#define P_END 0x8000                            /* the last hit in bar 4, then silence */
#define BASE (P_KICK | P_CLAP | P_HAT | P_HAT16 | P_BASS | P_ACID | P_STAB)

typedef struct { uint8_t prog, mel; uint16_t parts; } phrase_t;
static const phrase_t song[N_PHRASE] = {
    {0, 0, P_KICK | P_ACID | P_HAT16 | P_HAT | P_CLAP | P_HALF | P_FILL | P_RISER},  /*  0 the intro */
    {0, 0, BASE | P_CRASH},                                         /*  1 the first drop */
    {0, 1, BASE | P_LEAD},
    {0, 2, BASE | P_LEAD | P_ARP},                                  /*  3 the tunnel */
    {0, 1, BASE | P_LEAD | P_ARP | P_FILL},
    {1, 3, P_PAD | P_ARP | P_LEAD},                                 /*  5 the breakdown */
    {2, 0, P_PAD | P_ACID | P_BUILD | P_RISER | P_KICK},            /*  6 the build */
    {0, 1, BASE | P_LEAD | P_ARP | P_CRASH},                        /*  7 the second drop: the dancer */
    {0, 2, BASE | P_LEAD | P_PAD},
    {0, 1, BASE | P_LEAD | P_ARP | P_FILL},
    {3, 0, P_KICK | P_HAT16 | P_HAT | P_ACID | P_STAB | P_CRASH},  /* 10 the acid break */
    {2, 0, P_PAD | P_ACID | P_BUILD | P_RISER | P_KICK},            /* 11 the second build */
    {0, 1, BASE | P_LEAD | P_ARP | P_PAD | P_CRASH},                /* 12 the last drop */
    {0, 2, BASE | P_LEAD | P_ARP | P_PAD},
    {0, 1, BASE | P_LEAD | P_ARP | P_PAD | P_FILL},
    {0, 2, BASE | P_LEAD | P_END | P_CRASH},                        /* 15 the end */
};

/* chords: Am F C G E Dm; progressions of four, two bars each */
static const uint8_t ch_root[6] = {45, 41, 48, 43, 40, 38};
static const uint8_t ch_min[6] = {1, 0, 0, 0, 0, 1};
static const uint8_t progs[4][4] = {{0, 1, 2, 3}, {1, 3, 0, 0}, {0, 3, 1, 4}, {5, 1, 4, 0}};

/* melodies: (step, note, length) over 8 bars, 255 ends */
static const uint8_t mel_a[] = {
    0, 76, 3, 3, 76, 3, 6, 81, 2, 8, 79, 3, 11, 76, 3, 14, 72, 2,
    16, 74, 3, 19, 76, 3, 22, 72, 2, 24, 69, 4, 28, 71, 2, 30, 72, 2,
    32, 77, 3, 35, 77, 3, 38, 81, 2, 40, 79, 3, 43, 77, 3, 46, 76, 2,
    48, 74, 3, 51, 76, 3, 54, 77, 2, 56, 76, 4, 60, 74, 2, 62, 72, 2,
    64, 79, 3, 67, 79, 3, 70, 84, 2, 72, 83, 3, 75, 79, 3, 78, 76, 2,
    80, 74, 3, 83, 76, 3, 86, 79, 2, 88, 76, 4, 92, 74, 2, 94, 72, 2,
    96, 74, 3, 99, 79, 3, 102, 83, 2, 104, 86, 4, 108, 83, 2, 110, 79, 2,
    112, 83, 6, 118, 81, 2, 120, 79, 4, 124, 83, 4, 255};
static const uint8_t mel_b[] = {
    0, 81, 6, 6, 84, 2, 8, 83, 6, 14, 79, 2, 16, 81, 8, 24, 76, 4, 28, 79, 4,
    32, 77, 6, 38, 81, 2, 40, 84, 6, 46, 81, 2, 48, 79, 8, 56, 77, 4, 60, 76, 4,
    64, 79, 6, 70, 84, 2, 72, 88, 6, 78, 86, 2, 80, 84, 8, 88, 79, 4, 92, 83, 4,
    96, 86, 6, 102, 83, 2, 104, 79, 6, 110, 83, 2, 112, 86, 4, 116, 88, 4, 120, 91, 8, 255};
static const uint8_t mel_bd[] = {
    0, 77, 6, 6, 81, 2, 8, 84, 8, 16, 81, 6, 22, 79, 2, 24, 77, 8,
    32, 79, 6, 38, 83, 2, 40, 86, 8, 48, 83, 6, 54, 86, 2, 56, 91, 8,
    64, 88, 12, 76, 84, 4, 80, 81, 16, 96, 84, 8, 104, 88, 8, 112, 93, 16, 255};
static const uint8_t *const mels[4] = {0, mel_a, mel_b, mel_bd};
static const uint8_t arp_pat[16] = {0, 1, 2, 3, 1, 2, 3, 0, 2, 3, 1, 2, 3, 2, 1, 0};
/* the acid line: semitones over the root an octave up; 0x80 accent, 0x40 slide */
static const uint8_t acid_pat[16] = {0x80, 0, 12, 0, 0x83, 0, 15, 0x40, 0, 0x8C, 0, 10, 0, 0x87, 12, 3};
#define STAB_MASK 0x1249                        /* steps 0, 3, 6, 9, 12 of each bar */

/* ---------------------------------------------------------------- state */
uint16_t q_cur;                                 /* the step playing (16ths since the start) */
static uint16_t q_step;
static uint32_t q_next, q_t0;
static uint8_t q_on;
uint8_t song_done;
static uint32_t e_next;
uint32_t ticks50;
static const phrase_t *ph;
static const uint8_t *mp;
static uint8_t chord;
static uint8_t ld_note, ld_age, ld_left, bs_left, ac_note;
static uint16_t ac_env;                         /* the acid's filter envelope */
static uint8_t eh_l[4];                         /* the lead's notes 3 steps ago (the echo) */
static uint8_t roll32;                          /* a second snare due half way through this step */
static uint8_t roll_v;
uint16_t kick_step = 0xFFFF, clap_step = 0xFFFF, stab_step = 0xFFFF, crash_step = 0xFFFF;

static const int8_t ld_det[5] = {0, -4, 4, -9, 9};
static void lead_step(uint32_t st)
{
    uint8_t i;
    for (i = 0; i < 5; i++)
        v_step((uint8_t)(10 + i), (uint32_t)((int32_t)st + (int32_t)(st >> 9) * ld_det[i]));
}
static uint8_t chord_tone(uint8_t i)
{
    static const uint8_t iv[2][4] = {{0, 4, 7, 12}, {0, 3, 7, 12}};
    return (uint8_t)(ch_root[chord] + iv[ch_min[chord]][i]);
}

static void voices_init(void)
{
    uint8_t v;
    for (v = 0; v < 32; v++) { v_off(v); v_vol(v, 0, 0); VREG(v, 5) = 0x800; v_adsr(v, 0xFFFF, 0, 0xFFFF, 0xFFFF); }
    IO(0x1000) = 0x0900;
    v_adsr(5, 0xFFFF, 260, 0x5000, 600); v_vol(5, 0x90, 0x90);
    v_adsr(6, 0xFFFF, 200, 0x8000, 900); v_vol(6, 0x70, 0x70);
    v_adsr(7, 0xFFFF, 280, 0x7000, 1200); v_vol(7, 0x68, 0x58);
    v_adsr(8, 0x3000, 60, 0x9000, 300); v_vol(8, 0x0C, 0x20);
    v_adsr(9, 0xFFFF, 500, 0x0000, 500); v_vol(9, 0x40, 0x50);
    for (v = 10; v < 15; v++) { v_adsr(v, 0x3000, 40, 0xC000, 350); v_vol(v, (uint16_t)(0x1C + (v & 1) * 8), (uint16_t)(0x24 - (v & 1) * 8)); }
    for (v = 16; v < 24; v++) { v_adsr(v, 0xFFFF, 420, 0x0000, 420); v_vol(v, (v & 1) ? 0x1C : 0x28, (v & 1) ? 0x28 : 0x1C); }
    for (v = 24; v < 32; v++) v_adsr(v, 0x0280, 0, 0xFFFF, 260);
    IO(0x1002) = 1; IO(0x1006) = 0x1400; IO(0x1004) = 0x0800;
}

static void lead_on(uint8_t n)
{
    uint8_t i;
    ld_note = n; ld_age = 0;
    for (i = 0; i < 5; i++) chip_on((uint8_t)(10 + i), C_SAW | C_ENV, mstep(n));
    lead_step(mstep((int16_t)(n - 2)));         /* it starts low and swoops up (fx_tick) */
}
static void lead_off(void)
{
    uint8_t i;
    for (i = 0; i < 5; i++) VREG(10 + i, 0) = C_SAW | C_ENV;
    ld_note = 0;
}
static void stab(uint8_t vel)
{
    uint8_t i;
    stab_step = q_cur;
    for (i = 0; i < 8; i++) {
        uint8_t v = (uint8_t)(16 + i);
        uint32_t st = mstep((int16_t)(chord_tone((uint8_t)(i >> 1)) + 24));
        uint16_t g = (uint16_t)(((i & 1) ? 0x1C : 0x28) * vel >> 7);
        v_vol(v, g, (uint16_t)(((i & 1) ? 0x28 : 0x1C) * vel >> 7));
        VREG(v, 0) = C_SAW | C_ENV;
        v_step(v, (uint32_t)((int32_t)st + (int32_t)(st >> 9) * ((i & 1) ? 3 : -3)));
        VREG(v, 0) = C_SAW | C_ENV | C_GATE;
    }
}
static void pad_set(uint8_t on)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        uint8_t v = (uint8_t)(24 + i);
        uint32_t st = mstep((int16_t)(chord_tone((uint8_t)(i >> 1)) + 12));
        v_step(v, (uint32_t)((int32_t)st + (int32_t)(st >> 10) * ((i & 1) ? 2 : -2)));
        if (on == 1) VREG(v, 0) = C_SAW | C_ENV | C_GATE;
        else if (on == 2) VREG(v, 0) = C_SAW | C_ENV;
    }
}

static void drum(uint8_t kind, uint8_t vel)     /* 0 kick, 1 clap, 2 closed hat, 3 open hat, 4 crash, 5 snare */
{
    uint16_t g = (uint16_t)(vel << 1);
    switch (kind) {
    case 0: kick_step = q_cur; hit(0, S_KICK_OFF, S_KICK_LEN, S_KICK_STEP, (uint16_t)(0xE0 * g >> 8), (uint16_t)(0xE0 * g >> 8)); break;
    case 1: clap_step = q_cur; hit(1, S_CLAP_OFF, S_CLAP_LEN, S_CLAP_STEP, (uint16_t)(0x90 * g >> 8), (uint16_t)(0x80 * g >> 8)); break;
    case 2: hit(2, S_CHAT_OFF, S_CHAT_LEN, S_CHAT_STEP, (uint16_t)(0x30 * g >> 8), (uint16_t)(0x48 * g >> 8)); break;
    case 3: hit(3, S_OHAT_OFF, S_OHAT_LEN, S_OHAT_STEP, (uint16_t)(0x50 * g >> 8), (uint16_t)(0x38 * g >> 8)); break;
    case 4: crash_step = q_cur; hit(4, S_CRASH_OFF, S_CRASH_LEN, S_CRASH_STEP, (uint16_t)(0x60 * g >> 8), (uint16_t)(0x60 * g >> 8)); break;
    default: clap_step = q_cur; hit(1, S_SNARE_OFF, S_SNARE_LEN, S_SNARE_STEP, (uint16_t)(0x98 * g >> 8), (uint16_t)(0x98 * g >> 8)); break;
    }
}

/* ---------------------------------------------------------------- one 16th */
static void seq_step(uint16_t s)
{
    uint8_t p = (uint8_t)(s >> 7), r = (uint8_t)(s & 127), b16 = (uint8_t)(s & 15), bar = (uint8_t)(r >> 4);
    uint16_t pa;
    uint8_t late;
    q_cur = s;
    ph = &song[p];
    pa = ph->parts;
    if ((pa & P_END) && r >= 64) {              /* the last hit, then silence */
        if (r == 64) {
            uint8_t v;
            for (v = 2; v < 32; v++) if (v != 4 && (v < 16 || v > 23)) v_off(v);
            chord = 0;
            drum(0, 128); drum(4, 128); stab(128);
            for (v = 16; v < 24; v++) v_adsr(v, 0xFFFF, 40, 0x0000, 60);
        }
        return;
    }
    if (r == 0) {
        mp = mels[ph->mel];
        ld_left = 0;
        if (ld_note) lead_off();
    }
    {
        uint8_t c = progs[ph->prog][r >> 5];
        if (c != chord || r == 0) {
            chord = c;
            if (pa & P_PAD) pad_set(r == 0 ? 1 : 0);
            else if (r == 0) pad_set(2);
        }
    }
    late = !(pa & P_HALF) || bar >= 4;

    /* the drums */
    if (pa & P_BUILD) {                         /* quarters, 8ths, 16ths, 32nds; louder and louder */
        uint8_t hitme = bar < 3 ? !(b16 & 3) : (bar < 5 ? !(b16 & 1) : 1);
        roll_v = (uint8_t)(40 + r * 2 / 3);
        if (hitme) drum(5, roll_v);
        roll32 = bar == 7;
        if ((pa & P_KICK) && bar < 6 && !(b16 & 3)) drum(0, 128);
        if (bar >= 6 && !(b16 & 1)) drum(2, (uint8_t)(40 + b16 * 4));
    } else if ((pa & P_FILL) && bar == 7) {
        if (b16 < 8 ? !(b16 & 1) : 1) drum(5, (uint8_t)(60 + b16 * 4));
        if ((pa & P_KICK) && !(b16 & 3)) drum(0, 128);
        if (b16 >= 12) roll32 = 1, roll_v = (uint8_t)(100 + b16);
    } else {
        if ((pa & P_KICK) && !(b16 & 3)) drum(0, 128);
        if (late && (pa & P_CLAP) && (b16 == 4 || b16 == 12)) drum(1, 120);
        if (late && (pa & P_HAT) && (b16 & 3) == 2) drum(3, 110);
        else if ((pa & P_HAT16) && (b16 & 1)) drum(2, (uint8_t)((b16 & 2) ? 70 : 45));
    }
    if ((pa & P_CRASH) && r == 0) drum(4, 128);

    /* the bass: three 16ths between the kicks, the middle one an octave up */
    if (bs_left && !--bs_left) { wave_off(5); VREG(6, 0) = C_TRI | C_ENV; }
    if ((pa & P_BASS) && late && (b16 & 3)) {
        uint8_t root = ch_root[chord];
        wave_on(5, S_BASS_OFF, (int16_t)(root + ((b16 & 3) == 2 ? 12 : 0)));
        if ((b16 & 3) == 2) chip_on(6, C_TRI | C_ENV, mstep((int16_t)(root - 12)));
        bs_left = 1;
    }

    /* the acid line */
    if (pa & P_ACID) {
        uint8_t a = acid_pat[b16], n = (uint8_t)(ch_root[chord] + 12 + (a & 0x3F));
        if (a & 0x40) v_step(7, mstep(n));      /* a slide: no new attack */
        else chip_on(7, C_SAW | C_ENV | C_FILT, mstep(n));
        ac_note = n;
        ac_env = (uint16_t)(ac_env + ((a & 0x80) ? 0x1400 : 0x0600));
    } else if (ac_note) { VREG(7, 0) = C_SAW | C_ENV | C_FILT; ac_note = 0; }

    /* the stabs */
    if ((pa & P_STAB) && (STAB_MASK >> b16 & 1)) stab((uint8_t)(b16 ? 96 : 128));

    /* the piano arpeggio */
    if (pa & P_ARP) wave_on(9, S_PLUCK_OFF, (int16_t)(chord_tone(arp_pat[b16]) + 24 + ((bar & 1) && b16 >= 8 ? 12 : 0)));

    /* the lead and its echo */
    if (ld_left && !--ld_left) lead_off();
    if ((pa & P_LEAD) && mp) {
        uint8_t played = 0;
        while (*mp != 255 && *mp == r) {
            lead_on(mp[1]);
            ld_left = mp[2];
            played = mp[1];
            mp += 3;
        }
        {
            uint8_t e = eh_l[(s + 1) & 3];
            if (e) chip_on(8, C_SAW | C_ENV, mstep(e));
            eh_l[s & 3] = played;
        }
    }

    /* the riser */
    if ((pa & P_RISER) && r == ((pa & P_BUILD) ? 0 : 96)) {
        v_vol(15, 0, 0);
        VREG(15, 10) = 0;
        chip_on(15, C_NOISE, 0x6000);
    }
    if ((pa & P_RISER) && r >= ((pa & P_BUILD) ? 0 : 96)) {
        uint16_t k = (pa & P_BUILD) ? (uint16_t)(r >> 2) : (uint16_t)(r - 96);
        v_step(15, 0x6000UL + (uint32_t)k * k * 600UL);
        v_vol(15, (uint16_t)(k * 3), (uint16_t)(k * 3));
    }
    if (r == 0 && !(pa & P_BUILD)) v_off(15);
}

/* ---------------------------------------------------------------- 50 Hz: the lead's swoop and vibrato, the filter, pumping */
uint32_t mpos(void);
static void fx_tick(void)
{
    if (ld_note) {
        uint32_t st = mstep(ld_note);
        ld_age++;
        if (ld_age < 4) lead_step(mstep((int16_t)(ld_note - (4 - ld_age) / 2)));
        else if (ld_age == 4) lead_step(st);
        else if (ld_age > 12) {
            int8_t sv = SIN((uint8_t)(ld_age * 24));
            lead_step((uint32_t)((int32_t)st + ((int32_t)(st >> 12) * sv >> 5)));
        }
    }
    if (ph) {
        uint32_t m = mpos();
        uint16_t pa = ph->parts, base;
        uint8_t r = (uint8_t)(q_cur & 127);
        /* the acid's cutoff: rising through the intro and builds, a slow sweep in the drops */
        if (pa & (P_BUILD | P_RISER) && (q_cur >> 7) != 10) base = (uint16_t)(0x0500 + (uint16_t)r * 0x22);
        else base = (uint16_t)(0x0A00 + ((SIN((uint8_t)(m >> 11)) + 128) << 4));
        ac_env = (uint16_t)(ac_env - (ac_env >> 2));
        if (ac_env > 0x3000) ac_env = 0x3000;
        IO(0x1004) = (uint16_t)(base + ac_env);
        if (pa & P_PAD) {                       /* the pad ducks under the kick */
            uint16_t bp = (uint16_t)(m & 1023), k = 256, v;
            uint8_t i;
            if ((pa & P_KICK) && !((pa & P_BUILD) && r >= 96)) k = bp < 700 ? (uint16_t)(60 + bp * 7 / 25) : 256;
            v = (uint16_t)(0x1C * k >> 8);
            for (i = 0; i < 8; i++) v_vol((uint8_t)(24 + i), (i & 1) ? v : (uint16_t)(v * 3 >> 2), (i & 1) ? (uint16_t)(v * 3 >> 2) : v);
        }
    }
}

/* ---------------------------------------------------------------- the clock */
uint32_t mpos(void)                             /* the song position in 16ths x 256 */
{
    uint32_t t = scount() - q_t0;
    if (t > STEP_FR) t = STEP_FR;
    return ((uint32_t)q_cur << 8) + ((t * 437UL) >> 13);
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
        roll32 = 0;
        seq_step(q_step++);
    }
    if (roll32 && (int32_t)(t - q_t0) >= (int32_t)(STEP_FR / 2)) { roll32 = 0; drum(5, roll_v); }
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
    for (i = 0; i < SAMPLES_LEN; i += 2)
        *(volatile uint16_t *)(SMP + i) = (uint16_t)(samples[i] | samples[i + 1] << 8);
    IO(0x1008) = 0;
    voices_init();
    song_done = 0; ph = 0; chord = 0;
    q_step = (uint16_t)(phrase * 128);
    q_cur = q_step;
    q_t0 = scount();
    q_next = q_t0 + 480;
    e_next = q_next;
    q_on = 1;
}
