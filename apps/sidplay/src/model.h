/* The tracker's view of a tune: instruments, speed and rows, built from the SID register stream.
 * (A song/pattern/instrument model fed by the register stream now; the same structures could later be
 * edited and turned back into a driver's data.) */
#ifndef MODEL_H
#define MODEL_H
#include "sidplay.h"

#define N_INST 40                               /* instrument slots (1..N_INST-1 used) */
#define N_ROWS 160                              /* row ring */

typedef struct {
    uint8_t ctrl;                               /* waveform, ring, sync bits (ctrl & 0xF6) */
    uint8_t ad, sr;
} inst_t;

typedef struct {
    uint8_t note;                               /* 0 empty, 1..96 = note + 1, 0xFE gate off */
    uint8_t inst;                               /* 0 none */
    uint8_t wave;                               /* ctrl & 0xF6 at a note start */
    uint8_t fxc;                                /* effect character ('0' arpeggio, '1' slide up, '2' slide down, '4' vibrato, 'P' pulse), 0 none */
    uint8_t fxv;                                /* effect value (two hex digits) */
} cell_t;

typedef struct {
    uint32_t frame0;                            /* first play call of the row */
    uint8_t len;                                /* play calls in the row */
    cell_t c[3];
} row_t;

void model_init(void);
void model_detect(const frame_t *ring, uint16_t n);   /* first frames: find the speed and the row grid before feeding */
uint8_t model_nearest(uint16_t fn);
void model_feed(const frame_t *f, uint32_t frame_no);  /* call once per play call, in order */
uint16_t model_nrows(void);                     /* rows finished so far (absolute count) */
const row_t *model_row(uint16_t abs);           /* a finished row (the last N_ROWS) or NULL */
int16_t model_row_at(uint32_t frame);           /* absolute row containing the frame, -1 if none yet */
uint8_t model_speed(void);                      /* detected play calls per row */
uint8_t model_ninst(void);
const inst_t *model_inst(uint8_t i);
const char *note_name(uint8_t n);               /* "C-4" for n = 1..96 (static buffer) */

#endif
