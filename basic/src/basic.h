/* BASIC interpreter: shared declarations.
 *
 * Portability rule: int is 16 bits on the target. Anything that can
 * exceed +-32767 uses int32_t/uint32_t explicitly, and pointer
 * differences are stored in int32_t.
 */
#ifndef BASIC_H
#define BASIC_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal.h"

/* ---- I/O registers and memory layout (docs/io.md) ---- */
#define CON_OUT      0xF00000UL
#define REG_FRAME    0xF0000AUL
#define VID_CTRL     0xF00200UL
#define VID_BACKDROP 0xF00202UL
#define VID_TEXT     0xF00204UL
#define VID_BMP      0xF00208UL
#define VID_SPR      0xF0020CUL
#define PAL_INDEX    0xF00210UL
#define PAL_DATA     0xF00212UL
#define VID_SCROLLX  0xF00214UL
#define VID_SCROLLY  0xF00216UL
#define VID_CURSOR   0xF00218UL
#define SND_BASE     0xF00400UL

#define VC_TEXT   0x01
#define VC_BMP    0x02
#define VC_HICOL  0x04
#define VC_SPR    0x08
#define VC_SPRBUF 0x10
#define VC_CURSOR 0x20
#define VC_SPR8   0x40

#define SPR_ADDR  0x053400UL /* two 8 bpp buffers */
#define SPR_SIZE  230400UL
#define BMP_ADDR  0x0C3C00UL
#define TEXT_ADDR 0x0FC000UL

#define SCR_W 640
#define SCR_H 360
#define COLS  80
#define ROWS  25

/* ---- values ---- */
typedef struct {
    char *p; /* NULL for the empty string */
    uint16_t len;
} str_t;

enum { T_NUM, T_INT, T_STR, T_SREF };
typedef struct {
    uint8_t t;
    union {
        float f;
        int16_t i;
        str_t s;  /* T_STR: a heap temporary or text in the program */
        str_t *r; /* T_SREF: a variable's string, not copied */
    } u;
} val_t;

enum { VT_NUM, VT_INT, VT_STR };
typedef union {
    float f;
    int16_t i;
    str_t s;
} var_t;

/* ---- tokens ----
 * A program line is [len lo][len hi][line lo][line hi] tokens 0, where
 * len is the size of the whole line. The program ends with len = 0.
 */
#define TK_INT 0x01 /* int16, little-endian */
#define TK_FLT 0x02 /* float, 4 bytes little-endian */
#define TK_STR 0x03 /* length byte, then text */
#define TK_VAR 0x04 /* uint16 name id */
#define TK_RAW 0x05 /* length byte, then text: after REM and DATA */
#define TK_LE  0x10
#define TK_GE  0x11
#define TK_NE  0x12

/* Statements first, then the rest. Functions start at KW_ABS. */
#define KEYWORDS                                                                                   \
    K(END, "END") K(FOR, "FOR") K(NEXT, "NEXT") K(DATA, "DATA") K(INPUT, "INPUT") K(DIM, "DIM")  \
    K(READ, "READ") K(LET, "LET") K(GOTO, "GOTO") K(RUN, "RUN") K(IF, "IF")                       \
    K(RESTORE, "RESTORE") K(GOSUB, "GOSUB") K(RETURN, "RETURN") K(REM, "REM") K(STOP, "STOP")      \
    K(ON, "ON") K(DEF, "DEF") K(POKE, "POKE") K(DOKE, "DOKE") K(PRINT, "PRINT") K(CONT, "CONT")    \
    K(LIST, "LIST") K(CLEAR, "CLEAR") K(NEW, "NEW") K(GET, "GET") K(CALL, "CALL")                  \
    K(LOAD, "LOAD") K(SAVE, "SAVE") K(DIR, "DIR") K(DEL, "DEL") K(CLS, "CLS")                      \
    K(LOCATE, "LOCATE") K(COLOR, "COLOR") K(TEXT, "TEXT") K(BACKDROP, "BACKDROP") K(GR, "GR")      \
    K(HGR, "HGR") K(GCLS, "GCLS") K(PEN, "PEN") K(PLOT, "PLOT") K(LINE, "LINE") K(BOX, "BOX")      \
    K(CIRCLE, "CIRCLE") K(PALETTE, "PALETTE") K(SPRITE, "SPRITE") K(VSYNC, "VSYNC")                \
    K(PICTURE, "PICTURE") K(SHEET, "SHEET") K(STAMP, "STAMP")                                      \
    K(SOUND, "SOUND") K(WAVE, "WAVE") K(BEEP, "BEEP") K(PAUSE, "PAUSE") K(SCROLL, "SCROLL")        \
    K(ELSE, "ELSE") K(THEN, "THEN") K(TO, "TO") K(STEP, "STEP") K(AND, "AND") K(OR, "OR")          \
    K(NOT, "NOT") K(MOD, "MOD") K(AT, "AT") K(OFF, "OFF") K(DEFINE, "DEFINE") K(FRAME, "FRAME")    \
    K(HOTSPOT, "HOTSPOT") K(MOVE, "MOVE") K(ROTATE, "ROTATE") K(SCALE, "SCALE")                   \
    K(ANIMATE, "ANIMATE") K(FLIP, "FLIP")                                                          \
    K(ABS, "ABS") K(INT, "INT") K(SGN, "SGN") K(SQR, "SQR") K(SIN, "SIN") K(COS, "COS")            \
    K(TAN, "TAN") K(ATN, "ATN") K(EXP, "EXP") K(LOG, "LOG") K(RND, "RND") K(LEN, "LEN")            \
    K(LEFTS, "LEFT$") K(RIGHTS, "RIGHT$") K(MIDS, "MID$") K(CHRS, "CHR$") K(ASC, "ASC")            \
    K(STRS, "STR$") K(VAL, "VAL") K(HEXS, "HEX$") K(INSTR, "INSTR") K(FRE, "FRE")                  \
    K(PEEK, "PEEK") K(DEEK, "DEEK") K(POS, "POS") K(TIME, "TIME") K(INKEYS, "INKEY$")              \
    K(POINT, "POINT") K(FRAMES, "FRAMES") K(HIT, "HIT") K(SPRX, "SPRX") K(SPRY, "SPRY") K(PAL, "PAL") \
    K(TAB, "TAB") K(SPC, "SPC") K(FN, "FN")

