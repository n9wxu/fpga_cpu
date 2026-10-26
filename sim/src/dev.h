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
#define IO_KBD_DATA  0xF00100u
#define IO_KBD_MOD   0xF00102u
#define IO_VID       0xF00200u /* video registers F00200-F0021F */
#define IO_SND       0xF00400u /* 3 voices x 8 bytes */
#define IO_FS        0xF00500u /* file service F00500-F00517 */
#define IO_BOARD     0xF00600u /* board I/O F00600-F00609 */

/* video register indices (word offset from IO_VID) */
enum { V_CTRL, V_BACKDROP, V_TEXT_LO, V_TEXT_HI, V_BMP_LO, V_BMP_HI, V_SPR_LO, V_SPR_HI,
       V_PAL_INDEX, V_PAL_DATA, V_SCROLLX, V_SCROLLY, V_CURSOR, V__COUNT };
#define VC_TEXT   0x01
#define VC_BMP    0x02
#define VC_HICOL  0x04
#define VC_SPR    0x08
#define VC_SPRBUF 0x10
#define VC_CURSOR 0x20

/* frame timing: 50 MHz, 750 lines of which 720 are visible, 60 frames/s */
#define CPU_HZ        50000000u
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
    /* video */
    uint16_t vid[V__COUNT];
    uint16_t pal[256];
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
    uint32_t fs_name, fs_buf, fs_size;
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
void dev_con_in(iodev_t *d, uint16_t ch);  /* push a character into CON_IN */
uint32_t dev_frame(uint64_t cycles);
uint32_t dev_line(uint64_t cycles);

/* Video: render the composed 1280x720 frame as 0x00RRGGBB pixels. */
#define SCREEN_W 1280
#define SCREEN_H 720
void video_render(const struct cpu *c, uint32_t *px, int true_color, int cursor_on);
int write_ppm(const char *path, const uint32_t *px, int w, int h);

/* Sound: fill n mono float samples at the given rate from the voice registers. */
void sound_fill(iodev_t *d, float *out, int n, int rate);

#endif
