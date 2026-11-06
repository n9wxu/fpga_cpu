/* win.c: the windows on the desktop (paint.h).
 *
 * Windows are rectangles in a z-order; the two palettes (Tools, Colours)
 * always float above the picture windows. Each window's visible region is
 * its rectangle less the windows above it, kept as a list of rectangles;
 * everything drawn on the screen is clipped to the region of what is being
 * drawn (clip), so a window draws only what of it shows, in any order.
 *
 * A window's outer rectangle includes a one-pixel shadow on the right and
 * at the bottom; inside the frame is a 13-pixel title bar (striped when
 * active, with a close box) over the content. A picture window's content
 * is its view of the picture with scroll bars and a grow box. Picture
 * windows sit at x = 3 (mod 4) with widths a multiple of 4, so their views
 * start on a 16-bit word of the screen (view.c copies whole words). */
#include "paint.h"
#include <string.h>

win_t wins[NWIN];
static uint8_t zord[NWIN], nz;          /* the open windows, bottom to top */
static region_t r_win[NWIN], r_desk, r_tmp;
region_t r_screen = { 1, { { 0, 0, DW - 1, DH - 1 } } };
const region_t *clip = &r_screen;

static int is_palette(int i) { return i == WIN_TOOLS || i == WIN_COLOURS; }

/* ---- regions ---- */
static void reg_set(region_t *g, int x0, int y0, int x1, int y1)
{
    g->n = 0;
    if (x0 <= x1 && y0 <= y1) {
        g->r[0].x0 = (int16_t)x0; g->r[0].y0 = (int16_t)y0;
        g->r[0].x1 = (int16_t)x1; g->r[0].y1 = (int16_t)y1;
        g->n = 1;
    }
}

static void reg_add(region_t *g, int x0, int y0, int x1, int y1)
{
    if (x0 > x1 || y0 > y1 || g->n >= MAXCLIP)
        return;
    rect_t *r = &g->r[g->n++];
    r->x0 = (int16_t)x0; r->y0 = (int16_t)y0; r->x1 = (int16_t)x1; r->y1 = (int16_t)y1;
}

/* g less the rectangle */
static void reg_sub(region_t *g, int x0, int y0, int x1, int y1)
{
    region_t out;
    out.n = 0;
    for (int k = 0; k < g->n; k++) {
        rect_t r = g->r[k];
        if (x0 > r.x1 || x1 < r.x0 || y0 > r.y1 || y1 < r.y0) {
            reg_add(&out, r.x0, r.y0, r.x1, r.y1);
            continue;
        }
        int a = y0 > r.y0 ? y0 : r.y0, b = y1 < r.y1 ? y1 : r.y1;   /* the rows they share */
        reg_add(&out, r.x0, r.y0, r.x1, y0 - 1);      /* above */
        reg_add(&out, r.x0, y1 + 1, r.x1, r.y1);      /* below */
        reg_add(&out, r.x0, a, x0 - 1, b);            /* left */
        reg_add(&out, x1 + 1, a, r.x1, b);            /* right */
    }
    *g = out;
}

static void reg_and(region_t *out, const region_t *g, int x0, int y0, int x1, int y1)
{
    out->n = 0;
    for (int k = 0; k < g->n; k++) {
        const rect_t *r = &g->r[k];
        reg_add(out, r->x0 > x0 ? r->x0 : x0, r->y0 > y0 ? r->y0 : y0,
                r->x1 < x1 ? r->x1 : x1, r->y1 < y1 ? r->y1 : y1);
    }
}

void clip_screen(void) { clip = &r_screen; }
void clip_win(int i) { clip = &r_win[i]; }

void clip_and(const region_t *g, int x0, int y0, int x1, int y1)
{
    reg_and(&r_tmp, g, x0, y0, x1, y1);
    clip = &r_tmp;
}

