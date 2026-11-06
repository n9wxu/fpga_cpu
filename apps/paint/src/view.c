/* view.c: the picture on the screen at 1x-8x, the pointer, the selection's
 * outline and the text caret (paint.h). */
#include "paint.h"
#include <string.h>

int vx, vy, vw, vh;
int zoom = 1, ox, oy;
int cur_view;
uint8_t grid = 1;
uint8_t full_view;
uint8_t fl_on, fl_trans, fl_bg;
int fl_x, fl_y, fl_w, fl_h;

/* the overlays as drawn in the current view */
static uint8_t mq_drawn, ca_drawn;
static int md_x0, md_y0, md_x1, md_y1;     /* where the marquee was drawn: screen */
static int cd_x, cd_y, cd_w, cd_h;         /* and the caret */
static int pan_rx, pan_ry;                 /* screen pixels not yet a picture pixel */

/* every view's state; the current one is in the globals above */
typedef struct {
    int vx, vy, vw, vh, zoom, ox, oy, pan_rx, pan_ry;
    uint8_t mq_drawn, ca_drawn;
    int md_x0, md_y0, md_x1, md_y1, cd_x, cd_y, cd_w, cd_h;
} view_t;
static view_t views[NVIEW];

static void overlays_view(int redraw);

static void view_store(void)
{
    view_t *v = &views[cur_view];
    v->vx = vx; v->vy = vy; v->vw = vw; v->vh = vh; v->zoom = zoom; v->ox = ox; v->oy = oy;
    v->pan_rx = pan_rx; v->pan_ry = pan_ry;
    v->mq_drawn = mq_drawn; v->ca_drawn = ca_drawn;
    v->md_x0 = md_x0; v->md_y0 = md_y0; v->md_x1 = md_x1; v->md_y1 = md_y1;
    v->cd_x = cd_x; v->cd_y = cd_y; v->cd_w = cd_w; v->cd_h = cd_h;
}

static void view_load(int n)
{
    view_t *v = &views[n];
    cur_view = n;
    vx = v->vx; vy = v->vy; vw = v->vw; vh = v->vh; zoom = v->zoom; ox = v->ox; oy = v->oy;
    pan_rx = v->pan_rx; pan_ry = v->pan_ry;
    mq_drawn = v->mq_drawn; ca_drawn = v->ca_drawn;
    md_x0 = v->md_x0; md_y0 = v->md_y0; md_x1 = v->md_x1; md_y1 = v->md_y1;
    cd_x = v->cd_x; cd_y = v->cd_y; cd_w = v->cd_w; cd_h = v->cd_h;
}

void view_use(int v)
{
    if (v == cur_view || full_view || v < 0)
        return;
    view_store();
    view_load(v);
}

int view_zoom_of(int v) { return v == cur_view && !full_view ? zoom : views[v].zoom; }

/* body for every open view, as the current one (in full view: the one) */
#define EACH_VIEW(body) do { \
    if (full_view) { body; } \
    else { int sv_ = cur_view; \
        for (int v_ = 0; v_ < NVIEW; v_++) if (wins[WIN_PIC0 + v_].open) { view_use(v_); body; } \
        view_use(sv_); } } while (0)

static void geometry(int v)
{
    int i = WIN_PIC0 + v;
    vx = WIN_CX(i); vy = WIN_CY(i);
    vw = WIN_CW(i) - SB; vh = WIN_CH(i) - SB;
}

void view_init(int v, int z)
{
    int save = cur_view;
    if (v != cur_view) {
        view_store();
        cur_view = v;
    }
    geometry(v);
    zoom = z;
    ox = DW / 2 - vw / z / 2;
    oy = DH / 2 - vh / z / 2;
    pan_rx = pan_ry = 0;
    view_clamp();
    view_store();
    view_load(save);
}

void view_fit(int v)
{
    int save = cur_view;
    view_use(v);
    geometry(v);
    view_clamp();
    view_use(save);
}

void view_moved(int v, int dx, int dy)
{
    int save = cur_view;
    view_use(v);
    geometry(v);
    md_x0 += dx; md_x1 += dx; md_y0 += dy; md_y1 += dy;   /* the overlays moved with the pixels */
    cd_x += dx; cd_y += dy;
    view_use(save);
}

/* the window was sized: draw what is new in the view, and the bars */
void view_grown(int v, int ow, int oh)
{
    int save = cur_view;
    view_use(v);
    int ox0 = ox, oy0 = oy;
    geometry(v);
    view_clamp();
    if (ox != ox0 || oy != oy0)
        render_view();
    else {
        if (vw > ow) render(vx + ow, vy, vx + vw - 1, vy + vh - 1);
        if (vh > oh) render(vx, vy + oh, vx + (ow < vw ? ow : vw) - 1, vy + vh - 1);
        overlays_view(1);
        view_draw_bars();
    }
    view_use(save);
}

