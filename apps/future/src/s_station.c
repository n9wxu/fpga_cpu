/* Phrases 16-21: the space station, as a 16-bit computer would draw it. 320 x 180, 16 colours and then 32,
 * four sample channels. The Earth's limb with a glowing atmosphere and drifting clouds; the stars on a
 * second playfield in three layers moving at three speeds (line scroll: each row its own offset); the
 * station assembled module by module; hardware sprites for the astronaut, the sun's flare and the shuttle,
 * drawn with doubled pixels to match the bitmap. */

#define S_BLACK 1
#define S_GREY 2                                /* 2-6: the station's greys, dark to light */
#define S_GOLD 7                                /* 7-10: foil */
#define S_PANEL 11                              /* 11-14: solar panels */
#define S_SEA 15                                /* 15-20: the Earth, dark to light */
#define S_CLOUD 21                              /* 21-23 */
#define S_AIR 24                                /* 24-26: the atmosphere's glow */
#define S_STAR 27                               /* 27-29: the three star layers */
#define S_TILE 30
#define S_RED 31
#define S_FLARE 32                              /* 32-35 */
#define S_DRAW (0x0F1200UL)                     /* scratch canvas (stride 640, 32 rows) for the sprite art */
#define S_SPR (0x0F6200UL)                      /* sprite images: shuttle 4 x 32 x 32, astronaut, flares */
#define S_SHUT S_SPR
#define S_ASTRO (S_SPR + 0x1000UL)              /* 16 x 24 */
#define S_FLR (S_SPR + 0x1200UL)                /* 3 x 32 x 32 */

static int16_t st_x, st_y, st_k;                /* the station's centre and scale (k/64) */
static void srect(int x, int y, int w, int h, uint8_t c)
{
    wrect(st_x + x * st_k / 64, st_y + y * st_k / 64, w * st_k / 64, h * st_k / 64, c);
}
/* a cylinder lying along x: a mid tone with a light top band and a dark bottom one */
static void module(int x, int y, int w, int h, uint8_t ramp)
{
    srect(x, y, w, h, (uint8_t)(ramp + 2));
    srect(x, y, w, h / 4, (uint8_t)(ramp + 3));
    srect(x, y + h * 3 / 4, w, h / 4, (uint8_t)(ramp + 1));
    srect(x - 2, y + 2, 2, h - 4, ramp);        /* the end rings */
    srect(x + w, y + 2, 2, h - 4, ramp);
}
static void panel(int x, int y, int w, int h)   /* a solar wing, h may be 0 (folded) */
{
    int i;
    if (h <= 2) return;
    srect(x, y, w, h, S_PANEL + 1);
    for (i = 0; i < 4; i++) srect(x + i * w / 4, y, 1, h, S_PANEL);
    for (i = 1; i * 12 < h; i++) srect(x, y + i * 12, w, 1, S_PANEL + 3);
    srect(x + w / 2 - 1, y, 2, h, S_GREY + 3);
}
static void station(uint8_t parts, uint16_t wing)   /* parts: 1 core, 2 second module, 4 wings, 8 shuttle port */
{
    int i;
    srect(-210, -4, 420, 8, S_GREY + 1);        /* the truss */
    for (i = -208; i < 210; i += 14) srect(i, -4, 2, 8, S_GREY + 3);
    if (parts & 4) {
        int h = (int)wing * 90 / 256;
        panel(-200, -6 - h, 34, h); panel(-200, 6, 34, h);
        panel(166, -6 - h, 34, h); panel(166, 6, 34, h);
        panel(-150, -6 - h * 2 / 3, 26, h * 2 / 3); panel(124, 6, 26, h * 2 / 3);
    }
    srect(-90, -40, 6, 36, S_GREY + 4);         /* radiators */
    srect(84, 4, 6, 36, S_GREY + 4);
    module(-56, -20, 112, 40, S_GREY);          /* the core module and its node */
    wdisc(st_x + 64 * st_k / 64, st_y, 22 * st_k / 64, S_GREY + 2);
    wdisc(st_x + 60 * st_k / 64, st_y - 6 * st_k / 64, 10 * st_k / 64, S_GREY + 4);
    if (parts & 2) module(-150, -16, 80, 32, S_GOLD);
    if (parts & 8) srect(86, -8, 20, 16, S_GREY + 3);            /* the docking port */
    srect(-20, -26, 4, 6, S_RED);
}

