/* The Symphony No. 40 visualizer: see viz.h. Falling notes over a keyboard (the keys light in their
 * instrument's colour), sparks and a flash where each note lands, and under the keyboard the
 * sections' bouncing balls over a fire the notes feed. Registers: docs/io.md "Video", "Video
 * extensions", "Blitter". The video registers are write-only: the copies here are the truth.
 *
 * The notes are drawn once, into playfield B (behind A, a 360-line ring): each frame only the rows
 * coming into view at the top are drawn, and B's vertical scroll moves them all down; playfield A is
 * transparent over the falling area and opaque on the keyboard, so a note slides under the keys.
 * Hardware it works every frame: playfield B's scroll, the blitter (the new rows, changed keys), all
 * 64 hardware sprites (54 landing flashes, 10 bouncers) sent in vertical blank, the palette (stars
 * twinkle), double-buffered playfield A (sparks, fire), the text layer. */
#include <stdint.h>
#include <math.h>
#include <fcpu.h>
#include "viz.h"

#define R16(a)      (*(volatile uint16_t *)(uintptr_t)(a))
#define VID_CTRL    R16(0xF00200)
#define VID_BMP_LO  R16(0xF00208)
#define VID_BMP_HI  R16(0xF0020A)
#define PAL_INDEX   R16(0xF00210)
#define PAL_DATA    R16(0xF00212)
#define VID_SCROLLX R16(0xF00214)
#define VID_SCROLLY R16(0xF00216)
#define VID_CTRL2   R16(0xF00220)
#define VID_BMPB_LO R16(0xF00224)
#define VID_BMPB_HI R16(0xF00226)
#define VID_SXB     R16(0xF00228)
#define VID_SYB     R16(0xF0022A)
#define VID_STAT    R16(0xF00222)
#define SPR_INDEX   R16(0xF00230)
#define SPR_DATA    R16(0xF00232)
#define SPR_OVER    R16(0xF00234)
#define BLT(o)      R16(0xF00800 + (o))
enum { B_SRC = 0, B_DST = 4, B_SS = 8, B_DS = 10, B_W = 12, B_H = 14, B_KEY = 16, B_FILL = 18, B_CMD = 20, B_STAT = 22 };

#define W 640
#define H 360
#define BUF_A0   0x200000UL
#define BUF_A1   0x240000UL
#define BUF_B    0x280000UL                     /* the falling notes: a 360-line ring, scrolled */
#define SPRPIX   0x2C0000UL
#define FLASHPIX 0x2C4000UL                     /* the landing flashes: 32 x 32, 4 fades a section */
#define TEXT     ((volatile uint16_t *)0x0FC000UL)

/* palette */
enum { C_BLACK = 1, C_WHITE, C_GREY, C_DIM, C_KEYGAP, C_BLACKKEY, C_RAIL, C_BARBG, C_SPARK,
       C_STAR = 48, N_STAR = 8, C_INST = 64,           /* 10 sections x 8 shades, bright to dim */
       C_GLOW = 144,                                   /* 10 x 8: white-hot to the section's colour, dimming */
       C_FIRE = 224, N_FIRE = 32 };
#define INST(i, s) ((uint8_t)(C_INST + 8 * (i) + (s)))
#define GLOW(i, s) ((uint8_t)(C_GLOW + 8 * (i) + (s)))

/* regions */
#define FALL_TOP   44                           /* the falling notes: under the text's three rows */
#define KEY_TOP    236                          /* the keyboard: 236-289 */
#define KEY_H      54
#define BKEY_H     34
#define BARS_Y     296                          /* the fire and the bouncers: 296-333 */
#define BARS_H     38
#define PX_TICK    12                           /* the notes fall 12 pixels an eighth note */
#define LOW_MIDI   24                           /* the keyboard: C1 to C7, 43 white keys of 14 pixels */
#define HIGH_MIDI  96
#define WKEY       14
#define KEY_X0     ((W - 43 * WKEY) / 2)

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) { return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)); }

