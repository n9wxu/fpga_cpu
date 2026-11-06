/* a stand-in for viz.c: the bar, the section and the load on the text layer */
#include <stdio.h>
#include <fcpu.h>
#include "viz.h"
void viz_init(const viz_cfg_t *c) { (void)c; }
void viz_frame(const viz_t *v)
{
    static uint16_t bar;
    while (VSTAT & 1) ;
    while (!(VSTAT & 1)) ;
    if (v->bar != bar) { bar = v->bar; printf("bar %2u  load %2u%%  underruns %u  %s\n", v->bar, (unsigned)((uint32_t)v->blocks_busy * 100 / 45000), v->underruns, v->section); }
}
