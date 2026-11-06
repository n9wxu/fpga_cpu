/* Phrases 10-15: the Moon, as an Apple II game. 320 x 180 in the Apple's six hi-res colours, line-drawn
 * terrain, a lunar module coming down on its engine with the instruments in the Apple's 40-column text,
 * the view zooming in near the ground as in the arcade game; touchdown on the pad at the start of phrase
 * 12, an astronaut, a flag, a walk; the Earth rising over the horizon; then the extra colours arrive (16:
 * shades of blue and grey) and the ascent stage leaves. */

#define M_BLACK 1
#define M_WHITE 2
#define M_GREEN 3
#define M_VIOLET 4
#define M_ORANGE 5
#define M_BLUE 6
#define M_STAR 7
#define M_EARTH 8                               /* 8-12: the Earth's shades (all the Apple's blue at first) */
#define M_GREY 13                               /* 13-16: the ground's shades (all white at first) */
#define M_RIM 17                                /* the horizon's edge: white, then grey */
#define HUD_BUF R2                              /* the instruments, drawn when they change, blitted each frame */
#define TER_N 33

static int16_t ter[TER_N];                      /* the terrain: y every 40 units over 1280 */
static uint16_t hud_val[3] = {0xFFFF, 0xFFFF, 0xFFFF};
static uint8_t m_ground;

static const int8_t lm_desc[] = {-15, 0, 15, 0, 17, -11, -17, -11};
static const int8_t lm_legl[] = {-13, -3, -11, -6, -25, 9, -27, 9};
static const int8_t lm_legr[] = {13, -3, 27, 9, 25, 9, 11, -6};
static const int8_t lm_asc[] = {-11, -11, 11, -11, 13, -21, 5, -29, -8, -29, -13, -21};
static const int8_t lm_win[] = {-7, -22, -1, -22, -3, -26, -7, -26};
static const int8_t lm_noz[] = {-5, 0, 5, 0, 3, 5, -3, 5};

static void lm(int x, int y, uint8_t a, int k, int fl, uint8_t descent)
{
    if (descent) {
        wshape(lm_legl, 4, x, y, a, k, M_WHITE);
        wshape(lm_legr, 4, x, y, a, k, M_WHITE);
        wshape(lm_desc, 4, x, y, a, k, M_ORANGE);
        wshape(lm_noz, 4, x, y, a, k, M_WHITE);
    }
    wshape(lm_asc, 6, x, y, a, k, M_WHITE);
    wshape(lm_win, 4, x, y, a, k, M_BLACK);
    if (fl > 0) {
        int8_t f[6] = {-5, 4, 5, 4, 0, 0};
        f[5] = (int8_t)(4 + fl);
        wshape(f, 3, x, y, a, k, M_ORANGE);
    }
}
static void astro(int x, int y, int s)          /* feet at (x, y); s: units a pixel (1 or 2) */
{
    wrect(x - 3 * s, y - 15 * s, 6 * s, 9 * s, M_WHITE);
    wrect(x + 3 * s, y - 14 * s, 2 * s, 6 * s, M_WHITE);        /* the pack */
    wdisc(x, y - 18 * s, 3 * s, M_WHITE);
    wrect(x - 1 * s, y - 19 * s, 3 * s, 2 * s, M_ORANGE);        /* the visor */
    wrect(x - 3 * s, y - 6 * s, 2 * s, 6 * s, M_WHITE);
    wrect(x + 1 * s, y - 6 * s, 2 * s, 6 * s, M_WHITE);
}
static void flag(int x, int y, int s)
{
    wrect(x, y - 30 * s, s, 30 * s, M_WHITE);
    wrect(x + s, y - 30 * s, 16 * s, 10 * s, M_VIOLET);
    wrect(x + s, y - 25 * s, 16 * s, 2 * s, M_WHITE);
    wtext35(x + 5 * s, y - 29 * s, "T1", M_GREEN, s);
}