static const uint8_t inst_rgb[VI__N][3] = {
    { 255, 214, 90 },   /* violin I: gold */
    { 255, 160, 60 },   /* violin II: orange */
    { 255, 105, 80 },   /* viola: coral */
    { 230, 60, 110 },   /* cello: crimson */
    { 170, 90, 230 },   /* bass: violet */
    { 90, 230, 255 },   /* flute: cyan */
    { 110, 240, 140 },  /* oboe: green */
    { 80, 160, 255 },   /* clarinet: blue */
    { 200, 220, 90 },   /* bassoon: olive */
    { 255, 250, 210 },  /* horn: warm white */
};
static const char *const inst_name[VI__N] = { "Vln I", "Vln II", "Viola", "Cello", "Bass",
                                              "Flute", "Oboe", "Clar", "Bsn", "Horn" };

/* ---- the blitter ---- */
static void bwait(void) { while (BLT(B_STAT) & 1) ; }
static void bfill(uint32_t d, uint16_t w, uint16_t h, uint8_t v)
{
    bwait();
    BLT(B_DST) = (uint16_t)d; BLT(B_DST + 2) = (uint16_t)(d >> 16);
    BLT(B_DS) = W; BLT(B_W) = w; BLT(B_H) = h; BLT(B_FILL) = v; BLT(B_CMD) = 1;
}
static void rect(uint32_t buf, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t c)
{
    if (w && h) bfill(buf + (uint32_t)y * W + x, w, h, c);
}

/* ---- tables ---- */
static int16_t sn[256];                         /* sin, Q15 */
static uint16_t rng = 0xACE1;
static uint16_t rnd(void) { rng ^= (uint16_t)(rng << 7); rng ^= (uint16_t)(rng >> 9); rng ^= (uint16_t)(rng << 8); return rng; }

