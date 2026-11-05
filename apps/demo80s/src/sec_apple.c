/* Section 1: the Apple II. A 40-column text screen and an Applesoft-style program typed in with key clicks,
 * then hi-res graphics at 280x180 (shown 2x) in the six colours the Apple's hi-res artifacts give: moire line
 * fans drawn by the CPU a few lines a frame, mixed mode (four text lines at the bottom), a bouncing ball, and
 * the one-bit speaker playing a tune. All CPU drawing, as the Apple had no help. */

#define A_BLK 1                                 /* palette: index 0 is transparent, so black is 1 */
#define A_WHT 2
#define A_GRN 3
#define A_VIO 4
#define A_ORA 5
#define A_BLU 6
#define A_X0 40                                 /* hi-res: 280 x 180, 2x, from x 40 */

static uint32_t a_buf;

static void a_pal(uint16_t k)                   /* k/256 brightness (fades) */
{
    static const uint16_t c[7] = {0, RGB(0, 0, 0), RGB(255, 255, 255), RGB(20, 245, 60), RGB(255, 68, 253),
                                  RGB(255, 106, 60), RGB(20, 207, 253)};
    uint8_t i;
    for (i = 1; i < 7; i++) pal565(i, dim565(c[i], k));
}

/* 40-column text: the font 2x wide; row r at y = 12 + 14 r */
static void a_text(int col, int row, const char *s, uint8_t c)
{
    text8(a_buf, col * 16, 12 + row * 14, s, c, A_BLK, 2, 1);
}
static void a_cursor(int col, int row, uint8_t on)
{
    rect8(a_buf, col * 16, 12 + row * 14, 16, 14, on ? A_WHT : A_BLK);
}

static void a_plot(int x, int y, uint8_t c)     /* a hi-res pixel: 2 x 2 */
{
    if ((unsigned)x < 280 && (unsigned)y < 180) {
        volatile uint16_t *p = (volatile uint16_t *)(a_buf + (uint32_t)(2 * y) * W + (uint32_t)(A_X0 + 2 * x));
        uint16_t cc = (uint16_t)(c | c << 8);
        p[0] = cc;
        p[W / 2] = cc;
    }
}
static void a_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        a_plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
    }
}
/* the ball: what was under it is saved first and put back after (shape tables drew with XOR) */
#define BR 9
static uint8_t a_under[(2 * BR + 1) * (2 * BR + 1)];
static uint8_t a_get(int x, int y)
{
    return ((unsigned)x < 280 && (unsigned)y < 180) ? *(volatile uint8_t *)(a_buf + (uint32_t)(2 * y) * W + (uint32_t)(A_X0 + 2 * x)) : A_BLK;
}
static void a_ball(int x, int y, uint8_t c, uint8_t restore)
{
    int i, j, k = 0;
    for (j = -BR; j <= BR; j++)
        for (i = -BR; i <= BR; i++, k++)
            if (i * i + j * j <= BR * BR) {
                if (restore) a_plot(x + i, y + j, a_under[k]);
                else { a_under[k] = a_get(x + i, y + j); a_plot(x + i, y + j, c); }
            }
}

static const char *const a_prog[] = {
    "]10 HGR",
    "]20 FOR I = 0 TO 279 STEP 4",
    "]30 HCOLOR= 1 + I / 4 - INT (I / 20) * 5",
    "]40 HPLOT 0,0 TO I,179: NEXT",
    "]RUN",
};

static void sec_apple(void)
{
    uint32_t t0 = ticks50;
    int row, k, fan = 0, i = 0;
    int bx = 140, by = 40, vx = 3, vy = 0, obx = -1, oby = -1;
    screen_reset();
    a_buf = R0;
    clear8(a_buf, A_BLK);
    blt_wait();
    a_pal(256);
    set_pfa(a_buf);
    vid(VC_BMP | VC_HI);

    /* the text screen, and the program typed in */
    a_text(13, 0, "T1 ][  TRIBUTE", A_WHT);
    a_text(0, 2, "]", A_WHT);
    for (row = 0; row < 5; row++) {
        const char *s = a_prog[row];
        int n = strlen16(s);
        for (k = 1; k <= n; k++) {
            char one[2] = {s[k - 1], 0};
            a_text(k - 1, 2 + row, one, A_WHT);
            a_cursor(k, 2 + row, 1);
            if (s[k - 1] != ' ' && k > 1) click();
            vsync(); vsync(); vsync();
            a_cursor(k, 2 + row, 0);
        }
        for (k = 0; k < 10; k++) vsync();
    }
    for (k = 0; k < 20; k++) vsync();

    /* HGR: the screen clears to black, the speaker starts, the fans draw */
    clear8(a_buf, A_BLK);
    beep_start();
    while ((uint32_t)(ticks50 - t0) < 1150) {
        uint32_t t = ticks50 - t0;
        if (fan < 4) {
            for (k = 0; k < 2 && fan < 4; k++) {
                static const uint8_t fc[4] = {A_GRN, A_VIO, A_ORA, A_BLU};
                uint8_t c = fc[fan];
                switch (fan) {
                case 0: a_line(0, 0, i, 179, c); break;
                case 1: a_line(279, 0, 279 - i, 179, c); break;
                case 2: a_line(0, 179, i, 0, c); break;
                default: a_line(279, 179, 279 - i, 0, c); break;
                }
                i += 6;
                if (i >= 280) { i = 0; fan++; }
            }
            if (fan == 4) {                     /* mixed mode: four text lines at the bottom */
                rect8(a_buf, 0, 304, W, 56, A_BLK);
                a_text(1, 21, "280 X 192, SIX COLOURS, ONE-BIT SPEAKER", A_WHT);
                a_text(1, 22, "6502 AT 1 MHZ. EVERY PIXEL BY THE CPU.", A_GRN);
                a_text(1, 23, "]_", A_WHT);
            }
        } else {
            /* a ball bouncing over the fans (drawn in white, erased in black: the Apple had no sprites) */
            if (obx >= 0) a_ball(obx, oby, 0, 1);
            vy++;
            bx += vx; by += vy;
            if (bx < 10 || bx > 269) { vx = -vx; bx += vx; }
            if (by > 140) { by = 140; vy = -vy * 9 / 10 - 1; }
            a_ball(bx, by, A_WHT, 0);
            obx = bx; oby = by;
        }
        if (t > 1100) a_pal((uint16_t)((1150 - t) * 256 / 50));   /* fade out */
        vsync();
    }
    b_on = 0;
    v_off(12);
}
