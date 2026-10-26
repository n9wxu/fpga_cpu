#include "t.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct big { int16_t a; int32_t b; char s[9]; };
static struct big make(int16_t a, int32_t b, const char *s)
{
    struct big r;
    r.a = a;
    r.b = b;
    strncpy(r.s, s, 8);
    r.s[8] = 0;
    return r;
}
static int32_t take(struct big x, int16_t k) { return x.a * k + x.b + (int32_t)strlen(x.s); }

static int32_t vsum(int16_t n, ...)
{
    va_list ap;
    va_start(ap, n);
    int32_t s = 0;
    for (int16_t i = 0; i < n; i++)
        s += (i & 1) ? va_arg(ap, long) : va_arg(ap, int);
    va_end(ap);
    return s;
}

static jmp_buf jb;
static int depth;
static void dive(int n)
{
    if (n == 0)
        longjmp(jb, 42);
    depth++;
    dive(n - 1);
}

static int16_t ack(int16_t m, int16_t n)
{
    if (m == 0) return (int16_t)(n + 1);
    if (n == 0) return ack((int16_t)(m - 1), 1);
    return ack((int16_t)(m - 1), ack(m, (int16_t)(n - 1)));
}

static int16_t sw(int16_t x)
{
    switch (x) {
    case 0: return 11; case 1: return 22; case 2: return 33; case 3: return 44;
    case 4: return 55; case 5: return 66; case 7: return 88; case 100: return 1;
    default: return -1;
    }
}

static int16_t op_add(int16_t a, int16_t b) { return (int16_t)(a + b); }
static int16_t op_mul(int16_t a, int16_t b) { return (int16_t)(a * b); }

static int cmp16(const void *a, const void *b) { return *(const int16_t *)a - *(const int16_t *)b; }

struct bits { unsigned a : 3; unsigned b : 5; unsigned c : 8; };

static int32_t vla(int16_t n)
{
    int16_t v[n];
    for (int16_t i = 0; i < n; i++)
        v[i] = (int16_t)(i * i);
    int32_t s = 0;
    for (int16_t i = 0; i < n; i++)
        s += v[i];
    return s;
}

int main(void)
{
    struct big b = make(-3, 100000, "hello world");
    PI("struct", take(b, 7));
    PI("varargs", vsum(5, 1, (long)200000, 3, (long)-4, 5));
    depth = 0;
    int r = setjmp(jb);
    if (r == 0)
        dive(10);
    PI("longjmp", r * 100 + depth);
    PI("ack", ack(2, 3));
    uint32_t h = 0;
    for (int16_t i = -2; i < 110; i++)
        h = mix(h, (uint16_t)sw(i));
    PH("switch", h);
    int16_t (*ops[2])(int16_t, int16_t) = {op_add, op_mul};
    PI("fnptr", ops[0](3, 4) * 100 + ops[1](3, 4));
    int16_t arr[20];
    for (int i = 0; i < 20; i++)
        arr[i] = (int16_t)(rnd() >> 20) - 2000;
    qsort(arr, 20, sizeof arr[0], cmp16);
    h = 0;
    for (int i = 0; i < 20; i++)
        h = mix(h, (uint16_t)arr[i]);
    PH("qsort", h);
    struct bits bf = {5, 17, 200};
    bf.b += 3;
    PI("bitfield", (int32_t)bf.a * 10000 + bf.b * 100 + bf.c % 100);
    PI("vla", vla(30));
    char buf[64];
    snprintf(buf, sizeof buf, "[%5d|%-5d|%05d|%x|%s|%c|%ld]", 42, -7, 123, 0xABCu, "str", 'Z', -1234567L);
    printf("sprintf      %s\n", buf);
    memset(buf, 'x', 10);
    buf[10] = 0;
    memmove(buf + 2, buf, 5);
    memcpy(buf, "AB", 2);
    printf("mem          %s %d %d\n", buf, strcmp("abc", "abd") < 0, (int)strlen(buf));
    char *p = strchr("find:me", ':');
    printf("strchr       %s %s\n", p, strstr("haystack", "st"));
    PI("strtol", strtol("  -0x1F", NULL, 0) + atoi("77"));
    int32_t big[300];
    for (int i = 0; i < 300; i++)
        big[i] = (int32_t)i * 1000;
    int32_t s = 0;
    for (int i = 299; i >= 0; i -= 7)
        s += big[i];
    PI("bigframe", s);
    return 0;
}
