/* PS/2 keyboard and mouse driver (ps2.h): set-up by polling, then the
 * ports' interrupts decode scan codes (set 2) into key codes and mouse
 * packets into movement. */
#include "ps2.h"
#include "irq.h"

volatile uint8_t ps2_type[2];
static volatile uint8_t replug[2];      /* BAT (AA) arrived: a device was plugged in */

/* ---- polled ---- */

/* ---- sending: the host's side of the PS/2 line protocol ----
 * Request to send: the clock held low 120 us, then the data line low and the
 * clock released. The device then clocks eleven times (10-17 kHz): at each
 * of its first ten falling edges the next bit goes on the data line (data
 * 0-7, odd parity, the stop bit: released), which it takes at the rising
 * edge; at the eleventh it pulls the data line low (the acknowledge). The
 * clock's low half is 30-50 us, so each edge is polled for with the device
 * interrupts held off (IRQ_EN 0; IE cannot be read back). A device that has
 * not clocked within 20 ms is not there. */

#define T16 IO16(0xF00004)                /* CYCLE's low half: 10 MHz */

/* the clock line reaches level (PS2_CLK or 0) within us microseconds */
static int wait_clk(int port, uint16_t level, uint16_t us)
{
    uint16_t t0 = T16, ticks = (uint16_t)(us * 10);
    while ((PS2_STAT(port) & PS2_CLK) != level)
        if ((uint16_t)(T16 - t0) > ticks)
            return -1;
    return 0;
}

int ps2_send(int port, uint8_t b)
{
    uint16_t en = IRQ_EN;
    IRQ_EN = 0;
    PS2_STAT(port) = PS2_RX_OFF | PS2_CLK_LO;
    uint16_t t0 = T16;
    while ((uint16_t)(T16 - t0) < 1200) ;
    PS2_STAT(port) = PS2_RX_OFF | PS2_CLK_LO | PS2_DAT_LO;
    PS2_STAT(port) = PS2_RX_OFF | PS2_DAT_LO;             /* the start bit */
    /* the clock reads high first: the port's filter (three samples, ~4 us)
     * still shows the host's own low for a moment */
    int r = wait_clk(port, PS2_CLK, 100);
    if (r == 0) {
        r = -1;
        for (int i = 0; i < 4 && r; i++)                   /* its first clock: up to 20 ms */
            r = wait_clk(port, 0, 5000);
    }
    if (r == 0) {
        uint8_t x = (uint8_t)(b ^ b >> 4);
        x ^= (uint8_t)(x >> 2);
        x ^= (uint8_t)(x >> 1);
        uint16_t f = (uint16_t)(b | (x & 1 ? 0 : 0x100) | 0x200);   /* odd parity, the stop bit */
        for (int i = 0; i < 10 && r == 0; i++) {
            PS2_STAT(port) = (uint16_t)(PS2_RX_OFF | (f & 1 ? 0 : PS2_DAT_LO));
            f >>= 1;
            r = wait_clk(port, PS2_CLK, 1000);
            if (r == 0) r = wait_clk(port, 0, 1000);
        }
        /* the acknowledge: data low with the eleventh clock, then both high */
        if (r == 0) r = wait_clk(port, PS2_CLK, 1000);
        uint16_t t1 = T16;
        while ((PS2_STAT(port) & (PS2_CLK | PS2_DAT)) != (PS2_CLK | PS2_DAT))
            if ((uint16_t)(T16 - t1) > 10000) { r = -1; break; }
    }
    PS2_STAT(port) = 0;
    IRQ_EN = en;
    return r;
}

int ps2_recv(int port, uint16_t frames)
{
    uint16_t f0 = FRAME;
    do {
        uint16_t v = PS2_DATA(port);
        if (v != 0xFFFF)
            return v;
    } while ((uint16_t)(FRAME - f0) <= frames);
    return -1;
}

/* a command, and its acknowledge (FA); 0 if it came */
static int cmd(int port, uint8_t b)
{
    for (int tries = 0; tries < 3; tries++) {
        if (ps2_send(port, b))
            return -1;
        int r = ps2_recv(port, 3);
        if (r == 0xFA)
            return 0;
        if (r != 0xFE)
            return -1;
    }
    return -1;
}

