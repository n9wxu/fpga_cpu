/* t1mplay: plays a T1M module (docs/t1m.md) on the Tang's microcoded audio engine, with a tracker
 * view on the text screen: the pattern around the playing row, the channels' instruments and
 * levels (the engine's meters), the song's position.
 *
 * The module is the one built in (build/song.h, from songs/), or one loaded at 0x100000
 * (tang/tools/run.py --data 0x100000:SONG.t1m) when one is there.
 *
 * Keys (serial console): space pause, n / p next / previous order position, 1-9 0 mute channels
 * 1-10, q quit. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcpu.h>
#include "irq.h"
#include "mc.h"
#include "t1m.h"
#include "song.h"

#define VID_CTRL  IO16(0xF00200)
#define VID_TXT_L IO16(0xF00204)
#define VID_TXT_H IO16(0xF00206)
#define MODULE_AT ((const uint32_t *)0x100000UL)

static uint16_t txt[80 * 25];
enum { BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE };
#define A(fg, bg) ((uint16_t)(((bg) << 3 | (fg)) << 8))

static void put(int x, int y, const char *s, uint16_t a)
{
    uint16_t *p = &txt[y * 80 + x];
    while (*s && x++ < 80) *p++ = (uint16_t)(a | (uint8_t)*s++);
}
static void line(int y, const char *s, uint16_t a)   /* a whole row in one pass: no blank moment */
{
    uint16_t *p = &txt[y * 80];
    for (int x = 0; x < 80; x++) p[x] = (uint16_t)(a | (uint8_t)(*s ? *s++ : ' '));
}
static void fill(int y, uint16_t a)
{
    for (int x = 0; x < 80; x++) txt[y * 80 + x] = (uint16_t)(a | ' ');
}
static const char *note_s(uint8_t n, char *b)
{
    static const char nm[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
    if (n == 0xFF) return "...";
    if (n == 0x80) return "===";
    if (n == 0x81) return "^^^";
    b[0] = nm[2 * (n % 12)]; b[1] = nm[2 * (n % 12) + 1];
    int o = n / 12 - 1;
    b[2] = (char)(o < 0 ? '-' : '0' + o);
    b[3] = 0;
    return b;
}
static char hex(uint8_t v) { return "0123456789ABCDEF"[v & 15]; }

static void on_block(void)
{
    MC_IRQ = 0xFF;
    IRQ_PEND = (uint16_t)(1u << IRQ_AUDIO);
    t1m_block();
}

#define SHOWN 10                        /* channels in the pattern view */
static uint8_t pn[256 * SHOWN], pi_[256 * SHOWN];
static uint8_t shown_pat = 0xFF;

static void draw(void)
{
    char b[96], nb[4];
    uint8_t nch = t1m.channels < SHOWN ? t1m.channels : SHOWN;
    if (t1m.pattern != shown_pat) {
        shown_pat = t1m.pattern;
        t1m_decode(shown_pat, pn, pi_, nch, 256);
    }
    uint16_t rows = (uint16_t)(t1m.rows + 1);
    uint16_t hz = (uint16_t)(t1m.tick_hz256 >> 8);
    uint16_t clocks = MC_CLOCKS;
    snprintf(b, sizeof b, " T1M  %-50.50s%22.22s ", t1m.title, t1m.author);
    line(0, b, A(WHITE, BLUE));
    snprintf(b, sizeof b, " ORD %02X/%02X PAT %02X ROW %02X/%02X SPD %d TICK %dHz VOL %02X NODES %d TAILS %d ENGINE %d%% ",
             t1m.order, t1m.orders - 1, t1m.pattern, t1m.row, rows - 1, t1m.speed, hz, t1m.gvol, t1m.nodes, t1m.tails,
             (int)((uint32_t)clocks * 100 / 44992));
    line(1, b, A(CYAN, BLACK));
    fill(2, A(BLACK, CYAN));
    put(0, 2, " RW", A(BLACK, CYAN));
    for (uint8_t c = 0; c < nch; c++) {
        snprintf(b, sizeof b, "%s%-6d", t1m.ch[c].mute ? "x" : " ", c + 1);
        put(4 + 7 * c, 2, b, A(t1m.ch[c].mute ? RED : BLACK, CYAN));
    }
    for (int i = 0; i < 14; i++) {      /* the pattern: the playing row in the middle */
        int r = (int)t1m.row + i - 7, y = 3 + i;
        uint16_t bg = i == 7 ? BLUE : BLACK;
        fill(y, A(WHITE, bg));
        if (r < 0 || r >= rows) continue;
        b[0] = ' '; b[1] = hex((uint8_t)(r >> 4)); b[2] = hex((uint8_t)r); b[3] = 0;
        put(0, y, b, A(i == 7 ? WHITE : (r & 3) ? BLUE : CYAN, bg));
        for (uint8_t c = 0; c < nch; c++) {
            uint8_t n = pn[r * nch + c], in = pi_[r * nch + c];
            put(4 + 7 * c, y, note_s(n, nb), A(n == 0xFF ? BLUE : n >= 0x80 ? RED : WHITE, bg));
            b[0] = in == 0xFF ? '.' : hex((uint8_t)(in >> 4)); b[1] = in == 0xFF ? '.' : hex(in); b[2] = 0;
            put(8 + 7 * c, y, b, A(in == 0xFF ? BLUE : YELLOW, bg));
        }
    }
    for (uint8_t c = 0; c < 16 && c < t1m.channels; c++) {   /* the channels: instrument, note, level */
        int y = 17 + (c & 7), x = c < 8 ? 0 : 40;
        uint8_t in = t1m.ch[c].inst, n = t1m.ch[c].note;
        /* the note's meter: its first node's word 13, high half, read from the node itself (the
         * monitor's 32 meters follow list positions, and the list is all 64 nodes) */
        uint16_t lvl = t1m.ch[c].nodes ? ((volatile uint16_t *)MC_NODE(t1m.ch[c].node))[27] : 0;
        uint8_t bar = (uint8_t)(((uint32_t)lvl * t1m.ch[c].vol) >> 19);   /* 0-15 */
        if (t1m.ch[c].mute) bar = 0;
        snprintf(b, sizeof b, "%2d %-12.12s %s ", c + 1, in == 0xFF ? "" : t1m_inst_name(in), note_s(n, nb));
        put(x, y, b, A(t1m.ch[c].mute ? RED : WHITE, BLACK));
        for (uint8_t k = 0; k < 16; k++)
            txt[y * 80 + x + 21 + k] = (uint16_t)((k < bar ? A(k < 10 ? GREEN : k < 13 ? YELLOW : RED, BLACK) : A(BLUE, BLACK)) | (k < bar ? 0xDB : 0xFA));
    }
}

int main(void)
{
    const void *mod = song_t1m;
    if (MODULE_AT[0] == 0x1A4D3154UL) mod = MODULE_AT;
    printf("t1mplay: T1M modules on the microcoded audio engine\n");
    if (MC_ID != 0xA0C2) {
        printf("This needs the microcoded engine's bitstream (cd tang && make mcaudio prog-mc).\n");
        return 1;
    }
    int e = t1m_load(mod);
    if (e) {
        printf("t1mplay: %s\n", t1m_error(e));
        return 1;
    }
    printf("%s by %s: %d channels, up to %d engine voices\n", t1m.title, t1m.author, t1m.channels, t1m.voices);
    memset(txt, 0, sizeof txt);
    VID_TXT_L = (uint16_t)(uint32_t)txt;
    VID_TXT_H = (uint16_t)((uint32_t)txt >> 16);
    VID_CTRL = 1;                       /* text only */
    t1m_start();
    irq_on(IRQ_AUDIO, on_block);
    irq_enable(1);
    uint8_t pause = 0;
    for (;;) {
        uint16_t f = FRAME;
        draw();
        while (FRAME == f) ;
        int16_t k = (int16_t)CON_IN;
        if (k == 'q' || k == 27) break;
        if (k == ' ') t1m_pause(pause ^= 1);
        if (k == 'n') t1m_goto((uint8_t)(t1m.order + 1));
        if (k == 'p') t1m_goto((uint8_t)(t1m.order ? t1m.order - 1 : 0));
        if (k >= '0' && k <= '9') {
            uint8_t c = (uint8_t)(k == '0' ? 9 : k - '1');
            if (c < t1m.channels) t1m.ch[c].mute ^= 1;
        }
    }
    irq_enable(0);
    MC_CTRL = 0;
    return 0;
}
