/* paint: an 8-color paint program for the Spartan-3 (README.md).
 *
 * The picture (DOC) is a whole 640x360 bitmap in the 4-bit format of
 * docs/io.md, colours 0-7. The screen is a second bitmap: the menu bar,
 * the toolbox, the colour bar and a view of the picture, at 1x to 8x.
 * Drawing goes into DOC and marks a dirty rectangle; each frame the dirty
 * part is drawn (rendered) onto the screen at the view's zoom, and the
 * pointer goes on top with what it covers saved.
 *
 * Every change to DOC is an operation (op_begin .. op_end). BASE holds DOC
 * as it was before the operation, so a preview (a rubber-band line, a
 * shape) is undone from a list of the bytes it overwrote or, if that
 * overflows, from BASE. At op_end the changed rectangle of BASE goes into
 * the undo log, a ring of records in the rest of memory; undo and redo
 * swap a record with the picture.
 *
 * int is 16 bits: offsets into a bitmap (up to 115,200) are 32-bit. */
#ifndef PAINT_H
#define PAINT_H
#include <stdint.h>
#include <fcpu.h>

#define REG(a)      IO16(a)
#define VID_CTRL    REG(0xF00200)
#define VID_BACKDROP REG(0xF00202)
#define VID_BMP_LO  REG(0xF00208)
#define VID_BMP_HI  REG(0xF0020A)
#define VID_SCROLLX REG(0xF00214)
#define VID_SCROLLY REG(0xF00216)
#define LEDS        REG(0xF00600)
#define SWITCHES    REG(0xF00602)
#define SEG7        REG(0xF00606)
#define SEG7_CTRL   REG(0xF00608)

#define DW 640
#define DH 360
#define STRIDE 320
#define PICSZ 115200UL
#define BUFSTEP 0x1C200UL                /* = PICSZ */

/* memory: the program, its data and stack below SCRATCH_A; then the
 * bitmaps to the top of the 1 MB SRAM */
#define SCRATCH_A 0x01A000UL             /* the preview's list, the fill's mask, or what a menu covers */
#define SCRATCH_SZ 0x7100UL
#define HEADER_A  0x021100UL             /* for tools/paint.py (below) */
#define DOC_A     0x021200UL
#define SCREEN0_A (DOC_A + BUFSTEP)      /* the two screens: one shown, one drawn (view.c) */
#define SCREEN1_A (DOC_A + 2 * BUFSTEP)
#define FLOAT_A   (DOC_A + 3 * BUFSTEP)  /* the floating selection, at its 0,0 */
#define CLIP_A    (DOC_A + 4 * BUFSTEP)  /* the clipboard, at its 0,0 */
#define BASE_A    (DOC_A + 5 * BUFSTEP)  /* DOC before the operation */
#define LOG_A     (DOC_A + 6 * BUFSTEP)  /* the undo log: the rest, 1.9 bitmaps' worth */
#define LOG_SZ    (0x100000UL - LOG_A)

#define DOC    ((uint8_t *)DOC_A)
extern uint8_t *screen_back;            /* the screen being drawn (not shown) */
#define SCREEN screen_back
#define FLOATB ((uint8_t *)FLOAT_A)
#define CLIPB  ((uint8_t *)CLIP_A)
#define BASE   ((uint8_t *)BASE_A)

/* HEADER_A: tools/paint.py opens a picture by loading it into DOC with
 * PAINT_MAGIC in word 0; the program then keeps it (and clears the word).
 * Word 1 is PAINT_MAGIC while the program runs, for paint.py save. */
#define HEADER ((volatile uint32_t *)HEADER_A)
#define PAINT_MAGIC 0x31544E50UL         /* "PNT1" */
#define PAINT_RUNNING 0x4E555250UL       /* "PRUN" */

enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE, TRANSP, NAVY, DGREEN, TEAL, MAROON,
       PURPLE, OLIVE, GREY };

#define COLD __attribute__((noinline))

/* ---- draw.c: the picture ---- */
extern uint32_t rowoff[DH];             /* y * 320 */
static inline uint8_t getpx(const uint8_t *b, int x, int y)
{
    uint8_t v = b[rowoff[y] + (unsigned)(x >> 1)];
    return (x & 1) ? (uint8_t)(v & 15) : (uint8_t)(v >> 4);
}
static inline void setpx(uint8_t *b, int x, int y, uint8_t c)
{
    uint8_t *p = b + rowoff[y] + (unsigned)(x >> 1);
    *p = (x & 1) ? (uint8_t)((*p & 0xF0) | c) : (uint8_t)((*p & 0x0F) | c << 4);
}

