/* The microcoded audio engine from C: see mc.h. */
#include "mc.h"

static const uint16_t semi[12] = {      /* 2^(k/12) - 1, Q16 */
    0, 3897, 8026, 12400, 17034, 21944, 27146, 32657, 38496, 44682, 51236, 58179 };

uint32_t mc_ratio(int16_t s)
{
    int16_t oct = 0;
    while (s < 0) { s += 12; oct--; }
    while (s >= 12) { s -= 12; oct++; }
    uint32_t r = 0x10000UL + semi[s];
    return oct >= 0 ? r << oct : r >> -oct;
}

/* A4 (MIDI 69) = 440 Hz: 440 / 48000 * 2^32 = 39370533 */
uint32_t mc_step(int16_t midi)
{
    uint32_t r = mc_ratio((int16_t)(midi - 69));          /* 16.16 */
    return (39370533UL >> 8) * (r >> 8);                   /* (step * r) >> 16 */
}

void mc_list(const uint8_t *nodes, const uint8_t *linked, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) {
        uint32_t next = (i + 1 < n) ? MC_WADDR(MC_NODE(nodes[i + 1])) : 0;
        MC_NODE(nodes[i])[0] = next | (linked && linked[i] ? 0x80000000UL : 0);
    }
    uint32_t h = n ? MC_WADDR(MC_NODE(nodes[0])) : 0;
    MC_HEAD_LO = (uint16_t)h;
    MC_HEAD_HI = (uint16_t)(h >> 16);
}

void mc_init(void)
{
    MC_CTRL = 0;
    MC_CS_ADDR = 0;
    for (uint16_t i = 0; i < MC_CS_WORDS; i++)
        for (uint16_t k = 0; k < 5; k++)
            MC_CS(k) = mc_cs[i][k];             /* CS_DATA4 commits the word */
    for (uint16_t i = 0; i < MC_NNODES; i++)
        for (uint16_t k = 0; k < 16; k++)
            MC_NODE(i)[k] = 0;
    MC_HEAD_LO = 0; MC_HEAD_HI = 0;
    MC_IRQ = 0xFF;
    MC_CTRL = 0x0001;                           /* run, output shift 0 */
}
