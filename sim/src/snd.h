/* fpga_cpu simulator: sound out, and the Tang's audio engines.
 *
 * Every sound device makes its output frames lazily, up to the CPU's cycle count (like audlo.c),
 * and hands each one to snd_out(). snd_out() keeps them in a ring the GUI's and the web page's
 * audio callbacks drain (snd_pull) and, with --wav, writes them to a WAV file.
 *
 * The devices, by hardware (fsim --hw):
 *   spartan3   audlo.c   the low tier's player (fpga/rtl/audio_lite.v), 48828 Hz
 *   tang       audeng.c  the audio engine (fpga/rtl/audio_engine.v, docs/audio.md), 48 kHz
 *   tang-mc    mcaud.c   the microcoded audio engine (fpga/rtl/mcaudio, docs/mcaudio.md), 48 kHz
 *   sim, big   none here: the 3-voice F00400 device is synthesized by sound_fill (dev.c)
 */
#ifndef SND_H
#define SND_H

#include <stdint.h>

struct cpu;

enum { SND_NONE, SND_AUDLO, SND_ENGINE, SND_MC };
extern int snd_device;                 /* which of the above this machine has (set by the hardware) */

/* ---- the output (snd.c) */
void snd_reset(uint32_t rate);         /* a new machine: the ring emptied, frames at this rate */
void snd_out(int16_t l, int16_t r);    /* one output frame */
int snd_pull(float *out, int n, int rate);   /* n interleaved stereo frames resampled to rate; 0 = nothing queued */
void snd_wav(const char *path);        /* --wav FILE */
void snd_finish(void);                 /* the WAV's header, at exit */

/* the cycle of output frame k at rate frames a second, at the simulated clock (dev_cpu_hz) */
uint64_t snd_frame_cycle(uint64_t k, uint32_t rate);

/* the machine's device: chosen at each reset from the hardware (SND_AUDLO on the low tier) */
void snd_machine(int device);
extern int hw_audio;                   /* the hardware profile's device (fsim --hw), SND_NONE without one */

/* the device: route register accesses, make frames up to now */
int snd_handles(uint32_t a);           /* the address is the device's (F00400 group, F01000 page) */
void snd_write(struct cpu *c, uint32_t a, uint16_t v);
uint16_t snd_read(struct cpu *c, uint32_t a);
void snd_sync(struct cpu *c);          /* frames up to c->st.cycles */
int snd_irq(void);                     /* the device's interrupt line (a level: source 3 stays pending while it is up) */
extern uint64_t snd_next;              /* cycle at which the device has its next frame or block due */
static inline void snd_tick(struct cpu *c, uint64_t cycles) { if (cycles >= snd_next) snd_sync(c); }

#define IRQ_AUDIO 0x0008u              /* interrupt source 3 (docs/io.md, F00700) */

/* ---- the low tier's player (audlo.c), F00400-F0043F */
void audlo_reset(void);
void audlo_write(struct cpu *c, uint32_t a, uint16_t v);
uint16_t audlo_read(struct cpu *c, uint32_t a);
void audlo_sync(struct cpu *c);

/* ---- the audio engine (audeng.c) */
void audeng_reset(void);
void audeng_write(struct cpu *c, uint32_t a, uint16_t v);
uint16_t audeng_read(struct cpu *c, uint32_t a);
void audeng_sync(struct cpu *c);
uint64_t audeng_next(void);
int audeng_irq(void);

/* ---- the microcoded audio engine (mcaud.c) */
void mcaud_reset(void);
void mcaud_write(struct cpu *c, uint32_t a, uint16_t v);   /* a: F01000-F0102F */
uint16_t mcaud_read(struct cpu *c, uint32_t a);
void mcaud_sync(struct cpu *c);        /* every block due by c->st.cycles: rendered, snd_out() x 32, IRQ */
uint64_t mcaud_next(void);             /* the cycle the next block is due */
int mcaud_irq(void);                   /* any IRQ bit pending */

#endif
