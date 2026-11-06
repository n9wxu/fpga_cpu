/* ui.c: the menu bar and menus, the toolbox and the tool's options, the
 * colour bar and the status text, the help box (paint.h). */
#include "paint.h"
#include <string.h>
#include "../../../fpga/demo/font.h"

const uint8_t *glyph(uint8_t ch) { return font8x14[ch]; }

const char *const tool_names[NTOOLS] = {
    "Select", "Colour picker", "Pencil", "Brush", "Spray can", "Eraser", "Paint bucket", "Text",
    "Line", "Curve", "Rectangle", "Rounded rect", "Ellipse", "Polygon", "Magnifier", "Hand",
};

/* ---- drawing on the screen, within clip (win.c) ---- */
static void sfill_raw(int x0, int y0, int x1, int y1, uint8_t c)
{
    damage(x0, y0, x1, y1);
    uint8_t cc = (uint8_t)(c * 0x11);
    int b0 = x0 >> 1, b1 = x1 >> 1, mid = b1 - b0 - 1;
    uint8_t lm = (x0 & 1) ? 0x0F : 0xFF, rm = (x1 & 1) ? 0xFF : 0xF0;
    if (b0 == b1)
        lm &= rm;
    uint8_t *p = SCREEN + rowoff[y0];
    for (int y = y0; y <= y1; y++, p += STRIDE) {
        p[b0] = (uint8_t)((p[b0] & ~lm) | (cc & lm));
        if (b0 == b1)
            continue;
        if (mid > 0)
            memset(p + b0 + 1, cc, (unsigned)mid);
        p[b1] = (uint8_t)((p[b1] & ~rm) | (cc & rm));
    }
}

void sfill(int x0, int y0, int x1, int y1, uint8_t c)
{
    for (int k = 0; k < clip->n; k++) {
        const rect_t *r = &clip->r[k];
        int a = x0 > r->x0 ? x0 : r->x0, b = x1 < r->x1 ? x1 : r->x1;
        int e = y0 > r->y0 ? y0 : r->y0, d = y1 < r->y1 ? y1 : r->y1;
        if (a <= b && e <= d)
            sfill_raw(a, e, b, d, c);
    }
}

/* the rectangle is all in one of clip's rectangles (draw it directly), or in none */
static int whole(int x0, int y0, int x1, int y1)
{
    for (int k = 0; k < clip->n; k++) {
        const rect_t *r = &clip->r[k];
        if (x0 >= r->x0 && x1 <= r->x1 && y0 >= r->y0 && y1 <= r->y1)
            return 1;
    }
    return 0;
}

static int none(int x0, int y0, int x1, int y1)
{
    for (int k = 0; k < clip->n; k++) {
        const rect_t *r = &clip->r[k];
        if (x0 <= r->x1 && x1 >= r->x0 && y0 <= r->y1 && y1 >= r->y0)
            return 0;
    }
    return 1;
}

void sframe(int x0, int y0, int x1, int y1, uint8_t c)
{
    sfill(x0, y0, x1, y0, c);
    sfill(x0, y1, x1, y1, c);
    sfill(x0, y0, x0, y1, c);
    sfill(x1, y0, x1, y1, c);
}

/* 8 pixels from bits, at an even x: four bytes */
static void bits8(uint8_t *p, uint8_t bits, const uint8_t *t)
{
    p[0] = t[bits >> 6];
    p[1] = t[(bits >> 4) & 3];
    p[2] = t[(bits >> 2) & 3];
    p[3] = t[bits & 3];
}

static void two_tbl(uint8_t *t, uint8_t fg, uint8_t bg)
{
    t[0] = (uint8_t)(bg * 0x11);
    t[1] = (uint8_t)(bg << 4 | fg);
    t[2] = (uint8_t)(fg << 4 | bg);
    t[3] = (uint8_t)(fg * 0x11);
}

void stext(int x, int y, const char *s, uint8_t fg, uint8_t bg)
{
    int n = (int)strlen(s);
    if (y < 0 || y + 14 > DH)
        return;
    damage(x, y, x + 8 * n - 1, y + 13);
    uint8_t t[4];
    two_tbl(t, fg, bg);
    uint8_t *row = SCREEN + rowoff[y];
    for (; *s && x + 8 <= DW; s++, x += 8) {
        const uint8_t *g = font8x14[(uint8_t)*s];
        if (bg == TRANSP || !whole(x, y, x + 7, y + 13)) {
            if (none(x, y, x + 7, y + 13))
                continue;
            for (int r = 0; r < 14; r++)
                for (int k = 0; k < 8; k++) {
                    int on = g[r] & (0x80 >> k);
                    if (on || bg != TRANSP)
                        spx(x + k, y + r, on ? fg : bg);
                }
            continue;
        }
        uint8_t *p = row + (x >> 1);
        for (int r = 0; r < 14; r++, p += STRIDE) {
            uint8_t b = g[r];
            p[0] = t[b >> 6];
            p[1] = t[(b >> 4) & 3];
            p[2] = t[(b >> 2) & 3];
            p[3] = t[b & 3];
        }
    }
}

