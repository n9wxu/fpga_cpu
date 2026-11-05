/* Phrases 37-39: the credits. A spiral galaxy turning (1500 stars, the inner ones faster) on playfield A;
 * the credits on playfield B in front, drawn line by line into the rows about to scroll in and moved by
 * B's vertical scroll; letterbox bars on the text layer; the title at the end, and the fade. */

#define G_N 1100
#define C_HEAD 250
#define C_NAME 251
#define C_GAL 16                                /* 16-47: the galaxy's colours, by radius */

static uint8_t gx_r[G_N], gx_a[G_N];

typedef struct { uint8_t style; const char *s; } cred_t;     /* style: 0 gap, 1 heading, 2 name, 3 title */
static const cred_t creds[] = {
    {3, "THE FUTURE"}, {0, 0}, {2, "A DEMO FOR THE T1 COMPUTER"}, {2, "ON THE TANG NANO 20K"}, {0, 0}, {0, 0},
    {1, "CONCEPT"}, {2, "JOSEPH JULICHER"}, {0, 0},
    {1, "CODE, PICTURES AND MUSIC"}, {2, "CLAUDE"}, {0, 0},
    {1, "THE COMPUTER"}, {2, "T1 ON A GW2AR-18 FPGA"}, {2, "1 MB SDRAM"}, {0, 0},
    {1, "PICTURES"}, {2, "640 X 360  256 COLOURS"}, {2, "TWO PLAYFIELDS  LINE SCROLL"}, {2, "64 SPRITES  A BLITTER"}, {0, 0},
    {1, "SOUND"}, {2, "32 VOICES AT 48 KHZ"}, {2, "SAMPLES  CHIP VOICES  A FILTER"}, {0, 0},
    {1, "COMPILER"}, {2, "FCC"}, {0, 0},
    {1, "STARRING"}, {2, "THE JET  THE ROCKET  THE LANDER"}, {2, "THE STATION  THE BASE"}, {2, "FOUR PLANETS  ONE STARSHIP"}, {0, 0},
    {2, "EVERY PIXEL AND EVERY NOTE"}, {2, "IS MADE AS IT PLAYS"}, {0, 0}, {0, 0},
    {1, "THANK YOU FOR WATCHING"},
};
#define N_CRED (sizeof creds / sizeof creds[0])

static void galaxy_init(void)
{
    uint16_t i;
    for (i = 0; i < G_N; i++) {
        uint8_t arm = (uint8_t)(i & 1), r;
        uint16_t q = rnd();
        r = (uint8_t)(((uint16_t)(q & 255) * (q & 255)) >> 8);   /* more stars near the middle */
        if (r < 6) r = (uint8_t)(6 + (q >> 12));
        gx_r[i] = r;
        gx_a[i] = (uint8_t)(arm * 128 + r * 3 / 2 + ((rnd() & 31) - 16) * (r < 40 ? 4 : 1));
    }
}
static void galaxy_pal(uint16_t k)
{
    uint8_t i;
    for (i = 0; i < 32; i++) {
        uint16_t c = i < 6 ? mix565(RGB(255, 240, 200), RGB(255, 210, 150), (uint16_t)(i * 40))
                           : mix565(RGB(190, 200, 255), RGB(110, 130, 220), (uint16_t)((i - 6) * 9));
        if (i > 10 && (i % 7) == 3) c = RGB(255, 140, 190);
        pal565((uint8_t)(C_GAL + i), dim565(c, k));
    }
}
static void galaxy(uint32_t buf, uint32_t T)
{
    uint16_t i;
    uint8_t t = (uint8_t)(T >> 9);
    int cx = 320, cy = 180;
    volatile uint8_t *base = (volatile uint8_t *)buf;
    for (i = 0; i < G_N; i++) {
        uint8_t r = gx_r[i], a = (uint8_t)(gx_a[i] + (uint8_t)(t * 24 / (r / 16 + 3)));
        int x = cx + (int)(((int32_t)r * COS(a) * 9) >> 11), y = cy + (int)(((int32_t)r * SIN(a) * 9) >> 12);
        if ((unsigned)x < 640 && (unsigned)(y - 28) < 294) base[(uint32_t)y * W + (uint32_t)x] = (uint8_t)(C_GAL + (r >> 3));
        if (!(i & 255)) poll();
    }
    cv_screen(buf, 1);
    cv_ellipse(cx, cy, 22, 9, C_GAL + 1);
    cv_ellipse(cx, cy, 11, 5, C_GAL);
}