/* ---- the text layer ---- */
static void txt(uint16_t col, uint16_t row, const char *s, uint16_t fg)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s && col++ < 80) *t++ = (uint16_t)(0x4000 | fg << 8 | (uint8_t)*s++);
}
static void txtpad(uint16_t col, uint16_t row, const char *s, uint16_t fg, uint16_t n)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (n--) *t++ = (uint16_t)(0x4000 | fg << 8 | (uint8_t)(*s ? *s++ : ' '));
}
static char *utoa_(char *p, uint32_t v, uint16_t w)
{
    char b[10]; uint16_t n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (w > n) { *p++ = ' '; w--; }
    while (n) *p++ = b[--n];
    *p = 0;
    return p;
}
static char *cat(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

/* ---- the keyboard: where a note's key is ---- */
static const uint8_t wpos[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };    /* white index in the octave */
static const uint8_t isblk[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
static uint16_t key_x(uint8_t m, uint16_t *w)
{
    uint8_t o = (uint8_t)((m - LOW_MIDI) / 12), pc = (uint8_t)((m - LOW_MIDI) % 12);
    uint16_t wx = (uint16_t)(KEY_X0 + (o * 7 + wpos[pc]) * WKEY);
    if (isblk[pc]) { *w = 9; return (uint16_t)(wx + WKEY - 4); }       /* over the boundary */
    *w = WKEY - 1;
    return wx;
}

/* ---- the sprites: a shadow, sent in vertical blank ---- */
#define NSPR 64
#define NPART 54                                 /* landing flashes; sprites 54-63 bounce, a section each */
typedef struct { int16_t x, y, vx, vy; uint8_t inst, age, life, on; } part_t;   /* x, y, v: 12.4 */
static part_t pt[NSPR];
static uint16_t sh[NSPR * 4];
static uint8_t next_spr, alive;

static uint32_t spr_img(uint8_t inst, uint8_t fade) { return SPRPIX + ((uint32_t)inst * 4 + fade) * 256; }
static void spr_shadow(uint8_t n)
{
    const part_t *p = &pt[n];
    uint16_t *s = sh + 4 * n;
    if (!p->on) { s[3] = 0; return; }
    uint8_t fade = (uint8_t)(p->age * 4 / p->life);
    if (fade > 3) fade = 3;
    uint8_t big = n < NPART;                                             /* a flash: 32 x 32 */
    uint32_t a = big ? FLASHPIX + ((uint32_t)p->inst * 4 + fade) * 1024 : spr_img(p->inst, fade);
    s[0] = (uint16_t)((p->x >> 4) + 32);
    s[1] = (uint16_t)((((p->y >> 4) + 32) & 0x1FF) | (big ? 31 : 15) << 11);
    s[2] = (uint16_t)a;
    s[3] = (uint16_t)(0x8000 | (big ? 2 : 1) << 13 | ((a >> 16) & 0x7F));
}
static void spr_send(void)
{
    SPR_INDEX = 0;
    for (uint16_t i = 0; i < NSPR * 4; i++) SPR_DATA = sh[i];
}
static void make_sprites(void)                   /* a glowing ball per section, 4 fading sizes */
{
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)SPRPIX;
    for (uint8_t i = 0; i < VI__N; i++)
        for (uint8_t f = 0; f < 4; f++)
            for (int16_t y = 0; y < 16; y++)
                for (int16_t x = 0; x < 16; x++) {
                    int16_t dx = (int16_t)(2 * x - 15), dy = (int16_t)(2 * y - 15);
                    int16_t d2 = (int16_t)(dx * dx + dy * dy);
                    int16_t rr = (int16_t)(15 - 3 * f);
                    uint8_t v = 0;
                    if (d2 <= rr * rr) {
                        int16_t s = (int16_t)(f + d2 * 6 / (rr * rr + 1));
                        v = INST(i, s > 7 ? 7 : s);
                    }
                    p[(((uint32_t)i * 4 + f) * 16 + (uint16_t)y) * 16 + (uint16_t)x] = v;
                }
}

static uint16_t isqrt(uint16_t v) { uint16_t r = 0; while ((uint16_t)((r + 1) * (r + 1)) <= v) r++; return r; }
static void make_flashes(void)                   /* a white-hot burst: a core, a glow, a cross of rays */
{
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)FLASHPIX;
    for (uint8_t i = 0; i < VI__N; i++)
        for (uint8_t f = 0; f < 4; f++)
            for (int16_t y = 0; y < 32; y++)
                for (int16_t x = 0; x < 32; x++) {
                    int16_t dx = (int16_t)(2 * x - 31), dy = (int16_t)(2 * y - 31);   /* half pixels */
                    uint16_t d = isqrt((uint16_t)(dx * dx + dy * dy));                /* 0-44 */
                    int16_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
                    int16_t core = (int16_t)(12 - 3 * f), glow = (int16_t)(30 - 5 * f), ray = (int16_t)(31 - 4 * f);
                    int16_t s = 99;
                    if ((int16_t)d <= core) s = (int16_t)(f + d * 2 / (core + 1));                 /* white */
                    else if ((int16_t)d <= glow) s = (int16_t)(2 + f + (d - core) * 6 / (glow - core + 1));
                    if ((ax <= 1 || ay <= 1) && (int16_t)d <= ray) {                           /* the cross */
                        int16_t r = (int16_t)(f + 1 + d * 5 / (ray + 1));
                        if (r < s) s = r;
                    }
                    if (ax == ay && (int16_t)d <= ray * 2 / 3) {                                /* the diagonals */
                        int16_t r = (int16_t)(f + 2 + d * 6 / (ray + 1));
                        if (r < s) s = r;
                    }
                    p[(((uint32_t)i * 4 + f) * 32 + (uint16_t)y) * 32 + (uint16_t)x] = s > 7 ? 0 : GLOW(i, s);
                }
}

/* ---- the sparks: pixels in the bitmap, thrown up where a note lands ---- */
/* A live spark keeps its slot until it has cooled (or left the falling area): the live ones are kept
 * packed at the front, a dead one swapped with the last; a new spark when the pool is full is dropped,
 * never an older one cut short. Its heat counts up from 0 (white) to 2048 (cold), 8 shades. */
