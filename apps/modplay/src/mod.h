/* modplay: an Amiga ProTracker MOD player for the Tang Nano 20K's audio engine (see ../README.md).
 * int is 16 bits on the target: all widths are explicit. */
#ifndef MOD_H
#define MOD_H
#include <stdint.h>

#define NCH 8                   /* channels at most (M.K./4CHN: 4, 6CHN: 6, 8CHN: 8) */

typedef struct {
    char name[23];
    uint32_t addr;              /* byte address of the sample data in SDRAM */
    uint32_t len;               /* bytes */
    uint32_t rep, replen;       /* bytes; replen > 2: a loop */
    int8_t finetune;            /* -8..7 */
    uint8_t vol;                /* 0..64 */
} sample_t;

typedef struct {
    uint32_t base, size;        /* the file in SDRAM */
    uint8_t nch;                /* channels: 4, 6 or 8 (engine voices 0 .. nch-1) */
    char title[21];
    uint8_t songlen, restart;
    uint8_t order[128];
    uint8_t npat;
    uint32_t pat;               /* address of pattern 0 (64 rows x nch channels x 4 bytes each) */
    sample_t smp[32];           /* 1..31 */
} mod_t;

/* one pattern cell, decoded */
typedef struct {
    uint16_t period;
    uint8_t sample, fx, arg;
} cell_t;

/* channel state (what the display shows too) */
typedef struct {
    uint8_t sample;             /* current sample 1..31, 0 none */
    uint16_t period;            /* the note's period (before vibrato/arpeggio) */
    uint16_t out_period;        /* what the engine plays this tick */
    int8_t vol;                 /* 0..64 */
    int8_t out_vol;             /* with tremolo */
    int8_t finetune;
    uint8_t note;               /* 0..35, 0xFF none: the last note started */
    uint8_t fx, arg;            /* this row's effect */
    uint16_t porta_target;
    uint8_t porta_speed;
    uint8_t vib_speed, vib_depth, vib_pos, vib_wave;
    uint8_t trem_speed, trem_depth, trem_pos, trem_wave;
    uint8_t offset;             /* 9xx memory */
    uint8_t loop_row, loop_cnt; /* E6x */
    uint8_t pan;                /* 0 left .. 255 right */
    uint8_t trig;               /* a note started this row (for the display's meters) */
    uint8_t delay_cell;         /* EDx: the row's cell waits for this tick */
    cell_t pend;                /* the delayed cell */
} chan_t;

typedef struct {
    uint8_t pos, row;           /* song position, row: the next to be read */
    uint8_t ppos, prow;         /* the position and row being heard (read last) */
    uint8_t speed, bpm, tick;
    uint8_t pat_delay;          /* EEx rows still to repeat */
    uint8_t mute;               /* bit c: channel c silent */
    uint8_t interp;             /* 1: linear interpolation (else Amiga-style nearest) */
    uint8_t sep;                /* stereo separation 0 (mono) .. 128 (hard Amiga) */
    uint8_t ended;              /* the song wrapped to its restart position */
    uint8_t row_changed;        /* set when a new row starts (cleared by the display) */
} play_t;

extern mod_t mod;
extern chan_t ch[NCH];
extern play_t pl;

/* 0 = ok, else an error code (mod_err) */
int mod_parse(uint32_t base, mod_t *m);
const char *mod_err(int e);
uint32_t mod_size(uint32_t base);              /* a MOD file's size from its header, 0 = not a MOD we play */
void mod_cell(uint8_t pattern, uint8_t row, uint8_t c, cell_t *out);

void mod_start(void);                          /* from position 0 */
void mod_goto(uint8_t pos);                    /* a song position, row 0 */
void mod_tick(void);                           /* one tick: call speed x per row, at bpm x 2 / 5 Hz */
void mod_silence(void);
uint32_t mod_tick_frames_x100(void);           /* one tick in 48 kHz frames x 100 */
uint8_t mod_note_of(uint16_t period);          /* 0..35, 0xFF none */

#endif
