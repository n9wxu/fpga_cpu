/* fpga_cpu C runtime: console stdio and a small printf.
 * Conversions: d i u x X o c s p %, flags - 0 + space #, width, precision,
 * length hh h l ll z t j; f e g (and capitals) with the exact digits of the
 * float (double is a float here). */
#include <fcpu.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int putchar(int c)
{
    CON_OUT = (uint16_t)(unsigned char)c;
    return (unsigned char)c;
}

int puts(const char *s)
{
    while (*s)
        putchar(*s++);
    putchar('\n');
    return 0;
}

int getchar(void)
{
    for (;;) {
        uint16_t c = CON_IN;
        if (c == 0xFFFE)
            return EOF;
        if (c != 0xFFFF)
            return c;
    }
}

typedef struct {
    char *buf;
    size_t cap, len;
} out_t;

static void emit(out_t *o, char c)
{
    if (!o->buf)
        putchar(c);
    else if (o->len + 1 < o->cap)
        o->buf[o->len] = c;
    o->len++;
}

static void emit_pad(out_t *o, char c, int n)
{
    while (n-- > 0)
        emit(o, c);
}

static int fmt_u64(char *tmp, uint64_t v, unsigned base, int upper)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        tmp[n++] = dig[v % base];
        v /= base;
    } while (v);
    return n;
}

static int fmt_u32(char *tmp, uint32_t v, unsigned base, int upper)
{
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int n = 0;
    do {
        tmp[n++] = dig[v % base];
        v /= base;
    } while (v);
    return n;
}

/* Floating point. A float is m 2^e, so its exact decimal expansion is m 2^e
 * (e >= 0) or m 5^-e / 10^-e: a decimal digit array that is multiplied by
 * 2 or 5 until e is used up. Rounding then works on exact digits, round half
 * to even like glibc, and any precision is right (the digits past the
 * float's own are the exact tail of the binary value). */
typedef struct {
    unsigned char d[120]; /* decimal digits, least significant first */
    int n;                /* digits in use (0 for zero) */
    int dp;               /* digits after the decimal point */
} dec_t;

#define F_LEFT 1
#define F_ZERO 2
#define F_PLUS 4
#define F_SPACE 8
#define F_ALT 16

static int dig(const dec_t *x, int i) { return i >= 0 && i < x->n ? x->d[i] : 0; }

static void dmul(dec_t *x, unsigned k)
{
    unsigned c = 0;
    for (int i = 0; i < x->n; i++) {
        unsigned v = x->d[i] * k + c;
        c = (v * 205) >> 11; /* v / 10, exact for v < 320 */
        x->d[i] = (unsigned char)(v - c * 10);
    }
    while (c) {
        unsigned q = (c * 205) >> 11;
        x->d[x->n++] = (unsigned char)(c - q * 10);
        c = q;
    }
}

/* x = f (finite, >= 0) */
static void dec_from(dec_t *x, uint32_t bits)
{
    int e = (int)(bits >> 23);
    uint32_t m = bits & 0x7FFFFF;
    if (e)
        m |= 0x800000;
    else
        e = 1;
    e -= 150;
    while (m && !(m & 1) && e < 0) {
        m >>= 1;
        e++;
    }
    x->n = 0;
    x->dp = 0;
    for (; m; m /= 10)
        x->d[x->n++] = (unsigned char)(m % 10);
    if (e < 0) {
        x->dp = -e;
        for (; e <= -2; e += 2)
            dmul(x, 25);
        if (e)
            dmul(x, 5);
    } else {
        for (; e >= 4; e -= 4)
            dmul(x, 16);
        for (; e; e--)
            dmul(x, 2);
    }
}

/* round to a multiple of 10^(cut - dp), half to even; digits below cut
 * become zero */
