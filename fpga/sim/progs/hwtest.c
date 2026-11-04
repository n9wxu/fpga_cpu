/* Hardware bring-up for fpga/rtl/system.v: text, SRAM test, hi-color
 * bitmap, and the live registers (frame, scan line, cycles, switches). */
#include <stdint.h>
#include <fcpu.h>

#define REG(a) IO16(a)
#define TEXT ((volatile uint16_t *)0x0FC000)
#define BMP  ((volatile uint8_t *)0x0C3C00)

static void put(int row, int col, const char *s, int attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s) *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

static void hex(int row, int col, unsigned v, int digits, int attr)
{
    char b[9];
    for (int i = digits - 1; i >= 0; i--, v >>= 4) b[i] = "0123456789ABCDEF"[v & 15];
    b[digits] = 0;
    put(row, col, b, attr);
}

int main(void)
{
    /* attribute byte: [6] transparent background, [5:3] background, [2:0] foreground */
    const int W = 0x47, Y = 0x46, G = 0x42, R = 0x44, BAR = 0x0F;   /* BAR: white on blue */
    for (int i = 0; i < 2000; i++) TEXT[i] = 0x4720;   /* transparent spaces */
    put(0, 0, " fpga_cpu on the Spartan-3 board: T1 at 10 MHz, SRAM, video, I/O             ", BAR);
    put(2, 1, "SRAM test, 64 KB of words:", W);

    volatile uint16_t *m = (volatile uint16_t *)0x060000;
    unsigned errs = 0;
    for (unsigned pass = 0; pass < 2; pass++) {
        for (uint16_t i = 0; i < 32768u; i++) m[i] = (uint16_t)((i * 0x9E37u) ^ (pass ? 0xFFFF : 0));
        for (uint16_t i = 0; i < 32768u; i++)
            if (m[i] != (uint16_t)((i * 0x9E37u) ^ (pass ? 0xFFFF : 0))) errs++;
    }
    put(2, 28, errs ? "FAILED, errors:" : "ok, errors:", errs ? R : G);
    hex(2, 44, errs, 4, errs ? R : G);
    REG(0xF00606) = (uint16_t)errs;

    /* palette: a hue wheel; bitmap: diagonal bands, index 0 left as backdrop */
    REG(0xF00210) = 0;
    for (unsigned i = 0; i < 256; i++) {
        unsigned h = i % 192, s = h / 64, f = h % 64, r, g, b;
        if (s == 0)      { r = 63 - f; g = f;      b = 0; }
        else if (s == 1) { r = 0;      g = 63 - f; b = f; }
        else             { r = f;      g = 0;      b = 63 - f; }
        REG(0xF00212) = (uint16_t)((r >> 1) << 11 | g << 5 | (b >> 1));
    }
    for (unsigned y = 0; y < 360; y++) {
        volatile uint8_t *row = BMP + (uint32_t)y * 640u;
        for (unsigned x = 0; x < 640; x++) row[x] = (uint8_t)((x + y) | 1);
    }
    REG(0xF00202) = 1;
    REG(0xF00200) = 0x07;                     /* text, bitmap, hi-color */
    put(4, 1, "frame       scan line       cycles               switches", W);

    for (unsigned n = 0;; n++) {
        uint16_t fr = FRAME;
        hex(4, 7, fr, 4, Y);
        hex(4, 25, VSTAT >> 4, 3, Y);
        hex(4, 37, CYCLE, 8, Y);
        hex(4, 59, REG(0xF00602), 2, Y);
        REG(0xF00606) = errs ? (uint16_t)errs : fr;
        REG(0xF00600) = REG(0xF00602);
        REG(0xF00214) = fr;                   /* scroll the bitmap */
        while (FRAME == fr) ;
#ifdef FSIM
        if (n == 3) return 0;
#endif
    }
}
