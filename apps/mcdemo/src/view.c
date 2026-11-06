/* The microcoded-audio demo's screen (view.h). The diagram's boxes are drawn once into both
 * bitmap buffers; each frame redraws only what moves: the node meters, links and groups, the flow
 * lines with their dots and arrowheads, the scope, the load bar, the speaker and the progress bar,
 * mostly with the blitter. Labels are on the text layer (fixed colours, CP437 font). */
#include <stdint.h>
#include "view.h"

/* ---- hardware */
#define R16(a)       (*(volatile uint16_t *)(uintptr_t)(a))
#define VID_CTRL     R16(0xF00200)
#define VID_BACKDROP R16(0xF00202)
#define VID_TEXT_LO  R16(0xF00204)
#define VID_TEXT_HI  R16(0xF00206)
#define VID_BMP_LO   R16(0xF00208)
#define VID_BMP_HI   R16(0xF0020A)
#define PAL_INDEX    R16(0xF00210)
#define PAL_DATA     R16(0xF00212)
#define VID_SCROLLX  R16(0xF00214)
#define VID_SCROLLY  R16(0xF00216)
#define VID_CTRL2    R16(0xF00220)
#define VID_STAT     R16(0xF00222)
#define FRAMECNT     R16(0xF0000A)
#define BLT(o)       R16(0xF00800 + (o))
enum { B_SRC = 0, B_DST = 4, B_SS = 8, B_DS = 10, B_W = 12, B_H = 14, B_FILL = 18, B_CMD = 20, B_STAT = 22 };

#define BUF0  0x200000UL
#define BUF1  0x240000UL
#define TEXT  0x0FC000UL
#define W     640u
#define H     360u

/* ---- palette */
enum {
    C_BG = 1, C_HDR, C_SDRAM_F, C_SDRAM_B, C_ENG_F, C_ENG_B, C_MUX_F, C_MUX_B,
    C_BOX_F, C_BOX_B, C_BOX_DF, C_BOX_DB, C_LINE, C_LINE_D,
    C_DOT_RAM, C_DOT_AUD, C_DOT_MIC, C_DOT_CPU,
    C_SCOPE_BG, C_SCOPE_GRID, C_TR_L, C_TR_R,
    C_BAR_OK, C_BAR_MID, C_BAR_HI, C_BAR_BG, C_WHITE, C_OUT_F, C_OUT_B, C_SPK_D,
    C_KIND = 32, C_KIND_D = 40
};
#define RGB(r, g, b) (uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
static const uint16_t pal[48] = {
    0, RGB(10, 14, 26), RGB(28, 42, 72), RGB(27, 21, 48), RGB(122, 92, 192),
    RGB(14, 32, 24), RGB(63, 163, 107), RGB(42, 32, 16), RGB(192, 138, 48),
    RGB(22, 35, 58), RGB(106, 134, 184), RGB(16, 22, 31), RGB(44, 54, 70),
    RGB(124, 138, 166), RGB(40, 48, 64),
    RGB(255, 200, 48), RGB(64, 232, 255), RGB(255, 112, 224), RGB(160, 255, 96),
    RGB(4, 18, 10), RGB(28, 58, 38), RGB(80, 255, 144), RGB(255, 176, 64),
    RGB(64, 208, 96), RGB(224, 192, 64), RGB(240, 64, 64), RGB(32, 40, 48), RGB(240, 240, 240),
    RGB(16, 24, 40), RGB(74, 154, 208), RGB(40, 50, 60), 0,
    /* the kinds, bright: pulse, saw, tri, noise, sample, sync, ring, filter */
    RGB(255, 90, 90), RGB(255, 154, 48), RGB(240, 224, 64), RGB(176, 176, 176),
    RGB(64, 184, 255), RGB(176, 112, 255), RGB(255, 106, 208), RGB(96, 224, 112),
    /* and dark */
    RGB(74, 24, 24), RGB(74, 42, 12), RGB(72, 66, 14), RGB(48, 48, 48),
    RGB(14, 48, 72), RGB(46, 26, 72), RGB(72, 24, 58), RGB(22, 62, 26),
};

