/* A sustained three-voice chord through the audio engine, forever, over a
 * static screen: the engine and HDMI audio busy, the CPU and SDRAM idle
 * (a test for the HDMI audio dropouts). */
#include <stdint.h>
#include <fcpu.h>

#define V(v, w)   IO16(0xF01800 + 64 * (v) + 2 * (w))
enum { CTRL, STEP_LO, STEP_HI, VOL_L, VOL_R, DUTY, ATTACK, DECAY, SUSTAIN, RELEASE };

static void chip(int v, int src, uint32_t hz, int vl, int vr)
{
    uint32_t step = (uint32_t)(((uint64_t)hz << 24) / 48000);
    V(v, STEP_LO) = (uint16_t)step; V(v, STEP_HI) = (uint16_t)(step >> 16);
    V(v, VOL_L) = vl; V(v, VOL_R) = vr; V(v, DUTY) = 0x400;
    V(v, ATTACK) = 0x0800; V(v, DECAY) = 0x0040; V(v, SUSTAIN) = 0x8000; V(v, RELEASE) = 0x0080;
    V(v, CTRL) = (src << 2) | 3;
}

int main(void)
{
    IO16(0xF01000) = 0x1000;
    chip(0, 1, 220, 0x60, 0x60);
    chip(1, 2, 277, 0x60, 0x20);
    chip(2, 3, 330, 0x20, 0x60);
#ifdef ARP
    /* 50 times a second: new pitches and a gate retrigger on every voice */
    {
        static const uint16_t hz[8] = { 220, 262, 330, 392, 440, 523, 659, 784 };
        uint32_t t = CYCLE;
        unsigned k = 0;
        for (;;) {
            while ((int32_t)(CYCLE - t) < 0) ;
            t += 16875000 / 50;             /* CYCLE: T1 periods, 16.875 MHz */
            for (int v = 0; v < 3; v++) {
                V(v, CTRL) = V(v, CTRL) & ~1;
                chip(v, 1 + v, hz[(k + 2 * v) & 7], 0x50, 0x50);
            }
            k++;
        }
    }
#elif defined(WSPAM)
    /* SDRAM writes as fast as the CPU goes (words; a buffer nobody shows) */
    for (uint32_t n = 0;; n++) {
        volatile uint32_t *p = (volatile uint32_t *)0x040000;
        for (int i = 0; i < 16384; i++) p[i] = n ^ (uint32_t)i * 0x9E3779B9u;
    }
#elif defined(RSPAM)
    /* SDRAM reads, missing the cache: a 256 KB stride walk */
    for (uint32_t s = 0;;) {
        volatile uint32_t *p = (volatile uint32_t *)0x040000;
        for (int i = 0; i < 65536; i += 4) s += p[i];
        IO16(0xF00604) = (uint16_t)s;       /* (keep it) */
    }
#else
    for (;;) ;
#endif
}