/* a 16x16 icon (16 rows of bits); x even */
static void icon(int x, int y, const uint16_t *rows, uint8_t fg, uint8_t bg)
{
    if (none(x, y, x + 15, y + 15))
        return;
    if (!whole(x, y, x + 15, y + 15)) {
        for (int r = 0; r < 16; r++)
            for (int k = 0; k < 16; k++)
                spx(x + k, y + r, (rows[r] & (0x8000u >> k)) ? fg : bg);
        return;
    }
    uint8_t t[4];
    two_tbl(t, fg, bg);
    damage(x, y, x + 15, y + 15);
    uint8_t *p = SCREEN + rowoff[y] + (unsigned)(x >> 1);
    for (int r = 0; r < 16; r++, p += STRIDE) {
        bits8(p, (uint8_t)(rows[r] >> 8), t);
        bits8(p + 4, (uint8_t)rows[r], t);
    }
}

/* ---- the tools' icons ---- */
static const char *const icons[NTOOLS][16] = {
    { "                ", " ## ## ## ## ## ", " #            # ", "                ",
      " #            # ", " #            # ", "                ", " #            # ",
      " #            # ", "                ", " #            # ", " #            # ",
      "                ", " #            # ", " ## ## ## ## ## ", "                " },
    { "           ###  ", "          ##### ", "          ##### ", "        ####### ",
      "         ####   ", "        #  #    ", "       #   #    ", "      #   #     ",
      "     #   #      ", "    #   #       ", "   #   #        ", "  #   #         ",
      " #   #          ", " #  #           ", "# ##            ", "                " },
    { "            ##  ", "           #### ", "          # ####", "         #   ## ",
      "        #   #   ", "       #   #    ", "      #   #     ", "     #   #      ",
      "    #   #       ", "   #   #        ", "  ## ##         ", "  ####          ",
      " ####           ", " ##             ", "#               ", "                " },
    { "            ##  ", "           #### ", "          ####  ", "         ####   ",
      "        ####    ", "       # ##     ", "      #  #      ", "     ## #       ",
      "    #####       ", "   ######       ", "   #####        ", "  #####         ",
      "  ####          ", " ###            ", "##              ", "                " },
    { " #   #  #       ", "   #  #   #     ", " #  #   #       ", "      ###       ",
      "      # #       ", "     #####      ", "     #   #      ", "     # # #      ",
      "     # # #      ", "     # # #      ", "     #   #      ", "     #   #      ",
      "     #   #      ", "     #####      ", "                ", "                " },
    { "                ", "                ", "        ######  ", "       #    ##  ",
      "      #    # #  ", "     #    #  #  ", "    #    #  #   ", "   ######  #    ",
      "   #    # #     ", "   #    ##      ", "   ######       ", "                ",
      "                ", "                ", "                ", "                " },
    { "     ##         ", "    #  #        ", "    #  ##       ", "    # #  #      ",
      "    ## #  #     ", "    #  #   #    ", "   #   #    #   ", "  #    #     #  ",
      " #     #      # ", "#            ###", " #          ####", "  #        # ###",
      "   #      #  ###", "    #    #   ###", "     #  #     # ", "      ##        " },
    { "                ", "       ##       ", "       ##       ", "      #  #      ",
      "      #  #      ", "     #    #     ", "     #    #     ", "    ########    ",
      "    #      #    ", "   #        #   ", "   #        #   ", "  ###      ###  ",
      "                ", "                ", "                ", "                " },
    { "              ##", "             ## ", "            ##  ", "           ##   ",
      "          ##    ", "         ##     ", "        ##      ", "       ##       ",
      "      ##        ", "     ##         ", "    ##          ", "   ##           ",
      "  ##            ", " ##             ", "##              ", "                " },
    { "                ", "    ###         ", "  ##   ##       ", " #       #      ",
      "#         #     ", "#          #    ", "            #   ", "            #   ",
      "             #  ", "             #  ", "              # ", "              # ",
      "               #", "                ", "                ", "                " },
    { "                ", "                ", "################", "#              #",
      "#              #", "#              #", "#              #", "#              #",
      "#              #", "#              #", "#              #", "#              #",
      "################", "                ", "                ", "                " },
    { "                ", "                ", "   ##########   ", "  #          #  ",
      " #            # ", "#              #", "#              #", "#              #",
      "#              #", "#              #", "#              #", " #            # ",
      "  #          #  ", "   ##########   ", "                ", "                " },
    { "                ", "     ######     ", "   ##      ##   ", "  #          #  ",
      " #            # ", "#              #", "#              #", "#              #",
      "#              #", "#              #", " #            # ", "  #          #  ",
      "   ##      ##   ", "     ######     ", "                ", "                " },
    { "                ", "      #         ", "     # ##       ", "    #    ##     ",
      "   #       ##  ", "  #          # ", " #           #  ", "#            #  ",
      " ##         #   ", "   ##       #   ", "     ##    #    ", "       ##  #    ",
      "         ###    ", "                ", "                ", "                " },
    { "   #####        ", "  #     #       ", " #       #      ", "#    #    #     ",
      "#    #    #     ", "#  #####  #     ", "#    #    #     ", "#    #    #     ",
      " #       #      ", "  #     ###     ", "   ##### ###    ", "          ###   ",
      "           ###  ", "            ### ", "             ## ", "                " },
    { "      ##        ", "   ## #  ###    ", "  #  ##  #  #   ", "  #  ##  #  # # ",
      "   #  #  #  ## #", "   #  #  #  #  #", " ## #       #  #", "#  ##          #",
      "#   #         # ", " #            # ", "  #           # ", "  #          #  ",
      "   #         #  ", "    #       #   ", "     #      #   ", "     #      #   " },
};

