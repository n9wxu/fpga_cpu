/* fpga_cpu C runtime: <stdlib.h> */
#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

void exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int abs(int x);
long labs(long x);
int atoi(const char *s);
long atol(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
double strtod(const char *s, char **end);
float strtof(const char *s, char **end);
double atof(const char *s);
char *getenv(const char *name);
int system(const char *cmd);
int rand(void);
void srand(unsigned seed);
void *malloc(size_t n);
void *calloc(size_t n, size_t size);
void *realloc(void *p, size_t n);
void free(void *p);
void qsort(void *base, size_t n, size_t size, int (*cmp)(const void *, const void *));

#endif
