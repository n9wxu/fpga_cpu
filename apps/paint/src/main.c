/* main.c: the main loop, the mouse and keyboard, and what each tool does
 * with them (paint.h, README.md). */
#include "paint.h"
#include "../../../fpga/lib/ps2.h"
#include "../../../fpga/lib/irq.h"
#include <string.h>
#ifdef PERF
static void pnum(const char *s, uint32_t v)   /* (printf is too big to fit) */
{
    char t[12];
    int n = 0;
    while (*s) CON_OUT = (uint16_t)*s++;
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) CON_OUT = (uint16_t)t[--n];
}
#endif

state_t st;

/* the pointer, on the screen */
static int mx = DW / 2, my = DH / 2;
static int acc_x, acc_y;                /* sub-pixel motion, 1/2 px */
static uint8_t held;                    /* buttons down */
static uint8_t ignore;                  /* buttons whose press was used up (closing a menu) */
static int8_t drag = -1;                /* the button doing the operation */
static int pdx, pdy;                    /* the picture point under the pointer */
static int lx, ly;                      /* the last point of a stroke */
static int sx0, sy0;                    /* where a drag began */
static brush_t brush, eraser;

enum { OP_NONE, OP_FREEHAND, OP_SPRAY, OP_SHAPE, OP_POLY, OP_CURVE, OP_TEXT, OP_MARQUEE,
       OP_MOVE, OP_PAN, OP_MIDDLE, OP_PICK, OP_MENU, OP_HELP, OP_WMOVE, OP_WGROW, OP_THUMB };
static uint8_t op;
static uint8_t op_right;                /* begun with the right button: the colours swapped */
static uint8_t menu_drag;               /* the menu was opened by a press still held */

/* the selection: 1 a box on the picture, 2 floating (fl_* in view.c) */
static uint8_t sel;
static int sel_x0, sel_y0, sel_x1, sel_y1;
static int grab_x, grab_y;
static int clip_w, clip_h;

/* polygon and curve */
static int16_t ppx[MAXPOLY], ppy[MAXPOLY];
static int pn;
static uint8_t cv_stage;                /* 1 the line, 2 the first bend, 3 the second */
static int cv[8];
static uint16_t click_frame;
static int click_x, click_y;

/* text */
static int tx0, tx, ty;

static int shift(void) { return kbd_mod() & 1; }

/* ---- colours ---- */
static void paints(int right)
{
    uint8_t f = right ? st.bg : st.fg, b = right ? st.fg : st.bg;
    paint_make(paint_line, f, f, 0);
    paint_make(paint_fill, f, b, st.pat);
    paint_make(paint_back, b, b, 0);
}

static void tools_changed(void)
{
    brush_make(&brush, st.brush_shape, brush_sizes[st.brush_size]);
    brush_make(&eraser, BR_SQUARE, eraser_sizes[st.eraser_size]);
}

/* ---- the selection ---- */
static void sel_show(void)
{
    if (sel == 1)
        marquee_set(1, sel_x0, sel_y0, sel_x1, sel_y1);
    else if (sel == 2)
        marquee_set(1, fl_x, fl_y, fl_x + fl_w - 1, fl_y + fl_h - 1);
    else
        marquee_set(0, 0, 0, 0, 0);
}

static void fl_mark(void) { mark(fl_x, fl_y, fl_x + fl_w - 1, fl_y + fl_h - 1); }

/* put a floating selection down: the end of its operation */
static void sel_drop(void)
{
    if (sel == 2) {
        px_copy(DOC, fl_x, fl_y, FLOATB, 0, 0, fl_w, fl_h, fl_trans ? fl_bg : -1);
        fl_on = 0;
        fl_mark();
        op_end();
    }
    sel = 0;
    sel_show();
}

/* the box's pixels float; copy: they stay in the picture too */
static void sel_lift(int copy)
{
    int w = sel_x1 - sel_x0 + 1, h = sel_y1 - sel_y0 + 1;
    op_begin();
    px_copy(FLOATB, 0, 0, DOC, sel_x0, sel_y0, w, h, -1);
    if (!copy) {
        paints(0);
        USE(paint_back);
        for (int y = sel_y0; y <= sel_y1; y++)
            span(sel_x0, sel_x1, y);
    }
    fl_x = sel_x0; fl_y = sel_y0; fl_w = w; fl_h = h;
    fl_bg = st.bg;
    fl_trans = st.sel_trans;
    fl_on = 1;
    sel = 2;
    fl_mark();
    sel_show();
}

static int in_sel(int x, int y)
{
    if (sel == 1)
        return x >= sel_x0 && x <= sel_x1 && y >= sel_y0 && y <= sel_y1;
    if (sel == 2)
        return x >= fl_x && x < fl_x + fl_w && y >= fl_y && y < fl_y + fl_h;
    return 0;
}

/* ---- the operations of the shape tools ---- */
static void constrain(int *x, int *y, int line)
{
    if (!shift())
        return;
    int dx = *x - sx0, dy = *y - sy0;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (line && ay * 2 < ax) { *y = sy0; return; }
    if (line && ax * 2 < ay) { *x = sx0; return; }
    int m = ax > ay ? ax : ay;
    *x = sx0 + (dx < 0 ? -m : m);
    *y = sy0 + (dy < 0 ? -m : m);
}

static int shape_kind(void) { return st.tool == T_RECT ? SH_RECT : st.tool == T_RRECT ? SH_RRECT : SH_ELLIPSE; }