static uint16_t icon_bits[NTOOLS][16];

static void icons_init(void)
{
    for (int i = 0; i < NTOOLS; i++)
        for (int r = 0; r < 16; r++) {
            uint16_t v = 0;
            for (int k = 0; k < 16 && icons[i][r][k]; k++)
                if (icons[i][r][k] == '#')
                    v |= (uint16_t)(0x8000u >> k);
            icon_bits[i][r] = v;
        }
}

const uint8_t brush_sizes[6] = { 1, 2, 3, 5, 8, 12 };
const uint8_t eraser_sizes[5] = { 4, 6, 8, 12, 16 };
const uint8_t spray_sizes[4] = { 6, 10, 14, 20 };
const uint8_t line_widths[5] = { 1, 2, 3, 5, 8 };

/* ---- the layout: in the palettes' windows ---- */
#define TX (WIN_CX(WIN_TOOLS))
#define TY (WIN_CY(WIN_TOOLS))
#define TOOL_X(i) (TX + 2 + ((i) & 1) * 24)
#define TOOL_Y(i) (TY + 2 + ((i) >> 1) * 22)
#define OPT_X0 (TX + 2)
#define OPT_X1 (TX + 49)
#define OPT_Y0 (TY + 180)
#define OPT_Y1 (TY + 316)
#define TOP_Y (OPT_Y0 + 3)
#define ROW_Y(i) (OPT_Y0 + 22 + (i) * 19)
#define KX (WIN_CX(WIN_COLOURS))
#define KY (WIN_CY(WIN_COLOURS))
#define COL_X(i) (KX + 44 + (i) * 20)
#define COL_Y (KY + 2)
#define PAT_X(i) (KX + 44 + ((i) & 7) * 20)
#define PAT_Y(i) (KY + 21 + ((i) >> 3) * 19)
#define STATUS_X 336                    /* in the menu bar */
#define STATUS_N 17

/* what the options box offers for a tool: a top row of ntop cells, nrows rows */
typedef struct { uint8_t ntop, nrows; } opts_t;
static opts_t tool_opts(int t)
{
    opts_t o = { 0, 0 };
    switch (t) {
    case T_BRUSH: o.ntop = 4; o.nrows = 6; break;
    case T_ERASER: o.nrows = 5; break;
    case T_SPRAY: o.nrows = 4; break;
    case T_LINE: case T_CURVE: o.nrows = 5; break;
    case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON: o.ntop = 3; o.nrows = 5; break;
    case T_TEXT: o.ntop = 2; o.nrows = 2; break;
    case T_SELECT: o.nrows = 2; break;
    case T_ZOOM: o.nrows = 4; break;
    }
    return o;
}

static int top_sel(int t)
{
    switch (t) {
    case T_BRUSH: return st.brush_shape;
    case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON: return st.fill_mode;
    case T_TEXT: return st.text_opaque;
    }
    return -1;
}

static int row_sel(int t)
{
    switch (t) {
    case T_BRUSH: return st.brush_size;
    case T_ERASER: return st.eraser_size;
    case T_SPRAY: return st.spray_size;
    case T_LINE: case T_CURVE: case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON: return st.line_w;
    case T_TEXT: return st.text_scale;
    case T_SELECT: return st.sel_trans;
    case T_ZOOM: { int z = view_zoom_of(cur_view); return z == 1 ? 0 : z == 2 ? 1 : z == 4 ? 2 : 3; }
    }
    return -1;
}

