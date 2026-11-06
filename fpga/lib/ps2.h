/* PS/2 keyboard and mouse on the two ports at F00300 (docs/io.md "PS/2
 * ports"), driven by interrupts. Either device in either port:
 *
 *   ps2_init();                 // reset both ports, find what is in each
 *   irq_enable(1);
 *   int k = kbd_get();          // like KBD_DATA: a key code, 0xFFFF if none
 *   ps2_mouse_get(&dx, &dy);    // movement since the last call; buttons back
 *   ps2_mouse_read(&m);         // the same, with the wheel and the presses since
 *
 * Key codes are those of KBD_DATA (docs/io.md "Keyboard"): ASCII, Ctrl-A..Z
 * as 1-26, arrows 0x80-0x83, Home/End/PgUp/PgDn/Insert 0x84-0x88, F1-F12
 * 0x90-0x9B; with kbd_releases(1) releases also arrive, code | 0x8000.
 * ps2_service() in the main loop sets up a device plugged in later. */
#ifndef FPGA_PS2_H
#define FPGA_PS2_H
#include <stdint.h>
#include <fcpu.h>

#define PS2A_DATA IO16(0xF00300)
#define PS2A_STAT IO16(0xF00302)
#define PS2B_DATA IO16(0xF00304)
#define PS2B_STAT IO16(0xF00306)
#define PS2_DATA(p) IO16(0xF00300 + 4 * (p))
#define PS2_STAT(p) IO16(0xF00302 + 4 * (p))
#define PS2_WAITING 1                /* STAT (read): a byte waits in DATA */
#define PS2_CLK     2                /* STAT (read): the clock line is high */
#define PS2_DAT     4                /* STAT (read): the data line is high */
#define PS2_CLK_LO  1                /* STAT (write): hold the clock low */
#define PS2_DAT_LO  2                /* STAT (write): hold the data line low */
#define PS2_RX_OFF  4                /* STAT (write): the receiver off (a byte waiting is dropped) */

enum { PS2_NONE, PS2_KBD, PS2_MOUSE };
extern volatile uint8_t ps2_type[2];   /* what ps2_init found in A and B */

void ps2_init(void);
int ps2_detect(int port);             /* reset one port and find its device (interrupts off for it) */
void ps2_service(void);               /* a device was plugged in: set it up */

/* polled, for set-up: send a byte (0: sent; -1 no device), on the lines
 * by software, with the device interrupts held off for its ~1 ms (IRQ_EN);
 * a byte within frames (-1: none) */
int ps2_send(int port, uint8_t b);
int ps2_recv(int port, uint16_t frames);

uint16_t kbd_get(void);
uint16_t kbd_mod(void);               /* bit 0 shift, 1 ctrl, 2 alt (as KBD_MOD) */
void kbd_releases(int on);
uint8_t ps2_mouse_get(int16_t *dx, int16_t *dy);   /* buttons: bit 0 left, 1 right, 2 middle */

/* movement (dy + up), the wheel (dz + a turn away from the user), the
 * buttons now and those pressed since the last call (a click shorter than
 * the time between calls is still seen) */
typedef struct { int16_t dx, dy, dz; uint8_t buttons, pressed; } ps2_mouse_t;
void ps2_mouse_read(ps2_mouse_t *m);
extern volatile uint8_t ps2_wheel;    /* the mouse has a wheel (IntelliMouse) */
#endif
