/* fpga_cpu demo: a full-resolution (640x360, 256-color) plasma with
 * palette cycling, hardware scroll, a real-time starfield, and the text
 * layer for a title, a scroller and live CPU statistics.
 *
 * Runs on the T1 core in fpga/rtl/system.v (10 MHz, ~2 MIPS after video
 * DMA), on the Tang Nano 20K (tang/rtl/system.v, build with -DTANG: T1 at
 * 16.875 MHz, the screen in SDRAM) and in the simulator. The plasma is computed once, pixel by pixel;
 * everything that moves afterwards is what an old demo would do: rotate
 * the palette, wobble the scroll registers, redraw 96 stars, and keep the
 * text up to date, all inside one 60 Hz frame.
 *
 * Build: llvm/fcc -O2 -o demo.bin fpga/demo/demo.c
 * (int is 16 bits: products that can exceed 32767 use 32-bit types.) */
#include <stdint.h>
#include <fcpu.h>

#define REG(a)      IO16(a)
#define VID_CTRL    REG(0xF00200)
#define BACKDROP    REG(0xF00202)
#define PAL_INDEX   REG(0xF00210)
#define PAL_DATA    REG(0xF00212)
#define SCROLLX     REG(0xF00214)
#define SCROLLY     REG(0xF00216)
#define SEG7        REG(0xF00606)
#define LEDS        REG(0xF00600)
#define SWITCHES    REG(0xF00602)

#ifdef TANG
#define T1_KHZ 16875                    /* CYCLE's rate: T1 periods */
#else
#define T1_KHZ 10000
#endif

#define BMP  ((volatile uint8_t *)0x0C3C00)
#define TEXT ((volatile uint16_t *)0x0FC000)
#define W 640
#define H 360

/* text attributes: [7] stipple, [6] transparent background, [5:3] bg, [2:0] fg */
#define A_TITLE   0x0F          /* white on blue */
#define A_SUB     0x83          /* cyan on stippled black */
#define A_STATS   0x87          /* white on stippled black */
#define A_VALUE   0x86          /* yellow on stippled black */
#define A_SCROLL  0x86          /* yellow on stippled black */

static const int8_t sintab[256] = {
       0,    3,    6,    9,   12,   16,   19,   22,   25,   28,   31,   34,   37,   40,   43,   46,
      49,   51,   54,   57,   60,   63,   65,   68,   71,   73,   76,   78,   81,   83,   85,   88,
      90,   92,   94,   96,   98,  100,  102,  104,  106,  107,  109,  111,  112,  113,  115,  116,
     117,  118,  120,  121,  122,  122,  123,  124,  125,  125,  126,  126,  126,  127,  127,  127,
     127,  127,  127,  127,  126,  126,  126,  125,  125,  124,  123,  122,  122,  121,  120,  118,
     117,  116,  115,  113,  112,  111,  109,  107,  106,  104,  102,  100,   98,   96,   94,   92,
      90,   88,   85,   83,   81,   78,   76,   73,   71,   68,   65,   63,   60,   57,   54,   51,
      49,   46,   43,   40,   37,   34,   31,   28,   25,   22,   19,   16,   12,    9,    6,    3,
       0,   -3,   -6,   -9,  -12,  -16,  -19,  -22,  -25,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
     -49,  -51,  -54,  -57,  -60,  -63,  -65,  -68,  -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
     -90,  -92,  -94,  -96,  -98, -100, -102, -104, -106, -107, -109, -111, -112, -113, -115, -116,
    -117, -118, -120, -121, -122, -122, -123, -124, -125, -125, -126, -126, -126, -127, -127, -127,
    -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -122, -121, -120, -118,
    -117, -116, -115, -113, -112, -111, -109, -107, -106, -104, -102, -100,  -98,  -96,  -94,  -92,
     -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,  -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
     -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3
};

static int8_t   colx[W];        /* the plasma's x terms */
static uint8_t  colu[W];
static uint16_t pal[256];       /* the color wheel, RGB565 */
static uint8_t  rec[256];       /* 4096 / z for the star projection */

