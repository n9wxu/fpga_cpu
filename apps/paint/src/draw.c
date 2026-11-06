/* draw.c: drawing into the picture, previews, operations and the undo log
 * (paint.h). */
#include "paint.h"
#include <string.h>

uint32_t rowoff[DH];

/* 8x8 patterns: a set bit is the foreground colour; the first is solid */
const uint8_t patterns[NPAT][8] = {
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 },
    { 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22 },
    { 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD },
    { 0x80, 0x00, 0x08, 0x00, 0x80, 0x00, 0x08, 0x00 },
    { 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00 },
    { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA },
    { 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11 },
    { 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88 },
    { 0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08 },
    { 0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88 },
    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 },
    { 0xCC, 0xCC, 0x33, 0x33, 0xCC, 0xCC, 0x33, 0x33 },
    { 0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x10, 0x00 },
    { 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F },
    { 0x00, 0x66, 0x66, 0x00, 0x00, 0x66, 0x66, 0x00 },
};

paint_t paint_line, paint_fill, paint_back;
const uint8_t (*cur_paint)[4] = (const uint8_t (*)[4])paint_line;

void paint_make(paint_t p, uint8_t fg, uint8_t bg, int pat)
{
    for (int r = 0; r < 8; r++) {
        uint8_t bits = patterns[pat][r];
        for (int k = 0; k < 4; k++) {
            uint8_t hi = (bits & (0x80 >> (2 * k))) ? fg : bg;
            uint8_t lo = (bits & (0x40 >> (2 * k))) ? fg : bg;
            p[r][k] = (uint8_t)(hi << 4 | lo);
        }
    }
}

/* ---- dirty rectangles ---- */
int dirty_x0 = DW, dirty_y0 = DH, dirty_x1 = -1, dirty_y1 = -1;
static int op_x0, op_y0, op_x1, op_y1;  /* what this operation changed */
uint8_t op_active;

/* the preview: (offset << 8 | old byte) for each byte written, and what it covered */
#define PV_LIST ((uint32_t *)SCRATCH_A)
#define PV_CAP ((unsigned)(SCRATCH_SZ / 4))
static uint8_t pv_on, pv_over;
static unsigned pv_n;
static int pv_x0, pv_y0, pv_x1, pv_y1;

void mark(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DW - 1) x1 = DW - 1;
    if (y1 > DH - 1) y1 = DH - 1;
    if (x0 > x1 || y0 > y1)
        return;
    if (x0 < dirty_x0) dirty_x0 = x0;
    if (y0 < dirty_y0) dirty_y0 = y0;
    if (x1 > dirty_x1) dirty_x1 = x1;
    if (y1 > dirty_y1) dirty_y1 = y1;
    if (x0 < op_x0) op_x0 = x0;
    if (y0 < op_y0) op_y0 = y0;
    if (x1 > op_x1) op_x1 = x1;
    if (y1 > op_y1) op_y1 = y1;
    if (pv_on) {
        if (x0 < pv_x0) pv_x0 = x0;
        if (y0 < pv_y0) pv_y0 = y0;
        if (x1 > pv_x1) pv_x1 = x1;
        if (y1 > pv_y1) pv_y1 = y1;
    }
}

void mark_all(void) { mark(0, 0, DW - 1, DH - 1); }

void draw_init(void)
{
    uint32_t o = 0;
    for (int y = 0; y < DH; y++, o += STRIDE)
        rowoff[y] = o;
}

/* ---- spans: everything is drawn with them ---- */
static void rec(uint8_t *row, int b, int e)
{
    uint32_t off = (uint32_t)(uintptr_t)row - DOC_A;
    for (int i = b; i <= e; i++) {
        if (pv_n < PV_CAP)
            PV_LIST[pv_n++] = (off + (unsigned)i) << 8 | row[i];
        else {
            pv_over = 1;
            return;
        }
    }
}