#define NSPARK 1024
typedef struct { int16_t x, y, vx, vy; uint16_t heat, rate; uint8_t inst; } spark_t;   /* 12.4 */
static spark_t sp[NSPARK];
static uint16_t nsp, sp_drop;
static void spark(uint16_t x, uint8_t inst, uint8_t hard)
{
    if (nsp >= NSPARK) { sp_drop++; return; }
    spark_t *s = &sp[nsp++];
    s->x = (int16_t)((x + (rnd() % 12)) << 4);
    s->y = (int16_t)((KEY_TOP - 6) << 4);
    s->vx = (int16_t)((int16_t)(rnd() & 0x1F) - 0x10);                  /* +-1 pixel a frame */
    s->vy = (int16_t)-(0x10 + (rnd() & 0x1F) + (hard >> 1));             /* 1-3 up: an arc of 10-80 */
    s->inst = inst; s->heat = 0;
    s->rate = (uint16_t)(2048 / (70 + (rnd() & 63)));                  /* 70-133 frames: up and back down */
}

/* ---- the fire under the keyboard: 80 x 19 cells of 8 x 2 pixels; each section owns 8 columns (its
 * slot under its label), and its note-ons feed heat into them ---- */
#define FW 80
#define FH 19
static uint8_t heat[FH + 1][FW];
static uint16_t energy[VI__N];
static uint16_t frnd = 1;
static uint8_t frand(void) { frnd = (uint16_t)(frnd * 25173u + 13849u); return (uint8_t)(frnd >> 8); }
static void fire_step(const viz_t *v, uint16_t df)
{
    for (uint8_t j = 0; j < v->nnotes; j++)
        if (v->notes[j].inst < VI__N) {
            uint16_t e = (uint16_t)(energy[v->notes[j].inst] + 6 + (v->notes[j].vel >> 3));
            energy[v->notes[j].inst] = e > 31 ? 31 : e;
        }
    for (uint8_t i = 0; i < VI__N; i++) {
        uint16_t lv = (uint16_t)(v->level[i] >> 11);
        for (uint8_t x = 0; x < 8; x++) {
            uint16_t h = (uint16_t)(energy[i] + (lv > 4 ? lv - 4 : 0));
            h = (uint16_t)(h > 4 ? h - (frand() & 3) : h);
            heat[FH][i * 8 + x] = (uint8_t)(h > 31 ? 31 : h);
        }
        energy[i] = (uint16_t)(energy[i] > df ? energy[i] - df : 0);
    }
    for (uint8_t y = 0; y < FH; y++)
        for (uint8_t x = 0; x < FW; x++) {
            uint8_t l = heat[y + 1][x ? x - 1 : 0], m = heat[y + 1][x], r = heat[y + 1][x < FW - 1 ? x + 1 : x];
            uint16_t h = (uint16_t)((l + m + m + r) >> 2);
            uint8_t cool = (uint8_t)(frand() & 1);
            heat[y][x] = (uint8_t)(h > cool ? h - cool : 0);
        }
}
static void fire_draw(uint32_t buf)
{
    for (uint8_t y = 0; y < FH; y++) {
        volatile uint32_t *row = (volatile uint32_t *)(uintptr_t)(buf + (uint32_t)(BARS_Y + 2 * y) * W);
        for (uint8_t x = 0; x < FW; x++) {
            uint32_t c = C_FIRE + heat[y][x];
            c |= c << 8; c |= c << 16;
            row[2 * x] = c; row[2 * x + 1] = c;
            row[2 * x + W / 4] = c; row[2 * x + 1 + W / 4] = c;
        }
    }
}

/* ---- state ---- */
static uint8_t back;                             /* the hidden A buffer */
static uint16_t last_frame;
static const char *last_title, *last_section;
static uint32_t cyc_spent;
static uint16_t star_x[64], star_y[64];

/* the keyboard into a buffer: all of it, or only the keys whose colour changed since it was drawn
 * there (then all the black keys again, over the whites) */
