/* fpga_cpu C runtime: I/O registers (docs/io.md). */
#ifndef _FCPU_H
#define _FCPU_H
#include <stdint.h>

#define IO16(a) (*(volatile uint16_t *)(a))
#define IO32(a) (*(volatile uint32_t *)(a))

#define CON_OUT   IO16(0xF00000)
#define SIM_EXIT  IO16(0xF00002)
#define CYCLE     IO32(0xF00004)
#define CON_IN    IO16(0xF00008)
#define FRAME     IO16(0xF0000A)
#define VSTAT     IO16(0xF0000C)
#define KBD_DATA  IO16(0xF00100)
#define KBD_MOD   IO16(0xF00102)

#endif
