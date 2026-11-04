/* vidxtest: the Tang's video extensions and blitter (docs/io.md, "Video extensions", "Blitter") in one picture:
 *   playfield A   a hi-color diagonal gradient, waving by its line-scroll entries
 *   playfield B   in front: three bars and a ring with transparent holes, sheared by its line scroll
 *   sprites       20 hardware sprites (8, 16 and 32 wide), overlapping; sprite 3 flipped
 *   blitter       a fill (the white box) and a keyed copy (a cross, its key colour left out) into playfield A
 *   double buffer VID_STAT bit 0 checked around a VID_BMP write
 * It prints PASS or the failures on the console, then holds the picture (fsim --screenshot, or the board).
 * Build: llvm/fcc -O2 --stack 0x10000 -o vidxtest.bin tang/progs/vidxtest.c */
#include <stdint.h>

#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define PFA 0x060000UL                  /* 230,400 bytes each */
#define PFB 0x0A0000UL
#define SPRDATA 0x040000UL
#define LSCR 0x050000UL
#define TEXT ((volatile uint16_t *)0x0FC000UL)

static const int8_t sin32[32] = {0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
                                 0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25};
static int fails;

static void puts_(const char *s) { while (*s) IO(0) = (uint8_t)*s++; }
static void check(int ok, const char *what)
{
    if (!ok) {
        puts_("FAIL ");
        puts_(what);
        puts_("\n");
        fails++;
    }
}

