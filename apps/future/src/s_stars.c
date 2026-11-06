/* Phrases 33-36: for the stars, with everything: 32 voices, 640 x 360, 256 colours, both playfields, line
 * scroll, sprites, 3D. Phrase 33: the starship in flat-shaded 3D, the camera gliding along it while the
 * engines charge with the music. 34: ignition; it pulls away into a starfield that streaks. 35: the jump:
 * a tunnel on playfield B (each pixel its angle and depth once, then only the palette turns and the
 * line-scroll table bends it), the ship ahead, flashes on the kicks. 36: arrival at a new world. */

#define X_BLACK 1
#define X_STAR 2                                /* 2-5 */
#define X_HULL 16
#define X_HAB 32
#define X_GOLD 48
#define X_GLOW 64                               /* the engines: emissive */
#define X_RAD 80
#define X_TUN 96                                /* 96-255: the tunnel's colours (phrase 35) */
#define TUN_BUF R1
#define X_SPR (0x0F1200UL)                      /* the ship from behind: 64 x 64 as four 32 x 32 sprites */

static int16_t x_st[120][3];
static uint8_t x_ship_built;

/* a disc facing -x (an engine's exhaust) */
static void g_xdisc(int x, int cy, int cz, int r, uint8_t n, uint8_t ramp)
{
    uint16_t c = g_vert(x, cy, cz), b = g_nv;
    uint8_t i;
    for (i = 0; i < n; i++) {
        uint8_t a = (uint8_t)(i * 256 / n);
        g_vert(x, cy + (int)((int32_t)r * SIN16(a) >> 14), cz + (int)((int32_t)r * COS16(a) >> 14));
    }
    for (i = 0; i < n; i++) g_face(c, (uint16_t)(b + (i + 1) % n), (uint16_t)(b + i), 0xFFFF, ramp, -1, 0, 0);
}
static void ship_build(void)
{
    int8_t j;
    g_reset();
    g_cone(-600, 420, 0, 0, 80, 80, 12, X_HULL);                    /* the spine */
    g_cone(420, 760, 0, 0, 80, 12, 12, X_HAB);                      /* the nose */
    g_cone(-120, -40, 0, 0, 300, 300, 18, X_HAB);                   /* the habitat wheel */
    g_box(-100, -300, -14, -60, 300, 14, X_HULL);                   /* its spokes */
    g_box(-100, -14, -300, -60, 14, 300, X_HULL);
    g_cone(-860, -600, 0, 0, 130, 120, 12, X_GOLD);                 /* the drive section */
    for (j = 0; j < 4; j++) {                   /* four engine bells and their glow */
        int y = (j & 1) ? 70 : -70, z = (j & 2) ? 70 : -70;
        g_cone(-1020, -860, y, z, 70, 34, 8, X_HULL);
        g_xdisc(-1018, y, z, 62, 8, X_GLOW);
    }
    g_box(-520, 90, -6, -170, 420, 6, X_RAD);                       /* radiators */
    g_box(-520, -420, -6, -170, -90, 6, X_RAD);
    g_box(-520, -6, 90, -170, 6, 420, X_RAD);
    g_box(-520, -6, -420, -170, 6, -90, X_RAD);
    g_box(200, 80, -40, 330, 130, 40, X_GOLD);                      /* the bridge */
}

static void x_pal(uint32_t T, uint16_t charge)
{
    uint16_t k = ramp16(T, PHRASE(33), 4096);
    uint8_t i;
    g_ramp(X_HULL, RGB(140, 150, 165), k);
    g_ramp(X_HAB, RGB(230, 232, 240), k);
    g_ramp(X_GOLD, RGB(225, 175, 60), k);
    g_ramp(X_RAD, RGB(150, 60, 40), k);
    for (i = 0; i < 16; i++)
        pal565((uint8_t)(X_GLOW + i), dim565(mix565(RGB(40, 80, 200), RGB(220, 240, 255), charge), k));
    pal565(X_BLACK, 0);
    pal565(X_STAR, dim565(RGB(110, 110, 130), k)); pal565(X_STAR + 1, dim565(RGB(170, 170, 190), k));
    pal565(X_STAR + 2, dim565(RGB(230, 230, 245), k)); pal565(X_STAR + 3, dim565(RGB(255, 250, 230), k));
}

