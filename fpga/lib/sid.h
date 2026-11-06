/* A 3-voice SID (MOS 6581) synthesizer in software, into the sound's
 * streaming player (docs/io.md "Sound": the Spartan-3 has the stream only).
 *
 *   sid_init(buf, len);        // a ring of len frames at buf; the player starts
 *   sid_write(0x04, 0x41);     // the SID's registers 00-18 (D400-D418)
 *   sid_render(sid_room());    // keep the ring filled behind STR_POS
 *
 * The voices: the oscillators (24-bit, the SID's own frequency values at
 * its PAL clock, 985,248 Hz), triangle, sawtooth, pulse (12-bit width) and
 * noise (23-bit LFSR) and their combinations (ANDed), hard sync, ring
 * modulation, the test bit; ADSR envelopes with the SID's rates and its
 * exponential decay and release; the master volume, and voice 3 off
 * (D418 bit 7). Not modelled: the filter (its routing bits are ignored).
 *
 * The output rate is SID_RATE (the player's 48,828 Hz / SID_DIV). A sample
 * of three voices costs ~60 instructions (~120 while a voice uses sync or
 * ring modulation, or combined waveforms), when rendered in blocks of 128
 * samples or more (each call has its set-up): on the Spartan-3's T1S that
 * is 25-40% of the CPU. */
#ifndef FPGA_SID_H
#define FPGA_SID_H
#include <stdint.h>

#define SID_DIV  4                           /* each sample plays for 4 output frames */
#define SID_RATE (48828u / SID_DIV)          /* 12,207 Hz */

void sid_init(uint32_t buf, uint16_t len);   /* len frames (32-bit words) at buf */
void sid_write(uint8_t reg, uint8_t v);
uint16_t sid_room(void);                     /* samples that can be rendered now */
void sid_render(uint16_t n);
void sid_stop(void);
/* the voices as a simple sound device (fpga/demo/music.c): set voice v's
 * level directly (0-255), its envelope then left alone until the gate bit
 * changes */
void sid_level(int v, uint8_t level);
/* after a pause in rendering (longer than the ring): the stream muted
 * (mute 1), or restarted just ahead of the player and unmuted (mute 0) */
void sid_mute(int mute);
extern uint8_t sid_env[3];                   /* the envelopes (0-255), for a display */
#endif
