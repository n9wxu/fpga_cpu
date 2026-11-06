/* The tracker screen: bitmap panels, meters and scopes (hi-color 8 bpp, 640x360) under the 80x25 text layer.
 * Only what changes is drawn: the text goes through a shadow of the screen, the meters redraw their difference. */
#include <string.h>
#include "ui.h"
#include "layout.h"

#define BMP  ((volatile uint8_t *)0x0C3C00UL)
#define TEXT ((volatile uint16_t *)0x0FC000UL)
#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))

#define ROW_CUR 10                      /* text row of the playing pattern row */
#define ROW_TOP 4                       /* first pattern row on screen */
#define ROW_BOT 16

/* palette indices */
#define C_BG 1
#define C_PAT 2
#define C_PANEL 3
#define C_BORDER 4
#define C_TRACK 5
#define C_BAR 6
#define C_BAREDGE 7
#define C_HDR 8                         /* 8..15 */
#define C_VU 16                         /* 16..39 */
#define C_V1 40                         /* 40..42 bright, 43..45 dim */
#define C_V1DIM 43
#define C_WHITE 46
#define C_GRID 49

/* text colors */
#define T_BLK 0
#define T_BLU 1
#define T_GRN 2
#define T_CYN 3
#define T_RED 4
#define T_MAG 5
#define T_YEL 6
#define T_WHT 7
#define A(fg) ((uint16_t)(0x40 | (fg)) << 8)         /* transparent background */

static const uint8_t vcol[3] = {T_YEL, T_CYN, T_MAG};

#define tsh ((uint16_t *)SDR_TSHADOW)                /* what the text layer shows (SDRAM: it is only compared) */

/* ------------------------------------------------------------------ bitmap */
__attribute__((noinline)) static void b_fill(int x, int y, int w, int h, uint8_t c)
{
    uint16_t pair = (uint16_t)(c | c << 8);
    int j;
    for (j = 0; j < h; j++) {
        volatile uint8_t *p = BMP + (uint32_t)(y + j) * 640 + x;
        int n = w;
        if ((x & 1) && n > 0) {
            *p++ = c;
            n--;
        }
        {
            volatile uint16_t *q = (volatile uint16_t *)p;
            while (n >= 2) {
                *q++ = pair;
                n -= 2;
            }
            p = (volatile uint8_t *)q;
        }
        if (n)
            *p = c;
    }
}

__attribute__((noinline)) static void b_rect(int x, int y, int w, int h, uint8_t fill, uint8_t edge)
{
    b_fill(x, y, w, h, edge);
    b_fill(x + 1, y + 1, w - 2, h - 2, fill);
}

__attribute__((noinline)) static void pal_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
    IO(0x210) = i;
    IO(0x212) = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

static void palette(void)
{
    uint8_t i;
    pal_set(0, 0, 0, 0);
    pal_set(C_BG, 4, 6, 16);
    pal_set(C_PAT, 8, 10, 26);
    pal_set(C_PANEL, 12, 16, 40);
    pal_set(C_BORDER, 50, 80, 160);
    pal_set(C_TRACK, 20, 26, 58);
    pal_set(C_BAR, 34, 44, 112);
    pal_set(C_BAREDGE, 90, 120, 230);
    for (i = 0; i < 8; i++)
        pal_set(C_HDR + i, 10 + i * 4, 20 + i * 8, 70 + i * 17);
    for (i = 0; i < 24; i++) {
        uint8_t r, g;
        if (i < 14) { r = 40 + i * 14; g = 220; }           /* green -> yellow */
        else { r = 240; g = 230 - (i - 14) * 18; }          /* yellow -> red */
        pal_set(C_VU + i, r, g, 50);
    }
    pal_set(C_V1, 255, 196, 60);
    pal_set(C_V1 + 1, 60, 220, 235);
    pal_set(C_V1 + 2, 235, 90, 225);
    pal_set(C_V1DIM, 85, 65, 20);
    pal_set(C_V1DIM + 1, 20, 73, 78);
    pal_set(C_V1DIM + 2, 78, 30, 75);
    pal_set(C_WHITE, 255, 255, 255);
    pal_set(47, 170, 176, 200);
    pal_set(C_GRID, 28, 36, 84);
}

