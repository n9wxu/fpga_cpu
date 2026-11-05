/* The SDRAM above 1 MB, for T1F with 24-bit addresses (AW24 in tang/rtl/system.v): every word of
 * 0x100000-0x7FFFFF with two patterns (one from the address, then its complement), a tag in each
 * 64 KB block (an address line stuck or tied shows as a block reading another's tag), byte, word
 * and pair accesses across 0x100000, code copied to 0x500000 and run there, and the 8 MB repeat
 * (0x900000 is 0x100000). Results go to the serial console; the exit code is the number of
 * failed checks. */
#include <stdint.h>
#include <fcpu.h>

#define LO 0x100000ul
#define HI 0x800000ul

static void con(const char *s) { while (*s) CON_OUT = (uint8_t)*s++; }
static void hex(uint32_t v, int digits)
{
    char b[9];
    for (int i = digits - 1; i >= 0; i--, v >>= 4) b[i] = "0123456789ABCDEF"[v & 15];
    b[digits] = 0;
    con(b);
}
static int fails;
static void report(const char *what, uint32_t errs, uint32_t first)
{
    con(what);
    if (errs) { con(": FAILED, errors "); hex(errs, 6); con(" first at "); hex(first, 6); fails++; }
    else con(": ok");
    con("\n");
}
static uint32_t pat(uint32_t a) { return (a * 2654435761ul) ^ (a >> 7) ^ 0xA5C30F96ul; }

__attribute__((noinline)) static int leaf(int a, int b) { return a * 3 + b - 7; }

int main(void)
{
    uint32_t a, errs, first;
    int pass;
    volatile uint32_t *w;

    for (pass = 0; pass < 2; pass++) {
        uint32_t inv = pass ? 0xFFFFFFFFul : 0;
        for (a = LO; a < HI; a += 4) *(volatile uint32_t *)a = pat(a) ^ inv;
        errs = first = 0;
        for (a = LO; a < HI; a += 4)
            if (*(volatile uint32_t *)a != (pat(a) ^ inv)) { if (!errs) first = a; errs++; }
        report(pass ? "7 MB words, complement" : "7 MB words, pattern", errs, first);
    }

    /* a tag per 64 KB block, and a word below 1 MB that must not change */
    *(volatile uint32_t *)0x0F0000ul = 0x12345678ul;
    for (a = LO; a < HI; a += 0x10000ul) *(volatile uint32_t *)a = a ^ 0xC0DE0000ul;
    errs = first = 0;
    for (a = LO; a < HI; a += 0x10000ul)
        if (*(volatile uint32_t *)a != (a ^ 0xC0DE0000ul)) { if (!errs) first = a; errs++; }
    if (*(volatile uint32_t *)0x0F0000ul != 0x12345678ul) { if (!errs) first = 0x0F0000ul; errs++; }
    report("64 KB block tags", errs, first);

    /* bytes, words and a pair across 0x100000 */
    errs = 0;
    *(volatile uint16_t *)0x0FFFFEul = 0xBEEF;
    *(volatile uint16_t *)0x100000ul = 0xCAFE;
    if (*(volatile uint32_t *)0x0FFFFEul != 0xCAFEBEEFul) errs++;
    *(volatile uint8_t *)0x0FFFFFul = 0x11;
    *(volatile uint8_t *)0x100000ul = 0x22;
    if (*(volatile uint16_t *)0x0FFFFEul != 0x11EF || *(volatile uint16_t *)0x100000ul != 0xCA22) errs++;
    w = (volatile uint32_t *)0x7FFFFCul;
    *w = 0x600DF00Dul;
    if (*w != 0x600DF00Dul || *(volatile uint8_t *)0x7FFFFFul != 0x60) errs++;
    report("bytes, words, pairs at 0x100000 and the top", errs, 0);

    /* code at 0x500000 */
    {
        const uint8_t *src = (const uint8_t *)(uintptr_t)leaf;
        volatile uint8_t *dst = (volatile uint8_t *)0x500000ul;
        int (*f)(int, int) = (int (*)(int, int))0x500000ul;
        int i, r;
        for (i = 0; i < 64; i++) dst[i] = src[i];
        r = f(100, 9);
        report("code run at 0x500000", r == leaf(100, 9) ? 0 : 1, (uint32_t)(uint16_t)r);
    }

    /* the SDRAM repeats every 8 MB: 0x900000 is 0x100000 */
    *(volatile uint32_t *)0x100000ul = 0x31415926ul;
    report("0x900000 repeats 0x100000", *(volatile uint32_t *)0x900000ul == 0x31415926ul ? 0 : 1, 0x900000ul);

    con(fails ? "XRAM FAILED\n" : "XRAM ok\n");
    return fails;
}
