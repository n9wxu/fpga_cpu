/* The ROM's own header: every slot's function, the hardware, the shared state (docs/rom.md). */
#ifndef ROM_H
#define ROM_H
#include <stdint.h>
#include <stdarg.h>
#include <fcpu.h>

typedef void (*tb_handler_t)(void);
typedef void (*tb_fn_t)(void);
typedef struct {
    char     name[16];
    uint32_t offset, length, load, entry;
} tb_file_t;

#define SLOT(n, ret, name, params, args) ret name params;
#define SLOTV(n, name, params, args) void name params;
#include "../include/slots.def"
#undef SLOT
#undef SLOTV

#define ROM_VERSION 0x0100
#define BOARD_TANG  1

/* low RAM */
#define ANCHOR      ((volatile uint32_t *)0x000020)   /* [0] "FCRM", [1] warm restart, [2] version, [3] board */
#define SLOTS       ((void *volatile *)0x000100)
#define NSLOTS      512
#define APP_BASE    0x001000UL
#define APP_TOP     0x780000UL                         /* the ROM's SDRAM (F80000 = 780000) above */

/* the flash */
#define FL_ROM      0x400000UL                          /* the ROM image's header */
#define FL_DISK     0x480000UL                          /* the ROM disk */

/* registers (fcpu.h has CON_OUT, CON_IN, FRAME, VSTAT) */
#define R16(a)      (*(volatile uint16_t *)(uintptr_t)(a))
#define EXIT_REG    R16(0xF00002)
#define CYC_LO      R16(0xF00004)
#define CYC_HI      R16(0xF00006)
#define VID_CTRL    R16(0xF00200)
#define VID_BACKDROP R16(0xF00202)
#define VID_TEXT_LO R16(0xF00204)
#define VID_TEXT_HI R16(0xF00206)
#define VID_BMP_LO  R16(0xF00208)
#define VID_BMP_HI  R16(0xF0020A)
#define VID_SPR_LO  R16(0xF0020C)
#define VID_SPR_HI  R16(0xF0020E)
#define PAL_INDEX   R16(0xF00210)
#define PAL_DATA    R16(0xF00212)
#define VID_SCROLLX R16(0xF00214)
#define VID_SCROLLY R16(0xF00216)
#define VID_CURSOR  R16(0xF00218)
#define VID_CTRL2   R16(0xF00220)
#define VID_STAT    R16(0xF00222)
#define LEDS        R16(0xF00600)
#define BUTTONS     R16(0xF00604)
#define IRQ_PEND    R16(0xF00700)
#define IRQ_EN      R16(0xF00702)
#define FL_DATA     R16(0xF00900)
#define FL_CTRL     R16(0xF00902)
#define BLT(o)      R16(0xF00800 + (o))
#define AUD_G(o)    R16(0xF01000 + (o))
#define AUD_V(v, w) R16(0xF01800 + 64 * (v) + 2 * (w))

/* internals */
void con_init(void);
void con_flush_serial(void);
extern uint16_t con_mask;
void vid_init(void);
void snd_init(void);
void irq_init(void);
void mem_init(void);
void shell(void);
int16_t selftest(void);
uint32_t rom_crc_ok(void);              /* 0 when the copy matches the flash header's CRC */
int  rom_printf(const char *fmt, ...);
#endif
