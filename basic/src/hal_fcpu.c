/* fpga_cpu HAL: the registers in docs/io.md.
 *
 * Keyboard keys (KBD_DATA) drive the screen editor. Serial characters
 * (CON_IN) are line-buffered, so a terminal or a piped script works too.
 * Both are polled into one type-ahead buffer, which also lets the
 * Ctrl-C check run without losing keys.
 *
 * Link-time symbols: _end (end of BSS) marks the start of the arena,
 * which runs up to STACK_RESERVE below the initial stack pointer.
 */
#include "basic.h"

#define IO(a)         (*(volatile uint16_t *)(uintptr_t)(a))
#define IO8(a)        (*(volatile uint8_t *)(uintptr_t)(a))
#define REG_EXIT      0xF00002UL
#define REG_CON_IN    0xF00008UL
#define REG_KBD       0xF00100UL
#define FS_CMD        0xF00500UL
#define FS_STATUS     0xF00502UL
#define FS_HANDLE     0xF00504UL
#define FS_MODE       0xF00506UL
#define FS_NAME       0xF00508UL
#define FS_BUF        0xF0050CUL
#define FS_LEN        0xF00510UL
#define FS_RESULT     0xF00512UL
#define FS_SIZE       0xF00514UL
#define STACK_TOP     0x080000UL
#define STACK_RESERVE 0x4000UL

extern char _end[];

/* type-ahead: the key code, with bit 14 set for serial input */
#define TA_SIZE 256
static uint16_t ta[TA_SIZE];
static uint16_t ta_head, ta_tail;
static int brk_pending;
static int in_eof; /* CON_IN reported end of input (0xFFFE, simulator only) */

static void poll_input(void)
{
    uint16_t k;
    while ((uint16_t)(ta_head - ta_tail) < TA_SIZE - 1 && (k = IO(REG_KBD)) != 0xFFFF) {
        if (k == 3)
            brk_pending = 1;
        else
            ta[ta_head++ % TA_SIZE] = k;
    }
    while (!in_eof && (uint16_t)(ta_head - ta_tail) < TA_SIZE - 1 && (k = IO(REG_CON_IN)) != 0xFFFF) {
        if (k == 0xFFFE)
            in_eof = 1;
        else if (k == 3)
            brk_pending = 1;
        else
            ta[ta_head++ % TA_SIZE] = (uint16_t)(0x4000u | (k & 0xFF));
    }
}

static int next_key(int wait)
{
    for (;;) {
        poll_input();
        if (brk_pending) {
            brk_pending = 0;
            return 3;
        }
        if (ta_head != ta_tail)
            return (int)ta[ta_tail++ % TA_SIZE];
        if (!wait)
            return -1;
        if (in_eof)
            return HAL_EOF;
        snd_poll();
    }
}

void hal_init(void) {}

void hal_exit(int code)
{
    IO(REG_EXIT) = (uint16_t)code;
    for (;;)
        ;
}

uint8_t *hal_arena(uint32_t *size)
{
    uintptr_t a = (uintptr_t)_end;
    *size = (uint32_t)(STACK_TOP - STACK_RESERVE - a);
#ifdef BASIC_ARENA_MAX /* test builds: same arena as the host build */
    if (*size > BASIC_ARENA_MAX)
        *size = BASIC_ARENA_MAX;
#endif
    return (uint8_t *)a;
}

void hal_serial_putc(char c) { IO(CON_OUT) = (uint8_t)c; }

int hal_getkey(int wait)
{
    int k = next_key(wait);
    if (k < 0)
        return k;
    k &= 0x3FFF;
    return k == 10 ? 13 : k;
}

int hal_readline(char *buf, int max)
{
    int n = 0;
    con_edit_begin();
    for (;;) {
        int k = next_key(1);
        if (k == HAL_EOF) {
            if (n == 0)
                return HAL_EOF;
            buf[n] = 0;
            return n;
        }
        if (k == 3) {
            brk_pending = 1; /* reported by hal_break() after the read */
            buf[0] = 0;
            return 0;
        }
        if (k & 0x4000) { /* serial: simple line editing */
            k &= 0xFF;
            if (k == 13 || k == 10) {
                buf[n] = 0;
                con_putc('\n');
                con_flush();
                return n;
            }
            if ((k == 8 || k == 127) && n > 0) {
                n--;
                con_putc(8);
                con_putc(' ');
                con_putc(8);
            } else if (k >= 32 && k < 127 && n < max - 1) {
                buf[n++] = (char)k;
                con_putc(k);
            }
            con_flush();
            continue;
        }
        n = con_edit_key(k, buf, max);
        if (n >= 0)
            return n;
        n = 0;
    }
}

int hal_break(void)
{
    poll_input();
    if (brk_pending) {
        brk_pending = 0;
        return 1;
    }
    return 0;
}

uint8_t *hal_mem(uint32_t addr) { return (uint8_t *)(uintptr_t)addr; }
uint8_t hal_peek8(uint32_t a) { return IO8(a); }
void hal_poke8(uint32_t a, uint8_t v) { IO8(a) = v; }
uint16_t hal_peek16(uint32_t a) { return IO(a); }
void hal_poke16(uint32_t a, uint16_t v) { IO(a) = v; }

uint32_t hal_frame(void)
{
    static uint16_t last;
    static uint32_t hi;
    uint16_t f = IO(REG_FRAME);
    if (f < last)
        hi += 0x10000UL;
    last = f;
    return hi | f;
}

void hal_call(uint32_t addr)
{
    void (*fn)(void) = (void (*)(void))(uintptr_t)addr;
    fn();
}

/* ---- file service ---- */

static void reg32(uint32_t r, uint32_t v)
{
    IO(r) = (uint16_t)(v & 0xFFFFu);
    IO(r + 2) = (uint16_t)(v >> 16);
}

static int fs(uint16_t cmd)
{
    IO(FS_CMD) = cmd;
    while (IO(FS_STATUS) & 0x8000u)
        ;
    return (int16_t)IO(FS_RESULT);
}

int hal_fopen(const char *name, int mode)
{
    reg32(FS_NAME, (uint32_t)(uintptr_t)name);
    IO(FS_MODE) = (uint16_t)mode;
    return fs(1);
}

int hal_fread(int h, void *buf, int len)
{
    IO(FS_HANDLE) = (uint16_t)h;
    reg32(FS_BUF, (uint32_t)(uintptr_t)buf);
    IO(FS_LEN) = (uint16_t)len;
    return fs(3);
}

int hal_fwrite(int h, const void *buf, int len)
{
    IO(FS_HANDLE) = (uint16_t)h;
    reg32(FS_BUF, (uint32_t)(uintptr_t)buf);
    IO(FS_LEN) = (uint16_t)len;
    return fs(4);
}

int hal_fclose(int h)
{
    IO(FS_HANDLE) = (uint16_t)h;
    return fs(2);
}

int hal_fdelete(const char *name)
{
    reg32(FS_NAME, (uint32_t)(uintptr_t)name);
    return fs(5);
}

int hal_dir_open(void) { return fs(6); }

int hal_dir_next(char *name, uint32_t *size)
{
    int r;
    reg32(FS_BUF, (uint32_t)(uintptr_t)name);
    r = fs(7);
    if (r == 1)
        *size = IO(FS_SIZE) | (uint32_t)IO(FS_SIZE + 2) << 16;
    return r;
}

/* Test builds define BASIC_QUIET=1: no banner, like the host's -q. */
#ifndef BASIC_QUIET
#define BASIC_QUIET 0
#endif

int main(void)
{
    hal_init();
    basic_main(0, BASIC_QUIET);
    return 0;
}
