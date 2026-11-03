/* Section 6: a desktop with a paint program ("SketchPad"), a scripted session.
 *
 * BUF0 is the bitmap on screen. BUF1 is a clean copy of it (everything but
 * the pointer and the rubber-band outline), so the pointer and the outline
 * are erased by copying from BUF1; BUF2 holds what a drop-down menu covers.
 * Everything that is drawn for good goes into BUF0 and is then copied to
 * BUF1 (dt_sync). The script is a table of steps timed in frames; the
 * pointer eases between waypoints, drags are rubber bands, strokes are
 * drawn a segment a frame, and fills (pattern spans, the paint bucket's
 * flood fill) run a few rows or spans a frame. */

#define DT_CX 100                              /* canvas border: x, y, w, h */
#define DT_CY 47
#define DT_CW 458
#define DT_CH 254

enum { DT_WAIT, DT_MOVE, DT_TOOL, DT_PAT, DT_MENU, DT_DRAG, DT_STROKE, DT_FILL, DT_TEXT };
enum { DT_SEL, DT_LASSO, DT_HAND, DT_TXT, DT_BUCKET, DT_SPRAY, DT_BRUSH, DT_PENCIL, DT_LINE,
       DT_ERASER, DT_RECT, DT_FRECT, DT_OVAL, DT_FOVAL, DT_RRECT, DT_FREE };

typedef struct { uint8_t op; int16_t dur, a, b, c, d, e; } dt_step_t;

#define DT_AMP(a, f) ((a) | (f) << 8)
static const dt_step_t dt_script[] = {
    { DT_WAIT, 10, 0, 0, 0, 0, 0 },
    /* the File menu */
    { DT_MOVE, 55, 48, 8, 0, 0, 0 },
    { DT_MENU, 20, 1, 0, 0, 0, 0 },
    { DT_MOVE, 40, 74, 92, 0, 0, 0 },
    { DT_MOVE, 30, 74, 30, 0, 0, 0 },
    { DT_WAIT, 10, 0, 0, 0, 0, 0 },
    { DT_MENU, 10, 0, 0, 0, 0, 0 },
    /* a filled rectangle */
    { DT_MOVE, 38, 82, 190, 0, 0, 0 },
    { DT_TOOL, 8, DT_FRECT, 0, 0, 0, 0 },
    { DT_MOVE, 34, 393, 316, 0, 0, 0 },
    { DT_PAT, 8, 10, 0, 0, 0, 0 },
    { DT_MOVE, 36, 116, 64, 0, 0, 0 },
    { DT_DRAG, 44, 216, 124, 0, 0, 0 },
    { DT_WAIT, 22, 0, 0, 0, 0, 0 },
    /* a filled oval */
    { DT_MOVE, 34, 82, 216, 0, 0, 0 },
    { DT_TOOL, 8, DT_FOVAL, 0, 0, 0, 0 },
    { DT_MOVE, 30, 197, 316, 0, 0, 0 },
    { DT_PAT, 8, 3, 0, 0, 0, 0 },
    { DT_MOVE, 36, 250, 62, 0, 0, 0 },
    { DT_DRAG, 44, 350, 134, 1, 0, 0 },
    { DT_WAIT, 22, 0, 0, 0, 0, 0 },
    /* pencil */
    { DT_MOVE, 34, 82, 138, 0, 0, 0 },
    { DT_TOOL, 8, DT_PENCIL, 0, 0, 0, 0 },
    { DT_MOVE, 34, 116, 170, 0, 0, 0 },
    { DT_STROKE, 100, 116, 170, 300, 170, DT_AMP(14, 4) },
    { DT_WAIT, 6, 0, 0, 0, 0, 0 },
    /* brush */
    { DT_MOVE, 26, 56, 138, 0, 0, 0 },
    { DT_TOOL, 8, DT_BRUSH, 0, 0, 0, 0 },
    { DT_MOVE, 40, 330, 170, 0, 0, 0 },
    { DT_STROKE, 80, 330, 170, 540, 170, DT_AMP(14, 2) },
    { DT_WAIT, 6, 0, 0, 0, 0, 0 },
    /* a triangle with the line tool */
    { DT_MOVE, 26, 56, 164, 0, 0, 0 },
    { DT_TOOL, 8, DT_LINE, 0, 0, 0, 0 },
    { DT_MOVE, 45, 480, 64, 0, 0, 0 },
    { DT_DRAG, 32, 430, 126, 2, 0, 0 },
    { DT_DRAG, 32, 530, 126, 2, 0, 0 },
    { DT_DRAG, 32, 480, 64, 2, 0, 0 },
    { DT_WAIT, 4, 0, 0, 0, 0, 0 },
    /* the paint bucket */
    { DT_MOVE, 36, 56, 112, 0, 0, 0 },
    { DT_TOOL, 8, DT_BUCKET, 0, 0, 0, 0 },
    { DT_MOVE, 36, 480, 102, 0, 0, 0 },
    { DT_FILL, 34, 480, 102, RED, 0, 0 },
    /* spray can */
    { DT_MOVE, 34, 82, 112, 0, 0, 0 },
    { DT_TOOL, 8, DT_SPRAY, 0, 0, 0, 0 },
    { DT_MOVE, 30, 130, 225, 0, 0, 0 },
    { DT_STROKE, 70, 130, 225, 190, 235, DT_AMP(12, 3) },
    /* text */
    { DT_MOVE, 30, 82, 86, 0, 0, 0 },
    { DT_TOOL, 8, DT_TXT, 0, 0, 0, 0 },
    { DT_MOVE, 32, 240, 246, 0, 0, 0 },
    { DT_TEXT, 86, 240, 246, 0, 0, 4 },
    /* the eraser */
    { DT_MOVE, 30, 82, 164, 0, 0, 0 },
    { DT_TOOL, 8, DT_ERASER, 0, 0, 0, 0 },
    { DT_MOVE, 38, 530, 196, 0, 0, 0 },
    { DT_STROKE, 56, 530, 196, 380, 172, DT_AMP(6, 2) },
    /* a document to the trash */
    { DT_MOVE, 35, 600, 150, 0, 0, 0 },
    { DT_DRAG, 60, 600, 274, 3, 0, 0 },
    { DT_WAIT, 120, 0, 0, 0, 0, 0 },
};
#define DT_NSTEPS ((int)(sizeof dt_script / sizeof dt_script[0]))