#define NPAT 16
extern const uint8_t patterns[NPAT][8];
typedef uint8_t paint_t[8][4];          /* [y & 7][(x >> 1) & 3]: a byte of two pixels */
extern paint_t paint_line, paint_fill, paint_back;
extern const uint8_t (*cur_paint)[4];
void paint_make(paint_t p, uint8_t fg, uint8_t bg, int pat);
#define USE(p) (cur_paint = (const uint8_t (*)[4])(p))

/* the dirty rectangle (to render) */
extern int dirty_x0, dirty_y0, dirty_x1, dirty_y1;
void mark(int x0, int y0, int x1, int y1);
void mark_all(void);

void draw_init(void);
void span(int x0, int x1, int y);       /* inclusive, clipped, in cur_paint */
static inline void dot(int x, int y) { span(x, x, y); }
void line1(int x0, int y0, int x1, int y1);

enum { BR_ROUND, BR_SQUARE, BR_SLASH, BR_BACK };
#define MAXBR 32
typedef struct { uint8_t n; int8_t l[MAXBR], r[MAXBR]; } brush_t;
void brush_make(brush_t *b, int shape, int d);
void stamp(const brush_t *b, int cx, int cy);
void stroke(const brush_t *b, int x0, int y0, int x1, int y1);   /* stamps along a line, not the first point */
void line_w(int x0, int y0, int x1, int y1, int w);              /* a line w wide, round ends */

enum { SH_RECT, SH_RRECT, SH_ELLIPSE };
enum { FM_OUTLINE, FM_FILLED, FM_FRAMED };
void shape(int kind, int x0, int y0, int x1, int y1, int w, int mode);  /* paint_line, paint_fill, paint_back */
#define MAXPOLY 64
void poly_fill16(int n, const int16_t *xs, const int16_t *ys);   /* 1/16 pixel units, even-odd */
void polygon(int n, const int16_t *xs, const int16_t *ys, int w, int mode, int closed);
void bezier(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int w);
void flood(int x, int y);               /* in cur_paint */
void spray(int x, int y, int r, int n);
void glyph_draw(int x, int y, uint8_t ch, int scale, int opaque);  /* paint_line on paint_back */
int16_t isqrt32(uint32_t v);

/* operations and the undo log */
void op_begin(void);
void op_end(void);
void op_cancel(void);                   /* put DOC back as it was at op_begin */
extern uint8_t op_active;
int undo(void);
int redo(void);
int can_undo(void);
int can_redo(void);
void undo_reset(void);
void base_sync(void);                   /* BASE = DOC (after a whole-picture change outside an operation) */

/* previews: what is drawn between preview_begin and preview_erase is taken back */
void preview_begin(void);
void preview_erase(void);
void preview_end(void);                 /* keep what was drawn */

/* rectangles of pixels between bitmaps (any alignment); tr: a colour not
 * copied (-1 none) */
void fastcopy(uint8_t *d, const uint8_t *s, unsigned n);
void row_copy(uint8_t *dst, int dx, const uint8_t *src, int sx, int n, int tr);
void px_copy(uint8_t *dst, int dx, int dy, const uint8_t *src, int sx, int sy, int w, int h, int tr);
void px_fill(uint8_t *dst, int x, int y, int w, int h, uint8_t c);
void px_flip_h(uint8_t *b, int x, int y, int w, int h);
void px_flip_v(uint8_t *b, int x, int y, int w, int h);
void px_map(uint8_t *b, int x, int y, int w, int h, const uint8_t *map);   /* c = map[c] */
void buf_copy(uint8_t *dst, const uint8_t *src);   /* a whole bitmap */
void buf_fill(uint8_t *dst, uint8_t c);

/* ---- win.c: windows, and the clipping of everything drawn on the screen ---- */
#define MENU_H 14
typedef struct { int16_t x0, y0, x1, y1; } rect_t;     /* inclusive */
#define MAXCLIP 24
typedef struct { uint8_t n; rect_t r[MAXCLIP]; } region_t;
extern const region_t *clip;            /* screen drawing (sfill, stext, spx ...) stays in it */
extern region_t r_screen;

