/* Section 2: the Commodore 64. A BASIC start-up screen (a parody banner) in the C64's colours, LOAD typed in
 * and the tape-loading stripes in the border, then a demo: raster bars, a colour-cycled logo, eight
 * hardware sprites on Lissajous paths, a smooth scroller, and the three-voice SID tune.
 *
 * Every two lines of the background have their own palette entry (64 + y/2), so the border stripes and the
 * raster bars are palette writes only, as a C64 changes colours on the raster line. */

#define C_PAL 16                                /* C64 colour c is palette 16 + c (Pepto's) */
#define C_LOGO 32                               /* 32-39: the logo's rows, cycling */
#define C_SCR 40                                /* 40-53: the scroller's rows */
#define C_ROW 64                                /* 64-243: the background, two lines each */
#define C_SX 80                                 /* the screen inside the border: 480 x 280 at (80, 40) */
#define C_SY 40
#define C_STRIP (SCR_FREE + 0x1000UL)           /* the scroller's text, pre-drawn: STRIP_W x 28 */
#define STRIP_W 2400
#define C_SPRITE (SCR_FREE)                     /* the sprite image: 32 x 21 */

static const uint16_t c64[16] = {
    RGB(0, 0, 0), RGB(255, 255, 255), RGB(104, 55, 43), RGB(112, 164, 178), RGB(111, 61, 134), RGB(88, 141, 67),
    RGB(53, 40, 121), RGB(184, 199, 111), RGB(111, 79, 37), RGB(67, 57, 0), RGB(154, 103, 89), RGB(68, 68, 68),
    RGB(108, 108, 108), RGB(154, 210, 132), RGB(108, 94, 181), RGB(149, 149, 149)};
static uint16_t c_rows[180];                    /* the background's colours, two lines each */
static uint32_t c_buf;

/* text in the C64's proportions: the 8-pixel font 1.5x wide (12 pixels), 14 lines; on the 40 x 20 screen */
static void c_text(int col, int row, const char *s, uint8_t fg, uint8_t bg)
{
    for (; *s; s++, col++) {
        const uint8_t *g = font8x14[(uint8_t)*s];
        int x0 = C_SX + col * 12, y0 = C_SY + row * 14, r, b;
        for (r = 0; r < 14; r++) {
            int x = x0;
            for (b = 0; b < 8; b++) {
                uint8_t c = ((g[r] >> (7 - b)) & 1) ? fg : bg;
                pset8(c_buf, x++, y0 + r, c);
                if (!(b & 1)) pset8(c_buf, x++, y0 + r, c);
            }
        }
        poll();
    }
}
static void c_rows_all(uint16_t c)
{
    uint8_t i;
    for (i = 0; i < 180; i++) c_rows[i] = c;
}
static void c_rows_show(void) { pal_run(C_ROW, c_rows, 180); }

/* the background: each two-line band in its own entry */
static void c_background(void)
{
    uint16_t y;
    for (y = 0; y < 180; y++) blt_fill(c_buf + (uint32_t)y * 2 * W, W, W, 2, (uint8_t)(C_ROW + y));
    blt_wait();
}

/* a block letter logo: each font pixel a 9 x 3 block with a gap, colour by row */
static void c_logo(const char *s, int x0, int y0)
{
    int n = 0;
    for (; *s; s++, n++) {
        const uint8_t *g = font8x14[(uint8_t)*s];
        int r, b;
        for (r = 1; r < 13; r++)
            for (b = 0; b < 8; b++)
                if ((g[r] >> (7 - b)) & 1)
                    rect8(c_buf, x0 + n * 76 + b * 9, y0 + r * 5, 8, 4, (uint8_t)(C_LOGO + (r & 7)));
        poll();
    }
}

