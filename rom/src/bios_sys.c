/* System: version, timers, buttons and LEDs, interrupts (the vector at 000010), patching,
 * panic and the warm restart. docs/rom.md "System", "Interrupts". */
#include "rom.h"

uint16_t tb_version(void) { return ROM_VERSION; }
uint16_t sys_board(void) { return BOARD_TANG; }
uint32_t sys_ram_top(void) { return APP_TOP; }
uint16_t sys_buttons(void) { return (uint16_t)(BUTTONS & 3); }
void sys_leds(uint16_t on) { LEDS = (uint16_t)(on & 0x3F); }

uint32_t sys_cycles(void)
{
    uint16_t lo = CYC_LO, hi = CYC_HI;
    if (CYC_LO < lo) { lo = CYC_LO; hi = CYC_HI; }
    return (uint32_t)hi << 16 | lo;
}

/* FRAME is 16 bits: the BIOS extends it on each call (call at least every 18 minutes) */
uint32_t sys_frame(void)
{
    static uint32_t hi;
    static uint16_t last;
    uint16_t f = FRAME;
    if (f < last) hi += 0x10000UL;
    last = f;
    return hi | f;
}

void sys_wait(uint16_t frames)
{
    uint32_t t = sys_frame() + frames;
    while ((int32_t)(sys_frame() - t) < 0) ;
}

/* CYCLE runs at 16.875 MHz: 16875 a millisecond. Kept as a 32-bit count of milliseconds from
 * the cycle counter's wraps (every 254 s): call at least that often. */
uint32_t sys_ms(void)
{
    static uint32_t ms, rem, last;
    uint32_t c = sys_cycles();
    uint32_t d = c - last;
    last = c;
    rem += d % 16875u;
    ms += d / 16875u + rem / 16875u;
    rem %= 16875u;
    return ms;
}

void sys_delay(uint16_t ms)
{
    uint32_t t0 = sys_cycles();
    uint32_t n = (uint32_t)ms * 16875u;
    while (sys_cycles() - t0 < n) ;
}

tb_fn_t tb_patch(int16_t slot, tb_fn_t fn)
{
    if (slot < 0 || slot >= NSLOTS) return 0;
    tb_fn_t old = (tb_fn_t)SLOTS[slot];
    SLOTS[slot] = (void *)fn;           /* one STP: an interrupt sees the old pointer or the new */
    return old;
}

/* ---- interrupts */
static tb_handler_t handlers[8];

void irq_dispatch(void)
{
    uint16_t p = IRQ_PEND & IRQ_EN;
    for (int16_t i = 0; i < 8; i++)
        if ((p >> i) & 1) {
            if (handlers[i]) handlers[i]();
            else IRQ_PEND = (uint16_t)(1u << i);
        }
}

__asm__(
    "\t.text\n"
    "\t.globl rom_irq_entry\n"
    "rom_irq_entry:\n"
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
extern char rom_irq_entry[];

/* the vector: a long branch, 0xF700 | offset[22:16], offset[15:0], offset = (target - 0x10) / 2,
 * 23 bits: from 000010 it reaches the ROM at F8xxxx by wrapping at 24 bits */
void irq_init(void)
{
    __asm__ volatile("MTSR sr6, %0" : : "r"((uint16_t)0) : "memory");
    IRQ_EN = 0;
    IRQ_PEND = 0xFF;
    for (int16_t i = 0; i < 8; i++) handlers[i] = 0;
    uint32_t off = ((uint32_t)(uintptr_t)rom_irq_entry - 0x10u) >> 1;
    volatile uint16_t *v = (volatile uint16_t *)0x10;
    v[0] = (uint16_t)(0xF700 | ((off >> 16) & 0x7F));
    v[1] = (uint16_t)off;
}

tb_handler_t irq_set(int16_t src, tb_handler_t h)
{
    if (src < 0 || src > 7) return 0;
    tb_handler_t old = handlers[src];
    handlers[src] = h;
    return old;
}

void irq_enable(int16_t src, int16_t on)
{
    if (src < 0 || src > 7) return;
    uint16_t bit = (uint16_t)(1u << src);
    if (on) {
        IRQ_PEND = bit;
        IRQ_EN = (uint16_t)(IRQ_EN | bit);
        __asm__ volatile("MTSR sr6, %0" : : "r"((uint16_t)1) : "memory");
    } else {
        IRQ_EN = (uint16_t)(IRQ_EN & ~bit);
        IRQ_PEND = bit;
    }
}

/* ---- the end of things */
void sys_panic(const char *why)
{
    __asm__ volatile("MTSR sr6, %0" : : "r"((uint16_t)0) : "memory");
    con_color(7, 1);
    con_puts("\n*** PANIC: ");
    con_puts(why);
    con_puts("\nHold S1 and S2 for a second to restart.\n");
    snd_quiet();
    for (;;) { sys_leds(0x3F); sys_delay(200); sys_leds(0); sys_delay(200); }
}

extern void rom_warm(void);
void sys_warm(void)
{
    rom_warm();
}