/* the instruments: two lines of 40-column text, re-drawn only when a value changes */
static void hud(uint16_t alt, int16_t vs, uint16_t fuel)
{
    char l1[41], l2[41];
    uint16_t v = (uint16_t)(vs < 0 ? -vs : vs);
    uint8_t i;
    if (alt == hud_val[0] && v == hud_val[1] && fuel == hud_val[2]) return;
    hud_val[0] = alt; hud_val[1] = v; hud_val[2] = fuel;
    for (i = 0; i < 40; i++) { l1[i] = ' '; l2[i] = ' '; }
    l1[40] = 0; l2[40] = 0;
    memcpy(l1 + 1, "ALTITUDE", 8); num(l1 + 10, alt, 4); l1[14] = ' ';
    memcpy(l1 + 21, "FUEL", 4); num(l1 + 26, fuel, 4); l1[30] = ' ';
    memcpy(l2 + 1, "VERT SPEED", 10); l2[12] = vs < 0 ? '-' : ' '; num(l2 + 13, v, 3); l2[16] = ' ';
    memcpy(l2 + 21, "SCORE", 5); num(l2 + 27, (uint16_t)(alt ? 0 : 750), 4); l2[31] = ' ';
    blt_fill(HUD_BUF, W, W, 64, M_BLACK);
    cv_set(HUD_BUF, 2, 0, 0, 320, 32);
    cv_text8(0, 2, l1, M_WHITE, 1);
    cv_text8(0, 17, l2, M_WHITE, 1);
}

static void moon_pal(uint32_t T)
{
    uint8_t p = (uint8_t)(T >> 15), i;
    uint16_t k = p == 15 ? ramp16(T, PHRASE(15), 4 * 4096) : 0;    /* to 16 colours */
    uint16_t in = p == 10 ? ramp16(T, PHRASE(10), 2048) : 256;
    static const uint16_t earth16[5] = {RGB(10, 30, 90), RGB(20, 70, 170), RGB(40, 120, 220), RGB(110, 170, 240), RGB(230, 240, 255)};
    static const uint16_t grey16[4] = {RGB(70, 70, 75), RGB(120, 120, 125), RGB(170, 170, 170), RGB(220, 220, 215)};
    pal565(M_BLACK, 0);
    pal565(M_WHITE, dim565(RGB(255, 255, 255), in));
    pal565(M_GREEN, dim565(RGB(20, 245, 60), in));
    pal565(M_VIOLET, dim565(RGB(255, 68, 253), in));
    pal565(M_ORANGE, dim565(RGB(255, 106, 60), in));
    pal565(M_BLUE, dim565(RGB(20, 207, 253), in));
    pal565(M_STAR, dim565(RGB(255, 255, 255), in));
    for (i = 0; i < 5; i++) pal565((uint8_t)(M_EARTH + i), mix565(i == 4 ? RGB(255, 255, 255) : RGB(20, 207, 253), earth16[i], k));
    for (i = 0; i < 4; i++) pal565((uint8_t)(M_GREY + i), mix565(0, grey16[i], k));
    pal565(M_RIM, mix565(RGB(255, 255, 255), RGB(235, 235, 230), k));
}

/* the trajectory: g 0-255 over phrases 10-11; the module stops over the pad (x 900) at g 200 */
static void lm_at(uint16_t g, int *x, int *y, uint8_t *a, int *fl)
{
    int32_t q = g < 200 ? 200 - g : 0, r = 256 - g;
    *x = (int)(900 - 700 * q * q / 40000);
    *y = (int)(300 - 10 - 250 * r * r / 65536);
    *a = (uint8_t)(g < 200 ? 256 - q * 22 / 200 : 0);
    *fl = g < 200 ? (int)(q / 20) + 4 : (int)((g & 8) ? 6 : 3);
}

