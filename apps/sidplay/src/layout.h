/* Memory map of the Tang system: all of 000000-0FFFFF is SDRAM behind a 32 KB direct-mapped write-through cache
 * (cache set = address & 0x7FFF). Bases are staggered so that the hot parts of the big regions fall on
 * different sets: 6502 RAM at set 0 (zero page, stack, the tune), attributes at 0x4000, JIT tables at 0x6000,
 * frame ring 0xC00, text shadow 0x2000, rows 0x3000.
 *   001000  program image, BSS (JIT translation cache CODE_WORDS words), grows up; stack grows down from 030000
 *   030C00  frame ring (4 KB)   032000  text shadow (4 KB)   033000  tracker rows (4 KB)   034000  parsed PSID headers
 *   035000  statistics, 038000 engine register log (test build only, to 03EFFF)
 *   040000  6502 RAM (64 KB)    050000  SID write log (8 KB)   054000  6502 attributes (64 KB)
 *   066000  JIT event table, hash, bit maps (61.6 KB, to 075080)
 *   080000  tune slots 0-3, 64 KB each (the file; its length in the last 4 bytes)
 *   0C3C00  bitmap (hi-color)   0FC000  text */
#ifndef LAYOUT_H
#define LAYOUT_H
#define SDR_STACK_TOP 0x030000UL
#define SDR_FRAMES  0x030C00UL
#define SDR_TSHADOW 0x032000UL
#define SDR_ROWS    0x033000UL
#define SDR_TUNES   0x034000UL
#define SDR_STATS   0x035000UL
#define SDR_AUDLOG  0x038000UL
#define SDR_SLOT0   0x080000UL
#endif