static uint8_t cred_h(uint8_t st) { return st == 3 ? 56 : (st == 1 ? 30 : (st == 2 ? 22 : 16)); }
static void cred_draw(uint8_t i, int y)         /* line i at B's row y (mod 360) */
{
    const cred_t *c = &creds[i];
    int sc = c->style == 3 ? 3 : (c->style == 1 ? 2 : 1), h = cred_h(c->style), x, yy = y % 360;
    blt_fill(R2 + (uint32_t)yy * W, W, W, (uint16_t)(yy + h <= 360 ? h : 360 - yy), 0);
    if (yy + h > 360) blt_fill(R2, W, W, (uint16_t)(yy + h - 360), 0);
    if (!c->s) return;
    x = 320 - text8_w(c->s, sc) / 2;
    cv_set(R2, 1, 0, 0, 640, 360);
    cv_text8(x, yy, c->s, c->style == 2 ? C_NAME : C_HEAD, sc);
    if (yy + h > 360) cv_text8(x, yy - 360, c->s, c->style == 2 ? C_NAME : C_HEAD, sc);
    blt_wait();
}

static void sc_credits(uint8_t pend)
{
    uint8_t next = 0, fin = 0;
    int ny = 340;                               /* the next line's place in the scrolled column */
    uint16_t r;
    screen_reset();
    galaxy_init();
    blt_fill(R2, W, W, H, 0);
    blt_wait();
    for (r = 0; r < 2; r++) tstr(0, (int)r, "                                                                                ", TA(0, 0));
    for (r = 23; r < 25; r++) tstr(0, (int)r, "                                                                                ", TA(0, 0));
    set_pfb(R2);
    scroll_b(0, 0);
    vid(VC_BMP | VC_HI | VC_TEXT);
    vid2(VC2_PFB | VC2_BFRONT);
    pal565(C_HEAD, RGB(255, 200, 90));
    pal565(C_NAME, RGB(240, 240, 250));
    for (;;) {
        uint32_t T = mpos();
        uint8_t p = (uint8_t)(T >> 15);
        uint32_t u = T - PHRASE(37), buf = fb_back();
        int s = (int)(u / 96);                  /* the scroll: a pixel every 96 units, about 25 a second */
        uint16_t k = 256;
        if (p >= pend || song_done) break;
        if (u < 4096) k = (uint16_t)(u >> 4);
        /* the end: the last half phrase holds the title, then fades */
        if (u >= PHRASE(2) + 4 * 4096) {
            if (!fin) {
                blt_fill(R2, W, W, H, 0);
                cv_set(R2, 1, 0, 0, 640, 360);
                scroll_b(0, 0);
                cv_text8(320 - text8_w("THE FUTURE", 4) / 2, 150, "THE FUTURE", C_HEAD, 4);
                fin = 1;
            }
            k = (uint16_t)(256 - ramp16(u, PHRASE(2) + 6 * 4096, 2 * 4096 - 256));
            pal565(C_HEAD, dim565(RGB(255, 200, 90), k));
        } else {
            while (next < N_CRED && ny - s < 350) {
                cred_draw(next, ny);
                ny += cred_h(creds[next].style);
                next++;
            }
            scroll_b(0, (uint16_t)(s % 360));
        }
        galaxy_pal(k);
        clear8(buf, 0);
        galaxy(buf, T);
        fb_flip();
    }
    vid2(0);
    tclear();
}