/* ---- the menu bar ---- */
typedef struct { const char *label, *key; uint8_t cmd; } mitem_t;
typedef struct { const char *title; int16_t x; uint8_t n; const mitem_t *items; } menu_t;

static const mitem_t m_file[] = {
    { "New (white)", "Ctrl-N", CMD_NEW }, { "New (background)", "", CMD_NEWBG }, { "-", "", 0 },
    { "Show picture", "Tab", CMD_FULL }, { "-", "", 0 }, { "Help", "F1", CMD_HELP },
};
static const mitem_t m_edit[] = {
    { "Undo", "Ctrl-Z", CMD_UNDO }, { "Redo", "Ctrl-Y", CMD_REDO }, { "-", "", 0 },
    { "Cut", "Ctrl-X", CMD_CUT }, { "Copy", "Ctrl-C", CMD_COPY }, { "Paste", "Ctrl-V", CMD_PASTE },
    { "Clear", "Del", CMD_CLEAR }, { "Select all", "Ctrl-A", CMD_SELALL }, { "-", "", 0 },
    { "Transparent selection", "", CMD_TRANS },
};
static const mitem_t m_image[] = {
    { "Flip horizontal", "Ctrl-F", CMD_FLIPH }, { "Flip vertical", "Ctrl-J", CMD_FLIPV },
    { "Rotate 180", "", CMD_ROT180 }, { "-", "", 0 }, { "Invert colours", "Ctrl-E", CMD_INVERT },
    { "Fore- to background", "Ctrl-R", CMD_REPLACE }, { "-", "", 0 },
    { "Clear picture", "", CMD_CLEARPIC },
};
static const mitem_t m_view[] = {
    { "Zoom in", "+", CMD_ZOOMIN }, { "Zoom out", "-", CMD_ZOOMOUT }, { "Actual size", "", CMD_ZOOM1 },
    { "-", "", 0 }, { "Grid at 8x", "Ctrl-G", CMD_GRID }, { "Show picture", "Tab", CMD_FULL },
};
static const mitem_t m_window[] = {
    { "Tools", "", CMD_WTOOLS }, { "Colours", "", CMD_WCOLOURS }, { "-", "", 0 },
    { "New view", "", CMD_WNEW }, { "Tidy windows", "", CMD_WTIDY },
};
static const menu_t menus[] = {
    { "File", 8, sizeof m_file / sizeof m_file[0], m_file },
    { "Edit", 56, sizeof m_edit / sizeof m_edit[0], m_edit },
    { "Image", 104, sizeof m_image / sizeof m_image[0], m_image },
    { "View", 160, sizeof m_view / sizeof m_view[0], m_view },
    { "Window", 208, sizeof m_window / sizeof m_window[0], m_window },
    { "Help", 280, 0, 0 },
};
#define NMENUS 6
int menu_cur = -1;
static int mn_x0, mn_y0, mn_x1, mn_y1, mn_hi = -1;

static int title_w(int m) { return 8 * (int)strlen(menus[m].title) + 16; }

int menu_title_at(int x)
{
    for (int m = 0; m < NMENUS; m++)
        if (x >= menus[m].x - 8 && x < menus[m].x - 8 + title_w(m))
            return m;
    return -1;
}

static void draw_title(int m, int on)
{
    clip_screen();
    int x = menus[m].x - 8;
    sfill(x, 0, x + title_w(m) - 1, MENU_H - 1, on ? BLACK : WHITE);
    stext(menus[m].x, 0, menus[m].title, on ? WHITE : BLACK, on ? BLACK : WHITE);
}

static char status_text[24], coord_text[24];
static uint16_t msg_until;
static uint8_t msg_on;
static void draw_status(void);

void ui_draw_menubar_right(void)
{
    if (full_view)
        return;
    clip_screen();
    const char *t = tool_names[st.tool];
    sfill(STATUS_X + 8 * STATUS_N, 0, DW - 1, MENU_H - 1, WHITE);
    stext(DW - 8 - 8 * (int)strlen(t), 0, t, BLACK, WHITE);
}

void ui_draw_menubar(void)
{
    clip_screen();
    sfill(0, 0, DW - 1, MENU_H - 1, WHITE);
    sfill(0, MENU_H, DW - 1, MENU_H, BLACK);
    for (int m = 0; m < NMENUS; m++)
        draw_title(m, m == menu_cur);
    ui_draw_menubar_right();
    draw_status();
}

static int item_y(int i)
{
    int y = MENU_H + 3;
    for (int k = 0; k < i; k++)
        y += menus[menu_cur].items[k].label[0] == '-' ? 7 : 14;
    return y;
}

