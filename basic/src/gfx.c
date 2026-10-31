/* Graphics, sprites and sound (docs/io.md).
 *
 * The bitmap is 640x360, either 8-color (4 bits per pixel, [T][R][G][B],
 * left pixel in the high nibble) or hi-color (one palette index per
 * byte). Sprites are drawn in software into the hidden sprite buffer,
 * which is shown on the next VSYNC.
 */
#include "basic.h"

uint16_t vid_ctrl;

/* The palette belongs to the program: the default one is a starting
 * point. The hardware palette is write-only, so a copy is kept here, to
 * read colors back (PAL), rotate ranges and map images to whatever
 * palette is current. */
static uint8_t pal_rgb[256][3];
static uint8_t *bmp;
static int gmode; /* 0 none, 1 8-color, 2 hi-color */
static uint8_t pen = 7;
static int16_t lastx, lasty;

void vid_ctrl_set(uint16_t bits, int on)
{
    if (on)
        vid_ctrl |= bits;
    else
        vid_ctrl &= (uint16_t)~bits;
    hal_poke16(VID_CTRL, vid_ctrl);
}

static void poke32(uint32_t reg, uint32_t v)
{
    hal_poke16(reg, (uint16_t)(v & 0xFFFFu));
    hal_poke16(reg + 2, (uint16_t)(v >> 16));
}

static void default_palette(void)
{
    static const uint8_t prim[8][3] = { { 0, 0, 0 }, { 0, 0, 255 }, { 0, 255, 0 }, { 0, 255, 255 },
                                        { 255, 0, 0 }, { 255, 0, 255 }, { 255, 255, 0 }, { 255, 255, 255 } };
    int i;
    for (i = 0; i < 256; i++) {
        int r, g, b;
        if (i < 8) {
            r = prim[i][0];
            g = prim[i][1];
            b = prim[i][2];
        } else if (i < 16) { /* 8 = black, 9-15 = half-bright primaries */
            r = prim[i - 8][0] / 2;
            g = prim[i - 8][1] / 2;
            b = prim[i - 8][2] / 2;
        } else if (i < 232) { /* 6x6x6 cube */
            int c = i - 16;
            r = (c / 36) * 51;
            g = (c / 6 % 6) * 51;
            b = (c % 6) * 51;
        } else { /* gray ramp */
            r = g = b = 8 + (i - 232) * 10;
        }
        gfx_palette(i, r, g, b);
    }
}

void gfx_init(void)
{
    bmp = hal_mem(BMP_ADDR);
    default_palette(); /* sprites use the palette in every mode */
    poke32(VID_BMP, BMP_ADDR);
    poke32(VID_SPR, SPR_ADDR);
    gfx_backdrop(1);
}

void gfx_backdrop(int c) { hal_poke16(VID_BACKDROP, (uint16_t)(c & 7)); }

