/* fpga_cpu C runtime: single-precision math.
 *
 * float and double are both IEEE single on this target. These routines
 * use range reduction plus short polynomials: about 1 ulp for the basic
 * ones, a few ulp for pow. They work on the bit patterns where that is
 * cheaper than soft-float arithmetic.
 */
#include <math.h>
#include <stdint.h>

typedef union {
    float f;
    uint32_t u;
    int32_t i;
} fbits;

static float from_bits(uint32_t u)
{
    fbits b;
    b.u = u;
    return b.f;
}

static uint32_t to_bits(float f)
{
    fbits b;
    b.f = f;
    return b.u;
}

float fabsf(float x) { return from_bits(to_bits(x) & 0x7FFFFFFFu); }

float truncf(float x)
{
    uint32_t u = to_bits(x);
    int e = (int)((u >> 23) & 0xFF) - 127;
    if (e >= 23)
        return x;                       /* already integral (or inf/nan) */
    if (e < 0)
        return from_bits(u & 0x80000000u);
    return from_bits(u & ~(0x007FFFFFu >> e));
}

float floorf(float x)
{
    float t = truncf(x);
    return (t > x) ? t - 1.0f : t;
}

float ceilf(float x)
{
    float t = truncf(x);
    return (t < x) ? t + 1.0f : t;
}

float roundf(float x)
{
    return x < 0 ? -floorf(0.5f - x) : floorf(x + 0.5f);
}

float fmodf(float x, float y)
{
    if (y == 0 || x != x || y != y)
        return NAN;
    float ax = fabsf(x), ay = fabsf(y);
    if (ax < ay)
        return x;
    /* Subtract y scaled by powers of two: exact, no overflow. */
    float r = ax;
    while (r >= ay) {
        float s = ay;
        while (s * 2.0f <= r)
            s *= 2.0f;
        r -= s;
    }
    return x < 0 ? -r : r;
}

float ldexpf(float x, int e)
{
    uint32_t u = to_bits(x);
    int ex = (int)((u >> 23) & 0xFF);
    if (ex == 0 || ex == 0xFF)
        return x;                       /* zero/denormal: not scaled; inf/nan */
    ex += e;
    if (ex >= 0xFF)
        return (u & 0x80000000u) ? -INFINITY : INFINITY;
    if (ex <= 0)
        return from_bits(u & 0x80000000u);
    return from_bits((u & 0x807FFFFFu) | ((uint32_t)ex << 23));
}

float frexpf(float x, int *e)
{
    uint32_t u = to_bits(x);
    int ex = (int)((u >> 23) & 0xFF);
    if (ex == 0 || ex == 0xFF) {
        *e = 0;
        return x;
    }
    *e = ex - 126;
    return from_bits((u & 0x807FFFFFu) | ((uint32_t)126 << 23));
}

float sqrtf(float x)
{
    if (x <= 0)
        return x == 0 ? x : NAN;
    if (x != x || x > 3.4e38f)
        return x;
    /* Halve the exponent for a first guess, then Newton's method. */
    float y = from_bits((to_bits(x) >> 1) + 0x1FBD1DF5u);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}

#define LN2_HI 0.693145751953125f       /* ln 2, split for exact range reduction */
#define LN2_LO 1.42860682030941723212e-6f
#define INV_LN2 1.44269504088896341f

float expf(float x)
{
    if (x != x)
        return x;
    if (x > 88.72f)
        return INFINITY;
    if (x < -103.9f)
        return 0.0f;
    /* x = k ln2 + r, |r| <= ln2/2; exp(r) by its Taylor series. */
    float kf = floorf(x * INV_LN2 + 0.5f);
    int k = (int)kf;
    float r = (x - kf * LN2_HI) - kf * LN2_LO;
    float p = 1.0f + r * (1.0f + r * (0.5f + r * (1.0f / 6 + r * (1.0f / 24 + r * (1.0f / 120 +
                    r * (1.0f / 720 + r * (1.0f / 5040)))))));
    if (k < -125) {                     /* keep the result in range for ldexpf */
        p = ldexpf(p, -60);
        k += 60;
    }
    return ldexpf(p, k);
}

float logf(float x)
{
    if (x != x || x < 0)
        return NAN;
    if (x == 0)
        return -INFINITY;
    if (x > 3.4e38f)
        return x;
    int e;
    float m = frexpf(x, &e);            /* x = m 2^e, m in [0.5, 1) */
    if (m < 0.70710678f) {
        m *= 2.0f;
        e--;
    }
    /* log(m) = 2 atanh(s), s = (m-1)/(m+1), |s| <= 0.1716 */
    float s = (m - 1.0f) / (m + 1.0f);
    float s2 = s * s;
    float t = s2 * (1.0f / 3 + s2 * (1.0f / 5 + s2 * (1.0f / 7 + s2 * (1.0f / 9))));
    float lm = 2.0f * s + 2.0f * s * t;
    float ef = (float)e;
    return ef * LN2_HI + (lm + ef * LN2_LO);
}

