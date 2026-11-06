/* Overdrive: the effects. Everything that moves every frame is something the hardware does for a register
 * write: the palette turns precomputed fields (a plasma, a tunnel, raster rows), line scroll bends them,
 * the scroll registers shake them, and up to 64 sprites carry the letters and the vector balls. The CPU
 * draws only the dancer (spans by the blitter) and builds the fields a few rows a frame beforehand.
 *
 * Buffers: R0, R1, R2 (640 x 360 8 bpp): the fields, or the dancer's two buffers on playfield B.
 * Palette:   1-190 the field showing; 192-207 the balls' shades; 208-235 the letters' rows, 236 their
 *            shadow; 240-247 the dancers; 248-255 the intro's words and stripes. */

#define F_N 190                                 /* the field's colours: 1..190 */
#define C_BALL 192
#define C_FONT 208
#define C_SHADOW 236
#define C_DANCE 240
#define C_WORD 248

#define SPR_FONT (SMP + 0x7C00UL)                /* 0xF1600: letter sprites, 32 x 30, 960 bytes each */
#define FONT_SZ 960UL
#define FONT_CH "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!?.,-':"
#define N_FONT 43
#define SPR_BALL16 (SPR_FONT + N_FONT * FONT_SZ)  /* a 16 x 16 ball */
#define SPR_BALL32 (SPR_BALL16 + 256UL)           /* a 32 x 32 ball */
#define SPR_END (SPR_BALL32 + 1024UL)

#define B_PLASMA R2
#define B_TUNNEL R1
#define B_ROWS R0

/* ---------------------------------------------------------------- time */
static uint32_t T;                              /* the song position, 16ths x 256 */
static uint8_t ph_now, bar_now, b16_now;        /* the phrase, the bar in it (0-7), the 16th in the bar */
static uint16_t beat_pos;                       /* 0-1023 through the beat */
static uint16_t flash_k;                        /* 256 at a kick, falling over a beat */
static uint16_t clap_k;                         /* the same for the clap */
static uint16_t ago(uint16_t step)              /* 16ths x 256 since a step (0xFFFF: long ago) */
{
    uint32_t k = (uint32_t)step << 8;
    if (step == 0xFFFF || T < k || T - k > 0xFFFF) return 0xFFFF;
    return (uint16_t)(T - k);
}
static void tick(void)
{
    uint16_t sk;
    T = mpos();
    ph_now = (uint8_t)(T >> 15);
    bar_now = (uint8_t)((T >> 12) & 7);
    b16_now = (uint8_t)((T >> 8) & 15);
    beat_pos = (uint16_t)(T & 1023);
    sk = ago(kick_step);
    flash_k = sk < 1024 ? (uint16_t)((1024 - sk) >> 2) : 0;
    sk = ago(clap_step);
    clap_k = sk < 512 ? (uint16_t)((512 - sk) >> 1) : 0;
}