/* text colours (the eight fixed ones) */
enum { T_BLUE = 1, T_GREEN = 2, T_CYAN = 3, T_RED = 4, T_MAG = 5, T_YEL = 6, T_WHITE = 7 };

static uint32_t back;                   /* the buffer being drawn */
static uint8_t  bi;                     /* its index */

/* ---- drawing */
static uint32_t rowa(uint16_t y) { return ((uint32_t)y << 9) + ((uint32_t)y << 7); }
static void bwait(void) { while (BLT(B_STAT) & 1) ; }

static void fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{
    if (!w || !h) return;
    uint32_t a = back + rowa(y) + x;
    bwait();
    BLT(B_DST) = (uint16_t)a; BLT(B_DST + 2) = (uint16_t)(a >> 16);
    BLT(B_DS) = W; BLT(B_W) = w; BLT(B_H) = h; BLT(B_FILL) = c;
    BLT(B_CMD) = 1;
}

static void pset(uint16_t x, uint16_t y, uint8_t c)
{
    *(volatile uint8_t *)(uintptr_t)(back + rowa(y) + x) = c;
}

/* a box: x0..x1, y0..y1 inclusive, its fill and a 1-pixel border */
static void box(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t f, uint8_t b)
{
    fill(x0, y0, x1 - x0 + 1, y1 - y0 + 1, b);
    fill(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, f);
}

/* a box around text columns c0..c1 and rows r0..r1 */
static void tbox(uint16_t c0, uint16_t r0, uint16_t c1, uint16_t r1, uint8_t f, uint8_t b)
{
    box(8 * c0 - 4, 14 * r0 - 1, 8 * (c1 + 1) + 3, 14 * (r1 + 1), f, b);
}

/* ---- text */
static volatile uint16_t *const tx = (volatile uint16_t *)(uintptr_t)TEXT;
static void txt(uint16_t col, uint16_t row, const char *s, uint16_t fg)
{
    volatile uint16_t *p = tx + row * 80 + col;
    while (*s && col++ < 80) *p++ = (uint16_t)(0x4000 | fg << 8 | (uint8_t)*s++);
}
static void txtn(uint16_t col, uint16_t row, const char *s, uint16_t fg, uint16_t n)   /* padded to n */
{
    volatile uint16_t *p = tx + row * 80 + col;
    for (; n && col < 80; n--, col++) {
        uint8_t ch = (s && *s && *s != '\n') ? (uint8_t)*s++ : ' ';
        *p++ = (uint16_t)(0x4000 | fg << 8 | ch);
    }
}
static void num(uint16_t col, uint16_t row, uint32_t v, uint16_t width, uint16_t fg)
{
    char b[11];
    int16_t i = 10;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v && i);
    while (10 - i < width && i) b[--i] = ' ';
    txt(col, row, b + i, fg);
}

/* ---- the flow lines: axis-aligned polylines, 2 pixels wide, with dots moving along them */
enum { K_RAM, K_AUD, K_MIC, K_CPU };
typedef struct { int16_t n; uint8_t kind, gate; int16_t p[8]; } path_t;   /* p: x0 y0 x1 y1 ... */
static const path_t paths[] = {
    { 2, K_CPU, VF_TICK,    { 78, 50, 92, 50 } },                       /* CPU -> SDRAM */
    { 2, K_RAM, 0,          { 400, 86, 400, 93 } },                     /* nodes -> rammux */
    { 2, K_RAM, VF_SAMPLES, { 144, 81, 144, 93 } },                     /* samples -> rammux */
    { 2, K_RAM, 0,          { 56, 114, 56, 136 } },                     /* rammux -> walker */
    { 3, K_RAM, VF_SAMPLES, { 6, 114, 6, 195, 9, 195 } },               /* rammux -> DMA */
    { 2, K_RAM, 0,          { 108, 153, 120, 153 } },                   /* walker -> node buffer */
    { 2, K_RAM, 0,          { 172, 137, 172, 116 } },                   /* write-back -> rammux */
    { 4, K_RAM, 0,          { 196, 169, 196, 174, 280, 174, 280, 178 } },   /* node buffer -> datapath */
    { 2, K_MIC, 0,          { 436, 153, 456, 153 } },                   /* control store -> sequencer */
    { 4, K_MIC, 0,          { 508, 169, 508, 174, 400, 174, 400, 178 } },   /* sequencer -> datapath */
    { 2, K_RAM, VF_SAMPLES, { 108, 195, 120, 195 } },                   /* DMA -> sample buffer */
    { 2, K_RAM, VF_SAMPLES, { 220, 195, 240, 195 } },                   /* sample buffer -> datapath */
    { 2, K_AUD, 0,          { 428, 195, 456, 195 } },                   /* datapath -> mix buses */
    { 2, K_AUD, VF_FILTER,  { 556, 189, 568, 189 } },                   /* mix -> filter */
    { 2, K_AUD, VF_FILTER,  { 571, 201, 559, 201 } },                   /* filter -> mix */
    { 2, K_AUD, 0,          { 508, 211, 508, 219 } },                   /* mix -> FIFO */
    { 2, K_AUD, 0,          { 508, 241, 508, 261 } },                   /* FIFO -> HDMI */
    { 2, K_AUD, 0,          { 524, 279, 536, 279 } },                   /* HDMI -> TV */
    { 2, K_AUD, 0,          { 588, 279, 594, 279 } },                   /* TV -> speaker */
    { 4, K_AUD, 0,          { 508, 256, 280, 256, 280, 286, 277, 286 } },   /* tap -> scope */
};
#define NPATHS (sizeof paths / sizeof paths[0])