static void round_at(dec_t *x, int cut)
{
    if (cut <= 0)
        return;
    int half = dig(x, cut - 1), sticky = 0;
    for (int i = 0; i < cut - 1 && i < x->n; i++)
        sticky |= x->d[i];
    for (int i = 0; i < cut && i < x->n; i++)
        x->d[i] = 0;
    if (half > 5 || (half == 5 && (sticky || (dig(x, cut) & 1))))
        for (int i = cut;; i++) {
            if (i >= x->n)
                x->d[x->n++] = 0;
            if (++x->d[i] <= 9)
                break;
            x->d[i] = 0;
        }
}

static void fmt_float(out_t *o, uint32_t bits, int conv, int prec, int fl, int width)
{
    dec_t x;
    int upper = conv < 'a';
    conv |= 32;
    char sign = bits >> 31 ? '-' : (fl & F_PLUS) ? '+' : (fl & F_SPACE) ? ' ' : 0;
    bits &= 0x7FFFFFFF;
    int style_e = 0, frac = 0, X = 0, t = 0, intd = 0, len;
    const char *special = NULL;
    if (bits >= 0x7F800000) {
        special = bits > 0x7F800000 ? (upper ? "NAN" : "nan") : upper ? "INF" : "inf";
        len = 3;
    } else {
        dec_from(&x, bits);
        if (prec < 0)
            prec = 6;
        if (conv == 'f') {
            frac = prec;
            round_at(&x, x.dp - prec);
        } else {
            int P = conv == 'g' ? (prec ? prec : 1) : prec + 1; /* significant digits */
            if (x.n) {
                round_at(&x, x.n - P);
                X = x.n - 1 - x.dp;
            }
            if (conv == 'e' || X >= P || X < -4) {
                style_e = 1;
                frac = P - 1;
            } else {
                frac = P - 1 - X;
            }
            if (conv == 'g' && !(fl & F_ALT))
                while (frac > 0 && dig(&x, style_e ? x.n - 1 - frac : x.dp - frac) == 0)
                    frac--;
        }
        t = x.n - 1;
        intd = style_e ? 1 : x.n > x.dp ? x.n - x.dp : 1;
        len = intd + ((frac || (fl & F_ALT)) ? 1 : 0) + frac + (style_e ? 4 : 0);
    }
    int pad = width - len - (sign != 0);
    if (!(fl & F_LEFT) && !(fl & F_ZERO && !special))
        emit_pad(o, ' ', pad);
    if (sign)
        emit(o, sign);
    if (!(fl & F_LEFT) && (fl & F_ZERO) && !special)
        emit_pad(o, '0', pad);
    if (special) {
        for (; *special; special++)
            emit(o, *special);
    } else {
        int hi = style_e ? t : (x.n > x.dp ? x.n - 1 : x.dp), lo = style_e ? t : x.dp;
        for (int i = hi; i >= lo; i--)
            emit(o, (char)('0' + dig(&x, i)));
        if (frac || (fl & F_ALT))
            emit(o, '.');
        for (int k = 1; k <= frac; k++)
            emit(o, (char)('0' + dig(&x, lo - k)));
        if (style_e) {
            emit(o, upper ? 'E' : 'e');
            emit(o, X < 0 ? '-' : '+');
            if (X < 0)
                X = -X;
            emit(o, (char)('0' + X / 10));
            emit(o, (char)('0' + X % 10));
        }
    }
    if (fl & F_LEFT)
        emit_pad(o, ' ', pad);
}