int in_clip(int x, int y)
{
    for (int k = 0; k < clip->n; k++) {
        const rect_t *r = &clip->r[k];
        if (x >= r->x0 && x <= r->x1 && y >= r->y0 && y <= r->y1)
            return 1;
    }
    return 0;
}

/* the regions, limited to a damaged rectangle */
static void layout_in(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (x1 > DW - 1) x1 = DW - 1;
    if (y1 > DH - 1) y1 = DH - 1;
    if (y0 < MENU_H + 1)
        y0 = MENU_H + 1;
    reg_set(&r_desk, x0, y0, x1, y1);
    for (int i = 0; i < NWIN; i++)
        r_win[i].n = 0;
    for (int k = 0; k < nz; k++) {
        int i = zord[k];
        win_t *w = &wins[i];
        reg_set(&r_win[i], w->x > x0 ? w->x : x0, w->y > y0 ? w->y : y0,
                w->x + w->w - 1 < x1 ? w->x + w->w - 1 : x1, w->y + w->h - 1 < y1 ? w->y + w->h - 1 : y1);
        for (int j = k + 1; j < nz; j++) {
            win_t *u = &wins[zord[j]];
            reg_sub(&r_win[i], u->x, u->y, u->x + u->w - 1, u->y + u->h - 1);
        }
        reg_sub(&r_desk, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1);
    }
}

void win_layout(void) { layout_in(0, 0, DW - 1, DH - 1); }

/* ---- the windows ---- */
void win_snap(int *x, int *w)
{
    *x = ((*x + 3) & ~3) - 1;               /* the nearest 3 (mod 4) */
    if (*x > DW - 40) *x = DW - 41 - ((DW - 41 - 3) & 3);
    *w = (*w + 2) & ~3;
}

static const int16_t layout0[NWIN][4] = {
    { 3, 17, TOOLS_W, TOOLS_H },
    { 63, 284, COLOURS_W, COLOURS_H },
    { 63, 17, 572, 264 },
    { 343, 40, 292, 216 },
    { 323, 60, 292, 216 },
};

void win_reset(int i)
{
    wins[i].x = layout0[i][0];
    wins[i].y = layout0[i][1];
    wins[i].w = layout0[i][2];
    wins[i].h = layout0[i][3];
}

void win_init(void)
{
    nz = 0;
    for (int i = 0; i < NWIN; i++) {
        wins[i].x = layout0[i][0];
        wins[i].y = layout0[i][1];
        wins[i].w = layout0[i][2];
        wins[i].h = layout0[i][3];
        wins[i].open = 0;
    }
    /* pictures below, palettes above */
    win_open(WIN_PIC0);
    win_open(WIN_TOOLS);
    win_open(WIN_COLOURS);
    win_layout();
}

int win_front_pic(void)
{
    for (int k = nz; k-- > 0;)
        if (!is_palette(zord[k]))
            return zord[k];
    return -1;
}

/* put window i at the top of its layer */
static void zinsert(int i)
{
    int k = 0;
    for (int j = 0; j < nz; j++)
        if (zord[j] == i) {
            memmove(&zord[j], &zord[j + 1], (unsigned)(nz - j - 1));
            nz--;
            break;
        }
    if (is_palette(i))
        k = nz;
    else
        while (k < nz && !is_palette(zord[k]))
            k++;
    memmove(&zord[k + 1], &zord[k], (unsigned)(nz - k));
    zord[k] = (uint8_t)i;
    nz++;
}

void win_open(int i)
{
    wins[i].open = 1;
    zinsert(i);
}

void win_close(int i)
{
    if (!wins[i].open)
        return;
    wins[i].open = 0;
    for (int j = 0; j < nz; j++)
        if (zord[j] == i) {
            memmove(&zord[j], &zord[j + 1], (unsigned)(nz - j - 1));
            nz--;
            break;
        }
    win_t *w = &wins[i];
    win_draw_rect(w->x, w->y, w->x + w->w - 1, w->y + w->h - 1);
}