static void earth(uint16_t t)                   /* t: 16ths since the scene start (clouds drift) */
{
    uint8_t i;
    static const int16_t rr[6] = {0, 14, 34, 60, 95, 140};
    wellipse(320, 1250, 1016, 1016, S_AIR);
    wellipse(320, 1250, 1008, 1008, S_AIR + 1);
    wellipse(320, 1250, 1003, 1003, S_AIR + 2);
    for (i = 0; i < 6; i++) wellipse(320, 1250 + rr[i] / 3, 1000 - rr[i], 1000 - rr[i], (uint8_t)(S_SEA + 5 - i));
    for (i = 0; i < 9; i++) {                   /* clouds: flatter near the horizon */
        int y = 268 + (i * 37) % 90, sq = (y - 250) / 8;
        int x = (int)((i * 233 + 2000 - (t * (2 + (y - 250) / 30)) / 4 % 1000) % 1000) - 180;
        wellipse(x, y, 30 + (i * 13) % 40, 2 + sq, (uint8_t)(S_CLOUD + (i % 3)));
    }
}

/* the sprite art: drawn with the canvas into S_DRAW at 2x, then packed into sprite images */
static void spr_pack(uint32_t dst, int x0, int w, int h)
{
    int x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x += 2)
            *(volatile uint16_t *)(dst + (uint32_t)(y * w + x)) = *(volatile uint16_t *)(S_DRAW + (uint32_t)y * W + (uint32_t)(x0 + x));
}
static void s_sprites(void)
{
    static const int8_t sh_body[] = {-50, -2, 40, -4, 58, 2, 40, 8, -50, 8};
    static const int8_t sh_fin[] = {-50, -2, -36, -2, -54, -16};
    static const int8_t sh_wing[] = {-30, 6, 20, 6, -40, 14, -46, 14};
    int i, r;
    blt_fill(S_DRAW, W, W, 32, 0);
    cv_set(S_DRAW, 2, 0, 0, 320, 16);
    wshape(sh_wing, 4, 64, 16, 0, 64, S_GREY + 2);
    wshape(sh_body, 5, 64, 16, 0, 64, S_GREY + 4);
    wshape(sh_fin, 3, 64, 16, 0, 64, S_GREY + 3);
    wrect(14, 22, 100, 4, S_TILE);              /* the black belly and nose */
    wrect(106, 16, 14, 6, S_TILE);
    wrect(94, 13, 8, 3, S_TILE);                /* windows */
    wrect(6, 15, 8, 8, S_GREY + 1);             /* engines */
    blt_wait();
    for (i = 0; i < 4; i++) spr_pack(S_SHUT + (uint32_t)i * 1024, i * 32, 32, 32);
    /* the astronaut, 8 x 12 doubled */
    blt_fill(S_DRAW, W, W, 32, 0);
    wrect(4, 6, 8, 10, S_GREY + 4); wrect(0, 8, 4, 8, S_GREY + 3); wdisc(8, 4, 4, S_GREY + 4);
    wrect(8, 2, 4, 3, S_GOLD + 3); wrect(4, 16, 3, 8, S_GREY + 4); wrect(9, 16, 3, 8, S_GREY + 4);
    blt_wait();
    spr_pack(S_ASTRO, 0, 16, 24);
    /* the flare: soft discs (full resolution: the sun is not a sprite of the era, it is light) */
    for (i = 0; i < 3; i++)
        for (r = 0; r < 32; r++) {
            int x;
            for (x = 0; x < 32; x++) {
                int dx = x - 16, dy = r - 16, d = dx * dx + dy * dy;
                uint8_t c = 0;
                if (d < 225) c = (uint8_t)(S_FLARE + (d < 25 ? 3 : (d < 80 ? 2 : (d < 150 ? 1 : 0))));
                if (i == 1 && d < 120) c = 0;                   /* a ring */
                *(volatile uint8_t *)(S_FLR + (uint32_t)i * 1024 + (uint32_t)(r * 32 + x)) = c;
            }
        }
}

