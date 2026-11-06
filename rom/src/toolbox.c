/* The Toolbox's memory and utility managers (docs/rom.md "Toolbox"). The system heap is the
 * ROM's own RAM between its BSS and its stack (F80000-FFFFFF, the top 512 KB of the SDRAM). */
#include "rom.h"

/* ---- memory: first fit over a list of blocks, each with an 8-byte header, merged on free */
typedef struct blk { uint32_t size; struct blk *next; } blk_t;   /* size includes the header; bit 0 = used */
static blk_t *heap;
extern char _end[];                     /* the end of the ROM's BSS (fcc's layout) */
#define HEAP_END 0xFF0000UL             /* 64 KB of stack above it */

void mem_init(void)
{
    uintptr_t a = ((uintptr_t)_end + 7) & ~(uintptr_t)7;
    heap = (blk_t *)a;
    heap->size = (uint32_t)(HEAP_END - a);
    heap->next = 0;
}

void *mem_alloc(uint32_t n)
{
    if (!n) return 0;
    n = (n + sizeof(blk_t) + 7) & ~7UL;
    for (blk_t *b = heap; b; b = b->next) {
        if (b->size & 1 || b->size < n) continue;
        if (b->size - n >= 32) {        /* split */
            blk_t *r = (blk_t *)((char *)b + n);
            r->size = b->size - n;
            r->next = b->next;
            b->next = r;
            b->size = n;
        }
        b->size |= 1;
        return b + 1;
    }
    return 0;
}

void mem_free(void *p)
{
    if (!p) return;
    blk_t *b = (blk_t *)p - 1;
    b->size &= ~1UL;
    for (b = heap; b; b = b->next)      /* merge free neighbours */
        while (!(b->size & 1) && b->next && !(b->next->size & 1) &&
               (char *)b + b->size == (char *)b->next) {
            b->size += b->next->size;
            b->next = b->next->next;
        }
}

uint32_t mem_avail(void)
{
    uint32_t m = 0;
    for (blk_t *b = heap; b; b = b->next)
        if (!(b->size & 1) && b->size - sizeof(blk_t) > m) m = b->size - sizeof(blk_t);
    return m;
}

/* ---- utility */
void *ut_memcpy(void *d, const void *s, uint32_t n)
{
    uint8_t *p = d;
    const uint8_t *q = s;
    if (!(((uintptr_t)p | (uintptr_t)q | n) & 3)) {
        uint32_t *pw = d;
        const uint32_t *qw = s;
        for (n >>= 2; n; n--) *pw++ = *qw++;
        return d;
    }
    while (n--) *p++ = *q++;
    return d;
}

void *ut_memset(void *d, uint8_t v, uint32_t n)
{
    uint8_t *p = d;
    while (n--) *p++ = v;
    return d;
}

int16_t ut_memcmp(const void *a, const void *b, uint32_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (*p != *q) return *p < *q ? -1 : 1;
    return 0;
}

uint32_t ut_strlen(const char *s)
{
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

int16_t ut_strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return (uint8_t)*a < (uint8_t)*b ? -1 : (uint8_t)*a > (uint8_t)*b;
}

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }
int16_t ut_stricmp(const char *a, const char *b)
{
    while (*a && lower(*a) == lower(*b)) a++, b++;
    char x = lower(*a), y = lower(*b);
    return (uint8_t)x < (uint8_t)y ? -1 : (uint8_t)x > (uint8_t)y;
}

char *ut_strcpy(char *d, const char *s)
{
    char *r = d;
    while ((*d++ = *s++)) ;
    return r;
}

/* CRC-32 (IEEE, as zlib's crc32: start with 0), a 16-entry table: half a byte a step */
uint32_t ut_crc32(uint32_t crc, const void *p, uint32_t n)
{
    static const uint32_t t[16] = {
        0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL, 0x76DC4190UL, 0x6B6B51F4UL,
        0x4DB26158UL, 0x5005713CUL, 0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
        0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL };
    const uint8_t *b = p;
    crc = ~crc;
    while (n--) {
        crc ^= *b++;
        crc = (crc >> 4) ^ t[crc & 15];
        crc = (crc >> 4) ^ t[crc & 15];
    }
    return ~crc;
}

static uint32_t seed = 1;
void ut_srand(uint32_t s) { seed = s ? s : 1; }
uint16_t ut_rand(void)
{
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;   /* xorshift32 */
    return (uint16_t)(seed >> 8);
}