/* the sprite: a UFO, 32 x 21, in C64 colours (light grey hull, cyan dome, lights 1/7/2 cycling) */
static void c_sprite(void)
{
    int x, y;
    for (y = 0; y < 21; y++)
        for (x = 0; x < 32; x++) {
            int dx = x - 16, c = 0;
            if (y < 9 && dx * dx * 9 + (y - 9) * (y - 9) * 16 < 8 * 8 * 9) c = C_PAL + 3;           /* dome */
            if (y >= 8 && y < 15 && dx * dx * 4 + (y - 11) * (y - 11) * 64 < 16 * 16 * 4) c = C_PAL + 15;   /* hull */
            if (y >= 10 && y < 12 && (x % 6) == 3) c = C_PAL + 1 + (x / 6) % 3 * 6 % 15;        /* lights */
            if (y >= 15 && y < 18 && (dx == -8 || dx == 8 || dx == 0)) c = C_PAL + 12;            /* legs */
            if (y == 18 && (dx >= -10 && dx <= -6)) c = C_PAL + 12;
            if (y == 18 && (dx >= 6 && dx <= 10)) c = C_PAL + 12;
            *(volatile uint8_t *)(C_SPRITE + (uint32_t)y * 32 + x) = (uint8_t)c;
        }
}

/* the scroller's strip: the message in the font, 2x, colour by row */
static const char c_msg[] =
    "     HELLO FROM THE T1 ... A C64 HAS EIGHT SPRITES, THREE SID VOICES AND SIXTEEN COLOURS - AND SO DOES "
    "THIS PART. NEXT: SIXTEEN BITS OF AMIGA ...     ";
static void c_strip(void)
{
    int i, n = strlen16(c_msg);
    blt_fill(C_STRIP, STRIP_W, STRIP_W, 28, 0);
    blt_wait();
    for (i = 0; i < n && i * 16 + 16 <= STRIP_W; i++) {
        const uint8_t *g = font8x14[(uint8_t)c_msg[i]];
        int r, b;
        for (r = 0; r < 14; r++)
            for (b = 0; b < 8; b++)
                if ((g[r] >> (7 - b)) & 1) {
                    volatile uint8_t *p = (volatile uint8_t *)(C_STRIP + (uint32_t)(2 * r) * STRIP_W + (uint32_t)(i * 16 + 2 * b));
                    uint8_t c = (uint8_t)(C_SCR + r);
                    p[0] = c; p[1] = c; p[STRIP_W] = c; p[STRIP_W + 1] = c;
                }
        if (!(i & 7)) poll();
    }
}

/* a raster bar: entries around centre c (in two-line bands), from a colour ramp */
static void c_bar(int c, const uint8_t *ramp, int n)
{
    int i;
    for (i = -n + 1; i < n; i++) {
        int b = c + i;
        if (b >= 0 && b < 180) c_rows[b] = c64[ramp[n - 1 - (i < 0 ? -i : i)]];
    }
}