static void draw_drag(int final)
{
    int x = pdx, y = pdy, w = line_widths[st.line_w];
    paints(op_right);
    if (st.tool == T_LINE) {
        constrain(&x, &y, 1);
        USE(paint_line);
        line_w(sx0, sy0, x, y, w);
        ui_coords(x, y, (x > sx0 ? x - sx0 : sx0 - x) + 1, (y > sy0 ? y - sy0 : sy0 - y) + 1);
        return;
    }
    constrain(&x, &y, 0);
    int mode = st.fill_mode;
    if (!final && mode == FM_FILLED)        /* a preview of a filled shape: its outline */
        mode = FM_OUTLINE, w = 1;
    if (!final)
        mode = FM_OUTLINE;
    shape(shape_kind(), sx0, sy0, x, y, w, mode);
    ui_coords(x, y, (x > sx0 ? x - sx0 : sx0 - x) + 1, (y > sy0 ? y - sy0 : sy0 - y) + 1);
}

/* the polygon's last edge, to the corner following the pointer (a preview);
 * the edges before it stay drawn until the polygon is finished */
static void draw_poly(int final)
{
    paints(op_right);
    if (final) {
        int n = pn;
        while (n > 1 && ppx[n - 1] == ppx[n - 2] && ppy[n - 1] == ppy[n - 2])
            n--;                            /* (the double click's repeated corner) */
        polygon(n, ppx, ppy, line_widths[st.line_w], st.fill_mode, 1);
    } else
        polygon(2, ppx + pn - 2, ppy + pn - 2, line_widths[st.line_w], FM_OUTLINE, 0);
}

/* a corner is fixed: its edge stays, a new rubber band begins */
static void poly_fix(void)
{
    preview_erase();
    preview_end();
    draw_poly(0);
    preview_begin();
}

static void poly_finish(void)
{
    preview_erase();
    op_cancel();                            /* the edges drawn so far */
    op_begin();
    draw_poly(1);
    op_end();
    op = OP_NONE;
}

static void draw_curve(void)
{
    paints(op_right);
    bezier(cv[0], cv[1], cv[2], cv[3], cv[4], cv[5], cv[6], cv[7], line_widths[st.line_w]);
}

static void curve_finish(void)
{
    preview_erase();
    draw_curve();
    op_end();
    op = OP_NONE;
}

/* ---- text ---- */
static int tscale(void) { return st.text_scale + 1; }

static void text_begin(int x, int y)
{
    op_begin();
    op = OP_TEXT;
    tx0 = tx = x;
    ty = y - 7 * tscale();
    caret_set(1, tx, ty, 14 * tscale());
}

static void text_end(void)
{
    caret_set(0, 0, 0, 0);
    op_end();
    op = OP_NONE;
}

static void text_key(uint16_t k)
{
    int s = tscale();
    paints(op_right);
    if (k == 13) {
        tx = tx0;
        ty += 14 * s;
    } else if (k == 8) {
        if (tx > tx0) {
            tx -= 8 * s;
            px_copy(DOC, tx, ty, BASE, tx, ty, 8 * s, 14 * s, -1);   /* as it was */
            mark(tx, ty, tx + 8 * s - 1, ty + 14 * s - 1);
        }
    } else if (k >= 32 && k < 127) {
        glyph_draw(tx, ty, (uint8_t)k, s, st.text_opaque);
        tx += 8 * s;
    }
    caret_set(1, tx, ty, 14 * s);
}

/* ---- finishing what is under way (before another tool, a command) ---- */
static void finish_ops(void)
{
    switch (op) {
    case OP_TEXT: text_end(); break;
    case OP_POLY: poly_finish(); break;
    case OP_CURVE: curve_finish(); break;
    case OP_SHAPE: preview_erase(); draw_drag(1); op_end(); break;
    case OP_FREEHAND: case OP_SPRAY: op_end(); break;
    }
    if (op != OP_MENU && op != OP_HELP)
        op = OP_NONE;
    drag = -1;
}

static void cancel_ops(void)
{
    switch (op) {
    case OP_SHAPE: case OP_POLY: case OP_CURVE: preview_erase(); op_cancel(); op = OP_NONE; break;
    case OP_TEXT: text_end(); break;
    case OP_MARQUEE: sel = 0; sel_show(); op = OP_NONE; break;
    default: finish_ops(); break;
    }
    drag = -1;
}

static void select_tool(int t)
{
    if (t == st.tool)
        return;
    finish_ops();
    if (t != T_SELECT)
        sel_drop();
    if (st.tool != T_PICKER)
        st.prev_tool = st.tool;             /* (the picker goes back to it) */
    st.tool = (uint8_t)t;
    ui_tool_changed();
}

/* ---- commands ---- */
int cmd_enabled(int c)
{
    switch (c) {
    case CMD_UNDO: return can_undo() || sel == 2 || op == OP_TEXT;
    case CMD_REDO: return can_redo() && sel != 2;
    case CMD_CUT: case CMD_COPY: case CMD_CLEAR: return sel != 0;
    case CMD_PASTE: return st.has_clip;
    case CMD_ZOOMIN: return zoom < 8;
    case CMD_ZOOMOUT: case CMD_ZOOM1: return zoom > 1;
    case CMD_WNEW: return !full_view && (!wins[WIN_PIC1].open || !wins[WIN_PIC2].open);
    case CMD_WTOOLS: case CMD_WCOLOURS: case CMD_WTIDY: return !full_view;
    }
    return 1;
}

int cmd_checked(int c)
{
    switch (c) {
    case CMD_TRANS: return st.sel_trans;
    case CMD_GRID: return grid;
    case CMD_FULL: return full_view;
    case CMD_WTOOLS: return wins[WIN_TOOLS].open;
    case CMD_WCOLOURS: return wins[WIN_COLOURS].open;
    }
    return 0;
}

static void copy_sel(void)
{
    if (sel == 1) {
        clip_w = sel_x1 - sel_x0 + 1;
        clip_h = sel_y1 - sel_y0 + 1;
        px_copy(CLIPB, 0, 0, DOC, sel_x0, sel_y0, clip_w, clip_h, -1);
    } else if (sel == 2) {
        clip_w = fl_w;
        clip_h = fl_h;
        px_copy(CLIPB, 0, 0, FLOATB, 0, 0, fl_w, fl_h, -1);
    }
    st.has_clip = 1;
}