/* the tunnel: per half-resolution pixel, (angle x 3 + 2400 / distance) mod 160, into R2; one quadrant
 * computed and mirrored */
static uint8_t x_angle(int dx, int dy)          /* 0-255 */
{
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    uint8_t a;
    if (!ax && !ay) return 0;
    a = ax >= ay ? atan64[(uint16_t)((uint32_t)ay * 64 / ax)] : (uint8_t)(64 - atan64[(uint16_t)((uint32_t)ax * 64 / ay)]);
    if (dx < 0) a = (uint8_t)(128 - a);
    if (dy < 0) a = (uint8_t)(256 - a);
    return a;
}
static uint8_t tun_y;                           /* rows of the tunnel's quadrant done */
static void tunnel_rows(uint8_t nrows)         /* a few rows a frame while the ship flies */
{
    int x, y;
    for (y = tun_y; y < 90 && y < tun_y + nrows; y++) {
        for (x = 0; x < 160; x++) {
            int dx = x * 2 + 1, dy = y * 2 + 1;
            uint16_t d = isqrt32((uint32_t)((int32_t)dx * dx + (int32_t)dy * dy * 4));
            uint8_t a = x_angle(dx, dy), z = (uint8_t)(7200 / (d + 8));
            uint8_t q[4];
            uint8_t k;
            q[0] = a; q[1] = (uint8_t)(128 - a); q[2] = (uint8_t)(256 - a); q[3] = (uint8_t)(128 + a);
            for (k = 0; k < 4; k++) {
                uint16_t v = (uint16_t)(((uint16_t)q[k] * 5 / 8 + z * 3) % 160);
                uint8_t c = (uint8_t)(X_TUN + v);
                int px = k == 0 || k == 2 ? 320 + x * 2 : 318 - x * 2, py = k < 2 ? 180 + y * 2 : 178 - y * 2;
                *(volatile uint16_t *)(TUN_BUF + (uint32_t)py * W + (uint32_t)px) = (uint16_t)(c | c << 8);
            }
        }
        poll();
    }
    tun_y = (uint8_t)y;
    if (tun_y == 90) {
        blt_copy(TUN_BUF, 2 * W, TUN_BUF + W, 2 * W, W, 180, 0, 0);
        blt_wait();
        tun_y = 91;
    }
}
static void tunnel_pal(uint32_t T, uint16_t flash)
{
    uint8_t i, sh = (uint8_t)(T >> 6);
    IO(0x210) = X_TUN;
    for (i = 0; i < 160; i++) {
        uint8_t ph = (uint8_t)(i * 8 / 5 + sh);
        uint16_t c = hsv565((uint8_t)(150 + (SIN(ph) >> 2)), 200, (uint8_t)(110 + (SIN((uint8_t)(ph * 2)) >> 1) + 16));
        IO(0x212) = mix565(c, RGB(255, 255, 255), flash);
    }
}

static void stars_streak(uint16_t speed, uint8_t trail)    /* the starfield flying at the viewer */
{
    uint8_t i;
    for (i = 0; i < 120; i++) {
        int16_t *s = x_st[i];
        int32_t z0, z1;
        int16_t x0, y0, x1, y1;
        s[2] = (int16_t)(s[2] - speed);
        if (s[2] < 60) { s[0] = (int16_t)((int)(rnd() % 1600) - 800); s[1] = (int16_t)((int)(rnd() % 900) - 450); s[2] = 4000; }
        z0 = s[2]; z1 = s[2] + speed * trail;
        x0 = (int16_t)(320 + (int32_t)s[0] * 400 / z0); y0 = (int16_t)(180 + (int32_t)s[1] * 400 / z0);
        x1 = (int16_t)(320 + (int32_t)s[0] * 400 / z1); y1 = (int16_t)(180 + (int32_t)s[1] * 400 / z1);
        if ((unsigned)x0 < 640 && (unsigned)y0 < 360) {
            uint8_t c = (uint8_t)(X_STAR + (z0 < 1000 ? 3 : (z0 < 2000 ? 2 : 1)));
            if (trail && (x0 != x1 || y0 != y1)) cv_line(x1, y1, x0, y0, c);
            else cv_pset(x0, y0, c);
        }
    }
}

