/* Sprite sheets and sprites.
 *
 * Sheets are the image cache. A file is decoded once into the array area
 * as palette indices (0 transparent) and cut into frames; it stays there
 * until RUN, CLEAR or an edit. Each sheet has a hot spot: the point of a
 * frame that a sprite's position refers to, and that it turns and scales
 * about.
 *
 * Sprites are attributes, like a hardware sprite table: frame, position,
 * velocity, rotation and its rate, scale, flip and animation. Every VSYNC
 * steps the motion and animation, draws the visible sprites into the
 * hidden 8 bpp buffer, waits for vertical blank and shows it. A rotated
 * or scaled sprite is drawn by mapping each screen pixel back into its
 * frame, which is also how a hardware sprite engine reading the sheets
 * from sprite RAM would do it, so programs would not change.
 *
 * Sprite 0 is drawn first, so higher numbers are in front.
 */
#include <math.h>

#include "basic.h"

/* ---- sheets ---- */

#define NSHEET 16

typedef struct {
    uint8_t *pix;
    uint32_t cap; /* bytes allocated, reused by a reload that fits */
    uint16_t w, h, fw, fh, cols, count;
    int16_t hx, hy;
} sheet_t;

static sheet_t sheets[NSHEET];

typedef struct {
    img_sink_t s;
    sheet_t *sh;
    void *cm;
    int fw, fh, palbase;
} shs_t;

static void sheet_start(img_sink_t *s, const img_info_t *in)
{
    shs_t *p = (shs_t *)s;
    sheet_t *sh = p->sh;
    uint32_t n = (uint32_t)in->w * in->h;
    if (!p->fw) {
        p->fw = in->w;
        p->fh = in->h;
    }
    if (p->fw > (int)in->w || p->fh > (int)in->h)
        img_fail(E_FC);
    if (!sh->pix || sh->cap < n) {
        sh->pix = img_alloc(n);
        sh->cap = n;
    }
    sh->w = in->w;
    sh->h = in->h;
    sh->fw = (uint16_t)p->fw;
    sh->fh = (uint16_t)p->fh;
    sh->cols = (uint16_t)(in->w / (uint16_t)p->fw);
    p->cm = cmap_new(in, p->palbase);
}

