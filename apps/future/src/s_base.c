/* Phrases 22-26: the Moon base, as an isometric diorama in 256 colours (the way the early-90s PC games
 * drew their cities). The 3D engine renders it once with an orthographic camera: a slab of ground with
 * craters and grain, domes, tubes, a hangar, a landing pad, a solar farm, a mast, a mass driver. Phrases
 * 22-23 show it at 320 x 180; meanwhile the 640 x 360 picture is drawn a few depth buckets a frame into
 * the other buffer and shown from phrase 24. Everything that moves is a hardware sprite (the rover,
 * pre-rendered in four directions; a lander; the capsule and its trail) or the palette (night in phrase
 * 25, the windows, the beacon), so the picture runs at the full frame rate. */

#define B_REG 16
#define B_HAB 32
#define B_GLASS 48
#define B_WIN 64
#define B_GOLD 80
#define B_SOLAR 96
#define B_ROVER 112
#define B_METAL 128
#define B_PAD 144
#define B_GREEN 160
#define B_CRATER 176
#define B_EARTH 208                             /* 208-211 */
#define B_STAR 216
#define B_BEACON 220                            /* 220-235 */
#define B_FLASH 240                             /* 240-247: the capsule, the flames */
#define B_DRAW (0x0F1200UL)                     /* scratch canvas, stride 640, 48 rows */
#define B_SPR (0x0F9000UL)                      /* rover 4 directions x 2 resolutions x 32 x 32, the glow */

static uint8_t b_phase;                         /* 0 low res shown, hi being drawn; 1 hi drawn, grain; 2 done */
static uint16_t b_grain_y;

static void g_beam(int x0, int y0, int x1, int y1, int z, int w, int h, uint8_t ramp)  /* a slanted bar */
{
    uint16_t b = g_nv;
    g_vert(x0, y0, z - w); g_vert(x1, y1, z - w); g_vert(x1, y1 + h, z - w); g_vert(x0, y0 + h, z - w);
    g_vert(x0, y0, z + w); g_vert(x1, y1, z + w); g_vert(x1, y1 + h, z + w); g_vert(x0, y0 + h, z + w);
    g_face(b + 0, b + 1, b + 2, b + 3, ramp, 0, 0, -1);
    g_face(b + 5, b + 4, b + 7, b + 6, ramp, 0, 0, 1);
    g_face(b + 3, b + 2, b + 6, b + 7, ramp, (int32_t)(y0 - y1), (int32_t)(x1 - x0), 0);
    g_face(b + 1, b + 5, b + 6, b + 2, ramp, 1, 0, 0);
}
static const int16_t b_crat[8][3] = {{-1500, 700, 260}, {1900, 1000, 200}, {-600, -1800, 300}, {2300, -1500, 180},
                                     {-1900, -900, 150}, {600, 1100, 130}, {1300, -2000, 240}, {-2100, 1100, 120}};
