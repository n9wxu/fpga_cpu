/* demo80s: the hardware layer. Video (docs/io.md "Video", "Video extensions"), the blitter, sprites, the
 * text layer, palettes, drawing into 8 bpp buffers, and frame pacing. One compilation unit (main.c includes
 * everything), so the small helpers inline. int is 16 bits: offsets and products are 32-bit. */
#include <stdint.h>
#include <string.h>
#include "font.h"                               /* font8x14[256][14] (fpga/demo/font.h) */
#include "demo_tables.h"

#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define FRAME IO(0xA)
#define COLD __attribute__((noinline))

/* memory: the program below 0x40000 (stack top 0x40000), then three 640x360 8 bpp regions and scratch */
#define R0  0x040000UL
#define R1  0x078800UL
#define R2  0x0B1000UL
#define SCR 0x0E9400UL                           /* to 0x0FC000: 76 KB */
#define LSCR SCR                                 /* the line-scroll table, 1440 bytes */
#define SCR_FREE (SCR + 0x600UL)
#define TEXT ((volatile uint16_t *)0x0FC000UL)
#define W 640
#define H 360

void poll(void);                                 /* the music (music.c), called while waiting */

/* ---------------------------------------------------------------- video registers */
#define VC_TEXT 0x01
#define VC_BMP  0x02
#define VC_HI   0x04
#define VC2_PFB 0x01
#define VC2_BFRONT 0x02
#define VC2_LSCR 0x04
#define VC2_SPR 0x08

static void vid(uint16_t c) { IO(0x200) = c; }
static void vid2(uint16_t c) { IO(0x220) = c; }
static void set_pfa(uint32_t a) { IO(0x208) = (uint16_t)a; IO(0x20A) = (uint16_t)(a >> 16); }
static void set_pfb(uint32_t a) { IO(0x224) = (uint16_t)a; IO(0x226) = (uint16_t)(a >> 16); }
static void set_lscr(uint32_t a) { IO(0x22C) = (uint16_t)a; IO(0x22E) = (uint16_t)(a >> 16); }
static void scroll_a(uint16_t x, uint16_t y) { IO(0x214) = x; IO(0x216) = y; }
static void scroll_b(uint16_t x, uint16_t y) { IO(0x228) = x; IO(0x22A) = y; }
static void backdrop(uint8_t c) { IO(0x202) = c; }

static void vsync(void)
{
    uint16_t f = FRAME;
    while (FRAME == f)
        poll();
}
#ifdef FPS
/* -DFPS: frames drawn a second, on the console every 2 s ("A 30" = section A, 30 frames a second) */
static uint16_t fps_n, fps_f0;
static void fps_count(char sec)
{
    uint16_t f = FRAME;
    fps_n++;
    if ((uint16_t)(f - fps_f0) >= 120) {
        uint16_t r = (uint16_t)(fps_n / 2);
        IO(0) = (uint8_t)sec; IO(0) = ' ';
        IO(0) = (uint8_t)('0' + r / 10); IO(0) = (uint8_t)('0' + r % 10); IO(0) = '\n';
        fps_n = 0; fps_f0 = f;
    }
}
#else
#define fps_count(s)
#endif
static void wait_flip(void)
{
    while (IO(0x222) & 1)
        poll();
}

