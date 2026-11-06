/* The Shell (docs/rom.md "Shell"): a command line on the console. A word that is not a command
 * is looked up on the ROM disk and run. */
#include "rom.h"

static const char *skip(const char *s) { while (*s == ' ') s++; return s; }

static const char *word(const char *s, char *w, uint16_t n)
{
    s = skip(s);
    uint16_t i = 0;
    while (*s && *s != ' ') { if (i + 1 < n) w[i++] = *s; s++; }
    w[i] = 0;
    return s;
}

/* a number: hex with $ or 0x, else decimal; returns the rest, or 0 if none */
static const char *number(const char *s, uint32_t *v)
{
    s = skip(s);
    uint32_t x = 0;
    int base = 10, any = 0;
    if (*s == '$') { base = 16; s++; }
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (base == 16 && *s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (base == 16 && *s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else break;
        x = x * (uint32_t)base + (uint32_t)d;
        any = 1;
    }
    if (!any) return 0;
    *v = x;
    return s;
}

static void dump(uint32_t a, uint32_t n)
{
    while (n) {
        con_puthex(a, 6); con_puts(": ");
        uint16_t k = n < 16 ? (uint16_t)n : 16;
        for (uint16_t i = 0; i < 16; i++) {
            if (i < k) { con_puthex(*(volatile uint8_t *)(uintptr_t)(a + i), 2); con_putc(' '); }
            else con_puts("   ");
        }
        con_putc(' ');
        for (uint16_t i = 0; i < k; i++) {
            uint8_t c = *(volatile uint8_t *)(uintptr_t)(a + i);
            con_putc(c >= 32 && c < 127 ? c : '.');
        }
        con_putc('\n');
        a += k; n -= k;
    }
}

static void help(void)
{
    con_puts(
        "  HELP              this list\n"
        "  INFO              the ROM, the board, memory\n"
        "  DIR               the programs on the ROM disk\n"
        "  RUN name | name   load a ROM disk program and run it (BASIC, ...)\n"
        "  DUMP addr [len]   memory in hex (numbers: decimal, $hex or 0xhex)\n"
        "  POKE addr byte..  write bytes\n"
        "  GO addr           jump to a program in memory\n"
        "  BEEP [hz [ms]]    a tone\n"
        "  TEST              the self-test\n"
        "  CLS               clear the screen\n"
        "  Serial loading (tang/tools/run.py) works at any time; S1+S2 held: restart.\n");
}

static void info(void)
{
    rom_printf("  ROM %u.%u, board %u (Tang Nano 20K), slots 000100-0008FF\n",
               tb_version() >> 8, tb_version() & 255, sys_board());
    rom_printf("  program RAM %06lx-%06lx, system heap free %lu bytes\n",
               (uint32_t)APP_BASE, sys_ram_top() - 1, mem_avail());
    rom_printf("  flash %06lx, ROM image at %06lx, ROM disk at %06lx (%d files)\n",
               fl_id(), (uint32_t)FL_ROM, (uint32_t)FL_DISK, rd_count());
    rom_printf("  up %lu ms, frame %lu\n", sys_ms(), sys_frame());
}

static void dir(void)
{
    tb_file_t e;
    int16_t n = rd_count();
    for (int16_t i = 0; i < n; i++)
        if (!rd_entry(i, &e))
            rom_printf("  %-15s %7lu bytes  load %06lx  entry %06lx\n", e.name, e.length, e.load, e.entry);
    if (!n) con_puts("  (the ROM disk is empty)\n");
}

/* load a program and jump to it: it starts with interrupts off and the boot video layout; its
 * exit comes back through the anchor (rom_warm) */
static void go(uint32_t entry)
{
    con_flush_serial();
    __asm__ volatile("MTSR sr6, %0" : : "r"((uint16_t)0) : "memory");
    IRQ_EN = 0;
    IRQ_PEND = 0xFF;
    snd_quiet();
    ((void (*)(void))(uintptr_t)entry)();
    sys_warm();                         /* (a program that returns instead of exiting) */
}

int16_t sh_run(const char *name)
{
    tb_file_t e;
    if (rd_find(name, &e) < 0) return -1;
    uint32_t entry = rd_load(&e);
    if (!entry) { con_puts("  cannot load it there\n"); return -2; }
    go(entry);
    return 0;
}

int16_t sh_exec(const char *l)
{
    char w[16];
    uint32_t a, n;
    const char *r = word(l, w, sizeof w);
    if (!w[0]) return 0;
    if (!ut_stricmp(w, "help") || !ut_stricmp(w, "?")) help();
    else if (!ut_stricmp(w, "info")) info();
    else if (!ut_stricmp(w, "dir")) dir();
    else if (!ut_stricmp(w, "cls")) con_clear();
    else if (!ut_stricmp(w, "test")) { if (selftest()) con_puts("  something failed\n"); }
    else if (!ut_stricmp(w, "run")) {
        word(r, w, sizeof w);
        if (sh_run(w) == -1) con_puts("  no such program (DIR lists them)\n");
    } else if (!ut_stricmp(w, "dump")) {
        if (!(r = number(r, &a))) { con_puts("  DUMP addr [len]\n"); return -1; }
        if (!number(r, &n)) n = 128;
        dump(a & 0xFFFFFF, n);
    } else if (!ut_stricmp(w, "poke")) {
        uint32_t v;
        if (!(r = number(r, &a))) { con_puts("  POKE addr byte...\n"); return -1; }
        while ((r = number(r, &v))) *(volatile uint8_t *)(uintptr_t)(a++ & 0xFFFFFF) = (uint8_t)v;
    } else if (!ut_stricmp(w, "go")) {
        if (!number(r, &a)) { con_puts("  GO addr\n"); return -1; }
        go(a & 0xFFFFFE);
    } else if (!ut_stricmp(w, "beep")) {
        uint32_t hz = 880, ms = 150;
        if ((r = number(r, &hz))) number(r, &ms);
        snd_beep((uint16_t)hz, (uint16_t)ms);
    } else if (sh_run(w) == -1) {
        con_puts("  what? (HELP lists the commands)\n");
        return -1;
    }
    return 0;
}

void shell(void)
{
    char buf[80];
    con_puts("Shell: HELP lists the commands.\n");
    for (;;) {
        con_puts("] ");
        int16_t n = con_gets(buf, sizeof buf);
        if (n < 0) { con_puts("\n(end of input)\n"); EXIT_REG = 0; for (;;) ; }
        sh_exec(buf);
    }
}