static void background(void)
{
    int i, v;
    b_fill(0, 0, 640, 360, C_BG);
    for (i = 0; i < 28; i++)                                /* title bar gradient */
        b_fill(0, i, 640, 1, (uint8_t)(C_HDR + i * 8 / 28));
    b_fill(0, 41, 640, 1, C_BORDER);
    /* pattern area */
    b_rect(0, 43, 440, 196, C_PAT, C_BORDER);
    b_fill(1, 44, 438, 13, C_TRACK);
    b_fill(1, ROW_CUR * 14, 438, 14, C_BAR);
    b_fill(1, ROW_CUR * 14, 438, 1, C_BAREDGE);
    b_fill(1, ROW_CUR * 14 + 13, 438, 1, C_BAREDGE);
    b_fill(24, 44, 1, 194, C_GRID);
    for (v = 0; v < 3; v++)
        b_fill((19 + 17 * v) * 8 + 3, 44, 1, 194, C_GRID);
    /* instrument list */
    b_rect(442, 43, 198, 196, C_PANEL, C_BORDER);
    b_fill(443, 44, 196, 13, C_TRACK);
    /* voice panels */
    for (v = 0; v < 3; v++) {
        int x = 4 + 152 * v;
        b_rect(x, 239, 152, 121, C_PANEL, C_BORDER);
        b_fill(x + 1, 240, 150, 13, C_V1DIM + v);
        b_fill(x + 4, 295, 144, 10, C_TRACK);
        b_rect(x + 4, 307, 144, 46, C_PAT, C_TRACK);
    }
    /* filter / master panel */
    b_rect(460, 239, 178, 121, C_PANEL, C_BORDER);
    b_fill(461, 240, 176, 13, C_TRACK);
    b_fill(464, 295, 170, 10, C_TRACK);
    b_fill(464, 325, 170, 10, C_TRACK);
}

/* ------------------------------------------------------------------ text */
__attribute__((noinline)) static void t_set(int col, int row, uint16_t cell)
{
    int i = row * 80 + col;
    if (tsh[i] != cell) {
        tsh[i] = cell;
        TEXT[i] = cell;
    }
}

__attribute__((noinline)) static void t_str(int col, int row, const char *s, uint8_t fg)
{
    while (*s)
        t_set(col++, row, (uint16_t)(A(fg) | (uint8_t)*s++));
}

__attribute__((noinline)) static void t_hex(int col, int row, uint16_t v, int digits, uint8_t fg)
{
    int i;
    for (i = digits - 1; i >= 0; i--, v >>= 4)
        t_set(col + i, row, (uint16_t)(A(fg) | "0123456789ABCDEF"[v & 15]));
}

__attribute__((noinline)) static void t_dec(int col, int row, uint16_t v, int digits, uint8_t fg, char pad)
{
    int i;
    for (i = digits - 1; i >= 0; i--, v /= 10)
        t_set(col + i, row, (uint16_t)(A(fg) | (uint8_t)((v || i == digits - 1 || pad == '0') ? '0' + v % 10 : pad)));
}

__attribute__((noinline)) static void t_fill(int col, int row, int n, uint8_t fg, char ch)
{
    while (n-- > 0)
        t_set(col++, row, (uint16_t)(A(fg) | (uint8_t)ch));
}

/* ------------------------------------------------------------------ pattern view */
static uint16_t last_sec = 0xFFFF, last_late = 0xFFFF;
static uint8_t last_load = 0xFF;
static void forget_panels(void);
static void header_static(const ui_stat_t *s);
static int16_t shown_row = -2;                      /* the current row drawn last */
static uint8_t v_inst[3];                           /* instrument last started by each voice */
static int16_t inst_row = -1;                       /* row v_inst is valid for */

/* one pattern row is built in a line buffer and then copied to the screen where it differs */
static uint16_t lb[55];

