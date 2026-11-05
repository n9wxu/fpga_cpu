/* The flash ROM (flash.h): the SST39VF1601C's JEDEC command sequences on
 * its low byte, with the toggle bit (DQ6) for the end of a program or erase. */
#include "flash.h"

static void cmd(uint32_t a, uint8_t v) { FLASH_BYTE(a) = v; }

static void unlock(void)
{
    cmd(0x555, 0xAA);
    cmd(0x2AA, 0x55);
}

/* DQ6 toggles on each read until the part is done; n reads at most */
static int wait_done(uint32_t k, uint32_t n)
{
    uint8_t a = FLASH_BYTE(k);
    while (n--) {
        uint8_t b = FLASH_BYTE(k);
        if (((a ^ b) & 0x40) == 0) return 0;
        a = b;
    }
    return -1;
}

uint16_t flash_id(void)
{
    unlock();
    cmd(0x555, 0x90);
    uint16_t id = (uint16_t)(FLASH_BYTE(0) << 8 | FLASH_BYTE(1));
    cmd(0, 0xF0);
    return id;
}

/* a read is ~10 instructions here, a few microseconds: a program (10 us at
 * most) takes tens of reads, an erase (25 ms at most) thousands */
int flash_erase_sector(uint32_t k)
{
    unlock();
    cmd(0x555, 0x80);
    unlock();
    cmd(k & ~(FLASH_SECTOR - 1), 0x50);
    return wait_done(k, 200000);
}

int flash_program(uint32_t k, uint8_t v)
{
    unlock();
    cmd(0x555, 0xA0);
    cmd(k, v);
    return wait_done(k, 2000);
}

int flash_write(uint32_t k, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (src[i] != 0xFF && flash_program(k + i, src[i]) != 0) return -1;
    return 0;
}

void flash_read(uint32_t k, uint8_t *dst, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) dst[i] = FLASH_BYTE(k + i);
}