/* the ship rendered once: from the side into buf (orthographic), or from behind into the sprites */
static int16_t x_ex, x_ey;                      /* where the engines are in the side view */
static void ship_side(uint32_t buf)
{
    int32_t z;
    g_ortho = 74; g_mx = 330; g_my = 182; g_zfar = 16000;
    g_look(40, 248, 6000);
    g_transform(0);
    cv_screen(buf, 1);
    g_draw(0);
    blt_wait();
    g_project(-1060 - g_cx, -g_cy, -g_cz, &x_ex, &x_ey, &z);
}
static void ship_rear(void)
{
    uint8_t q, y;
    g_ortho = 14; g_mx = 32; g_my = 32; g_zfar = 16000;
    g_look(64, 250, 6000);
    g_transform(0);
    blt_fill(R2, W, W, 64, 0);
    cv_set(R2, 1, 0, 0, 640, 64);
    g_draw(0);
    blt_wait();
    for (q = 0; q < 4; q++)
        for (y = 0; y < 32; y++) {
            uint8_t x;
            for (x = 0; x < 32; x += 2)
                *(volatile uint16_t *)(X_SPR + (uint32_t)q * 1024 + y * 32 + x) =
                    *(volatile uint16_t *)(R2 + (uint32_t)(y + (q >> 1) * 32) * W + (q & 1) * 32 + x);
        }
    for (y = 0; y < 16; y++) {                  /* the exhaust glow, 16 x 16 */
        uint8_t x;
        for (x = 0; x < 16; x++) {
            int dx = x - 8, dy = y - 8, d = dx * dx + dy * dy;
            *(volatile uint8_t *)(X_SPR + 4096 + y * 16 + x) = (uint8_t)(d < 64 ? X_GLOW + 15 - d / 5 : 0);
        }
    }
}
static void rear_sprites(int x, int y)          /* the four quarters at (x, y), top left */
{
    uint8_t q;
    for (q = 0; q < 4; q++) spr_set((uint8_t)(q + 1), x + (q & 1) * 32, y + (q >> 1) * 32, 32, X_SPR + (uint32_t)q * 1024, 2, 0);
}
static void stars_b(uint8_t streak)             /* the stars on B: dots, or streaks for the jump */
{
    uint16_t i;
    blt_fill(R2, W, W, H, 0);
    blt_wait();
    for (i = 0; i < 300; i++) {
        uint16_t y = (uint16_t)(36 + rnd() % 288), x = (uint16_t)(rnd() % 600);
        uint8_t c = (uint8_t)(X_STAR + (rnd() & 3));
        if (streak) blt_fill(R2 + (uint32_t)y * W + x, W, (uint16_t)(10 + (rnd() & 31)), 1, c);
        else *(volatile uint8_t *)(R2 + (uint32_t)y * W + x) = c;
    }
    blt_wait();
}