static void draw_item(int i, int hi)
{
    const mitem_t *it = &menus[menu_cur].items[i];
    int y = item_y(i);
    if (it->label[0] == '-') {
        sfill(mn_x0 + 1, y, mn_x1 - 3, y + 6, WHITE);
        for (int x = mn_x0 + 2; x < mn_x1 - 3; x += 2)
            spx(x, y + 3, BLACK);
        return;
    }
    int en = cmd_enabled(it->cmd);
    uint8_t bg = hi && en ? BLACK : WHITE, fg = !en ? GREY : hi ? WHITE : BLACK;
    sfill(mn_x0 + 1, y, mn_x1 - 3, y + 13, bg);
    if (cmd_checked(it->cmd))
        stext(mn_x0 + 4, y, "\xFB", fg, bg);
    stext(mn_x0 + 16, y, it->label, fg, bg);
    int kl = (int)strlen(it->key);
    if (kl)
        stext(mn_x1 - 6 - 8 * kl, y, it->key, fg, bg);
}

void menu_open(int m)
{
    if (menu_cur >= 0)
        menu_close();
    clip_screen();
    menu_cur = m;
    draw_title(m, 1);
    mn_hi = -1;
    const menu_t *mn = &menus[m];
    if (!mn->n) {
        mn_x1 = -1;
        return;
    }
    int w = 0;
    for (int i = 0; i < mn->n; i++) {
        int l = 8 * ((int)strlen(mn->items[i].label) + (int)strlen(mn->items[i].key)) + 40;
        if (l > w) w = l;
    }
    mn_x0 = mn->x - 8;
    mn_y0 = MENU_H;
    mn_x1 = mn_x0 + w + 1;
    mn_y1 = item_y(mn->n) + 3;
    /* what it covers, to put back (the scratch area is free while a menu is open) */
    cursor_hide();
    {
        int b0 = mn_x0 >> 1, nb = (mn_x1 >> 1) - b0 + 1;
        uint8_t *q = (uint8_t *)SCRATCH_A;
        for (int y = mn_y0 + 1; y <= mn_y1; y++, q += nb)
            fastcopy(q, SCREEN + rowoff[y] + (unsigned)b0, (unsigned)nb);
    }
    sfill(mn_x0, mn_y0, mn_x1 - 2, mn_y1 - 2, WHITE);
    sframe(mn_x0, mn_y0, mn_x1 - 2, mn_y1 - 2, BLACK);
    sfill(mn_x0 + 2, mn_y1 - 1, mn_x1, mn_y1, BLACK);   /* the shadow */
    sfill(mn_x1 - 1, mn_y0 + 2, mn_x1, mn_y1, BLACK);
    for (int i = 0; i < mn->n; i++)
        draw_item(i, 0);
}

void menu_close(void)
{
    if (menu_cur < 0)
        return;
    int m = menu_cur;
    menu_cur = -1;
    draw_title(m, 0);
    if (mn_x1 < 0)
        return;
    clip_screen();
    damage(mn_x0, mn_y0 + 1, mn_x1, mn_y1);
    int b0 = mn_x0 >> 1, nb = (mn_x1 >> 1) - b0 + 1;
    const uint8_t *q = (const uint8_t *)SCRATCH_A;
    for (int y = mn_y0 + 1; y <= mn_y1; y++, q += nb)
        fastcopy(SCREEN + rowoff[y] + (unsigned)b0, q, (unsigned)nb);
}

int menu_item_at(int x, int y)
{
    if (menu_cur < 0 || mn_x1 < 0 || x < mn_x0 || x > mn_x1 - 2 || y <= MENU_H || y > mn_y1 - 2)
        return -1;
    const menu_t *mn = &menus[menu_cur];
    for (int i = 0; i < mn->n; i++) {
        int y0 = item_y(i), y1 = item_y(i + 1);
        if (y >= y0 && y < y1)
            return mn->items[i].label[0] == '-' ? -1 : i;
    }
    return -1;
}

void menu_hilite(int i)
{
    if (i == mn_hi || menu_cur < 0 || mn_x1 < 0)
        return;
    clip_screen();
    if (mn_hi >= 0)
        draw_item(mn_hi, 0);
    mn_hi = i;
    if (i >= 0)
        draw_item(i, 1);
}

int menu_cmd(int i)
{
    if (menu_cur < 0 || i < 0 || i >= menus[menu_cur].n)
        return 0;
    int c = menus[menu_cur].items[i].cmd;
    return cmd_enabled(c) ? c : 0;
}

/* ---- the toolbox ---- */
static void tool_button(int i)
{
    int x = TOOL_X(i), y = TOOL_Y(i), on = i == st.tool;
    if (none(x, y, x + 22, y + 20))
        return;
    sframe(x, y, x + 22, y + 20, BLACK);
    sfill(x + 1, y + 1, x + 21, y + 2, on ? BLACK : WHITE);
    sfill(x + 1, y + 19, x + 21, y + 19, on ? BLACK : WHITE);
    sfill(x + 1, y + 3, x + 3, y + 18, on ? BLACK : WHITE);
    sfill(x + 20, y + 3, x + 21, y + 18, on ? BLACK : WHITE);
    icon(x + 4, y + 3, icon_bits[i], on ? WHITE : BLACK, on ? BLACK : WHITE);
}

