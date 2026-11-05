/* Phrases 5-9: rockets. 213 x 120 in the four colours of the jets' last minutes: a night pad, the gantry, the
 * countdown in game-show digits. Ignition adds orange and yellow (six colours), smoke billows, the rocket
 * climbs out of the frame with an altitude counter running. Phrase 8: 320 x 180, the colours drift to the
 * Apple II's six, the first stage falls away and the Earth curves below. Phrase 9: the coast to the Moon. */

#define K_SKY 1
#define K_WHITE 2
#define K_CYAN 3                                /* ground, gantry; later the Apple's blue */
#define K_MAG 4                                 /* bands; later violet */
#define K_ORANGE 5
#define K_YELLOW 6
#define K_STAR 7
#define K_SMOKE 8
#define K_GREEN 9
#define K_DARK 10                               /* craters */

/* the rocket in half units (k = 128 doubles), origin at the bottom of each part, up is -y */
static const int8_t rk_s1[] = {-10, 0, 10, 0, 10, -70, -10, -70};
static const int8_t rk_finl[] = {-10, 0, -10, -16, -19, 5};
static const int8_t rk_finr[] = {10, 0, 19, 5, 10, -16};
static const int8_t rk_b1[] = {-10, -20, 10, -20, 10, -25, -10, -25};
static const int8_t rk_b2[] = {-10, -58, 10, -58, 10, -63, -10, -63};
static const int8_t rk_s2[] = {-8, 0, 8, 0, 8, -34, -8, -34};
static const int8_t rk_b3[] = {-8, -4, 8, -4, 8, -9, -8, -9};
static const int8_t rk_cap[] = {-8, -34, 8, -34, 0, -52};
static const int8_t rk_tow[] = {-1, -50, 1, -50, 1, -62, -1, -62};

static void flame(int x, int y, uint8_t a, int k, int len)
{
    int8_t f[6] = {-8, 0, 8, 0, 0, 0};
    f[5] = (int8_t)len;
    wshape(f, 3, x, y, a, k, K_ORANGE);
    f[0] = -5; f[2] = 5; f[5] = (int8_t)(len * 5 / 8);
    wshape(f, 3, x, y, a, k, K_YELLOW);
}
/* stage 1 at (x, y) (its bottom), turned a; returns nothing, draws fins, body, bands */
static void stage1(int x, int y, uint8_t a, int k)
{
    wshape(rk_finl, 3, x, y, a, k, K_MAG);
    wshape(rk_finr, 3, x, y, a, k, K_MAG);
    wshape(rk_s1, 4, x, y, a, k, K_WHITE);
    wshape(rk_b1, 4, x, y, a, k, K_MAG);
    wshape(rk_b2, 4, x, y, a, k, K_MAG);
}
static void stage2(int x, int y, int k)         /* upright, bottom at y */
{
    wshape(rk_s2, 4, x, y, 0, k, K_WHITE);
    wshape(rk_b3, 4, x, y, 0, k, K_MAG);
    wshape(rk_cap, 3, x, y, 0, k, K_WHITE);
    wshape(rk_tow, 4, x, y, 0, k, K_WHITE);
}
static void gantry(int x, int base, int top)
{
    int y;
    wrect(x, top, 4, base - top, K_CYAN);
    wrect(x + 40, top, 4, base - top, K_CYAN);
    for (y = base; y > top + 30; y -= 30) {
        wline(x, y, x + 44, y - 30, K_CYAN);
        wline(x + 44, y, x, y - 30, K_CYAN);
    }
    wrect(x + 44, top + 60, 30, 4, K_CYAN);     /* the arms */
    wrect(x + 44, top + 150, 30, 4, K_CYAN);
}
static uint8_t hash8(uint16_t v) { v = (uint16_t)(v * 0x9E37 + 0x7F4A); return (uint8_t)(v >> 8 ^ v); }
static void smoke(uint32_t age_all, int x, int y)   /* puffs rolling out both ways from (x, y) */
{
    uint8_t i;
    for (i = 0; i < 12; i++) {
        uint32_t born = (uint32_t)i * 1000;
        if (age_all > born) {
            int age = (int)((age_all - born) >> 6);              /* 0.. */
            int dir = (i & 1) ? 1 : -1, sp = 2 + (hash8(i) & 3);
            int px = x + dir * (age * sp / 8 + 10), py = y - (int)(hash8((uint16_t)(i + 40)) & 15) - age / 30;
            int r = 14 + age / 10 + (hash8((uint16_t)(i + 9)) & 7);
            if (r > 52) r = 52;
            wdisc(px, py, r, K_SMOKE);
        }
    }
}
static void digits(int x, int y, const char *s, int px) { wtext35(x, y, s, K_YELLOW, px); }
static void num(char *d, uint16_t v, uint8_t n)  /* n digits, zero padded */
{
    d[n] = 0;
    while (n--) { d[n] = (char)('0' + v % 10); v /= 10; }
}

