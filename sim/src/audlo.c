/* fpga_cpu simulator: the low tier's sound (fpga/rtl/audio_lite.v, the
 * Spartan-3), bit exact: the 3-voice device at F00400 and the streaming
 * player at F00420, an output frame every 1024 cycles of 50 MHz.
 *
 * Frames are made lazily, up to the current cycle, whenever the program
 * touches the device and at exit. The registers a frame uses are those
 * written before it starts (the hardware reads them early in the frame).
 * --wav FILE writes the output (16-bit stereo, 48828 Hz). */
#include "cpu.h"

#include <string.h>

#define FRAME_CYCLES 1024u

static struct {
    uint16_t regs[32];         /* words 0x400-0x43E */
    uint32_t ph[3];            /* 24-bit phases */
    uint16_t nz[3];            /* 15-bit LFSRs */
    int play, div;
    uint16_t pos;
    uint32_t word, wordh;
    int16_t mixl, mixr;
    uint64_t frames;           /* made so far */
    FILE *wav;
    uint32_t wav_frames;
} A = { .nz = { 1, 1, 1 } };

static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t rate = 48828, bytes = frames * 4;
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                      16, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 16, 0,
                      'd', 'a', 't', 'a', 0, 0, 0, 0 };
    uint32_t v[3] = { 36 + bytes, rate, rate * 4 };
    memcpy(h + 4, &v[0], 4);
    memcpy(h + 24, &v[1], 4);
    memcpy(h + 28, &v[2], 4);
    memcpy(h + 40, &bytes, 4);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

void audlo_wav(const char *path)
{
    A.wav = fopen(path, "wb");
    if (!A.wav) {
        perror(path);
        return;
    }
    wav_header(A.wav, 0);
}

static int16_t sat16(int32_t x) { return (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }

static void frame(const cpu_t *c)
{
    /* the output this frame is last frame's mix (as the I2S registers) */
    if (A.wav) {
        int16_t s[2] = { A.mixl, A.mixr };
        fwrite(s, 2, 2, A.wav);
        A.wav_frames++;
    }
    A.wordh = A.word;
    int32_t acc = 0;
    for (int v = 0; v < 3; v++) {
        uint16_t freq = A.regs[4 * v], vol = A.regs[4 * v + 1] & 15, wave = A.regs[4 * v + 2] & 3;
        uint32_t step = (uint32_t)(((uint64_t)freq * 21990) >> 6) & 0xFFFFFF;
        uint32_t old = A.ph[v];
        A.ph[v] = (old + step) & 0xFFFFFF;
        if (!(old >> 19 & 1) && (A.ph[v] >> 19 & 1))
            A.nz[v] = (uint16_t)(((A.nz[v] << 1) | ((A.nz[v] >> 14 ^ A.nz[v] >> 13) & 1)) & 0x7FFF);
        uint32_t p = A.ph[v];
        int32_t s;
        if (wave == 0 || wave == 3) {
            int neg = wave == 3 ? (A.nz[v] & 1) : (int)(p >> 23 & 1);
            s = neg ? -32767 : 32767;
        } else if (wave == 1) {                       /* triangle, half scale */
            int32_t t = (int32_t)((p >> 8) & 0x7FFF);
            if (p >> 23 & 1) t ^= 0x7FFF;
            s = t - 16384;
        } else {                                      /* saw */
            s = (int32_t)((p >> 8) & 0xFFFF) - 32768;
        }
        int32_t m = s * (int32_t)(vol * 5);
        acc += m >> 8;                                /* arithmetic: floor */
    }
    /* the stream's registers, the fetch and the step */
    uint16_t ctrl = A.regs[16];
    int nplay = ctrl & 1, rdiv = (ctrl >> 1) & 7;
    if (nplay && !A.play) { A.pos = 0; A.div = 0; }
    A.play = nplay;
    if (!A.play) A.word = 0;
    int32_t svol = A.regs[17] & 0x1FF;
    uint32_t base = ((uint32_t)(A.regs[18] >> 2) | (uint32_t)(A.regs[19] & 15) << 14) & 0x3FFFF;
    uint16_t len = A.regs[20];
    if (A.play) {
        uint32_t a = ((base + A.pos) & 0x3FFFF) * 4;
        A.word = a < SRAM_SIZE ? (uint32_t)c->sram[a] | (uint32_t)c->sram[a + 1] << 8 |
                                 (uint32_t)c->sram[a + 2] << 16 | (uint32_t)c->sram[a + 3] << 24 : 0;
        if (A.div == rdiv) {
            A.div = 0;
            A.pos = (uint16_t)(A.pos + 1) == len ? 0 : (uint16_t)(A.pos + 1);
        } else
            A.div++;
    }
    int32_t sl = (int16_t)(A.wordh & 0xFFFF), sr = (int16_t)(A.wordh >> 16);
    A.mixl = sat16(acc + ((sl * svol) >> 8));
    A.mixr = sat16(acc + ((sr * svol) >> 8));
}

void audlo_sync(const cpu_t *c)
{
    uint64_t want = c->st.cycles / FRAME_CYCLES;
    while (A.frames < want) {
        frame(c);
        A.frames++;
    }
}

void audlo_write(cpu_t *c, uint32_t a, uint16_t v)
{
    audlo_sync(c);
    A.regs[(a >> 1) & 31] = v;
}

uint16_t audlo_read(cpu_t *c, uint32_t a)
{
    audlo_sync(c);
    return (a & 0x3E) == 0x2A ? A.pos : 0;
}

void audlo_finish(const cpu_t *c)
{
    audlo_sync(c);
    if (A.wav) {
        wav_header(A.wav, A.wav_frames);
        fclose(A.wav);
        A.wav = NULL;
    }
}
