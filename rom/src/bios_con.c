/* The console: the serial port and the text layer (80 x 25 cells), docs/rom.md "Console".
 *
 * Serial output is paced: the Tang's transmit queue holds 256 bytes and drops what comes
 * after, and the CPU cannot see how full it is, so the BIOS counts what it has sent against
 * the time (115200 baud: 87 us a byte, 1465 CYCLE periods) and waits while ~200 are queued. */
#include "rom.h"

#define COLS 80
#define ROWS 25
#define TX_PERIOD 1465u                 /* CYCLE periods (16.875 MHz) per byte at 115200 baud */
#define TX_ROOM   200u

uint16_t con_mask = 3;                  /* bit 0 serial, bit 1 screen */
static uint16_t row, col;
static uint16_t attr = 0x0700;          /* white on black */
static volatile uint16_t *text = (volatile uint16_t *)0x0FC000;
static uint32_t tx_t;                   /* CYCLE when the queue is empty */

static uint32_t cyc(void)
{
    uint16_t lo = CYC_LO, hi = CYC_HI;
    if (CYC_LO < lo) { lo = CYC_LO; hi = CYC_HI; }  /* the low half wrapped between the reads */
    return (uint32_t)hi << 16 | lo;
}

static void ser_put(uint8_t c)
{
    uint32_t now = cyc();
    if ((int32_t)(tx_t - now) < 0) tx_t = now;
    while (tx_t - now > TX_ROOM * TX_PERIOD) now = cyc();
    CON_OUT = c;
    tx_t += TX_PERIOD;
}

/* waits until what was sent has gone out (before a reboot or a jump that could lose it) */
void con_flush_serial(void)
{
    while ((int32_t)(tx_t - cyc()) > 0) ;
}

static void cursor(void) { VID_CURSOR = (uint16_t)(row * COLS + col); }

static void scroll(void)
{
    volatile uint16_t *t = text;
    for (uint16_t i = 0; i < COLS * (ROWS - 1); i++) t[i] = t[i + COLS];
    for (uint16_t i = COLS * (ROWS - 1); i < COLS * ROWS; i++) t[i] = (uint16_t)(attr | ' ');
}

static void scr_put(uint8_t c)
{
    switch (c) {
    case '\r': col = 0; break;
    case '\n': col = 0; if (++row == ROWS) { scroll(); row = ROWS - 1; } break;
    case '\b': if (col) col--; else if (row) { row--; col = COLS - 1; } break;
    case '\t': do scr_put(' '); while (col & 7); return;
    case '\f': con_clear(); return;
    default:
        if (c < 32) return;
        text[row * COLS + col] = (uint16_t)(attr | c);
        if (++col == COLS) { col = 0; if (++row == ROWS) { scroll(); row = ROWS - 1; } }
    }
    cursor();
}

void con_init(void)
{
    text = (volatile uint16_t *)(uintptr_t)vid_text_addr();
    attr = 0x0700; tx_t = cyc();
    con_clear();
}

void con_putc(uint8_t c)
{
    if (con_mask & 1) { if (c == '\n') ser_put('\r'); ser_put(c); }
    if (con_mask & 2) scr_put(c);
}

void con_puts(const char *s) { while (*s) con_putc((uint8_t)*s++); }
void con_write(const char *s, uint16_t n) { while (n--) con_putc((uint8_t)*s++); }
void con_target(uint16_t mask) { con_mask = mask & 3; }

void con_clear(void)
{
    for (uint16_t i = 0; i < COLS * ROWS; i++) text[i] = (uint16_t)(attr | ' ');
    row = col = 0;
    cursor();
}

void con_goto(uint16_t r, uint16_t c)
{
    row = r < ROWS ? r : ROWS - 1;
    col = c < COLS ? c : COLS - 1;
    cursor();
}

void con_color(uint16_t fg, uint16_t bg)
{
    attr = (uint16_t)((fg & 7) << 8 | (bg >= 8 ? 0x4000 : (bg & 7) << 11));
}

