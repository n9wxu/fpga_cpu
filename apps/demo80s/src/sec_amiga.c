/* Section 3: the Amiga. Playfield B (behind) holds a copper-gradient sunset sky, mountains, and a perspective
 * checkerboard floor; each floor pixel's palette index encodes where it is on the checkerboard (eighths of a
 * square across and along, and a fog band), so the floor moves forwards and sideways by rewriting 128 palette
 * entries a frame. Playfield A (in front, double-buffered) holds a flat-shaded icosahedron, filled in spans by
 * the CPU, and a sine scroller made of 40 blitter copies a frame; the blitter clears them. Twelve shaded balls
 * orbit as hardware sprites. The music is the generated 4-channel module. */

#define M_SKY 64                                /* 64-163: the sky, two lines each (y < 200) */
#define M_MTN 20                                /* mountains (two tones) */
#define M_SUN 22
#define M_FLOOR 128                             /* 128-255: (fog, u eighth, v eighth) */
#define M_OBJ 32                                /* 32-47, 48-63: the object's two shade ramps */
#define M_SCR 24                                /* 24-31: the scroller's rows (copper colours) */
#define M_BALL 4                                /* 4-19: the balls' shades */
#define HZN 200
#define M_STRIP (SCR_FREE + 0x1000UL)
#define M_BALLIMG (SCR_FREE)
#define MSTRIP_W 2400

static uint32_t m_pfb, m_buf[2];
static int16_t m_bb[2][4];                      /* each buffer's last object box: x0 y0 x1 y1 */

/* ---- the background (playfield B), drawn once */
static void m_background(void)
{
    uint16_t y, x;
    clear8(m_pfb, 0);
    for (y = 0; y < HZN / 2; y++) blt_fill(m_pfb + (uint32_t)y * 2 * W, W, W, 2, (uint8_t)(M_SKY + y));
    blt_wait();
    disc8(m_pfb, 470, 150, 34, M_SUN);
    /* mountains: two ridges of sines */
    for (x = 0; x < W; x++) {
        int h1 = 40 + SIN(x * 2) / 6 + SIN(x * 5 + 40) / 9 + SIN(x * 11) / 20;
        int h2 = 22 + SIN(x * 3 + 100) / 8 + SIN(x * 7) / 14;
        int yy;
        for (yy = HZN - h1; yy < HZN; yy++) pset8(m_pfb, x, yy, M_MTN);
        for (yy = HZN - h2; yy < HZN; yy++) pset8(m_pfb, x, yy, M_MTN + 1);
        if (!(x & 31)) poll();
    }
    /* the floor: row y is at depth z = 6000 / (y - HZN + 8); a pixel's u = (x - 320) * z, v = z (fixed point) */
    for (y = HZN; y < H; y++) {
        int32_t z = 6000L / (int32_t)(y - HZN + 8);         /* depth */
        int32_t du = z * 8;                                  /* u per pixel, x 1/256 of an eighth */
        int32_t u = -320L * du + (1L << 30);                 /* (the offset keeps it positive) */
        uint8_t vs = (uint8_t)((z * 8 / 64) & 7);            /* the v eighth (a square is 64 units deep) */
        uint8_t fog = (uint8_t)(y < HZN + 24 ? 64 : 0);
        volatile uint8_t *p = (volatile uint8_t *)(m_pfb + (uint32_t)y * W);
        for (x = 0; x < W; x++) {
            uint8_t us = (uint8_t)((u >> 14) & 7);           /* the u eighth (a square is 512 units across) */
            p[x] = (uint8_t)(M_FLOOR + fog + (us << 3) + vs);
            u += du;
        }
        poll();
    }
}