static void clear_sel(void)
{
    if (sel == 1) {
        op_begin();
        paints(0);
        USE(paint_back);
        for (int y = sel_y0; y <= sel_y1; y++)
            span(sel_x0, sel_x1, y);
        op_end();
    } else if (sel == 2) {
        fl_on = 0;
        fl_mark();
        op_end();                           /* (what the lift cut out stays cut) */
    }
    sel = 0;
    sel_show();
}

/* a transformation of the selection, or of the whole picture */
enum { TR_FLIPH, TR_FLIPV, TR_ROT, TR_MAP };
static void transform(int kind, const uint8_t *map)
{
    uint8_t *b = DOC;
    int x = 0, y = 0, w = DW, h = DH;
    if (sel == 2) {
        b = FLOATB;
        w = fl_w;
        h = fl_h;
    } else {
        if (sel == 1) {
            x = sel_x0; y = sel_y0; w = sel_x1 - sel_x0 + 1; h = sel_y1 - sel_y0 + 1;
        }
        op_begin();
    }
    if (kind == TR_FLIPH || kind == TR_ROT) px_flip_h(b, x, y, w, h);
    if (kind == TR_FLIPV || kind == TR_ROT) px_flip_v(b, x, y, w, h);
    if (kind == TR_MAP) px_map(b, x, y, w, h, map);
    if (sel == 2)
        fl_mark();
    else {
        mark(x, y, x + w - 1, y + h - 1);
        op_end();
    }
}

static void new_picture(uint8_t c)
{
    finish_ops();
    sel_drop();
    op_begin();
    buf_fill(DOC, c);
    mark_all();
    op_end();
}

/* ---- windows ---- */
static void toggle_win(int i)
{
    if (wins[i].open)
        win_close(i);
    else {
        win_open(i);
        win_layout();
        win_draw(i);
    }
}

static void new_view(void)
{
    int i = !wins[WIN_PIC1].open ? WIN_PIC1 : WIN_PIC2;
    int cx = ox + vw / zoom / 2, cy = oy + vh / zoom / 2;   /* the middle of this view */
    int z = zoom * 4 > 8 ? 8 : zoom * 4;
    win_reset(i);
    win_open(i);
    win_layout();
    view_init(i - WIN_PIC0, z);
    view_use(i - WIN_PIC0);
    ox = cx - vw / zoom / 2;
    oy = cy - vh / zoom / 2;
    view_clamp();
    win_draw_all();
}

static void tidy(void)
{
    for (int i = 0; i < NWIN; i++)
        win_reset(i);
    for (int v = 0; v < NVIEW; v++)
        view_fit(v);
    win_layout();
    win_draw_all();
}

static void close_win(int i)
{
    if (i >= WIN_PIC0) {
        int n = 0;
        for (int k = WIN_PIC0; k < NWIN; k++)
            n += wins[k].open;
        if (n == 1) {
            ui_message("The last view");
            return;
        }
    }
    win_close(i);
    if (i == WIN_PIC0 + cur_view)
        view_use(win_front_pic() - WIN_PIC0);
}

static void do_cmd(int c)
{
    if (c != CMD_UNDO && c != CMD_HELP && c != CMD_FULL && c != CMD_TRANS)
        finish_ops();
    switch (c) {
    case CMD_NEW: new_picture(WHITE); break;
    case CMD_NEWBG: new_picture(st.bg); break;
    case CMD_FULL: view_full(!full_view); break;
    case CMD_HELP:
        finish_ops();
        help_show();
        op = OP_HELP;
        break;
    case CMD_UNDO:
        if (sel == 2) {                     /* the move or paste under way */
            op_cancel();
            fl_on = 0;
            fl_mark();
            sel = 0;
            sel_show();
            break;
        }
        finish_ops();
        if (sel == 1) { sel = 0; sel_show(); }
        if (!undo()) ui_message("Nothing to undo");
        break;
    case CMD_REDO:
        if (sel) { sel_drop(); }
        if (!redo()) ui_message("Nothing to redo");
        break;
    case CMD_CUT: copy_sel(); clear_sel(); break;
    case CMD_COPY: copy_sel(); ui_message("Copied"); break;
    case CMD_CLEAR: clear_sel(); break;
    case CMD_PASTE: {
        if (!st.has_clip) break;
        select_tool(T_SELECT);
        sel_drop();
        op_begin();
        px_copy(FLOATB, 0, 0, CLIPB, 0, 0, clip_w, clip_h, -1);
        fl_w = clip_w; fl_h = clip_h;
        fl_x = ox; fl_y = oy;
        fl_bg = st.bg;
        fl_trans = st.sel_trans;
        fl_on = 1;
        sel = 2;
        fl_mark();
        sel_show();
        break;
    }
    case CMD_SELALL:
        select_tool(T_SELECT);
        sel_drop();
        sel = 1;
        sel_x0 = 0; sel_y0 = 0; sel_x1 = DW - 1; sel_y1 = DH - 1;
        sel_show();
        break;
    case CMD_TRANS:
        st.sel_trans ^= 1;
        if (sel == 2) {
            fl_trans = st.sel_trans;
            fl_mark();
        }
        if (st.tool == T_SELECT) ui_draw_options();
        break;
    case CMD_FLIPH: transform(TR_FLIPH, 0); break;
    case CMD_FLIPV: transform(TR_FLIPV, 0); break;
    case CMD_ROT180: transform(TR_ROT, 0); break;
    case CMD_INVERT: {
        static const uint8_t inv[16] = { 7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15 };
        transform(TR_MAP, inv);
        break;
    }
    case CMD_REPLACE: {
        uint8_t m[16];
        for (int i = 0; i < 16; i++) m[i] = (uint8_t)i;
        m[st.fg] = st.bg;
        transform(TR_MAP, m);
        break;
    }
    case CMD_CLEARPIC: new_picture(st.bg); break;
    case CMD_ZOOMIN: view_zoom(zoom * 2, mx, my); break;
    case CMD_ZOOMOUT: view_zoom(zoom / 2, mx, my); break;
    case CMD_ZOOM1: view_zoom(1, mx, my); break;
    case CMD_GRID: grid ^= 1; if (!full_view) win_draw_all(); else render_view(); break;
    case CMD_WTOOLS: toggle_win(WIN_TOOLS); break;
    case CMD_WCOLOURS: toggle_win(WIN_COLOURS); break;
    case CMD_WNEW: new_view(); break;
    case CMD_WTIDY: tidy(); break;
    }
    if (!full_view && op != OP_HELP)
        ui_draw_options();                  /* (the zoom, the transparency) */
}

