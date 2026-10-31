/* fpga_cpu C runtime: <stdlib.h> basics. */
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

/* Shared by strtol and strtoul: the magnitude of the number, *neg set for
 * a minus sign, *ovf set when it does not fit in an unsigned long. */
static unsigned long conv(const char *s, char **end, int base, int *neg, int *ovf)
{
    const char *orig = s;
    unsigned long v = 0;
    int digits = 0;
    *neg = *ovf = 0;
    while (isspace(*s))
        s++;
    if (*s == '-' || *s == '+')
        *neg = *s++ == '-';
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 32) == 'x' && isxdigit(s[2])) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = s[0] == '0' ? 8 : 10;
    }
    unsigned long cutoff = ~0UL / (unsigned)base;
    unsigned cutlim = (unsigned)(~0UL % (unsigned)base);
    for (;; s++) {
        unsigned d;
        if (isdigit(*s))
            d = *s - '0';
        else if (isalpha(*s))
            d = (*s | 32) - 'a' + 10;
        else
            break;
        if (d >= (unsigned)base)
            break;
        if (v > cutoff || (v == cutoff && d > cutlim))
            *ovf = 1;
        v = v * (unsigned)base + d;
        digits++;
    }
    if (end)
        *end = (char *)(digits ? s : orig);
    return v;
}

unsigned long strtoul(const char *s, char **end, int base)
{
    int neg, ovf;
    unsigned long v = conv(s, end, base, &neg, &ovf);
    if (ovf) {
        errno = ERANGE;
        return ~0UL;
    }
    return neg ? -v : v;
}

long strtol(const char *s, char **end, int base)
{
    int neg, ovf;
    unsigned long v = conv(s, end, base, &neg, &ovf);
    if (ovf || v > (~0UL >> 1) + neg) {
        errno = ERANGE;
        return neg ? -(long)(~0UL >> 1) - 1 : (long)(~0UL >> 1);
    }
    return neg ? -(long)v : (long)v;
}

long atol(const char *s) { return strtol(s, NULL, 10); }
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }

static unsigned long seed = 1;
void srand(unsigned s) { seed = s; }
int rand(void)
{
    seed = seed * 1103515245UL + 12345UL;
    return (int)((seed >> 16) & RAND_MAX);
}

void abort(void) { exit(134); }

static void swap(char *a, char *b, size_t n)
{
    while (n--) {
        char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

/* Shell sort: small, no recursion, fine for the sizes used here. */
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *))
{
    char *b = base;
    for (size_t gap = n / 2; gap; gap /= 2)
        for (size_t i = gap; i < n; i++)
            for (size_t j = i; j >= gap && cmp(b + (j - gap) * size, b + j * size) > 0; j -= gap)
                swap(b + (j - gap) * size, b + j * size, size);
}