static void st_pal(uint32_t T)
{
    uint8_t i;
    uint16_t k = ramp16(T, PHRASE(18), 4 * 4096);                 /* 16 colours -> 32 */
    uint16_t in = ramp16(T, PHRASE(16), 2048);
    uint16_t out = 256 - ramp16(T, PHRASE(21) + 7 * 4096, 3500);
    uint16_t f = (uint16_t)((uint32_t)in * out >> 8);
    static const uint16_t grey[5] = {RGB(50, 55, 65), RGB(100, 105, 115), RGB(150, 155, 165), RGB(200, 205, 210), RGB(245, 245, 250)};
    static const uint16_t gold[4] = {RGB(110, 70, 10), RGB(180, 130, 30), RGB(225, 180, 60), RGB(255, 230, 140)};
    static const uint16_t pnl[4] = {RGB(10, 20, 60), RGB(30, 50, 130), RGB(60, 90, 180), RGB(120, 150, 220)};
    static const uint16_t sea[6] = {RGB(5, 20, 60), RGB(10, 40, 110), RGB(20, 70, 160), RGB(40, 110, 200), RGB(80, 150, 225), RGB(140, 190, 240)};
    static const uint16_t cl[3] = {RGB(190, 200, 215), RGB(225, 230, 240), RGB(255, 255, 255)};
    static const uint16_t air[3] = {RGB(20, 40, 120), RGB(60, 120, 230), RGB(150, 200, 255)};
    static const uint16_t fl[4] = {RGB(80, 60, 20), RGB(150, 120, 60), RGB(230, 200, 140), RGB(255, 250, 230)};
    /* 16 colours first: pairs of entries share a colour until phrase 18 */
    for (i = 0; i < 5; i++) pal565((uint8_t)(S_GREY + i), dim565(mix565(grey[i & ~1], grey[i], k), f));
    for (i = 0; i < 4; i++) pal565((uint8_t)(S_GOLD + i), dim565(mix565(gold[i | 1], gold[i], k), f));
    for (i = 0; i < 4; i++) pal565((uint8_t)(S_PANEL + i), dim565(mix565(pnl[i & ~1], pnl[i], k), f));
    for (i = 0; i < 6; i++) pal565((uint8_t)(S_SEA + i), dim565(mix565(sea[i & ~1], sea[i], k), f));
    for (i = 0; i < 3; i++) pal565((uint8_t)(S_CLOUD + i), dim565(mix565(cl[2], cl[i], k), f));
    for (i = 0; i < 3; i++) pal565((uint8_t)(S_AIR + i), dim565(mix565(air[1], air[i], k), f));
    pal565(S_STAR, dim565(RGB(110, 110, 140), f));
    pal565(S_STAR + 1, dim565(RGB(180, 180, 200), f));
    pal565(S_STAR + 2, dim565(RGB(255, 255, 255), f));
    pal565(S_TILE, dim565(RGB(30, 30, 35), f));
    pal565(S_RED, (T >> 9) & 1 ? dim565(RGB(255, 40, 30), f) : RGB(60, 0, 0));
    for (i = 0; i < 4; i++) pal565((uint8_t)(S_FLARE + i), dim565(fl[i], f));
    pal565(S_BLACK, 0);
}

