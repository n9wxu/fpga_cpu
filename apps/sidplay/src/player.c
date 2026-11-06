/* Runs a tune's init and play routines on the 6502 recompiler and collects each call's SID state. */
#include <string.h>
#include "sid6502.h"
#include "sidplay.h"

uint8_t pl_clock_ntsc;
uint32_t pl_rate_x100 = 5000;
uint32_t pl_period48 = 960;
uint32_t pl_clk_hz = 985248;
#ifndef PL_JIT
#define PL_JIT 1
#endif
uint8_t pl_jit = PL_JIT;
uint32_t pl_instrs;

static uint8_t regs[32];                        /* the SID registers as the tune last wrote them */
static uint8_t last_ctrl[3];
uint32_t pl_cyc_per_call;                       /* 6502 cycles per play call */
#define cyc_per_call pl_cyc_per_call
static uint32_t next_cyc;                       /* idle-time target for cpu.cycles */
static uint16_t play_addr;
static uint8_t song_lo;
void (*pl_log_hook)(void);

/* apply the call's writes (sidlog) to the register shadow and look for gate edges */
static void drain(frame_t *f)
{
    uint16_t i;
    f->rise = f->fall = f->wr = 0;
    for (i = 0; i < sid_n; i++) {
        uint8_t reg = sidlog[i].reg, val = sidlog[i].val;
        if (reg < 25) {
            regs[reg] = val;
            if (reg < 21) {
                uint8_t v = reg / 7, k = reg % 7;
                if (k < 5)
                    f->wr |= 1 << v;
                if (k == 4) {
                    uint8_t o = last_ctrl[v];
                    if (!(o & 1) && (val & 1))
                        f->rise |= 1 << v;
                    if ((o & 1) && !(val & 1))
                        f->fall |= 1 << v;
                    last_ctrl[v] = val;
                }
            }
        }
    }
    if (pl_log_hook)
        pl_log_hook();
    sid_log_clear();
    memcpy(f->r, regs, 25);
}

int pl_start(const psid_t *p, int song)
{
    int st;
    uint32_t spd_bit = (p->speed >> (song - 1 > 31 ? 31 : song - 1)) & 1;
    uint8_t ntsc = ((p->flags >> 2) & 3) == 2;
    uint16_t ct;
    pl_clock_ntsc = ntsc;
    pl_clk_hz = ntsc ? 1022727UL : 985248UL;
    cyc_per_call = ntsc ? 17045UL : 19656UL;
    pl_rate_x100 = ntsc ? 6000 : 5000;
    memset(regs, 0, sizeof regs);
    memset(last_ctrl, 0, sizeof last_ctrl);
    sid_reset();
    sid_load(p->load, p->data, p->dlen);
    play_addr = p->play;
    song_lo = (uint8_t)(song - 1);
    cpu.s = 0xFF;
    cpu.i = 1;
    st = call_routine(p->init, song_lo, 0, 0, 3000000UL, pl_jit);
    {
        frame_t dummy;
        drain(&dummy);                          /* init's SID writes are part of the state */
    }
    {   /* CIA-timed tunes (speed bit set) program timer A; 60 Hz otherwise */
        ct = (uint16_t)(mem6502[0xDC04] + mem6502[0xDC05] * 256u);
        if (spd_bit) {
            if (ct >= 2000 && ct < 60000u) {
                cyc_per_call = (uint32_t)ct + 1;            /* the timer counts system clocks */
                pl_rate_x100 = pl_clk_hz * 100UL / cyc_per_call;
            } else {
                cyc_per_call = ntsc ? 17045UL : 16421UL;    /* 60 Hz */
                pl_rate_x100 = 6000;
            }
        }
    }
    pl_period48 = 48000UL * cyc_per_call / pl_clk_hz;
    next_cyc = cpu.cycles;
    return st;
}

int pl_frame(frame_t *f)
{
    int st;
    uint32_t c0;
    next_cyc += cyc_per_call;
    if (cpu.cycles < next_cyc)
        cpu.cycles = next_cyc;
    cpu.s = 0xFF;
    c0 = cpu.cycles;
    st = call_routine(play_addr, 0, 0, 0, 60000UL, pl_jit);
    pl_instrs = cpu.cycles - c0;
    drain(f);
    return st;
}
