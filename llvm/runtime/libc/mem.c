/* fpga_cpu C runtime: memory functions. Word loops when both pointers are
 * even; the CPU moves 16 bits per access. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    char *d = dst;
    const char *s = src;
    if ((((uintptr_t)d | (uintptr_t)s) & 1) == 0) {
        uint16_t *dw = (uint16_t *)d;
        const uint16_t *sw = (const uint16_t *)s;
        for (; n >= 2; n -= 2)
            *dw++ = *sw++;
        d = (char *)dw;
        s = (const char *)sw;
    }
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    char *d = dst;
    const char *s = src;
    if (d <= s || d >= s + n)
        return memcpy(dst, src, n);
    d += n;
    s += n;
    while (n--)
        *--d = *--s;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    unsigned char b = (unsigned char)c;
    if (((uintptr_t)d & 1) && n) {
        *d++ = b;
        n--;
    }
    uint16_t w = (uint16_t)(b | (b << 8));
    uint16_t *dw = (uint16_t *)d;
    for (; n >= 2; n -= 2)
        *dw++ = w;
    d = (unsigned char *)dw;
    if (n)
        *d = b;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y)
            return *x - *y;
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c)
            return (void *)p;
    return NULL;
}