/* ---- the options box and the colour bar ---- */
static void set_option(int top, int i)
{
    if (i < 0)
        return;
    switch (st.tool) {
    case T_BRUSH: if (top) st.brush_shape = (uint8_t)i; else st.brush_size = (uint8_t)i; break;
    case T_ERASER: st.eraser_size = (uint8_t)i; break;
    case T_SPRAY: st.spray_size = (uint8_t)i; break;
    case T_LINE: case T_CURVE: st.line_w = (uint8_t)i; break;
    case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON:
        if (top) st.fill_mode = (uint8_t)i; else st.line_w = (uint8_t)i;
        break;
    case T_TEXT: if (top) st.text_opaque = (uint8_t)i; else st.text_scale = (uint8_t)i; break;
    case T_SELECT:
        st.sel_trans = (uint8_t)i;
        if (sel == 2) { fl_trans = st.sel_trans; fl_mark(); }
        break;
    case T_ZOOM: view_zoom(1 << i, -1, -1); break;
    }
    tools_changed();
    ui_draw_options();
}

/* the wheel over the options: the next or previous row */
static void step_option(int d)
{
    int n, v;
    switch (st.tool) {
    case T_BRUSH: n = 6; v = st.brush_size; break;
    case T_ERASER: n = 5; v = st.eraser_size; break;
    case T_SPRAY: n = 4; v = st.spray_size; break;
    case T_LINE: case T_CURVE: case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON: n = 5; v = st.line_w; break;
    case T_TEXT: n = 2; v = st.text_scale; break;
    case T_SELECT: n = 2; v = st.sel_trans; break;
    case T_ZOOM: n = 4; v = zoom == 1 ? 0 : zoom == 2 ? 1 : zoom == 4 ? 2 : 3; break;
    default: return;
    }
    v += d;
    if (v < 0) v = 0;
    if (v >= n) v = n - 1;
    set_option(0, v);
}

static void colours_changed(void)
{
    if (!full_view)
        ui_draw_colours();
}

/* ---- the canvas ---- */
static void eraser_at(int x, int y, int replace)
{
    if (!replace) {
        stamp(&eraser, x, y);
        return;
    }
    int d = eraser.n, x0 = x - d / 2, y0 = y - d / 2;   /* the right button: only the foreground colour */
    for (int j = 0; j < d; j++)
        for (int i = 0; i < d; i++) {
            int px = x0 + i, py = y0 + j;
            if ((unsigned)px < DW && (unsigned)py < DH && getpx(DOC, px, py) == st.fg)
                dot(px, py);
        }
}

static void eraser_stroke(int x0, int y0, int x1, int y1, int replace)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (x0 != x1 || y0 != y1) {
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
        eraser_at(x0, y0, replace);
    }
}

static void freehand_to(int x, int y)
{
    paints(op_right);
    switch (st.tool) {
    case T_PENCIL: USE(paint_line); line1(lx, ly, x, y); break;
    case T_BRUSH: USE(paint_fill); stroke(&brush, lx, ly, x, y); break;
    case T_ERASER:                          /* always the background colour */
        paint_make(paint_back, st.bg, st.bg, 0);
        USE(paint_back);
        eraser_stroke(lx, ly, x, y, op_right);
        break;
    }
    lx = x;
    ly = y;
}

static void wdrag_motion(void);
static void wdrag_end(void);
static int th_vert, th_grab;

