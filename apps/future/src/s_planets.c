/* Phrases 27-32: the planets, filmed. 640 x 360, letterboxed, 16 voices. Each planet is a texture wrapped on
 * a sphere (assets.h: per pixel of a radius-72 disc its longitude and light level), turning, lit from the
 * side: 8 tones x 16 light levels (dithered) in palette 128-255, drawn at half resolution with the rows doubled by the
 * blitter. Playfield A holds the planet and moves by its scroll register (the camera pans at 60 frames a
 * second whatever the planet's own rate); playfield B the stars, panning slower; moons and asteroids are
 * hardware sprites; titles on the text layer. Phrase 28 is the asteroid belt (a 3D rock tumbling past),
 * phrase 32 the edge of the solar system, where the starship passes. */

#define P_BLACK 1
#define P_STAR 2                                /* 2-5 */
#define P_RING 8                                /* 8-23: Saturn's rings */
#define P_ROCK 32                               /* 32-47: the 3D asteroid's ramp */
#define P_SHIP 48                               /* 48-111: the starship's ramps (phrase 32) */
#define P_PLANET 128
#define P_RINGFRONT (0x0F1200UL)                /* the rings' front half over the planet: 296 x 80 */
#define P_SPR (0x0F7000UL)                      /* sprite images: moons 32 x 32 (two), rocks 16 x 16 (four) */
#define P_CX 320
#define P_CY 180

static const uint8_t *pl_tex;
static uint16_t sph_row[2 * SPH_R];              /* where each row's entries start */
static const uint8_t p_lvl[16] = {0, 10, 20, 32, 46, 62, 80, 100, 120, 142, 164, 186, 208, 228, 245, 255};

static void planet_pal(const uint16_t *tones, uint16_t k)
{
    uint8_t s, t;
    IO(0x210) = P_PLANET;
    for (s = 0; s < 16; s++)
        for (t = 0; t < 8; t++) {
            uint16_t c = dim565(tones[t], (uint16_t)((uint32_t)p_lvl[s] * k >> 8));
            if (s == 1) c = mix565(c, RGB(10, 14, 40), 96);              /* the night side, a little blue */
            IO(0x212) = c;
        }
}
/* the planet into buf with its centre at (P_CX, P_CY), turned by rot (0-127 texels) */
static void planet_draw(uint32_t buf, uint8_t rot)
{
    const uint16_t *e = sph_e;
    const uint8_t *tex = pl_tex;
    uint8_t j;
    for (j = 0; j < 2 * SPH_R; j++) {
        uint8_t w = sph_w[j];
        const uint8_t *tr = tex + ((uint16_t)sph_v[j] << 7);
        volatile uint16_t *d = (volatile uint16_t *)(buf + (uint32_t)(P_CY - 2 * SPH_R + 2 * j) * W + (uint32_t)(P_CX - 2 * w));
        uint8_t n = w;                          /* 2w texels, two at a time */
        while (n--) {
            uint16_t a = e[0], b = e[1];
            uint8_t c0 = (uint8_t)((a >> 8) | tr[(uint8_t)(a + rot) & 127]);
            uint8_t c1 = (uint8_t)((b >> 8) | tr[(uint8_t)(b + rot) & 127]);
            d[0] = (uint16_t)(c0 | c0 << 8);
            d[1] = (uint16_t)(c1 | c1 << 8);
            d += 2; e += 2;
        }
        if (!(j & 15)) poll();
    }
    /* the odd rows: each even row copied down, one blit */
    blt_copy(buf + (uint32_t)(P_CY - 2 * SPH_R) * W + (P_CX - 2 * SPH_R), 2 * W,
             buf + (uint32_t)(P_CY - 2 * SPH_R + 1) * W + (P_CX - 2 * SPH_R), 2 * W, 4 * SPH_R, 2 * SPH_R, 0, 0);
}
/* a small sphere sprite (s x s) from the table, with a texture and tone offset */
static void sphere_sprite(uint32_t dst, uint8_t s, const uint8_t *tex, uint8_t dark)
{
    uint8_t x, y;
    for (y = 0; y < s; y++)
        for (x = 0; x < s; x++) {
            int16_t j = (int16_t)(((uint16_t)y * 2 + 1) * SPH_R / s), dx = (int16_t)((int16_t)(((uint16_t)x * 2 + 1) * SPH_R / s) - SPH_R);
            uint8_t w = sph_w[j], c = 0;
            if (dx >= -(int16_t)w && dx < (int16_t)w) {
                uint16_t e = sph_e[sph_row[j] + (uint16_t)(dx + w)];
                uint8_t t = tex[((uint16_t)sph_v[j] << 7) | ((uint8_t)(e + x * 3) & 127)];
                c = (uint8_t)((e >> 8) | (t > dark ? t - dark : 0));
            }
            *(volatile uint8_t *)(dst + (uint32_t)y * s + x) = c;
        }
}
/* Saturn's rings, into buf: bands of an ellipse annulus (tilted 1:4); the front half saved for redrawing */
static void rings(uint32_t buf)
{
    static const uint8_t band[12] = {300 / 4, 290 / 4, 278 / 4, 262 / 4, 250 / 4, 246 / 4, 236 / 4, 222 / 4, 208 / 4, 196 / 4, 186 / 4, 170 / 4};
    uint8_t i;
    cv_screen(buf, 1);
    for (i = 0; i < 12; i++)
        cv_ellipse(P_CX, P_CY, band[i] * 4, band[i], (uint8_t)(i == 4 ? 0 : P_RING + i));
    cv_ellipse(P_CX, P_CY, 170, 42, 0);
}
static void ring_save(uint32_t buf)             /* rows below the centre, over the planet's width */
{
    blt_copy(buf + (uint32_t)P_CY * W + (P_CX - 148), W, P_RINGFRONT, 296, 296, 78, 0, 0);
    blt_wait();
}
static void ring_front(uint32_t buf)
{
    blt_copy(P_RINGFRONT, 296, buf + (uint32_t)P_CY * W + (P_CX - 148), W, 296, 78, 1, 0);
}