static const char dt_msg[] = "Hello from fpga_cpu";
static const char *const dt_mi[6] = { "New", "Open...", "Close", "Save", "Save As...", "Quit" };

/* 8x8 patterns, a set bit is black; kept also as the bytes of a bitmap row */
static const uint8_t dt_pat[16][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x80, 0, 0x08, 0, 0x80, 0, 0x08, 0 },
    { 0x88, 0, 0x22, 0, 0x88, 0, 0x22, 0 },
    { 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22 },
    { 0xAA, 0, 0xAA, 0, 0xAA, 0, 0xAA, 0 },
    { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 },
    { 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77 },
    { 0xFF, 0xAA, 0xFF, 0x55, 0xFF, 0xAA, 0xFF, 0x55 },
    { 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0 },
    { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC },
    { 0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11 },
    { 0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88 },
    { 0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08 },
    { 0xFF, 0x88, 0x88, 0x88, 0xFF, 0x88, 0x88, 0x88 },
    { 0x3C, 0x7E, 0x7E, 0x3C, 0, 0, 0, 0 },
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
};
static uint8_t dt_pat4[16][8][4];              /* [pattern][row][byte of 2 pixels] */

static const char dt_art_arrow[] =
    "0..........."
    "00.........."
    "070........."
    "0770........"
    "07770......."
    "077770......"
    "0777770....."
    "07777770...."
    "077777770..."
    "0777777770.."
    "07777777770."
    "077777000000"
    "0770770....."
    "070.0770...."
    "00..0770...."
    "0....0770..."
    ".....0770..."
    "......00....";
static const char dt_art_cross[] =
    ".....0....."
    ".....0....."
    ".....0....."
    ".....0....."
    ".....0....."
    "00000000000"
    ".....0....."
    ".....0....."
    ".....0....."
    ".....0....."
    ".....0.....";
static const char dt_art_ibeam[] =
    "0000000"
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "...0..."
    "0000000";
static const char dt_art_pen[] =
    ".......00."
    "......0660"
    ".....0660."
    "....0660.."
    "...0660..."
    "..0660...."
    ".0660....."
    "0660......"
    "00........";

static sprite_t *dt_sp_arrow, *dt_sp_cross, *dt_sp_ibeam, *dt_sp_pen;

/* ---- state --------------------------------------------------------------------- */
static int dt_px, dt_py;                       /* the pointer */
static int dt_sx, dt_sy;                       /* where the step's motion began */
static int dt_lx, dt_ly;                       /* the stroke's last point */
static int dt_plv, dt_plx, dt_ply, dt_plw, dt_plh;   /* pointer drawn: rect to restore */
static int dt_hide;
static int dt_tool, dt_psel;
static int dt_mopen, dt_mitem;
static int dt_gk, dt_gx, dt_gy, dt_gw, dt_gh;  /* rubber band: 1 outline rect, 2 box */
static int dt_si, dt_tn;
static uint16_t dt_sf;

/* background jobs: 1 rectangle of pattern, 2 oval, 3 flood fill */
static int dt_jk, dt_jx0, dt_jx1, dt_jy, dt_jy1, dt_jp;
static int dt_jcx, dt_jcy, dt_jhw, dt_jdy;
static int32_t dt_jrx2, dt_jry2, dt_jlim;
static int16_t *dt_stk;
static int dt_sp, dt_fcol;