static void canvas_press(int b)
{
    int x = pdx, y = pdy, right = b == 1;
    if (op == OP_POLY) {
        int near0 = pn >= 3 && (to_scr_x(ppx[0]) - mx) * (to_scr_x(ppx[0]) - mx) +
                    (to_scr_y(ppy[0]) - my) * (to_scr_y(ppy[0]) - my) <= 25;
        int dbl = (uint16_t)(FRAME - click_frame) < 25 && (mx - click_x) * (mx - click_x) + (my - click_y) * (my - click_y) <= 16;
        click_frame = FRAME; click_x = mx; click_y = my;
        if (right || pn >= MAXPOLY) {
            ppx[pn - 1] = (int16_t)x; ppy[pn - 1] = (int16_t)y;
            poly_finish();
            return;
        }
        if (near0 || dbl) {
            pn--;                           /* the corner following the pointer */
            poly_finish();
            return;
        }
        ppx[pn - 1] = (int16_t)x; ppy[pn - 1] = (int16_t)y;
        poly_fix();
        ppx[pn] = (int16_t)x; ppy[pn] = (int16_t)y;
        pn++;
        drag = (int8_t)b;
        return;
    }
    if (op == OP_CURVE) {
        if (right) {
            curve_finish();
            return;
        }
        drag = (int8_t)b;
        preview_erase();
        if (cv_stage == 2) { cv[2] = cv[4] = x; cv[3] = cv[5] = y; }
        else { cv[4] = x; cv[5] = y; }
        draw_curve();
        return;
    }
    if (op == OP_TEXT) {
        text_end();
        if (st.tool == T_TEXT) {
            op_right = (uint8_t)right;
            text_begin(x, y);
        }
        return;
    }
    if (op != OP_NONE)
        return;
    op_right = (uint8_t)right;
    paints(right);
    sx0 = lx = x;
    sy0 = ly = y;
    switch (st.tool) {
    case T_PENCIL: case T_BRUSH: case T_ERASER:
        op_begin();
        op = OP_FREEHAND;
        drag = (int8_t)b;
        if (st.tool == T_PENCIL) { USE(paint_line); dot(x, y); }
        else if (st.tool == T_BRUSH) { USE(paint_fill); stamp(&brush, x, y); }
        else {
            paint_make(paint_back, st.bg, st.bg, 0);
            USE(paint_back);
            eraser_at(x, y, right);
        }
        break;
    case T_SPRAY:
        op_begin();
        op = OP_SPRAY;
        drag = (int8_t)b;
        break;
    case T_BUCKET:
        op_begin();
        USE(paint_fill);
        flood(x, y);
        op_end();
        break;
    case T_PICKER:
        if ((unsigned)x < DW && (unsigned)y < DH) {
            uint8_t c = getpx(DOC, x, y);
            if (right) st.bg = c; else st.fg = c;
            colours_changed();
        }
        op = OP_PICK;
        drag = (int8_t)b;
        break;
    case T_TEXT:
        text_begin(x, y);
        break;
    case T_LINE: case T_RECT: case T_RRECT: case T_ELLIPSE:
        op_begin();
        preview_begin();
        op = OP_SHAPE;
        drag = (int8_t)b;
        draw_drag(0);
        break;
    case T_POLYGON:
        op_begin();
        preview_begin();
        ppx[0] = ppx[1] = (int16_t)x;
        ppy[0] = ppy[1] = (int16_t)y;
        pn = 2;
        op = OP_POLY;
        drag = (int8_t)b;
        click_frame = FRAME; click_x = mx; click_y = my;
        draw_poly(0);
        break;
    case T_CURVE:
        op_begin();
        preview_begin();
        for (int i = 0; i < 8; i += 2) { cv[i] = x; cv[i + 1] = y; }
        cv_stage = 1;
        op = OP_CURVE;
        drag = (int8_t)b;
        draw_curve();
        break;
    case T_SELECT:
        if (in_sel(x, y)) {
            if (sel == 1)
                sel_lift(right);
            else if (right)                 /* leave a copy behind */
                px_copy(DOC, fl_x, fl_y, FLOATB, 0, 0, fl_w, fl_h, fl_trans ? fl_bg : -1), fl_mark();
            grab_x = x - fl_x;
            grab_y = y - fl_y;
            op = OP_MOVE;
        } else {
            sel_drop();
            sel_x0 = sel_x1 = x < 0 ? 0 : x >= DW ? DW - 1 : x;
            sel_y0 = sel_y1 = y < 0 ? 0 : y >= DH ? DH - 1 : y;
            op = OP_MARQUEE;
        }
        drag = (int8_t)b;
        break;
    case T_ZOOM:
        view_zoom(right ? zoom / 2 : zoom * 2, mx, my);
        ui_draw_options();
        break;
    case T_HAND:
        op = OP_PAN;
        drag = (int8_t)b;
        break;
    }
}

static int clampx(int x) { return x < 0 ? 0 : x >= DW ? DW - 1 : x; }
static int clampy(int y) { return y < 0 ? 0 : y >= DH ? DH - 1 : y; }

/* the pointer moved by dx, dy (screen) with the buttons held */
static void on_motion(int dx, int dy, int moved)
{
    switch (op) {
    case OP_FREEHAND:
        if (moved) freehand_to(pdx, pdy);
        break;
    case OP_SHAPE:
        if (moved) { preview_erase(); draw_drag(0); }
        break;
    case OP_POLY:
        if (moved) {
            preview_erase();
            ppx[pn - 1] = (int16_t)pdx;
            ppy[pn - 1] = (int16_t)pdy;
            draw_poly(0);
        }
        break;
    case OP_CURVE:
        if (moved && drag >= 0) {
            preview_erase();
            if (cv_stage == 1) { cv[6] = pdx; cv[7] = pdy; cv[2] = cv[0]; cv[3] = cv[1]; cv[4] = pdx; cv[5] = pdy; }
            else if (cv_stage == 2) { cv[2] = cv[4] = pdx; cv[3] = cv[5] = pdy; }
            else { cv[4] = pdx; cv[5] = pdy; }
            draw_curve();
        }
        break;
    case OP_MARQUEE:
        if (moved) {
            int x = clampx(pdx), y = clampy(pdy);
            int x0 = clampx(sx0), y0 = clampy(sy0);
            sel_x0 = x0 < x ? x0 : x; sel_x1 = x0 < x ? x : x0;
            sel_y0 = y0 < y ? y0 : y; sel_y1 = y0 < y ? y : y0;
            sel = 1;
            sel_show();
            ui_coords(pdx, pdy, sel_x1 - sel_x0 + 1, sel_y1 - sel_y0 + 1);
        }
        break;
    case OP_MOVE:
        if (moved) {
            int nx = pdx - grab_x, ny = pdy - grab_y;
            if (nx != fl_x || ny != fl_y) {
                fl_mark();
                fl_x = nx;
                fl_y = ny;
                fl_mark();
                sel_show();
            }
        }
        break;
    case OP_PAN:
        if (dx || dy) view_pan(dx, dy);
        break;
    case OP_WMOVE: case OP_WGROW:
        if (dx || dy) wdrag_motion();
        break;
    case OP_THUMB:
        if (dx || dy) view_thumb_to(th_vert, (th_vert ? my : mx) - th_grab);
        break;
    case OP_MIDDLE:
        if ((mx - sx0) * (mx - sx0) + (my - sy0) * (my - sy0) > 16) {
            op = OP_PAN;
            view_pan(mx - sx0, my - sy0);
        }
        break;
    }
}