/* the asteroid: an icosahedron with its vertices pushed in and out */
static const int8_t ico8[12][3] = {{0, 50, 81}, {0, -50, 81}, {0, 50, -81}, {0, -50, -81}, {50, 81, 0}, {-50, 81, 0},
                                   {50, -81, 0}, {-50, -81, 0}, {81, 0, 50}, {-81, 0, 50}, {81, 0, -50}, {-81, 0, -50}};
static const uint8_t icof[20][3] = {{0, 1, 8}, {0, 9, 1}, {0, 4, 5}, {0, 8, 4}, {0, 5, 9}, {1, 7, 6}, {1, 6, 8}, {1, 9, 7},
                                    {2, 3, 11}, {2, 10, 3}, {2, 5, 4}, {2, 4, 10}, {2, 11, 5}, {3, 6, 7}, {3, 10, 6}, {3, 7, 11},
                                    {4, 8, 10}, {5, 11, 9}, {6, 10, 8}, {7, 9, 11}};
static void rock(uint32_t buf, uint8_t ax, uint8_t ay, int cx, int cy, int size)
{
    int16_t px[12], py[12];
    int32_t rx[12], ry[12], rz[12];
    uint8_t i;
    int16_t sa = SIN16(ax), ca = COS16(ax), sb = SIN16(ay), cb = COS16(ay);
    cv_screen(buf, 1);
    for (i = 0; i < 12; i++) {
        int32_t s = (int32_t)size * (90 + (hash8(i) & 31)) / 100;
        int32_t x = ico8[i][0] * s / 96, y = ico8[i][1] * s / 96, z = ico8[i][2] * s / 96;
        int32_t y1 = (y * ca - z * sa) >> 14, z1 = (y * sa + z * ca) >> 14;
        int32_t x2 = (x * cb + z1 * sb) >> 14, z2 = (-x * sb + z1 * cb) >> 14;
        rx[i] = x2; ry[i] = y1; rz[i] = z2;
        px[i] = (int16_t)(cx + x2); py[i] = (int16_t)(cy + y1);
    }
    for (i = 0; i < 20; i++) {
        const uint8_t *f = icof[i];
        int32_t ex = px[f[1]] - px[f[0]], ey = py[f[1]] - py[f[0]], gx = px[f[2]] - px[f[0]], gy = py[f[2]] - py[f[0]];
        if (ex * gy - ey * gx <= 0) continue;
        {
            int32_t nx = rx[f[0]] + rx[f[1]] + rx[f[2]], ny = ry[f[0]] + ry[f[1]] + ry[f[2]], nz = rz[f[0]] + rz[f[1]] + rz[f[2]];
            uint8_t sh = g_shade(nx, -ny, -nz);
            cv_tri(px[f[0]], py[f[0]], px[f[1]], py[f[1]], px[f[2]], py[f[2]], (uint8_t)(P_ROCK + sh));
        }
    }
}

