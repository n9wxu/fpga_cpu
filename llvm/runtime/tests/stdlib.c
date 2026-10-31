#include "t.h"
#include <alloca.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* strtol/strtoul edge cases, div/ldiv, alloca. Values that depend on the
 * width of long are compared against the limits, not printed. */
static volatile int sink;

static void st(const char *s, int base)
{
    char *e = (char *)1;
    errno = 0;
    long v = strtol(s, &e, base);
    int big = v == (long)(~0UL >> 1), small = v == -(long)(~0UL >> 1) - 1;
    printf("strtol(%s,%d): %ld end=%d err=%d max=%d min=%d\n", s, base,
           (big || small) ? 0L : v, (int)(e - s), errno == ERANGE, big, small);
}

static void su(const char *s, int base)
{
    char *e = (char *)1;
    errno = 0;
    unsigned long v = strtoul(s, &e, base);
    int max = v == ~0UL;
    printf("strtoul(%s,%d): %lu end=%d err=%d max=%d\n", s, base, max ? 0UL : v, (int)(e - s), errno == ERANGE, max);
}

int main(void)
{
    st("123", 10); st("  -45xyz", 10); st("+7", 0); st("0x1f", 0); st("0x1f", 16); st("017", 0);
    st("zz", 36); st("0x", 0); st("0xg", 0); st("", 10); st("   ", 10); st("-", 10); st("abc", 10);
    st("99999999999999999999999", 10); st("-99999999999999999999999", 10);
    st("32767", 10); st("-32768", 10);
    st("0xFFFFFFFFFFFFFFFFFFFFFF", 16);
    su("123", 10); su("-1", 10); su("-0", 10); su("  42  ", 10); su("0777", 0); su("", 10); su("q", 10);
    su("99999999999999999999999", 10); su("-99999999999999999999999", 10);
    su("+5", 10);
    PI("atoi", atoi("  -321abc"));
    PI("atol", atol("2000000"));

    div_t d = div(-7, 2);
    PI("div-q", d.quot); PI("div-r", d.rem);
    d = div(17, -5);
    PI("div-q2", d.quot); PI("div-r2", d.rem);
    ldiv_t l = ldiv(-1000000L, 7);
    PI("ldiv-q", l.quot); PI("ldiv-r", l.rem);

    int n = 20 + sink;
    unsigned char *a = alloca(n);
    for (int i = 0; i < n; i++)
        a[i] = (unsigned char)(i * 3);
    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += a[i];
    PI("alloca", sum);
    return 0;
}