static void lb_str(uint16_t *p, const char *s, uint8_t fg)
{
    uint16_t a = A(fg);
    while (*s)
        *p++ = (uint16_t)(a | (uint8_t)*s++);
}

static void lb_hex(uint16_t *p, uint8_t v, uint8_t fg)
{
    static const char hx[] = "0123456789ABCDEF";
    p[0] = (uint16_t)(A(fg) | (uint8_t)hx[v >> 4]);
    p[1] = (uint16_t)(A(fg) | (uint8_t)hx[v & 15]);
}

static void lb_cell(uint16_t *p, const cell_t *c, uint8_t dim)
{
    uint8_t n = c->note, k;
    uint16_t sp = A(T_BLU) | ' ';
    if (n == 0)
        lb_str(p, "...", T_BLU);
    else if (n == 0xFE)
        lb_str(p, "---", dim ? T_BLU : T_RED);
    else
        lb_str(p, note_name(n), dim ? T_BLU : T_WHT);
    p[3] = sp;
    if (c->inst)
        lb_hex(p + 4, c->inst, dim ? T_BLU : T_CYN);
    else
        lb_str(p + 4, "..", T_BLU);
    p[6] = sp;
    if (c->wave) {
        static const char wl[4] = {'T', 'S', 'P', 'N'};
        for (k = 0; k < 4; k++)
            p[7 + k] = (c->wave & (0x10 << k)) ? (uint16_t)(A(dim ? T_BLU : T_GRN) | (uint8_t)wl[k]) : (uint16_t)(A(T_BLU) | '-');
        p[11] = (uint16_t)(A(dim ? T_BLU : T_YEL) | ((c->wave & 4) ? ((c->wave & 2) ? '*' : 'R') : ((c->wave & 2) ? 'Y' : '-')));
    } else {
        lb_str(p + 7, ".....", T_BLU);
    }
    p[12] = sp;
    if (c->fxc) {
        p[13] = (uint16_t)(A(dim ? T_BLU : T_MAG) | c->fxc);
        lb_hex(p + 14, c->fxv, dim ? T_BLU : T_MAG);
    } else {
        lb_str(p + 13, "...", T_BLU);
    }
}

__attribute__((noinline)) static void t_line(int row, uint16_t *p, int n)
{
    uint16_t *s = tsh + row * 80;
    volatile uint16_t *t = TEXT + row * 80;
    int i;
    for (i = 0; i < n; i++)
        if (s[i] != p[i]) {
            s[i] = p[i];
            t[i] = p[i];
        }
}

static void draw_pattern(int16_t cur)
{
    int i, v;
    uint16_t blank = A(T_BLU) | ' ';
    for (i = ROW_TOP; i <= ROW_BOT; i++) {
        int16_t a = cur - (ROW_CUR - i);
        const row_t *r = a >= 0 ? model_row((uint16_t)a) : 0;
        uint8_t dim = (i < ROW_CUR - 3 || i > ROW_CUR + 3);
        if (!r) {
            for (v = 0; v < 55; v++)
                lb[v] = blank;
        } else {
            lb_hex(lb, (uint8_t)a, (a & 3) == 0 ? (dim ? T_BLU : T_WHT) : T_BLU);
            lb[2] = (i == ROW_CUR) ? (uint16_t)(A(T_WHT) | 0x10) : blank;
            for (v = 0; v < 3; v++) {
                lb_cell(lb + 3 + 17 * v, &r->c[v], dim);
                lb[3 + 17 * v + 16] = (uint16_t)(A(T_BLU) | 0xB3);
            }
        }
        t_line(i, lb, 55);
    }
}

