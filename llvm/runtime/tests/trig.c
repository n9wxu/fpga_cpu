#include "t.h"
#include <math.h>

/* sinf/cosf/tanf over small, medium and huge arguments (up to FLT_MAX),
 * compared with the host's results to a few ulp (the "@N" lines of run.py).
 * Large arguments need a real Payne-Hanek reduction. */
static float fbits(uint32_t u)
{
    union { float f; uint32_t u; } b;
    b.u = u;
    return b.f;
}

static uint32_t ubits(float f)
{
    union { float f; uint32_t u; } b;
    b.f = f;
    return b.u;
}

#define PU(n, tol, v) printf("@%d %-10s %08lx\n", tol, n, (unsigned long)ubits(v))

static void one(int i, float x)
{
    char nm[16];
    snprintf(nm, sizeof nm, "sin%d", i);
    PU(nm, 3, sinf(x));
    snprintf(nm, sizeof nm, "cos%d", i);
    PU(nm, 3, cosf(x));
    snprintf(nm, sizeof nm, "tan%d", i);
    PU(nm, 4, tanf(x));
    /* the results stay in range */
    float s = sinf(x), c = cosf(x);
    PI("range", s >= -1.0f && s <= 1.0f && c >= -1.0f && c <= 1.0f);
}

int main(void)
{
    static const float fixed[] = {1e10f, -1e10f, 1e20f, 1e30f, 3.4028235e38f, -3.4028235e38f, 4096.0f, 4095.5f,
                                  8192.0f, 6433.0f, 100000.0f, 1048576.0f, 16777216.0f, 3.0e7f, 0.5e-9f,
                                  1000.0f, -1000.0f, 3.14159274f, 6.28318548f, 1.57079637f, 31415.9275f,
                                  2147483648.0f, 4294967296.0f, 1.0e15f};
    int i, n = 0;
    for (i = 0; i < (int)(sizeof fixed / sizeof fixed[0]); i++)
        one(n++, fixed[i]);
    /* multiples of pi/2 (as floats), where the reduction must be exact */
    for (i = 1; i < 40; i += 3)
        one(n++, (float)i * 1.57079637f);
    /* random exponents from 2^-10 to 2^127, random mantissas */
    for (i = 0; i < 90; i++) {
        uint32_t e = 117 + rnd() % 138;       /* biased: 2^-10 .. 2^127 */
        uint32_t u = (rnd() & 0x80000000u) | e << 23 | (rnd() & 0x7FFFFF);
        one(n++, fbits(u));
    }
    return 0;
}