void view_clamp(void)
{
    int mx = DW - vw / zoom, my = DH - vh / zoom;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (ox > mx) ox = mx;
    if (oy > my) oy = my;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    if (zoom == 1)
        ox &= ~3;                           /* whole 16-bit words on the screen */
}

static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int to_doc_x(int sx) { return ox + fdiv(sx - vx, zoom); }
int to_doc_y(int sy) { return oy + fdiv(sy - vy, zoom); }
int to_scr_x(int dx) { return vx + (dx - ox) * zoom; }
int to_scr_y(int dy) { return vy + (dy - oy) * zoom; }
int in_view(int sx, int sy) { return sx >= vx && sx < vx + vw && sy >= vy && sy < vy + vh; }

/* where the current view shows on the screen */
static const region_t *vreg(void) { return full_view ? &r_screen : win_region(WIN_PIC0 + cur_view); }

int view_at(int sx, int sy)
{
    if (full_view)
        return cur_view;
    int part, i = win_at(sx, sy, &part);
    if (i < WIN_PIC0 || part != PART_CONTENT)
        return -1;
    return i - WIN_PIC0;
}

static void view_changed(void)
{
    view_draw_bars();
    if (!full_view)
        win_frame(WIN_PIC0 + cur_view);    /* (the zoom in the title) */
}

void view_zoom(int z, int sx, int sy)
{
    if (z < 1) z = 1;
    if (z > 8) z = 8;
    if (z == zoom)
        return;
    if (!in_view(sx, sy)) {
        sx = vx + vw / 2;
        sy = vy + vh / 2;
    }
    int dx = to_doc_x(sx), dy = to_doc_y(sy);
    zoom = z;
    ox = dx - (sx - vx) / z;
    oy = dy - (sy - vy) / z;
    view_clamp();
    render_view();
    if (!full_view)
        win_frame(WIN_PIC0 + cur_view);
}

static void overlays_moved(void);

/* the whole view shows (nothing over it) */
static int unobscured(void)
{
    const region_t *g = vreg();
    for (int k = 0; k < g->n; k++)
        if (g->r[k].x0 <= vx && g->r[k].y0 <= vy && g->r[k].x1 >= vx + vw - 1 && g->r[k].y1 >= vy + vh - 1)
            return 1;
    return 0;
}

/* the view's picture moved by sx (even), sy screen pixels: move what is on
 * the screen and draw only what comes into view */
static void scroll_view(int sx, int sy)
{
    int ax = sx < 0 ? -sx : sx, ay = sy < 0 ? -sy : sy;
    if (ax >= vw || ay >= vh || (sx & 1) || !unobscured()) {
        render_view();
        return;
    }
    damage(vx, vy, vx + vw - 1, vy + vh - 1);
    int nb = (vw - ax) >> 1;
    int db = (vx + (sx > 0 ? sx : 0)) >> 1, sb = (vx + (sx < 0 ? -sx : 0)) >> 1;
    if (sy > 0)
        for (int r = vy + vh - 1; r >= vy + sy; r--)
            fastcopy(SCREEN + rowoff[r] + (unsigned)db, SCREEN + rowoff[r - sy] + (unsigned)sb, (unsigned)nb);
    else if (sy < 0)
        for (int r = vy; r < vy + vh + sy; r++)
            fastcopy(SCREEN + rowoff[r] + (unsigned)db, SCREEN + rowoff[r - sy] + (unsigned)sb, (unsigned)nb);
    else if (sx > 0)                        /* along the row, overlapping: from the end */
        for (int r = vy; r < vy + vh; r++) {
            uint8_t *d = SCREEN + rowoff[r] + (unsigned)db + (unsigned)nb;
            const uint8_t *q = SCREEN + rowoff[r] + (unsigned)sb + (unsigned)nb;
            for (int i = nb; i > 0; i--)
                *--d = *--q;
        }
    else
        for (int r = vy; r < vy + vh; r++)
            fastcopy(SCREEN + rowoff[r] + (unsigned)db, SCREEN + rowoff[r] + (unsigned)sb, (unsigned)nb);
    if (sy > 0) render(vx, vy, vx + vw - 1, vy + sy - 1);
    if (sy < 0) render(vx, vy + vh + sy, vx + vw - 1, vy + vh - 1);
    if (sx > 0) render(vx, vy, vx + sx - 1, vy + vh - 1);
    if (sx < 0) render(vx + vw + sx, vy, vx + vw - 1, vy + vh - 1);
    overlays_moved();
    view_draw_bars();
}