static inline uint16_t sr3(void)
{
    uint16_t v;
    __asm__ volatile("MFSR %0, sr3" : "=r"(v));   /* instructions retired, low 16 bits */
    return v;
}

static void put(int row, int col, const char *s, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s)
        *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

static void putnum(int row, int col, uint16_t v, int digits, uint16_t attr)
{
    char b[6];
    b[digits] = 0;
    for (int i = digits - 1; i >= 0; i--, v /= 10)
        b[i] = (char)('0' + v % 10);
    put(row, col, b, attr);
}

static void clear_text(void)
{
    for (int i = 0; i < 25 * 80; i++)
        TEXT[i] = 0x4020;        /* transparent spaces */
}

static void center(int row, const char *s, uint16_t attr)
{
    int n = 0;
    while (s[n]) n++;
    put(row, (80 - n) / 2, s, attr);
}

/* ---- the plasma, computed once ------------------------------------------
 * Every term repeats a whole number of times across 640 and across 360, so
 * the hardware scroll wraps it without a seam. */
static void plasma(void)
{
    for (int x = 0; x < W; x++) {
        colx[x] = sintab[(uint8_t)(x * 2)];                /* 5 periods across */
        colu[x] = (uint8_t)((uint32_t)x * 256 / W);        /* 1 period across */
    }
    volatile uint8_t *p = BMP;
    for (int y = 0; y < H; y++) {
        uint8_t v1 = (uint8_t)((uint32_t)y * 256 / H);     /* 1 period down */
        int16_t ry = sintab[(uint8_t)(v1 * 3)];
        uint8_t t = (uint8_t)(v1 * 2);
        for (int x = 0; x < W; x++) {
            uint8_t u = colu[x];
            int16_t v = colx[x] + ry + sintab[(uint8_t)(u * 2 + t)] + sintab[(uint8_t)(u * 3 - v1)];
            *p++ = (uint8_t)(((v + 512) >> 2) | 1);     /* never 0: 0 is see-through */
        }
        if ((y & 7) == 7) {      /* progress bar */
            volatile uint16_t *bar = TEXT + 13 * 80 + 20;
            int done = (y + 1) * 40 / H;
            for (int i = 0; i < 40; i++)
                bar[i] = (uint16_t)((i < done ? 0x2F : 0x4F) << 8 | (i < done ? 0xDB : 0xB0));
            LEDS = (uint16_t)(0xFF >> (7 - (y * 8 / H)));
        }
    }
}

static void make_palette(void)
{
    for (int i = 0; i < 256; i++) {
        unsigned r = (unsigned)(sintab[(uint8_t)i] + 128) >> 3;
        unsigned g = (unsigned)(sintab[(uint8_t)(i + 85)] + 128) >> 2;
        unsigned b = (unsigned)(sintab[(uint8_t)(i + 170)] + 128) >> 3;
        pal[i] = (uint16_t)(r << 11 | g << 5 | b);
    }
    for (int z = 0; z < 256; z++)
        rec[z] = z < 17 ? 255 : (uint8_t)(4096 / z);
}

/* ---- stars: drawn into the bitmap as index 0, so the (white) backdrop
 * shows through them whatever the palette is doing. Each moves 5 units of
 * depth a frame (just under a second from the back to the screen edge);
 * near ones are drawn 2x2. The plasma under them is saved and put back. */
#define NSTARS 96
#define SPEED  5
static int8_t   sx[NSTARS], sy[NSTARS];
static uint8_t  sz[NSTARS];
static uint8_t  nsaved[NSTARS];
static volatile uint8_t *saved_at[NSTARS];
static uint8_t  saved[NSTARS][4];
static uint16_t rnd = 0xACE1;

static uint16_t random16(void)   /* xorshift */
{
    rnd ^= (uint16_t)(rnd << 7);
    rnd ^= (uint16_t)(rnd >> 9);
    rnd ^= (uint16_t)(rnd << 8);
    return rnd;
}

