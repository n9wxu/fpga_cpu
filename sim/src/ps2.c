/* fpga_cpu simulator: the PS/2 ports (fpga/rtl/ps2.v, F00300), byte by
 * byte, with a keyboard and a mouse to plug into them.
 *
 * A port holds one received byte until a read takes it (the hardware holds
 * the clock low meanwhile, so the device waits); the next one follows a
 * byte time later. The host sends on the lines, by software (STAT writes:
 * clock low, data low, receiver off), and the device here takes it at line
 * level: on the request to send (the clock released with the data line
 * held low) it clocks eleven times (CLK_T apart), takes a data bit at each
 * of the first ten rising edges, acknowledges in the eleventh (the data
 * line low), and answers as a real one does (the acknowledge FA and what
 * the command returns). An empty port never clocks.
 *
 * Inputs: the GUI's keyboard (as set-2 scan codes) and mouse, or scripted:
 * --ps2keys STRING types into the keyboard (\p: wait 100 ms; \w: wait for
 * the mouse script's k), --ps2mouse "dx,dy,b[,wheel];..." moves the mouse
 * (wheel + is a turn away from the user; a step "k" waits until the
 * keyboard has typed what follows its \w; a wheel mouse: IntelliMouse, ID
 * 03 after the sample rates 200, 100, 80, with a fourth packet byte); each
 * starts 100 ms after
 * the device's last command, and the mouse only once reporting is on (F4). --ps2 A,B picks the devices (kbd,
 * mouse, none; default kbd,mouse). */
#include "cpu.h"

#include <stdlib.h>
#include <string.h>

#define BYTE_CYCLES 50000u    /* 1 ms: eleven bits at 11 kHz */
#define CLK_HALF 2000u        /* the device's clock: 40 us low, 40 us high (12.5 kHz) */
#define CLK_T (2 * CLK_HALF)
#define CLK_START 1000u       /* its first falling edge, 20 us after the request to send */
#define BAT_CYCLES 250000u    /* after a reset (FF): 5 ms (a real one takes 300-500 ms) */
#define IDLE_CYCLES 5000000u  /* scripted input starts 100 ms after the last command */
#define QLEN 256

enum { DEV_NONE, DEV_KBD, DEV_MOUSE };

typedef struct {
    int type;
    uint8_t q[QLEN];
    unsigned head, tail;
    int valid;
    uint8_t data;
    uint64_t next;            /* the device sends no sooner than this */
    uint8_t ctl;              /* STAT written: 1 clock low, 2 data low, 4 receiver off */
    int rxing, rx_k;          /* taking the host's byte: rising edges seen */
    uint64_t rx_t0;           /* its first falling edge */
    uint16_t rx_bits;
    uint8_t arg_for;          /* the command waiting for its argument */
    uint8_t last;             /* last byte sent to the host (FE: resend) */
    int scan, stream;         /* keyboard scanning, mouse reporting */
    uint8_t rates[3];         /* the mouse's last three sample rates (F3) */
    uint8_t id;               /* the mouse's ID: 00, or 03 once the rates were 200, 100, 80 (a wheel) */
    uint64_t last_cmd;
    const char *script;       /* scripted input still to come */
} port_t;

static port_t P[2] = { { .type = DEV_KBD, .scan = 1 }, { .type = DEV_MOUSE } };
static int mouse_at_k;                    /* the mouse script waits for the keyboard's */

static void put(port_t *p, uint8_t b)
{
    unsigned n = (p->tail + 1) % QLEN;
    if (n != p->head) {
        p->q[p->tail] = b;
        p->tail = n;
    }
}
static int qlen(const port_t *p) { return (int)((p->tail + QLEN - p->head) % QLEN); }