static void base_build(uint8_t hi)
{
    uint8_t s = hi ? 16 : 10, r = hi ? 5 : 3, i;
    g_reset();
    g_box(-2500, -260, -2500, 2900, 0, 1500, B_REG);               /* the ground: a slab */
    for (i = 0; i < 8; i++) {                   /* craters: a lit rim, a dark bowl */
        g_disc(b_crat[i][0], 2, b_crat[i][1], b_crat[i][2] + 40, (uint8_t)(hi ? 12 : 8), B_REG + 2);
        g_disc(b_crat[i][0] - b_crat[i][2] / 6, 4, b_crat[i][1] - b_crat[i][2] / 6, b_crat[i][2], (uint8_t)(hi ? 12 : 8), B_CRATER);
    }
    g_dome(0, 0, 520, s, r, B_HAB, B_WIN);
    g_dome(-1050, 0, 340, (uint8_t)(s - 2), (uint8_t)(r - 1), B_HAB, B_WIN);
    g_dome(900, 0, 290, (uint8_t)(s - 2), (uint8_t)(r - 1), B_GREEN, B_GREEN);
    g_tube(-740, -400, 110, 0, 100, (uint8_t)(hi ? 12 : 8), B_HAB);
    g_tube(440, 680, 100, 0, 90, (uint8_t)(hi ? 12 : 8), B_GOLD);
    g_box(-320, 0, -1000, 320, 240, -660, B_METAL);                /* the hangar and its lit door */
    g_box(-140, 0, -1012, 140, 170, -1000, B_WIN);
    g_disc(-1300, 6, -1500, 420, (uint8_t)(hi ? 20 : 12), B_PAD);  /* the landing pad */
    g_disc(-1300, 8, -1500, 220, (uint8_t)(hi ? 20 : 12), B_ROVER);
    g_disc(-1300, 10, -1500, 160, (uint8_t)(hi ? 20 : 12), B_PAD);
    for (i = 0; i < 4; i++) {                   /* the solar farm */
        int x = 1100 + (i & 1) * 520, z = -700 - (i >> 1) * 480;
        g_box(x + 180, 0, z + 140, x + 210, 180, z + 170, B_METAL);
        g_box(x, 180, z, x + 380, 205, z + 300, B_SOLAR);
    }
    g_box(-2050, 0, 300, -2010, 1000, 340, B_METAL);               /* the mast and its beacon */
    g_box(-2065, 1000, 285, -1995, 1060, 355, B_BEACON);
    g_beam(1300, 0, 2800, 900, 900, 50, 70, B_METAL);              /* the mass driver */
    for (i = 0; i < 6; i++) {
        int x = 1450 + i * 240, y = (int)((int32_t)(x - 1300) * 900 / 1500);
        g_box(x, y - 30, 830, x + 50, y + 120, 970, B_GOLD);
        if (i & 1) g_box(x, 0, 880, x + 30, y - 30, 920, B_METAL);
    }
}
static void base_camera(void)                   /* isometric: 45 degrees round, 27 down, from far off */
{
    g_ortho = 23; g_mx = 320; g_my = 168; g_zfar = 16000;
    g_camera(-5600, 5000, -5600, 32, 237);
}

/* the rover: four directions, rendered once into sprites (a copy at each resolution) */
static void rover_sprites(void)
{
    uint8_t d, hi;
    for (hi = 0; hi < 2; hi++)
        for (d = 0; d < 4; d++) {
            uint8_t y, k;
            uint8_t a = (uint8_t)(d * 64);
            int16_t s = SIN16(a), c = COS16(a);
            static const int16_t bx[5][6] = {{-110, 40, -60, 110, 120, 60}, {-100, 0, -85, -40, 55, -55}, {40, 0, -85, 100, 55, -55},
                                             {-100, 0, 55, -40, 55, 85}, {40, 0, 55, 100, 55, 85}};
            g_reset();
            for (k = 0; k < 5; k++) {           /* each box turned by a (its corners re-ordered) */
                int32_t x0 = bx[k][0], z0 = bx[k][2], x1 = bx[k][3], z1 = bx[k][5];
                int32_t ax = (x0 * c - z0 * s) >> 14, az = (x0 * s + z0 * c) >> 14, cx2 = (x1 * c - z1 * s) >> 14, cz2 = (x1 * s + z1 * c) >> 14;
                g_box((int)(ax < cx2 ? ax : cx2), bx[k][1], (int)(az < cz2 ? az : cz2), (int)(ax < cx2 ? cx2 : ax), bx[k][4], (int)(az < cz2 ? cz2 : az),
                      k ? B_METAL : B_ROVER);
            }
            g_box(-20, 120, -20, 20, 200, 20, B_METAL);                 /* the antenna */
            base_camera();
            g_mx = 32; g_my = 34;
            g_transform(0);
            blt_fill(B_DRAW, W, W, 48, 0);
            cv_set(B_DRAW, (uint8_t)(hi ? 1 : 2), 0, 0, hi ? 640 : 320, hi ? 48 : 24);
            g_draw(0);
            blt_wait();
            for (y = 0; y < 32; y++) {          /* the canvas's middle -> a 32 x 32 sprite */
                uint8_t x;
                for (x = 0; x < 32; x += 2)
                    *(volatile uint16_t *)(B_SPR + (uint32_t)(hi * 4 + d) * 1024 + y * 32 + x) =
                        *(volatile uint16_t *)(B_DRAW + (uint32_t)(y + 12) * W + 16 + x);
            }
        }
    for (d = 0; d < 16; d++) {                  /* a glow, 16 x 16: the capsule, the flames */
        uint8_t x;
        for (x = 0; x < 16; x++) {
            int dx = x - 8, dy = d - 8, q = dx * dx + dy * dy;
            *(volatile uint8_t *)(B_SPR + 8192 + d * 16 + x) = (uint8_t)(q < 64 ? B_FLASH + 7 - q / 10 : 0);
        }
    }
}
static void iso_at(int32_t x, int32_t y, int32_t z, int16_t *sx, int16_t *sy)   /* a world point's screen place */
{
    int32_t zz;
    base_camera();
    g_project(x - g_cx, y - g_cy, z - g_cz, sx, sy, &zz);
}
/* the grain: ground pixels a shade up or down at random (n rows of buf from y0) */
static void grain(uint32_t buf, uint16_t y0, uint16_t n)
{
    uint16_t y, x;
    for (y = y0; y < y0 + n && y < 360; y++) {
        volatile uint8_t *p = (volatile uint8_t *)(buf + (uint32_t)y * W);
        for (x = 0; x < 640; x++) {
            uint8_t c = p[x];
            if (c >= B_REG && c < B_REG + 16) {
                uint8_t r = (uint8_t)(rnd() & 7);
                if (r == 0 && c > B_REG) p[x] = (uint8_t)(c - 1);
                else if (r == 1 && c < B_REG + 15) p[x] = (uint8_t)(c + 1);
            }
        }
        poll();
    }
}
static void starfield(uint32_t buf)
{
    uint16_t i;
    for (i = 0; i < 140; i++) *(volatile uint8_t *)(buf + (uint32_t)(rnd() % 360) * W + rnd() % 640) = B_STAR;
    cv_screen(buf, 1);
    cv_disc(560, 60, 30, B_EARTH);
    cv_disc(566, 55, 24, B_EARTH + 1);
    cv_ellipse(560, 64, 10, 6, B_EARTH + 3);
    cv_ellipse(570, 46, 8, 3, B_EARTH + 2);
}