/* the floor's palette: square colours by the eighths, moved by (pu, pv) */
static void m_floor_pal(uint8_t pu, uint8_t pv, uint16_t k)
{
    static uint16_t buf[128];
    static const uint16_t sq[2] = {RGB(170, 40, 190), RGB(40, 190, 215)};
    static const uint16_t haze = RGB(250, 150, 110);
    uint8_t f, us, vs;
    for (f = 0; f < 2; f++)
        for (us = 0; us < 8; us++)
            for (vs = 0; vs < 8; vs++) {
                uint8_t par = (uint8_t)((((us + pu) & 7) >> 2) ^ (((vs + pv) & 7) >> 2));
                uint16_t c = sq[par];
                if (f) c = mix565(c, haze, 150);
                buf[f * 64 + us * 8 + vs] = dim565(c, k);
            }
    pal_run(M_FLOOR, buf, 128);
}
static void m_sky_pal(uint16_t k)
{
    uint8_t i;
    IO(0x210) = M_SKY;
    for (i = 0; i < 100; i++) {
        uint16_t c = i < 60 ? mix565(RGB(10, 10, 60), RGB(150, 40, 140), (uint16_t)(i * 256 / 60))
                            : mix565(RGB(150, 40, 140), RGB(255, 160, 80), (uint16_t)((i - 60) * 256 / 40));
        IO(0x212) = dim565(c, k);
    }
    pal565(M_MTN, dim565(RGB(70, 30, 90), k));
    pal565(M_MTN + 1, dim565(RGB(40, 15, 55), k));
    pal565(M_SUN, dim565(RGB(255, 230, 120), k));
}

/* ---- the object: an icosahedron */
static const int16_t ico_v[12][3] = {
    {0, 85, 138}, {0, -85, 138}, {0, 85, -138}, {0, -85, -138},
    {85, 138, 0}, {-85, 138, 0}, {85, -138, 0}, {-85, -138, 0},
    {138, 0, 85}, {-138, 0, 85}, {138, 0, -85}, {-138, 0, -85}};
static const uint8_t ico_f[20][3] = {
    {0, 1, 8}, {0, 9, 1}, {0, 8, 4}, {0, 4, 5}, {0, 5, 9}, {1, 6, 8}, {1, 9, 7}, {1, 7, 6},
    {2, 3, 11}, {2, 10, 3}, {2, 4, 10}, {2, 5, 4}, {2, 11, 5}, {3, 6, 7}, {3, 10, 6}, {3, 7, 11},
    {4, 8, 10}, {5, 11, 9}, {6, 10, 8}, {7, 9, 11}};

/* a flat triangle, spans by edge stepping (16.16) */
static void m_tri(uint32_t buf, int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    int t, y;
    int32_t xa, xb, da, db;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y0 > y2) { t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y2 == y0) return;
    db = ((int32_t)(x2 - x0) << 16) / (y2 - y0);
    xb = (int32_t)x0 << 16;
    if (y1 > y0) {
        da = ((int32_t)(x1 - x0) << 16) / (y1 - y0);
        xa = (int32_t)x0 << 16;
        for (y = y0; y < y1; y++) {
            hspan8(buf, (int)(xa >> 16), (int)(xb >> 16), y, c);
            xa += da; xb += db;
        }
    }
    if (y2 > y1) {
        da = ((int32_t)(x2 - x1) << 16) / (y2 - y1);
        xa = (int32_t)x1 << 16;
        for (y = y1; y < y2; y++) {
            hspan8(buf, (int)(xa >> 16), (int)(xb >> 16), y, c);
            xa += da; xb += db;
        }
    }
    poll();
}

