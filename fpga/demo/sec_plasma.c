/* Section 3: plasma.
 *
 * Low video tier (the Spartan-3, 16 direct colours): computed again as it
 * moves, in cells of 8x4 pixels (80 x 90 of them), each an ordered dither
 * of two neighbours on a ring of 8 colours (64 shades, each 4 rows of 4
 * bytes, one 32-bit store a row). The sum of three moving sine waves picks
 * the shade, and adding the time to it cycles the colours as a palette
 * would. A whole screen is 115 KB of stores, more than T1 (a byte a step)
 * can write in a frame, so each frame redraws every fourth row of cells,
 * interlaced, in the one bitmap shown (60 frames a second, each part of
 * the plasma new 15 times a second).
 *
 * High video tier (the Tang, -DTANG): the 640x360 plasma is computed once,
 * a byte a pixel through the 256-colour palette; what moves is palette
 * cycling, the hardware scroll and 96 3D stars (drawn as index 0, so the
 * white backdrop shows through). */

static const char pl_msg[] =
    "                                                                                "
#ifdef TANG
    "*** fpga_cpu on a Tang Nano 20K: the high video tier, 256 colours from a palette on HDMI ...   "
    "Every pixel of the 640x360 plasma was computed by T1 at start-up; what moves now is "
    "palette cycling, hardware scroll, 96 real-time 3D stars and this text layer ...   "
#else
    "*** fpga_cpu on a Spartan-3 Starter Board: the low video tier, 16 colours, 4 bits a pixel ...   "
    "There is no palette, so this plasma is computed again every frame: 7,200 cells of 8x4 "
    "pixels, each a dither of two neighbouring colours, the colours cycling as the waves move ...   "
#endif
    "Greetings to the demoscene ...   ";

/* the scroller on text row 22, a character every 4 frames */
static uint16_t pl_pos;
static void pl_scroller(uint16_t frame)
{
    if (frame & 3) return;
    volatile uint16_t *t = TEXT + 22 * 80;
    for (int i = 0; i < 80; i++) {
        uint16_t k = (uint16_t)(pl_pos + i);
        while (k >= sizeof pl_msg - 1) k -= sizeof pl_msg - 1;
        t[i] = (uint16_t)(ATTR(YELLOW, BLACK) << 8 | (uint8_t)pl_msg[k]);
    }
    if (++pl_pos >= sizeof pl_msg - 1) pl_pos = 0;
}

#ifndef TANG
/* ===================================================================== low tier */
static const uint8_t pl_ring[8] = { BLUE, MAGENTA, RED, YELLOW, WHITE, CYAN, GREEN, TEAL };
static const uint8_t pl_bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static uint32_t pl_pat[64][4];                 /* shade: 4 rows of 8 pixels */
static int8_t   pl_cp[80], pl_rp[90], pl_dg[170];

static void pl_setup(void)
{
    for (int s = 0; s < 64; s++) {
        uint8_t a = pl_ring[s >> 3], b = pl_ring[((s >> 3) + 1) & 7];
        int f = s & 7;                           /* eighths of b */
        for (int r = 0; r < 4; r++) {
            uint8_t px[8];
            for (int x = 0; x < 8; x++)
                px[x] = (pl_bayer[r][x & 3] >> 1) < f ? b : a;
            /* bytes in memory order: pixel 0 high nibble of byte 0 */
            uint8_t b0 = (uint8_t)(px[0] << 4 | px[1]), b1 = (uint8_t)(px[2] << 4 | px[3]);
            uint8_t b2 = (uint8_t)(px[4] << 4 | px[5]), b3 = (uint8_t)(px[6] << 4 | px[7]);
            pl_pat[s][r] = (uint32_t)b0 | (uint32_t)b1 << 8 | (uint32_t)b2 << 16 | (uint32_t)b3 << 24;
        }
    }
}