/* ---------------------------------------------------------------- the palette */
static uint16_t ramp[256];                      /* the field's colour cycle */
static uint16_t pbuf[256];                      /* the palette, composed during the frame, written in the blank */
static void pal_flush(void) { pal_run(1, pbuf + 1, 255); }
static void pb_set(uint8_t i, uint16_t c) { pbuf[i] = c; }
static uint8_t ramp_id = 0xFF;
/* schemes: four colours around the cycle */
static const uint16_t schemes[8][4] = {
    {RGB(20, 0, 40), RGB(230, 30, 120), RGB(255, 200, 60), RGB(40, 200, 255)},     /* 0 neon */
    {RGB(0, 0, 0), RGB(200, 20, 0), RGB(255, 160, 0), RGB(255, 255, 160)},         /* 1 fire */
    {RGB(0, 20, 10), RGB(40, 255, 60), RGB(240, 255, 40), RGB(0, 120, 120)},       /* 2 acid */
    {RGB(0, 0, 40), RGB(30, 90, 255), RGB(200, 240, 255), RGB(110, 0, 200)},       /* 3 ice */
    {RGB(40, 0, 20), RGB(255, 60, 60), RGB(255, 255, 255), RGB(255, 80, 200)},     /* 4 hot */
    {RGB(0, 0, 0), RGB(0, 70, 90), RGB(0, 10, 30), RGB(60, 0, 80)},               /* 5 dark (behind the dancer) */
    {RGB(10, 10, 10), RGB(250, 250, 250), RGB(10, 10, 10), RGB(250, 250, 250)},    /* 6 black and white */
    {RGB(60, 0, 90), RGB(255, 0, 140), RGB(0, 0, 0), RGB(0, 230, 255)},            /* 7 rave */
};
static void ramp_set(uint8_t id)
{
    uint16_t i;
    if (id == ramp_id) return;
    ramp_id = id;
    for (i = 0; i < 256; i++) {
        uint8_t q = (uint8_t)(i >> 6), f = (uint8_t)(i & 63);
        ramp[i] = mix565(schemes[id][q], schemes[id][(q + 1) & 3], (uint16_t)(f << 2));
    }
}
/* the field's palette: entry i is ramp[i * mul / 4 + shift], toward white by fl, inverted by inv */
static void field_pal(uint8_t mul, uint8_t shift, uint16_t fl, uint8_t inv)
{
    uint8_t i;
    uint16_t a = (uint16_t)shift << 2, *d = pbuf + 1;
    for (i = 0; i < F_N; i++) {
        uint16_t c = ramp[(uint8_t)(a >> 2)];
        if (fl) c = mix565(c, 0xFFFF, fl > 255 ? 255 : fl);
        if (inv) c = (uint16_t)~c;
        *d++ = c;
        a = (uint16_t)(a + mul);
    }
}
static void pal_fixed(uint16_t k)               /* the sprites' and dancers' colours, k/256 bright */
{
    uint8_t i;
    for (i = 0; i < 16; i++) {                  /* the balls: dark rim to a white highlight */
        uint16_t c = i < 12 ? mix565(RGB(30, 10, 60), RGB(255, 80, 180), (uint16_t)(i * 22)) : mix565(RGB(255, 80, 180), RGB(255, 255, 255), (uint16_t)((i - 11) * 60));
        pb_set((uint8_t)(C_BALL + i), dim565(c, k));
    }
    pb_set(C_SHADOW, RGB(0, 0, 0));
}
static void pal_font(uint8_t shift, uint16_t fl)  /* the letters: a gradient down each one, moving */
{
    uint8_t i;
    for (i = 0; i < 28; i++) {
        uint16_t c = hsv565((uint8_t)(shift + i * 5), i < 4 ? 80 : 220, 255);
        pbuf[C_FONT + i] = fl ? mix565(c, 0xFFFF, fl > 255 ? 255 : fl) : c;
    }
}

/* ---------------------------------------------------------------- line scroll and shake */
static void lscr_wave(uint8_t ampa, uint8_t ampb, uint8_t freq, uint8_t phase)
{
    volatile uint16_t *ls = (volatile uint16_t *)LSCR;
    uint16_t y;
    uint8_t a = phase;
    for (y = 0; y < 360; y++) {
        int8_t s = SIN(a);
        ls[0] = (uint16_t)((640 + ((s * ampa) >> 7)) % 640);
        ls[1] = (uint16_t)((640 + ((s * ampb) >> 7)) % 640);
        ls += 2;
        a = (uint8_t)(a + freq);
    }
}
static void lscr_glitch(uint8_t amt)            /* bands torn sideways */
{
    volatile uint16_t *ls = (volatile uint16_t *)LSCR;
    uint16_t y = 0;
    while (y < 360) {
        uint16_t h = (uint16_t)(4 + (rnd() & 31)), o = (rnd() & 3) ? 0 : (uint16_t)((rnd() % (amt + 1)) * ((rnd() & 1) ? 1 : 639) % 640);
        while (h-- && y < 360) { ls[2 * y] = o; ls[2 * y + 1] = o; y++; }
    }
}
static void lscr_flat(void) { lscr_wave(0, 0, 0, 0); }
static int16_t shake_y(void)                    /* a jolt down at the kick, settling */
{
    return flash_k > 128 ? (int16_t)((flash_k - 128) >> 4) * ((ticks50 & 1) ? 1 : -1) : 0;
}