static void draw_insts(void)
{
    uint8_t n = model_ninst(), i, v;
    static const char wl[4] = {'T', 'S', 'P', 'N'};
    for (i = 1; i < 14; i++) {
        int row = 3 + i;
        if (i < n) {
            const inst_t *t = model_inst(i);
            uint8_t k;
            t_hex(56, row, i, 2, T_CYN);
            for (k = 0; k < 4; k++)
                t_set(59 + k, row, (uint16_t)((t->ctrl & (0x10 << k)) ? (A(T_GRN) | (uint8_t)wl[k]) : (A(T_BLU) | '-')));
            t_hex(64, row, t->ad, 2, T_YEL);
            t_hex(67, row, t->sr, 2, T_YEL);
            t_set(70, row, (uint16_t)((t->ctrl & 4) ? (A(T_MAG) | 'R') : (A(T_BLU) | '-')));
            t_set(71, row, (uint16_t)((t->ctrl & 2) ? (A(T_MAG) | 'Y') : (A(T_BLU) | '-')));
            for (v = 0; v < 3; v++)
                t_set(73 + v, row, (uint16_t)(v_inst[v] == i ? (A(vcol[v]) | (uint8_t)('1' + v)) : (A(T_BLU) | 0xFA)));
        } else {
            t_fill(56, row, 24, T_BLU, ' ');
        }
    }
}

/* ------------------------------------------------------------------ meters */
static uint8_t vu_len[3];
static uint8_t scope_x[3];
static uint8_t cut_len, vol_len;

static void vu_set(uint8_t v, uint8_t len)
{
    int x0 = 8 + 152 * v, y0 = 296, i, j;
    uint8_t old = vu_len[v];
    if (len == old)
        return;
    if (len > old) {
        for (i = old; i < len; i++)
            for (j = 0; j < 8; j++)
                BMP[(uint32_t)(y0 + j) * 640 + x0 + i] = (uint8_t)(C_VU + i / 6);
    } else {
        b_fill(x0 + len, y0, old - len, 8, C_TRACK);
    }
    vu_len[v] = len;
}

static void scope_col(uint8_t v, uint16_t lvl)
{
    int x = 8 + 152 * v + 1 + scope_x[v], y0 = 308, j;
    int h = (int)(((uint32_t)lvl * 42) >> 16);
    volatile uint8_t *p = BMP + (uint32_t)y0 * 640 + x;
    for (j = 0; j < 44; j++, p += 640) {
        int fromb = 43 - j;
        *p = (fromb < h) ? (uint8_t)(C_V1 + v) : ((fromb == h && h < 42) ? (uint8_t)(C_V1DIM + v) : (uint8_t)C_PAT);
    }
    scope_x[v]++;
    if (scope_x[v] >= 142)
        scope_x[v] = 0;
    x = 8 + 152 * v + 1 + scope_x[v];
    p = BMP + (uint32_t)y0 * 640 + x;
    for (j = 0; j < 44; j++, p += 640)
        *p = C_WHITE;
}

static void bar_set(int x0, int y0, int w, uint8_t *cur, uint8_t len, uint8_t col)
{
    if (len == *cur)
        return;
    if (len > *cur)
        b_fill(x0 + *cur, y0, len - *cur, 8, col);
    else
        b_fill(x0 + len, y0, *cur - len, 8, C_TRACK);
    *cur = len;
    (void)w;
}

/* ------------------------------------------------------------------ init */
void ui_clear_dynamic(void)
{
    uint8_t v;
    for (v = 0; v < 3; v++) {
        b_fill(8 + 152 * v, 296, 144, 8, C_TRACK);
        b_fill(8 + 152 * v + 1, 308, 142, 44, C_PAT);
        vu_len[v] = 0;
        scope_x[v] = 0;
        v_inst[v] = 0;
    }
    b_fill(464, 296, 168, 8, C_TRACK);
    b_fill(464, 326, 168, 8, C_TRACK);
    cut_len = vol_len = 0;
    shown_row = -2;
    inst_row = -1;
}

void ui_init(void)
{
    int i;
    IO(0x200) = 0;
    IO(0x202) = 0;                                  /* black backdrop */
    IO(0x204) = 0xC000; IO(0x206) = 0x0F;           /* VID_TEXT = 0x0FC000 */
    IO(0x208) = 0x3C00; IO(0x20A) = 0x0C;           /* VID_BMP = 0x0C3C00 */
    IO(0x214) = 0;
    IO(0x216) = 0;
    palette();
    for (i = 0; i < 2000; i++) {
        tsh[i] = 0x4020;
        TEXT[i] = 0x4020;
    }
    background();
    IO(0x200) = 1 | 2 | 4;                          /* text, bitmap, hi-color; no cursor */
}

