/* sidplay: a PSID player and tracker display for the Tang Nano 20K (see ../README.md).
 * int is 16 bits on the target: all widths are explicit. */
#ifndef SIDPLAY_H
#define SIDPLAY_H
#include <stdint.h>

/* ---- platform (hw_target.c / hw_host.c) ---- */
void aud_raw(uint16_t addr, uint16_t val);      /* audio engine register write (address relative to F00000) */
uint32_t cyc_now(void);                         /* CPU cycle counter (T1 periods, 16.875 MHz on the Tang) */

/* ---- PSID ---- */
typedef struct {
    uint16_t load, init, play, songs, start, flags;
    uint32_t speed;
    uint8_t version, rsid;
    char name[33], author[33], released[33];
    const uint8_t *data;                        /* the music data after the header (and the load address) */
    uint16_t dlen;
} psid_t;
/* 0 = ok; else an error code (psid_err) */
int psid_parse(const uint8_t *buf, uint32_t len, psid_t *p);
const char *psid_err(int e);

/* ---- one frame of SID state (what one play() call left in the registers) ---- */
typedef struct {
    uint8_t r[25];                              /* $D400-$D418 after the call */
    uint8_t rise, fall;                         /* bit v: a gate 0->1 / 1->0 write to voice v happened in the call */
    uint8_t wr;                                 /* bit v: voice v's ctrl/freq registers were written */
    uint8_t pad[4];
} frame_t;                                      /* 32 bytes */

/* ---- tune runner (player.c) ---- */
extern uint8_t pl_clock_ntsc;                   /* 1 = NTSC clock */
extern uint32_t pl_rate_x100;                   /* play calls per second x 100 */
extern uint32_t pl_period48;                    /* one play period in 48 kHz engine frames */
extern uint32_t pl_cyc_per_call;                /* 6502 cycles per play call */
extern uint32_t pl_clk_hz;                      /* SID clock in Hz */
extern uint8_t pl_jit;                          /* 1 = run the 6502 with the JIT, 0 = interpreter */
extern void (*pl_log_hook)(void);             /* called with sidlog/sid_n filled, before it is cleared */
extern uint32_t pl_instrs;                      /* 6502 cycles run by the last frame */
int pl_start(const psid_t *p, int song);        /* load, init; 0 = ok (a RUN_ status otherwise) */
int pl_frame(frame_t *f);                       /* one play() call; 0 = ok */

/* ---- SID -> audio engine (sidmap.c) ---- */
extern uint8_t sm_mute;                         /* bit v: SID voice v silent */
extern uint8_t sm_spread;                       /* stereo spread 0 (mono) .. 255 */
void sm_init(void);
void sm_set_clock(uint32_t hz);
void sm_apply(const frame_t *f);
void sm_silence(void);
uint16_t sm_env(uint8_t v);                     /* hardware ENV level (16 bits) of SID voice v's engine voice */
uint8_t sm_engine_present(void);

#endif
