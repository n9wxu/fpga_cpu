/* fpga_cpu C runtime: a first-fit allocator between the end of BSS and
 * 16 KB below the stack top. The stack top comes from fcc's layout
 * (__stack_top: 053400 by default, docs/io.md memory layout). */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern char _end[];
extern char __stack_top[];
#define HEAP_TOP (__stack_top - 0x4000)

typedef struct blk {
    size_t size; /* bytes including this header; low bit = in use */
    struct blk *next_free;
} blk_t;

static blk_t *free_list;
static char *brk_ptr;

void *malloc(size_t n)
{
    if (n > (size_t)-1 - sizeof(blk_t) - 3)
        return NULL;
    size_t need = (n + sizeof(blk_t) + 3) & ~(size_t)3;
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
    if (need > (size_t)(HEAP_TOP - brk_ptr))
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
    /* the free list is address ordered: insert, then merge with the
     * neighbours that touch this block */
    blk_t **pp = &free_list;
    while (*pp && *pp < b)
        pp = &(*pp)->next_free;
    blk_t *next = *pp;
    if (next && (char *)b + b->size == (char *)next) {
        b->size += next->size;
        next = next->next_free;
    }
    b->next_free = next;
    *pp = b;
    if (pp != &free_list) {
        blk_t *prev = (blk_t *)((char *)pp - offsetof(blk_t, next_free));
        if ((char *)prev + prev->size == (char *)b) {
            prev->size += b->size;
            prev->next_free = b->next_free;
        }
    }
}

void *calloc(size_t n, size_t size)
{
    if (size && n > (size_t)-1 / size)
        return NULL;
    if (size && n > (size_t)-1 / size)
        return NULL;
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
