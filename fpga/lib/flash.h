/* The flash ROM on the Spartan-3 (docs/io.md "Flash ROM"): an SST39VF1601C
 * on the A1 expansion connector, its low byte only, 512 KB. Byte k
 * (0 to FLASH_SIZE - 1) is the byte at FLASH_BASE + k.
 *
 * Programming only clears bits: erase a sector (2 KB here, 2 KWord in the
 * part) to FF first. The bottom 8 KB (sectors 0-3) are the boot block,
 * which the part's WP# pin protects when it is jumpered low. */
#ifndef FPGA_FLASH_H
#define FPGA_FLASH_H
#include <stdint.h>

#define FLASH_BASE   0xF80000UL
#define FLASH_SIZE   0x80000UL              /* bytes (flash words 0-7FFFF) */
#define FLASH_SECTOR 2048UL                 /* bytes a sector */
#define FLASH_BYTE(k) (*(volatile uint8_t *)(FLASH_BASE + (uint32_t)(k)))

/* Software ID: the maker (BF) in bits 15:8, the device's low byte (4B, or
 * 4F as one table of the data sheet has it) in bits 7:0. */
uint16_t flash_id(void);
/* erase the sector holding byte k; program byte k; 0 when done, -1 if the
 * part did not finish in time */
int flash_erase_sector(uint32_t k);
int flash_program(uint32_t k, uint8_t v);
/* program n bytes from src at byte k (erased already); -1 at a failure */
int flash_write(uint32_t k, const uint8_t *src, uint32_t n);
/* copy n bytes from byte k to dst */
void flash_read(uint32_t k, uint8_t *dst, uint32_t n);
#endif
