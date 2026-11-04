/* The MOD file and the replayer: ProTracker 2 behaviour on four audio engine voices (docs/audio.md: "A MOD note").
 * The samples play straight from the file in SDRAM (8-bit signed). */
#include <string.h>
#include "mod.h"
#include "tables.h"

#define IO(a) (*(volatile uint16_t *)(0xF00000UL + (a)))
#define VREG(c, w) IO(0x1800 + 64 * (c) + 2 * (w))
#define V_CTRL 0
#define V_STEP_LO 1
#define V_STEP_HI 2
#define V_VOL_L 3
#define V_VOL_R 4
#define V_POS_F 11
#define V_POS_L 12
#define V_POS_H 13
#define V_ADDR_LO 16
#define V_ADDR_HI 17
#define V_LOOP_LO 18
#define V_LOOP_HI 19
#define V_END_LO 20
#define V_END_HI 21
#define CTRL_GATE 1
#define CTRL_INTERP 0x80
#define CTRL_LOOP 0x100

#define PAL_STEP 4842694UL              /* step (8.16) = this / period, for 48 kHz out (PAL Amiga clock) */
#define P_MIN 113
#define P_MAX 856

mod_t mod;
chan_t ch[NCH];
play_t pl;

static uint8_t next_pos, next_row, jump;    /* a row change after this row: B, D, E6 */
static uint8_t in_delay;                    /* repeating a row for EEx */
static uint32_t e_step[NCH];                /* what the engine was last given */
static uint16_t e_vl[NCH], e_vr[NCH];
static uint16_t e_ctrl[NCH];

/* ---------------------------------------------------------------- the file */
static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

/* channels from the signature, 0 = not a MOD we play */
static uint8_t sig_nch(const uint8_t *b)
{
    const uint8_t *s = b + 1080;
    if (!memcmp(s, "M.K.", 4) || !memcmp(s, "M!K!", 4) || !memcmp(s, "FLT4", 4) || !memcmp(s, "4CHN", 4))
        return 4;
    if (!memcmp(s, "6CHN", 4))
        return 6;
    if (!memcmp(s, "8CHN", 4))
        return 8;
    return 0;
}

uint32_t mod_size(uint32_t base)
{
    const uint8_t *b = (const uint8_t *)base;
    uint32_t size;
    uint8_t i, npat = 0, n = sig_nch(b);
    if (!n)
        return 0;
    for (i = 0; i < 128; i++)
        if (b[952 + i] >= npat)
            npat = (uint8_t)(b[952 + i] + 1);
    size = 1084UL + (uint32_t)npat * 256UL * n;
    for (i = 0; i < 31; i++)
        size += (uint32_t)be16(b + 20 + 30 * i + 22) * 2UL;
    return size;
}

int mod_parse(uint32_t base, mod_t *m)
{
    const uint8_t *b = (const uint8_t *)base;
    uint32_t a;
    uint8_t i;
    if (!sig_nch(b))
        return 1;
    memset(m, 0, sizeof *m);
    m->nch = sig_nch(b);
    m->base = base;
    m->size = mod_size(base);
    for (i = 0; i < 20 && b[i]; i++)
        m->title[i] = (char)b[i];
    m->songlen = b[950];
    m->restart = b[951];
    if (m->songlen == 0 || m->songlen > 128)
        return 2;
    memcpy(m->order, b + 952, 128);
    for (i = 0; i < 128; i++)
        if (m->order[i] >= m->npat)
            m->npat = (uint8_t)(m->order[i] + 1);
    m->pat = base + 1084;
    a = m->pat + (uint32_t)m->npat * 256UL * m->nch;
    for (i = 1; i <= 31; i++) {
        const uint8_t *h = b + 20 + 30 * (i - 1);
        sample_t *s = &m->smp[i];
        uint8_t k;
        for (k = 0; k < 22 && h[k]; k++)
            s->name[k] = (char)(h[k] >= 32 && h[k] < 127 ? h[k] : '.');
        s->len = (uint32_t)be16(h + 22) * 2UL;
        s->finetune = (int8_t)(h[24] & 15);
        if (s->finetune > 7)
            s->finetune = (int8_t)(s->finetune - 16);
        s->vol = h[25] > 64 ? 64 : h[25];
        s->rep = (uint32_t)be16(h + 26) * 2UL;
        s->replen = (uint32_t)be16(h + 28) * 2UL;
        if (s->replen <= 2 || s->rep >= s->len)
            s->replen = 0;                      /* no loop */
        else if (s->rep + s->replen > s->len)
            s->replen = s->len - s->rep;
        s->addr = a;
        a += s->len;
    }
    return 0;
}

