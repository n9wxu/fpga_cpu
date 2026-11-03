/* The demo's graphics library: 16-color (4 bpp, direct) bitmaps, the text
 * layer, sprites, frame pacing and the statistics line. Included by demo.c
 * (one compilation unit); the sections use only what is here.
 *
 * The bitmap format (docs/io.md, "16-color mode"): 640x360, 4 bits a pixel,
 * two pixels a byte with the left one in the high nibble, rows of 320
 * bytes. Colours 0-7 are black, blue, green, cyan, red, magenta, yellow,
 * white; 9-15 are 1-7 at half brightness (a checkerboard on the Spartan-3's
 * VGA); 8 is transparent (the backdrop shows). This works on both video
 * tiers, so the sections draw in it everywhere.
 *
 * Memory (the same on the Spartan-3 and the Tang): the program, its data
 * and stack below 0x020000 (on the Tang the code in the 64 KB of block RAM,
 * the stack at 0x0000-0x0FFF below the program, the end of the data in the
 * SDRAM), then
 *   SCRATCH 0x020000-0x05FFFF  for the running section (salloc)
 *   BUF0-3  0x060000, 0x080000, 0x0A0000, 0x0C0000   four bitmaps
 *   TEXT    0x0FC000   the text layer
 * int is 16 bits: anything that can pass 32767 (bitmap offsets, products)
 * must be 32-bit. */
#include <stdint.h>
#include <fcpu.h>
#include "font.h"

/* helpers that run rarely: one copy, not inlined at every call (code size) */
#define COLD __attribute__((noinline))

#define REG(a)      IO16(a)
#define VID_CTRL    REG(0xF00200)
#define BACKDROP    REG(0xF00202)
#define VID_BMP_LO  REG(0xF00208)
#define VID_BMP_HI  REG(0xF0020A)
#define PAL_INDEX   REG(0xF00210)
#define PAL_DATA    REG(0xF00212)
#define SCROLLX     REG(0xF00214)
#define SCROLLY     REG(0xF00216)
#define LEDS        REG(0xF00600)
#define SWITCHES    REG(0xF00602)
#define SEG7        REG(0xF00606)

#define VC_TEXT  0x01
#define VC_BMP   0x02
#define VC_HICOL 0x04
#define VC_C16   0x80

#define W      640
#define H      360
#define STRIDE 320
#define BUFSZ  115200UL

#define BUF0    0x060000UL
#define BUF1    0x080000UL
#define BUF2    0x0A0000UL
#define BUF3    0x0C0000UL
#define SCRATCH 0x020000UL
#define SCRATCH_END 0x05FF00UL               /* above: the statistics log */
#define TEXT   ((volatile uint16_t *)0x0FC000)

/* colours */
enum {
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE,
    TRANSP, NAVY, DGREEN, TEAL, MAROON, PURPLE, OLIVE, GREY
};

/* text attributes: [7] stipple, [6] transparent background, [5:3] bg, [2:0] fg */
#define ATTR(fg, bg)   ((uint16_t)((bg) << 3 | (fg)))     /* solid background: easy to read */
#define A_CLEAR        0x40            /* transparent background */

/* ---- small things ---------------------------------------------------------- */
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
     -49,  -46,  -43,  -40,  -37,  -34,  -31,  -28,  -25,  -22,  -19,  -16,  -12,   -9,   -6,   -3,
};
#define SIN(a) (sintab[(uint8_t)(a)])          /* a: 256 to the turn, result -127..127 */
#define COS(a) (sintab[(uint8_t)((a) + 64)])

static uint16_t rnd_state = 0xACE1;
static uint16_t rnd16(void)                    /* xorshift */
{
    rnd_state ^= (uint16_t)(rnd_state << 7);
    rnd_state ^= (uint16_t)(rnd_state >> 9);
    rnd_state ^= (uint16_t)(rnd_state << 8);
    return rnd_state;
}
/* 0 .. n-1 (n up to 256 is fair enough) */
static uint16_t rnd_n(uint16_t n) { return (uint16_t)(((uint32_t)(rnd16() & 0x7FFF) * n) >> 15); }

