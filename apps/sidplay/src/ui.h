#ifndef UI_H
#define UI_H
#include "sidplay.h"
#include "model.h"

typedef struct {
    uint8_t load;               /* CPU load of the last frames, percent */
    uint8_t load_max;
    uint32_t play_ms;           /* song time */
    uint8_t paused;
    uint8_t song, songs, tune, tunes;
    uint8_t mute;
    uint8_t spread;
    uint8_t has_engine;
    uint16_t late;              /* frames that missed their time */
} ui_stat_t;

void ui_init(void);                                         /* video registers, palette, static background */
void ui_tune(const psid_t *p, const ui_stat_t *s);          /* header texts for a new tune/song; clears the pattern view */
void ui_message(const char *s, uint8_t fg);                 /* line 3 (row 2) */
void ui_frame(const frame_t *pf, uint32_t play_frame, const ui_stat_t *s);
void ui_clear_dynamic(void);                                /* forget scope/VU state */

#endif