enum { WIN_TOOLS, WIN_COLOURS, WIN_PIC0, WIN_PIC1, WIN_PIC2, NWIN };
#define NVIEW 3
typedef struct { int x, y, w, h; uint8_t open; } win_t;   /* the outer rectangle, shadow included */
extern win_t wins[NWIN];
/* the content of a window: inside the frame, below the title bar */
#define WIN_CX(i) (wins[i].x + 1)
#define WIN_CY(i) (wins[i].y + 15)
#define WIN_CW(i) (wins[i].w - 3)
#define WIN_CH(i) (wins[i].h - 17)
#define SB 11                           /* a scroll bar's width */

void win_init(void);                    /* the default layout */
void win_reset(int i);                  /* window i where it starts */
void win_layout(void);                  /* the visible regions, after a change */
void win_draw_all(void);
void win_draw_rect(int x0, int y0, int x1, int y1);   /* the desktop and the windows there */
void win_draw(int i);
void win_frame(int i);
void win_raise(int i);
void win_open(int i);
void win_close(int i);
int win_front_pic(void);                /* the active picture window */
void win_move(int i, int x, int y, int w, int h);
void win_drag_to(int i, int x, int y);  /* live: the window follows the pointer */
void win_size_to(int i, int w, int h);
void win_snap(int *x, int *w);          /* x and width on the grid the views need */
enum { PART_NONE, PART_TITLE, PART_CLOSE, PART_GROW, PART_VSCROLL, PART_HSCROLL, PART_CONTENT };
int win_at(int x, int y, int *part);    /* the window there, -1: the desktop */
void clip_win(int i);
const region_t *win_region(int i);     /* its visible part */
void clip_screen(void);
void clip_and(const region_t *g, int x0, int y0, int x1, int y1);   /* clip = g within the rectangle */
int in_clip(int x, int y);

/* ---- view.c: the views of the picture, and the screen ---- */
/* the current view (one picture window's, or the full screen's) */
extern int vx, vy, vw, vh;              /* the view on the screen */
extern int zoom, ox, oy;                /* zoom 1/2/4/8; the picture's pixel at the view's corner */
extern int cur_view;
extern uint8_t grid;
extern uint8_t full_view;
/* the floating selection, drawn over the picture */
extern uint8_t fl_on, fl_trans, fl_bg;
extern int fl_x, fl_y, fl_w, fl_h;

void view_init(int v, int z);           /* from its window; centred */
void view_use(int v);                   /* make view v the current one */
void view_fit(int v);                   /* its window moved or changed size */
void view_moved(int v, int dx, int dy); /* its window's pixels were moved on the screen */
void view_grown(int v, int ow, int oh); /* its window changed size: draw what is new (ow, oh: the old view size) */
void view_clamp(void);
void view_zoom(int z, int sx, int sy);  /* keep the picture's point under sx,sy */
void view_pan(int dx, int dy);          /* in screen pixels */
void view_scroll_to(int nox, int noy);
void view_full(int on);
void view_draw_bars(void);
int view_zoom_of(int v);
void view_thumb(int vertical, int *p, int *t, int *start, int *len);   /* the scroll bar's thumb */
void view_thumb_to(int vertical, int p);              /* the scroll bars and the grow box */
int view_at(int sx, int sy);            /* the view whose picture is there, -1 none */
int to_doc_x(int sx);
int to_doc_y(int sy);
int to_scr_x(int dx);
int to_scr_y(int dy);
int in_view(int sx, int sy);
void render(int sx0, int sy0, int sx1, int sy1);   /* screen rectangle, inclusive */
void render_doc(int x0, int y0, int x1, int y1);   /* picture rectangle */
void render_view(void);
void flush_dirty(void);
void spx(int x, int y, uint8_t c);      /* a screen pixel */

/* the screens are double-buffered: everything is drawn into the back
 * one, which is shown at the next vertical blank; what was drawn (damage)
 * is copied into the other one at the start of the next frame. The pointer
 * is put on the back screen last of all, and taken off it first. */
