/* Overdrive: a high-energy demo for the Tang Nano 20K, after the rave demos of the early 90s (cuts on the
 * beat, flashes, shakes, a dancer, vector balls, a tunnel, a plasma, raster bars and a sine scroller),
 * all original. 150 BPM, 15 phrases of 8 bars and 6 more, about 3 minutes 20 (seq.c). -DSTART=p starts at phrase p;
 * -DSTOP_AT=s stops fsim at 16th s. */
#include "hw.h"
#include "assets.h"
#include "seq.c"
#include "cv.h"
#include "fx.c"
#include "dancer.c"
#ifndef START
#define START 0
#endif

#define STEP16 ((uint8_t)((T >> 8) & 127))     /* the 16th in the phrase */

static void show_field(uint32_t buf) { set_pfa(buf); vid(VC_BMP | VC_HI); set_lscr(LSCR); }
#ifdef FPS
static void fps_tick(void)                      /* -DFPS: "pNN ff" (phrase, frames a second) every 2 s */
{
    static uint16_t n, f0;
    uint16_t f = FRAME;
    n++;
    if ((uint16_t)(f - f0) >= 120) {
        uint16_t r = (uint16_t)(n / 2), ph = (uint16_t)(q_cur >> 7);
        IO(0) = 'p'; IO(0) = (uint8_t)('0' + ph / 10); IO(0) = (uint8_t)('0' + ph % 10); IO(0) = ' ';
        IO(0) = (uint8_t)('0' + r / 10); IO(0) = (uint8_t)('0' + r % 10); IO(0) = '\n';
        n = 0; f0 = f;
    }
}
#else
#define fps_tick()
#endif
static void frame_end(void) { vsync(); pal_flush(); fps_tick(); }

/* ---------------------------------------------------------------- 0: the intro: a word a beat */
static const char *const intro_words[32] = {
    "T1", "", "TANG", "", "NANO", "", "20K", "",
    "32", "VOICES", "64", "SPRITES", "ONE", "FIVE", "ZERO", "BPM",
    "NO", "BRAKES", "NO", "LIMITS", "FULL", "SPEED", "AHEAD", "!",
    "ARE", "YOU", "READY", "?", "", "", "", "",
};
static void sc_intro(uint8_t pend)
{
    uint8_t last = 0xFF;
    screen_reset();
    clear8(R0, 0); clear8(R1, 0); blt_wait();
    pl_y = 0; tn_y = 0; rows_ok = 0;            /* R0 and R1 are the intro's: the tunnel and rows come later */
    vid(VC_BMP | VC_HI);
    vid2(VC2_SPR);
    for (;;) {
        uint32_t buf;
        uint8_t bi, s16;
        const char *w;
        char cnt[2];
        tick();
        if (ph_now >= pend || song_done) break;
        if (pl_y < 180) plasma_rows(1);
        bi = (uint8_t)((T >> 10) & 31);
        s16 = STEP16;
        w = intro_words[bi];
        if (bar_now == 7) { cnt[0] = (char)('8' - ((T >> 9) & 7)); cnt[1] = 0; w = cnt; }
        buf = fb_back();
        clear8(buf, 0);
        if (bar_now >= 4) {                     /* stripes slamming in on the 16ths */
            uint8_t i, n = (uint8_t)(bar_now >= 6 ? 5 : 3);
            rnd_s = (uint16_t)(0x1234 + s16 * 77);
            for (i = 0; i < n; i++) {
                int y = (int)(rnd() % 340), h = (int)(4 + (rnd() & 15));
                rect8(buf, 0, y, 640, h, (uint8_t)(C_WORD + 1 + (i % 3)));
            }
        }
        if (*w) {
            int len = strlen16(w), sc = len <= 2 ? 10 : (len <= 4 ? 8 : (len <= 6 ? 6 : 5));
            int k = beat_pos < 128 ? 1 : 0;     /* a beat's start: one size bigger */
            sc += k;
            cv_screen(buf, 1);
            cv_text8(320 - text8_w(w, sc) / 2, 180 - 7 * sc, w, C_WORD, sc);
        }
        if (bi != last) last = bi;
        {
            uint16_t wk = beat_pos < 512 ? (uint16_t)(255 - (beat_pos >> 2)) : 128;
            uint16_t fl = bar_now == 7 ? (uint16_t)((T >> 4) & 255) : 0;
            pb_set(C_WORD, mix565(dim565(RGB(255, 255, 255), wk), 0xFFFF, fl));
            pb_set(C_WORD + 1, hsv565((uint8_t)(T >> 6), 255, 200));
            pb_set(C_WORD + 2, hsv565((uint8_t)((T >> 6) + 85), 255, 160));
            pb_set(C_WORD + 3, hsv565((uint8_t)((T >> 6) + 170), 255, 120));
        }
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        fb_flip();
        pal_flush();
    }
}

