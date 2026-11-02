/* System test for fpga/rtl/system.v: memory through memsys (with the
 * video DMA stealing cycles), the I/O registers, the console. The output
 * must match fsim's exactly; it prints no timing. */
#include <stdio.h>
#include <stdint.h>
#include <fcpu.h>

#define REG(a) IO16(a)

static uint8_t buf[300];

int main(void)
{
    REG(0xF00208) = 0x3C00; REG(0xF0020A) = 0x000C;   /* bitmap at 0C3C00 */
    REG(0xF00200) = 0x0007;                           /* text, bitmap, hi-color: DMA busy */
    printf("systest\n");

    /* bytes, words and pairs, at every alignment */
    for (int i = 0; i < 300; i++) buf[i] = (uint8_t)(i * 37 + 11);
    uint16_t sum = 0;
    for (int i = 0; i < 300; i++) sum = (uint16_t)(sum * 31 + buf[i]);
    printf("bytes %u\n", sum);
    uint16_t *w = (uint16_t *)buf;
    for (int i = 0; i < 100; i++) w[i] = (uint16_t)(w[i] ^ (i * 0x0101u + 0x8000u));
    sum = 0;
    for (int i = 0; i < 150; i++) sum = (uint16_t)(sum + w[i] * (i | 1));
    printf("words %u\n", sum);
    uint32_t *p = (uint32_t *)buf, acc = 0;
    for (int i = 0; i < 50; i++) { p[i] += 0x01020304u * (uint32_t)i; acc ^= p[i] << (i & 7); }
    printf("pairs %lu\n", (unsigned long)acc);

    /* far memory: near the top of SRAM */
    volatile uint8_t *top = (volatile uint8_t *)0x0FFF00;
    for (int i = 0; i < 64; i++) top[i] = (uint8_t)(255 - i);
    sum = 0;
    for (int i = 0; i < 64; i++) sum = (uint16_t)(sum * 3 + top[i]);
    printf("top %u\n", sum);

    /* I/O registers read back */
    REG(0xF00214) = 1234; REG(0xF00216) = 77;
    REG(0xF00600) = 0xA5;
    REG(0xF00210) = 250;  REG(0xF00212) = 0xF800; REG(0xF00212) = 0x07E0;
    printf("regs %u %u %u %u %u\n", REG(0xF00214), REG(0xF00216), REG(0xF00600),
           REG(0xF00210), REG(0xF00200));
    uint32_t c0 = CYCLE, c1 = CYCLE;
    printf("cycle %s\n", c1 > c0 ? "counts" : "STUCK");
    REG(0xF00600) = 0;
    printf("done\n");
    return 0;
}