void view_scroll_to(int nox, int noy)
{
    int ox0 = ox, oy0 = oy;
    ox = nox;
    oy = noy;
    view_clamp();
    if (ox != ox0 || oy != oy0)
        scroll_view((ox0 - ox) * zoom, (oy0 - oy) * zoom);
}

void view_pan(int dx, int dy)
{
    pan_rx += dx;
    pan_ry += dy;
    int step = zoom < 4 ? 4 : zoom;         /* (screen moves of whole 16-bit words) */
    int px = pan_rx / step, py = pan_ry / zoom;
    if (!px && !py)
        return;
    pan_rx -= px * step;
    pan_ry -= py * zoom;
    view_scroll_to(ox - px * (step / zoom), oy - py);
}

void view_full(int on)
{
    if (on == full_view)
        return;
    cursor_hide();
    if (on) {
        view_store();
        full_view = 1;
        vx = 0; vy = 0; vw = DW; vh = DH;
        zoom = 1; ox = 0; oy = 0;
        pan_rx = pan_ry = 0;
        render_view();
    } else {
        full_view = 0;
        view_load(cur_view);
        win_draw_all();
    }
}

/* ---- the scroll bars ---- */
/* a bar's thumb: its offset and length along a track of len pixels */
static void thumb(int len, int vis, int total, int off, int maxoff, int *p, int *t)
{
    if (vis >= total || maxoff <= 0) {
        *p = 0;
        *t = len;
        return;
    }
    int32_t tl = (int32_t)len * vis / total;
    if (tl < 10) tl = 10;
    if (tl > len) tl = len;
    *t = (int)tl;
    *p = (int)((int32_t)(len - tl) * off / maxoff);
}

static int max_ox(void) { int m = DW - vw / zoom; return m < 0 ? 0 : m; }
static int max_oy(void) { int m = DH - vh / zoom; return m < 0 ? 0 : m; }

/* the thumb of the current view's vertical (or horizontal) bar, on the screen */
void view_thumb(int vertical, int *p, int *t, int *start, int *len)
{
    if (vertical) {
        *start = vy;
        *len = vh;
        thumb(vh, vh, DH * zoom, oy, max_oy(), p, t);
    } else {
        *start = vx;
        *len = vw;
        thumb(vw, vw, DW * zoom, ox, max_ox(), p, t);
    }
    *p += *start;
}

/* the thumb dragged to screen position p */
void view_thumb_to(int vertical, int p)
{
    int tp, t, start, len;
    view_thumb(vertical, &tp, &t, &start, &len);
    int room = len - t;
    if (room <= 0)
        return;
    int32_t off = (int32_t)(p - start);
    if (off < 0) off = 0;
    if (off > room) off = room;
    if (vertical)
        view_scroll_to(ox, (int)(off * max_oy() / room));
    else
        view_scroll_to((int)(off * max_ox() / room) & (zoom < 4 ? ~(4 / zoom - 1) : ~0), oy);
}

void view_draw_bars(void)
{
    if (full_view)
        return;
    clip_win(WIN_PIC0 + cur_view);
    int bx = vx + vw, by = vy + vh, ex = bx + SB - 1, ey = by + SB - 1;
    int p, t, st0, len;
    sfill(bx, vy, bx, by, BLACK);
    sfill(vx, by, bx, by, BLACK);
    view_thumb(1, &p, &t, &st0, &len);
    sfill(bx + 1, vy, ex, by - 1, GREY);
    if (t < len) {
        sfill(bx + 1, p, ex, p + t - 1, WHITE);
        sframe(bx + 1, p, ex, p + t - 1, BLACK);
    } else
        sfill(bx + 1, vy, ex, by - 1, WHITE);
    view_thumb(0, &p, &t, &st0, &len);
    sfill(vx, by + 1, bx - 1, ey, GREY);
    if (t < len) {
        sfill(p, by + 1, p + t - 1, ey, WHITE);
        sframe(p, by + 1, p + t - 1, ey, BLACK);
    } else
        sfill(vx, by + 1, bx - 1, ey, WHITE);
    sfill(bx + 1, by + 1, ex, ey, WHITE);   /* the grow box */
    sframe(bx + 2, by + 2, bx + 6, by + 6, BLACK);
    sframe(bx + 4, by + 4, ex - 1, ey - 1, BLACK);
}

void spx(int x, int y, uint8_t c)
{
    if ((unsigned)x < DW && (unsigned)y < DH && in_clip(x, y)) {
        damage(x, y, x, y);
        setpx(SCREEN, x, y, c);
    }
}