static void rk_pal(uint32_t T)
{
    uint8_t p = (uint8_t)(T >> 15);
    uint16_t k8 = p >= 8 ? ramp16(T, PHRASE(8), 4 * 4096) : 0;          /* to the Apple's colours */
    uint16_t in = ramp16(T, PHRASE(5), 2048);
    uint16_t out = p == 9 ? 256 - ramp16(T, PHRASE(9) + 7 * 4096, 3500) : 256;
    uint16_t cyan = mix565(RGB(85, 255, 255), RGB(20, 207, 253), k8);
    uint16_t mag = mix565(RGB(255, 85, 255), RGB(255, 68, 253), k8);
    uint16_t fire = p >= 6 ? 256 : 0;
    pal565(K_SKY, 0);
    pal565(K_WHITE, dim565(RGB(255, 255, 255), (uint16_t)((uint32_t)in * out >> 8)));
    pal565(K_CYAN, dim565(cyan, (uint16_t)((uint32_t)in * out >> 8)));
    pal565(K_MAG, dim565(mag, (uint16_t)((uint32_t)in * out >> 8)));
    pal565(K_ORANGE, dim565(RGB(255, 106, 60), (uint16_t)(fire * out >> 8)));
    pal565(K_YELLOW, p >= 6 ? dim565(mix565(RGB(255, 255, 85), RGB(255, 106, 60), k8), out) : RGB(255, 255, 255));
    pal565(K_STAR, dim565(RGB(255, 255, 255), (uint16_t)((uint32_t)in * out >> 8)));
    pal565(K_SMOKE, RGB(255, 255, 255));
    pal565(K_GREEN, dim565(RGB(20, 245, 60), (uint16_t)((uint32_t)k8 * out >> 8)));
    pal565(K_DARK, 0);
}