/* out-of-line wrappers: the compiler would inline the library into every use */
#define DT_NI __attribute__((noinline))
DT_NI static void dt_rect(int x, int y, int w, int h, uint8_t c) { rect(x, y, w, h, c); }
DT_NI static void dt_fill_rect(int x, int y, int w, int h, uint8_t c) { fill_rect(x, y, w, h, c); }
DT_NI static void dt_hline(int x0, int x1, int y, uint8_t c) { hline(x0, x1, y, c); }
DT_NI static void dt_vline(int x, int y0, int y1, uint8_t c) { vline(x, y0, y1, c); }
DT_NI static void dt_line(int x0, int y0, int x1, int y1, uint8_t c) { line(x0, y0, x1, y1, c); }
DT_NI static void dt_pset(int x, int y, uint8_t c) { pset(x, y, c); }
DT_NI static uint8_t dt_pget(int x, int y) { return pget(x, y); }
DT_NI static void dt_ellipse(int cx, int cy, int rx, int ry, uint8_t c, int f) { ellipse(cx, cy, rx, ry, c, f); }
DT_NI static int dt_draw_text(int x, int y, const char *s, uint8_t fg, uint8_t bg, int sc) { return draw_text(x, y, s, fg, bg, sc); }
DT_NI static void dt_draw_char(int x, int y, uint8_t ch, uint8_t fg, uint8_t bg, int sc) { draw_char(x, y, ch, fg, bg, sc); }
DT_NI static void dt_copy_rect(uint32_t d, uint32_t s, int x, int y, int w, int h) { copy_rect(d, s, x, y, w, h); }

/* ---- helpers ----------------------------------------------------------------------- */
static void dt_sync(int x, int y, int w, int h) { dt_copy_rect(BUF1, BUF0, x, y, w, h); }

static void dt_syncb(int xa, int ya, int xb, int yb, int pad)   /* any two corners */
{
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    if (ya > yb) { int t = ya; ya = yb; yb = t; }
    dt_sync(xa - pad, ya - pad, xb - xa + 2 * pad + 1, yb - ya + 2 * pad + 1);
}

static void dt_pat_init(void)
{
    for (int p = 0; p < 16; p++)
        for (int r = 0; r < 8; r++)
            for (int k = 0; k < 4; k++) {
                uint8_t v = 0;
                for (int j = 0; j < 2; j++) {
                    int bit = (dt_pat[p][r] >> (7 - 2 * k - j)) & 1;
                    v = (uint8_t)(v << 4 | (bit ? BLACK : WHITE));
                }
                dt_pat4[p][r][k] = v;
            }
}

/* pixels x0..x1 of row y in pattern p (anchored to the screen) */
static void dt_span(int x0, int x1, int y, int p)
{
    if (x0 > x1) return;
    uint8_t *r = rowp(y);
    const uint8_t *t = dt_pat4[p][y & 7];
    if (x0 & 1) { r[x0 >> 1] = (uint8_t)((r[x0 >> 1] & 0xF0) | (t[(x0 >> 1) & 3] & 0x0F)); x0++; }
    if (!(x1 & 1)) { r[x1 >> 1] = (uint8_t)((r[x1 >> 1] & 0x0F) | (t[(x1 >> 1) & 3] & 0xF0)); x1--; }
    for (int b = x0 >> 1; b <= (x1 >> 1); b++) r[b] = t[b & 3];
}

/* swap black and white */
static void dt_invert(int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++) {
        uint8_t *p = rowp(y + j);
        int x0 = x, x1 = x + w - 1;
        if (x0 & 1) { p[x0 >> 1] ^= 0x07; x0++; }
        if (!(x1 & 1)) { p[x1 >> 1] ^= 0x70; x1--; }
        for (int b = x0 >> 1; b <= (x1 >> 1); b++) p[b] ^= 0x77;
    }
}