/* ---------------------------------------------------------------- the fields */
/* the plasma: per half-resolution pixel, four sines (one of them of the distance from a point) */
static uint8_t pl_y;
static void plasma_rows(uint8_t nrows)
{
    uint8_t y;
    for (y = pl_y; y < 180 && y < pl_y + nrows; y++) {
        volatile uint16_t *row = (volatile uint16_t *)(B_PLASMA + (uint32_t)y * 2 * W);
        int16_t dy = (int16_t)y - 70, sy = SIN((uint8_t)(y * 3));
        int32_t dy2 = (int32_t)dy * dy;
        uint16_t x;
        for (x = 0; x < 320; x++) {
            int16_t dx = (int16_t)x - 200;
            uint16_t d = isqrt32((uint32_t)((int32_t)dx * dx + dy2));
            int16_t v = (int16_t)(SIN((uint8_t)(x * 2)) + sy + SIN((uint8_t)((x + y * 2) * 3 / 2)) + SIN((uint8_t)(d * 3)));
            uint8_t c = (uint8_t)(1 + (uint16_t)((uint32_t)(v + 512) * (F_N - 1) >> 10));
            row[x] = (uint16_t)(c | c << 8);
        }
        blt_copy(B_PLASMA + (uint32_t)y * 2 * W, W, B_PLASMA + (uint32_t)y * 2 * W + W, W, W, 1, 0, 0);
        poll();
    }
    pl_y = y;
}
/* the tunnel: (angle + depth) mod 190 from a quadrant precomputed by tools/mkassets.py (angle, depth),
 * mirrored into the four quadrants at half resolution, then the rows doubled */
static uint8_t tn_y;
static void tunnel_rows(uint8_t nrows)
{
    uint8_t y;
    for (y = tn_y; y < 90 && y < tn_y + nrows; y++) {
        const uint8_t *ta = tun_a + (uint16_t)y * 160, *tz = tun_z + (uint16_t)y * 160;
        volatile uint16_t *r0 = (volatile uint16_t *)(B_TUNNEL + (uint32_t)(180 + y * 2) * W + 320);
        volatile uint16_t *r1 = (volatile uint16_t *)(B_TUNNEL + (uint32_t)(178 - y * 2) * W + 320);
        uint8_t x;
        for (x = 0; x < 160; x++) {
            uint8_t a = ta[x], z = tz[x];
            uint16_t v;
            uint8_t c;
            v = (uint16_t)((((uint16_t)a * 95) >> 7) + z); if (v >= F_N) v -= F_N; c = (uint8_t)(1 + v);
            r0[x] = (uint16_t)(c | c << 8);                         /* q0: right, below */
            v = (uint16_t)((((uint16_t)(uint8_t)(128 - a) * 95) >> 7) + z); if (v >= F_N) v -= F_N; c = (uint8_t)(1 + v);
            r0[-1 - (int16_t)x] = (uint16_t)(c | c << 8);           /* q1: left, below */
            v = (uint16_t)((((uint16_t)(uint8_t)(0 - a) * 95) >> 7) + z); if (v >= F_N) v -= F_N; c = (uint8_t)(1 + v);
            r1[x] = (uint16_t)(c | c << 8);                         /* q2: right, above */
            v = (uint16_t)((((uint16_t)(uint8_t)(128 + a) * 95) >> 7) + z); if (v >= F_N) v -= F_N; c = (uint8_t)(1 + v);
            r1[-1 - (int16_t)x] = (uint16_t)(c | c << 8);           /* q3: left, above */
        }
        poll();
    }
    tn_y = y;
    if (tn_y == 90) {
        blt_copy(B_TUNNEL, 2 * W, B_TUNNEL + W, 2 * W, W, 180, 0, 0);
        blt_wait();
        tn_y = 91;
    }
}
static uint8_t rows_ok;
static void rows_build(void)                    /* the raster rows: line y is colour 1 + y / 2 */
{
    uint16_t y;
    for (y = 0; y < 360; y++) blt_fill(B_ROWS + (uint32_t)y * W, W, W, 1, (uint8_t)(1 + (y >> 1)));
    blt_wait();
    rows_ok = 1;
}
/* raster bars on the rows field: n bars bobbing, over a dark gradient */
static void bars_pal(uint8_t n, uint8_t speed, uint16_t fl, uint8_t hue0)
{
    uint16_t *col = pbuf;
    uint8_t i, t = (uint8_t)(T >> (speed ? 6 : 7));
    int16_t y;
    for (y = 1; y <= 180; y++) col[y] = mix565(RGB(10, 0, 30), RGB(0, 0, 0), (uint16_t)(y < 90 ? 255 - y * 2 : (y - 90) * 2));
    for (i = 0; i < n; i++) {
        int16_t c = (int16_t)(90 + (SIN((uint8_t)(t * (2 + (i & 1)) + i * 37)) * 78 >> 7));
        uint16_t hc = hsv565((uint8_t)(hue0 + i * (256 / n)), 230, 255);
        int16_t d;
        for (d = -9; d <= 9; d++) {
            int16_t yy = (int16_t)(c + d);
            uint16_t k = (uint16_t)(255 - (d < 0 ? -d : d) * 26);
            if (yy < 1 || yy > 180) continue;
            col[yy] = (d == -2 || d == -1) ? mix565(hc, 0xFFFF, 180) : dim565(hc, k);
        }
    }
    if (fl) for (y = 1; y <= 180; y++) col[y] = mix565(col[y], 0xFFFF, fl > 255 ? 255 : fl);
    for (y = 181; y <= F_N; y++) col[y] = 0;
}

