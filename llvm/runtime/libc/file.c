/* fpga_cpu C runtime: FILE streams.
 *
 * stdout and stderr go to the console, stdin comes from it. Files use the
 * file service (docs/io.md): reads and writes go straight between the
 * caller's buffer and the file (the service copies memory itself), so
 * there is no stream buffer; the position is tracked here and SEEK is
 * only sent when it changes. Names are flat 8.3: a directory part is
 * dropped ("./doom1.wad" opens DOOM1.WAD).
 */
#include <errno.h>
#include <fcpu.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS_CMD    IO16(0xF00500)
#define FS_HANDLE IO16(0xF00504)
#define FS_MODE   IO16(0xF00506)
#define FS_NAME   IO32(0xF00508)
#define FS_BUF    IO32(0xF0050C)
#define FS_LEN    IO16(0xF00510)
#define FS_RESULT (*(volatile int16_t *)0xF00512)
#define FS_SIZE   IO32(0xF00514)
#define FS_POS    IO32(0xF00518)
#define FS_STATUS IO16(0xF00502)

enum { C_OPEN = 1, C_CLOSE, C_READ, C_WRITE, C_DELETE, C_SEEK = 8, C_SIZE };

struct _fcpu_file {
    int16_t h;      /* file service handle; -1 console */
    uint8_t used, eof, err, unget_ok;
    uint8_t unget;
    uint32_t pos;   /* where the next read or write goes */
    uint32_t fpos;  /* where the service's own position is */
};

static FILE con_in = { -1, 1, 0, 0, 0, 0, 0, 0 };
static FILE con_out = { -1, 1, 0, 0, 0, 0, 0, 0 };
static FILE con_err = { -1, 1, 0, 0, 0, 0, 0, 0 };
FILE *stdin = &con_in, *stdout = &con_out, *stderr = &con_err;

#define NFILES 8
static FILE files[NFILES];

static int16_t fs(int cmd)
{
    FS_CMD = (uint16_t)cmd;
    while (FS_STATUS & 0x8000)
        ;
    return FS_RESULT;
}

static void set_name(const char *name, char *buf)
{
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    strncpy(buf, base, 15);
    buf[15] = 0;
    FS_NAME = (uint32_t)(uintptr_t)buf;
}

FILE *fopen(const char *name, const char *mode)
{
    char nb[16];
    int i, m;
    FILE *f = NULL;
    for (i = 0; i < NFILES; i++)
        if (!files[i].used) {
            f = &files[i];
            break;
        }
    if (!f) {
        errno = EMFILE;
        return NULL;
    }
    m = mode[0] == 'w' ? 1 : mode[0] == 'a' ? 2 : 0;
    set_name(name, nb);
    FS_MODE = (uint16_t)m;
    int16_t h = fs(C_OPEN);
    if (h < 0) {
        errno = h == -1 ? ENOENT : EIO;
        return NULL;
    }
    memset(f, 0, sizeof *f);
    f->used = 1;
    f->h = h;
    if (m == 2) { /* append: the position is the end */
        FS_HANDLE = (uint16_t)h;
        if (fs(C_SIZE) >= 0)
            f->pos = f->fpos = FS_SIZE;
    }
    return f;
}

int fclose(FILE *f)
{
    if (!f || f->h < 0)
        return 0;
    FS_HANDLE = (uint16_t)f->h;
    fs(C_CLOSE);
    f->used = 0;
    return 0;
}

static int sync_pos(FILE *f)
{
    if (f->pos == f->fpos)
        return 0;
    FS_HANDLE = (uint16_t)f->h;
    FS_POS = f->pos;
    if (fs(C_SEEK) < 0) {
        f->err = 1;
        return -1;
    }
    f->fpos = f->pos;
    return 0;
}

/* Move n bytes between memory and the file, in pieces the service takes. */
static size_t xfer(FILE *f, void *p, size_t n, int cmd)
{
    size_t done = 0;
    if (sync_pos(f))
        return 0;
    while (done < n) {
        size_t k = n - done > 0x4000 ? 0x4000 : n - done;
        FS_HANDLE = (uint16_t)f->h;
        FS_BUF = (uint32_t)(uintptr_t)((char *)p + done);
        FS_LEN = (uint16_t)k;
        int16_t r = fs(cmd);
        if (r < 0) {
            f->err = 1;
            break;
        }
        done += (size_t)r;
        f->pos += (uint32_t)r;
        f->fpos = f->pos;
        if ((size_t)r < k) {
            if (cmd == C_READ)
                f->eof = 1;
            else
                f->err = 1;
            break;
        }
    }
    return done;
}