void span(int x0, int x1, int y)
{
    if ((unsigned)y >= DH)
        return;
    if (x0 < 0)
        x0 = 0;
    if (x1 > DW - 1)
        x1 = DW - 1;
    if (x0 > x1)
        return;
    if (x0 < dirty_x0) dirty_x0 = x0;
    if (x1 > dirty_x1) dirty_x1 = x1;
    if (y < dirty_y0) dirty_y0 = y;
    if (y > dirty_y1) dirty_y1 = y;
    if (x0 < op_x0) op_x0 = x0;
    if (x1 > op_x1) op_x1 = x1;
    if (y < op_y0) op_y0 = y;
    if (y > op_y1) op_y1 = y;
    if (pv_on) {
        if (x0 < pv_x0) pv_x0 = x0;
        if (x1 > pv_x1) pv_x1 = x1;
        if (y < pv_y0) pv_y0 = y;
        if (y > pv_y1) pv_y1 = y;
    }
    uint8_t *row = DOC + rowoff[y];
    const uint8_t *rb = cur_paint[y & 7];
    int b = x0 >> 1, e = x1 >> 1;
    uint8_t lm = (x0 & 1) ? 0x0F : 0xFF;    /* the pixels written in the first byte */
    uint8_t rm = (x1 & 1) ? 0xFF : 0xF0;    /* and in the last */
    if (pv_on && !pv_over)
        rec(row, b, e);
    if (b == e) {
        lm &= rm;
        row[b] = (uint8_t)((row[b] & ~lm) | (rb[b & 3] & lm));
        return;
    }
    row[b] = (uint8_t)((row[b] & ~lm) | (rb[b & 3] & lm));
    uint8_t *p = row + b + 1, *pe = row + e;
    int k = (b + 1) & 3;
    while (p < pe) {
        *p++ = rb[k];
        k = (k + 1) & 3;
    }
    row[e] = (uint8_t)((row[e] & ~rm) | (rb[e & 3] & rm));
}

void line1(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;   /* dy <= 0 */
    int err = dx + dy;
    int rs = x0;                            /* the start of this row's run */
    for (;;) {
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err, nx = x0, ny = y0;
        if (e2 >= dy) { err += dy; nx += sx; }
        if (e2 <= dx) { err += dx; ny += sy; }
        if (ny != y0) {                     /* the row is done: its run */
            span(rs < x0 ? rs : x0, rs < x0 ? x0 : rs, y0);
            rs = nx;
        }
        x0 = nx;
        y0 = ny;
    }
    span(rs < x0 ? rs : x0, rs < x0 ? x0 : rs, y0);
}

/* ---- brushes ---- */
int16_t isqrt32(uint32_t v)
{
    uint32_t r = 0, bit = 1UL << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else
            r >>= 1;
        bit >>= 2;
    }
    return (int16_t)r;
}

void brush_make(brush_t *b, int shape, int d)
{
    if (d > MAXBR)
        d = MAXBR;
    if (d < 1)
        d = 1;
    b->n = (uint8_t)d;
    for (int i = 0; i < d; i++) {
        int l = 0, r = d - 1;
        if (shape == BR_ROUND) {
            int dy2 = 2 * i - (d - 1);
            int hw2 = isqrt32((uint32_t)(d * d - dy2 * dy2));
            l = ((d - 1) - hw2 + 1) >> 1;
            r = ((d - 1) + hw2) >> 1;
        } else if (shape == BR_SLASH)
            l = r = d - 1 - i;
        else if (shape == BR_BACK)
            l = r = i;
        b->l[i] = (int8_t)l;
        b->r[i] = (int8_t)r;
    }
}

void stamp(const brush_t *b, int cx, int cy)
{
    int x = cx - b->n / 2, y = cy - b->n / 2;
    for (int i = 0; i < b->n; i++)
        span(x + b->l[i], x + b->r[i], y + i);
}

void stroke(const brush_t *b, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (x0 != x1 || y0 != y1) {
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        stamp(b, x0, y0);
    }
}

/* ---- polygons, in 1/16 pixel (pixel centres at 16x + 8) ---- */
typedef struct { int16_t r0, r1; int32_t x, dx; } edge_t;