static uint8_t shown_tool;

static int tools_shown(void) { return !full_view && wins[WIN_TOOLS].open; }

void ui_draw_tools(void)
{
    if (!icon_bits[0][1])
        icons_init();
    shown_tool = st.tool;
    if (!tools_shown())
        return;
    clip_win(WIN_TOOLS);
    sfill(TX, TY, TX + WIN_CW(WIN_TOOLS) - 1, OPT_Y0 - 1, WHITE);
    for (int i = 0; i < NTOOLS; i++)
        tool_button(i);
}

void ui_tool_changed(void)
{
    ui_draw_menubar_right();
    if (!tools_shown()) {
        shown_tool = st.tool;
        return;
    }
    clip_win(WIN_TOOLS);
    tool_button(shown_tool);
    tool_button(st.tool);
    shown_tool = st.tool;
    ui_draw_options();
}

static void opt_cell_draw(int t, int top, int i, int on)
{
    int x0, y0, x1, y1;
    opts_t o = tool_opts(t);
    if (top) {
        int w = 44 / o.ntop;
        x0 = TX + 4 + i * w; x1 = x0 + w - 2; y0 = TOP_Y; y1 = TOP_Y + 15;
    } else {
        x0 = TX + 4; x1 = TX + 47; y0 = ROW_Y(i); y1 = y0 + 17;
    }
    if (none(x0, y0, x1, y1))
        return;
    uint8_t fg = on ? WHITE : BLACK, bg = on ? BLACK : WHITE;
    sfill(x0, y0, x1, y1, bg);
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    if (top) {
        if (t == T_BRUSH) {
            brush_t b;
            brush_make(&b, i, 7);
            for (int r = 0; r < b.n; r++)
                sfill(cx - 3 + b.l[r], cy - 3 + r, cx - 3 + b.r[r], cy - 3 + r, fg);
        } else if (t == T_TEXT) {
            if (i) sframe(cx - 6, y0 + 1, cx + 6, y1 - 1, fg);
            else sfill(cx - 6, y0 + 1, cx + 6, y1 - 1, GREY);
            stext((cx - 4) & ~1, y0 + 1, "A", fg, TRANSP);
        } else {                            /* outline, filled, framed */
            if (i == FM_FILLED) sfill(cx - 5, cy - 4, cx + 5, cy + 4, fg);
            else {
                if (i == FM_FRAMED) sfill(cx - 5, cy - 4, cx + 5, cy + 4, GREY);
                sframe(cx - 5, cy - 4, cx + 5, cy + 4, fg);
            }
        }
        return;
    }
    switch (t) {
    case T_BRUSH: {
        brush_t b;
        int d = brush_sizes[i];
        brush_make(&b, st.brush_shape, d);
        for (int r = 0; r < b.n; r++)
            sfill(cx - d / 2 + b.l[r], cy - d / 2 + r, cx - d / 2 + b.r[r], cy - d / 2 + r, fg);
        break;
    }
    case T_ERASER: {
        int d = eraser_sizes[i];
        sframe(cx - d / 2, cy - d / 2, cx - d / 2 + d - 1, cy - d / 2 + d - 1, fg);
        break;
    }
    case T_SPRAY: {
        int r = spray_sizes[i] / 3 + 2;
        for (int y = -r; y <= r; y++)
            for (int x = -r; x <= r; x++)
                if (x * x + y * y <= r * r && ((x * 7 + y * 13) & 3) == 0)
                    spx(cx + x, cy + y, fg);
        break;
    }
    case T_LINE: case T_CURVE: case T_RECT: case T_RRECT: case T_ELLIPSE: case T_POLYGON: {
        int w = line_widths[i];
        sfill(x0 + 4, cy - w / 2, x1 - 4, cy - w / 2 + w - 1, fg);
        break;
    }
    case T_TEXT:
        stext(x0 + 4, y0 + 2, i ? "Large" : "Small", fg, bg);
        break;
    case T_SELECT:
        stext(x0 + 2, y0 + 2, i ? "Clear" : "Solid", fg, bg);
        break;
    case T_ZOOM: {
        static const char *const z[4] = { "1x", "2x", "4x", "8x" };
        stext(x0 + 14, y0 + 2, z[i], fg, bg);
        break;
    }
    }
}

