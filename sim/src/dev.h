/* fpga_cpu simulator: I/O devices (docs/io.md). */
#ifndef DEV_H
#define DEV_H

#include <stdint.h>
#include <stdio.h>

struct cpu;

/* register addresses */
#define IO_CONSOLE   0xF00000u /* W: serial console character */
#define IO_EXIT      0xF00002u /* W: stop the simulation, value = exit code */
#define IO_CYCLO     0xF00004u /* R: cycle counter bits 15:0 */
#define IO_CYCHI     0xF00006u /* R: cycle counter bits 31:16 */
#define IO_GETC      0xF00008u /* R: next serial input character, 0xFFFF if none */
#define IO_FRAME     0xF0000Au /* R: frame counter */
#define IO_VSTAT     0xF0000Cu /* R: bit 0 vblank, bits 15:4 scan line */
#define IO_INSTLO    0xF00010u /* R: instructions retired, bits 15:0 */
#define IO_INSTHI    0xF00012u /* R: instructions retired, bits 31:16 */
#define IO_KBD_DATA  0xF00100u
#define IO_KBD_MOD   0xF00102u
#define IO_KBD_CTRL  0xF00104u /* RW: bit 0 = also queue key releases (code | 0x8000) */
#define KBD_RELEASE  0x8000u
#define IO_VID       0xF00200u /* video registers F00200-F0021F */
#define IO_VIDX      0xF00220u /* video extensions F00220-F00235 (high tier) */
#define IO_BLT       0xF00800u /* blitter F00800-F00817 (high tier) */
#define IO_SND       0xF00400u /* 3 voices x 8 bytes */
#define IO_FS        0xF00500u /* file service F00500-F0051B */
#define IO_BOARD     0xF00600u /* board I/O F00600-F00609 */
#define IO_PS2       0xF00300u /* PS/2 ports: +0 A DATA, +2 A STAT, +4 B DATA, +6 B STAT */
#define IO_IRQ       0xF00700u /* interrupts: +0 IRQ_PEND (R; W 1s clear), +2 IRQ_EN, +4 IRQ_SET (W) */
#define IRQ_VBLANK   0x0001u   /* vertical blank started (FRAME moved) */
#define IRQ_SOFT     0x0080u   /* set by IRQ_SET only (the controller is 8 bits) */

/* video register indices (word offset from IO_VID) */
enum { V_CTRL, V_BACKDROP, V_TEXT_LO, V_TEXT_HI, V_BMP_LO, V_BMP_HI, V_SPR_LO, V_SPR_HI,
       V_PAL_INDEX, V_PAL_DATA, V_SCROLLX, V_SCROLLY, V_CURSOR, V__COUNT };
#define VC_TEXT   0x01
#define VC_BMP    0x02
#define VC_HICOL  0x04
#define VC_SPR    0x08
#define VC_SPRBUF 0x10
#define VC_CURSOR 0x20
#define VC_SPR8   0x40 /* sprite buffer is 8 bpp through the palette */
#define VC_C16    0x80 /* bitmap 16-color, direct: 9-15 = 1-7 at half brightness, 8 transparent */

/* video extension register indices (word offset from IO_VIDX) */
enum { VX_CTRL2, VX_STAT, VX_BMPB_LO, VX_BMPB_HI, VX_SCROLLXB, VX_SCROLLYB, VX_LSCR_LO, VX_LSCR_HI,
       VX_SPR_INDEX, VX_SPR_DATA, VX_SPR_OVER, VX__COUNT };
#define VC2_PFB    0x01 /* playfield B on */
#define VC2_BFRONT 0x02 /* playfield B in front of A */
#define VC2_LSCR   0x04 /* line scroll */
#define VC2_HWSPR  0x08 /* hardware sprites */
#define VC2_SPRBEH 0x10 /* hardware sprites behind the sprite buffer */
/* blitter register indices (word offset from IO_BLT) */
enum { BL_SRC_LO, BL_SRC_HI, BL_DST_LO, BL_DST_HI, BL_SSTRIDE, BL_DSTRIDE, BL_W, BL_H, BL_KEY, BL_FILL,
       BL_CMD, BL_STAT, BL__COUNT };

/* frame timing: 50 MHz, 750 lines of which 720 are visible, 60 frames/s */
#define CPU_HZ        50000000u    /* the default; dev_cpu_hz is the clock in use (fsim --hw) */
#define FRAME_LINES   750u
#define VISIBLE_LINES 720u

#define KBD_QLEN 256
#define CON_QLEN 4096
#define FS_HANDLES 8