void gfx_palette(int i, int r, int g, int b)
{
    if (i < 0 || i > 255 || r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
        error(E_FC);
    pal_rgb[i][0] = (uint8_t)r;
    pal_rgb[i][1] = (uint8_t)g;
    pal_rgb[i][2] = (uint8_t)b;
    hal_poke16(PAL_INDEX, (uint16_t)i);
    hal_poke16(PAL_DATA, (uint16_t)(((unsigned)(r >> 3) << 11) | ((unsigned)(g >> 2) << 5) | (unsigned)(b >> 3)));
}

void gfx_clear(int c)
{
    if (gmode == 2)
        memset(bmp, c & 0xFF, (size_t)SCR_W * SCR_H);
    else
        memset(bmp, (c & 15) * 0x11, (size_t)SCR_W * SCR_H / 2u);
}

void gfx_mode(int m)
{
    if (m == 0) {
        vid_ctrl_set(VC_BMP | VC_SPR, 0);
        gmode = 0;
        return;
    }
    gmode = m;
    if (m == 2) {
        default_palette();
        gfx_clear(0);
        pen = 7;
        vid_ctrl_set(VC_BMP | VC_HICOL, 1);
    } else {
        gfx_clear(8);
        pen = 7;
        vid_ctrl_set(VC_HICOL, 0);
        vid_ctrl_set(VC_BMP, 1);
    }
    lastx = lasty = 0;
}

void gfx_pen(int c)
{
    if (c < 0 || c > (gmode == 2 ? 255 : 15))
        error(E_FC);
    pen = (uint8_t)c;
}

void gfx_scroll(int16_t x, int16_t y)
{
    hal_poke16(VID_SCROLLX, (uint16_t)(((x % SCR_W) + SCR_W) % SCR_W));
    hal_poke16(VID_SCROLLY, (uint16_t)(((y % SCR_H) + SCR_H) % SCR_H));
}

/* ---- pixels ---- */

static void nib_set(uint8_t *row, int16_t x, uint8_t c)
{
    uint8_t *b = row + (x >> 1);
    if (x & 1)
        *b = (uint8_t)((*b & 0xF0) | c);
    else
        *b = (uint8_t)((*b & 0x0F) | (c << 4));
}

static void pset(int16_t x, int16_t y)
{
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H)
        return;
    if (gmode == 2)
        bmp[(uint32_t)y * SCR_W + (uint16_t)x] = pen;
    else
        nib_set(bmp + (uint32_t)y * (SCR_W / 2), x, pen);
}

void gfx_plot(int16_t x, int16_t y)
{
    pset(x, y);
    lastx = x;
    lasty = y;
}

int gfx_point(int16_t x, int16_t y)
{
    uint8_t b;
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H)
        return -1;
    if (gmode == 2)
        return bmp[(uint32_t)y * SCR_W + (uint16_t)x];
    b = bmp[(uint32_t)y * (SCR_W / 2) + (uint16_t)(x >> 1)];
    return (x & 1) ? (b & 15) : (b >> 4);
}

/* Fill x0..x1 of a 4-bit row with nibble c. */
static void nib_span(uint8_t *row, int16_t x0, int16_t x1, uint8_t c)
{
    if (x0 & 1) {
        nib_set(row, x0, c);
        x0++;
    }
    if (!(x1 & 1)) {
        nib_set(row, x1, c);
        x1--;
    }
    if (x0 <= x1)
        memset(row + (x0 >> 1), c * 0x11, (size_t)((x1 - x0 + 1) >> 1));
}

static void hline(int16_t x0, int16_t x1, int16_t y)
{
    if (x0 > x1) {
        int16_t t = x0;
        x0 = x1;
        x1 = t;
    }
    if (y < 0 || y >= SCR_H || x1 < 0 || x0 >= SCR_W)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 >= SCR_W)
        x1 = SCR_W - 1;
    if (gmode == 2)
        memset(bmp + (uint32_t)y * SCR_W + (uint16_t)x0, pen, (size_t)(x1 - x0 + 1));
    else
        nib_span(bmp + (uint32_t)y * (SCR_W / 2), x0, x1, pen);
}

/* Ctrl-C check for the long loops: cheap enough every few hundred steps. */
#define BRK_EVERY(n) (((n) & 255) == 0 && hal_break())

/* 32-bit arithmetic: a span can be 65535 wide, which does not fit an int16. */
void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int32_t dx = x1 > x0 ? (int32_t)x1 - x0 : (int32_t)x0 - x1;
    int32_t dy = y1 > y0 ? (int32_t)y0 - y1 : (int32_t)y1 - y0;
    int32_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy, x = x0, y = y0;
    uint16_t steps = 0;
    if (y0 == y1) {
        hline(x0, x1, y0);
    } else {
        for (;;) {
            int32_t e2;
            pset((int16_t)x, (int16_t)y);
            if (x == x1 && y == y1)
                break;
            if (BRK_EVERY(++steps))
                brk();
            e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y += sy;
            }
        }
    }
    lastx = x1;
    lasty = y1;
}

void gfx_line_to(int16_t x, int16_t y) { gfx_line(lastx, lasty, x, y); }