/* buf NULL: the console */
static int vfmt(char *buf, size_t cap, const char *fmt, va_list ap)
{
    out_t o = {buf, cap, 0};
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            emit(&o, *fmt);
            continue;
        }
        fmt++;
        int fl = 0;
        for (;; fmt++) {
            if (*fmt == '-') fl |= F_LEFT;
            else if (*fmt == '0') fl |= F_ZERO;
            else if (*fmt == '+') fl |= F_PLUS;
            else if (*fmt == ' ') fl |= F_SPACE;
            else if (*fmt == '#') fl |= F_ALT;
            else break;
        }
        int width = 0, prec = -1;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0) {
                fl |= F_LEFT;
                width = -width;
            }
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    prec = prec * 10 + (*fmt++ - '0');
            }
        }
        int lng = 0, sh = 0; /* l count (z and t are long, j is long long); h count */
        for (;; fmt++) {
            if (*fmt == 'l') lng++;
            else if (*fmt == 'h') sh++;
            else if (*fmt == 'z' || *fmt == 't') lng = 1;
            else if (*fmt == 'j') lng = 2;
            else break;
        }
        char tmp[24];
        const char *s = tmp;
        int len = 0;
        char conv = *fmt;
        switch (conv) {
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'o': case 'p': {
            unsigned base = conv == 'u' || conv == 'd' || conv == 'i' ? 10 : conv == 'o' ? 8 : 16;
            char sign = 0;
            uint64_t u;
            if (conv == 'p')
                lng = 1;
            if (conv == 'd' || conv == 'i') {
                int64_t v = lng >= 2 ? va_arg(ap, int64_t) : lng ? va_arg(ap, long) : va_arg(ap, int);
                if (sh)
                    v = sh >= 2 ? (signed char)v : (short)v;
                sign = v < 0 ? '-' : (fl & F_PLUS) ? '+' : (fl & F_SPACE) ? ' ' : 0;
                u = v < 0 ? (uint64_t)-v : (uint64_t)v;
            } else {
                u = lng >= 2 ? va_arg(ap, uint64_t) : lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                if (sh)
                    u = sh >= 2 ? (unsigned char)u : (unsigned short)u;
            }
            len = prec == 0 && u == 0 ? 0
                : lng >= 2 ? fmt_u64(tmp, u, base, conv == 'X') : fmt_u32(tmp, (uint32_t)u, base, conv == 'X');
            int zeros = prec > len ? prec - len : 0;
            int pre = ((fl & F_ALT) && base == 16 && u) || conv == 'p' ? 2 : 0;
            if ((fl & F_ALT) && base == 8 && !zeros && (!len || tmp[len - 1] != '0'))
                zeros = 1;
            int pad = width - len - zeros - pre - (sign != 0);
            if ((fl & F_ZERO) && prec < 0 && !(fl & F_LEFT)) {
                zeros += pad > 0 ? pad : 0;
                pad = 0;
            }
            if (!(fl & F_LEFT))
                emit_pad(&o, ' ', pad);
            if (sign)
                emit(&o, sign);
            if (pre) {
                emit(&o, '0');
                emit(&o, conv == 'X' ? 'X' : 'x');
            }
            emit_pad(&o, '0', zeros);
            while (len)
                emit(&o, tmp[--len]);
            if (fl & F_LEFT)
                emit_pad(&o, ' ', pad);
            continue;
        }
        case 'f': case 'e': case 'g': case 'F': case 'E': case 'G': {
            union { float f; uint32_t u; } b;
            b.f = (float)va_arg(ap, double);
            fmt_float(&o, b.u, conv, prec, fl, width);
            continue;
        }
        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            len = 1;
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            len = (int)strlen(s);
            if (prec >= 0 && len > prec)
                len = prec;
            break;
        case '%':
            tmp[0] = '%';
            len = 1;
            break;
        default:
            continue;
        }
        if (!(fl & F_LEFT))
            emit_pad(&o, ' ', width - len);
        for (int i = 0; i < len; i++)
            emit(&o, s[i]);
        if (fl & F_LEFT)
            emit_pad(&o, ' ', width - len);
    }
    if (buf && cap)
        buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}

int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    char none; /* snprintf(NULL, 0, ...) only counts */
    return buf ? vfmt(buf, cap, fmt, ap) : vfmt(&none, 0, fmt, ap);
}

int vprintf(const char *fmt, va_list ap) { return vfmt(NULL, 0, fmt, ap); }

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfmt(NULL, 0, fmt, ap);
    va_end(ap);
    return n;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, 0x7FFF, fmt, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *buf, const char *fmt, va_list ap) { return vsnprintf(buf, 0x7FFF, fmt, ap); }
