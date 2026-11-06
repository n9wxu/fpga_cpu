/* fpga_cpu simulator: sound out (snd.h). The machine's sound device makes its output frames up to
 * the CPU's cycle count and hands them to snd_out(); they go to a ring that the GUI's and the web
 * page's audio callbacks drain at their own rate (snd_pull), and to the --wav file.
 *
 * The emulation runs in slices (a display frame of time each, real time by default), so frames
 * arrive in bursts: snd_pull keeps a little queued before it starts playing, and when the
 * emulation runs ahead (max speed) it drops what is too old to play. */
#include "cpu.h"
#include "snd.h"

#include <string.h>

int snd_device;
uint64_t snd_next = UINT64_MAX;

#define RING 65536u                        /* frames, a power of 2 (1.4 s at 48 kHz) */
static int16_t ring[RING][2];
static uint32_t wr, rd;                    /* frame counts; one writer (the emulation), one reader (audio) */
static uint32_t src_rate = 48000;
static double frac;                        /* the reader's position between ring[rd] and ring[rd + 1] */
static int playing;                        /* 0: waiting for the queue to fill */

static FILE *wav;
static uint32_t wav_frames, wav_rate;

uint64_t snd_frame_cycle(uint64_t k, uint32_t rate)
{
    return k * dev_cpu_hz / rate;                            /* no overflow below 10^11 frames */
}

void snd_reset(uint32_t rate)
{
    __atomic_store_n(&rd, __atomic_load_n(&wr, __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    src_rate = rate;
    frac = 0;
    playing = 0;
    if (wav && !wav_frames)
        wav_rate = rate;
}

static void wav_header(void)
{
    uint32_t bytes = wav_frames * 4;
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                      16, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0,
                      'd', 'a', 't', 'a', 0, 0, 0, 0 };
    uint32_t v[3] = { 36 + bytes, wav_rate, wav_rate * 4 };
    memcpy(h + 4, &v[0], 4);
    memcpy(h + 24, &v[1], 4);
    memcpy(h + 28, &v[2], 4);
    memcpy(h + 40, &bytes, 4);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fseek(wav, 0, SEEK_END);
}

void snd_wav(const char *path)
{
    wav = fopen(path, "wb");
    if (!wav) {
        perror(path);
        return;
    }
    wav_rate = src_rate;
    wav_header();
}

void snd_finish(void)
{
    if (wav) {
        wav_header();
        fclose(wav);
        wav = NULL;
    }
}

void snd_out(int16_t l, int16_t r)
{
    uint32_t w = __atomic_load_n(&wr, __ATOMIC_RELAXED);
    ring[w & (RING - 1)][0] = l;
    ring[w & (RING - 1)][1] = r;
    __atomic_store_n(&wr, w + 1, __ATOMIC_RELEASE);
    if (wav) {
        int16_t s[2] = { l, r };
        fwrite(s, 2, 2, wav);
        wav_frames++;
    }
}

int snd_pull(float *out, int n, int rate)
{
    uint32_t w = __atomic_load_n(&wr, __ATOMIC_ACQUIRE);
    uint32_t avail = w - rd;
    uint32_t lo = src_rate / 20, hi = src_rate / 4;          /* queue 50 ms before playing; at most 250 ms */
    if (avail > RING - 4096 || avail > hi + lo) {           /* far behind (max speed, a stall): catch up */
        rd = w - lo;
        avail = lo;
    }
    if (!playing && avail < lo) {
        memset(out, 0, sizeof(float) * 2 * (size_t)n);
        return avail != 0;
    }
    playing = 1;
    double step = (double)src_rate / rate;
    for (int i = 0; i < n; i++) {
        if (w - rd < 2) {                                    /* ran dry: silence until the queue refills */
            playing = 0;
            memset(out + 2 * i, 0, sizeof(float) * 2 * (size_t)(n - i));
            break;
        }
        const int16_t *a = ring[rd & (RING - 1)], *b = ring[(rd + 1) & (RING - 1)];
        out[2 * i] = (float)((a[0] + (b[0] - a[0]) * frac) / 32768.0);
        out[2 * i + 1] = (float)((a[1] + (b[1] - a[1]) * frac) / 32768.0);
        frac += step;
        while (frac >= 1.0) {
            frac -= 1.0;
            rd++;
        }
    }
    return 1;
}

/* ---- the device */

int hw_audio = SND_NONE;

void snd_machine(int device)
{
    snd_device = device;
    switch (device) {
    case SND_AUDLO: audlo_reset(); snd_reset(48828); break;
    case SND_ENGINE: audeng_reset(); snd_reset(48000); break;
    case SND_MC: mcaud_reset(); snd_reset(48000); break;
    default: snd_reset(48000);
    }
    snd_next = device == SND_NONE ? UINT64_MAX : 0;
}

int snd_handles(uint32_t a)
{
    switch (snd_device) {
    case SND_AUDLO: return a >= 0xF00400u && a < 0xF00440u;
    case SND_ENGINE: return (a >= 0xF00400u && a < 0xF00418u) || (a >= 0xF01000u && a < 0xF02000u);
    case SND_MC: return a >= 0xF01000u && a < 0xF01030u;
    default: return 0;
    }
}

void snd_write(cpu_t *c, uint32_t a, uint16_t v)
{
    switch (snd_device) {
    case SND_AUDLO: audlo_write(c, a, v); break;
    case SND_ENGINE: audeng_write(c, a, v); break;
    case SND_MC: mcaud_write(c, a, v); snd_next = mcaud_next(); break;   /* CTRL starts or stops the blocks */
    }
}

int snd_irq(void)
{
    switch (snd_device) {
    case SND_ENGINE: return audeng_irq();
    case SND_MC: return mcaud_irq();
    default: return 0;
    }
}

uint16_t snd_read(cpu_t *c, uint32_t a)
{
    switch (snd_device) {
    case SND_AUDLO: return audlo_read(c, a);
    case SND_ENGINE: return audeng_read(c, a);
    case SND_MC: return mcaud_read(c, a);
    }
    return 0;
}

void snd_sync(cpu_t *c)
{
    switch (snd_device) {
    case SND_AUDLO: audlo_sync(c); break;
    case SND_ENGINE: audeng_sync(c); break;
    case SND_MC: mcaud_sync(c); break;
    default: snd_next = UINT64_MAX;
    }
}