/* ------------------------------------------------------------------ header / tune */
void ui_message(const char *s, uint8_t fg)
{
    t_fill(1, 2, 79, fg, ' ');
    t_str(1, 2, s, fg);
}

void ui_tune(const psid_t *p, const ui_stat_t *s)
{
    int v;
    char nm[33];
    (void)s;
    ui_clear_dynamic();
    forget_panels();
    t_fill(0, 0, 80, T_WHT, ' ');
    t_fill(0, 1, 80, T_WHT, ' ');
    t_str(1, 0, "SID TRACKER", T_WHT);
    t_str(1, 1, "", T_YEL);
    memcpy(nm, p->name, 32);
    nm[32] = 0;
    t_str(1, 1, nm, T_YEL);
    memcpy(nm, p->author, 32);
    t_str(34, 1, "by", T_BLU);
    t_str(37, 1, nm, T_CYN);
    memcpy(nm, p->released, 32);
    nm[14] = 0;
    t_str(66, 1, nm, T_GRN);
    header_static(s);
    ui_message("KEYS: 1-3 MUTE  S STEREO  N/P SONG  T TUNE  SPACE PAUSE    S1 SONG  S2 TUNE", T_CYN);
    t_str(0, 3, "ROW", T_WHT);
    for (v = 0; v < 3; v++) {
        t_str(3 + 17 * v, 3, "NOT IN WAVE  FX", vcol[v]);
        t_set(3 + 17 * v + 16, 3, (uint16_t)(A(T_BLU) | 0xB3));
    }
    t_str(56, 3, "INSTRUMENTS", T_WHT);
    t_str(56, 3 + 14 - 1 + 0, "", T_BLU);
    for (v = 0; v < 3; v++) {
        char t[8] = "VOICE n";
        t[6] = (char)('1' + v);
        t_str(1 + 19 * v, 17, t, T_WHT);
    }
    t_str(59, 17, "FILTER / MASTER", T_WHT);
    t_str(56 + 0, 3, "INSTRUMENTS", T_WHT);
    for (v = 4; v < 17; v++)
        t_fill(0, v, 55, T_BLU, ' ');
    for (v = 4; v < 17; v++)
        t_fill(56, v, 24, T_BLU, ' ');
}

/* ------------------------------------------------------------------ per frame */
static uint8_t pcache[3][9], fcache[8];
static void forget_panels(void)
{
    memset(pcache, 0xFF, sizeof pcache);
    memset(fcache, 0xFF, sizeof fcache);
    last_sec = last_late = 0xFFFF;
    last_load = 0xFF;
}