static int16_t iabs(int16_t v) { return v < 0 ? (int16_t)-v : v; }
static int16_t imin(int16_t a, int16_t b) { return a < b ? a : b; }

static void path_line(const path_t *p, uint8_t c)
{
    for (int16_t i = 0; i + 1 < p->n; i++) {
        int16_t x0 = p->p[2 * i], y0 = p->p[2 * i + 1], x1 = p->p[2 * i + 2], y1 = p->p[2 * i + 3];
        if (y0 == y1) fill((uint16_t)imin(x0, x1), (uint16_t)y0, (uint16_t)(iabs((int16_t)(x1 - x0)) + 2), 2, c);
        else fill((uint16_t)x0, (uint16_t)imin(y0, y1), 2, (uint16_t)(iabs((int16_t)(y1 - y0)) + 2), c);
    }
}

/* the arrowhead at the end, pointing along the last segment */
static void path_head(const path_t *p, uint8_t c)
{
    int16_t k = p->n - 1;
    int16_t ex = p->p[2 * k], ey = p->p[2 * k + 1], px = p->p[2 * k - 2], py = p->p[2 * k - 1];
    for (int16_t i = 0; i < 4; i++)
        for (int16_t j = -i; j <= 1 + i; j++) {
            if (ey == py) pset((uint16_t)(ex > px ? ex + 1 - i : ex + i), (uint16_t)(ey + j), c);
            else pset((uint16_t)(ex + j), (uint16_t)(ey > py ? ey + 1 - i : ey + i), c);
        }
}

/* dots every sp pixels from offset ph along the path */
static void path_dots(const path_t *p, uint16_t ph, uint16_t sp, uint8_t c)
{
    int16_t d = (int16_t)ph, s = 0;
    for (int16_t i = 0; i + 1 < p->n; i++) {
        int16_t x0 = p->p[2 * i], y0 = p->p[2 * i + 1], x1 = p->p[2 * i + 2], y1 = p->p[2 * i + 3];
        int16_t len = (int16_t)(iabs((int16_t)(x1 - x0)) + iabs((int16_t)(y1 - y0)));
        for (; d < s + len; d = (int16_t)(d + sp)) {
            int16_t o = (int16_t)(d - s);
            int16_t x = x0 + (x1 > x0 ? o : x1 < x0 ? -o : 0);
            int16_t y = y0 + (y1 > y0 ? o : y1 < y0 ? -o : 0);
            uint32_t a = back + rowa((uint16_t)y) + (uint16_t)x;
            volatile uint8_t *q = (volatile uint8_t *)(uintptr_t)a;
            q[0] = c; q[1] = c; q[W] = c; q[W + 1] = c;
        }
        s = (int16_t)(s + len);
    }
}

/* ---- the node list */
#define NX(i) (uint16_t)(200 + ((i) & 15) * 26)
#define NY(i) (uint16_t)((i) < 16 ? 30 : 52)