float log10f(float x) { return logf(x) * 0.434294481903251828f; }

float powf(float x, float y)
{
    if (y == 0)
        return 1.0f;
    if (x == 1.0f)
        return 1.0f;
    if (x != x || y != y)
        return NAN;
    float yi = truncf(y);
    int integral = yi == y;
    if (integral && fabsf(y) <= 64) {
        /* Exact repeated squaring for small integer powers. */
        int n = (int)y;
        int neg = n < 0;
        if (neg)
            n = -n;
        float r = 1.0f, b = x;
        while (n) {
            if (n & 1)
                r *= b;
            b *= b;
            n >>= 1;
        }
        return neg ? 1.0f / r : r;
    }
    if (x == 0)
        return y > 0 ? 0.0f : INFINITY;
    if (x < 0) {
        if (!integral)
            return NAN;
        float r = expf(y * logf(-x));
        /* odd integer powers keep the sign */
        float half = y * 0.5f;
        return truncf(half) != half ? -r : r;
    }
    return expf(y * logf(x));
}

/* ---- trigonometry ---- */

#define PIO2_HI 1.5707963705062866f     /* pi/2 split for range reduction */
#define PIO2_LO -4.37113900018624283e-8f
#define TWO_OVER_PI 0.636619772367581343f

/* sin and cos on [-pi/4, pi/4] */
static float ksin(float x)
{
    float x2 = x * x;
    return x + x * x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880))));
}

static float kcos(float x)
{
    float x2 = x * x;
    return 1.0f + x2 * (-0.5f + x2 * (1.0f / 24 + x2 * (-1.0f / 720 + x2 * (1.0f / 40320))));
}

/* x = q (pi/2) + r, |r| <= pi/4; returns q mod 4 */
static int reduce(float x, float *r)
{
    float qf = floorf(x * TWO_OVER_PI + 0.5f);
    *r = (x - qf * PIO2_HI) - qf * PIO2_LO;
    long q = (long)qf;
    return (int)(q & 3);
}

float sinf(float x)
{
    if (x != x || fabsf(x) > 3.4e38f)
        return NAN;
    float r;
    switch (reduce(x, &r)) {
    case 0: return ksin(r);
    case 1: return kcos(r);
    case 2: return -ksin(r);
    default: return -kcos(r);
    }
}

float cosf(float x)
{
    if (x != x || fabsf(x) > 3.4e38f)
        return NAN;
    float r;
    switch (reduce(x, &r)) {
    case 0: return kcos(r);
    case 1: return -ksin(r);
    case 2: return -kcos(r);
    default: return ksin(r);
    }
}

float tanf(float x)
{
    if (x != x || fabsf(x) > 3.4e38f)
        return NAN;
    float r;
    int q = reduce(x, &r);
    float s = ksin(r), c = kcos(r);
    return (q & 1) ? -c / s : s / c;
}

#define PI_F 3.14159265358979f

float atanf(float x)
{
    if (x != x)
        return x;
    int neg = x < 0;
    float a = fabsf(x);
    float base = 0;
    int inv = a > 1.0f;
    if (inv)
        a = 1.0f / a;
    /* a in [0, 1]; shift by pi/6 when above tan(pi/12) = 0.2679 */
    if (a > 0.26794919f) {
        const float s3 = 1.73205080756888f;
        a = (a * s3 - 1.0f) / (s3 + a);
        base = PI_F / 6;
    }
    float a2 = a * a;
    float r = base + a * (1.0f + a2 * (-1.0f / 3 + a2 * (1.0f / 5 + a2 * (-1.0f / 7 + a2 * (1.0f / 9 +
                                     a2 * (-1.0f / 11))))));
    if (inv)
        r = PI_F / 2 - r;
    return neg ? -r : r;
}

float atan2f(float y, float x)
{
    if (x != x || y != y)
        return NAN;
    if (x == 0) {
        if (y == 0)
            return 0;
        return y > 0 ? PI_F / 2 : -PI_F / 2;
    }
    float a = atanf(y / x);
    if (x > 0)
        return a;
    return y >= 0 ? a + PI_F : a - PI_F;
}

float asinf(float x)
{
    if (x != x || fabsf(x) > 1.0f)
        return NAN;
    return atan2f(x, sqrtf((1.0f - x) * (1.0f + x)));
}

float acosf(float x)
{
    if (x != x || fabsf(x) > 1.0f)
        return NAN;
    return atan2f(sqrtf((1.0f - x) * (1.0f + x)), x);
}
