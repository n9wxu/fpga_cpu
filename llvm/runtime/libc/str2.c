/* fpga_cpu C runtime: the rest of <string.h> and <strings.h>. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    return p ? memcpy(p, s, n) : NULL;
}

char *strndup(const char *s, size_t n)
{
    size_t k = 0;
    while (k < n && s[k])
        k++;
    char *p = malloc(k + 1);
    if (p) {
        memcpy(p, s, k);
        p[k] = 0;
    }
    return p;
}

int strcasecmp(const char *a, const char *b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d || !*a)
            return d;
    }
}

int strncasecmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d || !*a)
            return d;
    }
    return 0;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    while (s[n] && strchr(accept, s[n]))
        n++;
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n]))
        n++;
    return n;
}

char *strpbrk(const char *s, const char *accept)
{
    s += strcspn(s, accept);
    return *s ? (char *)s : NULL;
}

char *strtok(char *s, const char *delim)
{
    static char *last;
    if (!s)
        s = last;
    if (!s)
        return NULL;
    s += strspn(s, delim);
    if (!*s) {
        last = NULL;
        return NULL;
    }
    char *e = s + strcspn(s, delim);
    if (*e)
        *e++ = 0;
    else
        e = NULL;
    last = e;
    return s;
}

char *strerror(int e)
{
    switch (e) {
    case 0: return "no error";
    case 2: return "no such file";
    case 5: return "I/O error";
    case 12: return "out of memory";
    case 22: return "invalid argument";
    case 38: return "not supported";
    }
    return "error";
}