static void on_release(int b)
{
    if (b != drag)
        return;
    drag = -1;
    switch (op) {
    case OP_FREEHAND: case OP_SPRAY: op_end(); op = OP_NONE; break;
    case OP_SHAPE: preview_erase(); draw_drag(1); op_end(); op = OP_NONE; break;
    case OP_POLY: break;                    /* the next corner follows the pointer */
    case OP_CURVE:
        if (cv_stage == 1 && cv[0] == cv[6] && cv[1] == cv[7]) {   /* a click, no line */
            preview_erase();
            op_cancel();
            op = OP_NONE;
        } else if (++cv_stage > 3)
            curve_finish();
        break;
    case OP_MARQUEE:
        op = OP_NONE;
        if (sel == 1 && sel_x0 == sel_x1 && sel_y0 == sel_y1) {
            sel = 0;
            sel_show();
        }
        break;
    case OP_PICK:
        op = OP_NONE;
        select_tool(st.prev_tool);
        break;
    case OP_MIDDLE:
        op = OP_NONE;
        view_full(!full_view);
        break;
    case OP_MOVE: case OP_PAN: case OP_THUMB: op = OP_NONE; break;
    case OP_WMOVE: case OP_WGROW: wdrag_end(); break;
    }
}

/* ---- moving and sizing windows: live, a frame at a time ---- */
static int wd_win, wd_gx, wd_gy, wd_x, wd_y, wd_w, wd_h;   /* the window, the grab, where it began */

static void wdrag_begin(int i, int grow)
{
    wd_win = i;
    wd_x = wins[i].x; wd_y = wins[i].y; wd_w = wins[i].w; wd_h = wins[i].h;
    wd_gx = grow ? mx - wd_w : mx - wd_x;
    wd_gy = grow ? my - wd_h : my - wd_y;
    op = grow ? OP_WGROW : OP_WMOVE;
}

/* the window follows the pointer, a frame at a time */
static void wdrag_motion(void)
{
    win_t *o = &wins[wd_win];
    int x = o->x, y = o->y, w = o->w, h = o->h;
    if (op == OP_WMOVE) {
        x = mx - wd_gx;
        y = my - wd_gy;
        if (y < MENU_H + 1) y = MENU_H + 1;
        if (y > DH - 16) y = DH - 16;
        if (x < 40 - w) x = 40 - w;
        win_snap(&x, &w);
        if (x != o->x || y != o->y)
            win_drag_to(wd_win, x, y);
    } else {
        w = mx - wd_gx;
        h = my - wd_gy;
        if (w < 140) w = 140;
        if (h < 90) h = 90;
        if (x + w > DW + 1) w = DW + 1 - x;
        if (y + h > DH) h = DH - y;
        win_snap(&x, &w);
        if (w != o->w || h != o->h)
            win_size_to(wd_win, w, h);
    }
}

static void wdrag_end(void)
{
    op = OP_NONE;
}

/* a press on a scroll bar: the thumb is dragged, the track pages */

static void bar_press(int vertical)
{
    int p, t, start, len;
    view_thumb(vertical, &p, &t, &start, &len);
    int m = vertical ? my : mx;
    if (m >= p && m < p + t) {
        th_vert = vertical;
        th_grab = m - p;
        op = OP_THUMB;
        return;
    }
    int page = (vertical ? vh : vw) * 3 / 4 / zoom, d = m < p ? -page : page;
    if (vertical)
        view_scroll_to(ox, oy + d);
    else
        view_scroll_to((ox + d) & ~3, oy);
}

/* ---- the menus ---- */
static void menu_release(void)
{
    int i = menu_item_at(mx, my);
    if (i >= 0) {
        int c = menu_cmd(i);
        menu_close();
        op = OP_NONE;
        if (c) do_cmd(c);
        return;
    }
    if (menu_title_at(mx) == menu_cur && my <= MENU_H) {
        menu_drag = 0;                      /* a click on the title: it stays open */
        return;
    }
    menu_close();
    op = OP_NONE;
}

static void open_menu(int m)
{
    finish_ops();
    if (m == MENU_HELP) {                   /* Help: no menu */
        menu_close();
        do_cmd(CMD_HELP);
        return;
    }
    menu_open(m);
    op = OP_MENU;
}

static void on_press(int b)
{
    if (op == OP_HELP) {
        op = OP_NONE;
        ignore |= (uint8_t)(1 << b);
        ui_draw_all();
        return;
    }
    if (op == OP_MENU) {
        if (menu_item_at(mx, my) >= 0) {
            menu_drag = 1;                  /* chosen on the release */
            return;
        }
        int m = menu_title_at(mx);
        if (my <= MENU_H && m >= 0 && m != menu_cur) {
            open_menu(m);
            menu_drag = 1;
            return;
        }
        menu_close();
        op = OP_NONE;
        ignore |= (uint8_t)(1 << b);
        return;
    }
    if (b == 2) {                           /* the middle button: pan, or show the picture */
        if (drag < 0 && (op == OP_NONE || op == OP_POLY || op == OP_CURVE || op == OP_TEXT)) {
            if (op != OP_NONE) finish_ops();
            sx0 = mx; sy0 = my;
            op = OP_MIDDLE;
            drag = 2;
        }
        return;
    }
    if (drag >= 0)
        return;                             /* one operation at a time */
    int idx, w, hit = ui_hit(mx, my, &idx, &w);
    if (w >= 0 && !full_view && op == OP_NONE) {
        win_raise(w);
        if (w >= WIN_PIC0)
            view_use(w - WIN_PIC0);
    }
    switch (hit) {
    case HIT_TITLE: drag = (int8_t)b; wdrag_begin(w, 0); break;
    case HIT_GROW: drag = (int8_t)b; wdrag_begin(w, 1); break;
    case HIT_CLOSE: close_win(w); break;
    case HIT_VSCROLL: case HIT_HSCROLL:
        if (op == OP_NONE) {
            drag = (int8_t)b;
            bar_press(hit == HIT_VSCROLL);
            if (op != OP_THUMB) drag = -1;
        }
        break;
    case HIT_MENU: open_menu(idx); menu_drag = 1; break;
    case HIT_TOOL: select_tool(idx); break;
    case HIT_OPT_TOP: set_option(1, idx); break;
    case HIT_OPT_ROW: set_option(0, idx); break;
    case HIT_COLOUR:
        if (b) st.bg = (uint8_t)idx; else st.fg = (uint8_t)idx;
        colours_changed();
        break;
    case HIT_PATTERN: st.pat = (uint8_t)idx; colours_changed(); break;
    case HIT_SWAP: { uint8_t t = st.fg; st.fg = st.bg; st.bg = t; colours_changed(); break; }
    case HIT_CANVAS:
        if (op == OP_NONE) {                /* (the view may have changed: where the pointer is in it) */
            pdx = to_doc_x(mx);
            pdy = to_doc_y(my);
        }
        canvas_press(b);
        break;
    }
}