#define NKEY (HIGH_MIDI - LOW_MIDI + 1)
static uint8_t lit[NKEY], drawn[2][NKEY];
static void keyboard(uint32_t buf, uint8_t all)
{
    uint8_t *d = drawn[buf == BUF_A1], any = all;
    if (all) {
        rect(buf, 0, KEY_TOP - 3, W, 3, C_RAIL);
        rect(buf, 0, KEY_TOP, W, KEY_H, C_KEYGAP);
    }
    for (uint8_t m = LOW_MIDI; m <= HIGH_MIDI; m++) {
        uint8_t k = (uint8_t)(m - LOW_MIDI), l = lit[k];
        if (isblk[k % 12] || (!all && d[k] == l)) continue;
        uint16_t kw, kx = key_x(m, &kw);
        rect(buf, kx, KEY_TOP, kw, KEY_H, l ? INST(l - 1, 1) : C_WHITE);
        d[k] = l; any = 1;
    }
    for (uint8_t m = LOW_MIDI; m <= HIGH_MIDI; m++) {
        uint8_t k = (uint8_t)(m - LOW_MIDI), l = lit[k];
        if (!isblk[k % 12] || (!any && d[k] == l)) continue;
        uint16_t kw, kx = key_x(m, &kw);
        rect(buf, kx, KEY_TOP, kw, BKEY_H, l ? INST(l - 1, 2) : C_BLACKKEY);
        d[k] = l;
    }
}

/* the notes' ring: screen line y shows ring line (y + scroll) mod 360; a note's rows are at
 * (-u) mod 360, u its time in pixels (12 an eighth note), so the scroll is -(KEY_TOP + u_now) */
#define LOOK_PX 190                              /* (within the notes listed: VIZ_LOOK is 192 pixels) */
static uint32_t u_drawn;
static uint8_t ring_started;
static uint16_t ring_row(uint32_t u) { return (uint16_t)((360u - (uint16_t)(u % 360)) % 360); }

/* the sparks drawn into each buffer, to erase there next time */
static uint32_t sp_at[2][NSPARK];
static uint16_t sp_n[2];

static void palette_stars(uint16_t f)
{
    PAL_INDEX = C_STAR;
    for (uint16_t i = 0; i < N_STAR; i++) {
        uint16_t l = (uint16_t)(50 + ((sn[(uint8_t)(f * (2 + i) + i * 37)] + 32768L) >> 9));   /* twinkling */
        PAL_DATA = rgb((uint8_t)(l > 255 ? 255 : l), (uint8_t)(l * 3 / 4), (uint8_t)(l / 2));
    }
}

