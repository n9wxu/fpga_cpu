#include "t.h"

int main(void)
{
    uint32_t h[12] = {0};
    for (int i = 0; i < 2000; i++) {
        int16_t a = (int16_t)rnd(), b = (int16_t)(rnd() >> 7);
        uint16_t ua = (uint16_t)a, ub = (uint16_t)b;
        if (i & 1) b = (int16_t)(b >> 9);          /* small divisors too */
        h[0] = mix(h[0], (uint16_t)(a + b));
        h[1] = mix(h[1], (uint16_t)(a - b));
        h[2] = mix(h[2], (uint16_t)(ua * ub));
        if (b != 0 && !(a == -32768 && b == -1)) {
            h[3] = mix(h[3], (uint16_t)(int16_t)(a / b));
            h[4] = mix(h[4], (uint16_t)(int16_t)(a % b));
        }
        if (ub) {
            h[5] = mix(h[5], (uint16_t)(ua / ub));
            h[6] = mix(h[6], (uint16_t)(ua % ub));
        }
        int n = i & 15;
        h[7] = mix(h[7], (uint16_t)(ua << n));
        h[8] = mix(h[8], (uint16_t)(ua >> n));
        h[9] = mix(h[9], (uint16_t)(a >> n));
        h[10] = mix(h[10], (uint32_t)((a < b) | (a <= b) << 1 | (a > b) << 2 | (a >= b) << 3 | (a == b) << 4 |
                                      (ua < ub) << 5 | (ua <= ub) << 6 | (ua > ub) << 7 | (ua >= ub) << 8 |
                                      (a != b) << 9));
        h[11] = mix(h[11], (uint16_t)((a & b) ^ (a | ub) ^ ~a));
    }
    const char *names[] = {"add", "sub", "mul", "sdiv", "srem", "udiv", "urem", "shl", "lshr", "ashr", "cmp", "logic"};
    for (int i = 0; i < 12; i++)
        PH(names[i], h[i]);
    PI("min/1", (int16_t)OPAQUE(-32768) / (int16_t)OPAQUE(1));
    PI("7/-2", (int16_t)OPAQUE(7) / (int16_t)OPAQUE(-2));
    PI("-7%2", (int16_t)OPAQUE(-7) % (int16_t)OPAQUE(2));
    PI("65535/3", (uint16_t)OPAQUE(-1) / (uint16_t)OPAQUE(3));
    return 0;
}