static void star_new(int i, uint8_t z)
{
    uint16_t r = random16();
    sx[i] = (int8_t)(r >> 8);
    sy[i] = (int8_t)r;
    sz[i] = z;
}

static void stars(uint16_t scx, uint16_t scy)
{
    /* Put the plasma back, last-drawn star first: where stars overlap, a
     * later one saved an earlier one's white pixel, and undoing the saves in
     * reverse order leaves the plasma, not white. */
    for (int i = NSTARS - 1; i >= 0; i--) {
        volatile uint8_t *a = saved_at[i];
        uint8_t n = nsaved[i];
        if (n) {
            a[0] = saved[i][0];
            if (n > 1) { a[1] = saved[i][1]; a[W] = saved[i][2]; a[W + 1] = saved[i][3]; }
            nsaved[i] = 0;
        }
    }
    for (int i = 0; i < NSTARS; i++) {
        volatile uint8_t *a;
        uint8_t n;
        uint8_t z = sz[i];
        if (z < 20 + SPEED) { star_new(i, 255); continue; }
        z = (uint8_t)(z - SPEED);
        sz[i] = z;
        int16_t px = (int16_t)((sx[i] * rec[z]) >> 5);
        int16_t py = (int16_t)((sy[i] * rec[z]) >> 5);
        if (px < -320 || px > 318 || py < -180 || py > 178) { star_new(i, 255); continue; }
        /* screen position, plus the scroll, is where it lands in the bitmap */
        uint16_t bx = (uint16_t)(320 + px + scx), by = (uint16_t)(180 + py + scy);
        if (bx >= W) bx -= W;
        if (by >= H) by -= H;
        if (bx == W - 1 || by == H - 1) continue;  /* keep 2x2 inside the buffer */
        a = BMP + (uint32_t)by * W + bx;
        saved_at[i] = a;
        saved[i][0] = a[0];
        a[0] = 0;
        n = 1;
        if (z < 90) {
            saved[i][1] = a[1]; saved[i][2] = a[W]; saved[i][3] = a[W + 1];
            a[1] = 0; a[W] = 0; a[W + 1] = 0;
            n = 2;
        }
        nsaved[i] = n;
    }
}

/* ---- the scroller ---------------------------------------------------------- */
static const char msg[] =
    "                                                                                "
#ifdef TANG
    "*** fpga_cpu on a Tang Nano 20K ***   "
    "The T1 core runs these 60 frames a second at 16.875 MHz, the screen in the SDRAM "
    "inside the FPGA's package, shown on HDMI in true color ...   "
#else
    "*** fpga_cpu on a Spartan-3 Starter Board ***   "
    "The T1 core runs these 60 frames a second at 10 MHz, from the board's SRAM, "
    "sharing it with the video DMA ...   "
#endif
    "Every pixel of the 640x360 plasma was computed by T1 at start-up; what moves now is "
    "palette cycling, hardware scroll, 96 real-time 3D stars and this text layer ...   "
    "Greetings to the demoscene ...   ";