void gfx_box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int fill)
{
    int16_t y;
    if (y0 > y1) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    if (fill) {
        for (y = y0 < 0 ? 0 : y0; y <= y1 && y < SCR_H; y++)
            hline(x0, x1, y);
    } else {
        hline(x0, x1, y0);
        hline(x0, x1, y1);
        for (y = (int16_t)(y0 + 1); y < y1; y++) {
            if (BRK_EVERY(y))
                brk();
            pset(x0, y);
            pset(x1, y);
        }
    }
    lastx = x1;
    lasty = y1;
}

/* Out-of-range coordinates are clipped, not wrapped to 16 bits. */
static void pset32(int32_t x, int32_t y)
{
    if (x >= 0 && x < SCR_W && y >= 0 && y < SCR_H)
        pset((int16_t)x, (int16_t)y);
}

static void hline32(int32_t x0, int32_t x1, int32_t y)
{
    if (y < 0 || y >= SCR_H || x1 < 0 || x0 >= SCR_W)
        return;
    hline((int16_t)(x0 < 0 ? 0 : x0), (int16_t)(x1 >= SCR_W ? SCR_W - 1 : x1), (int16_t)y);
}

void gfx_circle(int16_t cx, int16_t cy, int16_t r, int fill)
{
    int32_t x = r, y = 0, err = 1 - (int32_t)r; /* 32-bit: 2*y overflows int16 */
    int32_t X = cx, Y = cy;
    if (r < 0)
        error(E_FC);
    while (x >= y) {
        if (BRK_EVERY(y))
            brk();
        if (fill) {
            hline32(X - x, X + x, Y + y);
            hline32(X - x, X + x, Y - y);
            hline32(X - y, X + y, Y + x);
            hline32(X - y, X + y, Y - x);
        } else {
            pset32(X + x, Y + y);
            pset32(X - x, Y + y);
            pset32(X + x, Y - y);
            pset32(X - x, Y - y);
            pset32(X + y, Y + x);
            pset32(X - y, Y + x);
            pset32(X + y, Y - x);
            pset32(X - y, Y - x);
        }
        y++;
        if (err < 0)
            err += 2 * y + 1;
        else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
    lastx = cx;
    lasty = cy;
}

/* ---- palette ---- */

int gfx_pal_get(int i, int c)
{
    if (i < 0 || i > 255 || c < 0 || c > 2)
        error(E_FC);
    return pal_rgb[i][c];
}

/* Colors in first..last move n entries up (down if n < 0), wrapping. */
void gfx_pal_rotate(int first, int last, int n)
{
    uint8_t t[256][3];
    int len, k;
    if (first < 0 || last > 255 || first >= last)
        error(E_FC);
    len = last - first + 1;
    n %= len;
    if (n < 0)
        n += len;
    memcpy(t, pal_rgb[first], (size_t)len * 3u);
    for (k = 0; k < len; k++) {
        const uint8_t *c = t[(k - n + len) % len];
        gfx_palette(first + k, c[0], c[1], c[2]);
    }
}

/* PALETTE LOAD "file", base: entries from base up */
void gfx_pal_load(const char *name, int base)
{
    uint8_t c[256][3];
    int n, i;
    if (base < 0 || base > 255)
        error(E_FC);
    n = img_palette(name, c, 256 - base);
    for (i = 0; i < n; i++)
        gfx_palette(base + i, c[i][0], c[i][1], c[i][2]);
}

/* ---- mapping image colors to palette entries ----
 * An indexed PNG keeps its own index numbers: the program is expected to
 * have loaded the matching palette (or to ask for it with PALETTE base,
 * which also shifts the indices up by base). True-color pixels take the
 * nearest entry of the current palette; JPEG pictures are dithered.
 * Index 0 is always transparent.
 */

typedef struct {
    uint8_t base, n, last;
    uint8_t rgb[256][3];
} ptab_t;

enum { CM_NEAR, CM_INDEX, CM_EXACT };

typedef struct {
    uint8_t mode, base;
    uint16_t lim;   /* CM_INDEX: file indices must be below this */
    ptab_t *pt;     /* CM_EXACT: colors given entries in order of appearance */
    uint8_t *c555;  /* CM_NEAR photo: nearest entry by RGB555, 0 = not known yet */
    uint32_t *tag;  /* CM_NEAR otherwise: recent colors, direct-mapped */
    uint8_t *val;
} cmap_t;

/* Nearest entry 1-255. Exact matches win; otherwise distance is measured
 * on 6-bit channels, which keeps the products 16-bit on the target. */
static uint8_t near_full(uint8_t r, uint8_t g, uint8_t b)
{
    int16_t i, best = 1, bd = 32767;
    for (i = 1; i < 256; i++) {
        const uint8_t *c = pal_rgb[i];
        int16_t dr, dg, db, d;
        if (c[0] == r && c[1] == g && c[2] == b)
            return (uint8_t)i;
        dr = (int16_t)((r >> 2) - (c[0] >> 2));
        dg = (int16_t)((g >> 2) - (c[1] >> 2));
        db = (int16_t)((b >> 2) - (c[2] >> 2));
        d = (int16_t)(dr * dr + dg * dg + db * db);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return (uint8_t)best;
}

static uint8_t near_rgb(cmap_t *m, uint8_t r, uint8_t g, uint8_t b)
{
    if (m->c555) {
        uint16_t k = (uint16_t)((r >> 3) << 10 | (g >> 3) << 5 | (b >> 3));
        if (!m->c555[k])
            m->c555[k] = near_full(r, g, b);
        return m->c555[k];
    } else {
        uint32_t key = (uint32_t)r << 16 | (uint32_t)g << 8 | b;
        uint8_t h = (uint8_t)(r * 7u + g * 13u + b * 31u);
        if (m->tag[h] != key) {
            m->tag[h] = key;
            m->val[h] = near_full(r, g, b);
        }
        return m->val[h];
    }
}

static uint8_t exact(ptab_t *t, const uint8_t *p)
{
    uint16_t i;
    if (t->n && !memcmp(t->rgb[t->last], p, 3))
        return (uint8_t)(t->base + t->last);
    for (i = 0; i < t->n; i++)
        if (!memcmp(t->rgb[i], p, 3)) {
            t->last = (uint8_t)i;
            return (uint8_t)(t->base + i);
        }
    if (t->base + t->n > 255)
        img_fail(E_TC);
    memcpy(t->rgb[t->n], p, 3);
    gfx_palette(t->base + t->n, p[0], p[1], p[2]);
    t->last = t->n;
    return (uint8_t)(t->base + t->n++);
}

static void cmap_init(cmap_t *m, const img_info_t *in, int palbase)
{
    uint16_t i;
    memset(m, 0, sizeof *m);
    if (palbase > 255)
        img_fail(E_FC);
    if (in->npal) {
        /* Files often carry a full 256-entry palette whatever they use, so
         * only the entries that fit are loaded, and only a pixel whose
         * index would land past 255 is an error. */
        m->mode = CM_INDEX;
        m->lim = 256;
        if (palbase < 0)
            return;
        m->base = (uint8_t)palbase;
        m->lim = (uint16_t)(256 - palbase);
        for (i = 0; i < in->npal && i < m->lim; i++)
            gfx_palette(palbase + i, in->pal[4 * i], in->pal[4 * i + 1], in->pal[4 * i + 2]);
        return;
    }
    if (palbase >= 0) {
        m->mode = CM_EXACT;
        m->pt = (ptab_t *)img_scratch(sizeof(ptab_t));
        m->pt->base = (uint8_t)palbase;
        return;
    }
    m->mode = CM_NEAR;
    if (in->photo)
        m->c555 = (uint8_t *)img_scratch(32768u);
    else {
        m->tag = (uint32_t *)img_scratch(256u * sizeof(uint32_t));
        m->val = (uint8_t *)img_scratch(256u);
        memset(m->tag, 0xFF, 256u * sizeof(uint32_t));
    }
}

/* Palette entry for pixel x of a row; 0 = transparent. */
uint8_t cmap_px(void *cm, const uint8_t *rgba, const uint8_t *idx, uint16_t x)
{
    cmap_t *m = (cmap_t *)cm;
    const uint8_t *p = rgba + 4u * x;
    if (p[3] < 128)
        return 0;
    if (m->mode == CM_INDEX) {
        if (idx[x] >= m->lim)
            img_fail(E_TC);
        return (uint8_t)(m->base + idx[x]);
    }
    if (m->mode == CM_EXACT)
        return exact(m->pt, p);
    return near_rgb(m, p[0], p[1], p[2]);
}

void *cmap_new(const img_info_t *in, int palbase)
{
    cmap_t *m = (cmap_t *)img_scratch(sizeof(cmap_t));
    cmap_init(m, in, palbase);
    return m;
}

/* ---- PICTURE: an image into the bitmap ---- */

typedef struct {
    img_sink_t s;
    int16_t x0, y0;
    uint16_t xa, xb; /* visible source columns */
    uint8_t dither;
    int16_t *err;    /* Floyd-Steinberg: two rows of (visible + 2) x RGB, x16 */
    cmap_t *cm;
    int palbase;
} pic_t;

static void pic_start(img_sink_t *s, const img_info_t *in)
{
    pic_t *p = (pic_t *)s;
    int32_t a = -(int32_t)p->x0, b = (int32_t)SCR_W - p->x0;
    if (a < 0)
        a = 0;
    if (b > (int32_t)in->w)
        b = in->w;
    if (b < a)
        b = a;
    p->xa = (uint16_t)a;
    p->xb = (uint16_t)b;
    p->dither = (uint8_t)(in->photo && p->palbase < 0);
    if (gmode == 2) {
        p->cm = (cmap_t *)cmap_new(in, p->palbase);
        if (p->dither)
            p->err = (int16_t *)img_scratch((uint32_t)(p->xb - p->xa + 2u) * 12u);
    }
}

static const uint8_t bayer4[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

static int pic_row(img_sink_t *s, uint16_t y, const uint8_t *rgba, const uint8_t *idx)
{
    pic_t *p = (pic_t *)s;
    int32_t ty = (int32_t)p->y0 + y;
    uint16_t i, k, n = (uint16_t)(p->xb - p->xa);
    int16_t tx = (int16_t)(p->x0 + (int16_t)p->xa);
    const uint8_t *px = rgba + 4u * p->xa;
    if (ty < 0)
        return 0;
    if (ty >= SCR_H || !n)
        return 1;
    if (gmode == 2) {
        uint8_t *o = bmp + (uint32_t)ty * SCR_W + (uint16_t)tx;
        int16_t *e0 = NULL, *e1 = NULL;
        if (p->err) {
            e0 = p->err + (y & 1 ? 0 : 3u * (n + 2u));
            e1 = p->err + (y & 1 ? 3u * (n + 2u) : 0);
            memset(e1, 0, 3u * (n + 2u) * sizeof(int16_t));
        }
        for (k = 0; k < n; k++, px += 4) {
            uint8_t v[3], c;
            if (px[3] < 128)
                continue;
            if (!e0) {
                c = cmap_px(p->cm, rgba, idx, (uint16_t)(p->xa + k));
                if (c)
                    o[k] = c;
                continue;
            }
            for (i = 0; i < 3; i++) {
                int16_t t = (int16_t)(px[i] + e0[3 * (k + 1) + i] / 16);
                v[i] = (uint8_t)(t < 0 ? 0 : t > 255 ? 255 : t);
            }
            c = near_rgb(p->cm, v[0], v[1], v[2]);
            for (i = 0; i < 3; i++) {
                int16_t d = (int16_t)(v[i] - pal_rgb[c][i]);
                e0[3 * (k + 2) + i] = (int16_t)(e0[3 * (k + 2) + i] + d * 7);
                e1[3 * k + i] = (int16_t)(e1[3 * k + i] + d * 3);
                e1[3 * (k + 1) + i] = (int16_t)(e1[3 * (k + 1) + i] + d * 5);
                e1[3 * (k + 2) + i] = (int16_t)(e1[3 * (k + 2) + i] + d);
            }
            o[k] = c;
        }
    } else {
        uint8_t *o = bmp + (uint32_t)ty * (SCR_W / 2);
        for (k = 0; k < n; k++, px += 4) {
            int16_t x = (int16_t)(tx + k);
            uint8_t t = 128, c;
            if (px[3] < 128)
                continue;
            if (p->dither)
                t = (uint8_t)(bayer4[ty & 3][x & 3] * 16 + 8);
            c = (uint8_t)((px[0] >= t) << 2 | (px[1] >= t) << 1 | (px[2] >= t));
            nib_set(o, x, c);
        }
    }
    return 0;
}

static void need_bitmap(void)
{
    if (!gmode)
        gfx_mode(2);
}

void gfx_picture(const char *name, int16_t x, int16_t y, int palbase)
{
    pic_t p;
    memset(&p, 0, sizeof p);
    need_bitmap();
    if (palbase >= 0 && gmode != 2)
        error(E_FC); /* the 8-color bitmap has no palette */
    p.s.start = pic_start;
    p.s.row = pic_row;
    p.x0 = x;
    p.y0 = y;
    p.palbase = palbase;
    img_load(name, &p.s);
}

/* STAMP: copy a frame of a cached sheet into the bitmap, its hot spot at
 * x, y. In the 8-color bitmap each entry becomes the nearest primary. */
void gfx_stamp(int s, int f, int16_t x, int16_t y, int flip)
{
    frame_t fr;
    int16_t i, j;
    need_bitmap();
    sheet_frame(s, f, &fr);
    if (flip < 0 || flip > 3)
        error(E_FC);
    x = (int16_t)(x - fr.hx);
    y = (int16_t)(y - fr.hy);
    for (j = 0; j < (int16_t)fr.h; j++) {
        int16_t ty = (int16_t)(y + j);
        const uint8_t *src = fr.pix + (uint32_t)(flip & 2 ? fr.h - 1 - j : j) * fr.stride;
        if (BRK_EVERY(j))
            brk();
        if (ty < 0 || ty >= SCR_H)
            continue;
        for (i = 0; i < (int16_t)fr.w; i++) {
            int16_t tx = (int16_t)(x + i);
            uint8_t c = src[flip & 1 ? fr.w - 1 - i : i];
            if (!c || tx < 0 || tx >= SCR_W)
                continue;
            if (gmode == 2)
                bmp[(uint32_t)ty * SCR_W + (uint16_t)tx] = c;
            else
                nib_set(bmp + (uint32_t)ty * (SCR_W / 2), tx,
                        (uint8_t)((pal_rgb[c][0] >= 128) << 2 | (pal_rgb[c][1] >= 128) << 1 | (pal_rgb[c][2] >= 128)));
        }
    }
}

/* ---- sound ---- */

static uint32_t snd_end[3];
static uint8_t snd_timed[3];

void snd_set(int v, uint16_t freq, int vol, int16_t frames)
{
    uint32_t r;
    if (v < 0 || v > 2 || vol < 0 || vol > 15)
        error(E_FC);
    r = SND_BASE + 8u * (uint32_t)v;
    hal_poke16(r, freq);
    hal_poke16(r + 2, (uint16_t)vol);
    snd_timed[v] = frames > 0;
    if (frames > 0)
        snd_end[v] = hal_frame() + (uint32_t)frames;
}

void snd_wave(int v, int w)
{
    if (v < 0 || v > 2 || w < 0 || w > 3)
        error(E_FC);
    hal_poke16(SND_BASE + 8u * (uint32_t)v + 4u, (uint16_t)w);
}

void snd_poll(void)
{
    int v;
    uint32_t f = 0;
    for (v = 0; v < 3; v++)
        if (snd_timed[v]) {
            if (!f)
                f = hal_frame();
            if ((int32_t)(f - snd_end[v]) >= 0) {
                hal_poke16(SND_BASE + 8u * (uint32_t)v + 2u, 0);
                snd_timed[v] = 0;
            }
        }
}