static void sc_rocket(uint8_t pend)
{
    uint8_t i;
    static int16_t st[48][2];
    screen_reset();
    vid(VC_BMP | VC_HI);
    for (i = 0; i < 48; i++) { st[i][0] = (int16_t)(rnd() % 640); st[i][1] = (int16_t)(rnd() % 360); }
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint16_t n = (uint16_t)((T >> 8) & 127);
        uint32_t u = T & 0x7FFF;
        uint32_t buf = fb_back();
        int rise = 0, cam = 0, x = 300;
        char s[24];
        if (p >= pend || song_done) break;
        clear8(buf, K_SKY);
        cv_screen(buf, (uint8_t)(p < 8 ? 3 : 2));
        rk_pal(T);
        if (p <= 7) {
            /* the rise: held down for a bar, then accelerating; the camera follows from y 180 */
            if (p >= 6) {
                uint32_t r = T - PHRASE(6);
                rise = r < 4096 ? 0 : (int)(((r - 4096) >> 8) * ((r - 4096) >> 8) / 14);
            }
            if (rise > 100) { cam = rise - 100; rise = 100; }
            for (i = 0; i < 48; i++) {          /* stars, scrolling down slowly once moving */
                int sy = (int)((st[i][1] + cam / 4) % 360);
                wrect(st[i][0], sy, 3, 3, K_STAR);
            }
            if (cam < 400) {
                int gy = 330 + cam;
                gantry(190, gy, gy - 300);
                wrect(0, gy, 640, 40, K_CYAN);
            }
            if (p >= 7) {                       /* clouds rushing down past the rocket */
                for (i = 0; i < 3; i++) {
                    int cy = (int)((u >> 6) * (3 + i) + i * 150) % 600 - 120;
                    wellipse(120 + i * 200, cy, 90, 20, K_SMOKE);
                }
            }
            if (p >= 6) {
                int fy = 330 - rise;
                flame(x, fy, 0, 128, 24 + (rnd() & 15) + (rise ? 14 : 0));
                if (cam < 300) smoke(T - PHRASE(6), x, 330 + cam);
            } else if (n >= 64 && (n & 4)) {    /* venting before the count ends */
                wdisc(x - 30 - (int)(n & 7) * 2, 140, 10 + (int)(n & 3) * 2, K_SMOKE);
            }
            stage1(x, 330 - rise, 0, 96);
            stage2(x, 330 - rise - 105, 96);
            if (p == 5) {
                int c = (int)(n >> 3) - 5;      /* 10..1 on every second beat from beat 12 */
                if (c >= 1 && c <= 10) {
                    num(s, (uint16_t)(11 - c), 2);
                    wtext35(380, 40, "T-MINUS", K_WHITE, 6);
                    digits(400, 90, s, 18);
                }
                wtext35(16, 336, "4 COLOURS  213X120  4 VOICES", K_WHITE, 3);
            } else {
                uint16_t alt = (uint16_t)((rise + cam) >> 2), vel = (uint16_t)(p == 6 ? n * 6 : 760 + n * 9);
                if (p == 6 && n < 8) digits(380, 60, "LIFTOFF", 9);
                wtext35(16, 16, "ALT", K_WHITE, 6);
                num(s, alt, 4); wtext35(90, 16, s, K_YELLOW, 6);
                wtext35(16, 52, "VEL", K_WHITE, 6);
                num(s, vel, 4); wtext35(90, 52, s, K_YELLOW, 6);
                wtext35(16, 336, "6 COLOURS  213X120  4 VOICES", K_WHITE, 3);
            }
        } else if (p == 8) {
            /* staging: the first stage falls away turning, the second lights; the Earth below */
            int sep = (int)(n > 8 ? (n - 8) : 0);
            for (i = 0; i < 48; i++) wrect(st[i][0], (st[i][1] + (int)n) % 360, 2, 2, K_STAR);
            wellipse(320, 360 + 520 - (int)n / 2, 900, 600, K_CYAN);           /* the Earth's limb */
            wellipse(150, 330 - (int)n / 2, 140, 30, K_GREEN);
            wellipse(470, 345 - (int)n / 2, 110, 25, K_GREEN);
            if (sep < 120) {
                stage1(x - sep / 3, 300 + sep * 2, (uint8_t)(sep / 2), 96);
                if (sep < 4) flame(x, 300, 0, 96, 30);
            }
            if (sep > 6) flame(x, 196, 0, 96, 14 + (rnd() & 7));
            stage2(x, 196, 96);
            wtext35(10, 6, "STAGE 2", K_WHITE, 4);
            wtext35(10, 336, "6 COLOURS  320X180  SID + FILTER", K_WHITE, 4);
        } else {
            /* the coast: Earth behind, the Moon ahead and growing */
            uint16_t z = (uint16_t)(u >> 7);                  /* 0-255 */
            int er = 120 - z / 3, mr = 30 + (int)((uint32_t)z * z / 300);
            int cx = 60 + z * 2;
            for (i = 0; i < 48; i++) wrect(st[i][0], st[i][1], 2, 2, K_STAR);
            wdisc(80 - z / 2, 220, er, K_CYAN);
            wellipse(60 - z / 2, 190, er / 2, er / 4, K_GREEN);
            wellipse(110 - z / 2, 260, er / 3, er / 5, K_GREEN);
            wdisc(560 - z / 3, 160, mr, K_WHITE);
            wellipse(560 - z / 3 - mr / 3, 160 - mr / 4, mr / 5, mr / 6, K_MAG);
            wellipse(560 - z / 3 + mr / 4, 160 + mr / 3, mr / 6, mr / 8, K_MAG);
            if (z < 200) {
                stage2(cx, 180 - (SIN((uint8_t)(z)) >> 4), 40);
            }
            wtext35(10, 336, "6 COLOURS  320X180  SID + FILTER", K_WHITE, 4);
        }
        fb_flip();
    }
}
