/* Video buffers above 1 MB on the Tang (video.v AW = 23): playfield A at 4 MB (flipping each second with
 * a second picture at 7 MB), playfield B at 6 MB, the text layer at 7.9 MB, the line-scroll table at
 * 3 MB and the hardware sprites' pixels at 5 MB; the blitter fills the 7 MB picture. Each picture says
 * where it is. Ten seconds, then exit 0. Run in fsim with --hw tang. */
#include <stdint.h>
#include <fcpu.h>

#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define A0   0x400000UL
#define A1   0x700000UL
#define BB   0x600000UL
#define TXT  0x7F0000UL
#define LS   0x300000UL
#define SPR  0x500000UL
#define W 640
#define H 360

static void set32(uint16_t r, uint32_t a) { IO(r) = (uint16_t)a; IO(r + 2) = (uint16_t)(a >> 16); }
static void pal(uint8_t i, uint16_t c) { IO(0x210) = i; IO(0x212) = c; }
static void fill(uint32_t dst, uint16_t w, uint16_t h, uint8_t v)
{
    while (IO(0x816) & 1) ;
    set32(0x804, dst); IO(0x80A) = W; IO(0x80C) = w; IO(0x80E) = h; IO(0x812) = v; IO(0x814) = 1;
    while (IO(0x816) & 1) ;
}
static void text(int col, int row, const char *s, uint8_t attr)
{
    volatile uint16_t *t = (volatile uint16_t *)TXT + row * 80 + col;
    while (*s) *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

int main(void)
{
    uint16_t i, f0, y;
    volatile uint8_t *a = (volatile uint8_t *)A0;
    for (y = 0; y < H; y++)                         /* A at 4 MB: vertical colour bars, by the CPU */
        for (i = 0; i < W; i++) a[(uint32_t)y * W + i] = (uint8_t)(1 + i / 80);
    fill(A1, W, H, 20);                             /* A at 7 MB: the blitter, a frame and a block */
    fill(A1 + 100UL * W + 200, 240, 160, 21);
    fill(BB, W, H, 0);                              /* B at 6 MB: transparent, one box */
    fill(BB + 40UL * W + 40, 160, 80, 30);
    for (i = 0; i < 2000; i++) ((volatile uint16_t *)TXT)[i] = 0x4020;
    text(2, 1, "TEXT AT 7F0000  A AT 400000 / 700000  B AT 600000  LSCR AT 300000  SPRITES AT 500000", 0x47);
    for (i = 0; i < 32 * 32; i++)                   /* a sprite image at 5 MB: a ring */
        ((volatile uint8_t *)SPR)[i] = (uint8_t)(((i % 32 - 16) * (i % 32 - 16) + (i / 32 - 16) * (i / 32 - 16) < 200) ? 40 : 0);
    for (i = 0; i < 8; i++) pal((uint8_t)(1 + i), (uint16_t)((i & 1 ? 0xF800 : 0) | (i & 2 ? 0x07E0 : 0) | (i & 4 ? 0x001F : 0) | 0x18E3));
    pal(20, 0x2104); pal(21, 0xFFE0); pal(30, 0xF81F); pal(40, 0x07FF);
    for (y = 0; y < H; y++) {                       /* line scroll at 3 MB: a wave on A */
        volatile uint16_t *l = (volatile uint16_t *)LS + 2 * y;
        l[0] = (uint16_t)((y / 8) % 2 ? 16 : 0); l[1] = 0;
    }
    IO(0x204) = (uint16_t)TXT; IO(0x206) = (uint16_t)(TXT >> 16);
    set32(0x208, A0); set32(0x224, BB); set32(0x22C, LS);
    for (i = 0; i < 8; i++) {                       /* eight sprites from 5 MB */
        IO(0x230) = (uint16_t)(i * 4);
        IO(0x232) = (uint16_t)(32 + 60 + i * 64);
        IO(0x232) = (uint16_t)((32 + 280) | 31 << 11);
        IO(0x232) = (uint16_t)SPR;
        IO(0x232) = (uint16_t)(0x8000 | 2 << 13 | ((SPR >> 16) & 0x7F));
    }
    IO(0x200) = 0x07;                               /* text, bitmap, hi-colour */
    IO(0x220) = 0x0F;                               /* B in front, line scroll, sprites */
    CON_OUT = 'v'; CON_OUT = 'i'; CON_OUT = 'd'; CON_OUT = '\n';
    for (i = 0; i < 10; i++) {                      /* flip A each second */
        f0 = IO(0xA);
        while ((uint16_t)(IO(0xA) - f0) < 60) ;
        set32(0x208, (i & 1) ? A0 : A1);
    }
    return 0;
}