static void panel_text(const frame_t *pf, const ui_stat_t *s)
{
    uint8_t v;
    const uint8_t *r = pf->r;
    for (v = 0; v < 3; v++) {
        const uint8_t *q = r + v * 7;
        int c0 = 1 + 19 * v;
        uint8_t ctrl = q[4], k;
        uint16_t fn = (uint16_t)(q[0] + q[1] * 256u), pw = (uint16_t)(q[2] + (q[3] & 15) * 256u);
        static const char wl[4] = {'T', 'S', 'P', 'N'};
        uint8_t key[9];
        memcpy(key, q, 7);
        key[7] = (uint8_t)(((r[23] >> v) & 1) | (((s->mute >> v) & 1) << 1) | (((r[24] >> 7) & 1) << 2));
        key[8] = 0;
        if (!memcmp(key, pcache[v], 9))
            continue;
        memcpy(pcache[v], key, 9);
        /* line 1: note and frequency */
        {
            uint8_t nn = model_nearest(fn);
            if (nn)
                t_str(c0, 18, note_name(nn), (ctrl & 1) ? T_WHT : T_BLU);
            else
                t_str(c0, 18, "...", T_BLU);
        }
        t_hex(c0 + 4, 18, fn, 4, vcol[v]);
        t_set(c0 + 3, 18, A(T_BLU) | ' ');
        t_str(c0 + 9, 18, (ctrl & 1) ? "GATE" : "    ", T_GRN);
        if (ctrl & 8)
            t_str(c0 + 14, 18, "TST", T_RED);
        else
            t_str(c0 + 14, 18, "   ", T_RED);
        /* line 2: pulse width and ADSR */
        t_str(c0, 19, "PW", T_BLU);
        t_hex(c0 + 3, 19, pw, 3, (ctrl & 0x40) ? T_WHT : T_BLU);
        t_str(c0 + 7, 19, "A", T_BLU); t_hex(c0 + 8, 19, q[5] >> 4, 1, T_YEL);
        t_str(c0 + 9, 19, "D", T_BLU); t_hex(c0 + 10, 19, q[5] & 15, 1, T_YEL);
        t_str(c0 + 11, 19, "S", T_BLU); t_hex(c0 + 12, 19, q[6] >> 4, 1, T_YEL);
        t_str(c0 + 13, 19, "R", T_BLU); t_hex(c0 + 14, 19, q[6] & 15, 1, T_YEL);
        /* line 3: waveform, ring, sync, filter */
        for (k = 0; k < 4; k++)
            t_set(c0 + k, 20, (uint16_t)((ctrl & (0x10 << k)) ? (A(T_GRN) | (uint8_t)wl[k]) : (A(T_BLU) | '-')));
        t_str(c0 + 5, 20, "RING", (ctrl & 4) ? T_MAG : T_BLU);
        t_str(c0 + 10, 20, "SYN", (ctrl & 2) ? T_MAG : T_BLU);
        t_str(c0 + 14, 20, "F", (r[23] >> v) & 1 ? T_CYN : T_BLU);
        t_set(c0 + 15, 20, (uint16_t)(A(T_RED) | ((s->mute >> v) & 1 ? 'M' : ' ')));
    }
    /* filter */
    {
        uint8_t fk[8];
        fk[0] = r[21] & 7; fk[1] = r[22]; fk[2] = r[23]; fk[3] = r[24]; fk[4] = s->spread; fk[5] = s->paused; fk[6] = s->has_engine; fk[7] = 0;
        if (!memcmp(fk, fcache, 8))
            return;
        memcpy(fcache, fk, 8);
    }
    {
        uint16_t fc = (uint16_t)((r[21] & 7) + r[22] * 8u);
        uint8_t m = r[24] >> 4, vol = r[24] & 15;
        t_str(59, 18, "CUT", T_BLU);
        t_hex(63, 18, fc, 3, T_YEL);
        t_str(67, 18, "RES", T_BLU);
        t_hex(71, 18, r[23] >> 4, 1, T_YEL);
        t_str(59, 19, "LP", (m & 1) ? T_GRN : T_BLU);
        t_str(62, 19, "BP", (m & 2) ? T_GRN : T_BLU);
        t_str(65, 19, "HP", (m & 4) ? T_GRN : T_BLU);
        t_str(68, 19, "3OFF", (m & 8) ? T_RED : T_BLU);
        t_str(59, 20, "ROUTE", T_BLU);
        t_str(65, 20, "1", (r[23] & 1) ? vcol[0] : T_BLU);
        t_str(67, 20, "2", (r[23] & 2) ? vcol[1] : T_BLU);
        t_str(69, 20, "3", (r[23] & 4) ? vcol[2] : T_BLU);
        t_str(71, 20, "EXT", (r[23] & 8) ? T_MAG : T_BLU);
        t_str(59, 22, "VOL", T_BLU);
        t_hex(63, 22, vol, 1, T_WHT);
        t_str(66, 22, "STEREO", T_BLU);
        t_dec(73, 22, (uint16_t)((uint32_t)s->spread * 100 / 255), 3, T_WHT, ' ');
        t_str(76, 22, "%", T_BLU);
        t_str(59, 24, s->has_engine ? "ENGINE ENV" : "SIM ENV", T_BLU);
        t_str(71, 24, s->paused ? "PAUSED" : "      ", T_YEL);
    }
}

