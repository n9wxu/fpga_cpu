#include "t.h"
#include <math.h>
#include <string.h>

static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float randf(void)
{
    uint32_t u = rnd();
    /* random sign, exponent in a moderate range, random mantissa */
    u = (u & 0x807FFFFF) | ((uint32_t)(100 + (rnd() >> 8) % 56) << 23);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

int main(void)
{
    uint32_t h[9] = {0};
    for (int i = 0; i < 1500; i++) {
        float a = randf(), b = randf();
        h[0] = mix(h[0], bits(a + b));
        h[1] = mix(h[1], bits(a - b));
        h[2] = mix(h[2], bits(a * b));
        h[3] = mix(h[3], bits(a / b));
        h[4] = mix(h[4], (uint32_t)((a < b) | (a <= b) << 1 | (a > b) << 2 | (a >= b) << 3 | (a == b) << 4 | (a != b) << 5));
        float s = a * 1000.0f;
        if (s > -2e9f && s < 2e9f)
            h[5] = mix(h[5], (uint32_t)(int32_t)s);
        int32_t k = (int32_t)rnd() >> (i % 31);
        h[6] = mix(h[6], bits((float)k));
        h[7] = mix(h[7], bits((float)(uint32_t)k));
        h[8] = mix(h[8], bits((float)(int16_t)k));
    }
    const char *names[] = {"fadd", "fsub", "fmul", "fdiv", "fcmp", "f2i", "i2f", "u2f", "s2f"};
    for (int i = 0; i < 9; i++)
        PH(names[i], h[i]);
    volatile float z = 0.0f;
    PH("inf", bits(1.0f / z));
    PH("nan<", (uint32_t)((z / z) < 1.0f));
    PH("-0", bits(-z));
    PH("denorm", bits(1e-38f * (float)OPAQUE(1) / 1000.0f));
    PI("trunc-7.9", (int32_t)(-7.9f * (float)OPAQUE(1)));
    PI("u16", (uint16_t)(60000.5f * (float)OPAQUE(1)));
    return 0;
}