/* ---- the tool palette ----------------------------------------------------------------- */
DT_NI static void dt_ticon(int t, int cx, int cy)
{
    int i;
    switch (t) {
    case DT_SEL:
        for (i = -7; i <= 7; i += 2) { dt_pset(cx + i, cy - 6, BLACK); dt_pset(cx + i, cy + 6, BLACK); }
        for (i = -6; i <= 6; i += 2) { dt_pset(cx - 7, cy + i, BLACK); dt_pset(cx + 7, cy + i, BLACK); }
        break;
    case DT_LASSO:
        dt_ellipse(cx, cy - 2, 8, 5, BLACK, 0);
        dt_line(cx - 4, cy + 3, cx - 7, cy + 8, BLACK);
        break;
    case DT_HAND:
        for (i = 0; i < 4; i++) dt_vline(cx - 5 + i * 3, cy - 7 + ((i == 0 || i == 3) ? 2 : 0), cy, BLACK);
        dt_rect(cx - 6, cy, 13, 8, BLACK);
        dt_line(cx - 6, cy + 2, cx - 9, cy - 2, BLACK);
        break;
    case DT_TXT:
        dt_draw_char(cx - 4, cy - 7, 'A', BLACK, TRANSP, 1);
        break;
    case DT_BUCKET:
        dt_rect(cx - 6, cy - 3, 10, 9, BLACK);
        dt_line(cx - 6, cy - 3, cx - 1, cy - 8, BLACK);
        dt_line(cx - 1, cy - 8, cx + 3, cy - 3, BLACK);
        dt_vline(cx + 6, cy, cy + 4, BLACK);
        dt_fill_rect(cx + 5, cy + 4, 3, 3, BLACK);
        break;
    case DT_SPRAY:
        dt_rect(cx - 5, cy - 2, 8, 11, BLACK);
        dt_fill_rect(cx - 3, cy - 5, 4, 3, BLACK);
        dt_pset(cx + 3, cy - 7, BLACK); dt_pset(cx + 5, cy - 5, BLACK); dt_pset(cx + 6, cy - 8, BLACK);
        dt_pset(cx + 7, cy - 3, BLACK); dt_pset(cx + 4, cy - 3, BLACK); dt_pset(cx + 7, cy - 6, BLACK);
        break;
    case DT_BRUSH:
        dt_line(cx - 7, cy + 7, cx, cy, BLACK);
        dt_line(cx - 6, cy + 7, cx + 1, cy, BLACK);
        dt_fill_rect(cx, cy - 7, 7, 7, BLACK);
        break;
    case DT_PENCIL:
        dt_line(cx - 6, cy + 6, cx + 5, cy - 5, BLACK);
        dt_line(cx - 5, cy + 6, cx + 6, cy - 5, BLACK);
        dt_line(cx - 7, cy + 7, cx - 6, cy + 6, BLACK);
        dt_pset(cx - 7, cy + 7, BLACK);
        break;
    case DT_LINE:
        dt_line(cx - 7, cy + 7, cx + 7, cy - 7, BLACK);
        break;
    case DT_ERASER:
        dt_rect(cx - 7, cy - 3, 14, 8, BLACK);
        dt_fill_rect(cx - 7, cy + 1, 14, 4, BLACK);
        break;
    case DT_RECT:
        dt_rect(cx - 8, cy - 6, 16, 12, BLACK);
        break;
    case DT_FRECT:
        dt_rect(cx - 8, cy - 6, 16, 12, BLACK);
        for (i = -5; i <= 5; i++) dt_span(cx - 7, cx + 7, cy + i, 5);
        break;
    case DT_OVAL:
        dt_ellipse(cx, cy, 8, 6, BLACK, 0);
        break;
    case DT_FOVAL:
        dt_ellipse(cx, cy, 8, 6, BLACK, 0);
        dt_ellipse(cx, cy, 4, 3, BLACK, 1);
        break;
    case DT_RRECT:
        dt_hline(cx - 6, cx + 6, cy - 6, BLACK); dt_hline(cx - 6, cx + 6, cy + 6, BLACK);
        dt_vline(cx - 8, cy - 4, cy + 4, BLACK); dt_vline(cx + 8, cy - 4, cy + 4, BLACK);
        dt_pset(cx - 7, cy - 5, BLACK); dt_pset(cx + 7, cy - 5, BLACK);
        dt_pset(cx - 7, cy + 5, BLACK); dt_pset(cx + 7, cy + 5, BLACK);
        break;
    default:
        dt_ellipse(cx - 2, cy, 6, 5, BLACK, 0);
        dt_line(cx + 3, cy + 3, cx + 8, cy + 7, BLACK);
        break;
    }
}

static void dt_cell_inv(int t)
{
    int x = 43 + 26 * (t & 1) + 1, y = 47 + 26 * (t >> 1) + 1;
    dt_invert(x, y, 25, 25);
    dt_sync(x, y, 25, 25);
}

static void dt_select_tool(int t)
{
    dt_cell_inv(dt_tool);
    dt_tool = t;
    dt_cell_inv(t);
}

/* ---- the pattern palette ------------------------------------------------------------------- */
DT_NI static void dt_pat_cell(int i, int sel)
{
    int x = DT_CX + 28 * i, y = 305;
    dt_fill_rect(x + 1, y + 1, 25, 19, WHITE);
    if (sel) dt_rect(x + 1, y + 1, 25, 19, BLACK);
    for (int r = 0; r < 15; r++) dt_span(x + 3, x + 23, y + 3 + r, i);
    dt_sync(x, y, 27, 21);
}

DT_NI static void dt_select_pat(int i)
{
    dt_pat_cell(dt_psel, 0);
    dt_psel = i;
    dt_pat_cell(i, 1);
    for (int y = 263; y < 300; y++) dt_span(44, 94, y, i);
    dt_sync(43, 262, 53, 39);
}

/* ---- menus -------------------------------------------------------------------------------- */
DT_NI static void dt_menu(int open)
{
    if (open) {
        dt_copy_rect(BUF2, BUF0, 28, 0, 142, 122);
        dt_invert(28, 0, 48, 17);
        dt_fill_rect(30, 20, 136, 100, BLACK);
        dt_fill_rect(28, 18, 136, 100, WHITE);
        dt_rect(28, 18, 136, 100, BLACK);
        for (int i = 0; i < 6; i++) dt_draw_text(40, 20 + 16 * i + 1, dt_mi[i], BLACK, WHITE, 1);
        dt_sync(28, 0, 142, 122);
        dt_mitem = -1;
    } else {
        dt_copy_rect(BUF0, BUF2, 28, 0, 142, 122);
        dt_copy_rect(BUF1, BUF2, 28, 0, 142, 122);
    }
    dt_mopen = open;
}

