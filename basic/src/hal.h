/* Hardware abstraction layer for BASIC.
 *
 * hal_host.c runs the interpreter on the development machine; hal_fcpu.c
 * runs it on the fpga_cpu target, against the registers in docs/io.md.
 * Everything else in basic/src is shared and must be correct with a
 * 16-bit int.
 */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#define HAL_EOF (-2) /* input is finished (host, or simulator stdin closed) */

void     hal_init(void);
void     hal_exit(int code);
uint8_t *hal_arena(uint32_t *size); /* the BASIC workspace */

/* Console */
void hal_serial_putc(char c);          /* serial console; '\n' is newline */
int  hal_getkey(int wait);             /* io.md key code; -1 = none (wait == 0); HAL_EOF */
int  hal_readline(char *buf, int max); /* one input line, without the newline; length or HAL_EOF */
int  hal_break(void);                  /* nonzero once if Ctrl-C was pressed */

/* Target memory: SRAM, I/O registers, ROM */
uint8_t *hal_mem(uint32_t addr);       /* pointer to SRAM (video buffers); addr < 1 MB */
uint8_t  hal_peek8(uint32_t addr);
void     hal_poke8(uint32_t addr, uint8_t v);
uint16_t hal_peek16(uint32_t addr);    /* addr must be even */
void     hal_poke16(uint32_t addr, uint16_t v);
uint32_t hal_frame(void);              /* frames since power-on (60 Hz) */
void     hal_call(uint32_t addr);      /* run machine code */

/* Files (docs/io.md file service). Results are >= 0, errors < 0. */
int hal_fopen(const char *name, int mode); /* mode 0 read, 1 write, 2 append; returns handle */
int hal_fread(int h, void *buf, int len);
int hal_fwrite(int h, const void *buf, int len);
int hal_fclose(int h);
int hal_fdelete(const char *name);
int hal_dir_open(void);
int hal_dir_next(char *name, uint32_t *size); /* 1 = entry, 0 = end */

/* Implemented by the interpreter, called by the HAL's main(). */
void basic_main(const char *autorun, int quiet);

#endif
