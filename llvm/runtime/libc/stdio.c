/* fpga_cpu C runtime: console stdio and a small printf.
 * Conversions: d i u x X o c s p %, flags - 0 + space, width, precision,
 * length l (32 bits) and ll (64 bits); f e g for floats. */
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

/* Fixed-point float formatting, enough for printf("%f") debugging. */
static int fmt_float(char *out, float v, int prec, char conv)
{
    int n = 0;
    if (v != v)
        return (int)(strcpy(out, "nan"), 3);
    if (v < 0) {
        out[n++] = '-';
        v = -v;
    }
    if (v > 3.4e38f)
        return (int)(strcpy(out + n, "inf"), n + 3);
    int exp10 = 0;
    if (conv == 'e' || (conv == 'g' && (v >= 1e7f || (v < 1e-4f && v != 0)))) {
        while (v >= 10.0f) { v /= 10.0f; exp10++; }
        while (v < 1.0f && v != 0) { v *= 10.0f; exp10--; }
        conv = 'e';
    }
    if (prec > 7)
        prec = 7;
    float r = 0.5f;
    for (int i = 0; i < prec; i++)
        r /= 10.0f;
    v += r;
    if (conv == 'e' && v >= 10.0f) { v /= 10.0f; exp10++; }
    uint32_t ip = (uint32_t)v;
    float frac = v - (float)ip;
    char tmp[12];
    int k = fmt_u32(tmp, ip, 10, 0);
    while (k)
        out[n++] = tmp[--k];
    if (prec) {
        out[n++] = '.';
        for (int i = 0; i < prec; i++) {
            frac *= 10.0f;
            int d = (int)frac;
            out[n++] = (char)('0' + d);
            frac -= (float)d;
        }
    }
    if (conv == 'e') {
        out[n++] = 'e';
        out[n++] = exp10 < 0 ? '-' : '+';
        if (exp10 < 0)
            exp10 = -exp10;
        out[n++] = (char)('0' + exp10 / 10);
        out[n++] = (char)('0' + exp10 % 10);
    }
    return n;
}

int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    out_t o = {buf, cap, 0};
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            emit(&o, *fmt);
            continue;
        }
        fmt++;
        int left = 0, zero = 0, plus = 0, space = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else break;
        }
        int width = 0, prec = -1;
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
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
        int lng = 0;
        while (*fmt == 'l') {
            lng++;
            fmt++;
        }
        if (*fmt == 'h')
            fmt++;
        char tmp[40];
        const char *s = tmp;
        int len = 0, neg = 0;
        char conv = *fmt;
        switch (conv) {
        case 'd': case 'i': {
            int64_t v = lng >= 2 ? va_arg(ap, int64_t) : lng ? va_arg(ap, long) : va_arg(ap, int);
            neg = v < 0;
            uint64_t u = neg ? (uint64_t)-v : (uint64_t)v;
            len = lng >= 2 ? fmt_u64(tmp, u, 10, 0) : fmt_u32(tmp, (uint32_t)u, 10, 0);
            goto number;
        }
        case 'u': case 'x': case 'X': case 'o': case 'p': {
            unsigned base = conv == 'u' ? 10 : conv == 'o' ? 8 : 16;
            if (conv == 'p') {
                lng = 1;
                emit(&o, '0');
                emit(&o, 'x');
            }
            if (lng >= 2)
                len = fmt_u64(tmp, va_arg(ap, uint64_t), base, conv == 'X');
            else
                len = fmt_u32(tmp, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), base, conv == 'X');
        number: {
            int digits = len;
            int sign = neg || plus || space;
            int zeros = prec > digits ? prec - digits : 0;
            int pad = width - digits - zeros - sign;
            if (zero && prec < 0 && !left) {
                zeros += pad > 0 ? pad : 0;
                pad = 0;
            }
            if (!left)
                emit_pad(&o, ' ', pad);
            if (sign)
                emit(&o, neg ? '-' : plus ? '+' : ' ');
            emit_pad(&o, '0', zeros);
            while (len)
                emit(&o, tmp[--len]);
            if (left)
                emit_pad(&o, ' ', pad);
            continue;
        }
        }
        case 'f': case 'e': case 'g': case 'F': case 'E': case 'G':
            len = fmt_float(tmp, (float)va_arg(ap, double), prec < 0 ? 6 : prec, (char)(conv | 32));
            break;
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
        if (!left)
            emit_pad(&o, ' ', width - len);
        for (int i = 0; i < len; i++)
            emit(&o, s[i]);
        if (left)
            emit_pad(&o, ' ', width - len);
    }
    if (buf && cap)
        buf[o.len < cap ? o.len : cap - 1] = 0;
    return (int)o.len;
}

int vprintf(const char *fmt, va_list ap) { return vsnprintf(NULL, 0, fmt, ap); }

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
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