static void base_pal(uint32_t T)
{
    uint8_t p = (uint8_t)(T >> 15), i;
    uint16_t in = ramp16(T, PHRASE(22), 2048), out = 256 - ramp16(T, PHRASE(26) + 7 * 4096, 3500);
    uint16_t night = p == 25 ? ramp16(T, PHRASE(25), 3 * 4096) : (p == 26 ? 256 - ramp16(T, PHRASE(26), 4 * 4096) : 0);
    uint16_t day = (uint16_t)(256 - night * 3 / 4), k = (uint16_t)((uint32_t)in * out >> 8), kd = (uint16_t)((uint32_t)k * day >> 8);
    g_ramp(B_REG, RGB(160, 152, 140), kd);
    g_ramp(B_CRATER, RGB(110, 104, 96), kd);
    g_ramp(B_HAB, RGB(228, 228, 234), kd);
    g_ramp(B_WIN, RGB(200, 170, 110), kd);
    if (night) for (i = 0; i < 16; i++) pal565((uint8_t)(B_WIN + i), dim565(mix565(RGB((uint16_t)(60 + i * 8), (uint16_t)(50 + i * 7), 40), RGB(255, 220, 120), night), k));
    g_ramp(B_GOLD, RGB(225, 175, 55), kd);
    g_ramp(B_SOLAR, RGB(50, 85, 200), kd);
    g_ramp(B_ROVER, RGB(240, 120, 40), kd);
    g_ramp(B_METAL, RGB(135, 140, 150), kd);
    g_ramp(B_PAD, RGB(195, 195, 190), kd);
    g_ramp(B_GREEN, mix565(RGB(90, 200, 120), RGB(130, 255, 150), night), k);
    pal565(B_EARTH, dim565(RGB(20, 60, 150), k));
    pal565(B_EARTH + 1, dim565(RGB(60, 130, 220), k));
    pal565(B_EARTH + 2, dim565(RGB(230, 240, 255), k));
    pal565(B_EARTH + 3, dim565(RGB(70, 150, 80), k));
    pal565(B_STAR, dim565(RGB(255, 255, 255), k));
    for (i = 0; i < 16; i++) pal565((uint8_t)(B_BEACON + i), (T >> 11) & 1 ? dim565(RGB(255, 30, 20), k) : RGB(50, 0, 0));
    for (i = 0; i < 8; i++) pal565((uint8_t)(B_FLASH + i), dim565(mix565(RGB(255, 140, 40), RGB(255, 255, 230), (uint16_t)(i * 36)), k));
}

