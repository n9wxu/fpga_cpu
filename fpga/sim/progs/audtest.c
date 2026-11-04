/* Sound test for the low tier (fpga/rtl/audio_lite.v): the three voices
 * in every waveform, the streaming player (with STR_VOL above 1.0 and a
 * rate divider), and changes made while it plays. Its output, from the
 * RTL (system_tb +i2s=FILE) and from fsim (--no-hicolor --wav FILE), must
 * be the same, sample for sample (fpga/tools/i2scmp.py).
 *
 * Register writes are made just after the stream's position moves (a new
 * output frame has started), so they land in the same output frame in the
 * RTL and in fsim, whatever their speeds. */
#include <stdint.h>
#include <fcpu.h>

#define R(a)      IO16(0xF00000 + (a))
#define FREQ(v)   R(0x400 + 8 * (v))
#define VOL(v)    R(0x402 + 8 * (v))
#define WAVE(v)   R(0x404 + 8 * (v))
#define STR_CTRL  R(0x420)
#define STR_VOL   R(0x422)
#define STR_ADDR_LO R(0x424)
#define STR_ADDR_HI R(0x426)
#define STR_LEN   R(0x428)
#define STR_POS   R(0x42A)

#define BUF ((volatile uint32_t *)0x030000)
#define LEN 200

static const int8_t sintab[16] = { 0, 49, 90, 117, 127, 117, 90, 49, 0, -49, -90, -117, -127, -117, -90, -49 };

/* wait until the stream moves on n times */
static void frames(int n)
{
    uint16_t p = STR_POS;
    while (n > 0) {
        uint16_t q = STR_POS;
        if (q != p) { p = q; n--; }
    }
}

int main(void)
{
    for (int i = 0; i < LEN; i++) {
        int16_t l = (int16_t)(sintab[i & 15] * 250);              /* a tone */
        int16_t r = (int16_t)((i * 331) - 32000);                  /* a ramp */
        BUF[i] = (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)r << 16;
    }
    STR_ADDR_LO = 0x0000;
    STR_ADDR_HI = 0x0003;
    STR_LEN = LEN;
    STR_VOL = 0x180;
    STR_CTRL = 1;                     /* play, every frame */
    while (STR_POS == 0) ;            /* the stream has started: the frames line up from here */

    FREQ(0) = 440;  WAVE(0) = 0; VOL(0) = 15;
    FREQ(1) = 660;  WAVE(1) = 1; VOL(1) = 12;
    FREQ(2) = 3000; WAVE(2) = 3; VOL(2) = 6;
    frames(600);
    FREQ(0) = 1234; WAVE(0) = 2;      /* saw */
    WAVE(1) = 0; VOL(1) = 4;
    STR_CTRL = 1 | (2 << 1);          /* every third frame */
    frames(500);
    STR_VOL = 0x1FF;
    FREQ(2) = 200;  WAVE(2) = 1;
    VOL(0) = 0;
    frames(400);
    STR_CTRL = 0;                     /* stop the stream */
    FREQ(1) = 0;
    for (volatile int i = 0; i < 2000; i++) ;   /* a tail (its length differs: compare what both have) */
    return 0;
}