static void terrain(int camx, int camy, int s)
{
    uint8_t i;
    for (i = 0; i + 1 < TER_N; i++) {
        int x0 = (i * 40 - camx) * s, x1 = ((i + 1) * 40 - camx) * s;
        if (x1 < 0 || x0 > 640) continue;
        wline(x0, (ter[i] - camy) * s, x1, (ter[i + 1] - camy) * s, i == 22 ? M_GREEN : M_WHITE);
    }
    if (s == 1) {
        wtext35((7 * 40 - camx) + 8, ter[7] + 10, "X2", M_GREEN, 4);
        wtext35((22 * 40 - camx) + 8, ter[22] + 10, "X5", M_GREEN, 4);
    }
}

static void sc_moon(uint8_t pend)
{
    uint8_t i;
    static int16_t st[40][2];
    static int16_t prints[16];
    screen_reset();
    vid(VC_BMP | VC_HI);
    for (i = 0; i < 40; i++) { st[i][0] = (int16_t)(rnd() % 640); st[i][1] = (int16_t)(rnd() % 250); }
    for (i = 0; i < TER_N; i++) ter[i] = (int16_t)(220 + (hash8((uint16_t)(i * 7 + 3)) % 100));
    ter[7] = ter[8] = 260;
    ter[22] = ter[23] = 300;
    hud_val[0] = 0xFFFF;
    m_ground = 0;
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127);
        uint32_t buf = fb_back();
        if (p >= pend || song_done) break;
        clear8(buf, M_BLACK);
        cv_screen(buf, 2);
        moon_pal(T);
        if (p <= 13) {
            int lx, ly, fl, s, camx, camy;
            uint8_t la;
            uint16_t g = p <= 11 ? (uint16_t)((T - PHRASE(10)) >> 8) : 256;
            if (g > 256) g = 256;
            lm_at(g, &lx, &ly, &la, &fl);
            if (p >= 12) { lx = 900; ly = 290; la = 0; fl = 0; }
            s = g >= 160 ? 2 : 1;
            camx = s == 1 ? lx - 320 : lx - 160;
            if (camx < 0) camx = 0;
            if (camx > 1280 - 640 / s) camx = 1280 - 640 / s;
            camy = s == 1 ? 0 : 170;
            for (i = 0; i < 40; i++) wrect(st[i][0], st[i][1], 2, 2, M_STAR);
            if (p >= 13) wdisc(560, 110, 26, M_EARTH + 2);      /* the Earth, small, in the black sky */
            terrain(camx, camy, s);
            lm((lx - camx) * s, (ly - camy) * s, la, 64 * s, fl, 1);
            if (p <= 11) {
                uint16_t alt = (uint16_t)((300 - 10 - ly) * 5);
                int16_t vs = (int16_t)(-(256 - (int)g) * 2);
                hud(alt, vs, (uint16_t)(900 - g * 3));
                cv_screen(buf, 2);
            } else {
                hud(0, 0, (uint16_t)(900 - 256 * 3));
                cv_screen(buf, 2);
                if (p == 12 && n < 24) {        /* dust */
                    for (i = 0; i < 12; i++) {
                        int dx = (int)(hash8(i) & 63) - 32, age = (int)n;
                        wrect((lx - camx + dx + dx * age / 8) * 2, (300 - camy) * 2 - (int)(hash8((uint16_t)(i + 5)) & 15) - age, 4, 4, M_ORANGE);
                    }
                }
                if (p == 12 && n < 48) wtext35(200, 140, "A PERFECT LANDING", M_GREEN, 6);
                /* the astronaut: down the ladder (bar 3-4), to the flag (bar 5-6), planted (bar 7) */
                if (p == 12 && n >= 48) {
                    int ax, ay = 300;
                    if (n < 80) { ax = 914; ay = 290 + (int)(n - 48) / 3; }
                    else if (n < 112) ax = 914 + (int)(n - 80) * 2;
                    else ax = 978;
                    if (ay > 300) ay = 300;
                    if (n >= 112) flag((990 - camx) * 2, (300 - camy) * 2, 2);
                    astro((ax - camx) * 2, (ay - camy) * 2, 2);
                }
                if (p == 13) {                  /* the walk: hops on each beat, prints where it lands */
                    uint16_t beat = (uint16_t)(n >> 2), ph4 = (uint16_t)(T & 1023);
                    int ax = 978 + (int)beat * 3, hop = (int)((uint32_t)ph4 * (1024 - ph4) >> 14);
                    if (ax > 1060) ax = 1060 - (ax - 1060);
                    for (i = 0; i < 16 && i < beat; i++) prints[i] = (int16_t)(978 + i * 3);
                    for (i = 0; i < 16 && i < beat; i++) wrect((prints[i] - camx) * 2, (300 - camy) * 2 + 2, 4, 2, M_GREY + 1);
                    flag((990 - camx) * 2, (300 - camy) * 2, 2);
                    astro((ax - camx) * 2, (300 - hop - camy) * 2, 2);
                }
            }
            blt_copy(HUD_BUF, W, buf, W, W, 64, 1, M_BLACK);
            wtext35(10, 336, "6 COLOURS  320X180  SID + FILTER", M_WHITE, 4);
        } else {
            /* earthrise over the horizon; then the colours; the ascent stage leaves towards the Earth */
            uint32_t u = T - PHRASE(14);                       /* 0..65535 over two phrases */
            int ey = 300 - (int)(u >> 9), er = 70 + (int)(u >> 12);
            uint8_t k;
            for (i = 0; i < 40; i++) wrect(st[i][0], st[i][1], 2, 2, M_STAR);
            if (ey < 120) ey = 120;
            for (k = 0; k < 5; k++)             /* the Earth: shades from the limb in to the lit side */
                wdisc(320 + k * er / 10, ey - k * er / 12, er - k * er / 6, (uint8_t)(M_EARTH + k));
            wellipse(320 - er / 3, ey - er / 4, er / 3, er / 5, M_GREEN);
            wellipse(320 + er / 3, ey + er / 6, er / 4, er / 3, M_GREEN);
            wellipse(320 - er * 2 / 3 - 4, ey + 4, er / 2 + 10, er + 10, M_BLACK);    /* the night side */
            if (!m_ground) {                    /* the ground: drawn once into R2, then a keyed copy a frame */
                blt_fill(R2 + 230UL * W, W, W, 130, 0);
                cv_screen(R2, 2);
                wellipse(320, 638, 902, 402, M_RIM);             /* the horizon: a line, then filled */
                wellipse(320, 640, 900, 400, M_GREY);
                wellipse(320, 660, 860, 370, M_GREY + 1);
                wellipse(120, 300, 60, 10, M_GREY + 2);
                wellipse(480, 320, 90, 14, M_GREY + 2);
                wellipse(330, 340, 50, 8, M_GREY + 3);
                blt_wait();
                cv_screen(buf, 2);
                m_ground = 1;
            }
            blt_copy(R2 + 230UL * W, W, buf + 230UL * W, W, W, 130, 1, 0);
            if (p == 15) {                      /* the ascent stage, leaving */
                uint16_t q = (uint16_t)((T - PHRASE(15)) >> 8);
                int ax = 140 + (int)q * 2, ay = 300 - (int)((uint32_t)q * q / 70), kk = 96 - (int)q / 2;
                if (kk < 16) kk = 16;
                lm(140, 300, 0, 96, 0, 1);                       /* the descent stage stays */
                if (q < 120) lm(ax, ay - 12, (uint8_t)(q > 30 ? 12 : 0), kk, 10 + (rnd() & 3), 0);
                wtext35(10, 336, q >= 32 ? "16 COLOURS  320X180  SID + FILTER" : "6 COLOURS  320X180  SID + FILTER", M_WHITE, 4);
            } else {
                lm(140, 300, 0, 96, 0, 1);
                wtext35(10, 336, "6 COLOURS  320X180  SID + FILTER", M_WHITE, 4);
            }
        }
        fb_flip();
    }
}