static void m_object(uint32_t buf, uint16_t f, int cx, int cy, int16_t *bb)
{
    int16_t px[12], py[12], rz[12];
    int32_t rx3[12], ry3[12];
    uint8_t ax = (uint8_t)(f * 2), ay = (uint8_t)(f * 3), i;
    int16_t sa = SIN16(ax), ca = COS16(ax), sb = SIN16(ay), cb = COS16(ay);
    bb[0] = 640; bb[1] = 360; bb[2] = 0; bb[3] = 0;
    for (i = 0; i < 12; i++) {
        int32_t x = ico_v[i][0], y = ico_v[i][1], z = ico_v[i][2];
        int32_t y1 = (y * ca - z * sa) >> 14, z1 = (y * sa + z * ca) >> 14;
        int32_t x2 = (x * cb + z1 * sb) >> 14, z2 = (-x * sb + z1 * cb) >> 14;
        int32_t d = 520 + z2;
        rx3[i] = x2; ry3[i] = y1; rz[i] = (int16_t)z2;
        px[i] = (int16_t)(cx + x2 * 420 / d);
        py[i] = (int16_t)(cy + y1 * 420 / d);
        if (px[i] < bb[0]) bb[0] = px[i];
        if (px[i] > bb[2]) bb[2] = px[i];
        if (py[i] < bb[1]) bb[1] = py[i];
        if (py[i] > bb[3]) bb[3] = py[i];
    }
    for (i = 0; i < 20; i++) {
        const uint8_t *fv = ico_f[i];
        int32_t ex = px[fv[1]] - px[fv[0]], ey = py[fv[1]] - py[fv[0]];
        int32_t gx = px[fv[2]] - px[fv[0]], gy = py[fv[2]] - py[fv[0]];
        if (ex * gy - ey * gx <= 0) continue;   /* facing away */
        {
            /* light from the upper left, towards the viewer: the face normal's direction from its centre */
            int32_t nx = rx3[fv[0]] + rx3[fv[1]] + rx3[fv[2]], ny = ry3[fv[0]] + ry3[fv[1]] + ry3[fv[2]];
            int32_t nz = rz[fv[0]] + rz[fv[1]] + rz[fv[2]];
            int32_t l = (-nx * 5 - ny * 7 - nz * 9) / 32;   /* about -200..200 */
            int sh = (int)((l + 210) * 16 / 420);
            if (sh < 0) sh = 0;
            if (sh > 15) sh = 15;
            m_tri(buf, px[fv[0]], py[fv[0]], px[fv[1]], py[fv[1]], px[fv[2]], py[fv[2]],
                  (uint8_t)((i & 1 ? M_OBJ + 16 : M_OBJ) + sh));
        }
    }
}

/* ---- the ball sprite: 32 x 32, shaded */
static void m_ball_img(void)
{
    int x, y;
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            int dx = x - 16, dy = y - 16, d2 = dx * dx + dy * dy, c = 0;
            if (d2 < 240) {
                int lx = x - 11, ly = y - 10, l2 = lx * lx + ly * ly;
                c = M_BALL + (l2 < 256 ? l2 / 17 : 15);
            }
            *(volatile uint8_t *)(M_BALLIMG + (uint32_t)y * 32 + x) = (uint8_t)c;
        }
}

static const char m_msg[] =
    "     A 68000 AT 7 MHZ, 4096 COLOURS, A COPPER AND A BLITTER, FOUR CHANNELS OF 8-BIT SAMPLES ... "
    "THE FLOOR IS ONLY THE PALETTE MOVING, THE SCROLLER IS THE BLITTER, THE BALLS ARE SPRITES.     ";

static void m_strip(void)
{
    int i, n = strlen16(m_msg);
    blt_fill(M_STRIP, MSTRIP_W, MSTRIP_W, 28, 0);
    blt_wait();
    for (i = 0; i < n && i * 16 + 16 <= MSTRIP_W; i++) {
        const uint8_t *g = font8x14[(uint8_t)m_msg[i]];
        int r, b;
        for (r = 0; r < 14; r++)
            for (b = 0; b < 8; b++)
                if ((g[r] >> (7 - b)) & 1) {
                    volatile uint8_t *p = (volatile uint8_t *)(M_STRIP + (uint32_t)(2 * r) * MSTRIP_W + (uint32_t)(i * 16 + 2 * b));
                    uint8_t c = (uint8_t)(M_SCR + (r >> 1));
                    p[0] = c; p[1] = c; p[MSTRIP_W] = c; p[MSTRIP_W + 1] = c;
                }
        if (!(i & 7)) poll();
    }
}