static void pl_draw(uint8_t t, int phase, int step)
{
    for (int x = 0; x < 80; x++)
        pl_cp[x] = (int8_t)((SIN(x * 6 + t * 2) + SIN(x * 3 - t)) >> 1);
    for (int y = 0; y < 90; y++)
        pl_rp[y] = (int8_t)((SIN(y * 5 - t * 3) + SIN(y * 2 + t)) >> 1);
    for (int k = 0; k < 170; k++)
        pl_dg[k] = SIN(k * 4 + t * 2);
    uint8_t ph = (uint8_t)(t >> 1);
    for (int y = phase; y < 90; y += step) {
        uint32_t *p = (uint32_t *)rowp(y * 4);
        int16_t r = (int16_t)(pl_rp[y] + 4 * 64 + (ph << 2));
        const int8_t *dg = pl_dg + y;
        const int8_t *cp = pl_cp;
        const uint32_t *pat = &pl_pat[0][0];
        for (int x = 0; x < 80; x++, p++) {
            uint16_t s = (uint16_t)(((int16_t)(*cp++ + r + *dg++) << 2) & 0x3F0);
            const uint32_t *q = (const uint32_t *)((const uint8_t *)pat + s);
            uint32_t a = q[0], b = q[1];
            p[0] = a; p[80] = b;
            a = q[2]; b = q[3];
            p[160] = a; p[240] = b;
        }
    }
}

static void pl_run(int32_t frames)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    uint16_t t = 0, fc = 0;
    int phase = 0;
    pl_pos = 0;
    gfx_target(BUF0);
    pl_draw(0, 0, 1);
    frame_wait();
    gfx_show(BUF0);
    sec_begin("plasma (16 colours)", frames);
    while (sec_running()) {
        pl_draw((uint8_t)(t >> 1), phase, 4);
        phase = (phase + 1) & 3;
        uint16_t n = frame_wait();
        t += n;
        for (uint16_t i = 0; i < n; i++) pl_scroller(fc++);
    }
}

#else
/* ===================================================================== high tier */
/* 230,400 bytes above the first megabyte, where no section draws: it is
 * computed once at start-up, and the platform game (BUF3) and the shooter
 * (BUF2) run before it every time round the loop */
#define PL_ADDR 0x100000UL
#define PL_BMP  ((uint8_t *)PL_ADDR)
static uint16_t pl_pal[256];
static int8_t   pl_colx[W];
static uint8_t  pl_colu[W];
static uint8_t  pl_rec[256];

static void pl_setup(void)
{
    for (int i = 0; i < 256; i++) {
        unsigned r = (unsigned)(SIN(i) + 128) >> 3;
        unsigned g = (unsigned)(SIN(i + 85) + 128) >> 2;
        unsigned b = (unsigned)(SIN(i + 170) + 128) >> 3;
        pl_pal[i] = (uint16_t)(r << 11 | g << 5 | b);
    }
    for (int z = 0; z < 256; z++)
        pl_rec[z] = z < 17 ? 255 : (uint8_t)(4096 / z);
    tcenter(11, " fpga_cpu demo - computing the plasma ", ATTR(WHITE, BLUE));
    VID_CTRL = VC_TEXT;
    /* every term repeats a whole number of times across 640 and 360, so
     * the hardware scroll wraps it without a seam */
    for (int x = 0; x < W; x++) {
        pl_colx[x] = SIN(x * 2);
        pl_colu[x] = (uint8_t)((uint32_t)x * 256 / W);
    }
    uint8_t *p = PL_BMP;
    for (int y = 0; y < H; y++) {
        uint8_t v1 = (uint8_t)((uint32_t)y * 256 / H);
        int16_t ry = SIN(v1 * 3);
        uint8_t t = (uint8_t)(v1 * 2);
        for (int x = 0; x < W; x++) {
            uint8_t u = pl_colu[x];
            int16_t v = pl_colx[x] + ry + SIN(u * 2 + t) + SIN(u * 3 - v1);
            *p++ = (uint8_t)(((v + 512) >> 2) | 1);     /* never 0: 0 is see-through */
        }
        if ((y & 7) == 7) {
            int done = (y + 1) * 40 / H;
            tfill(13, 20, done, 0xDB, ATTR(WHITE, GREEN));
            LEDS = (uint16_t)(0xFF >> (7 - (y * 8 / H)));
        }
    }
    tclear();
}