/* ---------------------------------------------------------------- palette */
#define RGB(r, g, b) ((uint16_t)(((uint16_t)(r) >> 3) << 11 | ((uint16_t)(g) >> 2) << 5 | ((uint16_t)(b) >> 3)))
static void pal(uint8_t i, uint8_t r, uint8_t g, uint8_t b) { IO(0x210) = i; IO(0x212) = RGB(r, g, b); }
static void pal565(uint8_t i, uint16_t c) { IO(0x210) = i; IO(0x212) = c; }
static void pal_run(uint8_t i, const uint16_t *c, uint16_t n)  /* PAL_DATA increments the index */
{
    IO(0x210) = i;
    while (n--)
        IO(0x212) = *c++;
}
/* c565 scaled by k/256 */
static uint16_t dim565(uint16_t c, uint16_t k)
{
    uint16_t r = (uint16_t)((c >> 11) * k >> 8), g = (uint16_t)(((c >> 5) & 63) * k >> 8), b = (uint16_t)((c & 31) * k >> 8);
    return (uint16_t)(r << 11 | g << 5 | b);
}
static uint16_t mix565(uint16_t a, uint16_t b, uint16_t k)     /* a..b, k/256 of the way */
{
    int16_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    ar = (int16_t)(ar + (int16_t)(((int32_t)(br - ar) * k) >> 8));
    ag = (int16_t)(ag + (int16_t)(((int32_t)(bg - ag) * k) >> 8));
    ab = (int16_t)(ab + (int16_t)(((int32_t)(bb - ab) * k) >> 8));
    return (uint16_t)(ar << 11 | ag << 5 | ab);
}
static uint16_t hsv565(uint8_t h, uint8_t s, uint8_t v)        /* hue 0-255 */
{
    uint8_t reg = h / 43, rem = (uint8_t)((h - reg * 43) * 6);
    uint8_t p = (uint8_t)((v * (255 - s)) >> 8), q = (uint8_t)((v * (255 - ((s * rem) >> 8))) >> 8),
            t = (uint8_t)((v * (255 - ((s * (255 - rem)) >> 8))) >> 8);
    switch (reg) {
    case 0: return RGB(v, t, p);
    case 1: return RGB(q, v, p);
    case 2: return RGB(p, v, t);
    case 3: return RGB(p, q, v);
    case 4: return RGB(t, p, v);
    default: return RGB(v, p, q);
    }
}

/* ---------------------------------------------------------------- the blitter */
static void blt_wait(void)
{
    while (IO(0x816) & 1)
        ;
}
static void blt_fill(uint32_t dst, uint16_t stride, uint16_t w, uint16_t h, uint8_t v)
{
    blt_wait();
    IO(0x804) = (uint16_t)dst; IO(0x806) = (uint16_t)(dst >> 16);
    IO(0x80A) = stride; IO(0x80C) = w; IO(0x80E) = h; IO(0x812) = v;
    IO(0x814) = 1;
}
static void blt_copy(uint32_t src, uint16_t ss, uint32_t dst, uint16_t ds, uint16_t w, uint16_t h, uint8_t keyed, uint8_t key)
{
    blt_wait();
    IO(0x800) = (uint16_t)src; IO(0x802) = (uint16_t)(src >> 16);
    IO(0x804) = (uint16_t)dst; IO(0x806) = (uint16_t)(dst >> 16);
    IO(0x808) = ss; IO(0x80A) = ds; IO(0x80C) = w; IO(0x80E) = h; IO(0x810) = key;
    IO(0x814) = (uint16_t)(keyed ? 2 : 0);
}
static void clear8(uint32_t buf, uint8_t v) { blt_fill(buf, W, W, H, v); }
static void rect8(uint32_t buf, int x, int y, int w, int h, uint8_t v)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w > 0 && h > 0)
        blt_fill(buf + (uint32_t)y * W + (uint32_t)x, W, (uint16_t)w, (uint16_t)h, v);
}

