/* Graphics, sprites and sound (docs/io.md).
 *
 * The bitmap is 640x360, either 8-color (4 bits per pixel, [T][R][G][B],
 * left pixel in the high nibble) or hi-color (one palette index per
 * byte). Sprites are drawn in software into the hidden sprite buffer,
 * which is shown on the next VSYNC.
 */
#include "basic.h"

uint16_t vid_ctrl;
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

void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = (int16_t)(x1 > x0 ? x1 - x0 : x0 - x1);
    int16_t dy = (int16_t)(y1 > y0 ? y0 - y1 : y1 - y0);
    int16_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);
    if (y0 == y1) {
        hline(x0, x1, y0);
    } else {
        for (;;) {
            int16_t e2;
            pset(x0, y0);
            if (x0 == x1 && y0 == y1)
                break;
            e2 = (int16_t)(2 * err);
            if (e2 >= dy) {
                err = (int16_t)(err + dy);
                x0 = (int16_t)(x0 + sx);
            }
            if (e2 <= dx) {
                err = (int16_t)(err + dx);
                y0 = (int16_t)(y0 + sy);
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
            pset(x0, y);
            pset(x1, y);
        }
    }
    lastx = x1;
    lasty = y1;
}

void gfx_circle(int16_t cx, int16_t cy, int16_t r, int fill)
{
    int16_t x = r, y = 0, err = (int16_t)(1 - r);
    if (r < 0)
        error(E_FC);
    while (x >= y) {
        if (fill) {
            hline((int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy + y));
            hline((int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy - y));
            hline((int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy + x));
            hline((int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy - x));
        } else {
            pset((int16_t)(cx + x), (int16_t)(cy + y));
            pset((int16_t)(cx - x), (int16_t)(cy + y));
            pset((int16_t)(cx + x), (int16_t)(cy - y));
            pset((int16_t)(cx - x), (int16_t)(cy - y));
            pset((int16_t)(cx + y), (int16_t)(cy + x));
            pset((int16_t)(cx - y), (int16_t)(cy + x));
            pset((int16_t)(cx + y), (int16_t)(cy - x));
            pset((int16_t)(cx - y), (int16_t)(cy - x));
        }
        y++;
        if (err < 0)
            err = (int16_t)(err + 2 * y + 1);
        else {
            x--;
            err = (int16_t)(err + 2 * (y - x) + 1);
        }
    }
    lastx = cx;
    lasty = cy;
}

/* ---- color mapping ----
 * Pictures and sprite sheets arrive as RGBA. Without PALETTE they map to
 * the default palette: the 6x6x6 cube (16-231) and the gray ramp
 * (232-255). JPEG pictures are dithered (Floyd-Steinberg on the cube,
 * ordered on the 8-color bitmap); PNG pixels take the nearest color, so
 * pixel art stays crisp. With PALETTE base, each new color gets the next
 * palette entry from base up, exactly.
 */

static uint8_t cube_q(int16_t v) { return (uint8_t)((v + 25) / 51); } /* 0..5 */

static uint8_t nearest(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t qr = cube_q(r), qg = cube_q(g), qb = cube_q(b);
    int16_t dr = (int16_t)(r - qr * 51), dg = (int16_t)(g - qg * 51), db = (int16_t)(b - qb * 51), gi, gv;
    int32_t ec = (int32_t)dr * dr + (int32_t)dg * dg + (int32_t)db * db, eg;
    gi = (int16_t)(((int16_t)((r + g + b) / 3) - 3) / 10);
    if (gi < 0)
        gi = 0;
    if (gi > 23)
        gi = 23;
    gv = (int16_t)(8 + 10 * gi);
    dr = (int16_t)(r - gv);
    dg = (int16_t)(g - gv);
    db = (int16_t)(b - gv);
    eg = (int32_t)dr * dr + (int32_t)dg * dg + (int32_t)db * db;
    return ec <= eg ? (uint8_t)(16 + 36 * qr + 6 * qg + qb) : (uint8_t)(232 + gi);
}

typedef struct {
    uint8_t base, n, last;
    uint8_t rgb[256][3];
} ptab_t;

