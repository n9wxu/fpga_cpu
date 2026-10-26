/* Number formatting and parsing. float is the only floating type, as on
 * the target, where double is also 32 bits. */
#include <math.h>

#include "basic.h"

static const float p10[11] = { 1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f };

/* x * 10^e, with as few roundings as possible (10^0..10^10 are exact). */
float scale10(float x, int e)
{
    while (e > 10) {
        x *= 1e10f;
        e -= 10;
    }
    while (e < -10) {
        x /= 1e10f;
        e += 10;
    }
    return e >= 0 ? x * p10[e] : x / p10[-e];
}

int fmt_u32(uint32_t v, char *out)
{
    char t[11];
    int n = 0, i;
    do {
        t[n++] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    } while (v);
    for (i = 0; i < n; i++)
        out[i] = t[n - 1 - i];
    out[n] = 0;
    return n;
}

/* a * b = p + err exactly (Dekker), using only float multiply and add. */
static float two_prod(float a, float b, float *err)
{
    float p = a * b, t, ah, al, bh, bl;
    t = 4097.0f * a;
    ah = t - (t - a);
    al = a - ah;
    t = 4097.0f * b;
    bh = t - (t - b);
    bl = b - bh;
    *err = ((ah * bh - p) + ah * bl + al * bh) + al * bl;
    return p;
}

/* round(x * 10^k) for a result in [1e6, 1e7], correct to the last digit
 * whenever the final scaling step is exact. */
static uint32_t scaled_round(float x, int k)
{
    float hi, lo, f;
    uint32_t r;
    while (k > 10) {
        x *= 1e10f;
        k -= 10;
    }
    while (k < -10) {
        x /= 1e10f;
        k += 10;
    }
    if (k >= 0)
        hi = two_prod(x, p10[k], &lo);
    else {
        float d = p10[-k], e;
        float q = x / d, pp = two_prod(q, d, &e);
        hi = q;
        lo = ((x - pp) - e) / d;
    }
    r = (uint32_t)hi;
    f = (hi - (float)r) + lo;
    while (f >= 1.0f) {
        r++;
        f -= 1.0f;
    }
    while (f < 0.0f) {
        r--;
        f += 1.0f;
    }
    return f >= 0.5f ? r + 1u : r;
}

/* MS-BASIC style: a leading space or minus sign, 7 significant digits,
 * no leading zero (".5"), E notation outside 0.01 .. 9999999. Whole
 * numbers below 2^24 are exact and print as integers. */
int fmt_num(float x, char *out)
{
    char *o = out;
    char d[8];
    int e = 0, nd, i;
    uint32_t m;

    if (x < 0) {
        *o++ = '-';
        x = -x;
    } else
        *o++ = ' ';
    if (x == 0) {
        *o++ = '0';
        *o = 0;
        return (int)(o - out);
    }
    if (x < 16777216.0f && x == floorf(x))
        return (int)(o - out) + fmt_u32((uint32_t)x, o);

    if (x >= 1.0f) {
        while (e < 38 && x >= scale10(1.0f, e + 1))
            e++;
    } else {
        while (e > -45 && x < scale10(1.0f, e))
            e--;
    }
    m = scaled_round(x, 6 - e);
    if (m >= 10000000UL) {
        m = (m + 5u) / 10u;
        e++;
    }
    if (m < 1000000UL) {
        m *= 10u;
        e--;
    }
    for (i = 6; i >= 0; i--) {
        d[i] = (char)('0' + (int)(m % 10u));
        m /= 10u;
    }
    nd = 7;
    while (nd > 1 && d[nd - 1] == '0')
        nd--;

    if (e >= 7 || e < -2) {
        *o++ = d[0];
        if (nd > 1) {
            *o++ = '.';
            for (i = 1; i < nd; i++)
                *o++ = d[i];
        }
        *o++ = 'E';
        *o++ = e < 0 ? '-' : '+';
        if (e < 0)
            e = -e;
        *o++ = (char)('0' + e / 10);
        *o++ = (char)('0' + e % 10);
    } else if (e >= 0) {
        for (i = 0; i <= e; i++)
            *o++ = i < nd ? d[i] : '0';
        if (nd > e + 1) {
            *o++ = '.';
            for (i = e + 1; i < nd; i++)
                *o++ = d[i];
        }
    } else {
        *o++ = '.';
        for (i = -1; i > e; i--)
            *o++ = '0';
        for (i = 0; i < nd; i++)
            *o++ = d[i];
    }
    *o = 0;
    return (int)(o - out);
}

static int isdig(char c) { return c >= '0' && c <= '9'; }

/* Decimal number: digits [. digits] [E [+-] digits], or a leading '.'.
 * *isint is set when it was written as an integer that fits in 16 bits. */
const char *parse_num(const char *s, float *f, int *isint)
{
    uint32_t mant = 0;
    int nd = 0, e = 0, dig = 0, isf = 0;

    while (isdig(*s)) {
        if (nd < 9) {
            mant = mant * 10u + (uint32_t)(*s - '0');
            if (mant)
                nd++;
        } else
            e++;
        dig = 1;
        s++;
    }
    if (*s == '.') {
        isf = 1;
        s++;
        while (isdig(*s)) {
            if (nd < 9) {
                mant = mant * 10u + (uint32_t)(*s - '0');
                if (mant)
                    nd++;
                e--;
            }
            dig = 1;
            s++;
        }
    }
    if (!dig)
        return NULL;
    if ((*s == 'E' || *s == 'e') &&
        (isdig(s[1]) || ((s[1] == '+' || s[1] == '-') && isdig(s[2])))) {
        int neg = 0, x = 0;
        isf = 1;
        s++;
        if (*s == '+' || *s == '-')
            neg = *s++ == '-';
        while (isdig(*s)) {
            if (x < 100)
                x = x * 10 + (*s - '0');
            s++;
        }
        e += neg ? -x : x;
    }
    *f = mant ? scale10((float)mant, e) : 0.0f;
    *isint = !isf && *f <= 32767.0f;
    return s;
}