enum {
    KW_FIRST_ = 0x7F,
#define K(n, s) KW_##n,
    KEYWORDS
#undef K
    KW_LAST_
};

#define RD16(p) ((uint16_t)((p)[0] | ((unsigned)(p)[1] << 8)))
#define LLEN(l) RD16(l)
#define LNUM(l) RD16((l) + 2)

/* ---- errors ---- */
enum {
    E_SN, E_RG, E_OD, E_FC, E_OV, E_OM, E_UL, E_BS, E_DD, E_DZ, E_ID, E_TM, E_LS, E_ST,
    E_CN, E_UF, E_NF, E_FNF, E_FIO, E_RD, E_LL, E_IF, E_TC
};
void error(int e) __attribute__((noreturn));
void brk(void) __attribute__((noreturn));

/* ---- interpreter state (core.c) ---- */
#define MAXV  400  /* distinct names */
#define NPOOL 4096 /* name text */
#define VSMAX 48   /* expression stack */

extern uint8_t *arena, *arena_top; /* workspace */
extern uint8_t *prog_end;          /* the end-of-program marker */
extern uint8_t *arr_start, *arr_end;
extern uint8_t *str_bot; /* string heap: str_bot .. arena_top */
extern uint8_t *ip, *lp; /* token pointer, current line (NULL = immediate) */

extern uint16_t nnames;
extern uint8_t vtype[MAXV];
extern var_t vars[MAXV];
extern uint8_t *arrs[MAXV];
extern uint8_t *fndef[MAXV];
extern uint16_t fnparam[MAXV];

extern val_t vs[VSMAX];
extern int vsp;

const char *var_name(uint16_t id, uint8_t *len);
uint16_t var_intern(const char *s, uint8_t len);
const char *kw_name(uint8_t t);
int tokenize(const char *src, uint8_t *out, int max);
uint8_t *tok_skip(uint8_t *p);
int list_line(const uint8_t *l, char *out); /* detokenize; returns length */

void prog_new(void);
void prog_edit(uint16_t num, const uint8_t *tok, uint16_t toklen);
uint8_t *find_line(uint16_t num, int exact);
void clear_vars(void);

char *str_alloc(uint16_t len);
void str_gc(void);
uint32_t mem_free(void);
void *arr_ref(uint16_t id);
void arr_dim(uint16_t id, int nd, const uint16_t *dims);

/* ---- expressions (eval.c) ---- */
void expr(void);
float pop_num(void);
int16_t pop_i16(void);
int32_t pop_i32(void);
str_t sval(const val_t *v);
void push_num(float f);
void push_int(int32_t i);
void push_newstr(const char *s, uint16_t len); /* copies s (must not be in the heap) */
void need(uint8_t t);
void assign_str(str_t *dst);
int16_t to_i16(float f);
int32_t to_i32(float f);
float num_of(const val_t *v);
void rnd_seed(uint32_t s);

/* ---- numbers (num.c) ---- */
int fmt_num(float x, char *out); /* " 12", "-1.5", " 1E+10"; returns length */
int fmt_u32(uint32_t v, char *out);
const char *parse_num(const char *s, float *f, int *isint); /* NULL if no number */
float scale10(float x, int e);

