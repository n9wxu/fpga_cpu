#include "t.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/* printf conformance, compared exactly with the host. double is a float on
 * fpga_cpu, so every value passed to a %f/%e/%g is a float the host can
 * represent too; the digits must be those of the exact binary value
 * (round half to even, any precision, any magnitude). */
static const float vals[] = {
    0.0f, -0.0f, 0.5f, 1.5f, 2.5f, 0.25f, 0.125f, 0.0005f, 0.375f, 1.0f, 100.0f, 123456.0f, 1234567.0f,
    0.0001f, 0.00001234f, 99999.5f, 999999.5f, 9.9999995f, 1e-5f, 3.14159274f, 2.71828175f, 1e7f, 1e15f,
    123456789.0f, 4294967295.0f, 4294967296.0f, 1e10f, 1e20f, 1e30f, 3.4028235e38f, -3.4028235e38f,
    1.17549435e-38f, 1e-40f, 1.40129846e-45f, 0.1f, 0.3f, 2.0f / 3.0f, -1.5f, -0.0625f, 65536.0f, 16777216.0f,
    8388607.5f, 0.9999999f, 5e-7f, 1.5e-7f, 999.9996f, 0.000999999f, 95.0f, 0.05f
};

static const char *const ffmts[] = {
    "%f", "%.0f", "%.1f", "%.2f", "%.3f", "%.7f", "%.10f", "%.20f", "%.40f", "%#.0f",
    "%e", "%.0e", "%.1e", "%.3e", "%.8e", "%.15e", "%#.0e", "%E",
    "%g", "%.0g", "%.1g", "%.3g", "%.10g", "%.20g", "%#g", "%#.3g", "%#.0g", "%G",
    "%12.3f|", "%-12.2e|", "%012.4f", "%+f", "% e", "%+012.3g", "%-+10.1f|", "%15g|", "%#12.4g|"
};

int main(void)
{
    unsigned i, j;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++)
        for (j = 0; j < sizeof ffmts / sizeof ffmts[0]; j++) {
            printf("%s: ", ffmts[j]);
            printf(ffmts[j], (double)vals[i]);
            printf("\n");
        }

    /* infinities and NaN */
    static const char *const sp[] = {"%f", "%e", "%g", "%F", "%G", "%8f|", "%-8f|", "%08f", "%.2f"};
    for (j = 0; j < sizeof sp / sizeof sp[0]; j++) {
        printf("%s: ", sp[j]); printf(sp[j], (double)INFINITY);
        printf(" "); printf(sp[j], -(double)INFINITY);
        printf(" "); printf(sp[j], (double)NAN);
        printf("\n");
    }
    printf("%+f % f %+f % f\n", (double)INFINITY, (double)INFINITY, -(double)INFINITY, -(double)INFINITY);

    /* integers: # flag, length modifiers, precision */
    printf("%#x %#X %#o %#x %#o %#.3o %#5x|%-#6x|%#06x\n", 255, 255, 8, 0, 0, 8, 10, 10, 10);
    printf("%hhd %hhu %hd %hu %hhx %hx %hd\n", 300, 300, -5, -1, 0x1ff, -2, 32767);
    printf("%zu %zd %zx %td %jd %ju %jx\n", (size_t)123456, (ptrdiff_t)-5, (size_t)255, (ptrdiff_t)-77,
           (intmax_t)-4000000000LL, (uintmax_t)4000000000ULL, (uintmax_t)0xFEDCBA987ULL);
    printf("%lld %llu %llx %lo %ld %lu\n", -123456789012LL, 123456789012ULL, 0xABCDEF012345ULL, 8UL, -2000000L, 3000000UL);
    printf("[%.0d] [%.0d] [%5.3d] [%-5d] [%+d] [% d] [%05d] [%5.0d] [%.0x] [%#.0o]\n", 0, 5, 7, 42, 42, 42, -42, 0, 0, 0);
    printf("[%*d] [%-*d] [%*d] [%.*d] [%.*f] [%*.*f]\n", 6, 12, 6, 12, -6, 12, 4, 7, 2, 1.5, 9, 3, 3.14159274);
    printf("[%.*f] [%5s] [%-5s] [%.2s] [%c] [%3c] [%-3c] [%%]\n", -1, 2.5, "ab", "ab", "abcdef", 'x', 'y', 'z');
    printf("[%lu] [%lx] [%lX] [%o] [%i]\n", 4000000000ul, 0xdeadbeeful, 0xdeadbeeful, 0177777u, -32768);

    char b[32];
    int n = snprintf(b, 8, "%f", 3.14159274);
    printf("%d [%s]\n", n, b);
    n = snprintf(b, sizeof b, "%.20f", 0.1f);
    printf("%d [%s]\n", n, b);
    n = snprintf(NULL, 0, "%e", 1e30f);
    printf("%d\n", n);
    return 0;
}
