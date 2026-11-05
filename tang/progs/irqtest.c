/* Interrupts on the Tang (T1F with IRQ, iosys's controller at F00700): a vertical-blank handler counts
 * frames while the main loop does 32-bit sums whose carries the handler must not disturb, then a
 * software interrupt (IRQ_SET), and nothing taken with IE clear. Results on the serial console; the
 * exit code is the number of failed checks. Build: fcc ... tang/progs/irqtest.c fpga/lib/irq.c */
#include <stdint.h>
#include <fcpu.h>
#include "../../fpga/lib/irq.h"

#define FRAME IO16(0xF0000A)

static volatile uint16_t n_vbl, n_soft;
static void on_vbl(void) { n_vbl++; IRQ_PEND = 1u << IRQ_VBLANK; }
static void on_soft(void) { n_soft++; IRQ_PEND = 1u << IRQ_SOFT; }

static void con(const char *s) { while (*s) CON_OUT = (uint8_t)*s++; }
static void num(uint32_t v)
{
    char b[11];
    int i = 10;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    con(b + i);
}
static int fails;
static void check(const char *what, int ok) { con(what); con(ok ? ": ok\n" : ": FAILED\n"); if (!ok) fails++; }

int main(void)
{
    uint16_t f0, f1, v0;
    uint32_t a = 1, b = 0, i;

    irq_on(IRQ_VBLANK, on_vbl);
    irq_on(IRQ_SOFT, on_soft);
    f0 = FRAME; v0 = n_vbl;
    irq_enable(1);
    for (i = 0; i < 400000ul; i++) {              /* sums with carries, interrupted many times */
        a = a * 1664525ul + 1013904223ul;
        b += a >> 3;
    }
    irq_enable(0);
    f1 = FRAME;
    con("frames "); num((uint16_t)(f1 - f0)); con(", vertical-blank interrupts "); num((uint16_t)(n_vbl - v0)); con("\n");
    {
        uint32_t x = 1, y = 0;                    /* the same sums without interrupts */
        for (i = 0; i < 400000ul; i++) { x = x * 1664525ul + 1013904223ul; y += x >> 3; }
        check("sums the same with interrupts", x == a && y == b);
    }
    {
        int d = (int)(uint16_t)(f1 - f0) - (int)(uint16_t)(n_vbl - v0);
        check("one interrupt a frame", d >= -1 && d <= 1 && f1 != f0);
    }
    v0 = n_vbl;
    f0 = FRAME;
    while ((uint16_t)(FRAME - f0) < 3) ;
    check("nothing taken with IE clear", n_vbl == v0);
    IRQ_SET = 1u << IRQ_SOFT;                     /* pending, IE clear: waits */
    check("software interrupt held while IE is clear", n_soft == 0);
    irq_off(IRQ_VBLANK);
    irq_enable(1);
    check("software interrupt taken when IE is set", n_soft == 1);
    con(fails ? "IRQ FAILED\n" : "IRQ ok\n");
    return fails;
}