void win_raise(int i)
{
    uint8_t before[NWIN];
    memcpy(before, zord, nz);
    int old = win_front_pic();
    zinsert(i);
    if (!memcmp(before, zord, nz))
        return;                             /* already at the top of its layer */
    /* nothing covered it: its pixels are all on the screen already */
    win_t *w = &wins[i];
    int x0 = w->x < 0 ? 0 : w->x, x1 = w->x + w->w - 1 > DW - 1 ? DW - 1 : w->x + w->w - 1;
    int y1 = w->y + w->h - 1 > DH - 1 ? DH - 1 : w->y + w->h - 1;
    const region_t *g = &r_win[i];
    int whole = g->n == 1 && g->r[0].x0 == x0 && g->r[0].x1 == x1 && g->r[0].y0 == w->y && g->r[0].y1 == y1;
    win_layout();
    if (!whole)
        win_draw(i);
    else if (!is_palette(i))
        win_frame(i);                       /* now active */
    if (old >= 0 && old != i && !is_palette(i))
        win_frame(old);                     /* no longer active */
}

const region_t *win_region(int i) { return &r_win[i]; }

void win_move(int i, int x, int y, int w, int h)
{
    win_t *o = &wins[i];
    int ox0 = o->x, oy0 = o->y, ox1 = o->x + o->w - 1, oy1 = o->y + o->h - 1;
    o->x = x; o->y = y; o->w = w; o->h = h;
    if (i >= WIN_PIC0)
        view_fit(i - WIN_PIC0);
    win_layout();
    if (x <= ox1 && x + w - 1 >= ox0 && y <= oy1 && y + h - 1 >= oy0)   /* overlapping: one redraw */
        win_draw_rect(x < ox0 ? x : ox0, y < oy0 ? y : oy0, x + w - 1 > ox1 ? x + w - 1 : ox1,
                      y + h - 1 > oy1 ? y + h - 1 : oy1);
    else {
        win_draw_rect(ox0, oy0, ox1, oy1);
        win_draw_rect(x, y, x + w - 1, y + h - 1);
    }
}

int win_at(int x, int y, int *part)
{
    *part = PART_NONE;
    for (int k = nz; k-- > 0;) {
        int i = zord[k];
        win_t *w = &wins[i];
        if (x < w->x || x >= w->x + w->w || y < w->y || y >= w->y + w->h)
            continue;
        if (y <= w->y + 14)
            *part = x >= w->x + 4 && x <= w->x + 16 && y >= w->y + 1 ? PART_CLOSE : PART_TITLE;
        else if (i >= WIN_PIC0) {
            int vr = WIN_CX(i) + WIN_CW(i) - SB, vb = WIN_CY(i) + WIN_CH(i) - SB;
            if (x >= vr && y >= vb) *part = PART_GROW;
            else if (x >= vr) *part = PART_VSCROLL;
            else if (y >= vb) *part = PART_HSCROLL;
            else *part = PART_CONTENT;
        } else
            *part = PART_CONTENT;
        return i;
    }
    return -1;
}

