/* romtest: a program that calls the ROM through its slots (rom/include/fcrom.h) and checks
 * what comes back; on the ROM disk as ROMTEST. Ends with "romtest: N failed" and returns to the
 * Shell (in fsim the exit stops the run with N as the exit code). */
#include <stdint.h>
#include <fcrom.h>

static int fails;
static void check(int ok, const char *what)
{
    con_printf("  %-34s %s\n", what, ok ? "ok" : "FAILED");
    if (!ok) fails++;
}

static volatile int ticks;
static void vbl(void) { ticks++; *(volatile uint16_t *)0xF00700 = 1; }

int main(void)
{
    con_puts("romtest\n");
    check(fcrom_present(), "the ROM's anchor");
    check(tb_version() >= 0x0100, "tb_version");
    check(sys_board() == 1, "sys_board: Tang Nano 20K");

    char buf[32];
    ut_strcpy(buf, "Hello");
    check(ut_strlen(buf) == 5 && !ut_strcmp(buf, "Hello") && !ut_stricmp(buf, "HELLO"), "strings");
    check(ut_crc32(0, "123456789", 9) == 0xCBF43926UL, "ut_crc32 (the check value)");
    uint8_t a[64], b[64];
    ut_memset(a, 0x5A, sizeof a);
    ut_memcpy(b, a, sizeof b);
    check(!ut_memcmp(a, b, sizeof a) && b[63] == 0x5A, "memset, memcpy, memcmp");

    uint32_t before = mem_avail();
    char *p = mem_alloc(1000), *q = mem_alloc(5000);
    check(p && q && q > p && mem_avail() < before, "mem_alloc");
    mem_free(p); mem_free(q);
    check(mem_avail() == before, "mem_free merges");

    check(fl_id() == 0x0B4017UL, "fl_id: XT25F64B");
    tb_file_t e;
    check(rd_count() >= 2 && rd_find("romtest", &e) >= 0 && e.load == 0x1000, "the ROM disk");
    uint8_t head[4];
    fl_read(e.offset, head, 4);
    check(!ut_memcmp(head, (const void *)0x1000, 4), "fl_read: this program's first bytes");

    vid_ctrl2(0x0C);
    check(vid_ctrl2_get() == 0x0C, "vid_ctrl2 keeps its copy");
    vid_ctrl2(0);
    snd_ctrl(5, 0x0C);
    check(snd_ctrl_get(5) == 0x0C, "snd_ctrl keeps its copy");
    snd_ctrl(5, 0);

    uint32_t f0 = sys_frame();
    irq_set(TB_IRQ_VBLANK, vbl);
    irq_enable(TB_IRQ_VBLANK, 1);
    sys_wait(3);
    irq_enable(TB_IRQ_VBLANK, 0);
    check(sys_frame() - f0 >= 3 && ticks >= 2, "vertical-blank interrupts through the ROM");

    con_printf("  printf: %d %u %x %lx %s %c\n", -42, 65535u, 0xBEEFu, 0x12345678UL, "str", '!');
    con_printf("romtest: %d failed\n", fails);
    return fails;
}
