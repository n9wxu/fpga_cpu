/* viewtest: the demo's screen (view.c) animated with made-up engine data: scenes that add nodes,
 * moving meters, a sine (L) and a saw (R) in the scope, the flags changing. Prints the CPU cycles
 * per view_frame every 64 frames. -DFRAMES=n stops after n frames (fsim then writes --screenshot).
 *   fcc -O2 viewtest.c view.c -o viewtest.bin
 *   fsim --xram 7 --true-color --screenshot shot.ppm viewtest.bin */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "view.h"

static int16_t sn[256];
static mon_t m;

static const struct { const char *title, *notes; uint16_t n, flags; uint8_t kinds[8], grp; } sc[] = {
    { "1. One pulse voice", "A square wave whose width the script sweeps (PWM).\n"
      "One node, one kernel: 6 micro-words a sample.", 1, VF_TICK, { VK_PULSE }, 0 },
    { "2. Chip waves: saw, triangle, noise", "Three oscillators, each its own node.\n"
      "Listen for the bright saw, the soft triangle, the hiss of noise.", 4, VF_TICK,
      { VK_PULSE, VK_SAW, VK_TRI, VK_NOISE }, 0 },
    { "3. Ring modulation and hard sync", "Linked nodes run in lockstep: the bracket marks a group.\n"
      "Ring: bell tones. Sync: the tearing sweep of a slave oscillator.\n"
      "Each group is fetched in consecutive bursts, then run sample by sample.",
      6, VF_TICK | VF_GROUP, { VK_TRI, VK_RING, VK_SAW, VK_SYNC, VK_PULSE, VK_PULSE }, 0x0B },
    { "4. Samples from SDRAM", "8-bit drums and a looped instrument: the DMA brings each block's frames.\n"
      "The sample buffer feeds the datapath; the write-back keeps the position.",
      8, VF_TICK | VF_SAMPLES, { VK_SAMPLE, VK_SAMPLE, VK_SAMPLE, VK_SAMPLE, VK_PULSE, VK_SAW, VK_TRI, VK_SAMPLE }, 0 },
    { "5. The filter sweep", "Voices mixed into the filter bus go through a resonant SVF node.\n"
      "Listen to the cutoff rise and fall.", 6, VF_TICK | VF_FILTER,
      { VK_SAW, VK_SAW, VK_SAW, VK_PULSE, VK_FILTER, VK_TRI }, 0 },
    { "6. Everything: 32 nodes", "All of it at once: the engine's busy bar shows the margin left.\n"
      "32 nodes, samples, groups and the filter in 45,000 clocks a block.",
      32, VF_TICK | VF_SAMPLES | VF_FILTER | VF_GROUP, { VK_SAMPLE, VK_PULSE, VK_SAW, VK_TRI, VK_RING, VK_SYNC, VK_NOISE, VK_FILTER }, 0x30 },
};
#define NSC (sizeof sc / sizeof sc[0])

int main(void)
{
    /* a sine table: the Minsky circle */
    int32_t s = 0, c = 32000;
    for (uint16_t i = 0; i < 256; i++) {
        sn[i] = (int16_t)s;
        s += (c * 804) >> 15;
        c -= (s * 804) >> 15;
    }
    view_init();
    uint32_t tot = 0, ins = 0;
    uint16_t blocks = 0;
    for (uint32_t fr = 0;; fr++) {
#ifdef SCENE
        uint16_t k = SCENE;
#else
        uint16_t k = (uint16_t)((fr / 48) % NSC);
#endif
        m.scene = sc[k].title; m.notes = sc[k].notes; m.flags = sc[k].flags;
        m.scene_no = (uint16_t)(k + 1); m.scene_count = NSC;
        uint16_t n = sc[k].n;
        m.nnodes = n;
        for (uint16_t i = 0; i < 32; i++) {
            m.kind[i] = sc[k].kinds[i & 7];
            m.group[i] = (uint8_t)((sc[k].grp >> (i & 7)) & 1);
            m.meter[i] = (uint16_t)(sn[(uint8_t)(fr * 5 + i * 37)] / 2 + 0x4000);
        }
        blocks = (uint16_t)(blocks + 25);
        m.blocks = blocks; m.underruns = k == 5 ? 2 : 0;
        m.clocks = (uint16_t)(1500 + n * 1150 + (sn[(uint8_t)(fr * 3)] >> 6));
        m.bursts = (uint16_t)(n + ((m.flags & VF_SAMPLES) ? n : 0));
        m.words = (uint16_t)(n * 20 + ((m.flags & VF_SAMPLES) ? n * 12 : 0));
        for (uint16_t i = 0; i < 256; i++) {
            m.scope_l[i] = (int16_t)(sn[(uint8_t)(i * (k + 2) + fr * 4)] >> (k == 0 ? 1 : 0));
            m.scope_r[i] = (int16_t)((int16_t)((i * (k + 3) * 64 + fr * 512) & 0xFFFF) / 3);
        }
        while (*(volatile uint16_t *)0xF00222 & 1) ;   /* the flip: not counted */
        uint32_t i0 = fcpu_instret(), t0 = CYCLE;
        view_frame(&m);
        uint32_t t1 = CYCLE, i1 = fcpu_instret();
        tot += t1 - t0; ins += i1 - i0;
        if ((fr & 63) == 63) { printf("frames %lu: %lu cycles, %lu instructions a view_frame\n", fr + 1, tot / 64, ins / 64); tot = ins = 0; }
#ifdef FRAMES
        if (fr + 1 == FRAMES) return 0;
#endif
    }
}
