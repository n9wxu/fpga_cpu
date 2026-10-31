#include "t.h"
#include <stdlib.h>
#include <string.h>

/* allocator: overflow handling, coalescing of freed neighbours, realloc */
static volatile size_t vm = (size_t)-1;
static void *volatile sink; /* keeps the compiler from folding malloc away */
#define ISNULL(e) (sink = (e), sink == NULL)
int main(void)
{
    /* the host's size_t is wider: use sizes that fail on both */
    PI("huge", ISNULL(malloc(vm - 15)));
    PI("huge2", ISNULL(malloc(vm)));
    PI("huge3", ISNULL(malloc(vm - 7)));
    PI("calloc-ovf", ISNULL(calloc(vm / 2 + 1, 4)));
    PI("calloc-ovf2", ISNULL(calloc(vm / 2 + 2, 2)));
    unsigned char *z = calloc(100, 3);
    int nz = 0;
    for (int i = 0; i < 300; i++)
        nz += z[i];
    PI("calloc-zero", nz);
    free(z);

    /* fragmentation: without coalescing the big block cannot be carved
     * out of the 20000-byte pieces */
    int fails = 0;
    for (int round = 0; round < 6; round++) {
        char *p[8];
        for (int i = 0; i < 8; i++) {
            p[i] = malloc(20000);
            if (!p[i]) { fails++; continue; }
            memset(p[i], i, 20000);
        }
        /* an order that needs merging with both neighbours */
        static const int order[8] = {1, 3, 2, 0, 5, 7, 4, 6};
        for (int i = 0; i < 8; i++)
            if (p[order[i]])
                free(p[order[i]]);
        char *big = malloc(100000);
        if (!big) fails++; else memset(big, 7, 100000);
        free(big);
    }
    PI("frag-fails", fails);

    /* realloc keeps contents when it grows and moves */
    char *r = malloc(10);
    memcpy(r, "0123456789", 10);
    char *keep = malloc(10);
    r = realloc(r, 5000);
    PI("realloc-cmp", memcmp(r, "0123456789", 10));
    r = realloc(r, 3);
    PI("realloc-shrink", memcmp(r, "012", 3));
    PI("realloc-null", realloc(NULL, 16) != NULL);
    PI("realloc-huge", ISNULL(realloc(r, vm - 15)));
    PI("realloc-still", memcmp(r, "012", 3));
    free(r);
    free(keep);
    return 0;
}
