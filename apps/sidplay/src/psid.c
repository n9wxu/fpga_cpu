/* PSID v1/v2 header parser. RSID and files without a play address are reported as errors
 * (they need an IRQ model this player does not have). */
#include <string.h>
#include "sidplay.h"

static uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] * 256u + p[1]); }

int psid_parse(const uint8_t *b, uint32_t len, psid_t *p)
{
    uint16_t off;
    uint32_t n;
    memset(p, 0, sizeof *p);
    if (len < 0x76)
        return 1;
    if (!memcmp(b, "RSID", 4)) {
        p->rsid = 1;
    } else if (memcmp(b, "PSID", 4)) {
        return 2;
    }
    p->version = (uint8_t)be16(b + 4);
    off = be16(b + 6);
    p->load = be16(b + 8);
    p->init = be16(b + 10);
    p->play = be16(b + 12);
    p->songs = be16(b + 14);
    p->start = be16(b + 16);
    p->speed = (uint32_t)be16(b + 18) * 65536UL + be16(b + 20);
    memcpy(p->name, b + 22, 32);
    memcpy(p->author, b + 54, 32);
    memcpy(p->released, b + 86, 32);
    if (p->version >= 2 && len >= 0x7C)
        p->flags = be16(b + 0x76);
    if (off < 0x76 || off > len)
        return 3;
    p->data = b + off;
    n = len - off;
    if (p->load == 0) {                         /* the load address is the first two bytes (little endian) */
        if (n < 2)
            return 3;
        p->load = (uint16_t)(p->data[0] + p->data[1] * 256u);
        p->data += 2;
        n -= 2;
    }
    if (n > 0x10000UL - p->load)
        n = 0x10000UL - p->load;                /* the 6502 address space ends at $FFFF */
    p->dlen = (uint16_t)n;
    if (p->songs == 0)
        p->songs = 1;
    if (p->start == 0 || p->start > p->songs)
        p->start = 1;
    if (p->init == 0)
        p->init = p->load;
    if (p->rsid)
        return 4;
    if (p->play == 0)
        return 5;
    return 0;
}

const char *psid_err(int e)
{
    switch (e) {
    case 1: return "file too short";
    case 2: return "no PSID header";
    case 3: return "bad data offset";
    case 4: return "RSID tune (needs its own IRQ handler): not supported";
    case 5: return "play address 0 (IRQ-driven tune): not supported";
    }
    return "ok";
}