/* instructions retired, all 32 bits (INSTRET; SR3 has only the low 16,
 * which wrap within a frame) */
static uint32_t instret(void) { return fcpu_instret(); }
static inline uint16_t sr3(void)              /* the low 16 bits only: short spans */
{
    uint16_t v;
    __asm__ volatile("MFSR %0, sr3" : "=r"(v));
    return v;
}

/* ---- the scratch area: a bump allocator, reset by each section ------------------ */
static uint32_t scratch_top = SCRATCH;
static void salloc_reset(void) { scratch_top = SCRATCH; }
static void *salloc(uint32_t n)
{
    uint32_t a = scratch_top;
    scratch_top = (scratch_top + n + 3) & ~3UL;
    if (scratch_top > SCRATCH_END) {          /* out of scratch: stop with code 99 */
        IO16(0xF00002) = 99;
        for (;;) ;
    }
    return (void *)a;
}

/* ---- the target bitmap ----------------------------------------------------------- */
static uint32_t rowoff[H];                     /* y * 320 */
static uint8_t *g_buf;                         /* where drawing goes */
static uint32_t g_shown;                       /* the bitmap on screen */

static void gfx_target(uint32_t buf) { g_buf = (uint8_t *)buf; }
static uint8_t *rowp(int y) { return g_buf + rowoff[y]; }

/* Show a bitmap: call it just after frame_wait() (in the vertical blank). */
static void gfx_show(uint32_t buf)
{
    g_shown = buf;
    VID_BMP_HI = (uint16_t)(buf >> 16);
    VID_BMP_LO = (uint16_t)buf;
}

static void pset(int x, int y, uint8_t c)
{
    if ((unsigned)x >= W || (unsigned)y >= H) return;
    uint8_t *p = rowp(y) + (x >> 1);
    if (x & 1) *p = (uint8_t)((*p & 0xF0) | c);
    else       *p = (uint8_t)((*p & 0x0F) | c << 4);
}

static uint8_t pget(int x, int y)
{
    if ((unsigned)x >= W || (unsigned)y >= H) return 0;
    uint8_t b = rowp(y)[x >> 1];
    return (x & 1) ? b & 15 : b >> 4;
}

/* a run of n bytes of value v (16-bit stores where it can) */
static void fill_bytes(uint8_t *p, int n, uint8_t v)
{
    if (n <= 0) return;
    if ((uintptr_t)p & 1) { *p++ = v; n--; }
    uint16_t vv = (uint16_t)(v | v << 8);
    uint16_t *q = (uint16_t *)p;
    int k = n >> 1;
    while (k >= 4) { q[0] = vv; q[1] = vv; q[2] = vv; q[3] = vv; q += 4; k -= 4; }
    while (k--) *q++ = vv;
    if (n & 1) *(uint8_t *)q = v;
}

/* pixels x0..x1 (inclusive) of row y */
static void hline(int x0, int x1, int y, uint8_t c)
{
    if ((unsigned)y >= H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= W) x1 = W - 1;
    if (x0 > x1) return;
    uint8_t *r = rowp(y);
    if (x0 & 1) { r[x0 >> 1] = (uint8_t)((r[x0 >> 1] & 0xF0) | c); x0++; }
    if (!(x1 & 1) && x1 >= x0) { r[x1 >> 1] = (uint8_t)((r[x1 >> 1] & 0x0F) | c << 4); x1--; }
    if (x1 > x0) fill_bytes(r + (x0 >> 1), (x1 - x0 + 1) >> 1, (uint8_t)(c << 4 | c));
}

static void vline(int x, int y0, int y1, uint8_t c)
{
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if ((unsigned)x >= W) return;
    if (y0 < 0) y0 = 0;
    if (y1 >= H) y1 = H - 1;
    uint8_t *p = rowp(y0) + (x >> 1);
    uint8_t m = (x & 1) ? 0xF0 : 0x0F, v = (x & 1) ? c : (uint8_t)(c << 4);
    for (int y = y0; y <= y1; y++, p += STRIDE) *p = (uint8_t)((*p & m) | v);
}