/* ---------------------------------------------------------------- 1-2: the first drop: the plasma */
static void logo_bounce(int y, uint8_t amp)
{
    letters(0, "OVERDRIVE", 320 - 9 * 28 / 2, y, 28, 24, (uint8_t)(T >> 3), amp);
}
static uint8_t morph(uint32_t t, uint8_t *a, uint8_t *b)    /* a new shape every 4 bars, morphing over a beat */
{
    uint16_t blk = (uint16_t)(t >> 14), in = (uint16_t)((t >> 2) & 4095);
    *a = (uint8_t)((blk + NSHAPE - 1) % NSHAPE); *b = (uint8_t)(blk % NSHAPE);
    return in >= 256 ? 255 : (uint8_t)in;
}
static void sc_plasma(uint8_t pend)
{
    static const uint8_t sch[2][4] = {{0, 1, 2, 3}, {7, 4, 2, 0}};
    while (pl_y < 180) plasma_rows(180);
    screen_reset();
    set_lscr(LSCR);
    lscr_flat();
    show_field(B_PLASMA);
    vid(VC_BMP | VC_HI);
    vid2(VC2_LSCR | VC2_SPR);
    for (;;) {
        uint8_t a, b, mk, second;
        uint16_t fl, cr;
        tick();
        if (ph_now >= pend || song_done) break;
        second = ph_now >= 2;
        if (tn_y < 91) tunnel_rows(4);
        ramp_set(sch[second][bar_now & 3]);
        cr = ago(crash_step);
        fl = (uint16_t)(flash_k >> 1);
        if (cr < 2048) fl = (uint16_t)(fl + ((2048 - cr) >> 3));
        field_pal(6, (uint8_t)(T >> 5), fl, second && clap_k > 160);
        pal_fixed(256);
        pal_font((uint8_t)(T >> 4), (uint16_t)(flash_k >> 2));
        mk = morph(T, &a, &b);
        frame_end();
        lscr_wave((uint8_t)(6 + (flash_k >> 4)), 0, 3, (uint8_t)(T >> 6));
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        logo_bounce(36, (uint8_t)(10 + (flash_k >> 4)));
        balls(a, b, mk, (uint8_t)(T >> 5), (uint8_t)(T >> 6), 320, 220, (int16_t)(330 + (flash_k >> 1)), NBALL);
    }
}