int ps2_detect(int port)
{
    irq_off(IRQ_PS2A + port);
    ps2_type[port] = PS2_NONE;
    replug[port] = 0;
    if (cmd(port, 0xFF))                  /* reset */
        return PS2_NONE;
    int r;
    while ((r = ps2_recv(port, 60)) >= 0 && r != 0xAA) ;   /* its self test: up to a second */
    if (r != 0xAA)
        return PS2_NONE;
    while (ps2_recv(port, 2) >= 0) ;      /* a mouse's ID (00) */
    if (cmd(port, 0xF2))                  /* identify */
        return PS2_NONE;
    r = ps2_recv(port, 3);
    int t;
    if (r == 0xAB) {                      /* a keyboard: AB 83 (or AB 41 translated) */
        ps2_recv(port, 3);
        t = PS2_KBD;
    } else if (r == 0x00 || r == 0x03 || r == 0x04) {
        t = cmd(port, 0xF4) ? PS2_NONE : PS2_MOUSE;   /* reporting on */
    } else
        t = PS2_NONE;
    ps2_type[port] = (uint8_t)t;
    return t;
}

/* ---- the keyboard ---- */

#define KQ 32
static volatile uint16_t kq[KQ];
static volatile uint8_t kq_head, kq_tail;
static uint8_t k_e0, k_f0, k_skip, k_rel;
static volatile uint8_t k_mod;          /* bit 0 shift, 1 ctrl, 2 alt, 3 caps lock */

