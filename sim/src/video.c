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

int video_hicol8 = 1;

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

/* playfield A (the bitmap) at logical (x, y) with its scroll; 1 if a pixel is there */
static int pf_a(const cpu_t *c, uint16_t ctrl, uint32_t bmp, int bx, int by, int x, int y, int true_color, uint32_t *col)
{
    const iodev_t *d = &c->dev;
    if (ctrl & VC_HICOL) {
        uint8_t i = rd8(c, bmp + (uint32_t)by * LW + (uint32_t)bx);
        if (!i)
            return 0;
        *col = hicolor(d->pal[i], x, y, true_color);
        return 1;
    }
    if (ctrl & VC_C16) {
        uint8_t b = rd8(c, bmp + (uint32_t)by * 320 + (uint32_t)bx / 2);
        int i = (bx & 1) ? b & 15 : b >> 4;
        if (i == 8)
            return 0;
        if (i < 8)
            *col = rgb8[i];
        else                                    /* half brightness */
            *col = true_color ? (rgb8[i & 7] & 0x808080) : ((x + y) & 1) ? 0 : rgb8[i & 7];
        return 1;
    }
    int p = nib(c, bmp, bx, by);
    if (p < 0)
        return 0;
    *col = rgb8[p];
    return 1;
}

/* the hardware sprites' pixels on line y (0 = none): the 16 lowest-numbered
 * sprites on the line, a lower number in front */
static void sprite_line(const cpu_t *c, int y, uint8_t *line)
{
    const uint16_t *at = c->dev.spr_attr;
    int list[16], n = 0;
    memset(line, 0, LW);
    for (int s = 0; s < 64 && n < 16; s++) {
        const uint16_t *a = at + 4 * s;
        int top = (a[1] & 0x1FF) - 32, h = (a[1] >> 11) + 1;
        if ((a[3] & 0x8000) && y >= top && y < top + h)
            list[n++] = s;
    }
    for (int k = n - 1; k >= 0; k--) {
        const uint16_t *a = at + 4 * list[k];
        int w = 8 << ((a[3] >> 13) & 3), flip = (a[3] >> 12) & 1;
        if (w > 32)
            continue;                           /* width code 3 is not defined */
        int x0 = (a[0] & 0x3FF) - 32, row = y - ((a[1] & 0x1FF) - 32);
        uint32_t base = (a[2] | (uint32_t)(a[3] & 15) << 16) + (uint32_t)(row * w);
        for (int i = 0; i < w; i++) {
            int x = x0 + i;
            uint8_t p = rd8(c, base + (uint32_t)(flip ? w - 1 - i : i));
            if (p && x >= 0 && x < LW)
                line[x] = p;
        }
    }
}

void video_render(const cpu_t *c, uint32_t *px, int true_color, int cursor_on)
{
    const iodev_t *d = &c->dev;
    uint16_t ctrl = d->vid[V_CTRL];
    uint16_t ctrl2 = video_hicol8 ? d->vidx[VX_CTRL2] : 0;
    if (!video_hicol8)
        ctrl &= (uint16_t)~(VC_HICOL | VC_SPR8);
    uint32_t text = reg32(d, V_TEXT_LO), bmp = reg32(d, V_BMP_LO), spr = reg32(d, V_SPR_LO);
    uint32_t bmpb = d->vidx[VX_BMPB_LO] | (uint32_t)d->vidx[VX_BMPB_HI] << 16;
    uint32_t lscr = d->vidx[VX_LSCR_LO] | (uint32_t)d->vidx[VX_LSCR_HI] << 16;
    int spr8 = (ctrl & VC_SPR8) != 0;
    if (ctrl & VC_SPRBUF)
        spr += spr8 ? 2 * SPR_BUF_BYTES : SPR_BUF_BYTES;
    uint32_t backdrop = rgb8[d->vid[V_BACKDROP] & 7];
    int sy = d->vid[V_SCROLLY] % LH, syb = d->vidx[VX_SCROLLYB] % LH;
    uint8_t sline[LW];
    for (int y = 0; y < LH; y++) {
        uint32_t *o = px + (size_t)(2 * y) * SCREEN_W;
        int sx = d->vid[V_SCROLLX], sxb = d->vidx[VX_SCROLLXB];
        if (ctrl2 & VC2_LSCR) {                 /* line scroll: this line's two offsets */
            uint32_t e = lscr + (uint32_t)y * 4;
            sx += rd8(c, e) | rd8(c, e + 1) << 8;
            sxb += rd8(c, e + 2) | rd8(c, e + 3) << 8;
        }
        sx %= LW;
        sxb %= LW;
        int by = (y + sy) % LH, byb = (y + syb) % LH;
        if (ctrl2 & VC2_HWSPR)
            sprite_line(c, y, sline);
        for (int x = 0; x < LW; x++) {
            uint32_t col = backdrop, cb;
            int bx = (x + sx) % LW;
            int b_on = 0;
            if (ctrl2 & VC2_PFB) {
                uint8_t i = rd8(c, bmpb + (uint32_t)byb * LW + (uint32_t)((x + sxb) % LW));
                if (i) {
                    b_on = 1;
                    cb = hicolor(d->pal[i], x, y, true_color);
                }
            }
            if (b_on && !(ctrl2 & VC2_BFRONT))
                col = cb;
            if (ctrl & VC_BMP)
                pf_a(c, ctrl, bmp, bx, by, x, y, true_color, &col);
            if (b_on && (ctrl2 & VC2_BFRONT))
                col = cb;
            if ((ctrl2 & VC2_HWSPR) && (ctrl2 & VC2_SPRBEH) && sline[x])
                col = hicolor(d->pal[sline[x]], x, y, true_color);
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
            if ((ctrl2 & VC2_HWSPR) && !(ctrl2 & VC2_SPRBEH) && sline[x])
                col = hicolor(d->pal[sline[x]], x, y, true_color);
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
