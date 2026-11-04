/* How far into the frame the demo's palette update reaches: the scan line
 * (VSTAT) after the scroll writes and 255 palette writes, for 4 frames. */
#include <stdint.h>
#include <fcpu.h>

#define REG(a) IO16(a)
static uint16_t pal[256];

static void num(uint16_t v)
{
    char b[6]; int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) CON_OUT = (uint8_t)b[--n];
}

int main(void)
{
    for (int i = 0; i < 256; i++) pal[i] = (uint16_t)(i * 257);
    uint8_t phase = 0;
    uint16_t last = FRAME;
    for (int f = 0; f < 4; f++) {
        while (FRAME == last) ;
        last = FRAME;
        uint16_t l0 = VSTAT >> 3;
        REG(0xF00214) = (uint16_t)(640 + phase);
        REG(0xF00216) = (uint16_t)(360 + phase);
        REG(0xF00210) = 1;
        for (uint16_t i = 1; i < 256; i++)
            REG(0xF00212) = pal[(uint8_t)(i + phase)];
        uint16_t l1 = VSTAT >> 3;
        num(l0); CON_OUT = ' '; num(l1); CON_OUT = '\n';
        phase++;
    }
    return 0;
}