/* stars, as index 0 over the plasma; what they covered is put back */
#define NSTARS 96
#define SPEED  5
static int8_t   pl_sx[NSTARS], pl_sy[NSTARS];
static uint8_t  pl_sz[NSTARS], pl_nsaved[NSTARS];
static uint8_t *pl_saved_at[NSTARS];
static uint8_t  pl_saved[NSTARS][4];

static void pl_star_new(int i, uint8_t z)
{
    uint16_t r = rnd16();
    pl_sx[i] = (int8_t)(r >> 8);
    pl_sy[i] = (int8_t)r;
    pl_sz[i] = z;
}

static void pl_stars(uint16_t scx, uint16_t scy)
{
    for (int i = NSTARS - 1; i >= 0; i--) {
        uint8_t *a = pl_saved_at[i];
        uint8_t n = pl_nsaved[i];
        if (n) {
            a[0] = pl_saved[i][0];
            if (n > 1) { a[1] = pl_saved[i][1]; a[W] = pl_saved[i][2]; a[W + 1] = pl_saved[i][3]; }
            pl_nsaved[i] = 0;
        }
    }
    for (int i = 0; i < NSTARS; i++) {
        uint8_t z = pl_sz[i];
        if (z < 20 + SPEED) { pl_star_new(i, 255); continue; }
        z = (uint8_t)(z - SPEED);
        pl_sz[i] = z;
        int16_t px = (int16_t)((pl_sx[i] * pl_rec[z]) >> 5);
        int16_t py = (int16_t)((pl_sy[i] * pl_rec[z]) >> 5);
        if (px < -320 || px > 318 || py < -180 || py > 178) { pl_star_new(i, 255); continue; }
        uint16_t bx = (uint16_t)(320 + px + scx), by = (uint16_t)(180 + py + scy);
        if (bx >= W) bx -= W;
        if (by >= H) by -= H;
        if (bx == W - 1 || by == H - 1) continue;
        uint8_t *a = PL_BMP + (uint32_t)by * W + bx;
        pl_saved_at[i] = a;
        pl_saved[i][0] = a[0];
        a[0] = 0;
        uint8_t n = 1;
        if (z < 90) {
            pl_saved[i][1] = a[1]; pl_saved[i][2] = a[W]; pl_saved[i][3] = a[W + 1];
            a[1] = 0; a[W] = 0; a[W + 1] = 0;
            n = 2;
        }
        pl_nsaved[i] = n;
    }
}

static void pl_run(int32_t frames)
{
    tclear();
    PAL_INDEX = 0;
    for (int i = 0; i < 256; i++) PAL_DATA = pl_pal[i];
    for (int i = 0; i < NSTARS; i++) { pl_star_new(i, (uint8_t)(20 + rnd16() % 236)); pl_nsaved[i] = 0; }
    BACKDROP = WHITE;
    gfx_show(PL_ADDR);
    VID_CTRL = VC_TEXT | VC_BMP | VC_HICOL;
    uint8_t phase = 0;
    uint16_t fc = 0;
    pl_pos = 0;
    sec_begin("plasma (256 colours)", frames);
    while (sec_running()) {
        uint16_t n = frame_wait();
        /* scroll first, still in the vertical blank */
        uint16_t scx = (uint16_t)(W + (SIN(phase) >> 2));
        uint16_t scy = (uint16_t)(H + (SIN(phase * 2 + 64) >> 3));
        SCROLLX = scx;
        SCROLLY = scy;
        PAL_INDEX = 1;
        for (uint16_t i = 1; i < 256; i++)
            PAL_DATA = pl_pal[(uint8_t)(i + phase)];
        pl_stars(scx % W, scy % H);
        for (uint16_t i = 0; i < n; i++) pl_scroller(fc++);
        phase = (uint8_t)(phase + n);
    }
    /* put the plasma back as it was, for next time */
    for (int i = NSTARS - 1; i >= 0; i--)
        if (pl_nsaved[i]) {
            uint8_t *a = pl_saved_at[i];
            a[0] = pl_saved[i][0];
            if (pl_nsaved[i] > 1) { a[1] = pl_saved[i][1]; a[W] = pl_saved[i][2]; a[W + 1] = pl_saved[i][3]; }
            pl_nsaved[i] = 0;
        }
    SCROLLX = 0;
    SCROLLY = 0;
    BACKDROP = BLACK;
}
#endif
