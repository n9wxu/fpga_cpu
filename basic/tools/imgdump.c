/* imgdump: run BASIC's image decoders (src/image.c) on the host and write
 * the decoded RGBA as a PAM file, so tools/imgcheck.py can compare them
 * with a reference decoder.
 *
 *   imgdump in.jpg|in.png out.pam      (exit 1 with "?ERROR n" on failure)
 *
 * It supplies just what image.c needs from the interpreter: the arena
 * pointers, str_gc, error and the file HAL.
 */
#include <stdio.h>
#include <stdlib.h>

#include "../src/basic.h"

static uint8_t arena_mem[1 << 20];
uint8_t *arr_end = arena_mem, *str_bot = arena_mem + sizeof arena_mem;

void str_gc(void) {}

void error(int e)
{
    printf("?ERROR %d\n", e);
    exit(1);
}

static FILE *files[4];

int hal_fopen(const char *name, int mode)
{
    (void)mode;
    files[0] = fopen(name, "rb");
    return files[0] ? 0 : -1;
}

int hal_fread(int h, void *buf, int len) { return (int)fread(buf, 1, (size_t)len, files[h]); }

int hal_fclose(int h)
{
    fclose(files[h]);
    return 0;
}

static FILE *out;
static uint16_t ow;

static void start(img_sink_t *s, uint16_t w, uint16_t h, int photo)
{
    (void)s;
    (void)photo;
    ow = w;
    fprintf(out, "P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", w, h);
}

static int row(img_sink_t *s, uint16_t y, const uint8_t *rgba)
{
    (void)s;
    (void)y;
    fwrite(rgba, 4, ow, out);
    return 0;
}

int main(int argc, char **argv)
{
    img_sink_t sink = { start, row };
    if (argc != 3) {
        fprintf(stderr, "usage: imgdump in out.pam\n");
        return 2;
    }
    out = fopen(argv[2], "wb");
    if (!out)
        return 2;
    img_load(argv[1], &sink);
    fclose(out);
    return 0;
}