void poly_fill16(int n, const int16_t *xs, const int16_t *ys)
{
    static edge_t ed[MAXPOLY + 1];
    static int16_t cx[MAXPOLY + 1];
    int ne = 0, rmin = DH, rmax = -1;
    for (int i = 0; i < n; i++) {
        int j = i + 1 == n ? 0 : i + 1;
        int32_t xa = xs[i], ya = ys[i], xb = xs[j], yb = ys[j];
        if (ya == yb)
            continue;
        if (ya > yb) {
            int32_t t = xa; xa = xb; xb = t;
            t = ya; ya = yb; yb = t;
        }
        /* rows whose centres are in [ya, yb) */
        int r0 = (int)((ya - 8 + 15) >> 4), r1 = (int)((yb - 8 - 1) >> 4);
        if (r1 < r0 || r1 < 0 || r0 >= DH)
            continue;
        edge_t *e = &ed[ne++];
        e->dx = ((xb - xa) << 16) / (yb - ya);          /* x per row, 1/16 px << 12 */
        int32_t yc = (int32_t)r0 * 16 + 8;
        e->x = (xa << 12) + (((xb - xa) * (yc - ya)) << 12) / (yb - ya);
        e->r0 = (int16_t)r0;
        e->r1 = (int16_t)r1;
        if (r0 < rmin) rmin = r0;
        if (r1 > rmax) rmax = r1;
    }
    if (rmin < 0) {
        for (int i = 0; i < ne; i++)
            if (ed[i].r0 < 0) {
                ed[i].x += ed[i].dx * (int32_t)(-ed[i].r0);
                ed[i].r0 = 0;
            }
        rmin = 0;
    }
    if (rmax >= DH)
        rmax = DH - 1;
    for (int r = rmin; r <= rmax; r++) {
        int nc = 0;
        for (int i = 0; i < ne; i++) {
            edge_t *e = &ed[i];
            if (r < e->r0 || r > e->r1)
                continue;
            int16_t v = (int16_t)(e->x >> 12);
            int k = nc++;
            while (k > 0 && cx[k - 1] > v) {   /* insertion sort */
                cx[k] = cx[k - 1];
                k--;
            }
            cx[k] = v;
            e->x += e->dx;
        }
        for (int k = 0; k + 1 < nc; k += 2) {
            int a = (cx[k] - 8 + 15) >> 4, b = (cx[k + 1] - 8 - 1) >> 4;   /* centres in [xl, xr) */
            if (a <= b)
                span(a, b, r);
        }
    }
}

static brush_t cap_brush;
static uint8_t cap_w;

void line_w(int x0, int y0, int x1, int y1, int w)
{
    if (w <= 1) {
        line1(x0, y0, x1, y1);
        return;
    }
    if (cap_w != w) {
        brush_make(&cap_brush, BR_ROUND, w);
        cap_w = (uint8_t)w;
    }
    if (w <= 4) {                           /* thin: a round brush along the line is cheaper */
        stamp(&cap_brush, x0, y0);
        stroke(&cap_brush, x0, y0, x1, y1);
        return;
    }
    int32_t dx = x1 - x0, dy = y1 - y0;
    int len = isqrt32((uint32_t)(dx * dx + dy * dy));
    if (len > 0) {
        int16_t nx = (int16_t)(-dy * w * 8 / len), ny = (int16_t)(dx * w * 8 / len);
        int16_t xs[4], ys[4];
        int16_t ax = (int16_t)(x0 * 16 + 8), ay = (int16_t)(y0 * 16 + 8);
        int16_t bx = (int16_t)(x1 * 16 + 8), by = (int16_t)(y1 * 16 + 8);
        xs[0] = ax + nx; ys[0] = ay + ny;
        xs[1] = bx + nx; ys[1] = by + ny;
        xs[2] = bx - nx; ys[2] = by - ny;
        xs[3] = ax - nx; ys[3] = ay - ny;
        poly_fill16(4, xs, ys);
    }
    stamp(&cap_brush, x0, y0);
    if (len > 0)
        stamp(&cap_brush, x1, y1);
}

/* ---- rectangles, rounded rectangles and ellipses, a row at a time ---- */
typedef struct { int x0, y0, x1, y1, rad; } box_t;