void ui_draw_options(void)
{
    if (!tools_shown())
        return;
    clip_win(WIN_TOOLS);
    sfill(TX, OPT_Y0, TX + WIN_CW(WIN_TOOLS) - 1, TY + WIN_CH(WIN_TOOLS) - 1, WHITE);
    sfill(OPT_X0, OPT_Y0, OPT_X1, OPT_Y1, WHITE);
    sframe(OPT_X0, OPT_Y0, OPT_X1, OPT_Y1, BLACK);
    int t = st.tool;
    opts_t o = tool_opts(t);
    for (int i = 0; i < o.ntop; i++)
        opt_cell_draw(t, 1, i, i == top_sel(t));
    for (int i = 0; i < o.nrows; i++)
        opt_cell_draw(t, 0, i, i == row_sel(t));
}

/* ---- the colours ---- */

void ui_draw_colours(void)
{
    if (full_view || !wins[WIN_COLOURS].open)
        return;
    clip_win(WIN_COLOURS);
    sfill(KX, KY, KX + WIN_CW(WIN_COLOURS) - 1, KY + WIN_CH(WIN_COLOURS) - 1, WHITE);
    /* the background behind the foreground */
    sfill(KX + 14, KY + 24, KX + 37, KY + 47, st.bg);
    sframe(KX + 14, KY + 24, KX + 37, KY + 47, BLACK);
    sfill(KX + 2, KY + 10, KX + 25, KY + 33, st.fg);
    sframe(KX + 2, KY + 10, KX + 25, KY + 33, BLACK);
    if (st.fg == BLACK)
        sframe(KX + 3, KY + 11, KX + 24, KY + 32, WHITE);
    for (int i = 0; i < 8; i++) {
        int x = COL_X(i);
        uint8_t mk = i == WHITE || i == YELLOW || i == CYAN ? BLACK : WHITE;
        sfill(x, COL_Y, x + 15, COL_Y + 16, (uint8_t)i);
        sframe(x, COL_Y, x + 15, COL_Y + 16, BLACK);
        if (i == st.fg) sframe(x + 1, COL_Y + 1, x + 14, COL_Y + 15, mk);
        if (i == st.bg) sfill(x + 5, COL_Y + 11, x + 10, COL_Y + 13, mk);
    }
    uint8_t t[4];
    two_tbl(t, st.fg, st.bg);
    for (int i = 0; i < NPAT; i++) {
        int x = PAT_X(i), y0 = PAT_Y(i);
        if (none(x - 1, y0 - 1, x + 16, y0 + 17))
            continue;
        if (whole(x, y0, x + 15, y0 + 16)) {
            damage(x, y0, x + 15, y0 + 16);
            for (int y = y0 + 1; y < y0 + 16; y++) {
                uint8_t *p = SCREEN + rowoff[y] + (unsigned)(x >> 1);
                uint8_t b = patterns[i][y & 7];
                bits8(p, b, t);
                bits8(p + 4, b, t);         /* (the frame covers the ends) */
            }
        } else
            for (int y = y0 + 1; y < y0 + 16; y++)
                for (int k = 1; k < 15; k++)
                    spx(x + k, y, (patterns[i][y & 7] & (0x80 >> ((x + k) & 7))) ? st.fg : st.bg);
        sframe(x, y0, x + 15, y0 + 16, BLACK);
        sframe(x - 1, y0 - 1, x + 16, y0 + 17, i == st.pat ? BLACK : WHITE);
    }
}

static void draw_status(void)
{
    if (full_view)
        return;
    char buf[24];
    const char *s = msg_on ? status_text : coord_text;
    int n = (int)strlen(s);
    if (n > STATUS_N) n = STATUS_N;
    memcpy(buf, s, (unsigned)n);
    while (n < STATUS_N) buf[n++] = ' ';
    buf[n] = 0;
    clip_screen();
    stext(STATUS_X, 0, buf, BLACK, WHITE);
}

