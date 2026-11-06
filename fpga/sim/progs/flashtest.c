/* Flash ROM test (docs/io.md "Flash ROM", fpga/lib/flash.c), on the RTL
 * with fpga/sim/sst39vf.v as the part: its ID, a sector erased and
 * programmed and read back, a byte past A18, and the SRAM around it still
 * right (the two share the bus).
 *
 * Exit code 0 when all of it holds. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcpu.h>
#include "../../lib/flash.h"

static volatile uint32_t ram[64];

int main(void)
{
    int bad = 0;
    for (int i = 0; i < 64; i++) ram[i] = 0x01010101u * (uint32_t)i ^ 0xA5C3u;

    uint16_t id = flash_id();
    printf("id %04x\n", id);
    if (id >> 8 != 0xBF || ((id & 0xFF) != 0x4B && (id & 0xFF) != 0x4F)) bad |= 1;

    static const char msg[] = "Hello from the flash";
    const uint32_t k = 3 * FLASH_SECTOR;
    if (flash_erase_sector(k) != 0) bad |= 2;
    for (uint32_t i = 0; i < 32; i++) if (FLASH_BYTE(k + i) != 0xFF) bad |= 4;
    if (flash_write(k, (const uint8_t *)msg, sizeof msg) != 0) bad |= 8;
    char back[sizeof msg];
    flash_read(k, (uint8_t *)back, sizeof back);
    printf("read \"%s\"\n", back);
    if (memcmp(back, msg, sizeof msg) != 0) bad |= 16;

    /* past A18 (byte 40000 up), and its alias at byte k must not change */
    const uint32_t k2 = 0x40000 + k + 0x100;
    if (flash_program(k2, 0x5A) != 0 || FLASH_BYTE(k2) != 0x5A) bad |= 32;
    if (FLASH_BYTE(k + 0x100) != 0xFF) bad |= 64;

    for (int i = 0; i < 64; i++) if (ram[i] != (0x01010101u * (uint32_t)i ^ 0xA5C3u)) bad |= 128;
    printf(bad ? "FAIL %x\n" : "ok\n", bad);
    return bad;
}