/* set-2 codes 0x00-0x7F: the character unshifted, and shifted */
static const char lo[128] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\t`\0"
    "\0\0\0\0\0q1\0\0\0zsaw2\0"
    "\0cxde43\0\0 vftr5\0"
    "\0nbhgy6\0\0\0mju78\0"
    "\0,kio09\0\0./l;p-\0"
    "\0\0'\0[=\0\0\0\0\r]\0\\\0\0"
    "\0\0\0\0\0\0\b\0\0" "1\0" "47\0\0\0"
    "0.2568\033\0\0+3-*9\0\0";
static const char hi[128] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\t~\0"
    "\0\0\0\0\0Q!\0\0\0ZSAW@\0"
    "\0CXDE$#\0\0 VFTR%\0"
    "\0NBHGY^\0\0\0MJU&*\0"
    "\0<KIO)(\0\0>?L:P_\0"
    "\0\0\"\0{+\0\0\0\0\r}\0|\0\0"
    "\0\0\0\0\0\0\b\0\0" "1\0" "47\0\0\0"
    "0.2568\033\0\0+3-*9\0\0";

static void kput(uint16_t k)
{
    uint8_t n = (uint8_t)((kq_tail + 1) % KQ);
    if (n != kq_head) {
        kq[kq_tail] = k;
        kq_tail = n;
    }
}

/* shift: bit 0 shift, bit 3 caps lock */
static uint16_t key_code(uint8_t s, uint8_t e0, uint8_t shift)
{
    if (e0) {
        switch (s) {
        case 0x75: return 0x80;
        case 0x72: return 0x81;
        case 0x6B: return 0x82;
        case 0x74: return 0x83;
        case 0x6C: return 0x84;
        case 0x69: return 0x85;
        case 0x7D: return 0x86;
        case 0x7A: return 0x87;
        case 0x70: return 0x88;
        case 0x71: return 0x7F;
        case 0x5A: return 13;
        case 0x4A: return '/';
        }
        return 0;
    }
    switch (s) {
    case 0x05: return 0x90; case 0x06: return 0x91; case 0x04: return 0x92; case 0x0C: return 0x93;
    case 0x03: return 0x94; case 0x0B: return 0x95; case 0x83: return 0x96; case 0x0A: return 0x97;
    case 0x01: return 0x98; case 0x09: return 0x99; case 0x78: return 0x9A; case 0x07: return 0x9B;
    }
    if (s >= 0x80)
        return 0;
    char c = (shift & 1) ? hi[s] : lo[s];
    if (!c)
        return 0;
    if ((shift & 8) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c = (char)(c ^ 0x20);             /* caps lock */
    if ((k_mod & 2) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        return (uint16_t)((c & 0x1F));
    return (uint8_t)c;
}

static void kbd_byte(uint8_t b)
{
    if (k_skip) { k_skip--; return; }
    switch (b) {
    case 0xE0: k_e0 = 1; return;
    case 0xF0: k_f0 = 1; return;
    case 0xE1: k_skip = 7; return;        /* Pause: E1 14 77 E1 F0 14 F0 77 */
    case 0xFA: case 0xEE: case 0xFE: case 0x00: case 0xFF: k_e0 = k_f0 = 0; return;
    }
    uint8_t e0 = k_e0, up = k_f0;
    k_e0 = k_f0 = 0;
    uint8_t bit = 0;
    if (b == 0x12 || b == 0x59) { if (e0) return; bit = 1; }   /* (E0 12: a fake shift) */
    else if (b == 0x14) bit = 2;
    else if (b == 0x11) bit = 4;
    if (bit) {
        k_mod = (uint8_t)(up ? (k_mod & ~bit) : (k_mod | bit));
        return;
    }
    if (b == 0x58 && !e0) {               /* caps lock: toggles */
        if (!up) k_mod ^= 8;
        return;
    }
    uint16_t k = key_code(b, e0, up ? 0 : (k_mod & 9));   /* releases: unshifted */
    if (!k)
        return;
    if (!up)
        kput(k);
    else if (k_rel)
        kput((uint16_t)(k | 0x8000));
}

uint16_t kbd_get(void)
{
    if (kq_head == kq_tail)
        return 0xFFFF;
    uint16_t k = kq[kq_head];
    kq_head = (uint8_t)((kq_head + 1) % KQ);
    return k;
}

uint16_t kbd_mod(void) { return k_mod & 7; }
void kbd_releases(int on) { k_rel = (uint8_t)on; }

/* ---- the mouse ---- */

static uint8_t m_pkt[3], m_n;
static volatile int16_t m_dx, m_dy;
static volatile uint8_t m_b;

static void mouse_byte(uint8_t b)
{
    if (m_n == 0 && !(b & 0x08))          /* out of step: the first byte has bit 3 set */
        return;
    m_pkt[m_n++] = b;
    if (m_n < 3)
        return;
    m_n = 0;
    uint8_t f = m_pkt[0];
    if (f & 0xC0)                         /* overflow: drop */
        return;
    m_dx += (int16_t)((f & 0x10) ? (int16_t)m_pkt[1] - 256 : m_pkt[1]);
    m_dy += (int16_t)((f & 0x20) ? (int16_t)m_pkt[2] - 256 : m_pkt[2]);
    m_b = f & 7;
}

uint8_t ps2_mouse_get(int16_t *dx, int16_t *dy)
{
    irq_enable(0);
    *dx = m_dx;
    *dy = m_dy;
    m_dx = m_dy = 0;
    uint8_t b = m_b;
    irq_enable(1);
    return b;
}

/* ---- interrupts ---- */

static void port_irq(int port)
{
    IRQ_PEND = (uint16_t)(2u << port);    /* first: a byte after this one pends again */
    uint16_t v = PS2_DATA(port);
    if (v == 0xFFFF)
        return;
    uint8_t b = (uint8_t)v;
    uint8_t t = ps2_type[port];
    if (t == PS2_KBD) {
        if (b == 0xAA && !k_f0 && !k_e0) { replug[port] = 1; return; }
        kbd_byte(b);
    } else if (t == PS2_MOUSE) {
        if (b == 0xAA && m_n == 0) { replug[port] = 1; return; }
        mouse_byte(b);
    } else if (b == 0xAA)
        replug[port] = 1;
}
static void irq_a(void) { port_irq(0); }
static void irq_b(void) { port_irq(1); }

static void start(int port)
{
    ps2_detect(port);
    replug[port] = 0;
    irq_on(IRQ_PS2A + port, port ? irq_b : irq_a);
}

void ps2_init(void)
{
    start(0);
    start(1);
}

void ps2_service(void)
{
    for (int p = 0; p < 2; p++)
        if (replug[p])
            start(p);
}