static void fill_rect(int x, int y, int w, int h, uint8_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    for (int i = 0; i < h; i++) hline(x, x + w - 1, y + i, c);
}

COLD static void clear(uint8_t c)
{
    fill_bytes(g_buf, (int)32000, (uint8_t)(c << 4 | c));
    fill_bytes(g_buf + 32000, (int)32000, (uint8_t)(c << 4 | c));
    fill_bytes(g_buf + 64000, (int)32000, (uint8_t)(c << 4 | c));
    fill_bytes(g_buf + 96000, (int)19200, (uint8_t)(c << 4 | c));
}

COLD static void rect(int x, int y, int w, int h, uint8_t c)   /* outline */
{
    if (w <= 0 || h <= 0) return;
    hline(x, x + w - 1, y, c);
    hline(x, x + w - 1, y + h - 1, c);
    vline(x, y, y + h - 1, c);
    vline(x + w - 1, y, y + h - 1, c);
}

COLD static void line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ellipse centred at (cx, cy) with radii rx, ry: filled (fill = colour) or
 * outlined (fill < 0) in colour c (midpoint algorithm, 32-bit terms) */
COLD static void ellipse(int cx, int cy, int rx, int ry, uint8_t c, int filled)
{
    if (rx <= 0 || ry <= 0) { if (filled) hline(cx - rx, cx + rx, cy, c); else pset(cx, cy, c); return; }
    int32_t a2 = (int32_t)rx * rx, b2 = (int32_t)ry * ry;
    int x = 0, y = ry;
    int px = -1, py = -1;
    int32_t d;
    /* region 1 */
    d = b2 - a2 * ry + a2 / 4;
    int32_t dx = 0, dy = 2 * a2 * y;
    while (dx < dy) {
        if (filled) {
            if (y != py) { hline(cx - x, cx + x, cy - y, c); hline(cx - x, cx + x, cy + y, c); py = y; }
            else { hline(cx - x, cx + x, cy - y, c); hline(cx - x, cx + x, cy + y, c); }
        } else {
            pset(cx + x, cy + y, c); pset(cx - x, cy + y, c);
            pset(cx + x, cy - y, c); pset(cx - x, cy - y, c);
        }
        if (d < 0) { x++; dx += 2 * b2; d += dx + b2; }
        else { x++; y--; dx += 2 * b2; dy -= 2 * a2; d += dx - dy + b2; }
    }
    /* region 2 */
    d = b2 * ((int32_t)(2 * x + 1) * (2 * x + 1)) / 4 + a2 * ((int32_t)(y - 1) * (y - 1)) - a2 * b2;
    while (y >= 0) {
        if (filled) { hline(cx - x, cx + x, cy - y, c); hline(cx - x, cx + x, cy + y, c); }
        else {
            pset(cx + x, cy + y, c); pset(cx - x, cy + y, c);
            pset(cx + x, cy - y, c); pset(cx - x, cy - y, c);
        }
        if (d > 0) { y--; dy -= 2 * a2; d += a2 - dy; }
        else { y--; x++; dx += 2 * b2; dy -= 2 * a2; d += dx - dy + a2; }
    }
    (void)px;
}

static void circle(int cx, int cy, int r, uint8_t c, int filled) { ellipse(cx, cy, r, r, c, filled); }

/* copy a rectangle from bitmap src to bitmap dst (x and w are rounded out
 * to whole bytes) */
static void copy_rect(uint32_t dst, uint32_t src, int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    int b0 = x >> 1, b1 = (x + w + 1) >> 1;
    int n = b1 - b0;
    uint32_t o = rowoff[y] + (uint32_t)b0;
    for (int i = 0; i < h; i++, o += STRIDE) {
        uint8_t *d = (uint8_t *)(dst + o), *s = (uint8_t *)(src + o);
        int k = n;
        if (!(((uintptr_t)d | (uintptr_t)s) & 1)) {
            uint16_t *d2 = (uint16_t *)d, *s2 = (uint16_t *)s;
            while (k >= 2) { *d2++ = *s2++; k -= 2; }
            d = (uint8_t *)d2; s = (uint8_t *)s2;
        }
        while (k--) *d++ = *s++;
    }
}