/* ---------------------------------------------------------------- 3-4: the tunnel */
static void sc_tunnel(uint8_t pend)
{
    static const uint8_t sch[4] = {3, 0, 7, 4};
    while (tn_y < 91) tunnel_rows(90);
    rows_build();
    show_field(B_TUNNEL);
    vid2(VC2_LSCR | VC2_SPR);
    balls_off();
    for (;;) {
        uint8_t fill;
        uint16_t fl;
        tick();
        if (ph_now >= pend || song_done) break;
        fill = ph_now == 4 && bar_now == 7;
        ramp_set(sch[bar_now & 3]);
        fl = (uint16_t)(flash_k >> 1);
        if (ago(crash_step) < 2048) fl = (uint16_t)(fl + ((2048 - ago(crash_step)) >> 3));
        field_pal(8, (uint8_t)((T >> 4) + (flash_k >> 3)), fl, fill && (b16_now & 1));
        pal_fixed(256);
        pal_font((uint8_t)(T >> 4), 0);
        frame_end();
        lscr_wave((uint8_t)(18 + (SIN((uint8_t)(T >> 9)) >> 3)), 0, 1, (uint8_t)(T >> 7));
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        if (ph_now == 3) {
            letters_off();
            balls(4, 4, 0, (uint8_t)(T >> 4), (uint8_t)(64 + (SIN((uint8_t)(T >> 8)) >> 2)), 320, 180, (int16_t)(240 + flash_k), NBALL);
        } else {                                /* the logo on a ring around the tunnel */
            const char *s = "OVERDRIVE";
            uint8_t i;
            int r = 130 + (flash_k >> 3);
            for (i = 0; i < 9; i++) {
                uint8_t an = (uint8_t)((T >> 5) + i * 28);
                spr_set(i, 320 + (COS(an) * r >> 7) - 14, 180 + (SIN(an) * r * 3 / 4 >> 7) - 15, 30, glyph(s[i]), 2, 0);
            }
            balls(0, 0, 0, (uint8_t)(T >> 3), (uint8_t)(T >> 4), 320, 180, (int16_t)(120 + (flash_k >> 2)), 24);
        }
    }
}

/* ---------------------------------------------------------------- 5: the breakdown: raster bars, a scroller */
static const char scroll_msg[] =
    "OVERDRIVE ... A DEMO FOR THE T1 ON THE TANG NANO 20K ... 150 BEATS A MINUTE, 32 VOICES, 64 SPRITES, "
    "ONE BLITTER AND A PALETTE THAT NEVER SITS STILL ... EVERY PIXEL AND EVERY NOTE IS MADE AS IT PLAYS ... "
    "HANDS IN THE AIR ... HERE COMES THE DROP ...         ";
static void sc_bars(uint8_t pend)
{
    if (!rows_ok) rows_build();
    show_field(B_ROWS);
    vid2(VC2_SPR);
    balls_off();
    scroll_a(0, 0);
    for (;;) {
        uint32_t u;
        tick();
        if (ph_now >= pend || song_done) break;
        u = T - PHRASE(5);
        bars_pal(7, 0, 0, (uint8_t)(T >> 8));
        pal_fixed(256);
        pal_font((uint8_t)(T >> 4), 0);
        frame_end();
        scroller(scroll_msg, (uint16_t)(sizeof scroll_msg - 1), (uint32_t)(u >> 4) + 0, 165, 70, (uint8_t)(T >> 4));
        balls(0, 1, (uint8_t)(SIN((uint8_t)(T >> 9)) + 128), (uint8_t)(T >> 5), 40, 320, 60, 110, 20);
    }
    letters_off();
}

/* ---------------------------------------------------------------- 6, 11: the builds: cuts speeding up */
static void sc_build(uint8_t pend)
{
    uint8_t cur = 0xFF;
    vid2(VC2_LSCR | VC2_SPR);
    set_lscr(LSCR);
    balls_off();
    for (;;) {
        uint8_t iv, f, s16;
        uint16_t fl;
        char cnt[2];
        tick();
        if (ph_now >= pend || song_done) break;
        s16 = STEP16;
        iv = bar_now < 2 ? 16 : (bar_now < 4 ? 8 : (bar_now < 6 ? 4 : (bar_now == 6 ? 2 : 1)));
        f = (uint8_t)((s16 / iv) % 3);
        fl = (uint16_t)((bar_now == 7 ? (uint16_t)(((T >> 4) & 255)) : 0) + (flash_k >> 2));
        if (f != cur) { show_field(f == 0 ? B_PLASMA : (f == 1 ? B_TUNNEL : B_ROWS)); cur = f; }
        if (f == 2) bars_pal(9, 1, fl, (uint8_t)(T >> 6));
        else {
            ramp_set(f == 0 ? 4 : 3);
            field_pal((uint8_t)(f == 0 ? 6 : 8), (uint8_t)(T >> 4), fl, 0);
        }
        pal_fixed(256);
        pal_font((uint8_t)(T >> 3), fl);
        frame_end();
        if (bar_now == 7) lscr_glitch(60);
        else lscr_wave((uint8_t)(bar_now * 3), 0, 2, (uint8_t)(T >> 5));
        scroll_a(0, 0);
        if (bar_now < 6) letters(0, ph_now == 6 ? "BUILDING UP" : "ONE MORE TIME", 320 - 11 * 28 / 2, 165, 28, 20, (uint8_t)(T >> 3), 12 + bar_now * 4);
        else if (bar_now == 6) letters(0, "FASTER", 320 - 6 * 28 / 2 + (int)(rnd() & 7), 165, 28, 40, (uint8_t)(T >> 2), 30);
        else { cnt[0] = (char)('8' - ((T >> 9) & 7)); cnt[1] = 0; letters(0, cnt, 306, 165, 28, 0, 0, 0); }
    }
    lscr_flat();
}

