/* SDRAM bring-up for tang/rtl/system.v: a memory test of the SDRAM up to
 * the bitmap (words, two patterns; the fpga_cpu has 1 MB, and T1 halts on
 * an address from 100000), a sparse test of the same (one word per 4 KB:
 * each bank and row), then a hi-color bitmap
 * scrolling under the text, with live counters. Results also go to the
 * serial console. -DQUICK tests 64 KB (for simulation) and stops after
 * three frames. */
#include <stdint.h>
#include <fcpu.h>

#define REG(a) IO16(a)
#define TEXT ((volatile uint16_t *)0x0FC000)
#define BMP  ((volatile uint8_t *)0x0C3C00)

#ifdef QUICK
#define TEST_END 0x020000ul
#else
#define TEST_END 0x0C3C00ul                  /* up to the bitmap */
#endif

static void con(const char *s) { while (*s) CON_OUT = (uint8_t)*s++; }

static void put(int row, int col, const char *s, int attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s) *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

static char hb[9];
static const char *hexs(uint32_t v, int digits)
{
    for (int i = digits - 1; i >= 0; i--, v >>= 4) hb[i] = "0123456789ABCDEF"[v & 15];
    hb[digits] = 0;
    return hb;
}

static void report(int row, const char *what, uint32_t errs, uint32_t first)
{
    const int G = 0x42, R = 0x44, W = 0x47;
    put(row, 1, what, W);
    put(row, 40, errs ? "FAILED" : "ok    ", errs ? R : G);
    put(row, 48, "errors", W);
    put(row, 55, hexs(errs, 6), errs ? R : G);
    con(what); con(errs ? ": FAILED, errors " : ": ok, errors ");
    con(hexs(errs, 6));
    if (errs) { put(row, 62, "first", W); put(row, 68, hexs(first, 6), R); con(" first "); con(hexs(first, 6)); }
    con("\n");
}

static uint16_t pat(uint32_t a, unsigned pass)
{
    uint16_t v = (uint16_t)(a >> 1) * 0x9E37u ^ (uint16_t)(a >> 17) * 0x4F1Bu;
    return pass ? (uint16_t)~v : v;
}

int main(void)
{
    const int W = 0x47, Y = 0x46, BAR = 0x0F;
    for (int i = 0; i < 2000; i++) TEXT[i] = 0x4720;
    put(0, 0, " fpga_cpu on the Tang Nano 20K: SDRAM test                                       ", BAR);
    REG(0xF00202) = 0;                        /* black backdrop */
    REG(0xF00200) = 0x01;
    con("sdtest\n");

    /* 1: words from 010000 to TEST_END, two patterns */
    uint32_t errs = 0, first = 0;
    uint32_t t0 = CYCLE;
    for (unsigned pass = 0; pass < 2; pass++) {
        for (uint32_t a = 0x010000ul; a < TEST_END; a += 2)
            *(volatile uint16_t *)a = pat(a, pass);
        for (uint32_t a = 0x010000ul; a < TEST_END; a += 2)
            if (*(volatile uint16_t *)a != pat(a, pass)) { if (!errs) first = a; errs++; }
    }
    uint32_t t1 = CYCLE;
    report(2, "words 010000-" , errs, first);
    put(2, 14, hexs(TEST_END - 1, 6), W);
    put(3, 1, "T1 periods for it:", W);
    put(3, 20, hexs(t1 - t0, 8), Y);
    con("periods "); con(hexs(t1 - t0, 8)); con("\n");

    /* 2: one word per 4 KB, each its own value, then read back */
    errs = 0; first = 0;
    for (uint32_t a = 0x010000ul; a < 0x0C3000ul; a += 0x1000)
        *(volatile uint32_t *)a = a ^ 0x5A5A0000ul;
    for (uint32_t a = 0x010000ul; a < 0x0C3000ul; a += 0x1000)
        if (*(volatile uint32_t *)a != (a ^ 0x5A5A0000ul)) { if (!errs) first = a; errs++; }
    report(5, "sparse, every 4 KB to 0C2FFF", errs, first);

    /* 3: bytes: each lane on its own */
    errs = 0; first = 0;
    volatile uint8_t *b = (volatile uint8_t *)0x0A0000ul;
    for (unsigned i = 0; i < 256; i++) b[i] = (uint8_t)(i * 7 + 3);
    for (unsigned i = 0; i < 256; i++) if (b[i] != (uint8_t)(i * 7 + 3)) { if (!errs) first = 0x0A0000ul + i; errs++; }
    report(6, "bytes at 0A0000", errs, first);

    /* the bitmap: a hue wheel in diagonal bands, scrolled each frame */
    REG(0xF00210) = 0;
    for (unsigned i = 0; i < 256; i++) {
        unsigned h = i % 192, s = h / 64, f = h % 64, r, g, bl;
        if (s == 0)      { r = 63 - f; g = f;      bl = 0; }
        else if (s == 1) { r = 0;      g = 63 - f; bl = f; }
        else             { r = f;      g = 0;      bl = 63 - f; }
        REG(0xF00212) = (uint16_t)((r >> 1) << 11 | g << 5 | (bl >> 1));
    }
#ifdef QUICK
    const unsigned rows = 8;
#else
    const unsigned rows = 360;
#endif
    for (unsigned y = 0; y < rows; y++) {
        volatile uint8_t *row = BMP + (uint32_t)y * 640u;
        for (unsigned x = 0; x < 640; x++) row[x] = (uint8_t)((x + y) | 1);
    }
    REG(0xF00202) = 0;
    REG(0xF00200) = 0x07;                     /* text, bitmap, hi-color */
    put(8, 1, "frame       scan line       cycles", W);
    con("bitmap on\n");

    for (unsigned n = 0;; n++) {
        uint16_t fr = FRAME;
        put(8, 7, hexs(fr, 4), Y);
        put(8, 25, hexs(VSTAT >> 3, 3), Y);
        put(8, 37, hexs(CYCLE, 8), Y);
        REG(0xF00214) = fr;                   /* scroll the bitmap */
        while (FRAME == fr) ;
#ifdef QUICK
        if (n == 2) return 0;
#endif
    }
}
