/* modplay: plays Amiga ProTracker MODs (4 channels) on the audio engine and shows them as a tracker.
 * MODs: in SDRAM from 0x040000, one after another, each at a 256-byte boundary after the last (tools/pack.py
 * gives tang/tools/run.py the addresses); the first address without a MOD signature ends the list.
 * Ticks are timed by the engine's 48 kHz frame counter (SCOUNT): bpm x 2 / 5 a second, speed ticks a row. */
#include <string.h>
#include "mod.h"
#include "ui.h"

#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define MOD_BASE 0x040000UL
#define MOD_LIMIT 0x0C3C00UL            /* the bitmap starts here */
#define MAX_MODS 8
#define T1_HZ 16875000UL                /* CYCLE counts T1 periods */

static uint32_t slots[MAX_MODS];
static uint8_t n_mods, cur;
static ui_stat_t st;

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

static uint32_t cyc_now(void)
{
    uint16_t h, l, h2;
    do {
        h = IO(0x6);
        l = IO(0x4);
        h2 = IO(0x6);
    } while (h != h2);
    return (uint32_t)h << 16 | l;
}

static int16_t poll_key(void)
{
    uint16_t k = IO(0x100);             /* keyboard: 0xFFFF none */
    if (k != 0xFFFF && k != 0)
        return (int16_t)k;
    k = IO(0x8);                        /* the serial console */
    if (k < 0xFFFE)
        return (int16_t)k;
    return -1;
}

static void scan(void)
{
    uint32_t a = MOD_BASE;
    n_mods = 0;
    while (n_mods < MAX_MODS && a + 1084 < MOD_LIMIT) {
        uint32_t sz = mod_size(a);
        if (!sz || a + sz > MOD_LIMIT)
            break;
        slots[n_mods++] = a;
        a = (a + sz + 255) & ~255UL;
    }
}

static uint32_t next_t, acc, play48;

static void start(uint8_t i)
{
    int e;
    cur = i;
    mod_silence();
    e = mod_parse(slots[i], &mod);
    ui_song((uint8_t)(i + 1), n_mods);
    if (e) {
        ui_message(mod_err(e), 4);
        return;
    }
    ui_keys();
    mod_start();
    play48 = 0;
    acc = 0;
    next_t = scount() + 480;            /* 10 ms from now */
}

int main(void)
{
    uint8_t btn_prev = 0;
    uint32_t period_cyc = 1;
#ifdef MAXTICKS
    uint32_t nticks = 0;
#endif
    ui_init();
    IO(0x1000) = 0x0800;                /* MASTER 0.5: four voices at full scale stay in range */
    pl.sep = 64;
    pl.interp = 0;
    scan();
    if (!n_mods) {
        ui_message(" NO MOD IN SDRAM: LOAD ONE WITH  make -C apps/modplay run MODS=\"a.mod b.mod\"", 4);
        for (;;)
            ;
    }
    if (IO(0x1014) != 0xA032)
        ui_message(" NO AUDIO ENGINE (ID F01014)", 4);
    start(0);
    for (;;) {
        int16_t key;
        uint8_t btn, want = 0;
        uint32_t t0;
#ifndef NOWAIT
        while ((int32_t)(scount() - next_t) < 0)
            ;
#endif
        t0 = cyc_now();
        if (!st.paused) {
            uint32_t f;
            mod_tick();
            acc += mod_tick_frames_x100();
            f = acc / 100;
            acc -= f * 100;
            next_t += f;
            play48 += f;
            st.play_s = play48 / 48000UL;
            period_cyc = T1_HZ / 100 * f / 480;  /* the tick in T1 cycles (f frames of 1/48000 s) */
        } else
            next_t += 960;
        ui_frame(&st);
        /* input */
        key = poll_key();
        btn = (uint8_t)(IO(0x604) & 3);
        if ((btn & 2) && !(btn_prev & 2))
            want = 1;
        if ((btn & 1) && !(btn_prev & 1))
            mod_goto((uint8_t)(pl.pos + 1 < mod.songlen ? pl.pos + 1 : 0));
        btn_prev = btn;
        switch (key) {
        case 't': case 'T': want = 1; break;
        case 'n': case '+': case 0x83: mod_goto((uint8_t)(pl.pos + 1 < mod.songlen ? pl.pos + 1 : 0)); break;
        case 'p': case '-': case 0x82: mod_goto((uint8_t)(pl.pos ? pl.pos - 1 : 0)); break;
        case '1': case '2': case '3': case '4': pl.mute ^= (uint8_t)(1 << (key - '1')); break;
        case 'i': case 'I': pl.interp ^= 1; break;
        case 's': case 'S': {
            uint8_t c;
            pl.sep = pl.sep == 64 ? 128 : pl.sep == 128 ? 0 : 64;
            for (c = 0; c < mod.nch; c++)
                ch[c].pan = (uint8_t)((c == 0 || c == 3) ? 128 - pl.sep : (pl.sep >= 128 ? 255 : 128 + pl.sep));
            break;
        }
        case ' ':
            st.paused ^= 1;
            if (st.paused)
                mod_silence();
            else
                next_t = scount() + 480;
            break;
        case 'q': case 'Q': mod_silence(); IO(0x2) = 0; break;
        }
        if (want)
            start((uint8_t)((cur + 1) % n_mods));
        {
            uint32_t busy = cyc_now() - t0, pct = busy * 100 / period_cyc;
            st.load = (uint8_t)(pct > 255 ? 255 : (st.load * 3 + pct) / 4);
        }
#ifdef MAXTICKS
        if (++nticks >= MAXTICKS) {
            mod_silence();
            IO(0x2) = 0;
            for (;;)
                ;
        }
#endif
    }
}
