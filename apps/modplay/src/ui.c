/* The tracker screen: an 8 bpp bitmap (panels, the current-row bar, VU meters) under the 80x25 text layer.
 * Text goes through a shadow of the screen, so only what changes is written; the meters redraw their difference.
 *
 *  row 0   title, position, pattern, row          row 1  speed, BPM, time, song, CPU
 *  row 2   keys / messages                        row 3  column heads
 *  rows 4-18  the pattern (row 11 is the one playing), 4 channels; the sample list on the right
 *  rows 20-24 the channel panels: note, sample, volume, effect, the sample's name, a VU meter */
#include <string.h>
#include "ui.h"

#define BMP  ((volatile uint8_t *)0x0C3C00UL)
#define TEXT ((volatile uint16_t *)0x0FC000UL)
#define TSH  ((uint16_t *)0x032000UL)           /* shadow of the text layer (SDRAM, only compared) */
#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))

#define ROW_TOP 4
#define ROW_CUR 11
#define NROWS 15
#define PCOL(c) (3 + 12 * (c))                  /* a channel's column in the pattern view */
#define SCOL 54                                 /* sample list */

/* palette */
#define C_BG 1
#define C_PAT 2
#define C_PANEL 3
#define C_BORDER 4
#define C_TRACK 5
#define C_BAR 6
#define C_BAREDGE 7
#define C_HDR 8                                 /* 8..15 */
#define C_VU 16                                 /* 16..39 */
#define C_CH 40                                 /* 40..43 channel colours, 44..47 dim */
#define C_GRID 49
#define C_OFF 50

/* text colours */
#define T_BLU 1
#define T_GRN 2
#define T_CYN 3
#define T_RED 4
#define T_MAG 5
#define T_YEL 6
#define T_WHT 7
#define A(fg) ((uint16_t)(0x40 | (fg)) << 8)    /* transparent background */

static const uint8_t ccol[4] = {T_YEL, T_CYN, T_MAG, T_GRN};
static const char note_names[12][3] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

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

static void b_rect(int x, int y, int w, int h, uint8_t fill, uint8_t edge)
{
    b_fill(x, y, w, h, edge);
    b_fill(x + 1, y + 1, w - 2, h - 2, fill);
}