static int ell_row(int x0, int y0, int x1, int y1, int y, int *l, int *r)
{
    if (y < y0 || y > y1)
        return 0;
    int32_t a2 = x1 - x0 + 1, b2 = y1 - y0 + 1;      /* the diameters */
    int32_t dy2 = 2 * (int32_t)y - (y0 + y1);       /* the row's centre from the middle, doubled */
    uint32_t q = (uint32_t)(b2 * b2 - dy2 * dy2);
    int32_t s = q < (1UL << 22) ? isqrt32(q << 8) : (int32_t)isqrt32(q) * 16;   /* 16 sqrt(q) */
    int32_t hw2 = a2 * s / (b2 * 16);
    int32_t c2 = x0 + x1;
    *l = (int)((c2 - hw2 + 1) >> 1);
    *r = (int)((c2 + hw2) >> 1);
    return 1;
}

static int shape_row(int kind, const box_t *b, int y, int *l, int *r)
{
    if (y < b->y0 || y > b->y1 || b->x0 > b->x1)
        return 0;
    if (kind == SH_ELLIPSE)
        return ell_row(b->x0, b->y0, b->x1, b->y1, y, l, r);
    *l = b->x0;
    *r = b->x1;
    int rad = b->rad;
    if (rad > 0) {
        int d = 2 * rad - 1, cl;
        if (y < b->y0 + rad)
            ell_row(b->x0, b->y0, b->x0 + d, b->y0 + d, y, &cl, r);
        else if (y > b->y1 - rad)
            ell_row(b->x0, b->y1 - d, b->x0 + d, b->y1, y, &cl, r);
        else
            return 1;
        *l = cl;
        *r = b->x1 - (cl - b->x0);
    }
    return 1;
}

void shape(int kind, int x0, int y0, int x1, int y1, int w, int mode)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    box_t o = { x0, y0, x1, y1, 0 }, in;
    if (kind == SH_RRECT) {
        int m = (x1 - x0 < y1 - y0 ? x1 - x0 : y1 - y0) + 1;
        o.rad = m / 4 < 12 ? m / 4 : 12;
        if (o.rad < 2 && m >= 4)
            o.rad = 2;
    }
    in.x0 = x0 + w; in.y0 = y0 + w; in.x1 = x1 - w; in.y1 = y1 - w;
    in.rad = o.rad > w ? o.rad - w : 0;
    /* the rows of the top half, each drawn with its mirror image below */
    int ym = (y0 + y1) >> 1;
    for (int y = y0; y <= ym; y++) {
        int y2 = y0 + y1 - y;               /* the mirror row */
        if (y2 < 0)
            break;
        if (y > DH - 1)
            break;
        if (y < 0 && y2 > DH - 1)
            continue;
        int ol, orr, il, ir;
        if (!shape_row(kind, &o, y, &ol, &orr))
            continue;
        int inner = in.y0 <= in.y1 && shape_row(kind, &in, y, &il, &ir) && il <= ir;
        for (int k = 0; k < 2; k++) {
            int yy = k ? y2 : y;
            if (k && y2 == y)
                break;
            if (mode == FM_FILLED) {
                USE(paint_fill);
                span(ol, orr, yy);
                continue;
            }
            if (mode == FM_FRAMED && inner) {
                USE(paint_back);
                span(il, ir, yy);
            }
            USE(paint_line);
            if (inner) {
                span(ol, il - 1, yy);
                span(ir + 1, orr, yy);
            } else
                span(ol, orr, yy);
        }
    }
}

void polygon(int n, const int16_t *xs, const int16_t *ys, int w, int mode, int closed)
{
    if (n < 1)
        return;
    if (closed && mode != FM_OUTLINE && n >= 3) {
        static int16_t px[MAXPOLY], py[MAXPOLY];
        for (int i = 0; i < n; i++) {
            px[i] = (int16_t)(xs[i] * 16 + 8);
            py[i] = (int16_t)(ys[i] * 16 + 8);
        }
        USE(mode == FM_FILLED ? paint_fill : paint_back);
        poly_fill16(n, px, py);
        if (mode == FM_FILLED)
            return;
    }
    USE(paint_line);
    if (n == 1)
        line_w(xs[0], ys[0], xs[0], ys[0], w);
    for (int i = 0; i + 1 < n; i++)
        line_w(xs[i], ys[i], xs[i + 1], ys[i + 1], w);
    if (closed && n > 2)
        line_w(xs[n - 1], ys[n - 1], xs[0], ys[0], w);
}