/* ---- the speaker's waves: three arcs around (608, 281) */
static int16_t arc_x[96], arc_y[96];
static uint8_t arc_r[96], narc;
static uint16_t isqrt(uint16_t v) { uint16_t r = 0; while ((uint16_t)((r + 1) * (r + 1)) <= v) r++; return r; }

/* ---- what each buffer shows of the things drawn only when they change */
static uint16_t drawn_flags[2];
static const char *shown_scene, *shown_notes;
static uint16_t text_flags = 0xFFFF;

static void flag_boxes(uint16_t f)
{
    uint8_t sf = (f & VF_SAMPLES) ? C_BOX_F : C_BOX_DF, sb = (f & VF_SAMPLES) ? C_BOX_B : C_BOX_DB;
    uint8_t ff = (f & VF_FILTER) ? C_BOX_F : C_BOX_DF, fb = (f & VF_FILTER) ? C_BOX_B : C_BOX_DB;
    tbox(2, 13, 12, 14, sf, sb);                           /* sample DMA */
    tbox(16, 13, 27, 14, sf, sb);                          /* sample buffer */
    tbox(72, 13, 78, 14, ff, fb);                          /* filter */
    /* the samples region in SDRAM, with a waveform */
    box(104, 27, 187, 80, C_SDRAM_F, (f & VF_SAMPLES) ? C_KIND + VK_SAMPLE : C_BOX_DB);
    /* a hit that decays into a loop */
    static const int8_t wav[8] = { 0, 7, 10, 7, 0, -7, -10, -7 };
    uint8_t wc = (f & VF_SAMPLES) ? C_KIND + VK_SAMPLE : C_BOX_DB;
    int16_t py = 61;
    for (uint16_t x = 0; x < 76; x++) {
        int16_t amp = x < 44 ? (int16_t)(16 - x / 3) : 3;
        int16_t y = (int16_t)(61 - wav[x & 7] * amp / 10);
        int16_t a = py < y ? py : y, b = py < y ? y : py;
        fill((uint16_t)(108 + x), (uint16_t)a, 1, (uint16_t)(b - a + 1), wc);
        py = y;
    }
}

static void labels(uint16_t f)
{
    uint16_t sc = (f & VF_SAMPLES) ? T_WHITE : T_BLUE, fc = (f & VF_FILTER) ? T_WHITE : T_BLUE;
    txt(14, 2, "samples", (f & VF_SAMPLES) ? T_CYAN : T_BLUE);
    txt(2, 13, "sample DMA", sc);  txt(2, 14, "bursts", sc);
    txt(16, 13, "sample buf", sc); txt(16, 14, "512 words", sc);
    txt(72, 13, "SVF", fc);        txt(72, 14, "filter", fc);
}