static int sheet_row(img_sink_t *s, uint16_t y, const uint8_t *rgba, const uint8_t *idx)
{
    shs_t *p = (shs_t *)s;
    uint8_t *o = p->sh->pix + (uint32_t)y * p->sh->w;
    uint16_t x;
    for (x = 0; x < p->sh->w; x++)
        o[x] = cmap_px(p->cm, rgba, idx, x);
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
    if (fw < 0 || fh < 0 || !fw != !fh)
        error(E_FC);
    sh = &sheets[n];
    sh->count = 0;
    sh->hx = sh->hy = 0;
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

void sheet_frame(int n, int f, frame_t *fr)
{
    sheet_t *t;
    sheet_check(n);
    t = &sheets[n];
    if (!t->count || f < 0 || f >= (int)t->count)
        error(E_FC);
    fr->pix = t->pix + (uint32_t)((uint16_t)f / t->cols) * t->fh * t->w + (uint32_t)((uint16_t)f % t->cols) * t->fw;
    fr->w = t->fw;
    fr->h = t->fh;
    fr->stride = t->w;
    fr->hx = t->hx;
    fr->hy = t->hy;
}

/* ---- sprites ---- */

#define NSPR 16
#define SPR_MAX 32
#define FIX 256 /* positions and velocities are in 1/256 pixel */

typedef struct {
    const uint8_t *pix; /* NULL: nothing to show */
    uint16_t w, h, stride;
    int16_t hx, hy;
    uint8_t vis, flip, sheet; /* sheet: 0 own pixels, else sheet number + 1 */
    uint8_t a_rate, a_count;
    uint16_t frame, a_first, a_last, fill;
    int32_t px, py, vx, vy; /* hot spot position and velocity, 1/256 pixel */
    float ang, spin, sx, sy;
    uint8_t affine;          /* turned or scaled */
    int32_t ia, ib, ic, id;  /* screen offset -> frame offset, 16.16 */
    int16_t bx0, by0, bx1, by1; /* box it covers, relative to the position */
} spr_t;

typedef struct {
    int16_t x, y, w, h;
} rect_t;

static spr_t spr[NSPR];
static uint8_t sprpix[NSPR][SPR_MAX * SPR_MAX];
static rect_t drawn[2][NSPR];
static uint8_t ndrawn[2];
static int spr_ready, spr_dirty, spr_shown, spr_setup;

static void spr_check(int n)
{
    if (!spr_setup) {
        int i;
        for (i = 0; i < NSPR; i++)
            spr[i].sx = spr[i].sy = 1.0f;
        spr_setup = 1;
    }
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

static int16_t fl16(float v) { return (int16_t)(v < -32000.0f ? -32000 : v > 32000.0f ? 32000 : floorf(v)); }

/* Recompute the transform and the box the sprite covers. */
static void spr_geom(spr_t *s)
{
    float r, c, sn, u[2], v[2], x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int i, j;
    s->affine = s->ang != 0.0f || s->sx != 1.0f || s->sy != 1.0f;
    if (!s->affine) {
        s->bx0 = (int16_t)-s->hx;
        s->by0 = (int16_t)-s->hy;
        s->bx1 = (int16_t)(s->w - s->hx);
        s->by1 = (int16_t)(s->h - s->hy);
        return;
    }
    r = s->ang * 0.017453293f;
    c = cosf(r);
    sn = sinf(r);
    s->ia = (int32_t)(c / s->sx * 65536.0f);
    s->ib = (int32_t)(sn / s->sx * 65536.0f);
    s->ic = (int32_t)(-sn / s->sy * 65536.0f);
    s->id = (int32_t)(c / s->sy * 65536.0f);
    u[0] = (float)-s->hx;
    u[1] = (float)(s->w - s->hx);
    v[0] = (float)-s->hy;
    v[1] = (float)(s->h - s->hy);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++) {
            float x = c * s->sx * u[i] - sn * s->sy * v[j], y = sn * s->sx * u[i] + c * s->sy * v[j];
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    s->bx0 = fl16(x0);
    s->by0 = fl16(y0);
    s->bx1 = (int16_t)(fl16(x1) + 1);
    s->by1 = (int16_t)(fl16(y1) + 1);
}

/* The sprite's pixel at screen (x, y), 0 if transparent or outside it. */
static uint8_t spr_src(const spr_t *s, int16_t x, int16_t y)
{
    int32_t u, v;
    int32_t dx = (int32_t)x - (s->px >> 8), dy = (int32_t)y - (s->py >> 8);
    if (s->affine) {
        int32_t dx2 = 2 * dx + 1, dy2 = 2 * dy + 1;
        u = (s->ia * dx2 + s->ib * dy2 + ((int32_t)s->hx << 17)) >> 17;
        v = (s->ic * dx2 + s->id * dy2 + ((int32_t)s->hy << 17)) >> 17;
    } else {
        u = dx + s->hx;
        v = dy + s->hy;
    }
    if (u < 0 || v < 0 || u >= s->w || v >= s->h)
        return 0;
    if (s->flip & 1)
        u = s->w - 1 - u;
    if (s->flip & 2)
        v = s->h - 1 - v;
    return s->pix[(uint32_t)v * s->stride + (uint32_t)u];
}

static void spr_set_frame(spr_t *s, uint16_t f)
{
    frame_t fr;
    sheet_frame(s->sheet - 1, f, &fr);
    s->frame = f;
    s->pix = fr.pix;
    s->w = fr.w;
    s->h = fr.h;
    s->stride = fr.stride;
    s->hx = fr.hx;
    s->hy = fr.hy;
}

/* Sprites showing frames of sheet n (any sheet if n < 0) lose them and
 * stay hidden until SPRITE ... FRAME picks a frame again. */
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
            spr[i].a_rate = 0;
        }
}

void sheets_reset(void)
{
    memset(sheets, 0, sizeof sheets);
    spr_drop_sheet(-1);
}

void sheet_hotspot(int n, int16_t x, int16_t y)
{
    int i;
    sheet_check(n);
    sheets[n].hx = x;
    sheets[n].hy = y;
    for (i = 0; i < NSPR; i++)
        if (spr[i].sheet == n + 1) {
            spr[i].hx = x;
            spr[i].hy = y;
            spr_geom(&spr[i]);
            spr_dirty = 1;
        }
}

void spr_define(int n, int w, int h)
{
    spr_t *s;
    spr_check(n);
    if (w < 1 || w > SPR_MAX || h < 1 || h > SPR_MAX)
        error(E_FC);
    s = &spr[n];
    s->w = (uint16_t)w;
    s->h = (uint16_t)h;
    s->stride = (uint16_t)w;
    s->hx = s->hy = 0;
    s->fill = 0;
    s->sheet = 0;
    s->flip = 0;
    s->a_rate = 0;
    s->pix = sprpix[n];
    memset(sprpix[n], 0, sizeof sprpix[n]);
    spr_geom(s);
    spr_dirty = 1;
}

/* '0'-'7' are the 8 colors (black is palette entry 8, because 0 is
 * transparent); any other character is transparent. */