/* ---------------------------------------------------------------- 7-9: the second drop: the dancers */
static void sc_dance(uint8_t pend)
{
    uint8_t which = 0;
    clear8(R0, 0); clear8(R1, 0); blt_wait();
    box_valid[0] = box_valid[1] = 0;
    rows_ok = 0; tn_y = 0;                      /* the tunnel and the rows are drawn over */
    show_field(B_PLASMA);
    set_pfb(R1);
    scroll_b(0, 0);
    vid(VC_BMP | VC_HI);
    vid2(VC2_PFB | VC2_BFRONT | VC2_LSCR | VC2_SPR);
    letters_off(); balls_off();
    for (;;) {
        uint32_t buf = which ? R1 : R0;
        pose_t p, m;
        uint8_t set, three, i;
        tick();
        if (ph_now >= pend || song_done) break;
        set = (uint8_t)(ph_now & 1);
        three = ph_now >= 8;
        dance_clear(which, buf);
        pose_now(set, &p);
        if (three) {                            /* two more, smaller and mirrored, either side */
            m.dy = p.dy;
            m.a[0] = (uint8_t)(0 - p.a[0]);
            for (i = 0; i < 2; i++) {
                m.a[1 + i] = (uint8_t)(0 - p.a[3 + i]); m.a[3 + i] = (uint8_t)(0 - p.a[1 + i]);
                m.a[5 + i] = (uint8_t)(0 - p.a[7 + i]); m.a[7 + i] = (uint8_t)(0 - p.a[5 + i]);
            }
            {                                   /* one drawn, the other copied (the same pose) */
                int16_t sb[4];
                for (i = 0; i < 4; i++) sb[i] = d_box[i];
                d_box[0] = 320; d_box[1] = 180; d_box[2] = -1; d_box[3] = -1;
                dancer(&m, 130, 230, 40, C_DANCE + 1);
                if (d_box[0] < 0) d_box[0] = 0;
                if (d_box[1] < 0) d_box[1] = 0;
                if (d_box[3] > 179) d_box[3] = 179;
                if (d_box[2] > 129) d_box[2] = 129;
                blt_copy(buf + (uint32_t)d_box[1] * 2 * W + (uint32_t)d_box[0] * 2, W,
                         buf + (uint32_t)d_box[1] * 2 * W + (uint32_t)d_box[0] * 2 + 380, W,
                         (uint16_t)((d_box[2] - d_box[0] + 1) * 2), (uint16_t)((d_box[3] - d_box[1] + 1) * 2), 0, 0);
                d_box[2] = (int16_t)(d_box[2] + 190);
                if (sb[0] < d_box[0]) d_box[0] = sb[0];
                if (sb[1] < d_box[1]) d_box[1] = sb[1];
                if (sb[2] > d_box[2]) d_box[2] = sb[2];
                if (sb[3] > d_box[3]) d_box[3] = sb[3];
            }
        }
        dancer(&p, 320, 206, 64, C_DANCE);
        dance_done(which);
        blt_wait();
        set_pfb(buf);
        which ^= 1;
        ramp_set(ph_now == 9 && (bar_now & 1) ? 7 : 5);
        field_pal(5, (uint8_t)(T >> 6), (uint16_t)(flash_k >> 2), ph_now == 9 && clap_k > 160);
        {
            uint16_t c = hsv565((uint8_t)(T >> 7), 120, 255);
            pb_set(C_DANCE, mix565(c, 0xFFFF, flash_k));
            pb_set(C_DANCE + 1, dim565(hsv565((uint8_t)((T >> 7) + 128), 200, 255), 200));
        }
        pal_fixed(256);
        pal_font((uint8_t)(T >> 4), (uint16_t)(flash_k >> 1));
        wait_flip();
        pal_flush();
        fps_tick();
        lscr_wave((uint8_t)(4 + (flash_k >> 5)), (uint8_t)(clap_k >> 4), 4, (uint8_t)(T >> 5));
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        letters(0, ph_now == 7 ? "MOVE" : (ph_now == 8 ? "DANCE" : "JUMP"), 320 - 5 * 28 / 2, 322, 28, 30, (uint8_t)(T >> 3), 8);
        if (ph_now == 9) balls(1, 1, 0, (uint8_t)(T >> 4), 70, 320, 250, 360, 24);
    }
    vid2(VC2_LSCR | VC2_SPR);
    balls_off();
}

