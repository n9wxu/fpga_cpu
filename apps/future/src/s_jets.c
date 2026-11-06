/* Phrases 0-4: jets. One bit at 128 x 72 (a block of 5 x 5 screen pixels): the title typed in, a jet on the
 * runway, the take-off. 160 x 90 for the formation flight; then colour arrives one entry at a time: the sky,
 * the jets' trim. 213 x 120 and the sound chip for the supersonic run, the climb, and the sky going dark. */

#define J_SKY 1
#define J_WHITE 2
#define J_GROUND 3
#define J_CLOUD 4
#define J_TRIM 5
#define J_FLAME 6
#define J_STAR 7
#define J_CONE 8

/* the jet, side on, facing right: 100 units long */
static const int8_t jet_body[] = {58, 1, 36, -8, -44, -9, -52, -4, -52, 8, 30, 9};
static const int8_t jet_fin[] = {-52, -8, -28, -8, -56, -32};
static const int8_t jet_wing[] = {-20, 3, 16, 3, -32, 20};
static const int8_t jet_can[] = {16, -8, 40, -5, 24, -15};

static void jet(int x, int y, uint8_t a, int k, uint8_t burner)
{
    wshape(jet_fin, 3, x, y, a, k, J_TRIM);
    wshape(jet_body, 6, x, y, a, k, J_WHITE);
    wshape(jet_wing, 3, x, y, a, k, J_TRIM);
    wshape(jet_can, 3, x, y, a, k, J_SKY);
    if (burner) {
        int8_t fl[6] = {-50, -3, -50, 4, 0, 0};
        fl[4] = (int8_t)(-56 - burner - (rnd() & 7));
        fl[5] = 1;
        wshape(fl, 3, x, y, a, k, J_FLAME);
    }
}
static void cloud(int x, int y, int s)          /* three puffs; s in 1/16 */
{
    wellipse(x, y, 5 * s, 2 * s, J_CLOUD);
    wellipse(x - 2 * s, y - s, 3 * s, 2 * s, J_CLOUD);
    wellipse(x + 2 * s, y - s, 2 * s, (3 * s) >> 1, J_CLOUD);
}
static int16_t j_star[40][2];

static void jets_pal(uint32_t T)
{
    uint8_t p = (uint8_t)(T >> 15);
    uint16_t sky = 0, cloud = RGB(255, 255, 255), trim = RGB(255, 255, 255), fl = RGB(255, 255, 255);
    uint16_t star = 0;
    static const uint16_t cyan = RGB(85, 255, 255), mag = RGB(255, 85, 255);
    if (p >= 3) {
        sky = mix565(0, cyan, ramp16(T, PHRASE(3), 4096));
        trim = mix565(RGB(255, 255, 255), mag, ramp16(T, PHRASE(3) + 4 * 4096, 2048));
        fl = trim;
    }
    if (p == 4) {                               /* the climb: the sky goes black and the stars come out */
        uint16_t k = ramp16(T, PHRASE(4) + 4 * 4096, 3 * 4096);
        sky = mix565(cyan, 0, k);
        cloud = mix565(RGB(255, 255, 255), cyan, k);
        star = mix565(0, RGB(255, 255, 255), ramp16(T, PHRASE(4) + 5 * 4096, 4096));
        if (T > PHRASE(4) + 7 * 4096) {         /* the last bar: out */
            uint16_t o = 256 - ramp16(T, PHRASE(4) + 7 * 4096, 3500);
            trim = dim565(trim, o); fl = dim565(fl, o); cloud = dim565(cloud, o);
            pal565(J_WHITE, dim565(RGB(255, 255, 255), o));
        } else pal565(J_WHITE, RGB(255, 255, 255));
    } else pal565(J_WHITE, RGB(255, 255, 255));
    pal565(J_SKY, sky);
    pal565(J_GROUND, p >= 3 ? cyan : RGB(255, 255, 255));
    pal565(J_CLOUD, cloud);
    pal565(J_TRIM, trim);
    pal565(J_FLAME, fl);
    pal565(J_STAR, star);
    pal565(J_CONE, mix565(cloud, sky, 128));
}