int16_t con_poll(void)
{
    uint16_t k = CON_IN;
    if (k == 0xFFFF) return -1;
    if (k == 0xFFFE) return -2;
    return (int16_t)(k & 0xFF);
}

int16_t con_getc(void)
{
    int16_t k;
    while ((k = con_poll()) == -1) ;
    return k;
}

/* a line: printable characters echoed, BS/DEL deletes, Ctrl-U clears, CR or LF ends;
 * -1 at the end of input (simulator) with nothing typed */
int16_t con_gets(char *buf, uint16_t size)
{
    uint16_t n = 0;
    for (;;) {
        int16_t k = con_getc();
        if (k == -2) { if (!n) return -1; break; }
        if (k == '\r' || k == '\n') break;
        if ((k == 8 || k == 127)) {
            if (n) { n--; con_puts("\b \b"); }
        } else if (k == 21) {
            while (n) { n--; con_puts("\b \b"); }
        } else if (k >= 32 && n + 1 < size) {
            buf[n++] = (char)k;
            con_putc((uint8_t)k);
        }
    }
    buf[n] = 0;
    con_putc('\n');
    return (int16_t)n;
}

void con_puthex(uint32_t v, int16_t digits)
{
    if (digits < 1 || digits > 8) digits = 8;
    for (int16_t i = (int16_t)(digits - 1); i >= 0; i--)
        con_putc((uint8_t)"0123456789ABCDEF"[(v >> (4 * i)) & 15]);
}

static void put_u(uint32_t v, uint16_t base, int16_t width, char pad, int upper)
{
    char b[12];
    int16_t n = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do { b[n++] = dig[v % base]; v /= base; } while (v);
    while (width-- > n) con_putc((uint8_t)pad);
    while (n) con_putc((uint8_t)b[--n]);
}

void con_putdec(int32_t v)
{
    if (v < 0) { con_putc('-'); put_u((uint32_t)-(v + 1) + 1u, 10, 0, ' ', 0); }
    else put_u((uint32_t)v, 10, 0, ' ', 0);
}

/* %[-0][width][l]{d,u,x,X,s,c,p,%} */
void con_vprintf(const char *f, va_list ap)
{
    for (; *f; f++) {
        if (*f != '%') { con_putc((uint8_t)*f); continue; }
        f++;
        int left = 0; char pad = ' '; int16_t w = 0; int lng = 0;
        if (*f == '-') { left = 1; f++; }
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') w = (int16_t)(w * 10 + (*f++ - '0'));
        if (*f == 'l') { lng = 1; f++; }
        switch (*f) {
        case 'd': {
            int32_t v = lng ? va_arg(ap, int32_t) : (int32_t)(int16_t)va_arg(ap, int);
            if (v < 0) { con_putc('-'); if (w) w--; put_u((uint32_t)-(v + 1) + 1u, 10, w, pad, 0); }
            else put_u((uint32_t)v, 10, w, pad, 0);
            break;
        }
        case 'u': put_u(lng ? va_arg(ap, uint32_t) : (uint16_t)va_arg(ap, unsigned), 10, w, pad, 0); break;
        case 'x': case 'X':
            put_u(lng ? va_arg(ap, uint32_t) : (uint16_t)va_arg(ap, unsigned), 16, w, pad, *f == 'X'); break;
        case 'p': put_u((uint32_t)(uintptr_t)va_arg(ap, void *), 16, 6, '0', 1); break;
        case 'c': con_putc((uint8_t)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            int16_t n = 0;
            if (!s) s = "(null)";
            while (s[n]) n++;
            if (!left) while (w-- > n) con_putc(' ');
            con_puts(s);
            if (left) while (w-- > n) con_putc(' ');
            break;
        }
        case '%': con_putc('%'); break;
        case 0: return;
        default: con_putc('%'); con_putc((uint8_t)*f);
        }
    }
}

int rom_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    con_vprintf(fmt, ap);
    va_end(ap);
    return 0;
}
