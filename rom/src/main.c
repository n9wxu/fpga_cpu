/* The ROM's start (docs/rom.md "Boot"): the FPGA's boot copier has put this image at F80000
 * and started it; fcc's crt0 set the stack (FFFFF0) and cleared the BSS. */
#include "rom.h"

/* ---- the slot table: what the BIOS writes into 000100 */
static void tb_unimpl(void) { sys_panic("a ROM call this ROM does not have"); }

#define SLOT(n, ret, name, params, args) [n] = (void *)name,
#define SLOTV(n, name, params, args) [n] = (void *)name,
static void *const table[NSLOTS] = {
#include "../include/slots.def"
};
#undef SLOT
#undef SLOTV

static void slots_install(void)
{
    for (int16_t i = 0; i < NSLOTS; i++)
        SLOTS[i] = table[i] ? table[i] : (void *)tb_unimpl;
}

/* the warm restart: where a program's exit comes back to (crt0 jumps through 000024), on the
 * program's stack: take back the ROM's own */
__asm__(
    "\t.text\n"
    "\t.globl rom_warm\n"
    "rom_warm:\n"
    "\tLA sp, 0xFFFFF0\n"
    "\tJAL rom_warm_c\n");

static void anchors(void)
{
    extern void rom_warm(void);
    ANCHOR[0] = 0x4D524346UL;           /* "FCRM" */
    ANCHOR[1] = (uint32_t)(uintptr_t)rom_warm;
    ANCHOR[2] = ROM_VERSION;
    ANCHOR[3] = BOARD_TANG;
}

static void devices(void)
{
    __asm__ volatile("MTSR sr6, %0" : : "r"((uint16_t)0) : "memory");
    irq_init();
    vid_init();
    con_init();
    snd_init();
}

void rom_warm_c(int16_t code)
{
    (void)code;
    slots_install();
    anchors();
    devices();
    con_puts("\n");
    shell();
}

static uint32_t crc_state;

static void tones(int ok)
{
    static const uint16_t up[] = { 523, 659, 784, 1047 }, down[] = { 392, 330, 262, 196 };
    const uint16_t *t = ok ? up : down;
    for (int16_t i = 0; i < 4; i++) {
        snd_tone(31, t[i], 0x50, ok ? 2 : 1);
        sys_delay(ok ? 70 : 160);
    }
    snd_off(31);
}

int main(void)
{
    crc_state = rom_crc_ok();           /* first: before any variable changes */
    anchors();
    slots_install();
    mem_init();
    devices();
    con_color(3, 0);
    con_puts("fpga_cpu ROM 1.0  BIOS and Toolbox  (Tang Nano 20K, T1F)\n");
    con_color(7, 0);
    int16_t fails = selftest();
    if (crc_state == 2) fails++;
    tones(fails == 0);

    tb_file_t e;
    if (!fails && rd_find("BASIC", &e) >= 0) {
        con_puts("Starting BASIC: press a key for the shell ");
        uint32_t t0 = sys_ms();
        int16_t k = -1;
        while (sys_ms() - t0 < 2000 && (k = con_poll()) == -1)
            ;
        con_puts("\n");
        if (k == -1) sh_run("BASIC");
    }
    shell();
    return 0;
}

/* ---- the self-test: each line printed before its test, the result after */
static int16_t line(const char *what) { con_puts("  "); con_puts(what); con_puts(" ... "); return 0; }
static int16_t result(int ok, const char *note)
{
    con_color(ok ? 2 : 1, 0);
    con_puts(ok ? "ok" : "FAILED");
    con_color(7, 0);
    if (note) { con_puts("  "); con_puts(note); }
    con_puts("\n");
    return ok ? 0 : 1;
}

/* a march over 64 KB at the top of the program's RAM and one word in every 4 KB page of it,
 * without destroying what is there */
static int16_t ram_test(void)
{
    volatile uint32_t *p = (volatile uint32_t *)(APP_TOP - 0x10000UL);
    for (uint16_t i = 0; i < 0x4000; i++) p[i] = 0x55AA0000UL ^ i;
    for (uint16_t i = 0; i < 0x4000; i++) if (p[i] != (0x55AA0000UL ^ i)) return 0;
    for (uint16_t i = 0; i < 0x4000; i++) p[i] = ~(0x55AA0000UL ^ i);
    for (uint16_t i = 0; i < 0x4000; i++) if (p[i] != ~(0x55AA0000UL ^ i)) return 0;
    for (uint32_t a = APP_BASE; a < APP_TOP; a += 0x1000UL) {
        volatile uint32_t *w = (volatile uint32_t *)(uintptr_t)(a + 0xFFC);
        uint32_t keep = *w;
        *w = a ^ 0xA5A5A5A5UL;
        if (*w != (a ^ 0xA5A5A5A5UL)) return 0;
        *w = keep;
    }
    return 1;
}

int16_t selftest(void)
{
    int16_t f = 0;
    char note[40];
    line("ROM copy CRC");
    f += result(crc_state != 2, crc_state == 1 ? "(not from the flash)" : 0);
    line("RAM");
    f += result(ram_test(), "7.5 MB for programs");
    line("flash");
    uint32_t id = fl_id();
    note[0] = 0;
    {
        int16_t n = rd_count();
        char *q = note;
        const char *h = "0123456789ABCDEF";
        for (int16_t s = 20; s >= 0; s -= 4) *q++ = h[(id >> s) & 15];
        *q++ = ','; *q++ = ' ';
        if (n >= 10) *q++ = (char)('0' + n / 10);
        *q++ = (char)('0' + n % 10);
        ut_strcpy(q, " file(s) on the ROM disk");
    }
    f += result(id == 0x0B4017UL, note);
    line("audio engine");                /* (not counted: the machine works without sound) */
    if (AUD_G(0x14) == 0xA032) result(1, 0);
    else { con_color(3, 0); con_puts("absent\n"); con_color(7, 0); }
    return f;
}