/* ---------------------------------------------------------------- sprite images */
static uint32_t glyph(char ch)
{
    const char *p = FONT_CH;
    uint8_t i;
    for (i = 0; p[i]; i++) if (p[i] == ch) return SPR_FONT + i * FONT_SZ;
    return 0;
}
COLD static void sprites_build(void)
{
    uint8_t g, r, c, dy, s;
    volatile uint8_t *m;
    for (g = 0; g < N_FONT; g++) {
        const uint8_t *f = font8x14[(uint8_t)FONT_CH[g]];
        m = (volatile uint8_t *)(SPR_FONT + g * FONT_SZ);
        { uint16_t z; for (z = 0; z < 30 * 32 / 2; z++) ((volatile uint16_t *)m)[z] = 0; }
        for (s = 0; s < 2; s++)                 /* the shadow, then the letter, 3 x 2 pixels a dot */
            for (r = 0; r < 14; r++)
                for (c = 0; c < 8; c++)
                    if (f[r] >> (7 - c) & 1)
                        for (dy = 0; dy < 2; dy++) {
                            uint16_t o = (uint16_t)((r * 2 + dy + (s ? 0 : 2)) * 32 + c * 3 + (s ? 0 : 2));
                            uint8_t v = s ? (uint8_t)(C_FONT + r * 2 + dy) : C_SHADOW;
                            m[o] = v; m[o + 1] = v; m[o + 2] = v; m[o + 3] = v;
                        }
        poll();
    }
    for (s = 0; s < 2; s++) {                   /* the balls: lit from the top left */
        uint8_t n = s ? 32 : 16;
        int16_t rr = (int16_t)(n / 2), x, y;
        m = (volatile uint8_t *)(s ? SPR_BALL32 : SPR_BALL16);
        for (y = 0; y < n; y++)
            for (x = 0; x < n; x++) {
                int16_t dx = (int16_t)(2 * x + 1 - n), dyy = (int16_t)(2 * y + 1 - n);
                int32_t d2 = (int32_t)dx * dx + (int32_t)dyy * dyy, R2_ = (int32_t)n * n;
                uint8_t v = 0;
                if (d2 < R2_) {
                    int16_t lx = (int16_t)(dx + n / 2), ly = (int16_t)(dyy + n / 2);
                    int32_t l2 = (int32_t)lx * lx + (int32_t)ly * ly;
                    int16_t sh = (int16_t)(15 - (int16_t)(l2 * 15 / ((int32_t)R2_ * 2)));
                    if (sh < 0) sh = 0;
                    if (sh > 15) sh = 15;
                    if (d2 > R2_ - n * 3) sh = sh > 3 ? 3 : sh;
                    v = (uint8_t)(C_BALL + sh);
                }
                m[y * n + x] = v;
            }
        (void)rr;
    }
}