/* a pixel of the pointer: on top of everything */
static void rpx(int x, int y, uint8_t c)
{
    if ((unsigned)x < DW && (unsigned)y < DH)
        setpx(SCREEN, x, y, c);
}

/* ---- rendering ---- */
static uint16_t linebuf[STRIDE / 2];       /* (16-bit aligned: copied a word at a time) */
#define line ((uint8_t *)linebuf)

static const uint8_t dup[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

/* screen bytes bx0..bx1 of a row showing picture row dy (zoom > 1) */
static void build_zoomed(int dy, int bx0, int bx1)
{
    int z = zoom, zb = z >> 1;
    int rx = 2 * bx0 - vx;                  /* even */
    int dx = ox + rx / z;
    int left = (z - rx % z) >> 1;           /* bytes left in the first pixel's block */
    int flrow = fl_on && dy >= fl_y && dy < fl_y + fl_h;
    const uint8_t *row = dy < DH ? DOC + rowoff[dy] : 0;
    const uint8_t *frow = flrow ? FLOATB + rowoff[dy - fl_y] : 0;
    uint8_t g = grid && z >= 8;
    uint8_t *p = line + bx0, *pe = line + bx1 + 1;
    for (;;) {
        /* whole picture bytes at a time: no selection on the row, two whole blocks */
        if (!flrow && row && !(dx & 1) && left == zb) {
            int nb = (int)(pe - p) / z, lim = (DW - dx) >> 1;
            if (nb > lim)
                nb = lim;
            const uint8_t *q = row + (dx >> 1);
            if (z == 2)
                for (int i = 0; i < nb; i++, q++) {
                    p[0] = dup[*q >> 4];
                    p[1] = dup[*q & 15];
                    p += 2;
                }
            else if (z == 4)
                for (int i = 0; i < nb; i++, q++) {
                    p[0] = p[1] = dup[*q >> 4];
                    p[2] = p[3] = dup[*q & 15];
                    p += 4;
                }
            else
                for (int i = 0; i < nb; i++, q++) {
                    uint8_t a = dup[*q >> 4], b = dup[*q & 15];
                    p[0] = p[1] = p[2] = a;
                    p[4] = p[5] = p[6] = b;
                    p[3] = g ? (uint8_t)((a & 0xF0) | (*q >> 4 == GREY ? BLACK : GREY)) : a;
                    p[7] = g ? (uint8_t)((b & 0xF0) | ((*q & 15) == GREY ? BLACK : GREY)) : b;
                    p += 8;
                }
            dx += 2 * nb;
            if (p >= pe)
                break;
        }
        uint8_t c = GREY;
        if (row && dx < DW) {
            uint8_t v = row[dx >> 1];
            c = (dx & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
            if (flrow && (unsigned)(dx - fl_x) < (unsigned)fl_w) {
                int fx = dx - fl_x;
                uint8_t fv = frow[fx >> 1];
                uint8_t f = (fx & 1) ? (uint8_t)(fv & 15) : (uint8_t)(fv >> 4);
                if (!fl_trans || f != fl_bg)
                    c = f;
            }
        }
        uint8_t b = dup[c];
        int k = left;
        if (k > (int)(pe - p))
            k = (int)(pe - p);
        for (int i = 0; i < k; i++)
            p[i] = b;
        p += k;
        if (g && k == left)                 /* the block's last column: the grid */
            p[-1] = (uint8_t)((c << 4) | (c == GREY ? BLACK : GREY));
        if (p >= pe)
            break;
        dx++;
        left = zb;
    }
}

static void render_raw(int sx0, int sy0, int sx1, int sy1);

void render(int sx0, int sy0, int sx1, int sy1)
{
    if (sx0 < vx) sx0 = vx;
    if (sy0 < vy) sy0 = vy;
    if (sx1 > vx + vw - 1) sx1 = vx + vw - 1;
    if (sy1 > vy + vh - 1) sy1 = vy + vh - 1;
    if (sx0 > sx1 || sy0 > sy1)
        return;
    const region_t *g = vreg();
    for (int k = 0; k < g->n; k++) {
        const rect_t *r = &g->r[k];
        int a = sx0 > r->x0 ? sx0 : r->x0, b = sx1 < r->x1 ? sx1 : r->x1;
        int c = sy0 > r->y0 ? sy0 : r->y0, d = sy1 < r->y1 ? sy1 : r->y1;
        if (a > b || c > d)
            continue;
        if ((a & 1) || !(b & 1)) {          /* half a byte at an end: keep the other half */
            uint8_t keep_l[DH], keep_r[DH];
            for (int y = c; y <= d; y++) {
                keep_l[y] = SCREEN[rowoff[y] + (unsigned)(a >> 1)];
                keep_r[y] = SCREEN[rowoff[y] + (unsigned)(b >> 1)];
            }
            render_raw(a, c, b, d);
            for (int y = c; y <= d; y++) {
                uint8_t *p = SCREEN + rowoff[y];
                if (a & 1) p[a >> 1] = (uint8_t)((keep_l[y] & 0xF0) | (p[a >> 1] & 0x0F));
                if (!(b & 1)) p[b >> 1] = (uint8_t)((keep_r[y] & 0x0F) | (p[b >> 1] & 0xF0));
            }
        } else
            render_raw(a, c, b, d);
    }
}

static void render_raw(int sx0, int sy0, int sx1, int sy1)
{
    damage(sx0, sy0, sx1, sy1);
    int bx0 = sx0 >> 1, bx1 = sx1 >> 1, n = bx1 - bx0 + 1;
    uint8_t *srow = SCREEN + rowoff[sy0];
    if (zoom == 1) {
        int dy = oy + (sy0 - vy);
        int d0 = ox + (2 * bx0 - vx), d1 = d0 + 2 * n - 1;     /* picture columns */
        const uint8_t *drow = DOC + rowoff[dy < DH ? dy : 0] + (unsigned)(d0 >> 1);
        for (int sy = sy0; sy <= sy1; sy++, dy++, srow += STRIDE, drow += STRIDE) {
            if (dy >= DH) {
                memset(srow + bx0, GREY * 0x11, (unsigned)n);
                continue;
            }
            fastcopy(srow + bx0, drow, (unsigned)n);
            if (fl_on && dy >= fl_y && dy < fl_y + fl_h) {
                int a = fl_x, b = fl_x + fl_w - 1;
                if (a < d0) a = d0;
                if (b > d1) b = d1;
                if (a <= b)
                    row_copy(srow, vx + a - ox, FLOATB + rowoff[dy - fl_y], a - fl_x, b - a + 1,
                             fl_trans ? fl_bg : -1);
            }
        }
        return;
    }
    int last = -1, ry = sy0 - vy, dy = oy + ry / zoom, k = ry % zoom;
    for (int sy = sy0; sy <= sy1; sy++, srow += STRIDE) {
        if (grid && zoom >= 8 && k == zoom - 1)
            memset(srow + bx0, GREY * 0x11, (unsigned)n);
        else {
            if (dy != last) {
                build_zoomed(dy, bx0, bx1);
                last = dy;
            }
            fastcopy(srow + bx0, line + bx0, (unsigned)n);
        }
        if (++k == zoom) {
            k = 0;
            dy++;
        }
    }
}

void render_doc(int x0, int y0, int x1, int y1)
{
    render(to_scr_x(x0), to_scr_y(y0), to_scr_x(x1 + 1) - 1, to_scr_y(y1 + 1) - 1);
}


/* the current view, all of it */
void render_view(void)
{
    render(vx, vy, vx + vw - 1, vy + vh - 1);
    overlays_view(1);
    view_draw_bars();
}

void flush_dirty(void)
{
    if (dirty_x1 < 0)
        return;
    EACH_VIEW(render_doc(dirty_x0, dirty_y0, dirty_x1, dirty_y1));
    dirty_x0 = DW; dirty_y0 = DH; dirty_x1 = -1; dirty_y1 = -1;
}

/* ---- the pointer: 16x16, '#' black, '.' white; its hot spot ---- */
typedef struct { uint8_t hx, hy; const char *art[16]; } cursor_t;
static const cursor_t cursors[] = {
    { 0, 0, {                           /* arrow */
        "#               ", "##              ", "#.#             ", "#..#            ",
        "#...#           ", "#....#          ", "#.....#         ", "#......#        ",
        "#.......#       ", "#........#      ", "#.....#####     ", "#..#..#         ",
        "#.# #..#        ", "##  #..#        ", "#    #..#       ", "     ####       " } },
    { 7, 7, {                           /* cross */
        "      ...       ", "      .#.       ", "      .#.       ", "      .#.       ",
        "      .#.       ", "      .#.       ", "...... # .......", ".##### . #####.",
        "...... # .......", "      .#.       ", "      .#.       ", "      .#.       ",
        "      .#.       ", "      .#.       ", "      ...       ", "                " } },
    { 4, 8, {                           /* I-beam */
        ".### ###.       ", " ..#.#..        ", "   .#.          ", "   .#.          ",
        "   .#.          ", "   .#.          ", "   .#.          ", "   .#.          ",
        "   .#.          ", "   .#.          ", "   .#.          ", "   .#.          ",
        "   .#.          ", " ..#.#..        ", ".### ###.       ", "                " } },
    { 8, 8, {                           /* hand */
        "      ##        ", "   ## #..###    ", "  #..##..#..#   ", "  #..##..#..# # ",
        "   #..#..#..##.#", "   #..#..#..#..#", " ## #.......#..#", "#..##..........#",
        "#...#.........# ", " #............# ", "  #...........# ", "  #..........#  ",
        "   #.........#  ", "    #.......#   ", "     #......#   ", "     #......#   " } },
    { 6, 6, {                           /* magnifier */
        "   #####        ", "  #.....#       ", " #.......#      ", "#....#....#     ",
        "#....#....#     ", "#..#####..#     ", "#....#....#     ", "#....#....#     ",
        " #.......#      ", "  #.....###     ", "   #####.###    ", "         .###   ",
        "          .###  ", "           .###", "            .## ", "                " } },
    { 1, 14, {                          /* dropper */
        "           ###  ", "          #####.", "          #####.", "        #######.",
        "         ####.  ", "        #..##.  ", "       #...#.#  ", "      #...#.    ",
        "     #...#.     ", "    #...#.      ", "   #...#.       ", "  #...#.        ",
        " #...#.         ", " #..#.          ", "# ##.           ", ".#..            " } },
    { 14, 13, {                         /* bucket */
        "     ##         ", "    #..#        ", "    #..##       ", "    #.#..#      ",
        "    ##.#..#     ", "    #..#...#    ", "   #...#....#   ", "  #....#.....#  ",
        " #.....#......# ", "#.............##", " #...........#.#", "  #.........#..#",
        "   #.......#...#", "    #.....#  ..#", "     #...#    .#", "      ###      #" } },
};

/* ---- the two screens ---- */
uint8_t *screen_back = (uint8_t *)SCREEN0_A;
static uint8_t *const scr[2] = { (uint8_t *)SCREEN0_A, (uint8_t *)SCREEN1_A };
static uint8_t back;                        /* which is drawn into */

/* damage: rectangles drawn this frame (cur) and in the last (prev, on the other screen) */
#define NDMG 16
static rect_t dmg[2][NDMG];
static uint8_t ndmg[2], dcur;

void damage(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > DW - 1) x1 = DW - 1;
    if (y1 > DH - 1) y1 = DH - 1;
    if (x0 > x1 || y0 > y1)
        return;
    rect_t *d = dmg[dcur];
    int n = ndmg[dcur];
    for (int k = 0; k < n; k++) {           /* touching one already there: one rectangle */
        rect_t *r = &d[k];
        if (x0 <= r->x1 + 8 && x1 >= r->x0 - 8 && y0 <= r->y1 + 1 && y1 >= r->y0 - 1) {
            if (x0 < r->x0) r->x0 = (int16_t)x0;
            if (y0 < r->y0) r->y0 = (int16_t)y0;
            if (x1 > r->x1) r->x1 = (int16_t)x1;
            if (y1 > r->y1) r->y1 = (int16_t)y1;
            return;
        }
    }
    if (n == NDMG) {                        /* full: into the last */
        rect_t *r = &d[n - 1];
        if (x0 < r->x0) r->x0 = (int16_t)x0;
        if (y0 < r->y0) r->y0 = (int16_t)y0;
        if (x1 > r->x1) r->x1 = (int16_t)x1;
        if (y1 > r->y1) r->y1 = (int16_t)y1;
        return;
    }
    d[n].x0 = (int16_t)x0; d[n].y0 = (int16_t)y0; d[n].x1 = (int16_t)x1; d[n].y1 = (int16_t)y1;
    ndmg[dcur] = (uint8_t)(n + 1);
}

/* ---- the pointer, on each screen: 16x16, '#' black, '.' white; its hot spot ---- */
#define CS_W 34                             /* saved bytes a row */
#define CS_H 64
typedef struct { uint8_t on; int bx, y, nb, h; uint8_t buf[CS_W * CS_H]; } csave_t;
static csave_t cs[2];

static void cs_restore(uint8_t *to, const csave_t *c)
{
    for (int r = 0; r < c->h; r++)
        fastcopy(to + rowoff[c->y + r] + (unsigned)c->bx, c->buf + r * CS_W, (unsigned)c->nb);
}

static void cs_save(int x0, int y0, int w, int h)
{
    csave_t *c = &cs[back];
    if (x0 < 0) { w += x0; x0 = 0; }
    if (y0 < 0) { h += y0; y0 = 0; }
    if (x0 + w > DW) w = DW - x0;
    if (y0 + h > DH) h = DH - y0;
    c->on = 1;
    if (w <= 0 || h <= 0) {
        c->h = 0;
        return;
    }
    c->bx = x0 >> 1;
    c->nb = ((x0 + w - 1) >> 1) - c->bx + 1;
    c->y = y0;
    c->h = h;
    for (int r = 0; r < h; r++)
        fastcopy(c->buf + r * CS_W, SCREEN + rowoff[y0 + r] + (unsigned)c->bx, (unsigned)c->nb);
}

static void cursor_draw(int x, int y, int shape, int box)
{
    if (shape == CUR_BOX && (box < 3 || box > 2 * CS_W - 4))
        shape = CUR_CROSS;
    if (shape == CUR_BOX) {                 /* the eraser: a square, black outside white */
        int x0 = x - box / 2, y0 = y - box / 2;
        cs_save(x0, y0, box, box);
        for (int i = 0; i < box; i++) {
            rpx(x0 + i, y0, BLACK);
            rpx(x0 + i, y0 + box - 1, BLACK);
            rpx(x0, y0 + i, BLACK);
            rpx(x0 + box - 1, y0 + i, BLACK);
        }
        for (int i = 1; i < box - 1; i++) {
            rpx(x0 + i, y0 + 1, WHITE);
            rpx(x0 + i, y0 + box - 2, WHITE);
            rpx(x0 + 1, y0 + i, WHITE);
            rpx(x0 + box - 2, y0 + i, WHITE);
        }
        return;
    }
    if (shape == CUR_WAIT)
        shape = CUR_ARROW;
    const cursor_t *c = &cursors[shape];
    int x0 = x - c->hx, y0 = y - c->hy;
    cs_save(x0, y0, 16, 16);
    for (int r = 0; r < 16; r++) {
        const char *a = c->art[r];
        for (int k = 0; k < 16 && a[k]; k++)
            if (a[k] != ' ')
                rpx(x0 + k, y0 + r, a[k] == '#' ? BLACK : WHITE);
    }
}

void screen_init(void)
{
    buf_fill(scr[0], GREY);
    buf_fill(scr[1], GREY);
    back = 0;
    screen_back = scr[0];
    VID_BMP_HI = (uint16_t)(SCREEN1_A >> 16);
    VID_BMP_LO = (uint16_t)SCREEN1_A;
    ndmg[0] = ndmg[1] = 0;
    dcur = 0;
}

/* the back screen as the front one is, less its pointer */
void frame_begin(void)
{
    uint8_t *b = scr[back], *f = scr[back ^ 1];
    screen_back = b;
    if (cs[back].on && cs[back].h)
        cs_restore(b, &cs[back]);
    cs[back].on = 0;
    const rect_t *d = dmg[dcur ^ 1];
    for (int k = 0; k < ndmg[dcur ^ 1]; k++) {
        int b0 = d[k].x0 >> 1, nb = (d[k].x1 >> 1) - b0 + 1;
        for (int y = d[k].y0; y <= d[k].y1; y++)
            fastcopy(b + rowoff[y] + (unsigned)b0, f + rowoff[y] + (unsigned)b0, (unsigned)nb);
    }
    if (cs[back ^ 1].on && cs[back ^ 1].h)
        cs_restore(b, &cs[back ^ 1]);       /* what the front's pointer covers */
    ndmg[dcur] = 0;
}

/* the pointer on the back screen; at the start of the next vertical blank, show it */
void frame_end(int x, int y, int shape, int box)
{
    cursor_draw(x, y, shape, box);
    uint16_t f = FRAME;
    while (FRAME == f) ;
    uint32_t a = (uint32_t)(uintptr_t)scr[back];
    VID_BMP_HI = (uint16_t)(a >> 16);
    VID_BMP_LO = (uint16_t)a;
    back ^= 1;
    dcur ^= 1;
}

/* ---- overlays ---- */
static uint8_t mq_on, mq_phase;
static int mq_x0, mq_y0, mq_x1, mq_y1;     /* picture rectangle */
static uint8_t ca_on;
static int ca_x, ca_y, ca_h;

static void mq_erase(void)
{
    if (!mq_drawn)
        return;
    mq_drawn = 0;
    render(md_x0, md_y0, md_x1, md_y0);
    render(md_x0, md_y1, md_x1, md_y1);
    render(md_x0, md_y0, md_x0 + 1, md_y1);
    render(md_x1 - 1, md_y0, md_x1, md_y1);
}

static inline void ov_px(int x, int y, uint8_t c)
{
    if (in_clip(x, y)) {
        uint8_t *p = SCREEN + rowoff[y] + (x >> 1);
        *p = (x & 1) ? (uint8_t)((*p & 0xF0) | c) : (uint8_t)((*p & 0x0F) | c << 4);
    }
}

static void mq_draw(void)
{
    int x0 = to_scr_x(mq_x0) - 1, y0 = to_scr_y(mq_y0) - 1;
    int x1 = to_scr_x(mq_x1 + 1), y1 = to_scr_y(mq_y1 + 1);
    damage(x0, y0, x1, y0);
    damage(x0, y1, x1, y1);
    damage(x0, y0, x0, y1);
    damage(x1, y0, x1, y1);
    md_x0 = x0; md_y0 = y0; md_x1 = x1; md_y1 = y1;
    mq_drawn = 1;
    clip_and(vreg(), vx, vy, vx + vw - 1, vy + vh - 1);
    int ax = x0 < vx ? vx : x0, bx = x1 > vx + vw - 1 ? vx + vw - 1 : x1;
    int ay = y0 < vy ? vy : y0, by = y1 > vy + vh - 1 ? vy + vh - 1 : y1;
    /* dashes 4 long: ((x + y) / 4 + phase) odd is white */
    for (int e = 0; e < 2; e++) {           /* the top and bottom edges */
        int y = e ? y1 : y0;
        if (y < ay || y > by)
            continue;
        for (int x = ax; x <= bx; x++)
            ov_px(x, y, (uint8_t)(((((x + y) >> 2) + mq_phase) & 1) ? WHITE : BLACK));
    }
    for (int e = 0; e < 2; e++) {           /* the sides */
        int x = e ? x1 : x0;
        if (x < ax || x > bx)
            continue;
        for (int y = ay; y <= by; y++)
            ov_px(x, y, (uint8_t)(((((x + y) >> 2) + mq_phase) & 1) ? WHITE : BLACK));
    }
    clip_screen();
}

/* the screen scrolled with the picture: the overlays moved with it */
static void overlays_moved(void)
{
    overlays_view(1);
}

void overlays_forget(void) {}

void marquee_set(int on, int x0, int y0, int x1, int y1)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (on == mq_on && (!on || (x0 == mq_x0 && y0 == mq_y0 && x1 == mq_x1 && y1 == mq_y1)))
        return;
    EACH_VIEW(mq_erase());
    mq_on = (uint8_t)on;
    mq_x0 = x0; mq_y0 = y0; mq_x1 = x1; mq_y1 = y1;
}

