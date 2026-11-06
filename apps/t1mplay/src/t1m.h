/* T1M player (docs/t1m.md) on the microcoded audio engine (docs/mcaudio.md).
 *
 * The module says what the music is; this player decides how the engine makes it: which of the
 * engine's routines a voice runs, how many nodes it takes, what is done in hardware (envelopes,
 * the oscillators' LFO, the filters and their sweep) and what in software every tick (sequences,
 * slides, arpeggios, vibrato on samples and pairs). Node words are written only from the
 * engine's block-done interrupt, t1m_block(). */
#ifndef T1M_H
#define T1M_H
#include <stdint.h>

#define T1M_MAXCH 32

enum {                                  /* t1m_load's errors */
    T1M_OK, T1M_EMAGIC, T1M_EVERSION, T1M_ECHUNK, T1M_ENODES, T1M_ESIZE
};

typedef struct {                        /* a channel, for a display (read it outside the interrupt) */
    uint8_t note;                       /* the sounding note (0xFF none) */
    uint8_t inst;                       /* 0xFF none */
    uint8_t vol;                        /* the channel's level */
    uint8_t gate;                       /* 1 held, 0 released or silent */
    uint8_t node, nodes;                /* its first engine node and how many it has */
    uint8_t mute;
} t1m_chan_t;

typedef struct {
    const char *title, *author, *comment;
    uint8_t channels, speed, order, orders, pattern, row, rows, gvol;
    uint16_t tick_hz256;                /* the tick rate, Hz x 256 */
    uint16_t needs, voices;             /* the header's */
    uint8_t nodes;                      /* engine nodes in use (voices and filters) */
    uint8_t ninst;
    uint32_t ticks;                     /* since the start */
    t1m_chan_t ch[T1M_MAXCH];
} t1m_state_t;

extern volatile t1m_state_t t1m;

int t1m_load(const void *module);       /* parse, check, place the voices on nodes: T1M_OK or an error */
const char *t1m_error(int e);
void t1m_start(void);                   /* the engine (mc_init), the node list; then enable IRQ_AUDIO */
void t1m_block(void);                   /* the block-done interrupt's work */
const char *t1m_inst_name(uint8_t i);   /* (16 characters at most, not terminated at 16) */
/* a pattern's notes and instruments for a display: [row x nch + channel], 0xFF empty (0x80 key off, 0x81 cut);
 * returns its rows */
uint16_t t1m_decode(uint8_t pat, uint8_t *note, uint8_t *inst, uint8_t nch, uint16_t max_rows);
void t1m_goto(uint8_t order);           /* jump to an order position (from the interrupt's side: takes effect next tick) */
void t1m_pause(uint8_t on);
#endif