static void on_wheel(int d)
{
    if (op == OP_MENU || op == OP_HELP || drag >= 0 || !d)
        return;
    int idx, w, hit = ui_hit(mx, my, &idx, &w);
    int s = d > 0 ? -1 : 1;                 /* away from you: up the list */
    if ((hit == HIT_VSCROLL || hit == HIT_HSCROLL) && op == OP_NONE)
        view_use(w - WIN_PIC0);
    switch (hit) {
    case HIT_VSCROLL: view_scroll_to(ox, oy + s * (32 / zoom > 1 ? 32 / zoom : 1)); break;
    case HIT_HSCROLL: view_scroll_to((ox + s * (32 / zoom > 4 ? 32 / zoom : 4)) & ~3, oy); break;
    case HIT_CANVAS: {
        int z = zoom;
        for (int i = d; i > 0; i--) if (z < 8) z *= 2;
        for (int i = d; i < 0; i++) if (z > 1) z /= 2;
        view_zoom(z, mx, my);
        if (st.tool == T_ZOOM) ui_draw_options();
        break;
    }
    case HIT_COLOUR: st.fg = (uint8_t)((st.fg + s) & 7); colours_changed(); break;
    case HIT_SWAP: st.bg = (uint8_t)((st.bg + s) & 7); colours_changed(); break;
    case HIT_PATTERN: st.pat = (uint8_t)((st.pat + s) & (NPAT - 1)); colours_changed(); break;
    case HIT_TOOL: select_tool((st.tool + 2 * s) & (NTOOLS - 1)); break;
    case HIT_OPT_TOP: case HIT_OPT_ROW: step_option(s); break;
    }
}

/* ---- the keyboard ---- */
static const char tool_keys[NTOOLS] = { 's', 'i', 'p', 'b', 'a', 'x', 'f', 't', 'l', 'c', 'r', 'o', 'e', 'g', 'z', 'h' };

static void step_size(int d)
{
    step_option(d);
}

static void on_key(uint16_t k)
{
    if (k & 0x8000)
        return;
    if (op == OP_HELP) {
        op = OP_NONE;
        ui_draw_all();
        return;
    }
    if (op == OP_MENU) {
        if (k == 27) { menu_close(); op = OP_NONE; }
        return;
    }
    if (op == OP_TEXT && k != 27 && ((k >= 32 && k < 127) || k == 13 || k == 8)) {
        text_key(k);
        return;
    }
    if (k >= 'A' && k <= 'Z')
        k = (uint16_t)(k + 32);
    switch (k) {
    case 27:
        if (full_view && op == OP_NONE) { view_full(0); break; }
        if (op != OP_NONE) cancel_ops();
        else if (sel) sel_drop();
        break;
    case 9: do_cmd(CMD_FULL); break;
    case 26: do_cmd(CMD_UNDO); break;
    case 25: do_cmd(CMD_REDO); break;
    case 24: if (cmd_enabled(CMD_CUT)) do_cmd(CMD_CUT); break;
    case 3: if (cmd_enabled(CMD_COPY)) do_cmd(CMD_COPY); break;
    case 22: if (cmd_enabled(CMD_PASTE)) do_cmd(CMD_PASTE); break;
    case 0x7F: case 8: if (cmd_enabled(CMD_CLEAR)) do_cmd(CMD_CLEAR); break;
    case 1: do_cmd(CMD_SELALL); break;
    case 14: do_cmd(CMD_NEW); break;
    case 7: do_cmd(CMD_GRID); break;
    case 6: do_cmd(CMD_FLIPH); break;
    case 10: do_cmd(CMD_FLIPV); break;
    case 5: do_cmd(CMD_INVERT); break;
    case 18: do_cmd(CMD_REPLACE); break;
    case 0x90: do_cmd(CMD_HELP); break;
    case '+': case '=': do_cmd(CMD_ZOOMIN); break;
    case '-': case '_': do_cmd(CMD_ZOOMOUT); break;
    case '[': step_size(-1); break;
    case ']': step_size(1); break;
    case 0x80: view_pan(0, 32); break;
    case 0x81: view_pan(0, -32); break;
    case 0x82: view_pan(32, 0); break;
    case 0x83: view_pan(-32, 0); break;
    default:
        if (k >= '0' && k <= '7') {
            if (kbd_mod() & 1) st.bg = (uint8_t)(k - '0'); else st.fg = (uint8_t)(k - '0');
            colours_changed();
            break;
        }
        for (int i = 0; i < NTOOLS; i++)
            if (tool_keys[i] == k) {
                select_tool(i);
                break;
            }
    }
}

