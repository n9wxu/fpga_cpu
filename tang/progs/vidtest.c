/* Video bring-up for tang/rtl/system.v (64 KB, so the text layer only):
 * every attribute, a backdrop that changes each second, live frame, scan
 * line and cycle counters, and serial input echoed on the screen. Link
 * with --stack 0xC000 (memory repeats every 64 KB: the text buffer at
 * 0FC000 is at C000). */
#include <stdint.h>
#include <fcpu.h>

#define TEXT ((volatile uint16_t *)0x0FC000)
#define VID_CTRL     IO16(0xF00200)
#define VID_BACKDROP IO16(0xF00202)
#define VID_CURSOR   IO16(0xF00218)
#define LEDS         IO16(0xF00600)
#define BUTTONS      IO16(0xF00604)

static void put(int row, int col, const char *s, int attr)
{
    volatile uint16_t *t = TEXT + row * 80 + col;
    while (*s) *t++ = (uint16_t)(attr << 8 | (uint8_t)*s++);
}

static void hex(int row, int col, uint32_t v, int digits, int attr)
{
    char b[9];
    for (int i = digits - 1; i >= 0; i--, v >>= 4) b[i] = "0123456789ABCDEF"[v & 15];
    b[digits] = 0;
    put(row, col, b, attr);
}

int main(void)
{
    /* attribute: [6] transparent background, [5:3] background, [2:0] foreground */
    const int W = 0x47, Y = 0x46, BAR = 0x0F;
    for (int i = 0; i < 2000; i++) TEXT[i] = 0x4720;
    put(0, 0, " fpga_cpu on the Tang Nano 20K: T1 at 16.875 MHz, 720p HDMI                    ", BAR);

    put(2, 1, "attributes (background down, foreground across):", W);
    for (int bg = 0; bg < 8; bg++)
        for (int fg = 0; fg < 8; fg++) {
            volatile uint16_t *t = TEXT + (4 + bg) * 80 + 2 + fg * 4;
            for (int k = 0; k < 3; k++) t[k] = (uint16_t)((bg << 3 | fg) << 8 | "Ab#"[k]);
        }
    put(13, 1, "transparent background over the backdrop:", W);
    for (int fg = 0; fg < 8; fg++) put(13, 44 + fg * 4, "Ab#", 0x40 | fg);
    put(15, 1, "CP437:", W);
    for (int c = 0; c < 256; c++) TEXT[(16 + c / 64) * 80 + 8 + c % 64] = (uint16_t)(0x0700 | c);

    put(21, 1, "frame", W);
    put(21, 14, "scan line", W);
    put(21, 31, "cycles", W);
    put(21, 50, "buttons", W);
    put(23, 1, "type on the serial console:", W);

    VID_CTRL = 0x21;                        /* text and cursor */
    int col = 29;
    uint16_t last = FRAME;
    for (;;) {
        uint16_t f = FRAME;
        hex(21, 7, f, 4, Y);
        hex(21, 24, VSTAT >> 3, 3, Y);
        hex(21, 38, CYCLE, 8, Y);
        hex(21, 58, BUTTONS, 1, Y);
        if ((uint16_t)(f - last) >= 60) {   /* each second, the next backdrop */
            last = f;
            VID_BACKDROP = (VID_BACKDROP + 1) & 7;
            LEDS = VID_BACKDROP;
        }
        uint16_t c = CON_IN;
        if (c != 0xFFFF) {
            if (c == '\r' || col >= 79) {
                for (int i = 29; i < 80; i++) TEXT[23 * 80 + i] = 0x4720;
                col = 29;
            }
            if (c >= ' ') TEXT[23 * 80 + col++] = (uint16_t)(0x4600 | c);
            CON_OUT = c;
        }
        VID_CURSOR = 23 * 80 + col;
    }
}
