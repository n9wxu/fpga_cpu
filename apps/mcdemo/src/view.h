/* The microcoded-audio demo's screen: a live dataflow diagram of the engine (issue #23), from the
 * node list in SDRAM to the speaker. 640x360 8 bpp bitmap (two buffers, 0x200000 and 0x240000)
 * under the text layer (0x0FC000) for the labels.
 *
 *   view_init();                  once: video mode, palette, the static diagram in both buffers
 *   for (;;) { fill a mon_t; view_frame(&m); }
 *
 * view_frame waits for the previous flip (at most a frame), draws the moving parts into the hidden
 * buffer and asks for it to be shown at the next frame; it returns at once, so the caller's own
 * work overlaps the wait for the flip. Animation runs on the video frame counter, so its speed
 * does not depend on how often view_frame is called. */
#ifndef MCDEMO_VIEW_H
#define MCDEMO_VIEW_H
#include <stdint.h>

enum { VK_PULSE, VK_SAW, VK_TRI, VK_NOISE, VK_SAMPLE, VK_SYNC, VK_RING, VK_FILTER, VK__N };
enum { VF_SAMPLES = 1, VF_FILTER = 2, VF_TICK = 4, VF_GROUP = 8 };   /* paths in use this scene */

typedef struct {
    uint16_t blocks, underruns;       /* engine counters */
    uint16_t clocks;                  /* clocks the last 32-sample block took, of 45,000 available */
    uint16_t bursts, words;           /* SDRAM traffic in the last block: bursts, 32-bit words */
    uint16_t nnodes;                  /* nodes in the list (<= 32) */
    int16_t  scope_l[256], scope_r[256];   /* the last 256 output samples, oldest first */
    uint16_t meter[32];               /* per node, list order: its level 0..0x7FFF */
    uint8_t  kind[32];                /* per node: VK_* */
    uint8_t  group[32];               /* per node: 1 if linked to the next (lockstep group) */
    const char *scene;                /* scene title, e.g. "3. Ring modulation and hard sync" */
    const char *notes;                /* what to listen for, up to 3 lines separated by '\n' */
    uint16_t flags;                   /* VF_* */
    uint16_t scene_no, scene_count;   /* for a progress indicator */
} mon_t;

void view_init(void);
void view_frame(const mon_t *m);
#endif
