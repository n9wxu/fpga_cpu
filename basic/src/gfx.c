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

/* ---- sprites ---- */

#define NSPR 16
#define SPR_MAX 32

typedef struct {
    uint8_t w, h, vis;
    uint16_t fill; /* pixels defined so far */
    int16_t x, y;
} spr_t;

typedef struct {
    int16_t x, y;
    uint8_t w, h;
} rect_t;

static spr_t spr[NSPR];
static uint8_t sprpix[NSPR][SPR_MAX * SPR_MAX]; /* 0-7 color, 8 transparent */
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
    memset(hal_mem(SPR_ADDR), 0x88, SPR_SIZE * 2u);
    spr_ready = 1;
    spr_shown = 0;
    vid_ctrl_set(VC_SPRBUF, 0);
}

void spr_define(int n, int w, int h)
{
    spr_check(n);
    if (w < 1 || w > SPR_MAX || h < 1 || h > SPR_MAX)
        error(E_FC);
    spr[n].w = (uint8_t)w;
    spr[n].h = (uint8_t)h;
    spr[n].fill = 0;
    memset(sprpix[n], 8, sizeof sprpix[n]);
    spr_dirty = 1;
}

void spr_pixels(int n, const char *s, uint16_t len)
{
    uint16_t i, total;
    spr_check(n);
    total = (uint16_t)(spr[n].w * spr[n].h);
    for (i = 0; i < len && spr[n].fill < total; i++) {
        char c = s[i];
        uint8_t v = 8;
        if (c >= '0' && c <= '7')
            v = (uint8_t)(c - '0');
        sprpix[n][spr[n].fill++] = v;
    }
    spr_dirty = 1;
}

void spr_at(int n, int16_t x, int16_t y)
{
    spr_check(n);
    if (!spr[n].w)
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

static void rect_fill(uint8_t *buf, int16_t x, int16_t y, int w, int h, uint8_t c)
{
    int16_t x1 = (int16_t)(x + w - 1), y1 = (int16_t)(y + h - 1), yy;
    if (x < 0)
        x = 0;
    if (x1 >= SCR_W)
        x1 = SCR_W - 1;
    if (x > x1)
        return;
    for (yy = y < 0 ? 0 : y; yy <= y1 && yy < SCR_H; yy++)
        nib_span(buf + (uint32_t)yy * (SCR_W / 2), x, x1, c);
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
        rect_fill(buf, drawn[b][i].x, drawn[b][i].y, drawn[b][i].w, drawn[b][i].h, 8);
    ndrawn[b] = 0;
    for (n = 0; n < NSPR; n++) {
        spr_t *s = &spr[n];
        int j;
        if (!s->vis)
            continue;
        for (j = 0; j < s->h; j++) {
            int16_t y = (int16_t)(s->y + j);
            uint8_t *row, *pix = sprpix[n] + j * s->w;
            if (y < 0 || y >= SCR_H)
                continue;
            row = buf + (uint32_t)y * (SCR_W / 2);
            for (i = 0; i < s->w; i++) {
                int16_t x = (int16_t)(s->x + i);
                if (pix[i] < 8 && x >= 0 && x < SCR_W)
                    nib_set(row, x, pix[i]);
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
