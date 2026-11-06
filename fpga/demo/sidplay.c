/* SID player for the Spartan-3: a C64 tune's register frames (made from a
 * .sid file by fpga/tools/sid2regs.py, which runs the tune's 6502 code on
 * the host) played on the software SID (fpga/lib/sid.c) through the sound's
 * streaming player. The screen shows the tune, the time, each voice's
 * envelope and the CPU time the synthesizer takes.
 *
 *   python3 fpga/tools/sid2regs.py TUNE.sid build/tune_regs.c --name tune
 *   llvm/fcc -O2 --stack 0x20000 -o build/sidplay.bin fpga/demo/sidplay.c \
 *       fpga/lib/sid.c build/tune_regs.c
 *
 * Built with -DSECONDS=n it stops after n seconds (for fsim --wav and the
 * RTL), exit code 0. Statistics at 0x05FF00 for a JTAG dump: samples made,
 * the synthesizer's share of the CPU in %, the frames played, underruns. */
#include <stdint.h>
#include <fcpu.h>
#include "../lib/sid.h"

extern const char tune_title[], tune_author[], tune_released[];
extern const uint32_t tune_frames;
extern const uint16_t tune_hz100;
extern const uint8_t tune[];

#define RING     0x070000UL
#define RING_LEN 2048u                       /* 168 ms at 12.2 kHz */
#define TEXT     ((volatile uint16_t *)0x0FC000)
#define STATS    ((volatile uint32_t *)0x05FF00)
#define T16      IO16(0xF00004)              /* CYCLE: 10 MHz */
enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE };
#define ATTR(fg, bg) ((uint16_t)((bg) << 3 | (fg)))

static void tput(int row, int col, const char *s, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s && col++ < 80)
        *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

static void tnum(int row, int col, uint32_t v, int digits, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col + digits;
    while (digits--) {
        *--t = (uint16_t)(attr << 8 | ('0' + v % 10));
        v /= 10;
    }
}

static void bar(int row, int col, uint8_t level, uint16_t attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    int n = level >> 3;                      /* 0-31 cells */
    for (int i = 0; i < 32; i++)
        *t++ = (uint16_t)(attr << 8 | (i < n ? '#' : '.'));
}

static const uint8_t *fp;                    /* the next frame */
static uint32_t frame;

static void next_frame(void)
{
    if (frame == tune_frames) {              /* the end: from the start again */
        frame = 0;
        fp = tune;
        for (uint8_t r = 0; r < 25; r++) sid_write(r, 0);
    }
    uint32_t mask = (uint32_t)fp[0] | (uint32_t)fp[1] << 8 | (uint32_t)fp[2] << 16 | (uint32_t)fp[3] << 24;
    fp += 4;
    for (uint8_t r = 0; r < 25; r++, mask >>= 1)
        if (mask & 1) sid_write(r, *fp++);
    frame++;
}

int main(void)
{
    for (int i = 0; i < 80 * 25; i++) TEXT[i] = (uint16_t)(ATTR(WHITE, BLACK) << 8 | ' ');
    tput(1, 2, "SID player: a 3-voice SID in software, on the Spartan-3's sound stream", ATTR(CYAN, BLACK));
    tput(3, 2, "Title", ATTR(WHITE, BLACK));    tput(3, 12, tune_title, ATTR(YELLOW, BLACK));
    tput(4, 2, "Author", ATTR(WHITE, BLACK));   tput(4, 12, tune_author, ATTR(YELLOW, BLACK));
    tput(5, 2, "Released", ATTR(WHITE, BLACK)); tput(5, 12, tune_released, ATTR(YELLOW, BLACK));
    tput(7, 2, "Time", ATTR(WHITE, BLACK));     tput(7, 14, ":", ATTR(YELLOW, BLACK));
    tput(9, 2, "Voice 1", ATTR(WHITE, BLACK));
    tput(10, 2, "Voice 2", ATTR(WHITE, BLACK));
    tput(11, 2, "Voice 3", ATTR(WHITE, BLACK));
    tput(13, 2, "Synthesizer CPU      %", ATTR(WHITE, BLACK));
    tput(14, 2, "Underruns", ATTR(WHITE, BLACK));
    tput(13, 26, "(12,207 samples/s, 3 voices, no filter)", ATTR(BLUE, BLACK));

    fp = tune;
    frame = 0;
    sid_init(RING, RING_LEN);
    /* samples between play calls, 16.16 */
    uint32_t spf = ((uint32_t)SID_RATE << 16) / tune_hz100 * 100u;
    uint32_t until = 0;                      /* 16.16: samples to the next frame */
    uint32_t samples = 0, busy = 0, total = 0, underruns = 0;
    uint16_t shown = 0xFFFF, f0 = FRAME;
    uint16_t t_mark = T16;
    for (;;) {
        uint16_t room = sid_room();
        if (room > RING_LEN - 64) underruns++;   /* the ring ran (nearly) dry */
        if (room < 128) room = 0;            /* render in blocks: each call has its set-up */
        uint16_t t0 = T16;
        while (room) {
            if (until < 0x10000) { next_frame(); until += spf; }
            uint16_t k = (uint16_t)(until >> 16);
            if (k > room) k = room;
            if (k > 256) k = 256;
            sid_render(k);
            room -= k;
            samples += k;
            until -= (uint32_t)k << 16;
        }
        uint16_t t1 = T16;
        busy += (uint16_t)(t1 - t0);
        total += (uint16_t)(t1 - t_mark);
        t_mark = t1;

        if (FRAME != shown) {                /* the display, once a video frame */
            shown = FRAME;
            uint32_t sec = samples / SID_RATE;
            tnum(7, 12, sec / 60, 2, ATTR(YELLOW, BLACK));
            tnum(7, 15, sec % 60, 2, ATTR(YELLOW, BLACK));
            for (int v = 0; v < 3; v++) bar(9 + v, 12, sid_env[v], ATTR(GREEN, BLACK));
            if (total > 100000) {             /* every ~10 ms of CYCLE */
                uint16_t pc = (uint16_t)(busy * 100 / total);
                tnum(13, 18, pc, 3, ATTR(YELLOW, BLACK));
                STATS[1] = pc;
                busy = total = 0;
            }
            tnum(14, 18, underruns, 5, ATTR(YELLOW, BLACK));
            STATS[0] = samples;
            STATS[2] = frame;
            STATS[3] = underruns;
        }
#ifdef SECONDS
        if (samples >= (uint32_t)SECONDS * SID_RATE) {
            sid_stop();
            return (int)(underruns > 1);    /* (the first fill counts as one) */
        }
#endif
        (void)f0;
    }
}
