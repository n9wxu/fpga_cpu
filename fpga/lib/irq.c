/* Interrupts: the entry at the vector (0x000010) and the dispatch to each
 * source's handler (irq.h). */
#include "irq.h"

static void (*handlers[8])(void);

/* every pending, enabled source's handler, lowest bit first */
void irq_dispatch(void)
{
    uint16_t p = IRQ_PEND & IRQ_EN;
    for (int i = 0; i < 8; i++)
        if ((p >> i) & 1) {
            if (handlers[i])
                handlers[i]();
            else
                IRQ_PEND = (uint16_t)(1u << i);
        }
}

/* the entry: the flags first (MFSR, before anything changes them), the
 * pointer pairs C may use, the dispatch, back with RETI */
__asm__(
    "\t.text\n"
    "\t.globl irq_entry\n"
    "irq_entry:\n"
    "\tADDPI p7, p7, -32\n"
    "\tSTP p0, [p7 + 0]\n"
    "\tMFSR r0, sr0\n"
    "\tSTW r0, [p7 + 28]\n"
    "\tSTP p1, [p7 + 4]\n"
    "\tSTP p2, [p7 + 8]\n"
    "\tSTP p3, [p7 + 12]\n"
    "\tSTP p4, [p7 + 16]\n"
    "\tSTP p5, [p7 + 20]\n"
    "\tSTP p6, [p7 + 24]\n"
    "\tJAL irq_dispatch\n"
    "\tLDP p1, [p7 + 4]\n"
    "\tLDP p2, [p7 + 8]\n"
    "\tLDP p3, [p7 + 12]\n"
    "\tLDP p4, [p7 + 16]\n"
    "\tLDP p5, [p7 + 20]\n"
    "\tLDP p6, [p7 + 24]\n"
    "\tLDW r0, [p7 + 28]\n"
    "\tMTSR sr0, r0\n"
    "\tLDP p0, [p7 + 0]\n"
    "\tADDPI p7, p7, 32\n"
    "\tRETI\n");
extern char irq_entry[];

/* a long branch at the vector to the entry: 0xF700 | offset[22:16], then
 * offset[15:0], offset = (target - 0x10) / 2 */
static void install(void)
{
    static uint8_t done;
    if (done)
        return;
    uint32_t off = ((uint32_t)(uintptr_t)irq_entry - 0x10u) >> 1;
    volatile uint16_t *v = (volatile uint16_t *)0x10;
    v[0] = (uint16_t)(0xF700 | ((off >> 16) & 0x7F));
    v[1] = (uint16_t)off;
    done = 1;
}

void irq_on(int src, void (*handler)(void))
{
    install();
    handlers[src] = handler;
    IRQ_PEND = (uint16_t)(1u << src);
    IRQ_EN = (uint16_t)(IRQ_EN | (1u << src));
}

void irq_off(int src)
{
    IRQ_EN = (uint16_t)(IRQ_EN & ~(1u << src));
    IRQ_PEND = (uint16_t)(1u << src);
}
