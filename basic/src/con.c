/* Text console: the 80x25 text layer, echoed to the serial console.
 *
 * Also a C64-style screen editor for keyboard input: the cursor moves
 * anywhere and Enter takes the logical line under it. A logical line
 * can wrap over several rows; cont[] marks rows that continue the one
 * above.
 */
#include "basic.h"

static uint16_t *tb;
static int crow, ccol;
static uint16_t attr;
static uint8_t cont[ROWS];
static int ed_row, ed_col;

void con_color(int fg, int bg)
{
    attr = (uint16_t)((fg & 7) << 8);
    if (bg < 0 || bg > 7)
        attr |= 0x4000; /* transparent background */
    else
        attr |= (uint16_t)((bg & 7) << 11);
}

static void clear_row(int r)
{
    uint16_t *p = tb + r * COLS;
    int i;
    for (i = 0; i < COLS; i++)
        p[i] = (uint16_t)(attr | ' ');
    cont[r] = 0;
}

void con_cls(void)
{
    int r;
    for (r = 0; r < ROWS; r++)
        clear_row(r);
    crow = ccol = 0;
    ed_row = ed_col = 0;
    con_flush();
}

void con_init(void)
{
    tb = (uint16_t *)hal_mem(TEXT_ADDR);
    hal_poke16(VID_TEXT, (uint16_t)(TEXT_ADDR & 0xFFFFu));
    hal_poke16(VID_TEXT + 2, (uint16_t)(TEXT_ADDR >> 16));
    con_color(7, -1);
    con_cls();
    vid_ctrl_set(VC_TEXT | VC_CURSOR, 1);
}

static void scroll(void)
{
    memmove(tb, tb + COLS, (size_t)(ROWS - 1) * COLS * 2u);
    memmove(cont, cont + 1, ROWS - 1);
    clear_row(ROWS - 1);
    crow = ROWS - 1;
    ed_row--;
}

static void newline(void)
{
    ccol = 0;
    if (++crow >= ROWS)
        scroll();
    cont[crow] = 0;
}

static void put_cell(int c)
{
    tb[crow * COLS + ccol] = (uint16_t)(attr | (uint8_t)c);
    if (++ccol >= COLS) {
        ccol = 0;
        if (++crow >= ROWS)
            scroll();
        cont[crow] = 1;
    }
}

void con_putc(int c)
{
    switch (c) {
    case '\n':
        hal_serial_putc('\n');
        newline();
        return;
    case '\r': return;
    case 8:
        if (ccol > 0)
            ccol--;
        return;
    case 12: con_cls(); return;
    }
    if ((uint8_t)c < 32)
        return;
    hal_serial_putc((char)c);
    put_cell(c);
}

void con_puts(const char *s)
{
    while (*s)
        con_putc((uint8_t)*s++);
}

void con_write(const char *s, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
        con_putc((uint8_t)s[i]);
}

void con_locate(int row, int col)
{
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
        error(E_FC);
    crow = row;
    ccol = col;
    con_flush();
}

int con_col(void) { return ccol; }

void con_flush(void) { hal_poke16(VID_CURSOR, (uint16_t)(crow * COLS + ccol)); }

/* ---- screen editor ---- */

void con_edit_begin(void)
{
    ed_row = crow;
    ed_col = ccol;
    con_flush();
}

static void del_at(int r, int c)
{
    uint16_t *p = tb + r * COLS;
    int i;
    for (i = c; i < COLS - 1; i++)
        p[i] = p[i + 1];
    p[COLS - 1] = (uint16_t)(attr | ' ');
}

int con_edit_key(int k, char *buf, int max)
{
    switch (k) {
    case 13: {
        int r0 = crow, r1 = crow, r, c0 = 0, n = 0;
        while (r0 > 0 && cont[r0])
            r0--;
        while (r1 < ROWS - 1 && cont[r1 + 1])
            r1++;
        if (ed_row >= r0 && ed_row <= r1) {
            r0 = ed_row;
            c0 = ed_col;
        }
        for (r = r0; r <= r1; r++) {
            int c;
            for (c = (r == r0 ? c0 : 0); c < COLS && n < max - 1; c++)
                buf[n++] = (char)(tb[r * COLS + c] & 0xFF);
        }
        while (n > 0 && buf[n - 1] == ' ')
            n--;
        buf[n] = 0;
        crow = r1;
        {
            int i;
            for (i = 0; i < n; i++)
                hal_serial_putc(buf[i]);
        }
        con_putc('\n');
        con_flush();
        return n;
    }
    case 0x80: /* up */
        if (crow > 0)
            crow--;
        break;
    case 0x81: /* down */
        if (crow < ROWS - 1)
            crow++;
        else {
            scroll();
            cont[crow] = 0;
        }
        break;
    case 0x82: /* left */
        if (ccol > 0)
            ccol--;
        else if (crow > 0) {
            crow--;
            ccol = COLS - 1;
        }
        break;
    case 0x83: /* right */
        if (++ccol >= COLS) {
            ccol = 0;
            if (crow < ROWS - 1)
                crow++;
        }
        break;
    case 0x84: ccol = 0; break; /* home */
    case 0x85: {                /* end of the text on this row */
        int c = COLS - 1;
        while (c > 0 && (tb[crow * COLS + c] & 0xFF) == ' ')
            c--;
        ccol = c < COLS - 1 && (tb[crow * COLS + c] & 0xFF) != ' ' ? c + 1 : c;
        break;
    }
    case 8: /* backspace */
        if (ccol > 0) {
            ccol--;
            del_at(crow, ccol);
        }
        break;
    case 0x7F: del_at(crow, ccol); break;
    case 0x88: { /* insert a space */
        uint16_t *p = tb + crow * COLS;
        int i;
        for (i = COLS - 1; i > ccol; i--)
            p[i] = p[i - 1];
        p[ccol] = (uint16_t)(attr | ' ');
        break;
    }
    case 12: con_cls(); break;
    default:
        if (k >= 32 && k < 127)
            put_cell(k);
        break;
    }
    con_flush();
    return -1;
}
