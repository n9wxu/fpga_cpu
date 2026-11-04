/* Stippled text backgrounds over a hi-color bitmap and over the backdrop,
 * for comparing tang/rtl/system.v's true-color video with fsim
 * --true-color (a 50% blend there; a checkerboard on 3-bit video). Only the
 * first 28 bitmap lines are drawn, to keep the simulation short. */
#include <stdint.h>
#include <fcpu.h>

#define REG(a) IO16(a)
#define TEXT ((volatile uint16_t *)0x0FC000)
#define BMP  ((volatile uint8_t *)0x0C3C00)

int main(void)
{
    for (int i = 0; i < 2000; i++) TEXT[i] = 0x4720;
    REG(0xF00210) = 0;
    for (unsigned i = 0; i < 256; i++)
        REG(0xF00212) = (uint16_t)((i >> 3) << 11 | (i >> 2) << 5 | (31 - (i >> 3)));
    for (unsigned y = 0; y < 28; y++)
        for (unsigned x = 0; x < 640; x++)
            BMP[(uint32_t)y * 640u + x] = (uint8_t)(x * 2 / 5 + y);
    /* row 0: every background color stippled over the bitmap, white text */
    for (int c = 0; c < 80; c++)
        TEXT[c] = (uint16_t)(0x8000 | ((c / 10) & 7) << 11 | 0x0700 | "Stipple!  "[c % 10]);
    /* row 2: stippled over the backdrop (green) */
    for (int c = 0; c < 80; c++)
        TEXT[2 * 80 + c] = (uint16_t)(0x8000 | ((c / 10) & 7) << 11 | 0x0600 | 'x');
    REG(0xF00202) = 2;
    REG(0xF00200) = 0x07;
    uint16_t f = FRAME;
#ifdef FSIM
    while ((uint16_t)(FRAME - f) < 2) ;
    return 0;
#else
    for (;;) ;                              /* the testbench takes a frame */
#endif
}