void screen_init(void);
void damage(int x0, int y0, int x1, int y1);
void frame_begin(void);                 /* the back screen up to date, without the pointer */
void frame_end(int x, int y, int shape, int boxsize);   /* the pointer; wait for the blank; show */
enum { CUR_ARROW, CUR_CROSS, CUR_IBEAM, CUR_HAND, CUR_ZOOM, CUR_PICK, CUR_BUCKET, CUR_BOX, CUR_WAIT };
static inline void cursor_hide(void) {}
static inline void cursor_guard(int x0, int y0, int x1, int y1) { (void)x0; (void)y0; (void)x1; (void)y1; }

/* overlays: the selection's outline and the text caret, on the screen only */
void marquee_set(int on, int x0, int y0, int x1, int y1);   /* picture rectangle, inclusive */
void caret_set(int on, int x, int y, int h);
void overlays_update(int redraw);      /* after rendering */
void overlays_forget(void);             /* the current view was drawn afresh */

/* ---- ui.c ---- */
enum { T_SELECT, T_PICKER, T_PENCIL, T_BRUSH, T_SPRAY, T_ERASER, T_BUCKET, T_TEXT,
       T_LINE, T_CURVE, T_RECT, T_RRECT, T_ELLIPSE, T_POLYGON, T_ZOOM, T_HAND, NTOOLS };
extern const char *const tool_names[NTOOLS];

#define TOOLS_W 56                      /* the palettes' outer sizes */
#define TOOLS_H 335
#define COLOURS_W 212
#define COLOURS_H 75

const uint8_t *glyph(uint8_t ch);
void stext(int x, int y, const char *s, uint8_t fg, uint8_t bg);   /* x even; bg TRANSP: none */
void sfill(int x0, int y0, int x1, int y1, uint8_t c);              /* screen, inclusive */
void sframe(int x0, int y0, int x1, int y1, uint8_t c);
void ui_draw_all(void);
void ui_draw_menubar(void);
void ui_draw_menubar_right(void);      /* the status and the tool's name */
void ui_tool_changed(void);
void ui_draw_tools(void);
void ui_draw_options(void);
void ui_draw_colours(void);
void ui_status(const char *s);
void ui_message(const char *s);        /* shown for two seconds */
void ui_coords(int x, int y, int w, int h);   /* w < 0: no size */
void ui_tick(void);

/* hit-testing */
enum { HIT_NONE, HIT_MENU, HIT_TOOL, HIT_OPT_TOP, HIT_OPT_ROW, HIT_COLOUR, HIT_PATTERN, HIT_SWAP, HIT_CANVAS,
       HIT_DESK, HIT_TITLE, HIT_CLOSE, HIT_GROW, HIT_VSCROLL, HIT_HSCROLL, HIT_WIN };
int ui_hit(int x, int y, int *idx, int *win);

/* the menus */
int menu_title_at(int x);              /* -1: none */
void menu_open(int m);
void menu_close(void);
int menu_item_at(int x, int y);        /* -1: none */
void menu_hilite(int i);
int menu_cmd(int i);                    /* the command of the open menu's item i, 0 if disabled */
extern int menu_cur;
#define MENU_HELP 5                     /* the menu that is only a title */
void help_show(void);

/* ---- main.c: the state the UI shows ---- */
typedef struct {
    uint8_t tool, prev_tool;
    uint8_t fg, bg, pat;
    uint8_t brush_shape, brush_size;    /* index into the size lists */
    uint8_t eraser_size, spray_size, line_w, fill_mode;
    uint8_t text_opaque, text_scale;
    uint8_t sel_trans;
    uint8_t has_clip;
} state_t;
extern state_t st;
extern const uint8_t brush_sizes[6], eraser_sizes[5], spray_sizes[4], line_widths[5];

enum {
    CMD_NONE, CMD_NEW, CMD_NEWBG, CMD_FULL, CMD_HELP,
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_CLEAR, CMD_SELALL, CMD_TRANS,
    CMD_FLIPH, CMD_FLIPV, CMD_ROT180, CMD_INVERT, CMD_REPLACE, CMD_CLEARPIC,
    CMD_ZOOMIN, CMD_ZOOMOUT, CMD_ZOOM1, CMD_GRID,
    CMD_WTOOLS, CMD_WCOLOURS, CMD_WNEW, CMD_WTIDY,
};
int cmd_enabled(int cmd);
int cmd_checked(int cmd);
#endif