static void pal_set(uint8_t i, uint8_t r, uint8_t g, uint8_t b)
{
    IO(0x210) = i;
    IO(0x212) = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

static void palette(void)
{
    uint8_t i;
    pal_set(0, 0, 0, 0);
    pal_set(C_BG, 6, 4, 14);
    pal_set(C_PAT, 10, 8, 24);
    pal_set(C_PANEL, 16, 12, 38);
    pal_set(C_BORDER, 120, 70, 170);
    pal_set(C_TRACK, 28, 20, 56);
    pal_set(C_BAR, 60, 34, 110);
    pal_set(C_BAREDGE, 200, 120, 240);
    for (i = 0; i < 8; i++)
        pal_set(C_HDR + i, 30 + i * 12, 10 + i * 4, 60 + i * 20);
    for (i = 0; i < 24; i++) {
        uint8_t r, g;
        if (i < 14) { r = (uint8_t)(40 + i * 14); g = 220; }
        else { r = 240; g = (uint8_t)(230 - (i - 14) * 18); }
        pal_set(C_VU + i, r, g, 50);
    }
    pal_set(C_CH + 0, 255, 196, 60);
    pal_set(C_CH + 1, 60, 220, 235);
    pal_set(C_CH + 2, 235, 90, 225);
    pal_set(C_CH + 3, 90, 230, 110);
    pal_set(C_CH + 4, 85, 65, 20);
    pal_set(C_CH + 5, 20, 73, 78);
    pal_set(C_CH + 6, 78, 30, 75);
    pal_set(C_CH + 7, 30, 76, 36);
    pal_set(C_GRID, 44, 30, 84);
    pal_set(C_OFF, 24, 18, 44);
}

static void background(void)
{
    int i, c;
    b_fill(0, 0, 640, 360, C_BG);
    for (i = 0; i < 28; i++)
        b_fill(0, i, 640, 1, (uint8_t)(C_HDR + i * 8 / 28));
    b_fill(0, 41, 640, 1, C_BORDER);
    /* pattern */
    b_rect(0, 43, 424, 226, C_PAT, C_BORDER);
    b_fill(1, 44, 422, 12, C_TRACK);
    b_fill(1, ROW_CUR * 14, 422, 14, C_BAR);
    b_fill(1, ROW_CUR * 14, 422, 1, C_BAREDGE);
    b_fill(1, ROW_CUR * 14 + 13, 422, 1, C_BAREDGE);
    for (c = 0; c < 4; c++) {
        b_fill(PCOL(c) * 8 + 3, 44, 1, 224, C_GRID);
        b_fill(PCOL(c) * 8 + 8, 45, 88, 10, (uint8_t)(C_CH + 4 + c));
    }
    /* samples */
    b_rect(426, 43, 214, 226, C_PANEL, C_BORDER);
    b_fill(427, 44, 212, 12, C_TRACK);
    /* channel panels */
    for (c = 0; c < 4; c++) {
        int x = 160 * c;
        b_rect(x, 272, 160, 88, C_PANEL, C_BORDER);
        b_fill(x + 1, 273, 158, 12, (uint8_t)(C_CH + 4 + c));
        b_fill(x + 6, 328, 148, 12, C_OFF);
    }
}

/* ------------------------------------------------------------------ text */
__attribute__((noinline)) static void t_set(int col, int row, uint16_t cell)
{
    int i = row * 80 + col;
    if (TSH[i] != cell) {
        TSH[i] = cell;
        TEXT[i] = cell;
    }
}

__attribute__((noinline)) static void t_str(int col, int row, const char *s, uint8_t fg)
{
    while (*s)
        t_set(col++, row, (uint16_t)(A(fg) | (uint8_t)*s++));
}

static void t_strn(int col, int row, const char *s, int n, uint8_t fg)      /* padded to n */
{
    while (n-- > 0)
        t_set(col++, row, (uint16_t)(A(fg) | (uint8_t)(*s ? *s++ : ' ')));
}

__attribute__((noinline)) static void t_hex(int col, int row, uint16_t v, int digits, uint8_t fg)
{
    int i;
    for (i = digits - 1; i >= 0; i--, v >>= 4)
        t_set(col + i, row, (uint16_t)(A(fg) | "0123456789ABCDEF"[v & 15]));
}

__attribute__((noinline)) static void t_dec(int col, int row, uint16_t v, int digits, uint8_t fg)
{
    int i;
    for (i = digits - 1; i >= 0; i--, v /= 10)
        t_set(col + i, row, (uint16_t)(A(fg) | (uint8_t)('0' + v % 10)));
}

static void t_note(int col, int row, uint8_t n, uint8_t fg)                 /* 0..35 = C-1..B-3 */
{
    if (n > 35) {
        t_str(col, row, "---", T_BLU);
        return;
    }
    t_set(col, row, (uint16_t)(A(fg) | (uint8_t)note_names[n % 12][0]));
    t_set(col + 1, row, (uint16_t)(A(fg) | (uint8_t)note_names[n % 12][1]));
    t_set(col + 2, row, (uint16_t)(A(fg) | (uint8_t)('1' + n / 12)));
}

/* ------------------------------------------------------------------ the screen */
static int16_t shown_pos = -1, shown_row = -1;
static uint8_t vu_lit[4];
static uint8_t vu[4];                   /* meter levels 0..64 */
static uint8_t active[32];              /* sample -> channel bits playing it (for the list's colours) */

void ui_init(void)
{
    int i;
    IO(0x200) = 0;
    IO(0x202) = 0;
    IO(0x204) = 0xC000; IO(0x206) = 0x0F;           /* VID_TEXT = 0x0FC000 */
    IO(0x208) = 0x3C00; IO(0x20A) = 0x0C;           /* VID_BMP = 0x0C3C00 */
    IO(0x214) = 0;
    IO(0x216) = 0;
    palette();
    for (i = 0; i < 2000; i++) {
        TSH[i] = 0x4020;
        TEXT[i] = 0x4020;
    }
    background();
    IO(0x200) = 1 | 2 | 4;                          /* text, bitmap, hi-color */
}

void ui_message(const char *s, uint8_t fg)
{
    t_strn(0, 2, s, 80, fg);
}

void ui_keys(void)
{
    ui_message(" SPACE PAUSE  N/P POSITION  T NEXT MOD  1-4 MUTE  I INTERP  S STEREO  Q QUIT", T_BLU);
}

static void sample_list(void)
{
    uint8_t i;
    for (i = 1; i <= 30; i++) {
        int col = SCOL + (i > 15 ? 13 : 0), row = ROW_TOP + (i - 1) % 15;
        uint8_t fg = T_BLU, c;
        for (c = 0; c < 4; c++)
            if (active[i] >> c & 1)
                fg = ccol[c];
        if (fg == T_BLU && mod.smp[i].len)
            fg = T_WHT;
        t_hex(col, row, i, 2, fg == T_WHT ? T_YEL : fg);
        t_strn(col + 3, row, mod.smp[i].name, 9, fg);
    }
}

void ui_song(uint8_t song, uint8_t songs)
{
    uint8_t c;
    t_str(1, 0, "MOD TRACKER", T_YEL);
    t_strn(14, 0, mod.title, 20, T_WHT);
    t_str(1, 1, "SPD", T_CYN);
    t_str(9, 1, "BPM", T_CYN);
    t_str(18, 1, "TIME", T_CYN);
    t_str(30, 1, "MOD", T_CYN);
    t_dec(34, 1, song, 1, T_WHT);
    t_str(35, 1, "/", T_WHT);
    t_dec(36, 1, songs, 1, T_WHT);
    t_str(40, 1, "CPU", T_CYN);
    t_str(52, 0, "POS", T_CYN);
    t_str(63, 0, "PAT", T_CYN);
    t_str(71, 0, "ROW", T_CYN);
    t_str(52, 1, "LEN", T_CYN);
    t_dec(56, 1, mod.songlen, 3, T_WHT);
    t_str(63, 1, "SMP", T_CYN);
    t_str(0, 3, "RW", T_CYN);
    for (c = 0; c < 4; c++) {
        t_str(PCOL(c) + 1, 3, "CHANNEL", T_WHT);
        t_dec(PCOL(c) + 9, 3, c + 1, 1, T_WHT);
        t_str(160 / 8 * c + 1, 20, "CH", T_WHT);
        t_dec(160 / 8 * c + 3, 20, c + 1, 1, T_WHT);
    }
    t_str(SCOL, 3, "SAMPLES", T_WHT);
    {
        uint8_t i, n = 0;
        for (i = 1; i <= 31; i++)
            if (mod.smp[i].len)
                n++;
        t_dec(67, 1, n, 2, T_WHT);
    }
    memset(active, 0, sizeof active);
    sample_list();
    shown_pos = shown_row = -1;
}

static void pattern_row(int line, uint8_t pat, int row)
{
    uint8_t c;
    if (row < 0 || row > 63) {
        t_strn(0, line, "", 51, T_BLU);
        return;
    }
    t_hex(0, line, (uint16_t)row, 2, line == ROW_CUR ? T_WHT : T_CYN);
    t_set(2, line, (uint16_t)(A(T_BLU) | ' '));
    for (c = 0; c < 4; c++) {
        cell_t x;
        int col = PCOL(c);
        mod_cell(pat, (uint8_t)row, c, &x);
        t_set(col, line, (uint16_t)(A(T_BLU) | ' '));
        t_note(col + 1, line, mod_note_of(x.period), line == ROW_CUR ? T_WHT : ccol[c]);
        t_set(col + 4, line, (uint16_t)(A(T_BLU) | ' '));
        if (x.sample)
            t_hex(col + 5, line, x.sample, 2, T_YEL);
        else
            t_str(col + 5, line, "..", T_BLU);
        t_set(col + 7, line, (uint16_t)(A(T_BLU) | ' '));
        if (x.fx || x.arg) {
            t_hex(col + 8, line, x.fx, 1, T_MAG);
            t_hex(col + 9, line, x.arg, 2, T_MAG);
        } else
            t_str(col + 8, line, "...", T_BLU);
        t_set(col + 11, line, (uint16_t)(A(T_BLU) | ' '));
    }
}

static void meter(uint8_t c, uint8_t level)
{
    uint8_t lit = (uint8_t)(level * 24 / 64), k;
    int x = 160 * c + 6;
    if (lit == vu_lit[c])
        return;
    for (k = 0; k < 24; k++)
        if ((k < lit) != (k < vu_lit[c]))
            b_fill(x + k * 6 + 1, 329, 5, 10, k < lit ? (uint8_t)(C_VU + k) : C_OFF);
    vu_lit[c] = lit;
}

void ui_frame(const ui_stat_t *s)
{
    uint8_t c;
    /* header */
    t_dec(5, 1, pl.speed, 2, T_WHT);
    t_dec(13, 1, pl.bpm, 3, T_WHT);
    t_dec(23, 1, (uint16_t)(s->play_s / 60), 2, T_WHT);
    t_str(25, 1, ":", T_WHT);
    t_dec(26, 1, (uint16_t)(s->play_s % 60), 2, T_WHT);
    t_dec(44, 1, s->load, 3, s->load > 80 ? T_RED : T_WHT);
    t_str(47, 1, "%", T_WHT);
    t_str(1, 0, s->paused ? "-- PAUSED -" : "MOD TRACKER", s->paused ? T_RED : T_YEL);
    t_dec(56, 0, pl.ppos, 3, T_WHT);
    t_dec(67, 0, mod.order[pl.ppos], 2, T_WHT);
    t_hex(75, 0, pl.prow, 2, T_WHT);
    /* the pattern view, when the row moves */
    if (pl.ppos != shown_pos || pl.prow != shown_row) {
        int l;
        uint8_t pat = mod.order[pl.ppos];
        for (l = 0; l < NROWS; l++)
            pattern_row(ROW_TOP + l, pat, pl.prow + l - (ROW_CUR - ROW_TOP));
        shown_pos = pl.ppos;
        shown_row = pl.prow;
        /* the sample list: who plays what */
        {
            uint8_t a2[32], i, chg = 0;
            memset(a2, 0, sizeof a2);
            for (c = 0; c < 4; c++)
                if (ch[c].sample && !(pl.mute >> c & 1) && ch[c].out_vol)
                    a2[ch[c].sample] |= (uint8_t)(1 << c);
            for (i = 0; i < 32; i++)
                if (a2[i] != active[i])
                    chg = 1;
            if (chg) {
                memcpy(active, a2, sizeof active);
                sample_list();
            }
        }
    }
    /* channel panels and meters */
    for (c = 0; c < 4; c++) {
        chan_t *h = &ch[c];
        int col = 20 * c;
        uint8_t target, running = (uint8_t)(IO(0x1800 + 64 * c + 28) & 1);
        t_str(col + 6, 20, (pl.mute >> c & 1) ? "MUTED" : "     ", T_RED);
        t_note(col + 1, 21, h->note, ccol[c]);
        t_hex(col + 5, 21, h->sample, 2, T_YEL);
        t_str(col + 8, 21, "V", T_CYN);
        t_hex(col + 9, 21, (uint16_t)h->out_vol, 2, T_WHT);
        t_str(col + 12, 21, "FX", T_CYN);
        t_hex(col + 14, 21, h->fx, 1, T_MAG);
        t_hex(col + 15, 21, h->arg, 2, T_MAG);
        t_strn(col + 1, 22, h->sample ? mod.smp[h->sample].name : "", 18, T_WHT);
        target = running && !(pl.mute >> c & 1) ? (uint8_t)h->out_vol : 0;
        if (h->trig) {
            h->trig = 0;
            vu[c] = target;
        } else if (vu[c] > target)
            vu[c] = (uint8_t)(vu[c] > target + 3 ? vu[c] - 3 : target);
        else
            vu[c] = target;
        meter(c, vu[c]);
    }
}
