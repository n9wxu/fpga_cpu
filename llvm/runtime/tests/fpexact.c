/* Float add, subtract and multiply, bit for bit: random operands over the
 * whole exponent range (zeros, subnormals, infinities and NaNs included),
 * near-equal pairs for cancellation, and rounding ties. NaNs hash as one
 * value (their payloads are not specified). */
#include "t.h"
#include <string.h>

static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float mk(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t canon(float f)
{
    uint32_t u = bits(f);
    return (u & 0x7F800000) == 0x7F800000 && (u & 0x7FFFFF) ? 0x7FC00000 : u;
}

static uint32_t rf(void)
{
    uint32_t u = rnd();
    switch (rnd() % 8) {
    case 0: return u & 0x807FFFFF;                     /* zero / subnormal */
    case 1: return (u & 0x80000000) | 0x7F800000 | ((rnd() & 3) ? 0 : (u & 0x7FFFFF)); /* inf/NaN */
    case 2: return (u & 0x807FFFFF) | ((rnd() % 3 + 1) << 23); /* tiny normals */
    case 3: return (u & 0x807FFFFF) | ((252 + rnd() % 3) << 23); /* huge */
    default: return u;
    }
}

int main(void)
{
    uint32_t h[4] = {0};
    volatile float x, y;
    int i;
    for (i = 0; i < 6000; i++) {
        uint32_t a = rf(), b;
        switch (rnd() % 4) {
        case 0: b = a ^ (rnd() & 0x8000000F); break;          /* cancellation */
        case 1: b = (a & 0xFF800000) ^ (rnd() & 0x87FFFFFF);   /* close exponents */
        default: b = rf();
        }
        x = mk(a); y = mk(b);
        h[0] = mix(h[0], canon(x + y));
        h[1] = mix(h[1], canon(x - y));
        h[2] = mix(h[2], canon(x * y));
        h[3] = mix(h[3], canon(y * x) ^ canon(y - x));
    }
    PH("add", h[0]); PH("sub", h[1]); PH("mul", h[2]); PH("mixed", h[3]);
    return 0;
}