/* ---- the pointer's shape ---- */
static int pointer_shape(int *box)
{
    *box = 0;
    if (op == OP_MENU || op == OP_HELP)
        return CUR_ARROW;
    if (op == OP_PAN || op == OP_MIDDLE || op == OP_WMOVE)
        return CUR_HAND;
    int idx, w;
    if (op == OP_WGROW || op == OP_THUMB || ui_hit(mx, my, &idx, &w) != HIT_CANVAS)
        return CUR_ARROW;
    switch (st.tool) {
    case T_SELECT: return in_sel(pdx, pdy) ? CUR_HAND : CUR_CROSS;
    case T_PICKER: return CUR_PICK;
    case T_ERASER: *box = eraser_sizes[st.eraser_size] * zoom; return CUR_BOX;
    case T_BUCKET: return CUR_BUCKET;
    case T_TEXT: return CUR_IBEAM;
    case T_ZOOM: return CUR_ZOOM;
    case T_HAND: return CUR_HAND;
    }
    return CUR_CROSS;
}

int main(void)
{
    draw_init();
    VID_CTRL = 0x82;                        /* the bitmap, 16-colour (no text layer) */
    VID_BACKDROP = 0;
    VID_SCROLLX = 0;
    VID_SCROLLY = 0;
    screen_init();

    if (HEADER[0] != PAINT_MAGIC)
        buf_fill(DOC, WHITE);
    HEADER[0] = 0;
    HEADER[1] = PAINT_RUNNING;
    base_sync();
    undo_reset();

    st.tool = st.prev_tool = T_PENCIL;
    st.fg = BLACK;
    st.bg = WHITE;
    st.brush_size = 2;
    st.eraser_size = 2;
    st.spray_size = 1;
    tools_changed();
    win_init();
    view_init(0, 1);
    ui_draw_all();
    ui_message("Finding the mouse");
    frame_end(mx, my, CUR_ARROW, 0);        /* shown while the mouse is set up */
    frame_begin();

    ps2_init();
    irq_enable(1);
    int have_mouse = ps2_type[0] == PS2_MOUSE || ps2_type[1] == PS2_MOUSE;
    ui_message(!have_mouse ? "No mouse found" : ps2_wheel ? "Wheel mouse found" : "Mouse, no wheel");
    ps2_mouse_t m;
    ps2_mouse_read(&m);                     /* (anything from the set-up) */

    frame_end(mx, my, CUR_ARROW, 0);
#ifdef PERF
    uint32_t i0 = fcpu_instret();
#endif
    for (;;) {
#ifdef PERF
        i0 = fcpu_instret();
#endif
        frame_begin();
        ps2_service();

        ps2_mouse_read(&m);
        /* acceleration: faster moves go further */
        int sp = (m.dx < 0 ? -m.dx : m.dx) + (m.dy < 0 ? -m.dy : m.dy);
        int f = sp > 24 ? 4 : sp > 10 ? 3 : 2;   /* half pixels per count */
#ifdef NOACCEL
        f = 2;
#endif
        acc_x += m.dx * f;
        acc_y -= m.dy * f;
        int ddx = acc_x / 2, ddy = acc_y / 2;
        acc_x -= ddx * 2;
        acc_y -= ddy * 2;
        int omx = mx, omy = my;
        mx += ddx;
        my += ddy;
        if (mx < 0) mx = 0;
        if (mx > DW - 1) mx = DW - 1;
        if (my < 0) my = 0;
        if (my > DH - 1) my = DH - 1;
        int moved = mx != omx || my != omy;
        int opdx = pdx, opdy = pdy;
        if (op == OP_NONE && drag < 0) {    /* the view under the pointer is the current one */
            int v = view_at(mx, my);
            if (v >= 0)
                view_use(v);
        }
        pdx = to_doc_x(mx);
        pdy = to_doc_y(my);

        uint8_t pressed = m.pressed, now = m.buttons;
        uint8_t released = (uint8_t)((held | pressed) & ~now);
        for (int b = 0; b < 3; b++)
            if (pressed & (1 << b)) {
                if (ignore & (1 << b)) continue;
                on_press(b);
            }
        held = (uint8_t)((held | pressed) & now);
        if (op == OP_MENU) {
            if (moved) {
                int t = menu_title_at(mx);
                if (my <= MENU_H && t >= 0 && t != menu_cur && t != MENU_HELP)
                    open_menu(t);
                menu_hilite(menu_item_at(mx, my));
            }
        } else
            on_motion(mx - omx, my - omy, moved || pdx != opdx || pdy != opdy);
        if (op == OP_SPRAY) {
            paints(op_right);
            USE(paint_fill);
            int r = spray_sizes[st.spray_size];
            spray(pdx, pdy, r, r * r / 10 + 4);
        }
        for (int b = 0; b < 3; b++)
            if (released & (1 << b)) {
                if (ignore & (1 << b)) { ignore &= (uint8_t)~(1 << b); continue; }
                if (op == OP_MENU && menu_drag) menu_release();
                else on_release(b);
            }
        if (m.dz)
            on_wheel(m.dz);

        for (uint16_t k; (k = kbd_get()) != 0xFFFF;)
            on_key(k);

        int rendered = dirty_x1 >= 0;
        flush_dirty();
        if (op != OP_HELP) {
            overlays_update(rendered);
            if (op != OP_SHAPE && op != OP_MARQUEE && op != OP_MENU) {
                int idx, w;
                if (ui_hit(mx, my, &idx, &w) == HIT_CANVAS) ui_coords(pdx, pdy, -1, -1);
                else ui_coords(-1, -1, -1, -1);
            }
            ui_tick();
        }
        int box, shape = pointer_shape(&box);
#ifdef PERF
        {   /* PERF: frames over the Spartan-3's budget (3.75 MIPS: ~62,000 a frame) */
            uint32_t i1 = fcpu_instret();
            if (i1 - i0 > 62000)
                pnum("frame ", FRAME), pnum(": ", i1 - i0), pnum(" instructions, tool ", st.tool),
                pnum(" op ", op), pnum(" zoom ", (uint32_t)zoom), CON_OUT = 10;
        }
#endif
        frame_end(mx, my, shape, box);
    }
}
