/* PS/2 test (docs/io.md "PS/2 ports", fpga/lib/ps2.c): finds a keyboard and
 * a mouse (in either port), then takes typed keys and mouse movement by
 * interrupt. The devices type "Hi!\n" and move (5,-3) with the left button,
 * then (10,2) with none:
 *
 *   fsim --no-hicolor --ps2keys 'Hi!\n' --ps2mouse '5,-3,1;10,2,0' ps2test.bin
 *   the RTL: fpga/sim/system_tb.v's devices (fpga/sim/ps2_dev.v) do the same
 *
 * Exit code 0 when everything arrived as sent. */
#include <stdint.h>
#include <stdio.h>
#include <fcpu.h>
#include "../../lib/irq.h"
#include "../../lib/ps2.h"

static const char *name(int t) { return t == PS2_KBD ? "keyboard" : t == PS2_MOUSE ? "mouse" : "none"; }

int main(void)
{
    ps2_init();
    printf("A: %s, B: %s\n", name(ps2_type[0]), name(ps2_type[1]));
    irq_enable(1);

    static const uint16_t want[] = { 'H', 'i', '!', 13 };
    uint16_t got[4];
    int n = 0, bad = 0;
    int16_t sx = 0, sy = 0;
    uint8_t saw_left = 0, b = 0;
    uint16_t f0 = FRAME;
    while ((n < 4 || sx != 15) && (uint16_t)(FRAME - f0) < 30) {
        uint16_t k = kbd_get();
        if (k != 0xFFFF) {
            if (n < 4) got[n] = k;
            n++;
        }
        int16_t dx, dy;
        b = ps2_mouse_get(&dx, &dy);
        sx += dx;
        sy += dy;
        if (b & 1) saw_left = 1;
    }
    irq_enable(0);
    printf("keys:");
    for (int i = 0; i < n && i < 4; i++) {
        printf(" %02x", got[i]);
        if (got[i] != want[i]) bad = 1;
    }
    printf("\nmouse: %d,%d, left %d, buttons now %d\n", sx, sy, saw_left, b);
    int ok = ps2_type[0] + ps2_type[1] == PS2_KBD + PS2_MOUSE && ps2_type[0] != PS2_NONE && n == 4 && !bad &&
             sx == 15 && sy == -1 && saw_left && b == 0;
    printf("%s\n", ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