static void ca_erase(void)
{
    if (ca_drawn) {
        render(cd_x, cd_y, cd_x + cd_w - 1, cd_y + cd_h - 1);
        ca_drawn = 0;
    }
}

void caret_set(int on, int x, int y, int h)
{
    if (on == ca_on && (!on || (x == ca_x && y == ca_y && h == ca_h)))
        return;
    EACH_VIEW(ca_erase());
    ca_on = (uint8_t)on;
    ca_x = x; ca_y = y; ca_h = h;
}

/* the current view's overlays; redraw: something was rendered under them */
static void overlays_view(int redraw)
{
    uint16_t f = FRAME;
    if (mq_on && (!mq_drawn || redraw))
        mq_draw();
    if (ca_on) {
        int vis = !((f >> 4) & 1);
        if (vis && (!ca_drawn || redraw)) {
            cd_x = to_scr_x(ca_x);
            cd_y = to_scr_y(ca_y);
            cd_w = zoom > 1 ? zoom / 2 : 1;
            cd_h = ca_h * zoom;
            damage(cd_x, cd_y, cd_x + cd_w - 1, cd_y + cd_h - 1);
            clip_and(vreg(), vx, vy, vx + vw - 1, vy + vh - 1);
            for (int j = 0; j < cd_h; j++) {
                int dy = ca_y + j / zoom;   /* the inverse of the picture under it */
                uint8_t c = (unsigned)dy < DH && (unsigned)ca_x < DW ? (uint8_t)(getpx(DOC, ca_x, dy) ^ 7) : BLACK;
                for (int i = 0; i < cd_w; i++)
                    ov_px(cd_x + i, cd_y + j, c);
            }
            clip_screen();
            ca_drawn = 1;
        } else if (!vis && ca_drawn)
            ca_erase();
    }
}

void overlays_update(int redraw)
{
    uint8_t ph = (uint8_t)((FRAME >> 3) & 1);
    if (mq_on && ph != mq_phase) {
        mq_phase = ph;
        redraw = 1;
    }
    EACH_VIEW(overlays_view(redraw));
}