/* ---------------------------------------------------------------- letters as sprites */
static uint8_t spr_used;                        /* sprites 0..spr_used-1 were shown last frame */
static void letters(uint8_t first, const char *s, int x, int y, int dx, uint8_t wave, uint8_t wph, int wamp)
{
    uint8_t n = first;
    for (; *s && n < 20; s++, x += dx) {
        uint32_t a = glyph(*s);
        int yy = y + (wave ? (SIN((uint8_t)(wph + (uint8_t)(n * wave))) * wamp >> 7) : 0);
        if (!a || x < -32 || x > 660) continue;
        spr_set(n++, x, yy, 30, a, 2, 0);
    }
    while (n < 20) spr_off(n++);
}
static void letters_off(void) { uint8_t i; for (i = 0; i < 20; i++) spr_off(i); }
/* the scroller: letters 40 apart moving left, each on a sine */
static void scroller(const char *msg, uint16_t len, uint32_t pos, int y, int amp, uint8_t ph)
{
    uint8_t n = 0;
    uint16_t first = (uint16_t)(pos / 40);
    int x0 = -(int)(pos % 40);
    uint8_t i;
    for (i = 0; i < 17 && n < 20; i++) {
        uint16_t k = (uint16_t)((first + i) % len);
        uint32_t a = glyph(msg[k]);
        int x = x0 + i * 40;
        if (!a) continue;
        spr_set(n++, x, y + (SIN((uint8_t)(x / 2 + ph)) * amp >> 7), 30, a, 2, 0);
    }
    while (n < 20) spr_off(n++);
}

/* ---------------------------------------------------------------- the vector balls (sprites 20-59) */
static int16_t vb_x[NBALL], vb_y[NBALL], vb_z[NBALL];
static uint8_t vb_ord[NBALL];
static void balls(uint8_t sh_a, uint8_t sh_b, uint16_t mk, uint8_t yaw, uint8_t pitch, int cx, int cy, int16_t size, uint8_t n)
{
    int16_t cy_ = COS(yaw), sy = SIN(yaw), cp = COS(pitch), sp = SIN(pitch);
    const int8_t *pa = shapes + sh_a * NBALL * 3, *pb = shapes + sh_b * NBALL * 3;
    uint8_t i, j;
    if (n > NBALL) n = NBALL;
    for (i = 0; i < n; i++) {
        int16_t x = (int16_t)(pa[3 * i] + (((int16_t)(pb[3 * i] - pa[3 * i]) * (int16_t)mk) >> 8));
        int16_t y = (int16_t)(pa[3 * i + 1] + (((int16_t)(pb[3 * i + 1] - pa[3 * i + 1]) * (int16_t)mk) >> 8));
        int16_t z = (int16_t)(pa[3 * i + 2] + (((int16_t)(pb[3 * i + 2] - pa[3 * i + 2]) * (int16_t)mk) >> 8));
        int16_t x1 = (int16_t)((x * cy_ - z * sy) >> 7), z1 = (int16_t)((x * sy + z * cy_) >> 7);
        int16_t y1 = (int16_t)((y * cp - z1 * sp) >> 7), z2 = (int16_t)((y * sp + z1 * cp) >> 7);
        int32_t dz = 300 + z2;
        vb_x[i] = (int16_t)(cx + (int32_t)x1 * size / dz);
        vb_y[i] = (int16_t)(cy + (int32_t)y1 * size / dz);
        vb_z[i] = z2;
        vb_ord[i] = i;
    }
    for (i = 1; i < n; i++) {                   /* nearest first: the lower sprite number is in front */
        uint8_t v = vb_ord[i];
        j = i;
        while (j && vb_z[vb_ord[j - 1]] > vb_z[v]) { vb_ord[j] = vb_ord[j - 1]; j--; }
        vb_ord[j] = v;
    }
    for (i = 0; i < n; i++) {
        uint8_t b = vb_ord[i], big = vb_z[b] < 20 && size > 200;
        if (big) spr_set((uint8_t)(20 + i), vb_x[b] - 16, vb_y[b] - 16, 32, SPR_BALL32, 2, 0);
        else spr_set((uint8_t)(20 + i), vb_x[b] - 8, vb_y[b] - 8, 16, SPR_BALL16, 1, 0);
    }
    for (; i < NBALL; i++) spr_off((uint8_t)(20 + i));
}
static void balls_off(void) { uint8_t i; for (i = 20; i < 20 + NBALL; i++) spr_off(i); }
