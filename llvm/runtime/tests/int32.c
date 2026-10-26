#include "t.h"

int main(void)
{
    uint32_t h[13] = {0};
    for (int i = 0; i < 1500; i++) {
        int32_t a = (int32_t)rnd(), b = (int32_t)rnd();
        if (i & 1) b >>= (i % 29);
        uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
        h[0] = mix(h[0], ua + ub);
        h[1] = mix(h[1], ua - ub);
        h[2] = mix(h[2], ua * ub);
        if (b != 0 && !(a == INT32_MIN && b == -1)) {
            h[3] = mix(h[3], (uint32_t)(a / b));
            h[4] = mix(h[4], (uint32_t)(a % b));
        }
        if (ub) {
            h[5] = mix(h[5], ua / ub);
            h[6] = mix(h[6], ua % ub);
        }
        int n = i & 31;
        h[7] = mix(h[7], ua << n);
        h[8] = mix(h[8], ua >> n);
        h[9] = mix(h[9], (uint32_t)(a >> n));
        h[10] = mix(h[10], (uint32_t)((a < b) | (a <= b) << 1 | (a > b) << 2 | (a >= b) << 3 | (a == b) << 4 |
                                      (ua < ub) << 5 | (ua <= ub) << 6 | (ua > ub) << 7 | (ua >= ub) << 8 |
                                      (a != b) << 9));
        h[11] = mix(h[11], (a & b) ^ (ua | 0x12345678) ^ ~ua ^ (ua & 0xFF00FF));
        /* constant shifts */
        h[12] = mix(h[12], (ua << 1) ^ (ua << 7) ^ (ua << 16) ^ (ua << 20) ^ (ua >> 1) ^ (ua >> 15) ^ (ua >> 16) ^
                               (ua >> 27) ^ (uint32_t)(a >> 3) ^ (uint32_t)(a >> 16) ^ (uint32_t)(a >> 31));
    }
    const char *names[] = {"add", "sub", "mul", "sdiv", "srem", "udiv", "urem", "shl", "lshr", "ashr", "cmp", "logic", "constshift"};
    for (int i = 0; i < 13; i++)
        PH(names[i], h[i]);
    int16_t s = (int16_t)OPAQUE(-5);
    uint16_t u = (uint16_t)OPAQUE(40000);
    PI("sext", (int32_t)s);
    PI("zext", (int32_t)u);
    PI("trunc", (int16_t)(int32_t)OPAQUE(0x12345));
    PI("mixed", (int32_t)s * 100000 + u);
    return 0;
}
