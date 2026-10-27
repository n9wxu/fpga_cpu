/* fpga_cpu C runtime: <stdio.h>.
 * The console (CON_OUT / CON_IN) is stdout, stderr and stdin; files go
 * through the file service (docs/io.md, F00500): flat 8.3 names, so any
 * directory part of a path is dropped. */
#ifndef _STDIO_H
#define _STDIO_H
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)
#define BUFSIZ 512
#define FILENAME_MAX 64
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct _fcpu_file FILE;
extern FILE *stdin, *stdout, *stderr;

int putchar(int c);
int puts(const char *s);
int getchar(void);
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);

FILE *fopen(const char *name, const char *mode);
int fclose(FILE *f);
size_t fread(void *p, size_t size, size_t n, FILE *f);
size_t fwrite(const void *p, size_t size, size_t n, FILE *f);
int fseek(FILE *f, long off, int whence);
long ftell(FILE *f);
void rewind(FILE *f);
int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);
int fflush(FILE *f);
int fgetc(FILE *f);
int getc(FILE *f);
int ungetc(int c, FILE *f);
char *fgets(char *s, int n, FILE *f);
int fputc(int c, FILE *f);
int putc(int c, FILE *f);
int fputs(const char *s, FILE *f);
int fprintf(FILE *f, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int remove(const char *name);
int rename(const char *from, const char *to);

int sscanf(const char *s, const char *fmt, ...);
int vsscanf(const char *s, const char *fmt, va_list ap);
int fscanf(FILE *f, const char *fmt, ...);
int vfscanf(FILE *f, const char *fmt, va_list ap);

#endif
