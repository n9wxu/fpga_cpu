/* Video test for fpga/rtl/video.v: the picture comes with the image (the
 * bitmap at 0C3C00 and the text at 0FC000, see tools/mkvidtest.py); this
 * sets the palette and the registers, then exits. The frame after exit
 * must equal fsim --screenshot. HICOL=0 tests the 8-color bitmap. */
#include <stdint.h>
#include <fcpu.h>

#ifndef HICOL
#define HICOL 1
#endif
#define REG(a) IO16(a)

int main(void)
{
    REG(0xF00210) = 0;
    for (unsigned i = 0; i < 256; i++) {
        unsigned r = (i * 7) & 31, g = (i * 13 + 5) & 63, b = (255 - i) >> 3;
        REG(0xF00212) = (uint16_t)(r << 11 | g << 5 | b);
    }
    REG(0xF00202) = 3;                     /* backdrop cyan */
    REG(0xF00214) = 700;                   /* scroll x: 700 mod 640 = 60 */
    REG(0xF00216) = 391;                   /* scroll y: 391 mod 360 = 31 */
    REG(0xF00218) = 3 * 80 + 5;            /* cursor */
    REG(0xF00200) = 0x23 | (HICOL ? 4 : 0);   /* text, bitmap, cursor (+ hi-color) */
    return 0;
}