/* ---- console (con.c) ---- */
void con_init(void);
void con_putc(int c);
void con_puts(const char *s);
void con_write(const char *s, uint16_t len);
void con_cls(void);
void con_locate(int row, int col);
void con_color(int fg, int bg);
int con_col(void);
void con_flush(void);
void con_edit_begin(void);
int con_edit_key(int k, char *buf, int max); /* screen editor; returns length on Enter, else -1 */

/* ---- graphics, sprites, sound (gfx.c) ---- */
void vid_ctrl_set(uint16_t bits, int on);
void gfx_init(void);
void gfx_mode(int m); /* 0 text only, 1 GR (8-color), 2 HGR (hi-color) */
void gfx_clear(int c);
void gfx_pen(int c);
void gfx_backdrop(int c);
void gfx_plot(int16_t x, int16_t y);
int gfx_point(int16_t x, int16_t y);
void gfx_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void gfx_line_to(int16_t x, int16_t y);
void gfx_box(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int fill);
void gfx_circle(int16_t x, int16_t y, int16_t r, int fill);
void gfx_palette(int i, int r, int g, int b);
void gfx_scroll(int16_t x, int16_t y);
void spr_define(int n, int w, int h);
void spr_pixels(int n, const char *s, uint16_t len); /* appends pixels */
void spr_at(int n, int16_t x, int16_t y);
void spr_off(int n);
void spr_flush(void);
void vsync(void);
int gfx_pal_get(int i, int c);
void gfx_pal_rotate(int first, int last, int n);
void gfx_pal_load(const char *name, int base);
void gfx_picture(const char *name, int16_t x, int16_t y, int palbase); /* palbase < 0: current palette */
void gfx_stamp(int s, int f, int16_t x, int16_t y, int flip);

/* ---- sheets and sprites (sprite.c) ---- */
typedef struct {
    const uint8_t *pix; /* palette indices, 0 transparent */
    uint16_t w, h, stride;
    int16_t hx, hy;     /* hot spot */
} frame_t;
void sheet_load(int n, const char *name, int fw, int fh, int palbase); /* fw = 0: one frame */
void sheet_hotspot(int n, int16_t x, int16_t y);
int sheet_frames(int n);
void sheet_frame(int n, int f, frame_t *fr);
void sheets_reset(void); /* sheets live in the array area: CLEAR, RUN and edits drop them */
void spr_frame(int s, int sheet, int f, int flip);
void spr_move(int n, float dx, float dy);              /* velocity, pixels per frame */
void spr_turn(int n, float deg, float spin);             /* rotation and its rate, degrees */
void spr_scale(int n, float sx, float sy);
void spr_flip(int n, int flip);
void spr_animate(int n, int first, int last, int rate);
int spr_pos(int n, int y); /* SPRX / SPRY */
int spr_hit(int a, int b); /* b < 0: the lowest sprite touching a, or -1 */
void snd_set(int v, uint16_t freq, int vol, int16_t frames);
void snd_wave(int v, int w);
void snd_poll(void);

/* ---- images (image.c) ----
 * img_load() decodes a JPEG or PNG file and hands each row to the sink as
 * RGBA, top to bottom, plus the raw palette indices for an indexed PNG.
 * start() comes first, with the size and the file's own palette. The sink
 * may keep memory with img_alloc() (it lives in the array area) and borrow
 * scratch with img_scratch() (gone afterwards). row() returns nonzero when
 * it wants no more rows. Inside a sink, report errors with img_fail(),
 * not error(), so the file gets closed.
 */
typedef struct {
    uint16_t w, h;
    uint8_t photo;      /* JPEG: worth dithering */
    uint16_t npal;      /* indexed PNG: palette entries (0 otherwise) */
    const uint8_t *pal; /* npal x RGBA (alpha from tRNS) */
} img_info_t;

typedef struct img_sink img_sink_t;
struct img_sink {
    void (*start)(img_sink_t *s, const img_info_t *in);
    int (*row)(img_sink_t *s, uint16_t y, const uint8_t *rgba, const uint8_t *idx); /* idx: indexed PNG only */
};
void img_load(const char *name, img_sink_t *s);
int img_palette(const char *name, uint8_t (*out)[3], int max); /* PNG, JPEG, GIMP .GPL or JASC .PAL */
void *cmap_new(const img_info_t *in, int palbase);              /* gfx.c: image colors to entries */
uint8_t cmap_px(void *cm, const uint8_t *rgba, const uint8_t *idx, uint16_t x);
uint8_t *arena_alloc(uint32_t n);                               /* core.c: array-area memory */
uint8_t *img_alloc(uint32_t n);
void *img_scratch(uint32_t n);
void img_fail(int e) __attribute__((noreturn));

#endif