static void sc_base(uint8_t pend)
{
    screen_reset();
    base_pal(PHRASE(22));
    rover_sprites();
    /* the low-resolution picture at once; the high one a little a frame in the other buffer */
    clear8(R0, 0); clear8(R1, 0);
    blt_wait();
    starfield(R0); starfield(R1);
    base_build(0);
    base_camera();
    g_transform(0);
    cv_screen(R0, 2);
    g_draw(0);
    blt_wait();
    grain(R0, 0, 360);
    base_build(1);
    base_camera();
    g_transform(0);
    g_sort(0);
    b_phase = 0; b_grain_y = 0;
    set_pfa(R0);
    vid(VC_BMP | VC_HI);
    vid2(VC2_SPR);
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127);
        int16_t sx, sy;
        if (p >= pend || song_done) break;
        base_pal(T);
        if (b_phase == 0) { cv_screen(R1, 1); if (g_paint(2)) b_phase = 1; }
        else if (b_phase == 1) { grain(R1, b_grain_y, 12); b_grain_y += 12; if (b_grain_y >= 360) b_phase = 2; }
        if (p >= 24 && b_phase < 2) {
            cv_screen(R1, 1);
            while (!g_paint(8)) ;
            if (b_grain_y < 360) grain(R1, b_grain_y, (uint16_t)(360 - b_grain_y));
            b_phase = 2;
        }
        set_pfa(p >= 24 ? R1 : R0);
        spr_alloff();
        {                                       /* the rover: round the domes, a side a bar */
            uint8_t hi = p >= 24;
            uint32_t q = (T >> 4) % 4096;
            uint8_t side = (uint8_t)(q >> 10);
            int32_t f = (int32_t)(q & 1023), x, z;
            static const int16_t cx[5] = {-1500, 1300, 1300, -1500, -1500}, cz[5] = {-450, -450, 600, 600, -450};
            static const uint8_t dir[4] = {0, 3, 2, 1};
            x = cx[side] + (cx[side + 1] - cx[side]) * f / 1024;
            z = cz[side] + (cz[side + 1] - cz[side]) * f / 1024;
            iso_at(x, 0, z, &sx, &sy);
            spr_set(0, sx - 16, sy - 22, 32, B_SPR + (uint32_t)(hi * 4 + dir[side]) * 1024, 2, 0);
        }
        if (p == 23) {                          /* a lander coming down on the pad on its flame */
            int32_t h = n < 96 ? (int32_t)(96 - n) * 30 : 0;
            iso_at(-1300, 30 + h, -1500, &sx, &sy);
            spr_set(1, sx - 16, sy - 30, 32, B_SPR + 1024, 2, 0);
            if (n < 100) spr_set(2, sx - 8, sy - 4 + (int)(rnd() & 3), 16, B_SPR + 8192, 1, 0);
        }
        if (p == 26 && n >= 32) {               /* the launch: the capsule up the rail and away, a trail */
            uint8_t j;
            for (j = 0; j < 8; j++) {
                int32_t qq = (int32_t)(n - 32) - j * 3, x, y;
                if (qq < 0) break;
                x = 1300 + qq * 50; y = (x - 1300) * 900 / 1500;
                if (x > 2800) y = 900 + (x - 2800) * 3 / 2;
                iso_at(x, y + 60, 900, &sx, &sy);
                spr_set((uint8_t)(3 + j), sx - 8, sy - 8, 16, B_SPR + 8192, 1, 0);
            }
        }
        cv_screen(p >= 24 ? R1 : R0, 1);
        wtext35(10, 8, p >= 24 ? "256 COLOURS  640X360  8 CHANNELS" : "256 COLOURS  320X180  8 CHANNELS", B_STAR, 4);
        vsync();
    }
    spr_alloff();
    vid2(0);
    g_ortho = 0;
}
