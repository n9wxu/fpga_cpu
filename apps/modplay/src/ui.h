#ifndef UI_H
#define UI_H
#include "mod.h"

typedef struct {
    uint8_t load;               /* CPU busy, percent of a tick */
    uint8_t paused;
    uint32_t play_s;            /* song time, seconds */
} ui_stat_t;

void ui_init(void);                             /* video registers, palette, the panels */
void ui_song(uint8_t song, uint8_t songs);      /* the static texts for a new MOD */
void ui_message(const char *s, uint8_t fg);     /* row 2 */
void ui_keys(void);                             /* row 2: the keys */
void ui_frame(const ui_stat_t *s);              /* everything that moves */

#endif