void viz_init(void)
{
    for (uint16_t i = 0; i < 256; i++) sn[i] = (int16_t)(32767.0f * sinf((float)i * 6.2831853f / 256.0f));
    PAL_INDEX = 0;
    PAL_DATA = 0;
    PAL_DATA = rgb(0, 0, 0);       PAL_DATA = rgb(235, 240, 250); PAL_DATA = rgb(150, 150, 170);
    PAL_DATA = rgb(60, 60, 80);    PAL_DATA = rgb(40, 44, 60);    PAL_DATA = rgb(12, 12, 16);
    PAL_DATA = rgb(120, 30, 20);   PAL_DATA = rgb(8, 8, 12);      PAL_DATA = rgb(255, 230, 180);
    palette_stars(0);
    PAL_INDEX = C_INST;
    for (uint8_t i = 0; i < VI__N; i++)
        for (uint8_t s = 0; s < 8; s++) {
            uint16_t k = (uint16_t)(256 - s * 30);
            PAL_DATA = rgb((uint8_t)(inst_rgb[i][0] * k >> 8), (uint8_t)(inst_rgb[i][1] * k >> 8), (uint8_t)(inst_rgb[i][2] * k >> 8));
        }
    PAL_INDEX = C_GLOW;                         /* white, through the colour, to dim */
    for (uint8_t i = 0; i < VI__N; i++)
        for (uint8_t s = 0; s < 8; s++) {
            uint16_t wt = s < 4 ? (uint16_t)(256 - s * 72) : 0;            /* white's share */
            uint16_t k = s < 4 ? 256 : (uint16_t)(256 - (s - 3) * 42);     /* then dimming */
            uint8_t c[3];
            for (uint8_t q = 0; q < 3; q++) {
                uint16_t v = (uint16_t)((inst_rgb[i][q] * (256 - wt) + 255 * wt) >> 8);
                c[q] = (uint8_t)(v * k >> 8);
            }
            PAL_DATA = rgb(c[0], c[1], c[2]);
        }
    PAL_INDEX = C_FIRE;                         /* fire: black, red, orange, yellow, white */
    for (uint16_t i = 0; i < N_FIRE; i++) {
        uint16_t r = i < 11 ? i * 23 : 255, g = i < 11 ? 0 : i < 22 ? (i - 11) * 22 : 255, b = i < 22 ? 0 : (i - 22) * 25;
        PAL_DATA = rgb((uint8_t)r, (uint8_t)(g > 255 ? 255 : g), (uint8_t)(b > 255 ? 255 : b));
    }
    for (uint8_t k = 0; k < 64; k++) { star_x[k] = (uint16_t)(rnd() % W); star_y[k] = (uint16_t)(FALL_TOP + rnd() % (KEY_TOP - FALL_TOP - 4)); }
    for (uint8_t b = 0; b < 2; b++) {
        uint32_t buf = b ? BUF_A1 : BUF_A0;
        rect(buf, 0, 0, W, H, C_BLACK);
        rect(buf, 0, FALL_TOP, W, KEY_TOP - 3 - FALL_TOP, 0);      /* see-through: the notes (B) below */
        keyboard(buf, 1);
    }
    rect(BUF_B, 0, 0, W, H, 0);
    make_sprites();
    make_flashes();
    for (uint8_t n = 0; n < NSPR; n++) { pt[n].on = 0; spr_shadow(n); }
    spr_send();
    for (uint16_t i = 0; i < 80 * 25; i++) TEXT[i] = 0x4000 | ' ';
    for (uint8_t i = 0; i < VI__N; i++) txt((uint16_t)(1 + i * 8), 24, inst_name[i], 7);
    VID_BMP_LO = (uint16_t)BUF_A0; VID_BMP_HI = (uint16_t)(BUF_A0 >> 16);
    VID_BMPB_LO = (uint16_t)BUF_B; VID_BMPB_HI = (uint16_t)(BUF_B >> 16);
    VID_SCROLLX = 0; VID_SCROLLY = 0; VID_SXB = 0; VID_SYB = 0;
    VID_CTRL = 0x07;                             /* text, bitmap, hi-colour */
    VID_CTRL2 = 0x09;                            /* playfield B (behind A), hardware sprites */
    back = 1;
    last_frame = FRAME;
}

static uint32_t cyc(void) { return CYCLE; }