size_t fread(void *p, size_t size, size_t n, FILE *f)
{
    size_t want = size * n, got = 0;
    if (!size || !n)
        return 0;
    if (f->h < 0) {
        char *q = p;
        while (got < want) {
            int c = getchar();
            if (c == EOF) {
                f->eof = 1;
                break;
            }
            q[got++] = (char)c;
        }
        return got / size;
    }
    if (f->unget_ok) {
        *(char *)p = (char)f->unget;
        f->unget_ok = 0;
        got = 1;
    }
    got += xfer(f, (char *)p + got, want - got, C_READ);
    return got / size;
}

size_t fwrite(const void *p, size_t size, size_t n, FILE *f)
{
    size_t want = size * n, i;
    if (!size || !n)
        return 0;
    if (f->h < 0) {
        for (i = 0; i < want; i++)
            putchar(((const char *)p)[i]);
        return n;
    }
    f->unget_ok = 0;
    return xfer(f, (void *)p, want, C_WRITE) / size;
}

int fseek(FILE *f, long off, int whence)
{
    long base = 0;
    if (f->h < 0)
        return -1;
    if (whence == SEEK_CUR)
        base = (long)f->pos - (f->unget_ok ? 1 : 0);
    else if (whence == SEEK_END) {
        FS_HANDLE = (uint16_t)f->h;
        if (fs(C_SIZE) < 0)
            return -1;
        base = (long)FS_SIZE;
    }
    if (base + off < 0) {
        errno = EINVAL;
        return -1;
    }
    f->pos = (uint32_t)(base + off);
    f->eof = 0;
    f->unget_ok = 0;
    return 0;
}

long ftell(FILE *f) { return f->h < 0 ? -1 : (long)f->pos - (f->unget_ok ? 1 : 0); }
void rewind(FILE *f) { fseek(f, 0, SEEK_SET); f->err = 0; }
int feof(FILE *f) { return f->eof; }
int ferror(FILE *f) { return f->err; }
void clearerr(FILE *f) { f->eof = f->err = 0; }
int fflush(FILE *f) { (void)f; return 0; }

int fgetc(FILE *f)
{
    unsigned char c;
    return fread(&c, 1, 1, f) == 1 ? c : EOF;
}

int getc(FILE *f) { return fgetc(f); }

int ungetc(int c, FILE *f)
{
    if (c == EOF || f->h < 0 || f->unget_ok)
        return EOF;
    f->unget = (uint8_t)c;
    f->unget_ok = 1;
    f->eof = 0;
    return c;
}

char *fgets(char *s, int n, FILE *f)
{
    int i = 0;
    while (i < n - 1) {
        int c = fgetc(f);
        if (c == EOF)
            break;
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    if (!i)
        return NULL;
    s[i] = 0;
    return s;
}

int fputc(int c, FILE *f)
{
    unsigned char b = (unsigned char)c;
    return fwrite(&b, 1, 1, f) == 1 ? b : EOF;
}

int putc(int c, FILE *f) { return fputc(c, f); }

int fputs(const char *s, FILE *f)
{
    size_t n = strlen(s);
    return fwrite(s, 1, n, f) == n ? 0 : EOF;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    char small[256], *buf = small;
    va_list ap2;
    int n;
    if (f->h < 0)
        return vprintf(fmt, ap);
    va_copy(ap2, ap);
    n = vsnprintf(small, sizeof small, fmt, ap);
    if (n >= (int)sizeof small) {
        buf = malloc((size_t)n + 1);
        if (!buf) {
            va_end(ap2);
            return -1;
        }
        vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    }
    va_end(ap2);
    fwrite(buf, 1, (size_t)n, f);
    if (buf != small)
        free(buf);
    return n;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vfprintf(f, fmt, ap);
    va_end(ap);
    return n;
}

int remove(const char *name)
{
    char nb[16];
    set_name(name, nb);
    if (fs(C_DELETE) < 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

/* The file service has no rename. */
int rename(const char *from, const char *to)
{
    (void)from;
    (void)to;
    errno = ENOSYS;
    return -1;
}
