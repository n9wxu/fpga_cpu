/* fpga_cpu C runtime: strtod and friends, getenv, system, assert. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

float strtof(const char *s, char **end)
{
    const char *p = s;
    float v = 0, scale = 1;
    int neg = 0, digits = 0;
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '-' || *p == '+')
        neg = *p++ == '-';
    while (isdigit((unsigned char)*p)) {
        v = v * 10 + (float)(*p++ - '0');
        digits++;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            scale /= 10;
            v += (float)(*p++ - '0') * scale;
            digits++;
        }
    }
    if (!digits) {
        if (end)
            *end = (char *)s;
        return 0;
    }
    if ((*p | 32) == 'e') {
        const char *q = p + 1;
        int eneg = 0, e = 0;
        if (*q == '-' || *q == '+')
            eneg = *q++ == '-';
        if (isdigit((unsigned char)*q)) {
            while (isdigit((unsigned char)*q))
                e = e * 10 + (*q++ - '0');
            while (e--)
                v = eneg ? v / 10 : v * 10;
            p = q;
        }
    }
    if (end)
        *end = (char *)p;
    return neg ? -v : v;
}

double strtod(const char *s, char **end) { return strtof(s, end); }
double atof(const char *s) { return strtof(s, NULL); }

char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

int system(const char *cmd) { return cmd ? -1 : 0; }

void __assert_fail(const char *expr, const char *file, int line)
{
    printf("assertion failed: %s, %s line %d\n", expr, file, line);
    abort();
}
