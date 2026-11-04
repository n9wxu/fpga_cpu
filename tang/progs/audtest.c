/* The audio engine on tang/rtl/system.v (docs/audio.md): its ID, the
 * sample counter, a chip voice's envelope, and a one-shot sample voice
 * read from SDRAM through rammux; then a few seconds of a chord (pulse,
 * triangle, saw) and a sampled noise burst in the TV's speakers.
 * -DQUICK: the checks only (for simulation). Results on the console;
 * the exit code is the number of failures. */
#include <stdint.h>
#include <fcpu.h>

#define G(o)      IO16(0xF01000 + (o))
#define V(v, w)   IO16(0xF01800 + 64 * (v) + 2 * (w))
#define MASTER 0x00
#define SCOUNT_LO 0x10
#define SCOUNT_HI 0x12
#define ID     0x14
#define UNDERRUNS 0x18
enum { CTRL, STEP_LO, STEP_HI, VOL_L, VOL_R, DUTY, ATTACK, DECAY, SUSTAIN, RELEASE,
       NOISE, POS_F, POS_L, POS_H, STATUS, ENV, ADDR_LO, ADDR_HI, LOOP_LO, LOOP_HI, END_LO, END_HI };

#define SAMPLE ((volatile int8_t *)0x080000)  /* in SDRAM */
#define SLEN 4800

static void con(const char *s) { while (*s) CON_OUT = (uint8_t)*s++; }
static void hex(uint32_t v, int d)
{
    while (d--) CON_OUT = (uint8_t)"0123456789ABCDEF"[(v >> (4 * d)) & 15];
}
static uint32_t scount(void)
{
    uint16_t lo = G(SCOUNT_LO);
    return (uint32_t)G(SCOUNT_HI) << 16 | lo;
}
static void wait_frames(uint32_t n)
{
    uint32_t t = scount() + n;
    while ((int32_t)(scount() - t) < 0) ;
}
static int fails;
static void check(const char *what, int ok, uint32_t v)
{
    con(what); con(ok ? ": ok " : ": FAIL "); hex(v, 8); con("\n");
    if (!ok) fails++;
}
static void chip(int v, int src, uint32_t hz, int vl, int vr)
{
    uint32_t step = (uint32_t)(((uint64_t)hz << 24) / 48000);
    V(v, STEP_LO) = (uint16_t)step; V(v, STEP_HI) = (uint16_t)(step >> 16);
    V(v, VOL_L) = vl; V(v, VOL_R) = vr; V(v, DUTY) = 0x400;
    V(v, ATTACK) = 0x0800; V(v, DECAY) = 0x0040; V(v, SUSTAIN) = 0x8000; V(v, RELEASE) = 0x0080;
    V(v, CTRL) = (src << 2) | 3;            /* SRC, ENVEN, GATE */
}

int main(void)
{
    con("audtest\n");
    check("ID", G(ID) == 0xA032, G(ID));
    uint32_t a = scount();
    wait_frames(48);
    uint32_t b = scount();
    check("SCOUNT advances", b - a >= 48 && b - a < 200, b - a);

    G(MASTER) = 0x1000;
    chip(0, 1, 220, 0x60, 0x60);            /* pulse */
    wait_frames(240);                       /* 5 ms: the attack is under way */
    check("pulse envelope", V(0, ENV) > 0x1000, V(0, ENV));

    /* a one-shot 8-bit sample from SDRAM: a pseudo-random burst */
    uint16_t x = 1;
    for (int i = 0; i < SLEN; i++) { x = (uint16_t)(x * 75 + 74); SAMPLE[i] = (int8_t)(x >> 8) / 4; }
    V(1, ADDR_LO) = 0x0000; V(1, ADDR_HI) = 0x08;
    V(1, LOOP_LO) = 0; V(1, LOOP_HI) = 0; V(1, END_LO) = SLEN; V(1, END_HI) = 0;
    V(1, STEP_LO) = 0; V(1, STEP_HI) = 1;   /* 1.0: 48 kHz */
    V(1, VOL_L) = 0x80; V(1, VOL_R) = 0x80;
    V(1, POS_F) = 0; V(1, POS_L) = 0; V(1, POS_H) = 0;   /* arms it: the first words are fetched */
    wait_frames(2);                         /* (a gate at once would hold a frame: an underrun) */
    V(1, CTRL) = 1;                         /* sample, gate, no envelope, one-shot */
    wait_frames(SLEN / 2);
    uint16_t pos = V(1, POS_L);
    check("sample plays", pos > SLEN / 4 && pos < SLEN, pos);
    wait_frames(SLEN);
    check("sample done", (V(1, STATUS) & 2) != 0, V(1, STATUS));
    check("no underruns", G(UNDERRUNS) == 0, G(UNDERRUNS));

#ifndef QUICK
    chip(2, 2, 277, 0x60, 0x20);            /* triangle, left */
    chip(3, 3, 330, 0x20, 0x60);            /* saw, right */
    wait_frames(48000 * 2);
    V(1, CTRL) = 0; V(1, POS_F) = 0; V(1, POS_L) = 0; V(1, POS_H) = 0;
    wait_frames(2);
    V(1, CTRL) = 1;
    wait_frames(48000);
    for (int v = 0; v < 4; v++) V(v, CTRL) = V(v, CTRL) & ~1;   /* release */
    wait_frames(48000);
#endif
    for (int v = 0; v < 4; v++) V(v, CTRL) = 0;
    con(fails ? "FAILED\n" : "all ok\n");
    SIM_EXIT = (uint16_t)fails;
    return fails;
}
