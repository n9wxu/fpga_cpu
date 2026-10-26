/* fpga_cpu C runtime: <stdio.h>, console only (CON_OUT / CON_IN). */
#ifndef _STDIO_H
#define _STDIO_H
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

int putchar(int c);
int puts(const char *s);
int getchar(void);
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

#endif
