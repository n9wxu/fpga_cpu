/* 64-bit multiply and divide (runtime/asm/muldi3.s, divdi3.s): operands of
 * every width from 1 to 64 bits, both signs, divisors under and over 32
 * bits, Doom's FixedMul/FixedDiv shapes, and edge values. */
#include "t.h"

static uint64_t r64(void)
{
    uint64_t v = (uint64_t)rnd() << 32 | rnd();
    unsigned bits = rnd() % 64 + 1;           /* random magnitude */
    v = bits == 64 ? v : v & (((uint64_t)1 << bits) - 1);
    return v;
}

static int32_t fixmul(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 16); }
static int32_t fixdiv(int32_t a, int32_t b) { return (int32_t)(((int64_t)a << 16) / b); }

int main(void)
{
    static const uint64_t edge[] = {0, 1, 2, 3, 0xFFFF, 0x10000, 0xFFFFFFFFu, 0x100000000ull,
                                    0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull,
                                    0xFFFFFFFFFFFFFFFFull, 0x123456789ABCDEFull};
    uint32_t h[10] = {0};
    int i, j;
    for (i = 0; i < 3000; i++) {
        uint64_t a = r64(), b = r64();
        int64_t sa = (int64_t)a, sb = (int64_t)b;
        if (rnd() & 1) sa = -sa;
        if (rnd() & 1) sb = -sb;
        uint64_t m = OPAQUE(a) * b;
        h[0] = mix(mix(h[0], (uint32_t)m), (uint32_t)(m >> 32));
        m = (uint64_t)(sa * sb);
        h[1] = mix(mix(h[1], (uint32_t)m), (uint32_t)(m >> 32));
        if (b) {
            m = a / b; h[2] = mix(mix(h[2], (uint32_t)m), (uint32_t)(m >> 32));
            m = a % b; h[3] = mix(mix(h[3], (uint32_t)m), (uint32_t)(m >> 32));
        }
        if (sb && !(sa == INT64_MIN && sb == -1)) {
            m = (uint64_t)(sa / sb); h[4] = mix(mix(h[4], (uint32_t)m), (uint32_t)(m >> 32));
            m = (uint64_t)(sa % sb); h[5] = mix(mix(h[5], (uint32_t)m), (uint32_t)(m >> 32));
        }
        {   /* 32-bit divide: the skip-16 / skip-8 paths need small values */
            uint32_t u = rnd() >> (rnd() % 32), v = rnd() >> (rnd() % 32);
            int32_t su = (rnd() & 1) ? -(int32_t)(u >> 1) : (int32_t)(u >> 1);
            int32_t sv = (rnd() & 1) ? -(int32_t)(v >> 1) : (int32_t)(v >> 1);
            if (v)
                h[8] = mix(mix(h[8], OPAQUE(u) / v), u % v);
            if (sv)
                h[9] = mix(mix(h[9], (uint32_t)(OPAQUE(su) / sv)), (uint32_t)(su % sv));
        }
        {
            int32_t x = (int32_t)rnd() >> (rnd() % 31), y = (int32_t)rnd() >> (rnd() % 31);
            h[6] = mix(h[6], (uint32_t)fixmul(x, y));
            if (y && (x < 0 ? -(int64_t)x : x) >> 14 < (y < 0 ? -(int64_t)y : y))
                h[7] = mix(h[7], (uint32_t)fixdiv(x, y));
        }
    }
    for (i = 0; i < 12; i++)
        for (j = 0; j < 12; j++) {
            uint64_t a = edge[i], b = edge[j];
            h[0] = mix(h[0], (uint32_t)(OPAQUE(a) * b ^ (a * b) >> 32));
            if (b) {
                h[2] = mix(h[2], (uint32_t)(a / b) ^ (uint32_t)((a / b) >> 32));
                h[3] = mix(h[3], (uint32_t)(a % b) ^ (uint32_t)((a % b) >> 32));
            }
            if (b && !((int64_t)a == INT64_MIN && (int64_t)b == -1)) {
                h[4] = mix(h[4], (uint32_t)((int64_t)a / (int64_t)b));
                h[5] = mix(h[5], (uint32_t)((int64_t)a % (int64_t)b));
            }
        }
    PH("umul", h[0]); PH("smul", h[1]); PH("udiv", h[2]); PH("urem", h[3]);
    PH("sdiv", h[4]); PH("srem", h[5]); PH("fixmul", h[6]); PH("fixdiv", h[7]);
    PH("udiv32", h[8]); PH("sdiv32", h[9]);
    return 0;
}