void viz_frame(const viz_t *v)
{
    /* the last frame's flip, then the start of a vertical blank: sprites, palette */
    while (VID_STAT & 1) ;
    while (!(VID_STAT & 2)) ;
    uint32_t t0 = cyc();
    uint16_t f = FRAME;
    uint16_t df = (uint16_t)(f - last_frame);
    if (df == 0) df = 1;
    if (df > 8) df = 8;
    last_frame = f;
    spr_send();
    palette_stars(f);

    /* ---- where notes land: a flash (a sprite) and a spray of sparks; the bouncer kicked ---- */
    for (uint8_t j = 0; j < v->nnotes; j++) {
        const viz_note_t *nt = &v->notes[j];
        if (nt->inst >= VI__N || nt->midi < LOW_MIDI || nt->midi > HIGH_MIDI) continue;
        uint16_t kw, kx = key_x(nt->midi, &kw);
        part_t *p = &pt[next_spr];
        next_spr = (uint8_t)((next_spr + 1) % NPART);
        p->on = 1; p->inst = nt->inst; p->age = 0; p->life = 12;
        p->x = (int16_t)((kx + kw / 2 - 16) << 4);
        p->y = (int16_t)((KEY_TOP - 18) << 4);                           /* centred on the keys' top edge */
        p->vx = 0; p->vy = 0;
        for (uint8_t k = 0; k < 4 + (nt->vel >> 4); k++) spark(kx, nt->inst, (uint8_t)(nt->vel >> 2));
        part_t *b = &pt[NPART + nt->inst];
        b->vy = (int16_t)-(0x70 + (nt->vel >> 1));
    }
    alive = 0;
    for (uint8_t i = 0; i < NPART; i++) {
        part_t *p = &pt[i];
        if (p->on) {
            p->age = (uint8_t)(p->age + df > 255 ? 255 : p->age + df);
            if (p->age >= p->life) p->on = 0;
            else { p->y = (int16_t)(p->y + p->vy * (int16_t)df); alive++; }
        }
        spr_shadow(i);
    }
    for (uint8_t i = 0; i < VI__N; i++) {                 /* the bouncers, on the fire */
        part_t *b = &pt[NPART + i];
        int16_t floor = (int16_t)((BARS_Y + BARS_H - 17) << 4);
        b->on = 1; b->inst = i; b->age = 0; b->life = 255;
        b->x = (int16_t)((8 + i * 64 + 19) << 4);
        for (uint16_t k = 0; k < df; k++) {
            b->vy = (int16_t)(b->vy + 32);
            b->y = (int16_t)(b->y + b->vy);
            if (b->y >= floor) { b->y = floor; b->vy = (int16_t)(b->vy > 0x60 ? -(b->vy >> 2) : 0); }
        }
        if (b->y < (KEY_TOP + KEY_H) << 4) { b->y = (int16_t)((KEY_TOP + KEY_H) << 4); b->vy = 0; }   /* under the keys */
        spr_shadow((uint8_t)(NPART + i));
    }

    /* ---- the falling notes: B's scroll, and the rows newly in view drawn into the ring ---- */
    uint32_t u_now = v->now16 * PX_TICK / 16;
    VID_SYB = (uint16_t)((720u - (uint16_t)((KEY_TOP + u_now) % 360)) % 360);
    for (uint8_t k = 0; k < NKEY; k++) lit[k] = 0;
    for (uint16_t j = 0; j < v->nfall; j++) {
        const viz_fall_t *n = &v->fall[j];
        if (n->midi >= LOW_MIDI && n->midi <= HIGH_MIDI && n->inst < VI__N && n->start <= 0 && n->start + (int16_t)n->len > 0)
            lit[n->midi - LOW_MIDI] = (uint8_t)(n->inst + 1);
        if (n->start <= 0 && n->start + (int16_t)n->len > 0 && (rnd() & 15) == 0 && n->midi >= LOW_MIDI && n->midi <= HIGH_MIDI) {
            uint16_t kw, kx = key_x(n->midi, &kw);
            spark(kx, n->inst, 0);                                   /* a sounding note smoulders */
        }
    }
    {
        uint32_t top = u_now + LOOK_PX;
        if (!ring_started || top - u_drawn > 300) { u_drawn = top > 300 ? top - 300 : 0; ring_started = 1; }
        for (uint32_t u = u_drawn + 1; u <= top; u++) {
            uint16_t r = ring_row(u);
            rect(BUF_B, 0, r, W, 1, 0);
            for (uint16_t j = 0; j < v->nfall; j++) {
                const viz_fall_t *n = &v->fall[j];
                if (n->midi < LOW_MIDI || n->midi > HIGH_MIDI || n->inst >= VI__N) continue;
                int32_t s16 = (int32_t)v->now16 + n->start;                /* its exact start, 1/16 ticks */
                uint32_t a = (uint32_t)(s16 * PX_TICK / 16), e = (uint32_t)((s16 + (int32_t)n->len) * PX_TICK / 16);
                if (u < a || u >= e) continue;
                uint16_t kw, kx = key_x(n->midi, &kw);
                if (u <= a + 1 || u >= e - 2 || kw < 5)
                    rect(BUF_B, kx, r, kw, 1, INST(n->inst, 1));             /* its ends: solid */
                else {
                    rect(BUF_B, kx, r, 2, 1, INST(n->inst, 0));              /* its sides bright, its core dark */
                    rect(BUF_B, (uint16_t)(kx + 2), r, (uint16_t)(kw - 4), 1, INST(n->inst, 6));
                    rect(BUF_B, (uint16_t)(kx + kw - 2), r, 2, 1, INST(n->inst, 0));
                }
            }
        }
        u_drawn = top;
    }

    /* ---- playfield A (the hidden buffer): last time's sparks erased, the stars, the keys ---- */
    uint32_t buf = back ? BUF_A1 : BUF_A0;
    {
        volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)buf;
        uint8_t bi = back;
        for (uint16_t k = 0; k < sp_n[bi]; k++) { uint32_t ad = sp_at[bi][k]; p[ad] = 0; p[ad + W] = 0; }
        sp_n[bi] = 0;
        for (uint8_t k = 0; k < 64; k++) p[(uint32_t)star_y[k] * W + star_x[k]] = (uint8_t)(C_STAR + (k & 7));
        keyboard(buf, 0);
        bwait();
        /* the sparks: they fly up, slow, fall back and fade */
        uint32_t *at = sp_at[bi];
        uint16_t na = 0;
        for (uint16_t k = 0; k < nsp; ) {
            spark_t *s = &sp[k];
            uint16_t h = (uint16_t)(s->heat + s->rate * df);
            s->x = (int16_t)(s->x + s->vx * (int16_t)df);
            s->y = (int16_t)(s->y + s->vy * (int16_t)df);
            s->vy = (int16_t)(s->vy + (int16_t)df);                          /* gravity 1/16 */
            int16_t x = (int16_t)(s->x >> 4), y = (int16_t)(s->y >> 4);
            if (h >= 2048 || x < 0 || x >= W - 1 || y < FALL_TOP || y > KEY_TOP - 5) {
                *s = sp[--nsp];                                              /* cooled: the last one moves in */
                continue;
            }
            s->heat = h;
            uint8_t c = GLOW(s->inst, (uint8_t)(h >> 8));                    /* white-hot, cooling */
            uint32_t ad = (uint32_t)y * W + (uint16_t)x;
            p[ad] = c; p[ad + W] = c;
            at[na++] = ad;
            k++;
        }
        sp_n[bi] = na;
    }

    /* the fire and the floor the bouncers sit on */
    fire_step(v, df);
    fire_draw(buf);

    /* ---- the text layer: title, section, position, the HUD ---- */
    if (v->title != last_title) { last_title = v->title; txtpad(0, 0, v->title ? v->title : "", 7, 80); }
    if (v->section != last_section) { last_section = v->section; txtpad(0, 1, v->section ? v->section : "", 6, 66); }
    {
        char s[80], *q = s;
        q = cat(q, "bar "); q = utoa_(q, v->bar, 3); q = cat(q, (v->beat & 1) ? "  ."  : "  *");
        txtpad(68, 1, s, 3, 12);
        q = s;
        q = cat(q, "frame "); q = utoa_(q, cyc_spent, 6);
        q = cat(q, " cyc  notes "); q = utoa_(q, v->nfall, 3);
        q = cat(q, " sparks "); q = utoa_(q, nsp, 4);
        q = cat(q, " over "); q = utoa_(q, SPR_OVER, 3);
        q = cat(q, "  engine "); q = utoa_(q, (uint32_t)v->blocks_busy * 100 / 45000, 3); q = cat(q, "%  underruns ");
        q = utoa_(q, v->underruns, 5);
        txtpad(0, 2, s, v->underruns ? 1 : 2, 80);
    }
    bwait();

    /* ---- the flip: the new A buffer from the next frame ---- */
    VID_BMP_LO = (uint16_t)buf; VID_BMP_HI = (uint16_t)(buf >> 16);
    back ^= 1;
    cyc_spent = cyc() - t0;
}

uint32_t viz_cycles(void) { return cyc_spent; }