static int dist(int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    return dx > dy ? dx + dy / 2 : dy + dx / 2;
}

void bezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int w)
{
    /* segments about 12 pixels long, 2 to 24 of them */
    int32_t n = (dist(x0, y0, x1, y1) + dist(x1, y1, x2, y2) + dist(x2, y2, x3, y3)) / 12;
    if (n < 2) n = 2;
    if (n > 24) n = 24;
    int32_t n3 = n * n * n;
    int px = x0, py = y0;
    USE(paint_line);
    for (int32_t i = 1; i <= n; i++) {
        int32_t t = i, u = n - i;
        int32_t a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;   /* / n^3 */
        int32_t xx = a * x0 + b * x1 + c * x2 + d * x3, yy = a * y0 + b * y1 + c * y2 + d * y3;
        int nx = (int)((xx + n3 / 2) / n3), ny = (int)((yy + n3 / 2) / n3);
        line_w(px, py, nx, ny, w);
        px = nx;
        py = ny;
    }
}

/* ---- the paint bucket: a scan-line flood fill, with a mask of the pixels
 * filled (a pattern can leave pixels of the colour being replaced) ---- */
#define MASK ((uint8_t *)SCRATCH_A)
#define NSEED 768
static int16_t seeds[2 * NSEED];
static unsigned nseed;
static uint8_t seed_over;

static inline int visited(int x, int y)
{
    return MASK[(unsigned)y * 80 + (unsigned)(x >> 3)] & (0x80 >> (x & 7));
}

static void push(int x, int y)
{
    if (nseed < NSEED) {
        seeds[2 * nseed] = (int16_t)x;
        seeds[2 * nseed + 1] = (int16_t)y;
        nseed++;
    } else
        seed_over = 1;
}

