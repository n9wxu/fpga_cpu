/* sidplay: plays a PSID tune with the 6502 recompiler on the audio engine and shows it as a tracker.
 * Tunes: SDRAM slots at 0x080000, 0x090000, 0x0A0000, 0x0B0000 (a PSID file; its length in the last 4
 * bytes of the 64 KB slot, written by tang/tools/run.py --tune); the built-in tune if there is none. */
#include <string.h>
#include "sid6502.h"
#include "sidplay.h"
#include "model.h"
#include "ui.h"
#include "tune0.h"
#include "layout.h"

#ifndef T1_KHZ
#define T1_KHZ 16875                    /* CYCLE counts T1 periods of 4 x 67.5 MHz */
#endif
#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define SLOT(i) (SDR_SLOT0 + (uint32_t)(i) * 0x10000UL)
#define N_SLOTS 4
#define FRING 128                       /* frame ring */
#define LOOKAHEAD 96                    /* play calls generated ahead of the one being heard */
#define FRAMES ((frame_t *)SDR_FRAMES)

#define tune_tab ((psid_t *)SDR_TUNES)             /* the parsed headers live in SDRAM (they are only read) */
static uint8_t n_tunes;
static uint8_t cur_tune, cur_song;
static ui_stat_t st;

#ifdef AUDLOG
extern volatile uint32_t aud_tick;
#endif