static void sec_c64(void)
{
    uint32_t t0;
    int k;
    screen_reset();
    c_buf = R0;
    c_background();
    rect8(c_buf, C_SX, C_SY, 480, 280, (uint8_t)(C_PAL + 6));
    {
        uint8_t i;
        for (i = 0; i < 16; i++) pal565((uint8_t)(C_PAL + i), c64[i]);
    }
    c_rows_all(c64[14]);
    c_rows_show();
    set_pfa(c_buf);
    vid(VC_BMP | VC_HI);

    /* the start-up screen */
    c_text(4, 1, "**** T1-64 BASIC V2 ****", C_PAL + 14, C_PAL + 6);
    c_text(1, 3, "64K RAM SYSTEM  38911 BASIC BYTES FREE", C_PAL + 14, C_PAL + 6);
    c_text(0, 5, "READY.", C_PAL + 14, C_PAL + 6);
    {
        static const char cmd[] = "LOAD\"FLEX\",8,1";
        char one[2] = {0, 0};
        for (k = 0; cmd[k]; k++) {
            one[0] = cmd[k];
            c_text(k, 6, one, C_PAL + 14, C_PAL + 6);
            rect8(c_buf, C_SX + (k + 1) * 12, C_SY + 6 * 14, 12, 14, C_PAL + 14);
            click();
            vsync(); vsync(); vsync(); vsync();
            rect8(c_buf, C_SX + (k + 1) * 12, C_SY + 6 * 14, 12, 14, C_PAL + 6);
        }
    }
    for (k = 0; k < 20; k++) vsync();
    c_text(0, 8, "SEARCHING FOR FLEX", C_PAL + 14, C_PAL + 6);
    for (k = 0; k < 40; k++) vsync();
    c_text(0, 9, "LOADING", C_PAL + 14, C_PAL + 6);
    c_sprite();
    c_strip();
    /* the loading stripes: bands of random colours in the border's entries, a new set each frame */
    t0 = ticks50;
    while ((uint32_t)(ticks50 - t0) < 160) {
        int y = 0;
        while (y < 180) {
            int h = 1 + (rnd() & 7);
            uint16_t c = c64[rnd() & 15];
            while (h-- && y < 180) c_rows[y++] = c;
        }
        c_rows_show();
        vsync();
    }
    c_rows_all(c64[14]);
    c_rows_show();
    c_text(0, 10, "READY.", C_PAL + 14, C_PAL + 6);
    c_text(0, 11, "RUN", C_PAL + 14, C_PAL + 6);
    for (k = 0; k < 30; k++) vsync();

    /* the demo: the screen opens to the border, the bars, the logo, the sprites, the scroller, the SID */
    c_background();
    c_rows_all(0);
    c_rows_show();
    c_logo("FLEX", 168, 60);
    sid_start(8);
    {
        uint8_t i;
        for (i = 0; i < 8; i++) spr_set(i, 0, -32, 21, C_SPRITE, 2, 0);
        static const uint8_t scr_c[14] = {7, 7, 1, 1, 7, 8, 8, 2, 10, 10, 2, 9, 9, 8};
        for (i = 0; i < 14; i++) pal565((uint8_t)(C_SCR + i), c64[scr_c[i]]);
    }
    vid2(VC2_SPR);
    t0 = ticks50;
    {
        static const uint8_t rb[3][5] = {{6, 14, 3, 13, 1}, {9, 2, 10, 15, 1}, {11, 5, 13, 7, 1}};
        static const uint8_t logo_c[8] = {2, 10, 8, 7, 13, 5, 3, 14};
        uint32_t so = 0;
        while ((uint32_t)(ticks50 - t0) < 1350) {
            uint32_t t = ticks50 - t0;
            uint16_t f = FRAME;
            uint8_t i;
            /* raster bars over a black background */
            c_rows_all(0);
            for (i = 0; i < 3; i++)
                c_bar(90 + SIN(f * 2 + i * 85) * 70 / 128, rb[i], 5);
            c_rows_show();
            /* the logo's rows cycle through a rainbow */
            for (i = 0; i < 8; i++) pal565((uint8_t)(C_LOGO + i), c64[logo_c[(i + (f >> 2)) & 7]]);
            /* sprites: eight UFOs on a Lissajous path; their lights blink */
            for (i = 0; i < 8; i++)
                spr_xy(i, 304 + SIN(f + i * 24) * 250 / 128, 190 + SIN(f * 3 / 2 + i * 32) * 60 / 128, 21);
            pal565(C_PAL + 1, (f & 16) ? c64[1] : c64[7]);
            /* the scroller: 2 pixels a frame, a 640-wide window of the strip */
            {
                uint32_t sw = STRIP_W - 640, o = so % sw;
                blt_copy(C_STRIP + o, STRIP_W, c_buf + 300UL * W, W, W, 28, 0, 0);
                so += 2;
            }
            if (t > 1300) {                     /* fade: everything down */
                uint16_t kk = (uint16_t)((1350 - t) * 256 / 50);
                for (i = 0; i < 16; i++) pal565((uint8_t)(C_PAL + i), dim565(c64[i], kk));
                for (i = 0; i < 14; i++) pal565((uint8_t)(C_SCR + i), 0);
            }
            vsync();
        }
    }
    sid_stop();
    spr_alloff();
}