/* set-2 scan codes by USB HID usage (= SDL scancode); 0x100: E0 prefix */
static const uint16_t set2[232] = {
    [4] = 0x1C, 0x32, 0x21, 0x23, 0x24, 0x2B, 0x34, 0x33, 0x43, 0x3B, 0x42, 0x4B, 0x3A, 0x31,
    0x44, 0x4D, 0x15, 0x2D, 0x1B, 0x2C, 0x3C, 0x2A, 0x1D, 0x22, 0x35, 0x1A,
    [30] = 0x16, 0x1E, 0x26, 0x25, 0x2E, 0x36, 0x3D, 0x3E, 0x46, 0x45,
    [40] = 0x5A, 0x76, 0x66, 0x0D, 0x29, 0x4E, 0x55, 0x54, 0x5B, 0x5D, 0x5D, 0x4C, 0x52, 0x0E,
    0x41, 0x49, 0x4A, 0x58,
    [58] = 0x05, 0x06, 0x04, 0x0C, 0x03, 0x0B, 0x83, 0x0A, 0x01, 0x09, 0x78, 0x07,
    [71] = 0x7E,
    [73] = 0x170, 0x16C, 0x17D, 0x171, 0x169, 0x17A, 0x174, 0x16B, 0x172, 0x175,
    [83] = 0x77, 0x14A, 0x7C, 0x7B, 0x79, 0x15A, 0x69, 0x72, 0x7A, 0x6B, 0x73, 0x74, 0x6C,
    0x75, 0x7D, 0x70, 0x71,
    [101] = 0x12F,
    [224] = 0x14, 0x12, 0x11, 0x11F, 0x114, 0x59, 0x111, 0x127,
};

static void scan(port_t *p, int usage, int down)
{
    if (usage <= 0 || usage >= 232 || !set2[usage] || !p->scan)
        return;
    uint16_t s = set2[usage];
    if (s & 0x100)
        put(p, 0xE0);
    if (!down)
        put(p, 0xF0);
    put(p, (uint8_t)s);
}

static port_t *find(int type)
{
    for (int i = 0; i < 2; i++)
        if (P[i].type == type)
            return &P[i];
    return NULL;
}

void ps2_key(int usage, int down)
{
    port_t *p = find(DEV_KBD);
    if (p)
        scan(p, usage, down);
}

static void packet(port_t *p, int dx, int dy, int b, int z)
{
    dx = dx > 255 ? 255 : dx < -255 ? -255 : dx;
    dy = dy > 255 ? 255 : dy < -255 ? -255 : dy;
    put(p, (uint8_t)(0x08 | (b & 7) | (dx < 0 ? 0x10 : 0) | (dy < 0 ? 0x20 : 0)));
    put(p, (uint8_t)dx);
    put(p, (uint8_t)dy);
    if (p->id == 3) {                       /* IntelliMouse: the wheel, + toward the user */
        z = z > 7 ? 7 : z < -7 ? -7 : z;
        put(p, (uint8_t)-z);
    }
}

void ps2_mouse(int dx, int dy, int buttons, int wheel)
{
    port_t *p = find(DEV_MOUSE);
    if (p && p->stream && qlen(p) < QLEN - 8)
        packet(p, dx, dy, buttons, wheel);
}

