/* Interrupts (docs/t1.md "Interrupts", docs/io.md "Interrupts"): an entry
 * at the vector that saves the flags and the registers C may change, and a
 * handler for each source of the controller at F00700.
 *
 *   irq_on(IRQ_PS2A, my_handler);  // installs the vector, enables the source
 *   irq_enable(1);                 // IE (MTSR sr6)
 *
 * A handler runs with IE clear and must leave its source cleared (write its
 * bit to IRQ_PEND) before it returns, or it runs again at once. */
#ifndef FPGA_IRQ_H
#define FPGA_IRQ_H
#include <stdint.h>
#include <fcpu.h>

#define IRQ_PEND IO16(0xF00700)
#define IRQ_EN   IO16(0xF00702)
#define IRQ_SET  IO16(0xF00704)

enum { IRQ_VBLANK = 0, IRQ_PS2A = 1, IRQ_PS2B = 2, IRQ_AUDIO = 3, IRQ_SOFT = 7 };   /* AUDIO: the Tang's engine */

void irq_on(int src, void (*handler)(void));
void irq_off(int src);

static inline void irq_enable(int on)
{
    uint16_t v = (uint16_t)on;
    __asm__ volatile("MTSR sr6, %0" : : "r"(v) : "memory");
}
#endif