/* ---- drawing ---- */
void win_frame(int i)
{
    win_t *w = &wins[i];
    clip_win(i);
    int x0 = w->x, y0 = w->y, x1 = w->x + w->w - 2, y1 = w->y + w->h - 2;
    int active = is_palette(i) || i == win_front_pic();
    sframe(x0, y0, x1, y1, BLACK);
    sfill(x0 + 1, y0 + 1, x1 - 1, y0 + 13, WHITE);
    if (active)
        for (int y = y0 + 3; y <= y0 + 11; y += 2)
            sfill(x0 + 2, y, x1 - 2, y, BLACK);
    sfill(x0 + 4, y0 + 2, x0 + 16, y0 + 12, WHITE);     /* the close box */
    sframe(x0 + 6, y0 + 3, x0 + 14, y0 + 11, BLACK);
    if (!is_palette(i)) {
        static const char *const names[NVIEW] = { "Picture", "View 2", "View 3" };
        char t[20];
        int v = i - WIN_PIC0, n = (int)strlen(names[v]);
        memcpy(t, names[v], (unsigned)n);
        t[n++] = ' ';
        t[n++] = ' ';
        t[n++] = (char)('0' + view_zoom_of(v));
        t[n++] = 'x';
        t[n] = 0;
        int tx = ((x0 + x1) / 2 - 4 * n) & ~1;
        if (tx < x0 + 20) tx = x0 + 20;
        sfill(tx - 6, y0 + 1, tx + 8 * n + 5, y0 + 13, WHITE);
        stext(tx, y0 + 1, t, BLACK, WHITE);
    }
    sfill(x0, y0 + 14, x1, y0 + 14, BLACK);
    sfill(x1 + 1, y0 + 2, x1 + 1, y1 + 1, BLACK);       /* the shadow */
    sfill(x0 + 2, y1 + 1, x1 + 1, y1 + 1, BLACK);
    sfill(x1 + 1, y0, x1 + 1, y0 + 1, GREY);            /* (the desktop at its ends) */
    sfill(x0, y1 + 1, x0 + 1, y1 + 1, GREY);
}

void win_draw(int i)
{
    if (!wins[i].open)
        return;
    win_frame(i);
    if (i == WIN_TOOLS) {
        ui_draw_tools();
        ui_draw_options();
    } else if (i == WIN_COLOURS)
        ui_draw_colours();
    else {
        int save = cur_view;
        view_use(i - WIN_PIC0);
        render_view();
        view_use(save);
    }
}

void win_draw_rect(int x0, int y0, int x1, int y1)
{
    if (full_view)
        return;
    layout_in(x0, y0, x1, y1);
    clip = &r_desk;
    sfill(0, 0, DW - 1, DH - 1, GREY);
    for (int k = 0; k < nz; k++)
        if (r_win[zord[k]].n)               /* (only what shows in the rectangle) */
            win_draw(zord[k]);
    win_layout();
    clip_screen();
}

/* ---- live moving and sizing ---- */

/* r (a list) less the rectangle, drawn */
static void redraw_less(region_t *r, int x0, int y0, int x1, int y1)
{
    reg_sub(r, x0, y0, x1, y1);
    for (int k = 0; k < r->n; k++)
        win_draw_rect(r->r[k].x0, r->r[k].y0, r->r[k].x1, r->r[k].y1);
}

/* the pixels of the rectangles g (where they go) from dx, dy back (a whole
 * number of 16-bit words across): a row at a time, from the end the
 * pixels move towards, each source row read whole before it is written */
static void move_region(const region_t *g, int dx, int dy)
{
    static uint16_t tmpw[STRIDE / 2];       /* (16-bit aligned: copied a word at a time) */
    uint8_t *tmp = (uint8_t *)tmpw;
    int y0 = DH, y1 = -1;
    for (int k = 0; k < g->n; k++) {
        if (g->r[k].y0 < y0) y0 = g->r[k].y0;
        if (g->r[k].y1 > y1) y1 = g->r[k].y1;
        damage(g->r[k].x0, g->r[k].y0, g->r[k].x1, g->r[k].y1);
    }
    int db = dx >> 1;                       /* (dx is even) */
    for (int n = 0; n <= y1 - y0; n++) {
        int y = dy > 0 ? y1 - n : y0 + n;
        int lo = STRIDE, hi = -1;           /* the bytes this row needs */
        for (int k = 0; k < g->n; k++) {
            const rect_t *r = &g->r[k];
            if (y < r->y0 || y > r->y1)
                continue;
            if ((r->x0 >> 1) < lo) lo = r->x0 >> 1;
            if ((r->x1 >> 1) > hi) hi = r->x1 >> 1;
        }
        if (hi < 0)
            continue;
        const uint8_t *src = SCREEN + rowoff[y - dy];
        if (!dy) {                          /* (the same row: read it all first) */
            fastcopy(tmp + lo - db, src + (unsigned)(lo - db), (unsigned)(hi - lo + 1));
            src = tmp;
        }
        uint8_t *row = SCREEN + rowoff[y];
        for (int k = 0; k < g->n; k++) {
            const rect_t *r = &g->r[k];
            if (y < r->y0 || y > r->y1)
                continue;
            int b0 = r->x0 >> 1, b1 = r->x1 >> 1;
            uint8_t keep_l = row[b0], keep_r = row[b1];
            fastcopy(row + b0, src + b0 - db, (unsigned)(b1 - b0 + 1));
            if (r->x0 & 1) row[b0] = (uint8_t)((keep_l & 0xF0) | (row[b0] & 0x0F));
            if (!(r->x1 & 1)) row[b1] = (uint8_t)((keep_r & 0x0F) | (row[b1] & 0xF0));
        }
    }
}