static void dt_menu_track(void)
{
    if (!dt_mopen) return;
    int it = -1;
    if (dt_px >= 30 && dt_px < 162 && dt_py >= 20 && dt_py < 116) it = (dt_py - 20) >> 4;
    if (it == dt_mitem) return;
    if (dt_mitem >= 0) dt_invert(30, 20 + 16 * dt_mitem, 132, 16);
    if (it >= 0) dt_invert(30, 20 + 16 * it, 132, 16);
    dt_mitem = it;
    dt_sync(30, 20, 132, 96);
}

/* ---- the desktop ------------------------------------------------------------------------- */
static void dt_label(int cx, int y, const char *s)
{
    int n = 0;
    while (s[n]) n++;
    dt_draw_text(cx - 4 * n, y, s, BLACK, WHITE, 1);
}

DT_NI static void dt_icon_disk(int x, int y)
{
    dt_fill_rect(x, y, 32, 28, WHITE);
    dt_rect(x, y, 32, 28, BLACK);
    dt_rect(x + 8, y + 1, 16, 9, BLACK);
    dt_fill_rect(x + 16, y + 2, 4, 6, BLACK);
    dt_fill_rect(x + 5, y + 14, 22, 12, YELLOW);
    dt_rect(x + 4, y + 13, 24, 14, BLACK);
    dt_hline(x + 8, x + 23, y + 18, BLACK);
    dt_hline(x + 8, x + 23, y + 21, BLACK);
    dt_label(x + 16, y + 32, "Disk");
}

DT_NI static void dt_icon_doc(int x, int y)
{
    dt_fill_rect(x + 2, y, 24, 32, WHITE);
    dt_rect(x + 2, y, 24, 32, BLACK);
    dt_fill_rect(x + 18, y, 8, 8, GREY);
    dt_line(x + 18, y, x + 25, y + 7, BLACK);
    dt_hline(x + 18, x + 25, y + 7, BLACK);
    dt_vline(x + 18, y, y + 7, BLACK);
    for (int i = 0; i < 5; i++) dt_hline(x + 6, x + 21, y + 12 + 4 * i, BLACK);
    dt_label(x + 14, y + 36, "Notes");
}

DT_NI static void dt_icon_trash(int x, int y, int full)
{
    dt_fill_rect(x - 4, y - 4, 40, 40, GREY);
    if (full) {
        dt_line(x + 6, y + 2, x + 16, y - 3, BLACK);
        dt_line(x + 16, y - 3, x + 24, y + 3, BLACK);
        dt_fill_rect(x + 6, y + 2, 18, 4, WHITE);
        dt_hline(x + 8, x + 22, y + 3, BLACK);
    }
    dt_fill_rect(x + 3, y + 6, 22, 26, WHITE);
    dt_rect(x + 3, y + 6, 22, 26, BLACK);
    dt_fill_rect(x, y + 2, 28, 4, WHITE);
    dt_rect(x, y + 2, 28, 4, BLACK);
    dt_fill_rect(x + 10, y - 1, 8, 3, WHITE);
    dt_rect(x + 10, y - 1, 8, 3, BLACK);
    for (int i = 0; i < 4; i++) dt_vline(x + 8 + 4 * i, y + 10, y + 28, BLACK);
    dt_label(x + 14, y + 36, "Trash");
}