/* ---------------------------------------------------------------- CPU drawing into an 8 bpp buffer */
static void pset8(uint32_t buf, int x, int y, uint8_t c)
{
    if ((unsigned)x < W && (unsigned)y < H)
        *(volatile uint8_t *)(buf + (uint32_t)y * W + (uint32_t)x) = c;
}
static void hspan8(uint32_t buf, int x0, int x1, int y, uint8_t c)    /* x0..x1 inclusive */
{
    volatile uint8_t *p;
    if ((unsigned)y >= H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= W) x1 = W - 1;
    if (x0 > x1) return;
    p = (volatile uint8_t *)(buf + (uint32_t)y * W + (uint32_t)x0);
    if ((uint32_t)p & 1) { *p++ = c; x0++; }
    {
        volatile uint16_t *q = (volatile uint16_t *)p;
        uint16_t cc = (uint16_t)(c | c << 8);
        int n = (x1 - x0 + 1) >> 1;
        while (n >= 4) { q[0] = cc; q[1] = cc; q[2] = cc; q[3] = cc; q += 4; n -= 4; }
        while (n--) *q++ = cc;
        if (!((x1 - x0) & 1)) *(volatile uint8_t *)q = c;
    }
}
COLD static void line8(uint32_t buf, int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        pset8(buf, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
    }
}
COLD static void disc8(uint32_t buf, int cx, int cy, int r, uint8_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        int32_t w2 = (int32_t)r * r - (int32_t)y * y;
        int w = 0;
        while ((int32_t)(w + 1) * (w + 1) <= w2) w++;
        hspan8(buf, cx - w, cx + w, cy + y, c);
    }
}
/* text from the 8x14 font, scaled sx x sy; bg 0 leaves the background */
COLD static void text8(uint32_t buf, int x, int y, const char *s, uint8_t fg, uint8_t bg, int sx, int sy)
{
    for (; *s; s++, x += 8 * sx) {
        const uint8_t *g = font8x14[(uint8_t)*s];
        int r, cbit, i, j;
        for (r = 0; r < 14; r++)
            for (cbit = 0; cbit < 8; cbit++) {
                uint8_t on = (uint8_t)((g[r] >> (7 - cbit)) & 1);
                if (!on && !bg) continue;
                for (j = 0; j < sy; j++)
                    for (i = 0; i < sx; i++)
                        pset8(buf, x + cbit * sx + i, y + r * sy + j, on ? fg : bg);
            }
        if (!(x & 63)) poll();
    }
}
static int strlen16(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* ---------------------------------------------------------------- the text layer */
#define TA(fg, bg) ((uint16_t)(((bg) << 3 | (fg)) << 8))
#define TCLEAR 0x4000                           /* transparent background */
#define TBLEND 0x8000                           /* 50% blend background (true colour) */
static void tclear(void)
{
    int i;
    for (i = 0; i < 2000; i++) TEXT[i] = 0x4020;
}
static void tstr(int col, int row, const char *s, uint16_t attr)
{
    volatile uint16_t *p = TEXT + row * 80 + col;
    while (*s && col++ < 80) *p++ = (uint16_t)(attr | (uint8_t)*s++);
}

/* ---------------------------------------------------------------- hardware sprites */
static void spr_set(uint8_t n, int x, int y, uint8_t h, uint32_t a, uint8_t wc, uint8_t flip)
{
    if (y < -32) y = -32;
    if (x < -32) x = -32;
    IO(0x230) = (uint16_t)(n * 4);
    IO(0x232) = (uint16_t)(x + 32);
    IO(0x232) = (uint16_t)((y + 32) | (h - 1) << 11);
    IO(0x232) = (uint16_t)a;
    IO(0x232) = (uint16_t)(0x8000 | wc << 13 | flip << 12 | ((a >> 16) & 15));
}
static void spr_xy(uint8_t n, int x, int y, uint8_t h)          /* a move: words 0 and 1 */
{
    if (y < -32) y = -32;
    if (x < -32) x = -32;
    if (x > 990) x = 990;
    IO(0x230) = (uint16_t)(n * 4);
    IO(0x232) = (uint16_t)(x + 32);
    IO(0x232) = (uint16_t)((y + 32) | (h - 1) << 11);
}
static void spr_off(uint8_t n) { IO(0x230) = (uint16_t)(n * 4 + 3); IO(0x232) = 0; }
static void spr_alloff(void)
{
    uint8_t i;
    for (i = 0; i < 64; i++) spr_off(i);
}

/* ---------------------------------------------------------------- misc */
static uint16_t rnd_s = 0xACE1;
static uint16_t rnd(void)
{
    rnd_s ^= (uint16_t)(rnd_s << 7);
    rnd_s ^= (uint16_t)(rnd_s >> 9);
    rnd_s ^= (uint16_t)(rnd_s << 8);
    return rnd_s;
}
#define SIN(a) (sin8[(uint8_t)(a)])
#define COS(a) (sin8[(uint8_t)((a) + 64)])
#define SIN16(a) (sin16[(uint8_t)(a)])
#define COS16(a) (sin16[(uint8_t)((a) + 64)])

/* a fresh screen: everything off, palette black, sprites off, scroll 0 */
COLD static void screen_reset(void)
{
    uint16_t i;
    vid(0);
    vid2(0);
    spr_alloff();
    scroll_a(0, 0);
    scroll_b(0, 0);
    backdrop(0);
    IO(0x210) = 0;
    for (i = 0; i < 256; i++) IO(0x212) = 0;
    tclear();
    IO(0x204) = 0xC000; IO(0x206) = 0x0F;       /* the text layer at 0x0FC000 */
}