COLD static void copy_buf(uint32_t dst, uint32_t src)
{
    uint16_t *d = (uint16_t *)dst, *s = (uint16_t *)src;
    for (uint32_t k = 0; k < BUFSZ / 16; k++) {
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
        d += 8; s += 8;
    }
}

/* ---- sprites ----------------------------------------------------------------------
 * Made at start-up from text art: one character a pixel, a hex digit is a
 * colour, anything else ('.', ' ') transparent. Each sprite is kept twice
 * (at even and odd x), as (data, mask) byte pairs: dst = (dst & mask) | data. */
typedef struct {
    int16_t w, h;
    uint8_t wb;                                 /* bytes a row, both versions */
    uint8_t *v[2];                              /* even x, odd x: h rows of wb (data, mask) pairs */
} sprite_t;

static int artpix(const char *art, int w, int x, int y)
{
    if (x < 0 || x >= w) return -1;
    char ch = art[y * w + x];
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

/* flip: 1 = mirrored left-right. The pixels may be 8 (transparent in the
 * bitmap too) only by leaving them out. */
COLD static sprite_t *spr_make(const char *art, int w, int h, int flip)
{
    sprite_t *s = (sprite_t *)salloc(sizeof(sprite_t));
    s->w = (int16_t)w; s->h = (int16_t)h;
    s->wb = (uint8_t)((w + 2) / 2);
    for (int odd = 0; odd < 2; odd++) {
        uint8_t *p = (uint8_t *)salloc((uint32_t)h * s->wb * 2);
        s->v[odd] = p;
        for (int y = 0; y < h; y++)
            for (int b = 0; b < s->wb; b++) {
                uint8_t d = 0, m = 0;
                for (int k = 0; k < 2; k++) {
                    int sx = 2 * b + k - odd;       /* the sprite pixel in this nibble */
                    int px = artpix(art, w, flip ? w - 1 - sx : sx, y);
                    if (sx < 0 || sx >= w) px = -1;
                    uint8_t sh = k ? 0 : 4;
                    if (px < 0) m |= (uint8_t)(15 << sh);
                    else d |= (uint8_t)(px << sh);
                }
                *p++ = d; *p++ = m;
            }
    }
    return s;
}

/* draw at (x, y), clipped to the screen */
static void spr_draw(const sprite_t *s, int x, int y)
{
    int h = s->h, wb = s->wb;
    int y0 = 0, b0 = 0;
    int bx = (x >> 1) + ((x < 0) ? 0 : 0);
    const uint8_t *src = s->v[x & 1];
    if (x < 0) bx = -((1 - x) >> 1);           /* floor(x / 2) */
    if (y < 0) { y0 = -y; }
    if (y + h > H) h = H - y;
    if (bx < 0) b0 = -bx;
    int b1 = wb;
    if (bx + b1 > STRIDE) b1 = STRIDE - bx;
    if (b0 >= b1 || y0 >= h) return;
    for (int r = y0; r < h; r++) {
        const uint8_t *q = src + ((uint16_t)r * wb + b0) * 2;
        uint8_t *p = rowp(y + r) + bx + b0;
        for (int b = b0; b < b1; b++, q += 2, p++) {
            uint8_t m = q[1];
            if (m == 0) *p = q[0];
            else if (m != 0xFF) *p = (uint8_t)((*p & m) | q[0]);
        }
    }
}

/* draw in one colour (a flash, a shadow): the sprite's shape only */
static void spr_draw_solid(const sprite_t *s, int x, int y, uint8_t c)
{
    uint8_t cc = (uint8_t)(c << 4 | c);
    int h = s->h, wb = s->wb;
    const uint8_t *src = s->v[x & 1];
    int bx = (x < 0) ? -((1 - x) >> 1) : x >> 1;
    for (int r = 0; r < h; r++) {
        int yy = y + r;
        if ((unsigned)yy >= H) continue;
        uint8_t *p = rowp(yy);
        const uint8_t *q = src + (uint16_t)r * wb * 2;
        for (int b = 0; b < wb; b++) {
            int xb = bx + b;
            uint8_t m = q[2 * b + 1];
            if ((unsigned)xb < STRIDE && m != 0xFF) p[xb] = (uint8_t)((p[xb] & m) | (cc & ~m));
        }
    }
}

/* ---- text in the bitmap (the 8x14 font) --------------------------------------------- */
/* bg 8 (TRANSP) leaves the background alone; scale 1 or 2 */
COLD static void draw_char(int x, int y, uint8_t ch, uint8_t fg, uint8_t bg, int scale)
{
    const uint8_t *g = font8x14[ch];
    for (int r = 0; r < 14; r++) {
        uint8_t bits = g[r];
        for (int sy = 0; sy < scale; sy++) {
            int yy = y + r * scale + sy;
            for (int k = 0; k < 8; k++) {
                int on = bits & (0x80 >> k);
                if (!on && bg == TRANSP) continue;
                if (scale == 1) pset(x + k, yy, on ? fg : bg);
                else { pset(x + 2 * k, yy, on ? fg : bg); pset(x + 2 * k + 1, yy, on ? fg : bg); }
            }
        }
    }
}

COLD static int draw_text(int x, int y, const char *s, uint8_t fg, uint8_t bg, int scale)
{
    while (*s) { draw_char(x, y, (uint8_t)*s++, fg, bg, scale); x += 8 * scale; }
    return x;
}

/* ---- the text layer ------------------------------------------------------------------- */
COLD static void tput(int row, int col, const char *s, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s && col++ < 80)
        *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

COLD static void tnum(int row, int col, uint16_t v, int digits, uint16_t attr)
{
    char b[6];
    b[digits] = 0;
    for (int i = digits - 1; i >= 0; i--, v /= 10)
        b[i] = (char)('0' + v % 10);
    tput(row, col, b, attr);
}

COLD static void tcenter(int row, const char *s, uint16_t attr)
{
    int n = 0;
    while (s[n]) n++;
    tput(row, (80 - n) / 2, s, attr);
}

COLD static void tfill(int row, int col, int n, uint8_t ch, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (n-- > 0) *t++ = (uint16_t)(attr << 8 | ch);
}

COLD static void tclear(void)
{
    for (int i = 0; i < 25 * 80; i++)
        TEXT[i] = (uint16_t)(A_CLEAR << 8 | ' ');
}

/* ---- the soundtrack ------------------------------------------------------------------------ */
#include "music.c"

/* ---- frame pacing, the section's time, statistics -------------------------------------- */
static uint16_t last_frame;
static int32_t  sec_left;                       /* frames the section has left */
static uint32_t frames_drawn, frames_total;     /* this section: frame_wait calls, frames */
static uint32_t st_busy, st_total, st_insts;    /* the second so far */
static uint16_t st_frames, st_drawn;
static uint32_t busy_from;
static uint32_t insts_from;
static uint32_t sec_insts_all;                  /* this section's instructions (busy) */
static int      stats_row = 24;                 /* the statistics line; -1: none */
static int      sec_index;                      /* the running section (demo.c sets it) */
/* each section's last statistics, for reading back over JTAG
 * (jtagload.py --dump 0x05FF00 48): {fps, busy %, instructions a frame, MIPS x 100} */
#define STATS_LOG ((volatile uint32_t *)0x05FF00)
static const char *sec_name = "";

COLD static void stats_show(void)
{
    if (stats_row < 0 || (SWITCHES & 1)) return;
    int r = stats_row;
    volatile uint32_t *lg = STATS_LOG + 4 * (sec_index & 7);
    tfill(r, 0, 80, ' ', ATTR(WHITE, BLACK));
    tput(r, 1, sec_name, ATTR(CYAN, BLACK));
    tput(r, 21, "fps", ATTR(WHITE, BLACK));
    tnum(r, 25, st_drawn, 2, ATTR(YELLOW, BLACK));
    tput(r, 28, "busy", ATTR(WHITE, BLACK));
    tnum(r, 33, (uint16_t)(st_total ? st_busy * 100 / st_total : 0), 3, ATTR(YELLOW, BLACK));
    tput(r, 36, "%", ATTR(WHITE, BLACK));
    tput(r, 39, "kI/f", ATTR(WHITE, BLACK));
    tnum(r, 44, (uint16_t)(st_drawn ? st_insts / st_drawn / 1000 : 0), 3, ATTR(YELLOW, BLACK));
    /* MIPS while busy: instructions a second over the busy share of the
     * time (by FRAME, so CYCLE's rate is not needed) */
    tput(r, 49, "MIPS", ATTR(WHITE, BLACK));
    uint32_t ips = st_insts * 60 / (st_frames ? st_frames : 1);
    uint32_t permille = st_busy / (st_total / 1000 + 1);
    uint32_t mips100 = ips / (10 * (permille ? permille : 1));
    tnum(r, 54, (uint16_t)(mips100 / 100 % 100), 2, ATTR(YELLOW, BLACK));
    lg[0] = st_drawn;
    lg[1] = st_total ? st_busy * 100 / st_total : 0;
    lg[2] = st_drawn ? st_insts / st_drawn : 0;
    lg[3] = mips100;
    tput(r, 56, ".", ATTR(YELLOW, BLACK));
    tnum(r, 57, (uint16_t)(mips100 % 100), 2, ATTR(YELLOW, BLACK));
    /* CPU clocks (CYCLE ticks: T1 clocks, or T1S steps) an instruction, and a second */
    tput(r, 61, "CPI", ATTR(WHITE, BLACK));
    uint32_t cpi10 = st_insts ? st_busy * 10 / st_insts : 0;
    tnum(r, 65, (uint16_t)(cpi10 / 10 % 100), 2, ATTR(YELLOW, BLACK));
    tput(r, 67, ".", ATTR(YELLOW, BLACK));
    tnum(r, 68, (uint16_t)(cpi10 % 10), 1, ATTR(YELLOW, BLACK));
    tput(r, 71, "MHz", ATTR(WHITE, BLACK));
    uint32_t khz = st_total / (st_frames ? st_frames : 1) * 60 / 1000;
    tnum(r, 75, (uint16_t)(khz / 1000 % 100), 2, ATTR(YELLOW, BLACK));
    tput(r, 77, ".", ATTR(YELLOW, BLACK));
    tnum(r, 78, (uint16_t)(khz / 100 % 10), 1, ATTR(YELLOW, BLACK));
}

/* Start a section's clock (frames: its length), after its set-up. */
COLD static void sec_begin(const char *name, int32_t frames)
{
    sec_name = name;
    sec_left = frames;
    frames_drawn = frames_total = 0;
    st_busy = st_total = st_insts = 0;
    st_frames = st_drawn = 0;
    sec_insts_all = 0;
    last_frame = FRAME;
    busy_from = CYCLE;
    insts_from = instret();
}

static int sec_running(void) { return sec_left > 0; }

/* Wait for the next vertical blank; returns the frames since the last call
 * (1 when the section keeps up with 60 Hz). Counts the section's time. */
static uint16_t frame_wait(void)
{
    uint32_t t0 = CYCLE;
    uint32_t di = instret() - insts_from;
    st_insts += di;
    sec_insts_all += di;
    st_busy += t0 - busy_from;
    while (FRAME == last_frame) ;
    uint16_t now = FRAME;
    uint16_t n = (uint16_t)(now - last_frame);
    last_frame = now;
    uint32_t t1 = CYCLE;
    st_total += t1 - busy_from;
    busy_from = t1;
    insts_from = instret();
    music_tick(n);
    sec_left -= n;
    frames_drawn++;
    frames_total += n;
    st_drawn++;
    st_frames += n;
    if (st_frames >= 60) {
        stats_show();
        st_busy = st_total = st_insts = 0;
        st_frames = st_drawn = 0;
    }
    return n;
}

/* the video registers for a section in 16-colour mode, text on */
COLD static void gfx_mode16(void)
{
    SCROLLX = 0;
    SCROLLY = 0;
    BACKDROP = BLACK;
    VID_CTRL = VC_TEXT | VC_BMP | VC_C16;
}

COLD static void gfx_init(void)
{
    uint32_t o = 0;
    for (int y = 0; y < H; y++, o += STRIDE) rowoff[y] = o;
    gfx_target(BUF0);
}
