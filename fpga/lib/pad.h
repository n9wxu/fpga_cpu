/* Two Sega Genesis (Mega Drive) pads, 3- or 6-button, on the Spartan-3's
 * expansion board (docs/io.md "Game pads"):
 *
 *   uint16_t p[2];
 *   pad_read(p);                  // once a frame (not again within 1.5 ms)
 *   if (p[0] & PAD_A) ...
 *
 * A pad's value: the buttons held (1 = pressed), PAD_SIX for a 6-button pad
 * (X, Y, Z and Mode are 0 on a 3-button pad), PAD_PRESENT for any pad; no
 * pad reads 0. */
#ifndef FPGA_PAD_H
#define FPGA_PAD_H
#include <stdint.h>
#include <fcpu.h>

#define PADS IO16(0xF00900)   /* write: bit 0 = the select line (TH); read: pad 1's
                                 lines in bits 5:0, pad 2's in 13:8 */

#define PAD_UP      0x0001
#define PAD_DOWN    0x0002
#define PAD_LEFT    0x0004
#define PAD_RIGHT   0x0008
#define PAD_B       0x0010
#define PAD_C       0x0020
#define PAD_A       0x0040
#define PAD_START   0x0080
#define PAD_Z       0x0100
#define PAD_Y       0x0200
#define PAD_X       0x0400
#define PAD_MODE    0x0800
#define PAD_SIX     0x4000
#define PAD_PRESENT 0x8000

void pad_read(uint16_t pad[2]);
#endif
