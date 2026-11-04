/* T1 self-check for fpga/rtl/t1test.v (block RAM only, 4 KB at 0x1000):
 * arithmetic, logic, shifts, multiply, memory bytes/words/pairs, calls and
 * branches, folded into a 16-bit checksum shown on the 7-segment display
 * (and printed in fsim). */
#include <stdint.h>
#include <fcpu.h>

#define SEG7 IO16(0xF00606)
#define LEDS IO16(0xF00600)

static uint8_t b[64];
static uint16_t w[32];
static uint32_t p[16];

static uint16_t mix(uint16_t s, uint16_t v) { return (uint16_t)((s << 5 | s >> 11) ^ v); }

static uint16_t fib(int n) { return n < 2 ? (uint16_t)n : (uint16_t)(fib(n - 1) + fib(n - 2)); }

int main(void)
{
    uint16_t s = 0x1234;
    for (int i = 0; i < 64; i++) b[i] = (uint8_t)(i * 37 + 11);
    for (int i = 0; i < 64; i++) s = mix(s, b[i]);
    LEDS = 1;
    for (int i = 0; i < 32; i++) w[i] = (uint16_t)(b[2 * i] * 251u + b[2 * i + 1]);
    for (int i = 0; i < 32; i++) s = mix(s, w[i] ^ (uint16_t)(w[i] >> 3));
    LEDS = 2;
    for (int i = 0; i < 16; i++) p[i] = (uint32_t)w[i] * w[i + 16] + 0x01020304u * (uint32_t)i;
    for (int i = 0; i < 16; i++) s = mix(s, (uint16_t)(p[i] >> 7) ^ (uint16_t)p[i]);
    LEDS = 3;
    int16_t a = -1234;
    s = mix(s, (uint16_t)(a / 7)); s = mix(s, (uint16_t)(a % 7)); s = mix(s, (uint16_t)(a >> 3));
    s = mix(s, fib(12));
    LEDS = 4;
    SEG7 = s;
#ifdef FSIM
    return s;
#else
    LEDS = 0x80;
    for (;;) ;
#endif
}