static char *put_num(char *p, int v)
{
    char t[8];
    int n = 0;
    if (v < 0) { *p++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = t[--n];
    return p;
}

void ui_coords(int x, int y, int w, int h)
{
    char buf[24], *p = buf;
    if (x >= 0 && y >= 0 && x < DW && y < DH) {
        p = put_num(p, x);
        *p++ = ',';
        p = put_num(p, y);
    }
    if (w >= 0) {
        *p++ = ' ';
        p = put_num(p, w);
        *p++ = 'x';
        p = put_num(p, h);
    }
    *p = 0;
    if (strcmp(buf, coord_text)) {
        strcpy(coord_text, buf);
        if (!msg_on)
            draw_status();
    }
}

void ui_message(const char *s)
{
    strncpy(status_text, s, STATUS_N);
    status_text[STATUS_N] = 0;
    msg_on = 1;
    msg_until = (uint16_t)(FRAME + 120);
    draw_status();
}

void ui_status(const char *s) { ui_message(s); }

void ui_tick(void)
{
    if (msg_on && (int16_t)(FRAME - msg_until) >= 0) {
        msg_on = 0;
        draw_status();
    }
}

void ui_draw_all(void)
{
    if (full_view) {
        render_view();
        return;
    }
    win_draw_all();
}

int ui_hit(int x, int y, int *idx, int *win)
{
    *idx = -1;
    *win = -1;
    if (full_view)
        return HIT_CANVAS;
    if (y < MENU_H + 1) {
        *idx = menu_title_at(x);
        return *idx >= 0 ? HIT_MENU : HIT_NONE;
    }
    int part, w = win_at(x, y, &part);
    *win = w;
    if (w < 0)
        return HIT_DESK;
    switch (part) {
    case PART_TITLE: return HIT_TITLE;
    case PART_CLOSE: return HIT_CLOSE;
    case PART_GROW: return HIT_GROW;
    case PART_VSCROLL: return HIT_VSCROLL;
    case PART_HSCROLL: return HIT_HSCROLL;
    }
    if (w >= WIN_PIC0)
        return HIT_CANVAS;
    if (w == WIN_COLOURS) {
        if (x >= KX + 2 && x <= KX + 37 && y >= KY + 10 && y <= KY + 47)
            return HIT_SWAP;
        for (int i = 0; i < 8; i++)
            if (x >= COL_X(i) && x <= COL_X(i) + 15 && y >= COL_Y && y <= COL_Y + 16) { *idx = i; return HIT_COLOUR; }
        for (int i = 0; i < NPAT; i++)
            if (x >= PAT_X(i) && x <= PAT_X(i) + 15 && y >= PAT_Y(i) && y <= PAT_Y(i) + 16) { *idx = i; return HIT_PATTERN; }
        return HIT_WIN;
    }
    for (int i = 0; i < NTOOLS; i++)
        if (x >= TOOL_X(i) && x <= TOOL_X(i) + 22 && y >= TOOL_Y(i) && y <= TOOL_Y(i) + 20) {
            *idx = i;
            return HIT_TOOL;
        }
    opts_t o = tool_opts(st.tool);
    if (o.ntop && y >= TOP_Y && y <= TOP_Y + 15 && x >= TX + 4 && x < TX + 4 + 44) {
        *idx = (x - TX - 4) / (44 / o.ntop);
        if (*idx >= o.ntop) *idx = o.ntop - 1;
        return HIT_OPT_TOP;
    }
    for (int i = 0; i < o.nrows; i++)
        if (y >= ROW_Y(i) && y <= ROW_Y(i) + 17 && x >= TX + 4 && x <= TX + 47) { *idx = i; return HIT_OPT_ROW; }
    if (y >= OPT_Y0 && y <= OPT_Y1)
        return HIT_OPT_ROW;                 /* (idx -1: the box, for the wheel) */
    return HIT_WIN;
}

/* ---- help ---- */
static const char *const help_lines[] = {
    "fpga_cpu Paint",
    "",
    "Left button     draw in the foreground colour",
    "Right button    draw in the background colour",
    "Middle button   drag: move the picture; click: show it all",
    "Wheel           on the picture: zoom; on a scroll bar:",
    "                scroll; on the colours, patterns, tools or",
    "                options: step through them",
    "Windows: drag the title bar to move one, the box at the",
    "bottom right to size a picture's; Window: New view.",
    "",
    "Shapes: Shift for squares, circles and 45 degrees.",
    "Polygon: click the corners; double-click, right-click",
    "or click the first corner to close it. Curve: draw the",
    "line, then drag twice to bend it.",
    "Select: drag a box, drag it to move it (right: copy).",
    "",
    "Keys: S I P B A X F T L C R O E G Z H pick the tools,",
    "0-7 colours, [ ] sizes, + - zoom, arrows move the view,",
    "Tab shows the picture, Esc cancels, Ctrl-Z/Y undo/redo.",
    "",
    "Pictures: apps/paint/tools/paint.py save/open over JTAG.",
    "",
    "                  Click to continue",
};

void help_show(void)
{
    clip_screen();
    int n = sizeof help_lines / sizeof help_lines[0];
    int w = 58 * 8, h = n * 14 + 16;
    int x0 = (DW - w) / 2 & ~1, y0 = (DH - h) / 2;
    sfill(x0 + 4, y0 + 4, x0 + w + 3, y0 + h + 3, BLACK);
    sfill(x0, y0, x0 + w - 1, y0 + h - 1, WHITE);
    sframe(x0, y0, x0 + w - 1, y0 + h - 1, BLACK);
    sframe(x0 + 2, y0 + 2, x0 + w - 3, y0 + h - 3, BLACK);
    for (int i = 0; i < n; i++)
        stext(x0 + 16, y0 + 8 + 14 * i, help_lines[i], i == 0 ? BLUE : BLACK, WHITE);
}