static void m_obj_pal(uint16_t k)
{
    uint8_t i;
    for (i = 0; i < 16; i++) {
        pal565((uint8_t)(M_OBJ + i), dim565(mix565(RGB(20, 10, 60), RGB(120, 200, 255), (uint16_t)(i * 17)), k));
        pal565((uint8_t)(M_OBJ + 16 + i), dim565(mix565(RGB(40, 10, 50), RGB(255, 150, 240), (uint16_t)(i * 17)), k));
        pal565((uint8_t)(M_BALL + i), dim565(mix565(RGB(255, 250, 220), RGB(160, 40, 20), (uint16_t)(i * 17)), k));
    }
}

static void sec_amiga(void)
{
    uint32_t t0;
    uint8_t cur = 0, i;
    uint32_t so = 0;
    screen_reset();
    m_pfb = R2; m_buf[0] = R0; m_buf[1] = R1;
    clear8(m_buf[0], 0);
    clear8(m_buf[1], 0);
    for (i = 0; i < 2; i++) { m_bb[i][0] = 0; m_bb[i][1] = 0; m_bb[i][2] = 0; m_bb[i][3] = 0; }
    m_background();
    m_strip();
    m_ball_img();
    for (i = 0; i < 8; i++) pal565((uint8_t)(M_SCR + i), hsv565((uint8_t)(i * 20), 160, 255));
    set_pfb(m_pfb);
    set_pfa(m_buf[0]);
    vid(VC_BMP | VC_HI | VC_TEXT);
    vid2(VC2_PFB | VC2_SPR);                    /* B behind, sprites on */
    tstr(26, 1, " AMIGA: 1985 - 16 BITS ", TA(7, 1) | TBLEND);
    for (i = 0; i < 12; i++) spr_set(i, 0, -32, 32, M_BALLIMG, 2, 0);
    mod_play(mod_amiga);
    t0 = ticks50;
    while ((uint32_t)(ticks50 - t0) < 2000) {
        uint32_t t = ticks50 - t0;
        uint16_t f = (uint16_t)(t * 6 / 5);     /* animation clock: 60 a second */
        uint16_t k = t < 50 ? (uint16_t)(t * 256 / 50) : t > 1950 ? (uint16_t)((2000 - t) * 256 / 50) : 256;
        uint32_t back = cur ? m_buf[0] : m_buf[1];
        int16_t *bb = cur ? m_bb[0] : m_bb[1];
        int cx = 320 + SIN(f / 2) * 150 / 128, cy = 150 + SIN(f * 3 / 2) * 30 / 128;
        /* clear what this buffer had: the object's box and the scroller's band */
        if (bb[2] > bb[0]) rect8(back, bb[0], bb[1], bb[2] - bb[0] + 2, bb[3] - bb[1] + 2, 0);
        rect8(back, 0, 280, W, 76, 0);
        blt_wait();
        m_object(back, f, cx, cy, bb);
        /* the sine scroller: 40 columns of 16 pixels, each at its own height */
        {
            uint32_t sw = MSTRIP_W - 640, o = so % sw;
            int g;
            for (g = 0; g < 40; g++) {
                int y = 300 + SIN(f * 4 + g * 5) * 20 / 128;
                blt_copy(M_STRIP + o + (uint32_t)g * 16, MSTRIP_W, back + (uint32_t)y * W + (uint32_t)g * 16, W, 16, 28, 1, 0);
            }
            so += 3;
        }
        /* the balls orbit the object */
        for (i = 0; i < 12; i++) {
            uint8_t a = (uint8_t)(f * 2 + i * 21);
            spr_xy(i, cx - 16 + COS(a) * 200 / 128, cy - 16 + SIN(a) * 70 / 128 + SIN(f * 3 + i * 40) / 12, 32);
        }
        /* palettes: the floor moves, the scroller's colours cycle, fades */
        m_floor_pal((uint8_t)(SIN(f) / 40), (uint8_t)(f / 2), k);
        if (t < 60 || t > 1940) { m_sky_pal(k); m_obj_pal(k); }
        for (i = 0; i < 8; i++) pal565((uint8_t)(M_SCR + i), dim565(hsv565((uint8_t)(i * 20 + f * 3), 170, 255), k));
        blt_wait();
        set_pfa(back);
        cur ^= 1;
        wait_flip();
        fps_count('A');
    }
    mod_stop();
    spr_alloff();
    tclear();
}