static void sc_jets(uint8_t pend)
{
    uint8_t i;
    screen_reset();
    vid(VC_BMP | VC_HI);
    for (i = 0; i < 40; i++) { j_star[i][0] = (int16_t)(rnd() % 640); j_star[i][1] = (int16_t)(rnd() % 300); }
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15), b = p < 2 ? 5 : (p < 4 ? 4 : 3);
        uint16_t n = (uint16_t)((T >> 8) & 127);        /* the 16th in the phrase */
        uint32_t u = T & 0x7FFF;                         /* the time in the phrase */
        uint32_t buf = fb_back();
        if (p >= pend || song_done) break;
        clear8(buf, J_SKY);
        cv_screen(buf, b);
        jets_pal(T);
        if (p == 0) {
            /* the title, a letter a beat; the jet waits on the runway */
            static const char ttl[] = "THE FUTURE";
            char s[12];
            int k = (int)(T >> 10) - 1, y = 60;
            if (k > 10) k = 10;
            if (k < 0) k = 0;
            for (i = 0; i < (uint8_t)k; i++) s[i] = ttl[i];
            s[i] = 0;
            if (n >= 112) y = 60 - (int)(n - 112) * 8;
            {
                int x = wtext35(125, y, s, J_WHITE, 10);
                if (k < 10 || (T & 512)) wrect(x, y, 30, 50, J_WHITE);
            }
            if (n >= 64) wtext35(20, 325, "1-BIT  128X72  1 VOICE", J_WHITE, 5);
            wrect(0, 280, 640, 5, J_GROUND);
            for (i = 0; i < 8; i++) wrect(i * 80 + 10, 295, 40, 5, J_GROUND);
            jet(150, 262, 0, 72, 0);
        } else if (p == 1) {
            /* the take-off roll, rotation and climb; the ground drops away */
            uint16_t nn = n, c = nn > 48 ? (uint16_t)(nn - 48) : 0;
            int climb = (int)((uint32_t)c * c / 40);
            int off = (int)(((uint32_t)nn * nn / 3) % 80);
            int gy = 280 + climb / 2, jy = 262 - climb / 2;
            uint8_t a = (uint8_t)(c ? 256 - (c > 40 ? 20 : c / 2) : 0);
            if (gy < 360) {
                wrect(0, gy, 640, 5, J_GROUND);
                for (i = 0; i < 9; i++) wrect(i * 80 + 10 - off, gy + 15, 40, 5, J_GROUND);
            }
            if (nn > 70) cloud(700 - (int)(nn - 70) * 14, 80, 10);
            if (nn > 100) cloud(760 - (int)(nn - 100) * 16, 150, 7);
            jet(150 + (int)(nn > 90 ? (nn - 90) * 2 : 0), jy, a, 72, (uint8_t)(nn > 20 ? 12 : 0));
            wtext35(20, 330, "1-BIT  128X72  1 VOICE", J_WHITE, 5);
        } else {
            /* cruising: two layers of clouds, wingmen joining, then (p 4) the run and the climb */
            uint16_t t16 = (uint16_t)((T - PHRASE(2)) >> 8);        /* 16ths since phrase 2 */
            int lx = 230, ly = 180 + (SIN((uint8_t)(t16 * 2)) >> 3);
            uint8_t la = 0;
            int k;
            for (k = 0; k < 4; k++) {           /* far clouds, small and slow */
                int x = (int)((k * 211 + 900 - (int)((t16 * 3) % 900)) % 900) - 130;
                cloud(x, 60 + k * 70 % 200, 5);
            }
            if (p == 4) {
                if (n >= 64) {                  /* the climb */
                    uint16_t c = (uint16_t)(n - 64);
                    la = (uint8_t)(256 - (c > 16 ? 32 : c * 2));
                    lx = 230 + (int)c * 3;
                    ly = 180 - (int)((uint32_t)c * c / 12);
                }
                if (n >= 72)
                    for (i = 0; i < 40; i++) wrect(j_star[i][0], j_star[i][1], 3, 3, J_STAR);
            }
            /* the wingmen: in at phrase 2 bar 2, out (one up, one down) in phrase 4 bar 2 */
            if (p < 4 || n < 48) {
                int in = t16 < 32 ? -300 : (t16 < 64 ? -300 + (int)(t16 - 32) * 12 : 84);
                int dy = p == 4 && n >= 32 ? (int)(n - 32) * 10 : 0;
                int wv = SIN((uint8_t)(t16 * 2 + 40)) >> 3;
                if (in > 84) in = 84;
                jet(150 + in, 115 + wv - dy, (uint8_t)(dy ? 248 : 0), 52, 8);
                jet(150 + in, 250 + wv + dy, (uint8_t)(dy ? 8 : 0), 52, 8);
            }
            if (p == 4 && n < 64) {             /* supersonic: the vapour cone and speed lines */
                int8_t cone[6] = {44, -2, 10, -26, 10, 28};
                if (n & 2) wshape(cone, 3, lx, ly, 0, 64, J_CONE);
                for (i = 0; i < 6; i++) {
                    int sy = 40 + i * 55, sx = (int)((i * 173 + 640 - (int)((n * 64) % 640)) % 640);
                    wrect(sx, sy, 90, 3, J_CLOUD);
                }
            }
            jet(lx, ly, la, 80, (uint8_t)(p == 4 ? 20 : 10));
            for (k = 0; k < 2; k++) {           /* near clouds, big and fast */
                int x = (int)((k * 470 + 1100 - (int)((t16 * 9) % 1100)) % 1100) - 230;
                if (p < 4 || n < 64) cloud(x, 300 - k * 40, 14);
            }
            {
                static const char *const tg[3] = {"1-BIT  160X90  2 VOICES", "4 COLOURS  160X90  2 VOICES",
                                                  "4 COLOURS  213X120  4 VOICES"};
                if (p < 4 || n < 112) wtext35(16, 336, tg[p - 2], J_WHITE, p == 4 ? 3 : 4);
            }
        }
        fb_flip();
    }
}
