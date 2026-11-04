/* Interrupt test (docs/t1.md "Interrupts", docs/io.md "Interrupts"):
 * the vertical-blank source and the software one (IRQ_SET), the handler
 * entry (EPC, IE cleared), the flags kept by the handler, and RETI, and that nothing is taken
 * with IE clear. Prints its counts; fsim and the RTL must agree. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>

#ifndef FRAMES
#define FRAMES 10                        /* the RTL test uses 3 */
#endif

#define IRQ_PEND IO16(0xF00700)
#define IRQ_EN   IO16(0xF00702)
#define IRQ_SET  IO16(0xF00704)

static volatile uint16_t n_vbl, n_soft, last_epc_ok, flags_ok;

/* the C side: what interrupted us, cleared */
void irq_handler(void)
{
    uint16_t p = IRQ_PEND & IRQ_EN;
    if (p & 0x0001) n_vbl++;
    if (p & 0x80) n_soft++;
    IRQ_PEND = p;
}

/* the entry: save the flags (before anything changes them) and the pointer
 * pairs the C code may use, call, restore, RETI */
__asm__(
    "\t.text\n"
    "\t.globl irq_entry\n"
    "irq_entry:\n"
    "\tADDPI p7, p7, -32\n"
    "\tSTP p0, [p7 + 0]\n"
    "\tSTP p1, [p7 + 4]\n"
    "\tSTP p2, [p7 + 8]\n"
    "\tSTP p3, [p7 + 12]\n"
    "\tSTP p4, [p7 + 16]\n"
    "\tSTP p5, [p7 + 20]\n"
    "\tSTP p6, [p7 + 24]\n"
    "\tMFSR r0, sr0\n"
    "\tSTW r0, [p7 + 28]\n"
    "\tJAL irq_handler\n"
    "\tLDW r0, [p7 + 28]\n"
    "\tMTSR sr0, r0\n"
    "\tLDP p0, [p7 + 0]\n"
    "\tLDP p1, [p7 + 4]\n"
    "\tLDP p2, [p7 + 8]\n"
    "\tLDP p3, [p7 + 12]\n"
    "\tLDP p4, [p7 + 16]\n"
    "\tLDP p5, [p7 + 20]\n"
    "\tLDP p6, [p7 + 24]\n"
    "\tADDPI p7, p7, 32\n"
    "\tRETI\n");
extern char irq_entry[];

/* a long branch at the vector, 0x000010, to the entry: B is 0xF700 |
 * offset[22:16], then offset[15:0], offset = (target - 0x10) / 2 */
static void irq_install(void)
{
    uint32_t off = ((uint32_t)(uintptr_t)irq_entry - 0x10u) >> 1;
    volatile uint16_t *v = (volatile uint16_t *)0x10;
    v[0] = (uint16_t)(0xF700 | ((off >> 16) & 0x7F));
    v[1] = (uint16_t)off;
}

static void ie(int on)
{
    uint16_t v = (uint16_t)on;
    __asm__ volatile("MTSR sr6, %0" : : "r"(v));
}

int main(void)
{
    irq_install();
    IRQ_PEND = 0xFF;
    IRQ_EN = 0x81;                   /* vertical blank, software (bit 7) */

    /* IE clear: a pending software interrupt is not taken */
    IRQ_SET = 0x80;
    for (volatile int i = 0; i < 100; i++) ;
    int held = n_soft == 0 && (IRQ_PEND & 0x80);

    /* IE set: taken at once */
    ie(1);
    int at_once = n_soft == 1;

    /* the flags survive an interrupt: a compare, a software interrupt, a branch on it */
    volatile uint16_t a = 5, b = 7;
    int lt;
    __asm__ volatile("" ::: "memory");
    lt = a < b;
    IRQ_SET = 0x80;
    flags_ok = lt && n_soft == 2;

    /* FRAMES frames of vertical blanks */
    uint16_t f0 = FRAME;
    while ((uint16_t)(FRAME - f0) < FRAMES) ;
    ie(0);
    uint16_t v = n_vbl;
    printf("held %d, at once %d, flags %d, soft %u, vblanks %u in %d frames\n",
           held, at_once, flags_ok, n_soft, v, FRAMES);
    return (held && at_once && flags_ok && n_soft == 2 && v >= FRAMES - 1 && v <= FRAMES + 1) ? 0 : 1;
}