/* push the start of each run of fillable pixels in row y, x0..x1 */
static void seed_row(int x0, int x1, int y, uint8_t t)
{
    const uint8_t *row = DOC + rowoff[y];
    int in = 0;
    for (int x = x0; x <= x1; x++) {
        uint8_t v = row[x >> 1];
        uint8_t c = (x & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
        int ok = c == t && !visited(x, y);
        if (ok && !in)
            push(x, y);
        in = ok;
    }
}

void flood(int sx, int sy)
{
    if ((unsigned)sx >= DW || (unsigned)sy >= DH)
        return;
    uint8_t t = getpx(DOC, sx, sy);
    int solid = 1;
    for (int r = 0; r < 8 && solid; r++)
        for (int k = 0; k < 4; k++)
            if (cur_paint[r][k] != (uint8_t)(t * 0x11)) { solid = 0; break; }
    if (solid)
        return;                             /* already that colour */
    memset(MASK, 0, (unsigned)(DH * 80));
    nseed = 0;
    push(sx, sy);
    for (;;) {
        while (nseed) {
            nseed--;
            int x = seeds[2 * nseed], y = seeds[2 * nseed + 1];
            const uint8_t *row = DOC + rowoff[y];
            if (visited(x, y) || getpx(DOC, x, y) != t)
                continue;
            int l = x, r = x;
            while (l > 0) {
                uint8_t v = row[(l - 1) >> 1];
                uint8_t c = ((l - 1) & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
                if (c != t || visited(l - 1, y)) break;
                l--;
            }
            while (r < DW - 1) {
                uint8_t v = row[(r + 1) >> 1];
                uint8_t c = ((r + 1) & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
                if (c != t || visited(r + 1, y)) break;
                r++;
            }
            span(l, r, y);
            uint8_t *m = MASK + (unsigned)y * 80;
            for (int i = l; i <= r; i++)
                m[i >> 3] |= (uint8_t)(0x80 >> (i & 7));
            if (y > 0) seed_row(l, r, y - 1, t);
            if (y < DH - 1) seed_row(l, r, y + 1, t);
        }
        if (!seed_over)
            break;
        /* seeds were dropped: find fillable pixels next to filled ones */
        seed_over = 0;
        for (int y = 0; y < DH && !seed_over; y++)
            for (int x = 0; x < DW; x++) {
                if (visited(x, y) || getpx(DOC, x, y) != t)
                    continue;
                if ((y > 0 && visited(x, y - 1)) || (y < DH - 1 && visited(x, y + 1)) ||
                    (x > 0 && visited(x - 1, y)) || (x < DW - 1 && visited(x + 1, y)))
                    push(x, y);
            }
        if (!nseed)
            break;
    }
}

static uint16_t rnd_state = 0xACE1;
static uint16_t rnd16(void)
{
    rnd_state ^= (uint16_t)(rnd_state << 7);
    rnd_state ^= (uint16_t)(rnd_state >> 9);
    rnd_state ^= (uint16_t)(rnd_state << 8);
    return rnd_state;
}

void spray(int x, int y, int r, int n)
{
    int d = 2 * r + 1;
    while (n > 0) {
        int dx = (int)(rnd16() % (unsigned)d) - r, dy = (int)(rnd16() % (unsigned)d) - r;
        if (dx * dx + dy * dy > r * r)
            continue;
        dot(x + dx, y + dy);
        n--;
    }
}

void glyph_draw(int x, int y, uint8_t ch, int scale, int opaque)
{
    const uint8_t *g = glyph(ch);
    for (int r = 0; r < 14; r++) {
        uint8_t bits = g[r];
        for (int s = 0; s < scale; s++) {
            int yy = y + r * scale + s;
            int k = 0;
            while (k < 8) {                 /* runs of set and clear bits */
                int on = (bits >> (7 - k)) & 1, e = k;
                while (e + 1 < 8 && ((bits >> (6 - e)) & 1) == on)
                    e++;
                if (on || opaque) {
                    USE(on ? paint_line : paint_back);
                    span(x + k * scale, x + (e + 1) * scale - 1, yy);
                }
                k = e + 1;
            }
        }
    }
}

/* ---- previews ---- */
void preview_begin(void)
{
    pv_on = 1;
    pv_over = 0;
    pv_n = 0;
    pv_x0 = DW; pv_y0 = DH; pv_x1 = -1; pv_y1 = -1;
}

void preview_erase(void)
{
    if (!pv_on)
        return;
    pv_on = 0;
    if (pv_x1 < 0)
        goto done;
    if (pv_over) {                          /* from BASE */
        int b0 = pv_x0 >> 1, n = (pv_x1 >> 1) - b0 + 1;
        for (int y = pv_y0; y <= pv_y1; y++)
            fastcopy(DOC + rowoff[y] + (unsigned)b0, BASE + rowoff[y] + (unsigned)b0, (unsigned)n);
    } else {
        for (unsigned i = pv_n; i-- > 0;) {
            uint32_t e = PV_LIST[i];
            DOC[e >> 8] = (uint8_t)e;
        }
    }
    mark(pv_x0, pv_y0, pv_x1, pv_y1);
done:
    preview_begin();
}

void preview_end(void) { pv_on = 0; }

/* memcpy by 16-bit words whenever the two are aligned alike (memcpy itself
 * needs both even) */
void fastcopy(uint8_t *d, const uint8_t *s, unsigned n)
{
    if (((uintptr_t)d ^ (uintptr_t)s) & 1) {
        while (n >= 4) {
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
            d += 4; s += 4; n -= 4;
        }
        while (n--)
            *d++ = *s++;
        return;
    }
    if (n && ((uintptr_t)d & 1)) {
        *d++ = *s++;
        n--;
    }
    uint16_t *dw = (uint16_t *)d;
    const uint16_t *sw = (const uint16_t *)s;
    unsigned w = n >> 1;
    while (w >= 4) {
        dw[0] = sw[0]; dw[1] = sw[1]; dw[2] = sw[2]; dw[3] = sw[3];
        dw += 4; sw += 4; w -= 4;
    }
    while (w--)
        *dw++ = *sw++;
    if (n & 1)
        *(uint8_t *)dw = *(const uint8_t *)sw;
}

/* ---- pixel rectangles ---- */
/* n pixels of a row from src (at sx) to dst (at dx); tr: a colour not copied */
void row_copy(uint8_t *dst, int dx, const uint8_t *src, int sx, int n, int tr)
{
    if (n <= 0)
        return;
    if (tr < 0 && (dx & 1) == (sx & 1)) {   /* the same alignment: whole bytes */
        if (dx & 1) {
            uint8_t *p = dst + (dx >> 1);
            *p = (uint8_t)((*p & 0xF0) | (src[sx >> 1] & 0x0F));
            dx++; sx++; n--;
        }
        fastcopy(dst + (dx >> 1), src + (sx >> 1), (unsigned)(n >> 1));
        if (n & 1) {
            int k = n - 1;
            uint8_t *p = dst + ((dx + k) >> 1);
            *p = (uint8_t)((*p & 0x0F) | (src[(sx + k) >> 1] & 0xF0));
        }
        return;
    }
    if (tr < 0) {                           /* half a byte apart: shifted bytes */
        if (dx & 1) {
            uint8_t *p = dst + (dx >> 1);
            *p = (uint8_t)((*p & 0xF0) | (src[sx >> 1] >> 4));     /* (sx even) */
            dx++; sx++; n--;
        }
        uint8_t *p = dst + (dx >> 1);       /* dx even, sx odd */
        const uint8_t *q = src + (sx >> 1);
        for (int i = n >> 1; i > 0; i--, q++)
            *p++ = (uint8_t)((q[0] << 4) | (q[1] >> 4));
        if (n & 1)
            *p = (uint8_t)((*p & 0x0F) | (q[0] << 4));
        return;
    }
    for (; n > 0; n--, sx++, dx++) {
        uint8_t v = src[sx >> 1];
        uint8_t c = (sx & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
        if (c == tr)
            continue;
        uint8_t *p = dst + (dx >> 1);
        *p = (dx & 1) ? (uint8_t)((*p & 0xF0) | c) : (uint8_t)((*p & 0x0F) | c << 4);
    }
}

void px_copy(uint8_t *dst, int dx, int dy, const uint8_t *src, int sx, int sy, int w, int h, int tr)
{
    /* clip to both bitmaps */
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (sx + w > DW) w = DW - sx;
    if (dx + w > DW) w = DW - dx;
    if (sy + h > DH) h = DH - sy;
    if (dy + h > DH) h = DH - dy;
    if (w <= 0 || h <= 0)
        return;
    for (int y = 0; y < h; y++)
        row_copy(dst + rowoff[dy + y], dx, src + rowoff[sy + y], sx, w, tr);
}

void px_fill(uint8_t *dst, int x, int y, int w, int h, uint8_t c)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            setpx(dst, x + i, y + j, c);
}

void px_flip_h(uint8_t *b, int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w / 2; i++) {
            uint8_t a = getpx(b, x + i, y + j), c = getpx(b, x + w - 1 - i, y + j);
            setpx(b, x + i, y + j, c);
            setpx(b, x + w - 1 - i, y + j, a);
        }
}

void px_flip_v(uint8_t *b, int x, int y, int w, int h)
{
    for (int j = 0; j < h / 2; j++)
        for (int i = 0; i < w; i++) {
            uint8_t a = getpx(b, x + i, y + j), c = getpx(b, x + i, y + h - 1 - j);
            setpx(b, x + i, y + j, c);
            setpx(b, x + i, y + h - 1 - j, a);
        }
}

void px_map(uint8_t *b, int x, int y, int w, int h, const uint8_t *map)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            setpx(b, x + i, y + j, map[getpx(b, x + i, y + j)]);
}

void buf_copy(uint8_t *dst, const uint8_t *src)
{
    /* in halves: memcpy's count is 16 bits */
    fastcopy(dst, src, (unsigned)(PICSZ / 2));
    fastcopy(dst + PICSZ / 2, src + PICSZ / 2, (unsigned)(PICSZ / 2));
}

void buf_fill(uint8_t *dst, uint8_t c)
{
    memset(dst, c * 0x11, (unsigned)(PICSZ / 2));
    memset(dst + PICSZ / 2, c * 0x11, (unsigned)(PICSZ / 2));
}

/* ---- operations and the undo log ----
 * A record is a rectangle of whole bytes (b0..b1, y0..y1) in the log at
 * off: before the operation was done, the picture's pixels before it;
 * after it is undone, the pixels after it. Undo and redo swap. */
typedef struct { uint32_t off, size; int16_t b0, b1, y0, y1; } rec_t;
#define NREC 96
static rec_t recs[NREC];
static uint8_t r_first, r_n, r_cur;     /* oldest record, records kept, records done */
#define LOG ((uint8_t *)LOG_A)

static rec_t *R(int i) { return &recs[(r_first + i) % NREC]; }

static void drop_oldest(void)
{
    r_first = (uint8_t)((r_first + 1) % NREC);
    r_n--;
    if (r_cur)
        r_cur--;
}

static uint32_t log_alloc(uint32_t size)
{
    if (r_n == NREC)
        drop_oldest();
    for (;;) {
        if (r_n == 0)
            return 0;
        rec_t *o = R(0), *t = R(r_n - 1);
        uint32_t head = o->off, tail = t->off + t->size;
        if (head <= t->off) {               /* not wrapped: free at the end and at the start */
            if (tail + size <= LOG_SZ)
                return tail;
            if (size <= head)
                return 0;
        } else if (tail + size <= head)     /* wrapped: free between */
            return tail;
        drop_oldest();
    }
}

void undo_reset(void) { r_first = r_n = r_cur = 0; }

void base_sync(void) { buf_copy(BASE, DOC); }

void op_begin(void)
{
    op_active = 1;
    op_x0 = DW; op_y0 = DH; op_x1 = -1; op_y1 = -1;
}

void op_end(void)
{
    preview_end();
    op_active = 0;
    if (op_x1 < 0)
        return;
    int b0 = op_x0 >> 1, b1 = op_x1 >> 1, n = b1 - b0 + 1;
    uint32_t size = (uint32_t)n * (uint32_t)(op_y1 - op_y0 + 1);
    r_n = r_cur;                            /* what could be redone is gone */
    uint32_t off = log_alloc(size);
    rec_t *r = R(r_n);
    r->off = off; r->size = size;
    r->b0 = (int16_t)b0; r->b1 = (int16_t)b1; r->y0 = (int16_t)op_y0; r->y1 = (int16_t)op_y1;
    r_n++;
    r_cur = r_n;
    uint8_t *p = LOG + off;
    for (int y = op_y0; y <= op_y1; y++, p += n) {
        fastcopy(p, BASE + rowoff[y] + (unsigned)b0, (unsigned)n);
        fastcopy(BASE + rowoff[y] + (unsigned)b0, DOC + rowoff[y] + (unsigned)b0, (unsigned)n);
    }
}

void op_cancel(void)
{
    preview_end();
    op_active = 0;
    if (op_x1 < 0)
        return;
    int b0 = op_x0 >> 1, n = (op_x1 >> 1) - b0 + 1;
    for (int y = op_y0; y <= op_y1; y++)
        fastcopy(DOC + rowoff[y] + (unsigned)b0, BASE + rowoff[y] + (unsigned)b0, (unsigned)n);
    mark(op_x0, op_y0, op_x1, op_y1);
}

static void rec_swap(const rec_t *r)
{
    int n = r->b1 - r->b0 + 1;
    uint8_t *p = LOG + r->off;
    for (int y = r->y0; y <= r->y1; y++, p += n) {
        uint8_t *d = DOC + rowoff[y] + (unsigned)r->b0, *b = BASE + rowoff[y] + (unsigned)r->b0;
        for (int i = 0; i < n; i++) {
            uint8_t t = d[i];
            d[i] = b[i] = p[i];
            p[i] = t;
        }
    }
    mark(2 * r->b0, r->y0, 2 * r->b1 + 1, r->y1);
}

int can_undo(void) { return r_cur > 0; }
int can_redo(void) { return r_cur < r_n; }

int undo(void)
{
    if (!r_cur)
        return 0;
    r_cur--;
    rec_swap(R(r_cur));
    return 1;
}

int redo(void)
{
    if (r_cur >= r_n)
        return 0;
    rec_swap(R(r_cur));
    r_cur++;
    return 1;
}