/* ASCII -> HID usage, with bit 8 for shift (US layout) */
static int usage_of(int ch)
{
    static const char lo[] = "abcdefghijklmnopqrstuvwxyz1234567890\r\033\b\t -=[]\\\0;'`,./";
    static const char hi[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()\0\0\0\0 _+{}|\0:\"~<>?";
    for (int i = 0; i < (int)sizeof lo - 1; i++) {
        if (lo[i] && lo[i] == ch) return 4 + i;
        if (hi[i] && hi[i] == ch) return 0x100 | (4 + i);
    }
    return 0;
}

static void script_step(port_t *p, uint64_t now)
{
    if (!p->script || !*p->script || qlen(p) || now < p->last_cmd + IDLE_CYCLES)
        return;
    if (p->type == DEV_KBD && p->scan) {
        int ch = (unsigned char)*p->script++;
        if (ch == '\\' && *p->script == 'p') {   /* \p: a pause of 100 ms */
            p->script++;
            p->last_cmd = now;
            return;
        }
        if (ch == '\\' && *p->script == 'w') {   /* \w: until the mouse script reaches k */
            if (mouse_at_k) {
                p->script++;
                mouse_at_k = 2;             /* typing now */
            } else
                p->script--;
            return;
        }
        if (ch == '\\' && *p->script) {
            int e = *p->script++;
            ch = e == 'n' ? '\r' : e == 'e' ? 27 : e == 't' ? '\t' : e == 'b' ? '\b' : e;
        } else if (ch == '\n')
            ch = '\r';
        int u = usage_of(ch);
        if (u & 0x100) scan(p, 225, 1);
        scan(p, u & 0xFF, 1);
        scan(p, u & 0xFF, 0);
        if (u & 0x100) scan(p, 225, 0);
    } else if (p->type == DEV_MOUSE && p->stream) {
        if (*p->script == 'k') {            /* wait for the keyboard */
            port_t *k = find(DEV_KBD);
            if (!mouse_at_k)
                mouse_at_k = 1;
            if (mouse_at_k == 2 && k && (!k->script || !*k->script || (k->script[0] == '\\' && k->script[1] == 'w')) &&
                !qlen(k) && !k->valid) {
                mouse_at_k = 0;
                p->script++;
                if (*p->script == ';') p->script++;
            } else if (!k || !k->script)
                p->script++, mouse_at_k = 0;
            return;
        }
        char *end;
        long dx = strtol(p->script, &end, 0);
        long dy = *end == ',' ? strtol(end + 1, &end, 0) : 0;
        long b = *end == ',' ? strtol(end + 1, &end, 0) : 0;
        long z = *end == ',' ? strtol(end + 1, &end, 0) : 0;
        while (*end && *end != ';') end++;
        p->script = *end ? end + 1 : end;
        packet(p, (int)dx, (int)dy, (int)b, (int)z);
    }
}

/* the device takes a command byte */
static void command(port_t *p, uint8_t b, uint64_t now)
{
    p->last_cmd = now;
    if (p->arg_for) {                       /* LEDs, rate, resolution, scan set */
        if (p->type == DEV_MOUSE && p->arg_for == 0xF3) {
            p->rates[0] = p->rates[1];
            p->rates[1] = p->rates[2];
            p->rates[2] = b;
            if (p->rates[0] == 200 && p->rates[1] == 100 && p->rates[2] == 80)
                p->id = 3;
        }
        p->arg_for = 0;
        put(p, 0xFA);
        return;
    }
    if (b == 0xFE) {                        /* resend */
        put(p, p->last);
        return;
    }
    if (p->type == DEV_KBD) {
        switch (b) {
        case 0xFF: p->head = p->tail; p->scan = 1; put(p, 0xFA); put(p, 0xAA);
                   p->next = now + BAT_CYCLES; return;
        case 0xF2: put(p, 0xFA); put(p, 0xAB); put(p, 0x83); return;
        case 0xEE: put(p, 0xEE); return;
        case 0xED: case 0xF3: case 0xF0: put(p, 0xFA); p->arg_for = b; return;
        case 0xF4: p->scan = 1; put(p, 0xFA); return;
        case 0xF5: p->head = p->tail; p->scan = 0; put(p, 0xFA); return;
        case 0xF6: p->scan = 1; put(p, 0xFA); return;
        default: put(p, 0xFA); return;
        }
    }
    switch (b) {                            /* the mouse */
    case 0xFF: p->head = p->tail; p->stream = 0; p->id = 0; put(p, 0xFA); put(p, 0xAA); put(p, 0x00);
               p->next = now + BAT_CYCLES; return;
    case 0xF2: put(p, 0xFA); put(p, p->id); return;
    case 0xF4: p->stream = 1; put(p, 0xFA); return;
    case 0xF5: p->stream = 0; put(p, 0xFA); return;
    case 0xF6: p->stream = 0; put(p, 0xFA); return;
    case 0xE8: case 0xF3: put(p, 0xFA); p->arg_for = b; return;
    case 0xE9: put(p, 0xFA); put(p, p->stream ? 0x20 : 0x00); put(p, 0x02); put(p, 0x64); return;
    case 0xEB: put(p, 0xFA); packet(p, 0, 0, 0, 0); return;
    default: put(p, 0xFA); return;
    }
}

/* the device taking the host's byte: the rising edges up to now, each
 * taking the data line as the host's STAT writes left it */
static void rx_advance(port_t *p, uint64_t now)
{
    while (p->rxing && now >= p->rx_t0 + (uint64_t)p->rx_k * CLK_T + CLK_HALF) {
        if (p->rx_k < 10)
            p->rx_bits |= (uint16_t)((p->ctl & 2 ? 0 : 1) << p->rx_k);
        if (++p->rx_k == 11) {
            p->rxing = 0;
            uint8_t b = (uint8_t)p->rx_bits;
            unsigned ones = 0;
            for (int i = 0; i < 9; i++) ones += (p->rx_bits >> i) & 1;
            if (ones & 1) {
                command(p, b, now);
            } else
                put(p, 0xFE);                       /* parity error: resend */
            if (p->next < now + BYTE_CYCLES / 2)
                p->next = now + BYTE_CYCLES / 2;
        }
    }
}

/* the lines now: bit 0 clock high, bit 1 data high */
static int lines(port_t *p, uint64_t now)
{
    int clk = !(p->ctl & 1) && !p->valid, dat = !(p->ctl & 2);
    if (p->rxing) {
        uint64_t t = now - p->rx_t0;
        if (now >= p->rx_t0 && t < 11 * (uint64_t)CLK_T && t % CLK_T < CLK_HALF) {
            clk = 0;
            if (t / CLK_T == 10) dat = 0;            /* the acknowledge */
        }
    }
    return clk | dat << 1;
}

/* time passes: bytes arrive (interrupt sources 1 and 2) */
void ps2_poll(cpu_t *c)
{
    uint64_t now = c->st.cycles;
    for (int i = 0; i < 2; i++) {
        port_t *p = &P[i];
        rx_advance(p, now);
        if (p->rxing || p->ctl || p->type == DEV_NONE)
            continue;
        script_step(p, now);
        if (!p->valid && qlen(p) && now >= p->next) {
            p->data = p->last = p->q[p->head];
            p->head = (p->head + 1) % QLEN;
            p->valid = 1;
            c->dev.irq_pend |= (uint16_t)(2 << i);
        }
    }
}

int ps2_handles(uint32_t a) { return a >= 0xF00300u && a < 0xF00308u; }

uint16_t ps2_read(cpu_t *c, uint32_t a)
{
    ps2_poll(c);
    port_t *p = &P[(a >> 2) & 1];
    if (a & 2)
        return (uint16_t)(p->valid | lines(p, c->st.cycles) << 1);
    if (!p->valid)
        return 0xFFFF;
    p->valid = 0;
    p->next = c->st.cycles + BYTE_CYCLES;
    return p->data;
}

void ps2_write(cpu_t *c, uint32_t a, uint16_t v)
{
    ps2_poll(c);
    if (!(a & 2))
        return;                             /* DATA: nothing (the host sends on the lines) */
    port_t *p = &P[(a >> 2) & 1];
    uint8_t ctl = (uint8_t)(v & 7);
    if (ctl & 4)
        p->valid = 0;                       /* the receiver off: a waiting byte is dropped */
    /* the request to send: the clock released with the data line low */
    if ((p->ctl & 1) && !(ctl & 1) && (ctl & 2) && p->type != DEV_NONE && !p->rxing) {
        p->rxing = 1;
        p->rx_k = 0;
        p->rx_bits = 0;
        p->rx_t0 = c->st.cycles + CLK_START;
    }
    p->ctl = ctl;
}

void ps2_reset(void)
{
    for (int i = 0; i < 2; i++) {
        port_t *p = &P[i];
        p->valid = 0;
        p->ctl = 0;
        p->rxing = 0;
        p->arg_for = 0;
    }
}

/* --ps2 A,B */
int ps2_config(const char *s)
{
    for (int i = 0; i < 2; i++) {
        const char *e = strchr(s, ',');
        size_t n = e ? (size_t)(e - s) : strlen(s);
        int t = !strncmp(s, "kbd", n) && n == 3 ? DEV_KBD : !strncmp(s, "mouse", n) && n == 5 ? DEV_MOUSE
              : !strncmp(s, "none", n) && n == 4 ? DEV_NONE : -1;
        if (t < 0)
            return -1;
        P[i].type = t;
        P[i].scan = t == DEV_KBD;
        P[i].stream = 0;
        P[i].id = 0;
        if (!e) {
            if (i == 0) P[1].type = DEV_NONE;
            break;
        }
        s = e + 1;
    }
    return 0;
}

void ps2_script(int mouse, const char *s)
{
    port_t *p = find(mouse ? DEV_MOUSE : DEV_KBD);
    if (p)
        p->script = s;
}
