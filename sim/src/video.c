/* fpga_cpu simulator: video renderer (docs/io.md, "Video").
 *
 * Composes the four layers at the logical 640x360 resolution, then doubles
 * every pixel to the 1280x720 output.  This is what the FPGA scan-out does,
 * one line at a time. */
#include "cpu.h"

#include <string.h>

#define LW 640
#define LH 360
#define TEXT_ROWS 25
#define TEXT_COLS 80
#define CELL_H 14
#define SPR_BUF_BYTES 115200u  /* 4 bpp; 8 bpp buffers are twice this */

extern const uint8_t font8x14[256][14];

static const uint32_t rgb8[8] = {
    0x000000, 0x0000FF, 0x00FF00, 0x00FFFF, 0xFF0000, 0xFF00FF, 0xFFFF00, 0xFFFFFF,
};

static uint8_t rd8(const cpu_t *c, uint32_t a)
{
    a &= 0xFFFFFF;
    return a < SRAM_SIZE ? c->sram[a] : 0;
}

static uint32_t reg32(const iodev_t *d, int lo) { return d->vid[lo] | (uint32_t)d->vid[lo + 1] << 16; }

/* 4-bit pixel at (x, y) of a 320-byte-stride buffer; -1 if transparent */
static int nib(const cpu_t *c, uint32_t base, int x, int y)
{
    uint8_t b = rd8(c, base + (uint32_t)y * 320 + (uint32_t)x / 2);
    int p = (x & 1) ? b & 15 : b >> 4;
    return (p & 8) ? -1 : p;
}

/* Hi-color palette entry to RGB: 2x2 ordered dither to 3 bits, or the true
 * color (RGB565 expanded to 8 bits per channel). */
static uint32_t hicolor(uint16_t c565, int x, int y, int true_color)
{
    unsigned r = c565 >> 11, g = (c565 >> 5) & 63, b = c565 & 31;
    if (true_color)
        return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
    static const unsigned bayer[2][2] = { { 0, 2 }, { 3, 1 } };
    unsigned t = 2 * bayer[y & 1][x & 1] + 1; /* threshold (t/8) of full scale */
    unsigned on = (8 * r > t * 31) << 2 | (8 * g > t * 63) << 1 | (8 * b > t * 31);
    return rgb8[on];
}

/* the average of two colors, channel by channel */
static uint32_t blend50(uint32_t a, uint32_t b)
{
    uint32_t r = 0;
    for (int sh = 0; sh < 24; sh += 8)
        r |= ((((a >> sh) & 255) + ((b >> sh) & 255)) >> 1) << sh;
    return r;
}

void video_render(const cpu_t *c, uint32_t *px, int true_color, int cursor_on)
{
    const iodev_t *d = &c->dev;
    uint16_t ctrl = d->vid[V_CTRL];
    uint32_t text = reg32(d, V_TEXT_LO), bmp = reg32(d, V_BMP_LO), spr = reg32(d, V_SPR_LO);
    int spr8 = (ctrl & VC_SPR8) != 0;
    if (ctrl & VC_SPRBUF)
        spr += spr8 ? 2 * SPR_BUF_BYTES : SPR_BUF_BYTES;
    uint32_t backdrop = rgb8[d->vid[V_BACKDROP] & 7];
    int sx = d->vid[V_SCROLLX] % LW, sy = d->vid[V_SCROLLY] % LH;
    for (int y = 0; y < LH; y++) {
        uint32_t *o = px + (size_t)(2 * y) * SCREEN_W;
        for (int x = 0; x < LW; x++) {
            uint32_t col = backdrop;
            if (ctrl & VC_BMP) {
                int bx = (x + sx) % LW, by = (y + sy) % LH;
                if (ctrl & VC_HICOL) {
                    uint8_t i = rd8(c, bmp + (uint32_t)by * LW + (uint32_t)bx);
                    if (i)
                        col = hicolor(d->pal[i], x, y, true_color);
                } else {
                    int p = nib(c, bmp, bx, by);
                    if (p >= 0)
                        col = rgb8[p];
                }
            }
            if (ctrl & VC_SPR) {
                if (spr8) {
                    uint8_t i = rd8(c, spr + (uint32_t)y * LW + (uint32_t)x);
                    if (i)
                        col = hicolor(d->pal[i], x, y, true_color);
                } else {
                    int p = nib(c, spr, x, y);
                    if (p >= 0)
                        col = rgb8[p];
                }
            }
            if ((ctrl & VC_TEXT) && y < TEXT_ROWS * CELL_H) {
                int cell = (y / CELL_H) * TEXT_COLS + x / 8, row = y % CELL_H;
                uint32_t ca = text + (uint32_t)cell * 2;
                uint16_t v = rd8(c, ca) | rd8(c, ca + 1) << 8;
                int fg = (v >> 8) & 7, bg = (v >> 11) & 7;
                int on = (font8x14[v & 0xFF][row] >> (7 - (x & 7))) & 1;
                if ((ctrl & VC_CURSOR) && cursor_on && cell == d->vid[V_CURSOR] && row >= CELL_H - 2)
                    on = 1;
                if (on)
                    col = rgb8[fg];
                else if (!(v & 0x4000)) {
                    if (!(v & 0x8000))
                        col = rgb8[bg];
                    else if (true_color)       /* stipple: a 50% blend */
                        col = blend50(col, rgb8[bg]);
                    else if (!((x + y) & 1))   /* stipple: a checkerboard */
                        col = rgb8[bg];
                }
            }
            o[2 * x] = o[2 * x + 1] = col;
        }
        memcpy(o + SCREEN_W, o, SCREEN_W * sizeof *o);
    }
}

int write_ppm(const char *path, const uint32_t *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint8_t rgb[3] = { (uint8_t)(px[i] >> 16), (uint8_t)(px[i] >> 8), (uint8_t)px[i] };
        fwrite(rgb, 1, 3, f);
    }
    return fclose(f);
}
