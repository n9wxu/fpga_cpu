/* The microcoded audio engine from C (docs/mcaudio.md): its registers, a pool of nodes in the
 * SDRAM, and the monitor. Node words are written only from the block-done interrupt (mc_on_block),
 * while the engine waits for room in its FIFO: a 32-bit store reaches the SDRAM a byte at a time,
 * and the engine must not fetch a node half written. */
#ifndef MCDEMO_MC_H
#define MCDEMO_MC_H
#include <stdint.h>
#include "mc_cs.h"

#define MC_REG(o)    (*(volatile uint16_t *)(0xF01000UL + (o)))
#define MC_HEAD_LO   MC_REG(0x00)
#define MC_HEAD_HI   MC_REG(0x02)
#define MC_CTRL      MC_REG(0x04)
#define MC_TEMPO     MC_REG(0x06)
#define MC_CS_ADDR   MC_REG(0x08)
#define MC_CS(i)     MC_REG(0x0A + 2 * (i))
#define MC_IRQ       MC_REG(0x14)
#define MC_ID        MC_REG(0x16)
#define MC_BLOCKS    MC_REG(0x18)
#define MC_UNDER     MC_REG(0x1A)
#define MC_CLOCKS    MC_REG(0x1C)
#define MC_BURSTS    MC_REG(0x1E)
#define MC_WORDS     MC_REG(0x20)
#define MC_NODES     MC_REG(0x22)
#define MC_MON_IDX   MC_REG(0x24)           /* write: the index; read: the output ring's position */
#define MC_MON_LO    MC_REG(0x26)
#define MC_MON_HI    MC_REG(0x28)

#define MC_NODE_BASE 0x300000UL             /* the node pool: 32 nodes of 64 bytes */
#define MC_NNODES    32
#define MC_NODE(i)   ((volatile uint32_t *)(MC_NODE_BASE + 64UL * (uint32_t)(i)))
#define MC_WADDR(p)  ((uint32_t)(p) >> 2)   /* the engine's addresses are SDRAM word addresses */

/* routines (mc_cs.h): MC_OSC_SAW, MC_OSC_TRI, MC_OSC_PULSE, MC_OSC_NOISE, MC_SMP, MC_SYNC, MC_FLT */

/* the list: the nodes in this order, each linked to the next one or not (a lockstep group) */
void mc_init(void);                                         /* control store, an empty list, run */
void mc_list(const uint8_t *nodes, const uint8_t *linked, uint16_t n);   /* (from the interrupt) */
uint32_t mc_step(int16_t midi);                             /* an oscillator's step for a note */
uint32_t mc_ratio(int16_t semis);                           /* 2^(semis/12), 16.16 */
#endif