/* ---------------------------------------------------------------- 10: the acid break */
static void sc_acid(uint8_t pend)
{
    show_field(B_PLASMA);
    vid2(VC2_LSCR | VC2_SPR);
    for (;;) {
        uint8_t a, b, mk;
        tick();
        if (ph_now >= pend || song_done) break;
        if (tn_y < 91) tunnel_rows(4);
        else if (!rows_ok) rows_build();
        ramp_set(2);
        field_pal((uint8_t)(4 + (bar_now & 3) * 2), (uint8_t)(T >> 5), (uint16_t)(flash_k >> 2), 0);
        pal_fixed(256);
        pal_font((uint8_t)(T >> 4), 0);
        mk = morph(T << 2, &a, &b);              /* a new shape every bar */
        frame_end();
        lscr_wave((uint8_t)(26 + (SIN((uint8_t)(T >> 7)) >> 3)), 0, (uint8_t)(1 + (b16_now & 3)), (uint8_t)(T >> 4));
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        letters(0, "ACID", 320 - 4 * 28 / 2, 30, 28, 40, (uint8_t)(T >> 3), 14);
        balls(a, b, mk, (uint8_t)(T >> 4), (uint8_t)(T >> 5), 320, 200, (int16_t)(420 + flash_k), NBALL);
    }
}

/* ---------------------------------------------------------------- 12-14: the last drop: a cut every beat */
static void sc_final(uint8_t pend)
{
    uint8_t cur = 0xFF;
    vid2(VC2_LSCR | VC2_SPR);
    for (;;) {
        uint8_t f, fill, a, b, mk;
        uint16_t fl, beat;
        tick();
        if (ph_now >= pend || song_done) break;
        beat = (uint16_t)(T >> 10);
        fill = ph_now == 14 && bar_now == 7;
        f = (uint8_t)(fill ? (b16_now % 3) : (beat % 3));
        if (f != cur) { show_field(f == 0 ? B_PLASMA : (f == 1 ? B_TUNNEL : B_ROWS)); cur = f; }
        fl = (uint16_t)(flash_k >> 1);
        if (ago(crash_step) < 2048) fl = (uint16_t)(fl + ((2048 - ago(crash_step)) >> 3));
        if (f == 2) bars_pal(9, 1, fl, (uint8_t)(T >> 6));
        else {
            static const uint8_t sch[8] = {0, 7, 4, 1, 3, 2, 6, 0};
            ramp_set(sch[(beat >> 1) & 7]);
            field_pal((uint8_t)(f == 0 ? 6 : 8), (uint8_t)(T >> 4), fl, (ph_now == 13 && clap_k > 160) || (fill && (b16_now & 1)));
        }
        pal_fixed(256);
        pal_font((uint8_t)(T >> 3), (uint16_t)(flash_k >> 1));
        mk = morph(T << 1, &a, &b);
        frame_end();
        if (fill) lscr_glitch(80);
        else if (f == 1) lscr_wave(18, 0, 1, (uint8_t)(T >> 7));
        else lscr_wave((uint8_t)(6 + (flash_k >> 4)), 0, 3, (uint8_t)(T >> 6));
        scroll_a(0, (uint16_t)((360 + shake_y()) % 360));
        if (f == 2) scroller(scroll_msg, (uint16_t)(sizeof scroll_msg - 1), T >> 3, 40, 24, (uint8_t)(T >> 3));
        else logo_bounce(f == 1 ? 165 : 36, (uint8_t)(12 + (flash_k >> 3)));
        balls(a, b, mk, (uint8_t)(T >> 4), (uint8_t)(T >> 5), 320, f == 1 ? 180 : 220, (int16_t)(f == 1 ? 260 + flash_k : 360 + (flash_k >> 1)), NBALL);
    }
}