void view_init(void)
{
    VID_CTRL = 0x01;                    /* text only while drawing */
    VID_CTRL2 = 0;
    VID_BACKDROP = 0;
    VID_SCROLLX = 0; VID_SCROLLY = 0;
    VID_TEXT_LO = (uint16_t)TEXT; VID_TEXT_HI = (uint16_t)(TEXT >> 16);
    PAL_INDEX = 0;
    for (uint16_t i = 0; i < 48; i++) PAL_DATA = pal[i];
    for (uint16_t i = 0; i < 2000; i++) tx[i] = 0x4000 | ' ';

    back = BUF0; bi = 0;
    fill(0, 0, W, H, C_BG);
    /* CPU, SDRAM, rammux, engine, output */
    box(6, 13, 77, 85, C_BOX_F, C_DOT_CPU);
    box(96, 13, 636, 85, C_SDRAM_F, C_SDRAM_B);
    box(6, 96, 636, 112, C_MUX_F, C_MUX_B);
    box(2, 124, 637, 250, C_ENG_F, C_ENG_B);
    tbox(2, 10, 12, 11, C_BOX_F, C_BOX_B);                 /* list walker */
    tbox(16, 10, 27, 11, C_BOX_F, C_BOX_B);                /* node buffer */
    tbox(40, 10, 53, 11, C_BOX_F, C_DOT_MIC);              /* control store */
    tbox(58, 10, 68, 11, C_BOX_F, C_DOT_MIC);              /* sequencer */
    tbox(31, 13, 52, 14, C_BOX_F, C_BOX_B);                /* datapath */
    tbox(58, 13, 68, 14, C_BOX_F, C_DOT_AUD);              /* mix buses */
    tbox(58, 16, 68, 16, C_BOX_F, C_DOT_AUD);              /* output FIFO */
    tbox(54, 19, 64, 20, C_OUT_F, C_OUT_B);                /* HDMI */
    tbox(68, 19, 72, 20, C_OUT_F, C_OUT_B);                /* TV */
    box(14, 264, 273, 309, C_SCOPE_BG, C_SCOPE_GRID);      /* scope */
    fill(52, 228, 128, 8, C_BAR_BG);                       /* load bar */
    /* the speaker */
    fill(598, 275, 6, 13, C_WHITE);
    for (uint16_t i = 0; i < 8; i++) fill((uint16_t)(604 + i), (uint16_t)(278 - i), 1, (uint16_t)(7 + 2 * i), C_WHITE);
    narc = 0;
    for (uint16_t r = 9; r <= 21; r = (uint16_t)(r + 6))
        for (int16_t dy = (int16_t)-(r * 3 / 5); dy <= (int16_t)(r * 3 / 5); dy++) {
            arc_x[narc] = (int16_t)(608 + isqrt((uint16_t)(r * r - dy * dy)));
            arc_y[narc] = (int16_t)(281 + dy);
            arc_r[narc++] = (uint8_t)((r - 9) / 6);
        }
    /* the kinds' legend */
    static const char *const kn[VK__N] = { "pulse", "saw", "tri", "noise", "sample", "sync", "ring", "filter" };
    uint16_t c = 25;
    for (uint16_t k = 0; k < VK__N; k++) {
        fill((uint16_t)(8 * c + 1), 74, 6, 6, (uint8_t)(C_KIND + k));
        txt((uint16_t)(c + 1), 5, kn[k], T_WHITE);
        uint16_t len = 0;
        while (kn[k][len]) len++;
        c = (uint16_t)(c + 2 + len);
    }
    flag_boxes(0xFFFF);

    /* the labels */
    txt(1, 1, "T1F CPU", T_GREEN);
    txt(1, 2, "music", T_WHITE); txt(1, 3, "program", T_WHITE); txt(1, 4, "writes", T_WHITE); txt(1, 5, "nodes", T_WHITE);
    txt(13, 1, "SDRAM", T_MAG);
    txt(25, 1, "node list: one burst per node, walked every block", T_WHITE);
    txt(2, 7, "RAM machine (rammux): CPU, video and blitter first; audio gets what is left", T_YEL);
    txt(23, 8, "state write-back", T_WHITE);
    txt(26, 9, "AUDIO ENGINE: microcoded, 32-sample blocks, 48 kHz", T_GREEN);
    txt(2, 10, "list walker", T_WHITE);   txt(2, 11, "next ptr", T_WHITE);
    txt(16, 10, "node buffer", T_WHITE);  txt(16, 11, "4 nodes", T_WHITE);
    txt(40, 10, "control store", T_MAG);  txt(40, 11, "512 x 72 bits", T_WHITE);
    txt(58, 10, "sequencer", T_MAG);      txt(58, 11, "1 uword/clk", T_WHITE);
    txt(31, 13, "datapath: wave, sample", T_WHITE);
    txt(31, 14, "envelope, MAC (1 mult)", T_WHITE);
    txt(58, 13, "mix buses", T_CYAN);     txt(58, 14, "main+filter", T_WHITE);
    txt(58, 16, "output FIFO", T_CYAN);
    txt(2, 16, "busy", T_WHITE);
    txt(28, 16, "bursts", T_WHITE);       txt(39, 16, "words", T_WHITE);
    txt(2, 18, "speaker feed: last 256 samples", T_WHITE);
    txt(2, 19, "L", T_GREEN);             txt(4, 19, "R", T_YEL);
    txt(55, 19, "HDMI audio", T_CYAN);    txt(54, 20, "in blanking", T_WHITE);
    txt(69, 19, "TV", T_CYAN);
    txt(52, 0, "blocks", T_WHITE);        txt(66, 0, "underruns", T_WHITE);
    labels(0xFFFF);
    text_flags = 0xFFFF;

    /* the same picture in the other buffer */
    bwait();
    BLT(B_SRC) = (uint16_t)BUF0; BLT(B_SRC + 2) = (uint16_t)(BUF0 >> 16);
    BLT(B_DST) = (uint16_t)BUF1; BLT(B_DST + 2) = (uint16_t)(BUF1 >> 16);
    BLT(B_SS) = W; BLT(B_DS) = W; BLT(B_W) = W; BLT(B_H) = H;
    BLT(B_CMD) = 0;
    bwait();
    drawn_flags[0] = drawn_flags[1] = 0xFFFF;
    shown_scene = shown_notes = 0;

    VID_BMP_LO = (uint16_t)BUF0; VID_BMP_HI = (uint16_t)(BUF0 >> 16);
    VID_CTRL = 0x07;                    /* text, bitmap, hi-color */
    back = BUF1; bi = 1;
}