static void sc_station(uint8_t pend)
{
    uint16_t i;
    screen_reset();
    /* the stars: three layers in rows y % 3, on playfield B */
    blt_fill(R2, W, W, H, 0);
    blt_wait();
    for (i = 0; i < 230; i++) {
        uint16_t y = (uint16_t)(rnd() % 228), x = (uint16_t)(rnd() % 640);
        *(volatile uint8_t *)(R2 + (uint32_t)y * W + x) = (uint8_t)(S_STAR + y % 3);
        if (y % 3 == 2) *(volatile uint8_t *)(R2 + (uint32_t)y * W + x + 1) = (uint8_t)(S_STAR + 2);
    }
    cv_screen(R2, 2);                           /* the Earth, once, on B under the stars */
    earth(0);
    blt_wait();
    s_sprites();
    set_pfb(R2);
    set_lscr(LSCR);
    for (i = 0; i < 360; i++) { *(volatile uint16_t *)(LSCR + i * 4UL) = 0; *(volatile uint16_t *)(LSCR + i * 4UL + 2) = 0; }
    vid(VC_BMP | VC_HI);
    vid2(VC2_PFB | VC2_LSCR | VC2_SPR);
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127);
        uint16_t t = (uint16_t)((T - PHRASE(16)) >> 8);              /* 16ths in the scene */
        uint32_t buf = fb_back();
        uint8_t parts = 1;
        uint16_t wing = 0;
        if (p >= pend || song_done) break;
        clear8(buf, 0);                         /* transparent: the stars show through */
        cv_screen(buf, 2);
        st_pal(T);
        {                                       /* the star layers: 1, 2 and 4 pixels a 16th */
            uint16_t o[3];
            volatile uint16_t *ls = (volatile uint16_t *)(LSCR + 2);
            o[0] = (uint16_t)((t >> 1) % 640); o[1] = (uint16_t)(t % 640); o[2] = (uint16_t)((t * 3) % 640);
            for (i = 0; i < 228; i += 3) { ls[0] = o[0]; ls[2] = o[1]; ls[4] = o[2]; ls += 6; }
        }
        /* the station: in from the right, then the modules */
        st_k = 64;
        st_x = (int16_t)(t < 64 ? 320 + (64 - t) * 8 : 320);
        st_y = (int16_t)(140 + (SIN((uint8_t)(t >> 1)) >> 4));
        if (p >= 17) parts |= (p > 17 || n >= 96) ? 2 : 0;
        if (p >= 18) { parts |= 4; wing = p > 18 ? 256 : (uint16_t)(n < 64 ? n * 4 : 256); }
        if (p >= 20) parts |= 8;
        if (p == 21) {                          /* pulling back */
            st_k = (int16_t)(64 - (n >> 2));
            st_y = (int16_t)(140 - (n >> 2));
        }
        if (p == 17 && n < 96)                  /* the second module flying in */
            module(-150 - (int)(96 - n) * 4, -16, 80, 32, S_GOLD);
        station(parts, wing);
        /* sprites: the astronaut (19), the flare (19-21), the shuttle (20-21) */
        spr_alloff();
        if (p == 19 || p == 20) {
            int ax = 300 + (SIN((uint8_t)(t * 2)) >> 2), ay = 200 + (SIN((uint8_t)(t * 3 + 40)) >> 3);
            spr_set(0, ax, ay, 24, S_ASTRO, 1, 0);
            wline(st_x + 10, st_y + 20, ax + 4, ay + 8, S_GREY + 3);            /* the tether */
        }
        if (p >= 19) {                          /* the sun comes over the limb; a flare through the middle */
            uint16_t q = p == 19 ? n : 127;
            int sx = 560, sy = 300 - (int)q;
            if (sy < 230) sy = 230;
            spr_set(1, sx - 16, sy - 16, 32, S_FLR, 2, 0);
            spr_set(2, 320 + (320 - sx) / 2 - 16, 180 + (180 - sy) / 2 - 16, 32, S_FLR + 1024, 2, 0);
            spr_set(3, 320 + (320 - sx) - 16, 180 + (180 - sy) - 16, 32, S_FLR + 2048, 2, 0);
        }
        if (p >= 20) {                          /* the shuttle: in from the right, nose first; out to the Moon */
            int lx, sy = 136;
            uint8_t j;
            if (p == 20) lx = n < 112 ? 426 + (int)(112 - n) * 4 : 426;
            else if (n < 64) lx = 426;
            else { lx = 426 + (int)(n - 64) * 6; sy = 136 - (int)(n - 64) * 2; }
            for (j = 0; j < 4; j++) spr_set((uint8_t)(4 + j), lx + j * 32, sy - 16, 32, S_SHUT + (uint32_t)(3 - j) * 1024, 2, 1);
        }
        wtext35(10, 8, p >= 18 && (p > 18 || n >= 32) ? "32 COLOURS  320X180  4 CHANNELS  SPRITES  PARALLAX"
                                                         : "16 COLOURS  320X180  4 SAMPLE CHANNELS", S_GREY + 4, 4);
        fb_flip();
    }
    spr_alloff();
    vid2(0);
}