typedef struct iodev {
    /* serial console */
    FILE *con_out;                /* CON_OUT also goes here (stdout headless) */
    void (*con_hook)(void *, int); /* GUI: copy of every CON_OUT character */
    void *con_hook_arg;
    int con_stdin;                /* 1: CON_IN reads stdin (headless) */
    int con_eof;                  /* stdin has ended */
    uint16_t con_q[CON_QLEN];
    unsigned con_head, con_tail;
    /* keyboard */
    uint16_t kbd_q[KBD_QLEN];
    unsigned kbd_head, kbd_tail;
    uint16_t kbd_mod;
    uint16_t kbd_ctrl;
    /* video */
    uint16_t vid[V__COUNT];
    /* interrupts */
    uint16_t irq_pend, irq_en;
    uint32_t irq_frame;        /* the frame the vertical-blank source last saw */
    uint16_t pal[256];
    uint16_t vidx[VX__COUNT];     /* video extensions (high tier) */
    uint16_t spr_attr[256];       /* hardware sprites: 64 x 4 words */
    uint16_t blt[BL__COUNT];      /* blitter */
    /* sound */
    volatile uint16_t snd[3][4];
    double snd_phase[3];
    uint32_t snd_lfsr[3];
    float snd_noise[3];
    /* files */
    char disk[1024];
    FILE *fh[FS_HANDLES];
    uint16_t fs_handle, fs_mode, fs_len;
    int16_t fs_result;
    uint32_t fs_name, fs_buf, fs_size, fs_pos;
    char **dir_list;
    int dir_n, dir_pos;
    /* board */
    uint16_t leds, switches, buttons, seg7, seg7_ctrl;
} iodev_t;

void dev_init(iodev_t *d);
void dev_reset(iodev_t *d);   /* power-on register state; keeps disk, hooks and inputs */
void dev_free(iodev_t *d);
uint16_t dev_read(struct cpu *c, uint32_t a);
void dev_write(struct cpu *c, uint32_t a, uint32_t v);
void dev_key(iodev_t *d, uint16_t code);   /* push a key into the keyboard queue */
void dev_key_up(iodev_t *d, uint16_t code); /* a key was released: queued only if KBD_CTRL bit 0 */
void dev_con_in(iodev_t *d, uint16_t ch);  /* push a character into CON_IN */
extern uint32_t dev_cpu_hz;            /* cycles a second: video and sound timing (CPU_HZ unless fsim --hw) */
uint32_t dev_frame(uint64_t cycles);
/* the interrupt line now (pending and enabled sources), updating the sources */
int dev_irq(struct cpu *c);
uint32_t dev_line(uint64_t cycles);
/* vidx.c: the high tier's video extensions and blitter */
uint16_t vidx_read(struct cpu *c, uint32_t a);
int vidx_write(struct cpu *c, uint32_t a, uint16_t v);
void vidx_buffer_written(const struct cpu *c);
void vidx_reset(void);
int vidx_flip_pending(const struct cpu *c);

/* Video: render the composed 1280x720 frame as 0x00RRGGBB pixels. */
#define SCREEN_W 1280
#define SCREEN_H 720
void video_render(const struct cpu *c, uint32_t *px, int true_color, int cursor_on);
/* the low tier's sound (audlo.c, fpga/rtl/audio_lite.v): F00400-F0043F with --no-hicolor */
void audlo_write(struct cpu *c, uint32_t a, uint16_t v);
uint16_t audlo_read(struct cpu *c, uint32_t a);
void audlo_finish(const struct cpu *c);
void audlo_wav(const char *path);
void audlo_reset(void);
/* the PS/2 ports (ps2.c, fpga/rtl/ps2.v): F00300-F00307, a keyboard and a mouse */
int ps2_handles(uint32_t a);
uint16_t ps2_read(struct cpu *c, uint32_t a);
void ps2_write(struct cpu *c, uint32_t a, uint16_t v);
void ps2_poll(struct cpu *c);             /* bytes arrive, sends end (interrupt sources 1, 2) */
void ps2_reset(void);
int ps2_config(const char *s);            /* --ps2 A,B: kbd, mouse or none each */
void ps2_script(int mouse, const char *s); /* --ps2keys, --ps2mouse */
void ps2_key(int usage, int down);        /* a key (USB HID usage = SDL scancode) */
void ps2_mouse(int dx, int dy, int buttons);
/* 0: the low video tier, the Spartan-3 (--no-hicolor): no hi-color, no palette */
extern int video_hicol8;
int write_ppm(const char *path, const uint32_t *px, int w, int h);

/* Sound: fill n mono float samples at the given rate from the voice registers. */
void sound_fill(iodev_t *d, float *out, int n, int rate);

#endif