void view_frame(const mon_t *m)
{
    uint16_t f = m->flags, n = m->nnodes > 32 ? 32 : m->nnodes;
    uint16_t t = FRAMECNT;
    while (VID_STAT & 1) ;              /* the last flip: the back buffer is free */

    /* ---- blitter work */
    if (drawn_flags[bi] != f) { flag_boxes(f); drawn_flags[bi] = f; }
    /* header and the progress bar */
    fill(0, 2, W, 11, C_HDR);
    uint16_t pw = m->scene_count ? (uint16_t)((uint32_t)W * m->scene_no / m->scene_count) : 0;
    fill(0, 0, pw, 2, C_BAR_OK);
    fill(pw, 0, (uint16_t)(W - pw), 2, C_BAR_BG);
    /* nodes, links, the row wrap and the groups */
    for (uint16_t i = 0; i < 32; i++) {
        uint16_t x = NX(i), y = NY(i);
        if (i < n) {
            uint8_t k = m->kind[i] < VK__N ? m->kind[i] : 0;
            uint16_t h = (uint16_t)(((uint32_t)(m->meter[i] & 0x7FFF) * 14) >> 15);
            box(x, y, (uint16_t)(x + 19), (uint16_t)(y + 15), (uint8_t)(C_KIND_D + k), (uint8_t)(C_KIND + k));
            fill((uint16_t)(x + 1), (uint16_t)(y + 15 - h), 18, h, (uint8_t)(C_KIND + k));
        } else box(x, y, (uint16_t)(x + 19), (uint16_t)(y + 15), C_SDRAM_F, C_BOX_DB);
        if ((i & 15) != 15) fill((uint16_t)(x + 20), (uint16_t)(y + 7), 6, 2, i + 1 < n ? C_LINE : C_SDRAM_F);
        fill(x, (uint16_t)(y + 16), 26, 2, C_SDRAM_F);
    }
    {
        uint8_t c = n > 16 ? C_LINE : C_SDRAM_F;
        fill(610, 37, 12, 2, c); fill(620, 37, 2, 14, c); fill(194, 49, 428, 2, c);
        fill(194, 49, 2, 12, c); fill(194, 59, 6, 2, c);
    }
    if (f & VF_GROUP)
        for (uint16_t i = 0; i + 1 < n; i++)
            if (m->group[i]) {
                if ((i & 15) != 15) fill(NX(i), (uint16_t)(NY(i) + 16), 46, 2, C_WHITE);
                else { fill(NX(i), (uint16_t)(NY(i) + 16), 20, 2, C_WHITE); fill(NX(i + 1), (uint16_t)(NY(i + 1) + 16), 20, 2, C_WHITE); }
            }
    /* load bar */
    {
        uint16_t bw = (uint16_t)(((uint32_t)(m->clocks > 45000u ? 45000u : m->clocks) * 128) / 45000u);
        uint8_t c = bw < 77 ? C_BAR_OK : bw < 109 ? C_BAR_MID : C_BAR_HI;
        fill(52, 228, bw, 8, c);
        fill((uint16_t)(52 + bw), 228, (uint16_t)(128 - bw), 8, C_BAR_BG);
    }
    /* scope background */
    fill(16, 266, 256, 42, C_SCOPE_BG);
    fill(16, 286, 256, 1, C_SCOPE_GRID);
    /* the lines */
    for (uint16_t i = 0; i < NPATHS; i++) {
        const path_t *p = &paths[i];
        path_line(p, (p->gate && !(f & p->gate)) ? C_LINE_D : C_LINE);
    }
    bwait();

    /* ---- the CPU's work */
    for (uint16_t i = 0; i < NPATHS; i++) {
        const path_t *p = &paths[i];
        uint8_t on = !p->gate || (f & p->gate);
        path_head(p, on ? C_LINE : C_LINE_D);
        if (!on) continue;
        uint16_t spd, sp;
        uint8_t c;
        switch (p->kind) {
        case K_RAM:
            if (!m->words) continue;
            spd = (uint16_t)(1 + (m->words >> 8)); if (spd > 6) spd = 6;
            sp = (uint16_t)(40 - (m->bursts >> 1)); if (sp < 10 || sp > 40) sp = 10;
            c = C_DOT_RAM; break;
        case K_MIC:
            if (!m->clocks) continue;
            spd = (uint16_t)(1 + m->clocks / 9000u); sp = 12; c = C_DOT_MIC; break;
        case K_CPU: spd = 2; sp = 8; c = C_DOT_CPU; break;
        default: spd = 3; sp = 12; c = C_DOT_AUD; break;
        }
        path_dots(p, (uint16_t)((uint16_t)(t * spd) % sp), sp, c);
    }
    /* scope traces, and the peak for the speaker */
    {
        int16_t pl = 287 - (m->scope_l[0] >> 11), pr = 287 - (m->scope_r[0] >> 11);
        uint16_t peak = 0;
        uint32_t row = back + rowa(266);
        for (uint16_t i = 0; i < 256; i++) {
            int16_t vl = m->scope_l[i], vr = m->scope_r[i];
            int16_t yl = (int16_t)(287 - (vl >> 11)), yr = (int16_t)(287 - (vr >> 11));
            uint16_t al = (uint16_t)iabs(vl), ar = (uint16_t)iabs(vr);
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;
            for (int16_t k = 0; k < 2; k++) {
                int16_t y = k ? yr : yl, py = k ? pr : pl;
                int16_t a = py < y ? py : y, b = py < y ? y : py;
                if (a < 267) a = 267;
                if (b > 306) b = 306;
                if (a > b) a = b;
                volatile uint8_t *q = (volatile uint8_t *)(uintptr_t)(row + rowa((uint16_t)(a - 266)) + 16 + i);
                for (; a <= b; a++, q += W) *q = k ? C_TR_R : C_TR_L;
            }
            pl = yl; pr = yr;
        }
        /* the speaker's waves: one arc per 1/4 of full scale */
        uint16_t lit = peak > 600 ? (uint16_t)(1 + (peak >> 13)) : 0;
        for (uint16_t i = 0; i < narc; i++)
            pset((uint16_t)arc_x[i], (uint16_t)arc_y[i], arc_r[i] < lit ? C_DOT_AUD : C_SPK_D);
    }

    /* ---- text */
    if (text_flags != f) { labels(f); text_flags = f; }
    if (m->scene != shown_scene) {
        txtn(1, 0, m->scene, T_YEL, 50);
        shown_scene = m->scene;
    }
    if (m->notes != shown_notes) {
        const char *s = m->notes;
        for (uint16_t r = 22; r < 25; r++) {
            txtn(1, r, s, T_WHITE, 78);
            while (s && *s && *s != '\n') s++;
            if (s && *s) s++;
        }
        shown_notes = m->notes;
    }
    num(59, 0, m->blocks, 5, T_CYAN);
    num(76, 0, m->underruns, 3, m->underruns ? T_RED : T_CYAN);
    num(23, 16, (uint32_t)m->clocks * 100u / 45000u, 3, T_CYAN); txt(26, 16, "%", T_CYAN);
    num(35, 16, m->bursts, 3, T_CYAN);
    num(45, 16, m->words, 5, T_CYAN);

    /* show it at the next frame */
    VID_BMP_LO = (uint16_t)back; VID_BMP_HI = (uint16_t)(back >> 16);
    bi ^= 1;
    back = bi ? BUF1 : BUF0;
}