/* ---------------------------------------------------------------- 15: the end */
static void sc_end(uint8_t pend)
{
    uint8_t txt = 0;
    if (!rows_ok) rows_build();
    show_field(B_ROWS);
    vid2(VC2_SPR);
    vid(VC_BMP | VC_HI | VC_TEXT);
    tclear();
    balls_off();
    for (;;) {
        uint8_t s16;
        uint16_t after;
        tick();
        if (song_done || ph_now >= pend) break;
        s16 = STEP16;
        after = s16 >= 64 ? (uint16_t)((T - PHRASE(15) - (64UL << 8)) >> 2) : 0;   /* 0..: since the last hit */
        if (s16 < 64) {
            bars_pal(9, 1, (uint16_t)(flash_k >> 1), (uint8_t)(T >> 6));
            pal_fixed(256);
            pal_font((uint8_t)(T >> 3), (uint16_t)(flash_k >> 1));
        } else {                                /* the last hit: white, then black */
            uint16_t k = after < 256 ? (uint16_t)(255 - after) : 0, i;
            for (i = 1; i <= F_N; i++) pbuf[i] = dim565(0xFFFF, k);
            pal_font((uint8_t)(T >> 4), k);
        }
        frame_end();
        if (s16 < 64) logo_bounce(165, (uint8_t)(20 + (flash_k >> 3)));
        else logo_bounce(110, 0);
        if (s16 >= 72 && !txt) {
            tstr(26, 12, "CODE, PICTURES AND MUSIC", TA(7, 0) | TCLEAR);
            tstr(37, 13, "CLAUDE", TA(6, 0) | TCLEAR);
            tstr(35, 15, "CONCEPT", TA(7, 0) | TCLEAR);
            tstr(32, 16, "JOSEPH JULICHER", TA(6, 0) | TCLEAR);
            tstr(27, 19, "T1 ON THE TANG NANO 20K", TA(3, 0) | TCLEAR);
            txt = 1;
        }
    }
    {
        uint16_t f0 = FRAME;                    /* hold the credits, then start again */
        while ((uint16_t)(FRAME - f0) < 240) poll();
    }
    spr_alloff();
    tclear();
}

typedef void (*scene_fn)(uint8_t pend);
static const struct { uint8_t pend; scene_fn f; } scenes[] = {
    {1, sc_intro}, {3, sc_plasma}, {5, sc_tunnel}, {6, sc_bars}, {7, sc_build},
    {10, sc_dance}, {11, sc_acid}, {12, sc_build}, {15, sc_final}, {16, sc_end},
};

int main(void)
{
    uint8_t i;
    IO(0x1000) = 0x0900;
    screen_reset();
    sprites_build();
    pl_y = 0; tn_y = 0; rows_ok = 0;
    for (;;) {
        song_start(START);
        if (START > 0) {                        /* a test start: the fields the scenes expect */
            while (pl_y < 180) plasma_rows(180);
            if (START < 7 || START > 10) { while (tn_y < 91) tunnel_rows(90); rows_build(); }
        }
        for (i = 0; i < sizeof scenes / sizeof scenes[0]; i++)
            if (scenes[i].pend > cur_phrase() && !song_done) scenes[i].f(scenes[i].pend);
        while (!song_done) poll();
        pl_y = 180;                             /* the plasma survives; the others are rebuilt */
        tn_y = 0; rows_ok = 0;
    }
}