/* Live: what shows of the window is copied to where it goes, except under
 * the windows above it; then only the rest is drawn: the parts of it the
 * copy did not give, and what it uncovers. */
void win_drag_to(int i, int x, int y)
{
    win_t *w = &wins[i];
    int ox0 = w->x, oy0 = w->y, dx = x - ox0, dy = y - oy0;
    if (!dx && !dy)
        return;
    int k0 = 0;
    while (zord[k0] != i) k0++;
    /* the copy: its visible region, moved, on the screen, not under those above */
    region_t c;
    c.n = 0;
    for (int k = 0; k < r_win[i].n; k++) {
        const rect_t *r = &r_win[i].r[k];
        reg_add(&c, r->x0 + dx < 0 ? 0 : r->x0 + dx, r->y0 + dy < MENU_H + 1 ? MENU_H + 1 : r->y0 + dy,
                r->x1 + dx > DW - 1 ? DW - 1 : r->x1 + dx, r->y1 + dy > DH - 1 ? DH - 1 : r->y1 + dy);
    }
    for (int k = k0 + 1; k < nz; k++) {
        win_t *u = &wins[zord[k]];
        reg_sub(&c, u->x, u->y, u->x + u->w - 1, u->y + u->h - 1);
    }
    move_region(&c, dx, dy);
    w->x = x;
    w->y = y;
    if (i >= WIN_PIC0)
        view_moved(i - WIN_PIC0, dx, dy);
    win_layout();
    /* the rest of it, and what it uncovers (not what is above it) */
    region_t r;
    r = r_win[i];
    for (int k = 0; k < c.n; k++)
        reg_sub(&r, c.r[k].x0, c.r[k].y0, c.r[k].x1, c.r[k].y1);
    redraw_less(&r, DW, DH, DW, DH);
    reg_set(&r, ox0 < 0 ? 0 : ox0, oy0, ox0 + w->w - 1, oy0 + w->h - 1);
    reg_sub(&r, x, y, x + w->w - 1, y + w->h - 1);
    for (int k = k0 + 1; k < nz; k++) {
        win_t *u = &wins[zord[k]];
        reg_sub(&r, u->x, u->y, u->x + u->w - 1, u->y + u->h - 1);
    }
    redraw_less(&r, DW, DH, DW, DH);
}

void win_size_to(int i, int nw, int nh)
{
    win_t *w = &wins[i];
    int ow = w->w, oh = w->h;
    if (nw == ow && nh == oh)
        return;
    int ovw = WIN_CW(i) - SB, ovh = WIN_CH(i) - SB;
    w->w = nw;
    w->h = nh;
    win_layout();
    win_frame(i);
    if (i >= WIN_PIC0)
        view_grown(i - WIN_PIC0, ovw, ovh);
    else
        win_draw(i);
    region_t r;
    reg_set(&r, w->x, w->y, w->x + ow - 1, w->y + oh - 1);   /* what it uncovers */
    redraw_less(&r, w->x, w->y, w->x + nw - 1, w->y + nh - 1);
}

void win_draw_all(void)
{
    clip_screen();
    ui_draw_menubar();
    win_draw_rect(0, 0, DW - 1, DH - 1);
}