static ptab_t *ptab_new(int base)
{
    ptab_t *t;
    if (base < 0)
        return NULL;
    if (base < 1 || base > 255)
        img_fail(E_FC);
    t = (ptab_t *)img_scratch(sizeof(ptab_t));
    t->base = (uint8_t)base;
    return t;
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

/* ---- PICTURE: an image into the bitmap ---- */

typedef struct {
    img_sink_t s;
    int16_t x0, y0;
    uint16_t xa, xb; /* visible source columns */
    uint8_t dither;
    int16_t *err;    /* Floyd-Steinberg: two rows of (visible + 2) x RGB, x16 */
    ptab_t *pt;
    int palbase;
} pic_t;

static void pic_start(img_sink_t *s, uint16_t w, uint16_t h, int photo)
{
    pic_t *p = (pic_t *)s;
    int32_t a = -(int32_t)p->x0, b = (int32_t)SCR_W - p->x0;
    (void)h;
    if (a < 0)
        a = 0;
    if (b > (int32_t)w)
        b = w;
    if (b < a)
        b = a;
    p->pt = ptab_new(p->palbase);
    p->xa = (uint16_t)a;
    p->xb = (uint16_t)b;
    p->dither = (uint8_t)(photo && !p->pt);
    if (p->dither && gmode == 2)
        p->err = (int16_t *)img_scratch((uint32_t)(p->xb - p->xa + 2u) * 12u);
}

static const uint8_t bayer4[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

static int pic_row(img_sink_t *s, uint16_t y, const uint8_t *rgba)
{
    pic_t *p = (pic_t *)s;
    int32_t ty = (int32_t)p->y0 + y;
    uint16_t i, k, n = (uint16_t)(p->xb - p->xa);
    int16_t tx = (int16_t)(p->x0 + (int16_t)p->xa);
    const uint8_t *px = rgba + 4u * p->xa;
    int16_t *e0 = NULL, *e1 = NULL;
    if (ty < 0)
        return 0;
    if (ty >= SCR_H || !n)
        return 1;
    if (p->err) {
        e0 = p->err + (y & 1 ? 0 : 3u * (n + 2u));
        e1 = p->err + (y & 1 ? 3u * (n + 2u) : 0);
        memset(e1, 0, 3u * (n + 2u) * sizeof(int16_t));
    }
    if (gmode == 2) {
        uint8_t *o = bmp + (uint32_t)ty * SCR_W + (uint16_t)tx;
        for (k = 0; k < n; k++, px += 4) {
            if (px[3] < 128)
                continue;
            if (p->pt)
                o[k] = exact(p->pt, px);
            else if (!e0)
                o[k] = nearest(px[0], px[1], px[2]);
            else {
                uint8_t q[3];
                for (i = 0; i < 3; i++) {
                    int16_t v = (int16_t)(px[i] + e0[3 * (k + 1) + i] / 16), d;
                    if (v < 0)
                        v = 0;
                    if (v > 255)
                        v = 255;
                    q[i] = cube_q(v);
                    d = (int16_t)(v - q[i] * 51);
                    e0[3 * (k + 2) + i] = (int16_t)(e0[3 * (k + 2) + i] + d * 7);
                    e1[3 * k + i] = (int16_t)(e1[3 * k + i] + d * 3);
                    e1[3 * (k + 1) + i] = (int16_t)(e1[3 * (k + 1) + i] + d * 5);
                    e1[3 * (k + 2) + i] = (int16_t)(e1[3 * (k + 2) + i] + d);
                }
                o[k] = (uint8_t)(16 + 36 * q[0] + 6 * q[1] + q[2]);
            }
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

void gfx_picture(const char *name, int16_t x, int16_t y, int palbase)
{
    pic_t p;
    memset(&p, 0, sizeof p);
    if (!gmode)
        gfx_mode(2);
    if (palbase >= 0 && gmode != 2)
        error(E_FC); /* the 8-color bitmap has no palette */
    p.s.start = pic_start;
    p.s.row = pic_row;
    p.x0 = x;
    p.y0 = y;
    p.palbase = palbase;
    img_load(name, &p.s);
}

/* ---- sprite sheets ----
 * A sheet is a decoded image, one palette index per pixel (0 is
 * transparent), cut into fw x fh frames numbered left to right, top to
 * bottom. Sheets live in the array area, so CLEAR and RUN drop them.
 */

#define NSHEET 8

typedef struct {
    uint8_t *pix;
    uint32_t cap; /* bytes allocated, reused by a reload that fits */
    uint16_t w, h, cols, count;
    uint8_t fw, fh;
} sheet_t;

static sheet_t sheets[NSHEET];

typedef struct {
    img_sink_t s;
    sheet_t *sh;
    ptab_t *pt;
    int fw, fh, palbase;
} shs_t;

static void sheet_start(img_sink_t *s, uint16_t w, uint16_t h, int photo)
{
    shs_t *p = (shs_t *)s;
    sheet_t *sh = p->sh;
    uint32_t n = (uint32_t)w * h;
    (void)photo;
    if (p->fw > (int)w || p->fh > (int)h)
        img_fail(E_FC);
    p->pt = ptab_new(p->palbase);
    if (!sh->pix || sh->cap < n) {
        sh->pix = img_alloc(n);
        sh->cap = n;
    }
    sh->w = w;
    sh->h = h;
    sh->fw = (uint8_t)p->fw;
    sh->fh = (uint8_t)p->fh;
    sh->cols = (uint16_t)(w / (uint16_t)p->fw);
}

static int sheet_row(img_sink_t *s, uint16_t y, const uint8_t *rgba)
{
    shs_t *p = (shs_t *)s;
    uint8_t *o = p->sh->pix + (uint32_t)y * p->sh->w;
    uint16_t x;
    for (x = 0; x < p->sh->w; x++, rgba += 4)
        o[x] = rgba[3] < 128 ? 0 : p->pt ? exact(p->pt, rgba) : nearest(rgba[0], rgba[1], rgba[2]);
    return 0;
}

static void sheet_check(int n)
{
    if (n < 0 || n >= NSHEET)
        error(E_FC);
}

static void spr_drop_sheet(int n);

void sheet_load(int n, const char *name, int fw, int fh, int palbase)
{
    shs_t p;
    sheet_t *sh;
    sheet_check(n);
    if (fw < 1 || fw > 255 || fh < 1 || fh > 255)
        error(E_FC);
    sh = &sheets[n];
    sh->count = 0;
    spr_drop_sheet(n); /* its pixels are about to change */
    memset(&p, 0, sizeof p);
    p.s.start = sheet_start;
    p.s.row = sheet_row;
    p.sh = sh;
    p.fw = fw;
    p.fh = fh;
    p.palbase = palbase;
    img_load(name, &p.s);
    sh->count = (uint16_t)(sh->cols * (sh->h / sh->fh));
}

int sheet_frames(int n)
{
    sheet_check(n);
    return sheets[n].count;
}

/* ---- sprites ----
 * 16 sprites, drawn in software into the hidden 8 bpp sprite buffer
 * (palette indices, 0 transparent) and shown on the next VSYNC. Sprite
 * 0 is drawn first, so higher numbers are in front. A sprite shows either
 * its own pixels (DEFINE, up to 32x32) or a frame of a sheet.
 */

#define NSPR 16
#define SPR_MAX 32

typedef struct {
    const uint8_t *pix; /* NULL: nothing to show */
    uint16_t stride;
    uint8_t w, h, vis, flip;
    uint8_t sheet; /* 0: own pixels, else the sheet number + 1 */
    uint16_t fill; /* DEFINE pixels given so far */
    int16_t x, y;
} spr_t;

typedef struct {
    int16_t x, y;
    uint8_t w, h;
} rect_t;

static spr_t spr[NSPR];
static uint8_t sprpix[NSPR][SPR_MAX * SPR_MAX];
static rect_t drawn[2][NSPR];
static uint8_t ndrawn[2];
static int spr_ready, spr_dirty, spr_shown;

static void spr_check(int n)
{
    if (n < 0 || n >= NSPR)
        error(E_FC);
}

static void spr_init(void)
{
    if (spr_ready)
        return;
    memset(hal_mem(SPR_ADDR), 0, SPR_SIZE * 2u);
    spr_ready = 1;
    spr_shown = 0;
    vid_ctrl_set(VC_SPRBUF, 0);
    vid_ctrl_set(VC_SPR8, 1);
}

/* Sprites showing frames of sheet n (or of any sheet, n < 0) lose them;
 * they stay hidden until SPRITE ... FRAME picks a frame again. */
static void spr_drop_sheet(int n)
{
    int i;
    for (i = 0; i < NSPR; i++)
        if (spr[i].sheet && (n < 0 || spr[i].sheet == n + 1)) {
            if (spr[i].vis)
                spr_dirty = 1;
            spr[i].pix = NULL;
            spr[i].vis = 0;
            spr[i].sheet = 0;
        }
}

void sheets_reset(void)
{
    memset(sheets, 0, sizeof sheets);
    spr_drop_sheet(-1);
}

void spr_define(int n, int w, int h)
{
    spr_check(n);
    if (w < 1 || w > SPR_MAX || h < 1 || h > SPR_MAX)
        error(E_FC);
    spr[n].w = (uint8_t)w;
    spr[n].h = (uint8_t)h;
    spr[n].fill = 0;
    spr[n].sheet = 0;
    spr[n].flip = 0;
    spr[n].stride = (uint16_t)w;
    spr[n].pix = sprpix[n];
    memset(sprpix[n], 0, sizeof sprpix[n]);
    spr_dirty = 1;
}

/* '0'-'7' are the 8 colors (black is palette entry 8, because 0 is
 * transparent); any other character is transparent. */
void spr_pixels(int n, const char *s, uint16_t len)
{
    uint16_t i, total;
    spr_check(n);
    total = (uint16_t)(spr[n].w * spr[n].h);
    for (i = 0; i < len && spr[n].fill < total; i++) {
        char c = s[i];
        uint8_t v = 0;
        if (c == '0')
            v = 8;
        else if (c >= '1' && c <= '7')
            v = (uint8_t)(c - '0');
        sprpix[n][spr[n].fill++] = v;
    }
    spr_dirty = 1;
}

void spr_frame(int n, int sh, int f, int flip)
{
    sheet_t *t;
    spr_check(n);
    sheet_check(sh);
    t = &sheets[sh];
    if (!t->count || f < 0 || f >= (int)t->count || flip < 0 || flip > 3)
        error(E_FC);
    spr[n].sheet = (uint8_t)(sh + 1);
    spr[n].w = t->fw;
    spr[n].h = t->fh;
    spr[n].stride = t->w;
    spr[n].flip = (uint8_t)flip;
    spr[n].pix = t->pix + (uint32_t)((uint16_t)f / t->cols) * t->fh * t->w + (uint32_t)((uint16_t)f % t->cols) * t->fw;
    spr_dirty = 1;
}

void spr_at(int n, int16_t x, int16_t y)
{
    spr_check(n);
    if (!spr[n].pix)
        error(E_FC);
    spr_init();
    spr[n].x = x;
    spr[n].y = y;
    spr[n].vis = 1;
    spr_dirty = 1;
    vid_ctrl_set(VC_SPR, 1);
}

void spr_off(int n)
{
    spr_check(n);
    if (spr[n].vis)
        spr_dirty = 1;
    spr[n].vis = 0;
}

static void rect_clear(uint8_t *buf, int16_t x, int16_t y, int w, int h)
{
    int16_t x1 = (int16_t)(x + w - 1), y1 = (int16_t)(y + h - 1), yy;
    if (x < 0)
        x = 0;
    if (x1 >= SCR_W)
        x1 = SCR_W - 1;
    if (x > x1)
        return;
    for (yy = y < 0 ? 0 : y; yy <= y1 && yy < SCR_H; yy++)
        memset(buf + (uint32_t)yy * SCR_W + (uint16_t)x, 0, (size_t)(x1 - x + 1));
}

static void wait_frame(void)
{
    uint32_t f = hal_frame();
    while (hal_frame() == f) {
        if (hal_break())
            brk();
    }
}

/* Draw the visible sprites into the hidden buffer and show it. */
void spr_flush(void)
{
    int b, n, i;
    uint8_t *buf;
    if (!spr_dirty || !spr_ready)
        return;
    b = 1 - spr_shown;
    buf = hal_mem(SPR_ADDR + (uint32_t)b * SPR_SIZE);
    for (i = 0; i < ndrawn[b]; i++)
        rect_clear(buf, drawn[b][i].x, drawn[b][i].y, drawn[b][i].w, drawn[b][i].h);
    ndrawn[b] = 0;
    for (n = 0; n < NSPR; n++) {
        spr_t *s = &spr[n];
        int16_t j, x0 = 0, x1 = s->w;
        if (!s->vis || !s->pix)
            continue;
        if (s->x < 0)
            x0 = (int16_t)-s->x;
        if (s->x + x1 > SCR_W)
            x1 = (int16_t)(SCR_W - s->x);
        for (j = 0; j < s->h && x0 < x1; j++) {
            int16_t y = (int16_t)(s->y + j);
            const uint8_t *src;
            uint8_t *row;
            if (y < 0 || y >= SCR_H)
                continue;
            src = s->pix + (uint32_t)(s->flip & 2 ? s->h - 1 - j : j) * s->stride;
            row = buf + (uint32_t)y * SCR_W;
            if (s->flip & 1) {
                for (i = x0; i < x1; i++)
                    if (src[s->w - 1 - i])
                        row[s->x + i] = src[s->w - 1 - i];
            } else {
                for (i = x0; i < x1; i++)
                    if (src[i])
                        row[s->x + i] = src[i];
            }
        }
        drawn[b][ndrawn[b]].x = s->x;
        drawn[b][ndrawn[b]].y = s->y;
        drawn[b][ndrawn[b]].w = s->w;
        drawn[b][ndrawn[b]].h = s->h;
        ndrawn[b]++;
    }
    wait_frame();
    spr_shown = b;
    vid_ctrl_set(VC_SPRBUF, b);
    spr_dirty = 0;
}

void vsync(void)
{
    if (spr_dirty && spr_ready)
        spr_flush();
    else
        wait_frame();
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