DT_NI static void dt_setup(void)
{
    clear(GREY);
    /* menu bar */
    dt_fill_rect(0, 0, 640, 17, WHITE);
    dt_hline(0, 639, 17, BLACK);
    dt_fill_rect(8, 2, 6, 6, RED);   dt_fill_rect(14, 2, 6, 6, YELLOW);
    dt_fill_rect(8, 8, 6, 6, BLUE);  dt_fill_rect(14, 8, 6, 6, GREEN);
    dt_rect(8, 2, 12, 12, BLACK);
    {
        int x = 28;
        static const char *const mt[5] = { " File ", " Edit ", " Goodies ", " Font ", " Style " };
        for (int i = 0; i < 5; i++) x = dt_draw_text(x, 2, mt[i], BLACK, WHITE, 1);
    }
    dt_draw_text(584, 2, "10:42", BLACK, WHITE, 1);
    /* desktop icons */
    dt_icon_disk(584, 34);
    dt_icon_doc(586, 134);
    dt_icon_trash(586, 254, 0);
    /* the window */
    dt_fill_rect(43, 29, 520, 304, BLACK);
    dt_fill_rect(40, 26, 520, 304, WHITE);
    dt_rect(40, 26, 520, 304, BLACK);
    for (int y = 29; y < 42; y += 2) dt_hline(41, 558, y, BLACK);
    dt_hline(40, 559, 44, BLACK);
    dt_fill_rect(48, 28, 16, 13, WHITE);
    dt_rect(50, 29, 11, 11, BLACK);
    dt_fill_rect(536, 28, 16, 13, WHITE);
    dt_rect(538, 29, 11, 11, BLACK);
    dt_rect(540, 31, 7, 7, BLACK);
    dt_fill_rect(256, 28, 88, 13, WHITE);
    dt_draw_text(264, 27, "SketchPad", BLACK, WHITE, 1);
    /* tools */
    for (int t = 0; t < 16; t++) {
        int x = 43 + 26 * (t & 1), y = 47 + 26 * (t >> 1);
        dt_rect(x, y, 27, 27, BLACK);
        dt_ticon(t, x + 13, y + 13);
    }
    dt_rect(43, 262, 53, 39, BLACK);
    dt_rect(DT_CX, DT_CY, DT_CW, DT_CH, BLACK);
    for (int i = 0; i < 16; i++) dt_rect(DT_CX + 28 * i, 305, 27, 21, BLACK);
    dt_pat_init();
    dt_psel = 0;
    dt_tool = DT_SEL;
    for (int i = 0; i < 16; i++) dt_pat_cell(i, 0);
    dt_select_pat(5);
    dt_cell_inv(DT_SEL);
}

/* ---- the pointer ----------------------------------------------------------------------- */
static void dt_ptr_erase(void)
{
    if (dt_plv) { dt_copy_rect(BUF0, BUF1, dt_plx, dt_ply, dt_plw, dt_plh); dt_plv = 0; }
}

static void dt_ptr_draw(void)
{
    if (dt_hide) return;
    const sprite_t *s = dt_sp_arrow;
    int hx = 0, hy = 0;
    if (dt_px > DT_CX && dt_px < DT_CX + DT_CW - 1 && dt_py > DT_CY && dt_py < DT_CY + DT_CH - 1) {
        if (dt_tool == DT_TXT) { s = dt_sp_ibeam; hx = 3; hy = 7; }
        else if (dt_tool == DT_PENCIL) { s = dt_sp_pen; hy = 8; }
        else { s = dt_sp_cross; hx = 5; hy = 5; }
    }
    dt_plx = dt_px - hx; dt_ply = dt_py - hy;
    dt_plw = s->w + 1; dt_plh = s->h;
    spr_draw(s, dt_plx, dt_ply);
    dt_plv = 1;
}

/* ---- rubber bands ------------------------------------------------------------------------- */
static void dt_gh_erase(void)
{
    if (!dt_gk) return;
    if (dt_gk == 1) {
        dt_copy_rect(BUF0, BUF1, dt_gx, dt_gy, dt_gw, 1);
        dt_copy_rect(BUF0, BUF1, dt_gx, dt_gy + dt_gh - 1, dt_gw, 1);
        dt_copy_rect(BUF0, BUF1, dt_gx, dt_gy, 1, dt_gh);
        dt_copy_rect(BUF0, BUF1, dt_gx + dt_gw - 1, dt_gy, 1, dt_gh);
    } else
        dt_copy_rect(BUF0, BUF1, dt_gx, dt_gy, dt_gw, dt_gh);
    dt_gk = 0;
}

static void dt_ghost(int shape)
{
    int x0 = dt_sx, y0 = dt_sy, x1 = dt_px, y1 = dt_py;
    if (shape == 3) { x0 = dt_px - 16; y0 = dt_py - 16; x1 = dt_px + 15; y1 = dt_py + 15; shape = 0; }
    else if (shape == 2) dt_line(x0, y0, x1, y1, BLACK);
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    dt_gx = x0 - 1; dt_gy = y0 - 1; dt_gw = x1 - x0 + 3; dt_gh = y1 - y0 + 3;
    dt_gk = 2;
    if (shape == 0) { dt_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, BLACK); dt_gk = 1; dt_gx = x0; dt_gy = y0; dt_gw -= 2; dt_gh -= 2; }
    else if (shape == 1) dt_ellipse((x0 + x1) >> 1, (y0 + y1) >> 1, (x1 - x0) >> 1, (y1 - y0) >> 1, BLACK, 0);
}

