#include "t.h"

/* Division by zero is undefined in C, but the runtime routines agree: the
 * unsigned quotient is all ones and the remainder is the dividend; signed
 * division works on the magnitudes (so the quotient is -1 for a dividend
 * >= 0 and +1 for a negative one). The host would trap, so it prints what
 * the target is documented to give. */
#ifdef __fcpu__
#define UQ(T, a, b) ((T)((a) / (b)))
#define UR(T, a, b) ((T)((a) % (b)))
#define SQ(T, a, b) ((T)((a) / (b)))
#define SR(T, a, b) ((T)((a) % (b)))
#else
#define UQ(T, a, b) ((T)((b) ? (a) / (b) : (T)-1))
#define UR(T, a, b) ((T)((b) ? (a) % (b) : (a)))
#define SQ(T, a, b) ((T)((b) ? (a) / (b) : (a) < 0 ? 1 : -1))
#define SR(T, a, b) ((T)((b) ? (a) % (b) : (a)))
#endif

static volatile int16_t z16;
static volatile int32_t z32;
static volatile int64_t z64;

static void p64(const char *n, uint64_t v)
{
    printf("%-8s %08lx%08lx\n", n, (unsigned long)(uint32_t)(v >> 32), (unsigned long)(uint32_t)v);
}

int main(void)
{
    static const int64_t a64[] = {0, 1, -1, 0x123456789ABCDEF0LL, -0x123456789ABCDEFLL, 0x7FFFFFFFLL, 0x100000000LL};
    for (unsigned i = 0; i < sizeof a64 / sizeof a64[0]; i++) {
        int64_t a = a64[i];
        p64("udiv64", UQ(uint64_t, (uint64_t)a, (uint64_t)z64));
        p64("urem64", UR(uint64_t, (uint64_t)a, (uint64_t)z64));
        p64("sdiv64", SQ(int64_t, a, z64));
        p64("srem64", SR(int64_t, a, z64));
    }
    static const int32_t a32[] = {0, 1, -1, 0x12345678, -1234567};
    for (unsigned i = 0; i < sizeof a32 / sizeof a32[0]; i++) {
        int32_t a = a32[i];
        PH("udiv32", UQ(uint32_t, (uint32_t)a, (uint32_t)z32));
        PH("urem32", UR(uint32_t, (uint32_t)a, (uint32_t)z32));
        PH("sdiv32", SQ(int32_t, a, z32));
        PH("srem32", SR(int32_t, a, z32));
    }
    static const int16_t a16[] = {0, 1, -1, 1234, -1234};
    for (unsigned i = 0; i < sizeof a16 / sizeof a16[0]; i++) {
        int16_t a = a16[i];
        PH("udiv16", UQ(uint16_t, (uint16_t)a, (uint16_t)z16));
        PH("urem16", UR(uint16_t, (uint16_t)a, (uint16_t)z16));
        PH("sdiv16", (uint16_t)SQ(int16_t, a, z16));
        PH("srem16", (uint16_t)SR(int16_t, a, z16));
    }
    return 0;
}