const char *mod_err(int e)
{
    return e == 1 ? "NOT A 4, 6 OR 8-CHANNEL MOD" : e == 2 ? "BAD SONG LENGTH" : "?";
}

void mod_cell(uint8_t pattern, uint8_t row, uint8_t c, cell_t *out)
{
    uint16_t rb = (uint16_t)mod.nch * 4;          /* bytes a row */
    const uint8_t *p = (const uint8_t *)(mod.pat + (uint32_t)pattern * 64UL * rb + (uint32_t)row * rb + c * 4);
    out->sample = (uint8_t)((p[0] & 0xF0) | (p[2] >> 4));
    out->period = (uint16_t)((uint16_t)(p[0] & 15) << 8 | p[1]);
    out->fx = (uint8_t)(p[2] & 15);
    out->arg = p[3];
}

uint8_t mod_note_of(uint16_t period)
{
    uint8_t i, best = 0;
    uint16_t bd = 0xFFFF;
    if (!period)
        return 0xFF;
    for (i = 0; i < 36; i++) {
        uint16_t d = periods[0][i] > period ? periods[0][i] - period : period - periods[0][i];
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

static uint8_t note_in(uint16_t period, int8_t ft)       /* nearest note in a finetune's row */
{
    const uint16_t *r = periods[ft & 15];
    uint8_t i;
    for (i = 0; i < 35; i++)
        if (period >= r[i])
            return i;
    return 35;
}

/* ---------------------------------------------------------------- the engine */
static void eng_out(uint8_t c)
{
    chan_t *h = &ch[c];
    uint32_t step = h->out_period ? PAL_STEP / h->out_period : 0;
    uint16_t v = (pl.mute >> c & 1) ? 0 : (uint16_t)h->out_vol * 4;
    uint16_t pan = h->pan, vl, vr;
    uint32_t t;
    if (step != e_step[c]) {
        e_step[c] = step;
        VREG(c, V_STEP_LO) = (uint16_t)step;
        VREG(c, V_STEP_HI) = (uint16_t)(step >> 16);
    }
    /* equal gain in the middle: each side up to v */
    t = (uint32_t)v * (256 - pan) >> 7;
    vl = t > v ? v : (uint16_t)t;
    t = (uint32_t)v * pan >> 7;
    vr = t > v ? v : (uint16_t)t;
    if (vl != e_vl[c]) {
        e_vl[c] = vl;
        VREG(c, V_VOL_L) = vl;
    }
    if (vr != e_vr[c]) {
        e_vr[c] = vr;
        VREG(c, V_VOL_R) = vr;
    }
}

static void voice_off(uint8_t c)
{
    e_ctrl[c] = 0;
    VREG(c, V_CTRL) = 0;
}

/* start the channel's sample from offset (bytes) */
static void trigger(uint8_t c, uint32_t offset)
{
    chan_t *h = &ch[c];
    const sample_t *s = &mod.smp[h->sample];
    uint16_t ctrl;
    uint32_t end;
    h->trig = 1;
    if (!h->sample || s->len <= 2 || offset >= s->len) {
        voice_off(c);
        return;
    }
    end = s->replen ? s->rep + s->replen : s->len;
    ctrl = (uint16_t)((s->replen ? CTRL_LOOP : 0) | (pl.interp ? CTRL_INTERP : 0));
    VREG(c, V_CTRL) = ctrl;                     /* gate off */
    VREG(c, V_ADDR_LO) = (uint16_t)s->addr;
    VREG(c, V_ADDR_HI) = (uint16_t)(s->addr >> 16);
    VREG(c, V_LOOP_LO) = (uint16_t)s->rep;
    VREG(c, V_LOOP_HI) = (uint16_t)(s->rep >> 16);
    VREG(c, V_END_LO) = (uint16_t)end;
    VREG(c, V_END_HI) = (uint16_t)(end >> 16);
    VREG(c, V_POS_F) = 0;
    VREG(c, V_POS_L) = (uint16_t)offset;
    VREG(c, V_POS_H) = (uint16_t)(offset >> 16);   /* arms the voice */
    e_ctrl[c] = (uint16_t)(ctrl | CTRL_GATE);
    VREG(c, V_CTRL) = e_ctrl[c];
}

void mod_silence(void)
{
    uint8_t c;
    for (c = 0; c < mod.nch; c++) {
        voice_off(c);
        VREG(c, V_VOL_L) = 0;
        VREG(c, V_VOL_R) = 0;
        e_vl[c] = e_vr[c] = 0;
        e_step[c] = 0;
    }
}

/* ---------------------------------------------------------------- effects */
static int16_t wave(uint8_t w, uint8_t pos)                  /* -255..255 */
{
    int16_t v;
    switch (w & 3) {
    case 1: v = (int16_t)((pos & 31) * 8); return (pos & 32) ? (int16_t)(v - 255) : (int16_t)(255 - v);  /* ramp down */
    case 2: return (pos & 32) ? -255 : 255;                                                            /* square */
    default: v = vib_sine[pos & 31]; return (pos & 32) ? (int16_t)-v : v;
    }
}

static void vol_slide(chan_t *h, uint8_t arg)
{
    int8_t v = h->vol;
    if (arg >> 4)
        v = (int8_t)(v + (arg >> 4));
    else
        v = (int8_t)(v - (arg & 15));
    h->vol = v < 0 ? 0 : v > 64 ? 64 : v;
}

static void tone_porta(chan_t *h)
{
    if (!h->porta_target)
        return;
    if (h->period < h->porta_target) {
        h->period = (uint16_t)(h->period + h->porta_speed);
        if (h->period > h->porta_target)
            h->period = h->porta_target;
    } else if (h->period > h->porta_target) {
        h->period = h->period > h->porta_speed ? (uint16_t)(h->period - h->porta_speed) : 0;
        if (h->period < h->porta_target)
            h->period = h->porta_target;
    }
}

static void apply_cell(uint8_t c, const cell_t *x)
{
    chan_t *h = &ch[c];
    if (x->sample && x->sample <= 31) {
        h->sample = x->sample;
        h->vol = (int8_t)mod.smp[x->sample].vol;
        h->finetune = mod.smp[x->sample].finetune;
    }
    if (x->period) {
        uint8_t n = mod_note_of(x->period);
        if (x->fx == 0xE && (x->arg >> 4) == 5) {
            h->finetune = (int8_t)(x->arg & 15);
            if (h->finetune > 7)
                h->finetune = (int8_t)(h->finetune - 16);
        }
        if (x->fx == 3 || x->fx == 5) {
            h->porta_target = periods[h->finetune & 15][n];
        } else {
            uint32_t off = 0;
            h->period = periods[h->finetune & 15][n];
            h->note = n;
            if (x->fx == 9) {
                if (x->arg)
                    h->offset = x->arg;
                off = (uint32_t)h->offset << 8;
            }
            if (h->vib_wave < 4)
                h->vib_pos = 0;
            if (h->trem_wave < 4)
                h->trem_pos = 0;
            trigger(c, off);
        }
    }
    if (x->fx == 3 && x->arg)
        h->porta_speed = x->arg;
}

/* tick-0 effects of a row */
static void row_fx(uint8_t c)
{
    chan_t *h = &ch[c];
    uint8_t a = h->arg, hi = a >> 4, lo = a & 15;
    switch (h->fx) {
    case 4:
        if (hi) h->vib_speed = hi;
        if (lo) h->vib_depth = lo;
        break;
    case 7:
        if (hi) h->trem_speed = hi;
        if (lo) h->trem_depth = lo;
        break;
    case 8:
        h->pan = a;
        break;
    case 0xB:
        next_pos = a;
        next_row = 0;
        jump = 1;
        break;
    case 0xC:
        h->vol = a > 64 ? 64 : (int8_t)a;
        break;
    case 0xD:
        if (!jump || next_row == 0) {
            if (!jump)
                next_pos = (uint8_t)(pl.pos + 1);
            next_row = (uint8_t)(hi * 10 + lo);
            if (next_row > 63)
                next_row = 0;
            jump = 1;
        }
        break;
    case 0xE:
        switch (hi) {
        case 1: h->period = h->period > lo + P_MIN ? (uint16_t)(h->period - lo) : P_MIN; break;
        case 2: h->period = h->period + lo < P_MAX ? (uint16_t)(h->period + lo) : P_MAX; break;
        case 4: h->vib_wave = lo; break;
        case 6:
            if (!lo)
                h->loop_row = pl.row;
            else {
                if (!h->loop_cnt)
                    h->loop_cnt = lo;
                else
                    h->loop_cnt--;
                if (h->loop_cnt) {
                    next_pos = pl.pos;
                    next_row = h->loop_row;
                    jump = 1;
                }
            }
            break;
        case 7: h->trem_wave = lo; break;
        case 0xA: vol_slide(h, (uint8_t)(lo << 4)); break;
        case 0xB: vol_slide(h, lo); break;
        case 0xC: if (!lo) h->vol = 0; break;
        case 0xE: if (!in_delay) pl.pat_delay = lo; break;
        }
        break;
    case 0xF:
        if (a == 0)
            break;
        if (a < 32)
            pl.speed = a;
        else
            pl.bpm = a;
        break;
    }
}

/* per-tick effects (ticks 1..speed-1) */
static void tick_fx(uint8_t c)
{
    chan_t *h = &ch[c];
    uint8_t a = h->arg, hi = a >> 4, lo = a & 15;
    switch (h->fx) {
    case 1:
        h->period = h->period > a + P_MIN ? (uint16_t)(h->period - a) : P_MIN;
        break;
    case 2:
        h->period = h->period + a < P_MAX ? (uint16_t)(h->period + a) : P_MAX;
        break;
    case 3:
        tone_porta(h);
        break;
    case 5:
        tone_porta(h);
        vol_slide(h, a);
        break;
    case 4:
    case 6:
        h->vib_pos = (uint8_t)((h->vib_pos + h->vib_speed) & 63);
        if (h->fx == 6)
            vol_slide(h, a);
        break;
    case 7:
        h->trem_pos = (uint8_t)((h->trem_pos + h->trem_speed) & 63);
        break;
    case 0xA:
        vol_slide(h, a);
        break;
    case 0xE:
        if (hi == 9 && lo && pl.tick % lo == 0)
            trigger(c, 0);
        else if (hi == 0xC && pl.tick == lo)
            h->vol = 0;
        else if (hi == 0xD && pl.tick == lo && h->delay_cell) {
            h->delay_cell = 0;
            apply_cell(c, &h->pend);
        }
        break;
    }
}

/* what the engine plays this tick: arpeggio, vibrato, tremolo on top of the channel's period and volume */
static void outputs(uint8_t c)
{
    chan_t *h = &ch[c];
    uint16_t p = h->period;
    int16_t v = h->vol;
    if (h->fx == 0 && h->arg && p) {
        uint8_t k = (uint8_t)(pl.tick % 3);
        if (k) {
            uint8_t n = note_in(p, h->finetune);
            n = (uint8_t)(n + (k == 1 ? h->arg >> 4 : h->arg & 15));
            p = periods[h->finetune & 15][n > 35 ? 35 : n];
        }
    } else if ((h->fx == 4 || h->fx == 6) && p) {
        int32_t d = (int32_t)wave(h->vib_wave, h->vib_pos) * h->vib_depth / 128;
        int32_t q = (int32_t)p + d;
        p = (uint16_t)(q < P_MIN ? P_MIN : q > P_MAX ? P_MAX : q);
    }
    if (h->fx == 7) {
        v = (int16_t)(v + (int16_t)((int32_t)wave(h->trem_wave, h->trem_pos) * h->trem_depth / 64));
        v = v < 0 ? 0 : v > 64 ? 64 : v;
    }
    h->out_period = p;
    h->out_vol = (int8_t)v;
    eng_out(c);
}

/* ---------------------------------------------------------------- the song */
static void read_row(void)
{
    uint8_t c, pat = mod.order[pl.pos];
    pl.ppos = pl.pos;
    pl.prow = pl.row;
    for (c = 0; c < mod.nch; c++) {
        chan_t *h = &ch[c];
        cell_t x;
        mod_cell(pat, pl.row, c, &x);
        h->fx = x.fx;
        h->arg = x.arg;
        h->delay_cell = 0;
        if (x.fx == 0xE && (x.arg >> 4) == 0xD && (x.arg & 15)) {
            h->pend = x;                        /* EDx: the note waits for tick x */
            h->delay_cell = 1;
        } else
            apply_cell(c, &x);
        row_fx(c);
    }
    pl.row_changed = 1;
}

static void next_line(void)
{
    if (pl.pat_delay) {
        pl.pat_delay--;
        in_delay = 1;
        return;
    }
    in_delay = 0;
    if (jump) {
        jump = 0;
        pl.row = next_row;
        if (next_pos >= mod.songlen) {
            next_pos = mod.restart < mod.songlen ? mod.restart : 0;
            pl.ended = 1;
        }
        pl.pos = next_pos;
        return;
    }
    if (++pl.row >= 64) {
        pl.row = 0;
        if (++pl.pos >= mod.songlen) {
            pl.pos = mod.restart < mod.songlen ? mod.restart : 0;
            pl.ended = 1;
        }
    }
}

void mod_tick(void)
{
    uint8_t c;
    if (pl.tick == 0 && !in_delay)
        read_row();
    else
        for (c = 0; c < mod.nch; c++)
            tick_fx(c);
    for (c = 0; c < mod.nch; c++)
        outputs(c);
    if (++pl.tick >= pl.speed) {
        pl.tick = 0;
        next_line();
    }
}

void mod_goto(uint8_t pos)
{
    uint8_t c;
    mod_silence();
    for (c = 0; c < mod.nch; c++) {
        chan_t *h = &ch[c];
        uint8_t pan = h->pan;
        memset(h, 0, sizeof *h);
        h->note = 0xFF;
        h->pan = pan;
    }
    pl.pos = pos < mod.songlen ? pos : 0;
    pl.row = 0;
    pl.ppos = pl.pos;
    pl.prow = 0;
    pl.tick = 0;
    pl.pat_delay = 0;
    in_delay = 0;
    jump = 0;
}

void mod_start(void)
{
    uint8_t c;
    pl.speed = 6;
    pl.bpm = 125;
    pl.ended = 0;
    for (c = 0; c < mod.nch; c++)                   /* Amiga: 0 and 3 left, 1 and 2 right */
        ch[c].pan = (uint8_t)(((c & 3) == 0 || (c & 3) == 3) ? 128 - pl.sep : (pl.sep >= 128 ? 255 : 128 + pl.sep));
    mod_goto(0);
}

uint32_t mod_tick_frames_x100(void)
{
    return 12000000UL / pl.bpm;                 /* 2.5 / bpm s of 48 kHz frames, x 100 */
}