DT_NI static void dt_commit(const dt_step_t *s)
{
    int x0 = dt_sx, y0 = dt_sy, x1 = s->a, y1 = s->b;
    if (s->c == 2) {
        dt_line(x0, y0, x1, y1, BLACK);
        dt_syncb(x0, y0, x1, y1, 1);
        return;
    }
    if (s->c == 3) {                           /* the document goes into the trash */
        dt_fill_rect(576, 124, 48, 70, GREY);
        dt_icon_trash(586, 254, 1);
        dt_sync(576, 124, 48, 70);
        dt_sync(580, 246, 48, 60);
        return;
    }
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int filled = dt_tool == DT_FRECT || dt_tool == DT_FOVAL;
    dt_jp = dt_psel;
    if (s->c == 0) {
        dt_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, BLACK);
        dt_jk = filled ? 1 : 0;
        dt_jx0 = x0 + 1; dt_jx1 = x1 - 1; dt_jy = y0 + 1; dt_jy1 = y1 - 1;
    } else {
        int cx = (x0 + x1) >> 1, cy = (y0 + y1) >> 1, rx = (x1 - x0) >> 1, ry = (y1 - y0) >> 1;
        dt_ellipse(cx, cy, rx, ry, BLACK, 0);
        dt_jk = filled ? 2 : 0;
        dt_jcx = cx; dt_jcy = cy; dt_jdy = ry - 1; dt_jhw = 0;
        dt_jrx2 = (int32_t)(rx - 1) * (rx - 1);
        dt_jry2 = (int32_t)(ry - 1) * (ry - 1);
        dt_jlim = dt_jrx2 * dt_jry2;
    }
    dt_syncb(x0, y0, x1, y1, 1);
}

/* ---- background jobs --------------------------------------------------------------------- */
DT_NI static void dt_job(void)
{
    int n;
    if (dt_jk == 1) {
        int ys = dt_jy;
        for (n = 0; n < 10 && dt_jy <= dt_jy1; n++, dt_jy++) dt_span(dt_jx0, dt_jx1, dt_jy, dt_jp);
        dt_sync(dt_jx0, ys, dt_jx1 - dt_jx0 + 1, dt_jy - ys);
        if (dt_jy > dt_jy1) dt_jk = 0;
    } else if (dt_jk == 2) {
        for (n = 0; n < 5 && dt_jdy >= 0; n++, dt_jdy--) {
            int32_t d2 = (int32_t)dt_jdy * dt_jdy * dt_jrx2;
            while ((int32_t)(dt_jhw + 1) * (dt_jhw + 1) * dt_jry2 + d2 <= dt_jlim) dt_jhw++;
            int x0 = dt_jcx - dt_jhw, w = 2 * dt_jhw + 1;
            dt_span(x0, x0 + w - 1, dt_jcy - dt_jdy, dt_jp);
            dt_sync(x0, dt_jcy - dt_jdy, w, 1);
            if (dt_jdy) {
                dt_span(x0, x0 + w - 1, dt_jcy + dt_jdy, dt_jp);
                dt_sync(x0, dt_jcy + dt_jdy, w, 1);
            }
        }
        if (dt_jdy < 0) dt_jk = 0;
    } else if (dt_jk == 3) {
        for (n = 0; n < 3 && dt_sp > 0; n++) {
            dt_sp--;
            int x = dt_stk[2 * dt_sp], y = dt_stk[2 * dt_sp + 1];
            if (dt_pget(x, y) != WHITE) continue;
            int l = x, r = x;
            while (dt_pget(l - 1, y) == WHITE) l--;
            while (dt_pget(r + 1, y) == WHITE) r++;
            dt_hline(l, r, y, (uint8_t)dt_fcol);
            for (int dy = -1; dy <= 1; dy += 2) {
                int in = 0;
                for (int xx = l; xx <= r; xx++) {
                    int w = dt_pget(xx, y + dy) == WHITE;
                    if (w && !in && dt_sp < 200) { dt_stk[2 * dt_sp] = (int16_t)xx; dt_stk[2 * dt_sp + 1] = (int16_t)(y + dy); dt_sp++; }
                    in = w;
                }
            }
            dt_sync(l, y, r - l + 1, 1);
        }
        if (dt_sp <= 0) dt_jk = 0;
    }
}

/* ---- strokes ---------------------------------------------------------------------------- */
static void dt_stamp(int x, int y)
{
    if (dt_tool == DT_ERASER) { dt_fill_rect(x - 6, y - 6, 12, 12, WHITE); return; }
    dt_hline(x - 1, x + 1, y - 2, BLACK);
    dt_hline(x - 2, x + 2, y - 1, BLACK);
    dt_hline(x - 2, x + 2, y, BLACK);
    dt_hline(x - 2, x + 2, y + 1, BLACK);
    dt_hline(x - 1, x + 1, y + 2, BLACK);
}

static void dt_seg(int x0, int y0, int x1, int y1)
{
    if (dt_tool == DT_PENCIL) { dt_line(x0, y0, x1, y1, BLACK); }
    else if (dt_tool == DT_SPRAY) {
        for (int i = 0; i < 18; i++) {
            int ox = (int)rnd_n(19) - 9, oy = (int)rnd_n(19) - 9;
            if (ox * ox + oy * oy <= 81) dt_pset(x1 + ox, y1 + oy, BLACK);
        }
    } else {
        int dx = x1 - x0, dy = y1 - y0;
        int n = (dx < 0 ? -dx : dx);
        if ((dy < 0 ? -dy : dy) > n) n = dy < 0 ? -dy : dy;
        if (n == 0) dt_stamp(x1, y1);
        else {
            int step = dt_tool == DT_ERASER ? 4 : 2;
            int32_t ix = ((int32_t)dx << 8) / n, iy = ((int32_t)dy << 8) / n;
            int32_t fx = ((int32_t)x0 << 8) + 128, fy = ((int32_t)y0 << 8) + 128;
            for (int k = 0; k < n; k += step) {
                fx += ix * step; fy += iy * step;
                dt_stamp((int)(fx >> 8), (int)(fy >> 8));
            }
            dt_stamp(x1, y1);
        }
    }
    dt_syncb(x0, y0, x1, y1, 10);
}

