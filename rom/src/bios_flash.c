/* The SPI flash (F00900, docs/io.md "Flash") and the ROM disk at 480000 (docs/rom.md).
 * Each FL_DATA write sends a byte and receives one; the BIOS waits on FL_CTRL's busy bit. */
#include "rom.h"

static void sel(int on) { FL_CTRL = (uint16_t)(on ? 1 : 0); }
static uint8_t xfer(uint8_t b)
{
    FL_DATA = b;
    while (FL_CTRL & 1) ;
    return (uint8_t)FL_DATA;
}
static void cmd_addr(uint8_t c, uint32_t a)
{
    xfer(c); xfer((uint8_t)(a >> 16)); xfer((uint8_t)(a >> 8)); xfer((uint8_t)a);
}

uint32_t fl_id(void)
{
    sel(1);
    xfer(0x9F);
    uint32_t id = (uint32_t)xfer(0) << 16;
    id |= (uint32_t)xfer(0) << 8;
    id |= xfer(0);
    sel(0);
    return id;
}

void fl_read(uint32_t a, void *buf, uint32_t n)
{
    uint8_t *p = buf;
    sel(1);
    cmd_addr(0x03, a);
    while (n--) *p++ = xfer(0);
    sel(0);
}

static int16_t wait_ready(uint32_t limit_ms)
{
    uint32_t t0 = sys_ms();
    for (;;) {
        sel(1); xfer(0x05); uint8_t s = xfer(0); sel(0);
        if (!(s & 1)) return 0;
        if (sys_ms() - t0 > limit_ms) return -1;
    }
}

static void wren(void) { sel(1); xfer(0x06); sel(0); }

/* the bitstream (000000-3FFFFF) is not ours to change */
#define FL_GUARD 0x400000UL

int16_t fl_erase(uint32_t a)
{
    if (a < FL_GUARD || a >= 0x800000UL) return -2;
    wren();
    sel(1); cmd_addr(0x20, a & ~0xFFFUL); sel(0);
    return wait_ready(500);
}

int16_t fl_program(uint32_t a, const void *buf, uint32_t n)
{
    const uint8_t *p = buf;
    if (a < FL_GUARD || a + n > 0x800000UL) return -2;
    while (n) {
        uint32_t room = 256 - (a & 255);            /* a page program stays in its page */
        uint32_t k = n < room ? n : room;
        wren();
        sel(1); cmd_addr(0x02, a);
        for (uint32_t i = 0; i < k; i++) xfer(p[i]);
        sel(0);
        if (wait_ready(10) < 0) return -1;
        a += k; p += k; n -= k;
    }
    return 0;
}

/* ---- the ROM disk: "FCRD", count, 0, 0, then 32-byte entries (name, offset from the disk,
 * length, load, entry) */
static int16_t disk_n = -1;

int16_t rd_count(void)
{
    if (disk_n < 0) {
        uint32_t h[4];
        fl_read(FL_DISK, h, sizeof h);
        disk_n = (h[0] == 0x44524346UL && h[1] < 512) ? (int16_t)h[1] : 0;   /* "FCRD" */
    }
    return disk_n;
}

int16_t rd_entry(int16_t i, tb_file_t *e)
{
    if (i < 0 || i >= rd_count()) return -1;
    fl_read(FL_DISK + 16 + 32UL * (uint16_t)i, e, 32);
    e->name[15] = 0;
    e->offset += FL_DISK;
    return 0;
}

int16_t rd_find(const char *name, tb_file_t *e)
{
    for (int16_t i = 0; i < rd_count(); i++)
        if (!rd_entry(i, e) && !ut_stricmp(e->name, name)) return i;
    return -1;
}

uint32_t rd_load(const tb_file_t *e)
{
    if (e->load < APP_BASE || e->load + e->length > APP_TOP) return 0;
    fl_read(e->offset, (void *)(uintptr_t)e->load, e->length);
    return e->entry;
}

/* the copy in RAM against the CRC in the flash's header. The image includes .data's initial
 * values, so main calls this first, before anything has changed a variable. */
uint32_t rom_crc_ok(void)
{
    uint32_t h[8];
    fl_read(FL_ROM, h, sizeof h);
    if (h[0] != 0x4D524346UL) return 1;             /* no header: booted another way */
    uint32_t crc = ut_crc32(0, (const void *)(uintptr_t)h[2], h[1]);
    return crc == h[4] ? 0 : 2;
}