static void header_static(const ui_stat_t *s)
{
    t_str(14, 0, "SONG", T_CYN);
    t_dec(19, 0, s->song, 2, T_WHT, '0');
    t_str(21, 0, "/", T_CYN);
    t_dec(22, 0, s->songs, 2, T_WHT, '0');
    t_str(26, 0, "TUNE", T_CYN);
    t_dec(31, 0, s->tune, 1, T_WHT, '0');
    t_str(32, 0, "/", T_CYN);
    t_dec(33, 0, s->tunes, 1, T_WHT, '0');
    t_str(36, 0, "TIME", T_CYN);
    t_str(43, 0, ":", T_CYN);
    t_str(47, 0, pl_clock_ntsc ? "NTSC" : "PAL", T_CYN);
    t_dec(52, 0, (uint16_t)(pl_rate_x100 / 100), 2, T_CYN, ' ');
    t_str(54, 0, "HZ", T_CYN);
    t_str(58, 0, "CPU", T_CYN);
    t_str(73, 0, "%", T_CYN);
    t_str(75, 0, "L", T_CYN);
}

static void header(const ui_stat_t *s)
{
    uint16_t sec = (uint16_t)(s->play_ms / 1000);
    if (sec != last_sec) {
        last_sec = sec;
        t_dec(41, 0, sec / 60, 2, T_WHT, '0');
        t_dec(44, 0, sec % 60, 2, T_WHT, '0');
    }
    if (s->load != last_load) {
        last_load = s->load;
        t_dec(70, 0, s->load, 3, s->load > 90 ? T_RED : T_WHT, ' ');
    }
    if (s->late != last_late) {
        last_late = s->late;
        t_dec(76, 0, s->late > 999 ? 999 : s->late, 3, s->late ? T_RED : T_CYN, ' ');
    }
}

void ui_frame(const frame_t *pf, uint32_t play_frame, const ui_stat_t *s)
{
    int16_t cur = model_row_at(play_frame);
    uint8_t v;
    static uint8_t cpu_len;
    if (cur != shown_row) {
        /* the instruments voices started, up to this row */
        int16_t a;
        for (a = (inst_row < 0 || cur - inst_row > 40) ? (cur > 40 ? cur - 40 : 0) : inst_row + 1; a <= cur && cur >= 0; a++) {
            const row_t *r = model_row((uint16_t)a);
            if (r)
                for (v = 0; v < 3; v++)
                    if (r->c[v].inst)
                        v_inst[v] = r->c[v].inst;
        }
        inst_row = cur;
        shown_row = cur;
        draw_pattern(cur);
        draw_insts();
    }
    header(s);
    panel_text(pf, s);
    /* CPU bar in the title */
    {
        uint8_t len = (uint8_t)(s->load > 100 ? 100 : s->load);
        if (len != cpu_len) {
            int i;
            for (i = 0; i < 100; i++)
                if ((i < len) != (i < cpu_len))
                    b_fill(512 + i * 1, 5, 1, 8, (i < len) ? (uint8_t)(C_VU + i / 5 + (i > 99 ? 0 : 0)) : (uint8_t)C_HDR);
            cpu_len = len;
        }
    }
    for (v = 0; v < 3; v++) {
        uint16_t env = sm_env(v);
        vu_set(v, (uint8_t)(((uint32_t)env * 144) >> 16));
        scope_col(v, env);
    }
    {
        uint16_t fc = (uint16_t)((pf->r[21] & 7) + pf->r[22] * 8u);
        bar_set(464, 296, 168, &cut_len, (uint8_t)((uint32_t)fc * 168 / 2048), C_V1 + 1);
        bar_set(464, 326, 168, &vol_len, (uint8_t)((pf->r[24] & 15) * 168 / 15), C_WHITE);
    }
}
