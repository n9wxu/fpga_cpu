/* Video, sound and the blitter (docs/io.md, docs/audio.md). The video and the audio engine's
 * configuration registers are write-only, so the BIOS keeps the copies programs ask for. */
#include "rom.h"

/* ---- video */
#define TEXT_DEF 0x0FC000UL             /* the boot layout (as crt0's) */
static uint16_t ctrl, ctrl2;
static uint32_t text_a = TEXT_DEF;

void vid_ctrl(uint16_t v) { ctrl = v; VID_CTRL = v; }
uint16_t vid_ctrl_get(void) { return ctrl; }
void vid_ctrl2(uint16_t v) { ctrl2 = v; VID_CTRL2 = v; }
uint16_t vid_ctrl2_get(void) { return ctrl2; }
uint32_t vid_text_addr(void) { return text_a; }
void vid_text(uint32_t a) { text_a = a; VID_TEXT_LO = (uint16_t)a; VID_TEXT_HI = (uint16_t)(a >> 16); }
void vid_bmp(uint32_t a) { VID_BMP_LO = (uint16_t)a; VID_BMP_HI = (uint16_t)(a >> 16); }
void vid_spr(uint32_t a) { VID_SPR_LO = (uint16_t)a; VID_SPR_HI = (uint16_t)(a >> 16); }
void vid_scroll(uint16_t x, uint16_t y) { VID_SCROLLX = x; VID_SCROLLY = y; }
void vid_backdrop(uint16_t c) { VID_BACKDROP = (uint16_t)(c & 7); }

void vid_palette(uint16_t first, const uint16_t *rgb, uint16_t n)
{
    PAL_INDEX = first;
    while (n--) PAL_DATA = *rgb++;      /* PAL_DATA advances PAL_INDEX */
}

void vid_wait_vbl(void)
{
    while (VSTAT & 1) ;                 /* out of a blank under way */
    while (!(VSTAT & 1)) ;
}

void vid_reset(void)
{
    vid_ctrl2(0);
    vid_text(TEXT_DEF);
    vid_bmp(0x0C3C00UL);
    vid_spr(0x053400UL);
    vid_scroll(0, 0);
    vid_backdrop(0);
    vid_ctrl(0x21);                     /* text layer and cursor */
}

void vid_init(void) { vid_reset(); }

/* ---- sound: voices of the audio engine */
enum { V_CTRL, V_STEP_LO, V_STEP_HI, V_VOL_L, V_VOL_R, V_DUTY, V_ATTACK, V_DECAY, V_SUSTAIN, V_RELEASE };
#define C_GATE  1
#define C_ENV   2
static uint16_t vctl[32];
static uint8_t snd_ok;

void snd_ctrl(uint16_t v, uint16_t c) { if (v < 32) { vctl[v] = c; AUD_V(v, V_CTRL) = c; } }
uint16_t snd_ctrl_get(uint16_t v) { return v < 32 ? vctl[v] : 0; }
void snd_master(uint16_t m) { AUD_G(0) = m; }

void snd_init(void)
{
    snd_ok = AUD_G(0x14) == 0xA032;     /* the engine's ID */
    snd_quiet();
    if (snd_ok) snd_master(0x1000);
}

void snd_quiet(void)
{
    if (!snd_ok) return;
    for (uint16_t v = 0; v < 32; v++) snd_ctrl(v, 0);
}

/* wave: 1 pulse, 2 triangle, 3 saw, 4 noise; vol 0-255; an envelope with a short attack and
 * release (snd_off releases) */
void snd_tone(uint16_t v, uint16_t hz, uint16_t vol, uint16_t wave)
{
    if (!snd_ok || v >= 32) return;
    uint32_t step = (uint32_t)hz * 349u + (((uint32_t)hz * 134u) >> 8);   /* hz * 2^24 / 48000 */
    AUD_V(v, V_STEP_LO) = (uint16_t)step;
    AUD_V(v, V_STEP_HI) = (uint16_t)(step >> 16);
    AUD_V(v, V_VOL_L) = vol;
    AUD_V(v, V_VOL_R) = vol;
    AUD_V(v, V_DUTY) = 0x800;
    AUD_V(v, V_ATTACK) = 0x2000;
    AUD_V(v, V_DECAY) = 0x0010;
    AUD_V(v, V_SUSTAIN) = 0xC000;
    AUD_V(v, V_RELEASE) = 0x0100;
    if (vctl[v] & C_GATE) snd_ctrl(v, (uint16_t)(vctl[v] & ~C_GATE));   /* a new attack */
    snd_ctrl(v, (uint16_t)((wave & 7) << 2 | C_ENV | C_GATE));
}

void snd_off(uint16_t v) { if (snd_ok && v < 32) snd_ctrl(v, (uint16_t)(vctl[v] & ~C_GATE)); }

void snd_beep(uint16_t hz, uint16_t ms)
{
    snd_tone(31, hz, 0x60, 1);
    sys_delay(ms);
    snd_off(31);
}

/* ---- blitter (F00800, docs/io.md) */
enum { B_SRC = 0, B_DST = 4, B_SS = 8, B_DS = 10, B_W = 12, B_H = 14, B_KEY = 16, B_FILL = 18, B_CMD = 20, B_STAT = 22 };

void blt_wait(void) { while (BLT(B_STAT) & 1) ; }

static void blt_go(uint32_t s, uint32_t d, int16_t ss, int16_t ds, uint16_t w, uint16_t h, uint16_t cmd)
{
    blt_wait();
    BLT(B_SRC) = (uint16_t)s; BLT(B_SRC + 2) = (uint16_t)(s >> 16);
    BLT(B_DST) = (uint16_t)d; BLT(B_DST + 2) = (uint16_t)(d >> 16);
    BLT(B_SS) = (uint16_t)ss; BLT(B_DS) = (uint16_t)ds;
    BLT(B_W) = w; BLT(B_H) = h;
    BLT(B_CMD) = cmd;
}

void blt_copy(uint32_t s, uint32_t d, int16_t ss, int16_t ds, uint16_t w, uint16_t h)
{
    blt_go(s, d, ss, ds, w, h, 0);
}

void blt_keycopy(uint32_t s, uint32_t d, int16_t ss, int16_t ds, uint16_t w, uint16_t h, uint8_t key)
{
    blt_wait();
    BLT(B_KEY) = key;
    blt_go(s, d, ss, ds, w, h, 2);
}

void blt_fill(uint32_t d, int16_t ds, uint16_t w, uint16_t h, uint8_t v)
{
    blt_wait();
    BLT(B_FILL) = v;
    blt_go(0, d, 0, ds, w, h, 1);
}