void spr_pixels(int n, const char *str, uint16_t len)
{
    uint16_t i, total;
    spr_check(n);
    total = (uint16_t)(spr[n].w * spr[n].h);
    for (i = 0; i < len && spr[n].fill < total; i++) {
        char c = str[i];
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
    spr_t *s;
    spr_check(n);
    sheet_check(sh);
    if (flip < 0 || flip > 3)
        error(E_FC);
    s = &spr[n];
    s->sheet = (uint8_t)(sh + 1);
    s->a_rate = 0;
    spr_set_frame(s, (uint16_t)f);
    s->flip = (uint8_t)flip;
    spr_geom(s);
    spr_dirty = 1;
}

void spr_flip(int n, int flip)
{
    spr_check(n);
    if (flip < 0 || flip > 3)
        error(E_FC);
    spr[n].flip = (uint8_t)flip;
    spr_dirty = 1;
}

void spr_at(int n, int16_t x, int16_t y)
{
    spr_check(n);
    if (!spr[n].pix)
        error(E_FC);
    spr_init();
    spr[n].px = (int32_t)x * FIX;
    spr[n].py = (int32_t)y * FIX;
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

void spr_move(int n, float dx, float dy)
{
    spr_check(n);
    /* Velocities are bounded so px + vx cannot overflow an int32 before it
     * is clamped in spr_step: under 32768 pixels per frame. */
    if (!(dx > -32768.0f && dx < 32768.0f && dy > -32768.0f && dy < 32768.0f))
        error(E_FC);
    spr[n].vx = to_i32(dx * FIX);
    spr[n].vy = to_i32(dy * FIX);
}

void spr_turn(int n, float deg, float spin)
{
    spr_check(n);
    spr[n].ang = fmodf(deg, 360.0f);
    spr[n].spin = spin;
    spr_geom(&spr[n]);
    spr_dirty = 1;
}

void spr_scale(int n, float sx, float sy)
{
    spr_check(n);
    if (sx < 0.125f || sx > 8.0f || sy < 0.125f || sy > 8.0f)
        error(E_FC);
    spr[n].sx = sx;
    spr[n].sy = sy;
    spr_geom(&spr[n]);
    spr_dirty = 1;
}

/* ANIMATE first, last, rate: step through frames first..last (backwards
 * if last < first), one step every rate VSYNCs; rate 0 stops. */
void spr_animate(int n, int first, int last, int rate)
{
    spr_t *s;
    spr_check(n);
    s = &spr[n];
    if (!s->sheet || rate < 0 || rate > 255)
        error(E_FC);
    if (first < 0 || last < 0 || first >= sheets[s->sheet - 1].count || last >= sheets[s->sheet - 1].count)
        error(E_FC);
    s->a_first = (uint16_t)first;
    s->a_last = (uint16_t)last;
    s->a_rate = (uint8_t)rate;
    s->a_count = 0;
    spr_set_frame(s, (uint16_t)first);
    spr_geom(s);
    spr_dirty = 1;
}

int spr_pos(int n, int y)
{
    spr_check(n);
    /* positions are kept within int16 pixels: the value fits a 16-bit int */
    return (int16_t)((y ? spr[n].py : spr[n].px) >> 8);
}

/* Do two visible sprites touch? Pixel-exact, after rotation, scaling and
 * flips, at their current positions. */
static int hit_pair(const spr_t *a, const spr_t *b)
{
    /* 32-bit: position plus box offset can pass 32767 */
    int32_t ax = a->px >> 8, ay = a->py >> 8, bx = b->px >> 8, by = b->py >> 8;
    int32_t x0 = ax + a->bx0, y0 = ay + a->by0, x1 = ax + a->bx1, y1 = ay + a->by1;
    int32_t x, y;
    if (!a->vis || !b->vis || !a->pix || !b->pix)
        return 0;
    if (bx + b->bx0 > x0) x0 = bx + b->bx0;
    if (by + b->by0 > y0) y0 = by + b->by0;
    if (bx + b->bx1 < x1) x1 = bx + b->bx1;
    if (by + b->by1 < y1) y1 = by + b->by1;
    if (x1 > 32767) x1 = 32767;
    if (y1 > 32767) y1 = 32767;
    if (x0 < -32768) x0 = -32768;
    if (y0 < -32768) y0 = -32768;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (spr_src(a, (int16_t)x, (int16_t)y) && spr_src(b, (int16_t)x, (int16_t)y))
                return 1;
    return 0;
}

int spr_hit(int a, int b)
{
    int i;
    spr_check(a);
    if (b >= 0) {
        spr_check(b);
        return a != b && hit_pair(&spr[a], &spr[b]) ? -1 : 0;
    }
    for (i = 0; i < NSPR; i++)
        if (i != a && hit_pair(&spr[a], &spr[i]))
            return i;
    return -1;
}

/* ---- drawing ---- */

static void wait_frame(void)
{
    uint32_t f = hal_frame();
    while (hal_frame() == f) {
        if (hal_break())
            brk();
    }
}

/* One frame of motion, spin and animation. */
static void spr_step(void)
{
    int n;
    for (n = 0; n < NSPR; n++) {
        spr_t *s = &spr[n];
        int ch = 0;
        if (s->vx || s->vy) {
            s->px += s->vx;
            s->py += s->vy;
            if (s->px > 32000L * FIX) s->px = 32000L * FIX;
            if (s->px < -32000L * FIX) s->px = -32000L * FIX;
            if (s->py > 32000L * FIX) s->py = 32000L * FIX;
            if (s->py < -32000L * FIX) s->py = -32000L * FIX;
            ch = 1;
        }
        if (s->spin != 0.0f) {
            s->ang = fmodf(s->ang + s->spin, 360.0f);
            spr_geom(s);
            ch = 1;
        }
        if (s->a_rate && s->sheet && ++s->a_count >= s->a_rate) {
            uint16_t f = s->frame;
            s->a_count = 0;
            if (s->a_first <= s->a_last)
                f = f >= s->a_last ? s->a_first : (uint16_t)(f + 1);
            else
                f = f <= s->a_last ? s->a_first : (uint16_t)(f - 1);
            spr_set_frame(s, f);
            ch = 1;
        }
        if (ch && s->vis)
            spr_dirty = 1;
    }
}

static void draw(uint8_t *buf, spr_t *s, rect_t *r)
{
    int16_t X0 = (int16_t)(s->px >> 8), Y0 = (int16_t)(s->py >> 8), x, y;
    int16_t x0 = (int16_t)(X0 + s->bx0), y0 = (int16_t)(Y0 + s->by0), x1 = (int16_t)(X0 + s->bx1), y1 = (int16_t)(Y0 + s->by1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCR_W) x1 = SCR_W;
    if (y1 > SCR_H) y1 = SCR_H;
    r->x = x0;
    r->y = y0;
    r->w = (int16_t)(x1 > x0 ? x1 - x0 : 0);
    r->h = (int16_t)(y1 > y0 ? y1 - y0 : 0);
    for (y = y0; y < y1; y++) {
        uint8_t *row = buf + (uint32_t)y * SCR_W;
        if (s->affine) {
            /* step through the frame along the screen row, 16.16 x2 */
            int32_t dx2 = 2 * (x0 - X0) + 1, dy2 = 2 * (y - Y0) + 1;
            int32_t u2 = s->ia * dx2 + s->ib * dy2 + ((int32_t)s->hx << 17);
            int32_t v2 = s->ic * dx2 + s->id * dy2 + ((int32_t)s->hy << 17);
            for (x = x0; x < x1; x++, u2 += 2 * s->ia, v2 += 2 * s->ic) {
                int32_t u = u2 >> 17, v = v2 >> 17;
                uint8_t c;
                if (u < 0 || v < 0 || u >= s->w || v >= s->h)
                    continue;
                if (s->flip & 1) u = s->w - 1 - u;
                if (s->flip & 2) v = s->h - 1 - v;
                c = s->pix[(uint32_t)v * s->stride + (uint32_t)u];
                if (c)
                    row[x] = c;
            }
        } else {
            int16_t v = (int16_t)(y - Y0 + s->hy);
            const uint8_t *src = s->pix + (uint32_t)(s->flip & 2 ? s->h - 1 - v : v) * s->stride;
            for (x = x0; x < x1; x++) {
                int16_t u = (int16_t)(x - X0 + s->hx);
                uint8_t c = src[s->flip & 1 ? s->w - 1 - u : u];
                if (c)
                    row[x] = c;
            }
        }
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
    for (i = 0; i < ndrawn[b]; i++) {
        rect_t *r = &drawn[b][i];
        int16_t y;
        for (y = r->y; y < r->y + r->h; y++)
            memset(buf + (uint32_t)y * SCR_W + (uint16_t)r->x, 0, (size_t)r->w);
    }
    ndrawn[b] = 0;
    for (n = 0; n < NSPR; n++)
        if (spr[n].vis && spr[n].pix)
            draw(buf, &spr[n], &drawn[b][ndrawn[b]++]);
    wait_frame();
    spr_shown = b;
    vid_ctrl_set(VC_SPRBUF, b);
    spr_dirty = 0;
}

void vsync(void)
{
    spr_check(0);
    spr_step();
    if (spr_dirty && spr_ready)
        spr_flush();
    else
        wait_frame();
}
