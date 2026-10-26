/* fpga_cpu C runtime: <stdlib.h> basics. */
#include <ctype.h>
#include <stdlib.h>

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }

unsigned long strtoul(const char *s, char **end, int base)
{
    while (isspace(*s))
        s++;
    if (*s == '+')
        s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 32) == 'x') {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = s[0] == '0' ? 8 : 10;
    }
    unsigned long v = 0;
    for (;; s++) {
        int d;
        if (isdigit(*s))
            d = *s - '0';
        else if (isalpha(*s))
            d = (*s | 32) - 'a' + 10;
        else
            break;
        if (d >= base)
            break;
        v = v * (unsigned)base + (unsigned)d;
    }
    if (end)
        *end = (char *)s;
    return v;
}

long strtol(const char *s, char **end, int base)
{
    while (isspace(*s))
        s++;
    int neg = *s == '-';
    if (neg)
        s++;
    unsigned long v = strtoul(s, end, base);
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