/* ---- the script ------------------------------------------------------------------------- */
static int dt_ease(int a, int b, int f, int dur, int smooth)
{
    int32_t u = (int32_t)f * 256 / dur;
    if (u > 256) u = 256;
    if (smooth) u = (u * u * (768 - 2 * u)) >> 16;
    return a + (int)(((int32_t)(b - a) * u) >> 8);
}

DT_NI static void dt_begin(const dt_step_t *s)
{
    switch (s->op) {
    case DT_MOVE: case DT_DRAG: dt_sx = dt_px; dt_sy = dt_py; break;
    case DT_STROKE: dt_lx = s->a; dt_ly = s->b; break;
    case DT_TOOL: dt_select_tool(s->a); break;
    case DT_PAT: dt_select_pat(s->a); break;
    case DT_MENU: dt_menu(s->a); break;
    case DT_FILL:
        dt_stk[0] = s->a; dt_stk[1] = s->b; dt_sp = 1; dt_fcol = s->c; dt_jk = 3;
        break;
    case DT_TEXT: dt_tn = 0; break;
    }
}

DT_NI static void dt_update(const dt_step_t *s, int f)
{
    switch (s->op) {
    case DT_MOVE:
        dt_px = dt_ease(dt_sx, s->a, f, s->dur, 1);
        dt_py = dt_ease(dt_sy, s->b, f, s->dur, 1);
        break;
    case DT_DRAG:
        dt_px = dt_ease(dt_sx, s->a, f, s->dur, 1);
        dt_py = dt_ease(dt_sy, s->b, f, s->dur, 1);
        dt_ghost(s->c);
        break;
    case DT_STROKE: {
        int x = dt_ease(s->a, s->c, f, s->dur, 0), y = dt_ease(s->b, s->d, f, s->dur, 0);
        int32_t u = (int32_t)f * 256 / s->dur;
        if (u > 255) u = 255;
        y += ((s->e & 255) * SIN((s->e >> 8) * u)) >> 7;
        dt_px = x; dt_py = y;
        dt_seg(dt_lx, dt_ly, x, y);
        dt_lx = x; dt_ly = y;
        break;
    }
    case DT_TEXT: {
        int cnt = f / s->e, len = (int)sizeof dt_msg - 1;
        if (cnt > len) cnt = len;
        while (dt_tn < cnt) {
            int x = s->a + 16 * dt_tn;
            dt_hide = 1;
            dt_draw_char(x, s->b, (uint8_t)dt_msg[dt_tn], BLACK, TRANSP, 2);
            dt_sync(x, s->b, 16, 28);
            dt_tn++;
        }
        break;
    }
    }
}

static void dt_end(const dt_step_t *s)
{
    if (s->op == DT_DRAG) { dt_gh_erase(); dt_commit(s); }
    else if (s->op == DT_TEXT) dt_hide = 0;
}

DT_NI static void dt_frame(uint16_t n)
{
    dt_ptr_erase();
    dt_gh_erase();
    dt_sf += n;
    while (dt_si < DT_NSTEPS && dt_sf >= (uint16_t)dt_script[dt_si].dur) {
        dt_update(&dt_script[dt_si], dt_script[dt_si].dur);
        dt_end(&dt_script[dt_si]);
        dt_sf -= (uint16_t)dt_script[dt_si].dur;
        dt_si++;
        if (dt_si < DT_NSTEPS) dt_begin(&dt_script[dt_si]);
    }
    if (dt_si < DT_NSTEPS) dt_update(&dt_script[dt_si], dt_sf);
    dt_menu_track();
    dt_job();
    dt_ptr_draw();
}

static void dt_run(int32_t frames)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    gfx_target(BUF0);
    dt_sp_arrow = spr_make(dt_art_arrow, 12, 18, 0);
    dt_sp_cross = spr_make(dt_art_cross, 11, 11, 0);
    dt_sp_ibeam = spr_make(dt_art_ibeam, 7, 15, 0);
    dt_sp_pen = spr_make(dt_art_pen, 10, 9, 0);
    dt_stk = (int16_t *)salloc(800);
    dt_setup();
    copy_buf(BUF1, BUF0);
    gfx_show(BUF0);
    dt_px = 606; dt_py = 205;
    dt_plv = dt_gk = dt_hide = dt_mopen = dt_jk = 0;
    dt_si = 0; dt_sf = 0;
    dt_begin(&dt_script[0]);
    sec_begin("desktop", frames);
    while (sec_running()) {
        uint16_t n = frame_wait();
        dt_frame(n);
    }
}
