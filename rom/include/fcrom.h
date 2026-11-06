/* fcrom.h: calling the ROM (BIOS and Toolbox) from a program, docs/rom.md.
 *
 *   #include <fcrom.h>          fcc -I rom/include ...
 *   if (fcrom_present()) { con_puts("hello\n"); snd_beep(880, 100); }
 *
 * Each call loads its slot's pointer (0x000100 + 4n, filled by the BIOS at boot) and calls it:
 * an ordinary C call, so the program needs no symbols from the ROM and works with any ROM
 * version that has the slot (tb_version()). A program that returns from main (or calls exit)
 * goes back to the ROM's shell. */
#ifndef FCROM_H
#define FCROM_H
#include <stdint.h>
#include <stdarg.h>

typedef void (*tb_handler_t)(void);    /* an interrupt handler (irq_set) */
typedef void (*tb_fn_t)(void);         /* any slot's function (tb_patch) */

/* a ROM disk file (rd_entry, rd_find) */
typedef struct {
    char     name[16];
    uint32_t offset;                    /* its flash address */
    uint32_t length;
    uint32_t load;                      /* where rd_load puts it */
    uint32_t entry;
} tb_file_t;

/* the anchors in low RAM (docs/rom.md) */
#define FCROM_MAGIC   0x4D524346UL      /* "FCRM" at 0x000020 */
#define FCROM_ANCHOR  ((volatile uint32_t *)0x000020)
#define FCROM_SLOTS   ((void *volatile *)0x000100)
#define FCROM_NSLOTS  512
static inline int fcrom_present(void) { return FCROM_ANCHOR[0] == FCROM_MAGIC; }

/* interrupt sources (docs/io.md "Interrupts") */
enum { TB_IRQ_VBLANK = 0, TB_IRQ_PS2A = 1, TB_IRQ_PS2B = 2, TB_IRQ_AUDIO = 3, TB_IRQ_SOFT = 7 };
/* snd_tone waves */
enum { TB_PULSE = 1, TB_TRIANGLE = 2, TB_SAW = 3, TB_NOISE = 4 };
/* con_target */
enum { TB_SERIAL = 1, TB_SCREEN = 2 };

#define FCROM_CALL(n, ret, params) ((ret (*) params)FCROM_SLOTS[n])
#define SLOT(n, ret, name, params, args) \
    static inline ret name params { return FCROM_CALL(n, ret, params) args; }
#define SLOTV(n, name, params, args) \
    static inline void name params { FCROM_CALL(n, void, params) args; }
#include "slots.def"
#undef SLOT
#undef SLOTV

/* printf over con_vprintf (no variadic slot) */
static inline void con_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    con_vprintf(fmt, ap);
    va_end(ap);
}
#endif