static uint32_t scount(void)
{
    uint16_t h, l, h2;
    do {
        h = IO(0x1012);
        l = IO(0x1010);
        h2 = IO(0x1012);
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}

static uint16_t sr3(void)
{
    uint16_t v;
    __asm__ volatile("MFSR %0, sr3" : "=r"(v));
    return v;
}

static void scan_tunes(void)
{
    uint8_t i;
    n_tunes = 0;
    for (i = 0; i < N_SLOTS; i++) {
        const uint8_t *b = (const uint8_t *)SLOT(i);
        uint32_t len = (uint32_t)b[0xFFFC] | (uint32_t)b[0xFFFD] << 8 | (uint32_t)b[0xFFFE] << 16 | (uint32_t)b[0xFFFF] << 24;
        if ((!memcmp(b, "PSID", 4) || !memcmp(b, "RSID", 4)) && len > 0x76 && len <= 0xFFFC) {
            int e = psid_parse(b, len, &tune_tab[n_tunes]);
            if (e == 0 || e == 4 || e == 5)
                n_tunes++;                      /* unsupported ones are listed too; start_tune reports why */
        }
    }
    if (!n_tunes) {
        psid_parse(tune0, sizeof tune0, &tune_tab[0]);
        n_tunes = 1;
    }
}

/* ---------------------------------------------------------------- the song */
static uint32_t gen_f, play_f;          /* next frame to generate / to hear */
static uint8_t gen_err;
static uint32_t next_t, period_q, period_r, period_acc, clk_for_acc;
static uint32_t period_cyc, next_c;
static uint8_t eng;
static uint32_t play48;
static uint8_t paused;
static uint32_t acc_busy;

static void gen_one(uint8_t feed)
{
    frame_t *f = &FRAMES[gen_f % FRING];
    if (gen_err) {
        *f = FRAMES[(gen_f - 1) % FRING];
        f->rise = f->fall = 0;
        f->r[24] = 0;                           /* silence */
    } else {
        int s = pl_frame(f);
        if (s == RUN_ILLEGAL) {
            char m[40] = "ILLEGAL 6502 OPCODE AT ....: STOPPED";
            uint8_t k;
            gen_err = 1;
            for (k = 0; k < 4; k++)
                m[24 + k] = "0123456789ABCDEF"[(cpu.pc >> (12 - 4 * k)) & 15];
            ui_message(m, 4);
        } else if (s == RUN_LIMIT) {
            ui_message("PLAY ROUTINE RAN TOO LONG (CYCLE LIMIT)", 6);
        }
    }
    if (feed)
        model_feed(f, gen_f);
    gen_f++;
}

/* pacing: engine frames per play call, with the remainder carried */
static void set_pacing(void)
{
    clk_for_acc = pl_clk_hz;
    period_q = 48000UL * pl_cyc_per_call / pl_clk_hz;
    period_r = 48000UL * pl_cyc_per_call % pl_clk_hz;
    period_acc = 0;
    period_cyc = (uint32_t)T1_KHZ * pl_cyc_per_call / (pl_clk_hz / 1000);
    next_t = (eng ? scount() : 0) + 2000;
    next_c = cyc_now() + period_cyc;
}

static int start_song(uint8_t tune, uint8_t song)
{
    const psid_t *p = &tune_tab[tune];
    int s;
    uint32_t k;
    cur_tune = tune;
    cur_song = song;
    sm_silence();
    gen_err = 0;
    st.tune = tune + 1;
    st.tunes = n_tunes;
    st.song = song;
    st.songs = (uint8_t)p->songs;
    st.play_ms = 0;
    play48 = 0;
    memset(FRAMES, 0, FRING * sizeof(frame_t));
    model_init();
    if (p->rsid || p->play == 0) {
        ui_tune(p, &st);
        ui_message(p->rsid ? "RSID TUNE (NEEDS AN IRQ MODEL): NOT SUPPORTED" : "NO PLAY ADDRESS (IRQ TUNE): NOT SUPPORTED", 4);
        gen_err = 1;
        set_pacing();
        gen_f = play_f = 0;
        return -1;
    }
    s = pl_start(p, song);
    ui_tune(p, &st);                    /* after pl_start: it shows the clock and rate */
    sm_set_clock(pl_clk_hz);
    model_init();
    if (s != RUN_STOP) {
        ui_message("INIT DID NOT RETURN", 4);
        gen_err = 1;
    }
    gen_f = play_f = 0;
    for (k = 0; k < LOOKAHEAD; k++)
        gen_one(0);
    model_detect(FRAMES, LOOKAHEAD);
    for (k = 0; k < LOOKAHEAD; k++)
        model_feed(&FRAMES[k], k);
    set_pacing();
    return 0;
}

static void next_frame_time(void)
{
    period_acc += period_r;
    next_t += period_q;
    if (period_acc >= clk_for_acc) {
        period_acc -= clk_for_acc;
        next_t++;
    }
    next_c += period_cyc;
}

static int16_t poll_key(void)
{
    uint16_t k = IO(0x100);                 /* no key: 0xFFFF (docs/io.md; also 0 is ignored) */
    if (k != 0xFFFF && k != 0)
        return (int16_t)k;
    k = IO(0x8);
    if (k < 0xFFFE)
        return (int16_t)k;
    return -1;
}

int main(void)
{
    uint8_t btn_prev = 0, want = 0;       /* want: 1 next song, 2 prev song, 3 next tune */
    uint32_t nticks = 0;
#ifdef STATS
    uint32_t busy_max = 0, busy_sum = 0, ins_max = 0, ins_sum = 0, ins_n = 0;
    uint32_t ui_ins_max = 0, gen_ins_max = 0, apply_ins_max = 0, ui_ins_sum = 0, gen_ins_sum = 0;
    uint16_t m0 = 0, m1 = 0, m2 = 0, m3 = 0;
#endif
#ifdef STATS
    {   /* stack high-water mark: paint the free block RAM, look for the paint at the end */
        extern char _end[];
        volatile uint8_t *p, marker;
        for (p = (volatile uint8_t *)(SDR_STACK_TOP - 8192); p < &marker - 48; p++)
            *p = 0xA5;
    }
#endif
    ui_init();
    eng = (IO(0x1014) == 0xA032);
    sm_init();
    st.has_engine = eng;
    st.spread = 128;
    scan_tunes();
    sm_spread = st.spread;
    st.mute = 0;
    start_song(0, tune_tab[0].start);
    for (;;) {
        uint32_t t0, busy;
        uint16_t i0;
        int16_t key;
        uint8_t btn;
        /* wait for the time of this frame */
        if (eng) {
            while ((int32_t)(scount() - next_t) < 0)
                ;
            if ((int32_t)(scount() - next_t) > (int32_t)(period_q * 2))
                st.late++;
        } else {
#ifndef NOWAIT
            while ((int32_t)(cyc_now() - next_c) < 0)
                ;
#endif
        }
        next_frame_time();
        t0 = cyc_now();
        i0 = sr3();
#ifdef AUDLOG
        aud_tick = play_f;
#endif
        if (!paused) {
#ifdef STATS
            m0 = sr3();
#endif
            gen_one(1);
#ifdef STATS
            m1 = sr3();
#endif
            sm_apply(&FRAMES[play_f % FRING]);
#ifdef STATS
            m2 = sr3();
#endif
            play48 += period_q;
            st.play_ms = play48 / 48;
            play_f++;
        }
        ui_frame(&FRAMES[(play_f ? play_f - 1 : 0) % FRING], play_f ? play_f - 1 : 0, &st);
#ifdef STATS
        m3 = sr3();
#endif
        /* input: console, keyboard, buttons */
        key = poll_key();
        btn = (uint8_t)(IO(0x604) & 3);
        if ((btn & 1) && !(btn_prev & 1)) want = 1;
        if ((btn & 2) && !(btn_prev & 2)) want = 3;
        btn_prev = btn;
        if (key >= 0) {
            switch (key) {
            case '+': case '=': case 'n': case 0x83: want = 1; break;
            case '-': case 'p': case 0x82: want = 2; break;
            case 't': case 'T': want = 3; break;
            case '1': case '2': case '3': st.mute ^= 1 << (key - '1'); sm_mute = st.mute; break;
            case 's': st.spread = st.spread ? 0 : 128; sm_spread = st.spread; break;
            case ' ': paused ^= 1; st.paused = paused; if (paused) sm_silence(); break;
            case 'q': IO(0x2) = 0; break;
            }
        }
        if (want) {
            uint8_t t = cur_tune, s = cur_song;
            if (want == 1) s = (uint8_t)(s >= tune_tab[t].songs ? 1 : s + 1);
            else if (want == 2) s = (uint8_t)(s <= 1 ? tune_tab[t].songs : s - 1);
            else { t = (uint8_t)((t + 1) % n_tunes); s = tune_tab[t].start; }
            want = 0;
            paused = st.paused = 0;
            start_song(t, s);
            continue;
        }
        busy = cyc_now() - t0;
        {
            uint32_t pct = busy * 100 / (period_cyc ? period_cyc : 1);
            if (pct > 255) pct = 255;
            st.load = (uint8_t)((st.load * 3 + pct) / 4);
            if (pct > st.load_max) st.load_max = (uint8_t)pct;
        }
#ifdef STATS
        {
            uint32_t ins = (uint32_t)(uint16_t)(m1 - m0) + (uint16_t)(m2 - m1) + (uint16_t)(m3 - m2);
            (void)i0;
            if (nticks >= LOOKAHEAD) {
                if (busy > busy_max) busy_max = busy;
                busy_sum += busy;
                if (ins > ins_max) ins_max = ins;
                ins_sum += ins;
                ins_n++;
                gen_ins_sum += (uint16_t)(m1 - m0);
                if ((uint16_t)(m1 - m0) > gen_ins_max) gen_ins_max = (uint16_t)(m1 - m0);
                if ((uint16_t)(m2 - m1) > apply_ins_max) apply_ins_max = (uint16_t)(m2 - m1);
                ui_ins_sum += (uint16_t)(m3 - m2);
                if ((uint16_t)(m3 - m2) > ui_ins_max) ui_ins_max = (uint16_t)(m3 - m2);
            }
        }
#endif
        nticks++;
#ifdef MAXTICKS
        if (nticks >= MAXTICKS) {
#ifdef STATS
            {   /* results for the host: fsim --dump 0x048000:64:out/stats.bin (tools/stats.py) */
                volatile uint32_t *s = (volatile uint32_t *)SDR_STATS;
                s[0] = ins_n; s[1] = busy_sum / (ins_n ? ins_n : 1); s[2] = busy_max;
                s[3] = ins_sum / (ins_n ? ins_n : 1); s[4] = ins_max; s[5] = pl_cyc_per_call;
                s[6] = jit_stats.traces; s[7] = jit_stats.instrs; s[8] = jit_stats.events; s[9] = jit_stats.assists; s[10] = jit_stats.flushes;
                {
                    extern char _end[];
                    volatile uint8_t *p = (volatile uint8_t *)(SDR_STACK_TOP - 8192);
                    while (p < (volatile uint8_t *)SDR_STACK_TOP && *p == 0xA5)
                        p++;
                    s[16] = (uint32_t)(SDR_STACK_TOP - (uint32_t)p);   /* stack bytes used (the 8 KB below the top are painted) */
                    s[17] = (uint32_t)(SDR_STACK_TOP - (uint32_t)_end);
                }
                s[11] = (uint32_t)ui_ins_max; s[12] = gen_ins_max; s[13] = apply_ins_max; s[14] = ui_ins_sum; s[15] = gen_ins_sum;
            }
#endif
            IO(0x2) = 0;
            for (;;)
                ;
        }
#endif
        (void)i0;
    }
}
