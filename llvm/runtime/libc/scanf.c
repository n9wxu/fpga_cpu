/* fpga_cpu C runtime: sscanf and fscanf.
 * Conversions: d i u o x X c s [set] n %, with width and the h, l, ll
 * length modifiers, and e f g (into float, or double with l: both are
 * IEEE single on this target). */
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *s; /* string source, or NULL */
    FILE *f;
    int c;         /* lookahead, or EOF */
    int count;     /* characters consumed */
} src_t;

static int peek(src_t *in) { return in->c; }

static void next(src_t *in)
{
    if (in->c == EOF)
        return;
    in->count++;
    if (in->s) {
        in->s++;
        in->c = *in->s ? (unsigned char)*in->s : EOF;
    } else
        in->c = fgetc(in->f);
}

static void store_int(va_list *ap, int len, unsigned long long v)
{
    switch (len) {
    case 'H': *va_arg(*ap, signed char *) = (signed char)v; break;
    case 'h': *va_arg(*ap, short *) = (short)v; break;
    case 'l': *va_arg(*ap, long *) = (long)v; break;
    case 'L': *va_arg(*ap, long long *) = (long long)v; break;
    default: *va_arg(*ap, int *) = (int)v; break;
    }
}

static int scan(src_t *in, const char *fmt, va_list *ap)
{
    int n = 0;
    while (*fmt) {
        if (isspace((unsigned char)*fmt)) {
            while (isspace(peek(in)))
                next(in);
            fmt++;
            continue;
        }
        if (*fmt != '%' || fmt[1] == '%') {
            if (*fmt == '%')
                fmt++;
            if (peek(in) != (unsigned char)*fmt)
                break;
            next(in);
            fmt++;
            continue;
        }
        fmt++;
        int skip = 0, width = 0, len = 0;
        if (*fmt == '*') {
            skip = 1;
            fmt++;
        }
        while (isdigit((unsigned char)*fmt))
            width = width * 10 + (*fmt++ - '0');
        if (*fmt == 'h') {
            len = 'h';
            if (*++fmt == 'h') {
                len = 'H';
                fmt++;
            }
        } else if (*fmt == 'l') {
            len = 'l';
            if (*++fmt == 'l') {
                len = 'L';
                fmt++;
            }
        }
        char conv = *fmt++;
        if (!width)
            width = conv == 'c' ? 1 : 0x7FFF;
        if (conv == 'n') {
            if (!skip)
                store_int(ap, len, (unsigned long long)in->count);
            continue;
        }
        if (conv != 'c' && conv != '[')
            while (isspace(peek(in)))
                next(in);
        if (peek(in) == EOF)
            return n ? n : EOF;
        if (conv == 'c' || conv == 's' || conv == '[') {
            char set[256] = { 0 }, neg = 0;
            if (conv == '[') {
                if (*fmt == '^') {
                    neg = 1;
                    fmt++;
                }
                if (*fmt == ']')
                    set[(unsigned char)*fmt++] = 1;
                while (*fmt && *fmt != ']') {
                    if (fmt[1] == '-' && fmt[2] && fmt[2] != ']') {
                        for (int c = (unsigned char)fmt[0]; c <= (unsigned char)fmt[2]; c++)
                            set[c] = 1;
                        fmt += 3;
                    } else
                        set[(unsigned char)*fmt++] = 1;
                }
                if (*fmt == ']')
                    fmt++;
            }
            char *out = skip ? NULL : va_arg(*ap, char *);
            int got = 0;
            while (got < width && peek(in) != EOF) {
                int c = peek(in);
                if (conv == 's' && isspace(c))
                    break;
                if (conv == '[' && (set[c] != 0) == (neg != 0))
                    break;
                if (out)
                    out[got] = (char)c;
                got++;
                next(in);
            }
            if (!got)
                break;
            if (out && conv != 'c')
                out[got] = 0;
            if (!skip)
                n++;
            continue;
        }
        if (conv == 'e' || conv == 'f' || conv == 'g' || conv == 'E' || conv == 'G') {
            char buf[48];
            int k = 0;
            while (k < width && k < 47) {
                int c = peek(in);
                if (!(isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'))
                    break;
                buf[k++] = (char)c;
                next(in);
            }
            buf[k] = 0;
            char *end;
            float v = strtof(buf, &end);
            if (end == buf)
                break;
            if (!skip) {
                if (len == 'l')
                    *va_arg(*ap, double *) = v;
                else
                    *va_arg(*ap, float *) = v;
                n++;
            }
            continue;
        }
        int base = conv == 'o' ? 8 : (conv == 'x' || conv == 'X') ? 16 : conv == 'i' ? 0 : 10;
        int neg = 0, digits = 0;
        unsigned long long v = 0;
        if ((peek(in) == '-' || peek(in) == '+') && width > 0) {
            neg = peek(in) == '-';
            next(in);
            width--;
        }
        if ((base == 0 || base == 16) && peek(in) == '0' && width > 0) {
            next(in);
            width--;
            digits = 1;
            if ((peek(in) | 32) == 'x' && width > 0) {
                next(in);
                width--;
                base = 16;
                digits = 0;
            } else if (base == 0)
                base = 8;
        }
        if (base == 0)
            base = 10;
        while (width > 0) {
            int c = peek(in), d;
            if (isdigit(c))
                d = c - '0';
            else if (isalpha(c))
                d = (c | 32) - 'a' + 10;
            else
                break;
            if (d >= base)
                break;
            v = v * (unsigned)base + (unsigned)d;
            digits++;
            width--;
            next(in);
        }
        if (!digits)
            break;
        if (!skip) {
            store_int(ap, len, neg ? (unsigned long long)-(long long)v : v);
            n++;
        }
    }
    return n;
}

int vsscanf(const char *s, const char *fmt, va_list ap)
{
    src_t in = { s, NULL, *s ? (unsigned char)*s : EOF, 0 };
    va_list cp;
    va_copy(cp, ap);
    int n = scan(&in, fmt, &cp);
    va_end(cp);
    return n;
}

int sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsscanf(s, fmt, ap);
    va_end(ap);
    return n;
}

int vfscanf(FILE *f, const char *fmt, va_list ap)
{
    src_t in = { NULL, f, fgetc(f), 0 };
    va_list cp;
    va_copy(cp, ap);
    int n = scan(&in, fmt, &cp);
    va_end(cp);
    if (in.c != EOF)
        ungetc(in.c, f);
    return n;
}

int fscanf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfscanf(f, fmt, ap);
    va_end(ap);
    return n;
}