int main(void)
{
    clear_text();
    BACKDROP = 7;                            /* white: the stars */
    VID_CTRL = 0x01;                         /* text only while computing */
    center(11, " fpga_cpu demo - computing the plasma ", A_TITLE);
    make_palette();
    PAL_INDEX = 0;
    for (int i = 0; i < 256; i++) PAL_DATA = pal[i];
    plasma();

    clear_text();
    put(0, 0, "                                 fpga_cpu  demo                                 ", A_TITLE);
    center(1, "T1 core * 640x360 in 256 colors * text layer * palette * scroll * 3D stars", A_SUB);
    for (int i = 0; i < 80; i++) TEXT[22 * 80 + i] = (uint16_t)(A_STATS << 8 | ' ');
    put(22, 2, "fps", A_STATS);
    put(22, 12, "CPU busy", A_STATS);
    put(22, 25, "%", A_STATS);
    put(22, 30, "MIPS", A_STATS);
    put(22, 44, "T1 clocks per instruction", A_STATS);
    for (int i = 0; i < NSTARS; i++) star_new(i, (uint8_t)(20 + random16() % 236));
    for (int i = 0; i < 80; i++) TEXT[24 * 80 + i] = (uint16_t)(A_SCROLL << 8 | ' ');
    VID_CTRL = 0x07;                         /* text, bitmap, hi-color */

    uint8_t phase = 0;
    uint16_t pos = 0, frames = 0, sec_frames = 0;
    uint32_t sec_insts = 0, sec_busy = 0, sec_total = 0;
    uint16_t last_frame = FRAME;
    uint32_t last_cycle = CYCLE;
    for (;;) {
        /* wait for the next frame */
        uint32_t idle_from = CYCLE;
        while (FRAME == last_frame) ;
        uint16_t now = FRAME;
        uint16_t i0 = sr3();
        uint32_t t0 = CYCLE;
        sec_frames += (uint16_t)(now - last_frame);
        last_frame = now;

        /* palette cycling: indices 1-255 (0 is see-through) */
        PAL_INDEX = 1;
        for (uint16_t i = 1; i < 256; i++)
            PAL_DATA = pal[(uint8_t)(i + phase)];

        /* hardware scroll wobble */
        uint16_t scx = (uint16_t)(W + (sintab[phase] >> 2));
        uint16_t scy = (uint16_t)(H + (sintab[(uint8_t)(phase * 2 + 64)] >> 3));
        SCROLLX = scx;
        SCROLLY = scy;
        stars(scx % W, scy % H);

        /* the scroller, a character every 4 frames */
        if ((frames & 3) == 0) {
            volatile uint16_t *t = TEXT + 24 * 80;
            for (int i = 0; i < 80; i++) {
                uint16_t k = (uint16_t)(pos + i);
                while (k >= sizeof msg - 1) k -= sizeof msg - 1;
                t[i] = (uint16_t)(A_SCROLL << 8 | (uint8_t)msg[k]);
            }
            if (++pos >= sizeof msg - 1) pos = 0;
        }

        phase++;
        frames++;
        SEG7 = frames;
        LEDS = (uint16_t)(1u << ((frames >> 3) & 7));

        uint32_t t1 = CYCLE;
        sec_insts += (uint16_t)(sr3() - i0);
        sec_busy  += t1 - t0;
        sec_total += t1 - last_cycle;
        last_cycle = t1;
        (void)idle_from;

        /* once a second: the statistics */
        if (sec_frames >= 60) {
            putnum(22, 6, sec_frames, 2, A_VALUE);
            putnum(22, 21, (uint16_t)(sec_busy * 100 / sec_total), 3, A_VALUE);
            /* MIPS while working: instructions per busy CYCLE tick, at
             * T1_KHZ (CYCLE counts T1 clocks, on the Tang T1 periods of
             * 4 x 67.5 MHz; fsim counts 50 MHz cycles, so there this reads
             * low) */
            uint32_t mips100 = sec_insts * (T1_KHZ / 10) / (sec_busy ? sec_busy : 1);
            putnum(22, 35, (uint16_t)(mips100 / 100), 1, A_VALUE);
            put(22, 36, ".", A_VALUE);
            putnum(22, 37, (uint16_t)(mips100 % 100), 2, A_VALUE);
            uint32_t cpi100 = sec_busy * 100 / (sec_insts ? sec_insts : 1);
            putnum(22, 70, (uint16_t)(cpi100 / 100), 1, A_VALUE);
            put(22, 71, ".", A_VALUE);
            putnum(22, 72, (uint16_t)(cpi100 % 100), 2, A_VALUE);
            sec_frames = 0; sec_insts = 0; sec_busy = 0; sec_total = 0;
        }
#ifdef FSIM
        if (frames == 70) return 0;
#endif
    }
}