static void letterbox(uint32_t buf)
{
    blt_fill(buf, W, W, 36, P_BLACK);
    blt_fill(buf + 324UL * W, W, W, 36, P_BLACK);
}
static void title(const char *a, const char *b, uint8_t on)
{
    uint8_t r;
    for (r = 18; r <= 20; r++) tstr(0, r, "                                                                                ", TCLEAR);
    if (!on) return;
    {
        char sp[40];
        int i = 0, n = 0;
        while (a[n] && i < 38) { sp[i++] = a[n++]; sp[i++] = ' '; }
        sp[i ? i - 1 : 0] = 0;
        tstr(40 - (i - 1) / 2, 18, sp, TA(7, 0) | TCLEAR);
        tstr(40 - strlen16(b) / 2, 20, b, TA(6, 0) | TCLEAR);
    }
}

typedef struct { const uint8_t *tex; const uint16_t *pal; const char *name, *sub; } planet_t;

static void sc_planets(uint8_t pend)
{
    uint16_t i;
    uint8_t cur = 0xFF, drawn[2] = {0xFF, 0xFF};
    static const planet_t pls[4] = {
        {tex_mars, pal_mars, "MARS", "2071  THE FIRST CITY"},
        {tex_jupiter, pal_jupiter, "JUPITER", "2094  THE MOONS"},
        {tex_saturn, pal_saturn, "SATURN", "2110  THE RINGS"},
        {tex_neptune, pal_neptune, "NEPTUNE", "2131  THE LAST WORLD"}};
    static const int8_t which[6] = {0, -1, 1, 2, 3, -2};             /* phrase 27.. -> planet, belt, edge */
    screen_reset();
    for (i = 0, cur = 0; cur < 2 * SPH_R; cur++) { sph_row[cur] = i; i = (uint16_t)(i + 2 * sph_w[cur]); }
    cur = 0xFF;
    /* the stars on B */
    blt_fill(R2, W, W, H, 0);
    blt_wait();
    for (i = 0; i < 420; i++) {
        uint16_t y = (uint16_t)(36 + rnd() % 288), x = (uint16_t)(rnd() % 640);
        *(volatile uint8_t *)(R2 + (uint32_t)y * W + x) = (uint8_t)(P_STAR + (rnd() & 3));
    }
    pal565(P_STAR, RGB(90, 90, 110)); pal565(P_STAR + 1, RGB(150, 150, 170));
    pal565(P_STAR + 2, RGB(210, 210, 230)); pal565(P_STAR + 3, RGB(255, 245, 220));
    pal565(P_BLACK, 0);
    for (i = 0; i < 12; i++) pal565((uint8_t)(P_RING + i), dim565(pal_saturn[2 + (i * 5) % 6], (uint16_t)(150 + (i * 41) % 100)));
    g_ramp(P_ROCK, RGB(130, 115, 100), 256);
    sphere_sprite(P_SPR, 32, tex_mars, 2);                          /* Phobos-like: Mars's darker tones */
    sphere_sprite(P_SPR + 1024, 32, tex_jupiter, 0);                /* a moon of Jupiter */
    for (i = 0; i < 4; i++) sphere_sprite(P_SPR + 2048 + i * 256, 16, tex_mars, (uint8_t)(2 + (i >> 1)));
    set_pfb(R2);
    vid(VC_BMP | VC_HI | VC_TEXT);
    vid2(VC2_PFB | VC2_SPR);
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127);
        uint32_t u = T & 0x7FFF;
        int8_t w;
        uint32_t buf = fb_back();
        uint16_t k = 256;
        if (p >= pend || song_done) break;
        w = which[p - 27];
        /* fades at the joins: in over half a bar, out over the last half bar of each phrase */
        if (u < 2048) k = (uint16_t)(u >> 3);
        if (u > 32768 - 2048) k = (uint16_t)((32768 - u) >> 3);
        if (p == 27 && u < 4096) k = (uint16_t)(u >> 4);
        if (w >= 0 && cur != (uint8_t)w) {     /* a new planet: its texture, palette and (Saturn) rings */
            cur = (uint8_t)w;
            pl_tex = pls[w].tex;
            drawn[0] = drawn[1] = 0xFF;
        }
        if (w >= 0) planet_pal(pls[w].pal, k);
        else g_ramp(P_ROCK, RGB(130, 115, 100), k);
        {
            uint16_t sk = (uint16_t)k;
            pal565(P_STAR, dim565(RGB(90, 90, 110), sk)); pal565(P_STAR + 1, dim565(RGB(150, 150, 170), sk));
            pal565(P_STAR + 2, dim565(RGB(210, 210, 230), sk)); pal565(P_STAR + 3, dim565(RGB(255, 245, 220), sk));
        }
        /* the camera: the planet crosses from right of centre to left over the phrase */
        {
            int px = 470 - (int)(u >> 7) * 300 / 256;               /* where the planet's centre is */
            if (w == 2) px = 334 - (int)(u >> 7) * 28 / 256;           /* Saturn: slow, its rings inside the frame */
            scroll_a((uint16_t)((P_CX - px + 640) % 640), 0);
            scroll_b((uint16_t)((u >> 9) % 640), 0);
        }
        spr_alloff();
        if (w >= 0) {
            uint8_t bi = fb_cur;
            if (drawn[bi] != cur) {             /* this buffer: clear, letterbox, rings once */
                clear8(buf, 0);
                letterbox(buf);
                if (w == 2) { rings(buf); if (bi == 0) ring_save(buf); }
                blt_wait();
                drawn[bi] = cur;
            }
            planet_draw(buf, (uint8_t)((T >> 10) & 127));
            if (w == 2) ring_front(buf);
            /* moons */
            if (w == 0 || w == 1) {
                uint8_t a = (uint8_t)(T >> 8);
                int mx = 470 - (int)(u >> 7) * 300 / 256 + (COS(a) * 230 >> 7), my = 180 + (SIN(a) * 40 >> 7);
                spr_set(0, mx - 16, my - 16, 32, P_SPR + (w ? 1024 : 0), 2, 0);
                if (w == 1) {
                    uint8_t b2 = (uint8_t)(a * 2 + 90);
                    spr_set(1, mx - (COS(a) * 230 >> 7) + (COS(b2) * 180 >> 7) - 8, 180 + (SIN(b2) * 30 >> 7) - 8, 16, P_SPR + 2048, 1, 0);
                }
            }
            title(pls[w].name, pls[w].sub, (uint8_t)(n >= 8 && n < 56));
        } else if (w == -1) {
            /* the belt: rocks streaming past, one big one tumbling through */
            uint8_t j;
            clear8(buf, 0);
            letterbox(buf);
            scroll_a(0, 0);
            rock(buf, (uint8_t)(T >> 7), (uint8_t)(T >> 8), 700 - (int)(u >> 6), 180 + (int)((u >> 8) % 40), 60 + (int)(u >> 10));
            for (j = 0; j < 24; j++) {
                int sx = (int)((uint16_t)(j * 97 + 640 + 640 - ((u >> 4) * (1 + (j & 3))) % 1280) % 1280) - 320;
                spr_set(j, sx, 48 + (j * 53) % 260, 16, P_SPR + 2048 + (j & 3) * 256, 1, (uint8_t)(j & 1));
            }
            title("ASTEROIDS", "2088  THE MINES", (uint8_t)(n >= 16 && n < 64));
        } else {
            /* the edge: the Sun a bright star behind, the starship passing */
            clear8(buf, 0);
            letterbox(buf);
            scroll_a(0, 0);
            title("THE EDGE", "2140  BEYOND THE SUN", (uint8_t)(n >= 24 && n < 80));
            cv_screen(buf, 1);
            wdisc(560, 120, 3, P_STAR + 3);
            wdisc(560, 120, 1, P_STAR + 2);
        }
        fb_flip();
    }
    title("", "", 0);
    spr_alloff();
    scroll_a(0, 0); scroll_b(0, 0);
    vid2(0);
}