static void pal(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
    IO(0x210) = i;
    IO(0x212) = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

static void blit_wait(void) { while (IO(0x816) & 1) ; }

static void blit(uint32_t src, uint32_t dst, int16_t ss, int16_t ds, uint16_t w, uint16_t h, uint16_t cmd)
{
    blit_wait();
    IO(0x800) = (uint16_t)src; IO(0x802) = (uint16_t)(src >> 16);
    IO(0x804) = (uint16_t)dst; IO(0x806) = (uint16_t)(dst >> 16);
    IO(0x808) = (uint16_t)ss;  IO(0x80A) = (uint16_t)ds;
    IO(0x80C) = w; IO(0x80E) = h;
    IO(0x814) = cmd;
}

static void sprite(uint8_t n, int x, int y, uint8_t h, uint32_t addr, uint8_t wcode, uint8_t flip)
{
    IO(0x230) = (uint16_t)(n * 4);
    IO(0x232) = (uint16_t)(x + 32);
    IO(0x232) = (uint16_t)((y + 32) | (h - 1) << 11);
    IO(0x232) = (uint16_t)addr;
    IO(0x232) = (uint16_t)(0x8000 | wcode << 13 | flip << 12 | (addr >> 16 & 15));
}

int main(void)
{
    uint16_t i, x, y;
    IO(0x200) = 0;
    /* palette: 64-127 a rainbow gradient, 128-135 greens, 200-215 a ball's shades, 255 white, 250 red */
    for (i = 0; i < 64; i++) {
        uint8_t ph = (uint8_t)(i * 4);
        pal((uint8_t)(64 + i), (uint8_t)(128 + sin32[(ph >> 3) & 31]), (uint8_t)(128 + sin32[((ph >> 3) + 11) & 31]),
            (uint8_t)(128 + sin32[((ph >> 3) + 21) & 31]));
    }
    for (i = 0; i < 8; i++)
        pal((uint8_t)(128 + i), 0, (uint8_t)(80 + i * 22), (uint8_t)(i * 8));
    for (i = 0; i < 16; i++)
        pal((uint8_t)(200 + i), (uint8_t)(255 - i * 10), (uint8_t)(200 - i * 12), (uint8_t)(40 + i * 4));
    pal(255, 255, 255, 255);
    pal(250, 255, 0, 0);
    pal(251, 0, 0, 255);

    /* playfield A: (x + y) / 4 through the gradient */
    for (y = 0; y < 360; y++) {
        volatile uint16_t *r = (volatile uint16_t *)(PFA + (uint32_t)y * 640);
        for (x = 0; x < 640; x += 2) {
            uint8_t a = (uint8_t)(64 + ((x + y) >> 3 & 63)), b = (uint8_t)(64 + ((x + 1 + y) >> 3 & 63));
            r[x >> 1] = (uint16_t)(a | b << 8);
        }
    }
    /* playfield B: transparent, three green bars and a ring */
    for (y = 0; y < 360; y++) {
        volatile uint16_t *r = (volatile uint16_t *)(PFB + (uint32_t)y * 640);
        for (x = 0; x < 640; x += 2) {
            int dx = (int)x - 320, dy = (int)y - 180;
            int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy;
            uint8_t c = 0;
            if ((x >= 40 && x < 80) || (x >= 560 && x < 600) || (y >= 300 && y < 330))
                c = (uint8_t)(128 + (y >> 5 & 7));
            if (d2 > 90L * 90 && d2 < 110L * 110)
                c = 131;
            r[x >> 1] = (uint16_t)(c | c << 8);
        }
    }
    /* line scroll: A waves by 12 pixels, B shears */
    for (y = 0; y < 360; y++) {
        volatile uint16_t *e = (volatile uint16_t *)(LSCR + (uint32_t)y * 4);
        e[0] = (uint16_t)(640 + sin32[(y >> 2) & 31] / 10);
        e[1] = (uint16_t)(y / 4);
    }
    /* sprite images: a 32x32 ball (rings of shades), a 16x16 arrow (left-pointing, to see the flip) and an 8x8 dot */
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            int dx = (int)x - 16, dy = (int)y - 16;
            int d2 = dx * dx + dy * dy;
            *(volatile uint8_t *)(SPRDATA + y * 32 + x) = d2 < 225 ? (uint8_t)(200 + d2 / 15) : 0;
        }
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            int on = (int)x >= (int)(y < 8 ? 8 - y : y - 7) || (y >= 6 && y < 10);
            *(volatile uint8_t *)(SPRDATA + 0x400 + y * 16 + x) = on ? 250 : 0;
        }
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            *(volatile uint8_t *)(SPRDATA + 0x500 + y * 8 + x) = (uint8_t)((x ^ y) & 1 ? 255 : 251);
    for (i = 0; i < 64; i++) {                          /* all off */
        IO(0x230) = (uint16_t)(i * 4 + 3);
        IO(0x232) = 0;
    }
    sprite(0, 100, 100, 32, SPRDATA, 2, 0);
    sprite(1, 116, 108, 32, SPRDATA, 2, 0);             /* behind sprite 0 */
    sprite(2, 400, 60, 16, SPRDATA + 0x400, 1, 0);
    sprite(3, 430, 60, 16, SPRDATA + 0x400, 1, 1);      /* flipped */
    for (i = 4; i < 20; i++)
        sprite((uint8_t)i, 200 + (i - 4) * 24, 250 + sin32[(i * 4) & 31] / 8, 8, SPRDATA + 0x500, 0, 0);
    sprite(20, -16, 200, 32, SPRDATA, 2, 0);            /* half off the left edge */

    /* blitter: a white 120x40 box into A, then a keyed copy of a red-on-key cross from scratch */
    IO(0x812) = 255;
    blit(0, PFA + 40UL * 640 + 480, 0, 640, 120, 40, 1);
    for (y = 0; y < 24; y++)
        for (x = 0; x < 24; x++)
            *(volatile uint8_t *)(SPRDATA + 0x800 + y * 24 + x) = (x >= 9 && x < 15) || (y >= 9 && y < 15) ? 250 : 7;
    IO(0x810) = 7;                                      /* the key */
    blit(SPRDATA + 0x800, PFA + 48UL * 640 + 528, 24, 640, 24, 24, 2);
    blit_wait();
    check(*(volatile uint8_t *)(PFA + 40UL * 640 + 480) == 255, "blit fill");
    check(*(volatile uint8_t *)(PFA + 60UL * 640 + 540) == 250, "blit keyed copy: cross");
    check(*(volatile uint8_t *)(PFA + 48UL * 640 + 528) == 255, "blit keyed copy: key left out");

    /* the layers on; the buffers flip at the next frame */
    IO(0x204) = 0xC000; IO(0x206) = 0x0F;
    IO(0x224) = (uint16_t)PFB; IO(0x226) = (uint16_t)(PFB >> 16);
    IO(0x22C) = (uint16_t)LSCR; IO(0x22E) = (uint16_t)(LSCR >> 16);
    IO(0x208) = (uint16_t)PFA; IO(0x20A) = (uint16_t)(PFA >> 16);
    check(IO(0x222) & 1, "flip pending after a VID_BMP write");
    {
        uint16_t f = IO(0xA);
        while (IO(0xA) == f) ;
        while (IO(0xA) == (uint16_t)(f + 1)) ;
    }
    check(!(IO(0x222) & 1), "flip done after a frame");
    for (i = 0; i < 2000; i++)
        TEXT[i] = 0x4020;
    {
        const char *m = " VIDXTEST: PLAYFIELDS A+B, LINE SCROLL, 21 SPRITES, BLITTER ";
        for (i = 0; m[i]; i++)
            TEXT[80 * 23 + 10 + i] = (uint16_t)(0x0F00 | (uint8_t)m[i]);     /* white on blue */
    }
    IO(0x220) = 1 | 2 | 4 | 8;                          /* B on, in front, line scroll, sprites */
    IO(0x200) = 1 | 2 | 4;                              /* text, bitmap, hi-color */
    puts_(fails ? "vidxtest: FAIL\n" : "vidxtest: PASS\n");
#ifdef FSIM
    {
        uint16_t f = IO(0xA);
        while ((uint16_t)(IO(0xA) - f) < 3) ;
        IO(2) = (uint16_t)fails;
    }
#endif
    for (;;) ;
}
