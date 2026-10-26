#include "t.h"

int main(void)
{
    uint32_t h[8] = {0};
    for (int i = 0; i < 400; i++) {
        int64_t a = (int64_t)((uint64_t)rnd() << 32 | rnd()), b = (int64_t)((uint64_t)rnd() << 32 | rnd());
        if (i & 1) b >>= (i % 50);
        uint64_t ua = (uint64_t)a, ub = (uint64_t)b, r;
        r = ua + ub; h[0] = mix(mix(h[0], (uint32_t)r), (uint32_t)(r >> 32));
        r = ua - ub; h[1] = mix(mix(h[1], (uint32_t)r), (uint32_t)(r >> 32));
        r = ua * ub; h[2] = mix(mix(h[2], (uint32_t)r), (uint32_t)(r >> 32));
        if (b) {
            r = (uint64_t)(a / b); h[3] = mix(mix(h[3], (uint32_t)r), (uint32_t)(r >> 32));
            r = ua % ub; h[4] = mix(mix(h[4], (uint32_t)r), (uint32_t)(r >> 32));
        }
        int n = i & 63;
        r = ua << n; h[5] = mix(mix(h[5], (uint32_t)r), (uint32_t)(r >> 32));
        r = (uint64_t)(a >> n); h[6] = mix(mix(h[6], (uint32_t)r), (uint32_t)(r >> 32));
        h[7] = mix(h[7], (uint32_t)((a < b) | (ua < ub) << 1 | (a == b) << 2));
    }
    const char *names[] = {"add", "sub", "mul", "sdiv", "urem", "shl", "ashr", "cmp"};
    for (int i = 0; i < 8; i++)
        PH(names[i], h[i]);
    return 0;
}
