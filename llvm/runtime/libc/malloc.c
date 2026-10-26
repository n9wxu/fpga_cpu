/* fpga_cpu C runtime: a first-fit allocator between the end of BSS and
 * 16 KB below the stack top (docs/io.md memory layout). */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern char _end[];
#define HEAP_TOP ((char *)0x07C000)

typedef struct blk {
    size_t size; /* bytes including this header; low bit = in use */
    struct blk *next_free;
} blk_t;

static blk_t *free_list;
static char *brk_ptr;

static size_t round_up(size_t n) { return (n + sizeof(blk_t) + 3) & ~(size_t)3; }

void *malloc(size_t n)
{
    size_t need = round_up(n);
    blk_t **pp = &free_list;
    for (blk_t *b = free_list; b; pp = &b->next_free, b = b->next_free) {
        if (b->size >= need) {
            if (b->size >= need + 16) { /* split */
                blk_t *rest = (blk_t *)((char *)b + need);
                rest->size = b->size - need;
                rest->next_free = b->next_free;
                *pp = rest;
                b->size = need;
            } else {
                *pp = b->next_free;
            }
            b->size |= 1;
            return b + 1;
        }
    }
    if (!brk_ptr)
        brk_ptr = (char *)(((uintptr_t)_end + 3) & ~(uintptr_t)3);
    if (brk_ptr + need > HEAP_TOP)
        return NULL;
    blk_t *b = (blk_t *)brk_ptr;
    brk_ptr += need;
    b->size = need | 1;
    return b + 1;
}

void free(void *p)
{
    if (!p)
        return;
    blk_t *b = (blk_t *)p - 1;
    b->size &= ~(size_t)1;
    b->next_free = free_list;
    free_list = b;
}

void *calloc(size_t n, size_t size)
{
    void *p = malloc(n * size);
    if (p)
        memset(p, 0, n * size);
    return p;
}

void *realloc(void *p, size_t n)
{
    if (!p)
        return malloc(n);
    blk_t *b = (blk_t *)p - 1;
    size_t have = (b->size & ~(size_t)1) - sizeof(blk_t);
    if (have >= n)
        return p;
    void *q = malloc(n);
    if (q) {
        memcpy(q, p, have);
        free(p);
    }
    return q;
}