static void sc_stars(uint8_t pend)
{
    uint8_t streaks = 0, gone = 0;
    uint16_t y;
    screen_reset();
    x_pal(PHRASE(33), 0);
    for (y = 0; y < 256; y++) pal565((uint8_t)y, 0);
    ship_build();
    ship_rear();
    stars_b(0);
    clear8(R0, 0);
    blt_wait();
    ship_side(R0);
    letterbox(R0);
    tun_y = 0;
    set_pfa(R0); set_pfb(R2); set_lscr(LSCR);
    for (y = 0; y < 360; y++) { *(volatile uint16_t *)(LSCR + y * 4UL) = 0; *(volatile uint16_t *)(LSCR + y * 4UL + 2) = 0; }
    vid(VC_BMP | VC_HI | VC_TEXT);
    vid2(VC2_PFB | VC2_LSCR | VC2_SPR);
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127), sk = since_kick();
        uint32_t u = T & 0x7FFF;
        if (p >= pend || song_done) break;
        spr_alloff();
        if (p <= 34) {
            volatile uint16_t *ls = (volatile uint16_t *)(LSCR + 2);
            int o;                              /* the ship's offset to the right */
            if (tun_y < 90) tunnel_rows(p == 33 ? 2 : 6);
            if (p == 33) {
                x_pal(T, (uint16_t)(sk < 384 ? 255 : n * 2));
                o = 30 - (int)(u >> 10);        /* a slow glide to the left */
                for (y = 0; y < 360; y++) { *ls = (uint16_t)((640 - (((u >> 7) * (1 + (y & 3))) >> 2) % 640) % 640); ls += 2; }
                title("THE STARSHIP", "2150", (uint8_t)(n >= 16 && n < 64));
            } else {
                uint32_t q = u >> 8;
                if (!streaks) { stars_b(1); streaks = 1; title("", "", 0); }
                x_pal(T, 255);
                o = q < 16 ? -2 : (int)((q - 16) * (q - 16) / 6) - 2;
                for (y = 0; y < 360; y++) { *ls = (uint16_t)((640 - ((q * q * (2 + (y % 5))) >> 3) % 640) % 640); ls += 2; }
                if (o > 0 && !gone) {           /* what would wrap round to the left is cleared */
                    int c0 = 640 - o;
                    if (c0 < 0) c0 = 0;
                    blt_fill(R0 + 36UL * W + (uint32_t)c0, W, (uint16_t)(640 - c0), 288, 0);
                    if (c0 == 0) gone = 1;
                }
                if (!gone) spr_set(5, x_ex + o - 14 - (int)(rnd() & 3), x_ey - 8, 16, X_SPR + 4096, 1, 0);   /* the exhaust */
            }
            scroll_a((uint16_t)((640 - o % 640) % 640), 0);
            vsync();
        } else if (p == 35) {
            /* the jump: the tunnel on A (built while the ship flew), bent by line scroll; the ship ahead */
            uint16_t fl = sk < 512 ? (uint16_t)(160 - (sk * 5 >> 4)) : 0;
            volatile uint16_t *ls = (volatile uint16_t *)LSCR;
            if (tun_y < 91) { tunnel_rows(90); }
            if (n == 0 || (IO(0x220) & VC2_PFB)) {
                letterbox(TUN_BUF);
                set_pfa(TUN_BUF);
                scroll_a(0, 0);
                vid2(VC2_LSCR | VC2_SPR);
            }
            x_pal(T, 255);
            tunnel_pal(T * 3, fl);
            for (y = 0; y < 360; y++) { *ls = (uint16_t)((SIN((uint8_t)(y * 2 + (T >> 6))) * 12 >> 7) + 640) % 640; ls += 2; }
            rear_sprites(288 + (SIN((uint8_t)(T >> 8)) >> 4), 150 + (SIN((uint8_t)(T >> 9)) >> 5));
            spr_set(5, 312 + (SIN((uint8_t)(T >> 8)) >> 4) - (int)(rnd() & 1), 172 + (SIN((uint8_t)(T >> 9)) >> 5), 16, X_SPR + 4096, 1, 0);
            title("", "", 0);
            vsync();
        } else {
            /* arrival: a white flash, then a new world turning; the ship heading for it */
            uint32_t buf = fb_back();
            uint16_t f = n < 16 ? (uint16_t)(256 - n * 16) : 0;
            if (IO(0x220) & VC2_LSCR) { vid2(VC2_SPR); scroll_a(0, 0); }
            pl_tex = tex_terra;
            planet_pal(pal_terra, (uint16_t)(256 - f / 2));
            clear8(buf, 0);
            letterbox(buf);
            planet_draw(buf, (uint8_t)((T >> 10) & 127));
            stars_streak(0, 0);
            x_pal(T, 200);
            if (f) {
                uint16_t i2;
                IO(0x210) = 128;
                for (i2 = 0; i2 < 128; i2++) IO(0x212) = mix565(0, RGB(255, 255, 255), f);
            }
            rear_sprites(150 + (int)n, 200 - (int)(n / 3));
            title("A NEW WORLD", "2200", (uint8_t)(n >= 24 && n < 80));
            fb_flip();
        }
    }
    title("", "", 0);
    spr_alloff();
    g_ortho = 0;
    vid2(0);
}
