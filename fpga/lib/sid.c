/* The SID in software (sid.h). Built for T1, a 16-bit CPU with a slow
 * multiply: samples are made in blocks (up to CTL, between envelope steps),
 * a voice at a time, each waveform with a loop of its own; a voice's 24-bit
 * phase is kept times 256 in 32 bits, so its bits 23:8 are the high word;
 * and a voice's volume is a table (32 levels of the 256 waveform values),
 * so a sample needs no multiply. Voices with combined waveforms, and all
 * three while any uses sync or ring modulation, take a general loop. */
#include "sid.h"
#include <fcpu.h>

#define STR_CTRL IO16(0xF00420)
#define STR_VOL  IO16(0xF00422)
#define STR_ALO  IO16(0xF00424)
#define STR_AHI  IO16(0xF00426)
#define STR_LEN  IO16(0xF00428)
#define STR_POS  IO16(0xF0042A)

#define CTL 32                               /* samples between envelope steps (2.6 ms) */
#define PAL 985248u

enum { ATTACK, DECAY, RELEASE, HOLD };

/* the voices */
static uint32_t ph[3];                       /* the 24-bit phase x 256: bits 23:8 are the high word */
static uint32_t inc[3];                      /* the step a sample, likewise */
static uint16_t pw[3];                       /* pulse width, 12 bits */
static uint8_t  ctrl[3];
static uint8_t  nz[3];                       /* the noise output */
static uint32_t lfsr[3];
static uint8_t  ad[3], sr[3];
static uint8_t  st[3];                       /* ATTACK, DECAY or RELEASE */
static uint16_t env[3];                      /* 8.8 */
uint8_t sid_env[3];
static uint8_t  mvol, v3off;
static const int8_t *vt[3];                  /* each voice's volume table */

static int8_t   lvl[32][256];                /* (w - 128) x l / 31 */
static uint16_t att_step[16], dec_step[16];  /* 8.8 a CTL step */
static uint8_t  ctl_left;

/* the ring */
static uint32_t *ring;                       /* (the player reads it by DMA) */
static uint16_t ring_len, wpos;

/* the SID's attack times (ms); decay and release take 3 times as long */
static const uint16_t att_ms[16] = { 2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 };

static void set_freq(int v, uint16_t f)
{
    /* the step a sample: f x PAL / SID_RATE in units of 2^-24 (cycles of
     * the 24-bit phase), here split 16.8 */
    inc[v] = (uint32_t)f * (PAL * 256u / SID_RATE);    /* (f x 20662 < 2^31) */
}

static uint8_t noise_out(uint32_t r)
{
    return (uint8_t)((r >> 15 & 0x80) | (r >> 14 & 0x40) | (r >> 11 & 0x20) | (r >> 9 & 0x10) |
                     (r >> 8 & 0x08) | (r >> 5 & 0x04) | (r >> 3 & 0x02) | (r >> 2 & 0x01));
}

static uint16_t freq[3];

void sid_write(uint8_t reg, uint8_t val)
{
    if (reg >= 21) {
        if (reg == 24) { mvol = val & 15; v3off = val >> 7; }
        return;
    }
    int v = reg / 7, r = reg % 7;
    switch (r) {
    case 0: freq[v] = (uint16_t)((freq[v] & 0xFF00) | val); set_freq(v, freq[v]); break;
    case 1: freq[v] = (uint16_t)((freq[v] & 0x00FF) | val << 8); set_freq(v, freq[v]); break;
    case 2: pw[v] = (uint16_t)((pw[v] & 0xF00) | val); break;
    case 3: pw[v] = (uint16_t)((pw[v] & 0x0FF) | (val & 15) << 8); break;
    case 4: {
        uint8_t old = ctrl[v];
        ctrl[v] = val;
        if ((val & 1) && !(old & 1)) st[v] = ATTACK;
        if (!(val & 1) && (old & 1)) st[v] = RELEASE;
        if (val & 8) { ph[v] = 0; lfsr[v] = 0x7FFFF8; nz[v] = noise_out(lfsr[v]); }
        break;
    }
    case 5: ad[v] = val; break;
    case 6: sr[v] = val; break;
    }
}

/* every CTL samples: the envelopes, and each voice's volume table */
static void envelopes(void)
{
    for (int v = 0; v < 3; v++) {
        uint16_t e = env[v];
        if (st[v] == HOLD) {
        } else if (st[v] == ATTACK) {
            uint16_t s = att_step[ad[v] >> 4];
            if (e >= 0xFF00 - s) { e = 0xFF00; st[v] = DECAY; }
            else e += s;
        } else {
            uint8_t rate = st[v] == DECAY ? ad[v] & 15 : sr[v] & 15;
            uint16_t target = st[v] == DECAY ? (uint16_t)((sr[v] >> 4) * 0x1100) : 0;
            if (e > target) {
                /* exponential: slower as the level falls (the SID's periods x1..x30) */
                uint8_t e8 = (uint8_t)(e >> 8);
                uint16_t s = dec_step[rate];
                s >>= e8 >= 93 ? 0 : e8 >= 54 ? 1 : e8 >= 26 ? 2 : e8 >= 14 ? 3 : e8 >= 6 ? 4 : 5;
                if (s == 0) s = 1;
                e = (e - target > s) ? (uint16_t)(e - s) : target;
            }
        }
        env[v] = e;
        uint8_t e8 = (uint8_t)(e >> 8);
        sid_env[v] = e8;
        uint16_t l = (uint16_t)(e8 * mvol) >> 7;          /* 0-29 */
        if (v == 2 && v3off) l = 0;
        vt[v] = lvl[l];
    }
}


/* one voice's waveform value (0-255) at phase h (bits 23:8); m: the ring
 * source's phase, for ring modulation */
static inline uint8_t wave(int v, uint16_t h, uint16_t m)
{
    uint8_t c = ctrl[v];
    uint8_t sel = c >> 4;
    if (!sel) return 0x80;
    uint8_t w = 0xFF;
    if (sel & 1) {
        uint16_t msb = (c & 4) ? (uint16_t)((h ^ m) & 0x8000) : (uint16_t)(h & 0x8000);
        uint8_t t = (uint8_t)(h >> 7);
        w &= msb ? (uint8_t)~t : t;
    }
    if (sel & 2) w &= (uint8_t)(h >> 8);
    if (sel & 4) w &= ((h >> 4) >= pw[v]) ? 0xFF : 0x00;
    if (sel & 8) w &= nz[v];
    return w;
}

static int16_t mix[CTL];

/* the general loop: every waveform, sync and ring, all three voices */
static void block_general(uint16_t k)
{
    for (uint16_t i = 0; i < k; i++) {
        uint16_t h[3];
        uint8_t rise = 0;
        for (int v = 0; v < 3; v++) {
            uint16_t o = (uint16_t)(ph[v] >> 16);
            if (ctrl[v] & 8) { h[v] = o; continue; }      /* the test bit holds the phase at 0 */
            ph[v] += inc[v];
            uint16_t nh = (uint16_t)(ph[v] >> 16);
            h[v] = nh;
            if (nh & ~o & 0x8000) rise |= (uint8_t)(1 << v);
            if (nh & ~o & 0x0800) {                         /* bit 19 rose: the noise moves */
                uint32_t r = lfsr[v];
                r = ((r << 1) | ((r >> 22 ^ r >> 17) & 1)) & 0x7FFFFF;
                lfsr[v] = r;
                nz[v] = noise_out(r);
            }
        }
        /* hard sync: voice v restarts when its source (v - 1) passes 0 */
        if (rise) {
            if ((ctrl[0] & 2) && (rise & 4)) { ph[0] = 0; h[0] = 0; }
            if ((ctrl[1] & 2) && (rise & 1)) { ph[1] = 0; h[1] = 0; }
            if ((ctrl[2] & 2) && (rise & 2)) { ph[2] = 0; h[2] = 0; }
        }
        mix[i] = (int16_t)(vt[0][wave(0, h[0], h[2])] + vt[1][wave(1, h[1], h[0])] +
                           vt[2][wave(2, h[2], h[1])]);
    }
}

/* one voice, added into mix */
static void block_voice(int v, uint16_t k)
{
    uint32_t p = ph[v], d = inc[v];
    const int8_t *t = vt[v];
    int16_t *m = mix;
    uint8_t c = ctrl[v];
    if (c & 8) d = 0;
    switch (c >> 4) {
    case 0:                                  /* no waveform: silent (the phase runs on) */
        p += d * k;
        break;
    case 1:                                  /* triangle */
        for (uint16_t i = 0; i < k; i++) {
            p += d;
            uint16_t h = (uint16_t)(p >> 16);
            uint8_t w = (uint8_t)(h >> 7);
            if (h & 0x8000) w = (uint8_t)~w;
            m[i] = (int16_t)(m[i] + t[w]);
        }
        break;
    case 2:                                  /* sawtooth */
        for (uint16_t i = 0; i < k; i++) {
            p += d;
            m[i] = (int16_t)(m[i] + t[(uint16_t)(p >> 16) >> 8]);
        }
        break;
    case 4: {                                /* pulse */
        uint16_t w16 = (uint16_t)(pw[v] << 4);
        int8_t hi = t[255], lo = t[0];
        if (c & 8) lo = hi;                  /* the test bit: high */
        for (uint16_t i = 0; i < k; i++) {
            p += d;
            m[i] = (int16_t)(m[i] + ((uint16_t)(p >> 16) >= w16 ? hi : lo));
        }
        break;
    }
    case 8: {                                /* noise */
        uint32_t r = lfsr[v];
        int8_t val = t[nz[v]];
        for (uint16_t i = 0; i < k; i++) {
            uint16_t o = (uint16_t)(p >> 16);
            p += d;
            if ((uint16_t)(p >> 16) & ~o & 0x0800) {
                r = ((r << 1) | ((r >> 22 ^ r >> 17) & 1)) & 0x7FFFFF;
                nz[v] = noise_out(r);
                val = t[nz[v]];
            }
            m[i] = (int16_t)(m[i] + val);
        }
        lfsr[v] = r;
        break;
    }
    default:                                 /* combined waveforms */
        for (uint16_t i = 0; i < k; i++) {
            uint16_t o = (uint16_t)(p >> 16);
            p += d;
            uint16_t h = (uint16_t)(p >> 16);
            if (h & ~o & 0x0800) {
                uint32_t r = lfsr[v];
                r = ((r << 1) | ((r >> 22 ^ r >> 17) & 1)) & 0x7FFFFF;
                lfsr[v] = r;
                nz[v] = noise_out(r);
            }
            m[i] = (int16_t)(m[i] + t[wave(v, h, 0)]);
        }
        break;
    }
    ph[v] = p;
}

void sid_render(uint16_t n)
{
    while (n) {
        if (ctl_left == 0) { envelopes(); ctl_left = CTL; }
        uint16_t k = n < ctl_left ? n : ctl_left;
        if (k > ring_len - wpos) k = (uint16_t)(ring_len - wpos);
        if ((ctrl[0] | ctrl[1] | ctrl[2]) & 6)
            block_general(k);
        else {
            for (uint16_t i = 0; i < k; i++) mix[i] = 0;
            block_voice(0, k);
            block_voice(1, k);
            block_voice(2, k);
        }
        uint16_t *o = (uint16_t *)(ring + wpos);
        for (uint16_t i = 0; i < k; i++) {
            uint16_t s = (uint16_t)(mix[i] << 6);
            o[0] = s;
            o[1] = s;
            o += 2;
        }
        wpos = (uint16_t)(wpos + k == ring_len ? 0 : wpos + k);
        ctl_left = (uint8_t)(ctl_left - k);
        n = (uint16_t)(n - k);
    }
}

void sid_level(int v, uint8_t level)
{
    env[v] = (uint16_t)(level << 8);
    st[v] = HOLD;
    if (mvol == 0) mvol = 15;
}

void sid_mute(int mute)
{
    if (mute) { STR_VOL = 0; return; }
    uint16_t p = (uint16_t)(STR_POS + 4);
    wpos = p >= ring_len ? (uint16_t)(p - ring_len) : p;
    STR_VOL = 0x100;
}

uint16_t sid_room(void)
{
    /* up to two frames short of the one being fetched */
    int32_t d = (int32_t)STR_POS - 2 - (int32_t)wpos;
    if (d < 0) d += ring_len;
    return (uint16_t)d;
}

void sid_init(uint32_t buf, uint16_t len)
{
    for (int l = 0; l < 32; l++)
        for (int w = 0; w < 256; w++)
            lvl[l][w] = (int8_t)((w - 128) * l / 31);
    for (int i = 0; i < 16; i++) {
        /* 0xFF00 over the attack time, in steps of CTL samples */
        uint32_t steps = (uint32_t)att_ms[i] * SID_RATE / (1000u * CTL);
        att_step[i] = (uint16_t)(steps ? 0xFF00u / steps : 0xFF00u);
        uint32_t dsteps = steps * 3;
        dec_step[i] = (uint16_t)(dsteps ? 0xFF00u / dsteps : 0xFF00u);
        if (att_step[i] == 0) att_step[i] = 1;
        if (dec_step[i] == 0) dec_step[i] = 1;
    }
    for (int v = 0; v < 3; v++) {
        ph[v] = 0; inc[v] = 0; pw[v] = 0; ctrl[v] = 0; freq[v] = 0;
        lfsr[v] = 0x7FFFF8; nz[v] = noise_out(lfsr[v]);
        ad[v] = sr[v] = 0; st[v] = RELEASE; env[v] = 0; vt[v] = lvl[0];
    }
    mvol = 0; v3off = 0; ctl_left = 0;
    ring = (uint32_t *)buf;
    ring_len = len;
    wpos = 0;
    for (uint16_t i = 0; i < len; i++) ring[i] = 0;
    STR_CTRL = 0;
    STR_VOL = 0x100;
    STR_ALO = (uint16_t)buf;
    STR_AHI = (uint16_t)(buf >> 16);
    STR_LEN = len;
    STR_CTRL = (uint16_t)(1 | (SID_DIV - 1) << 1);
}

void sid_stop(void)
{
    STR_CTRL = 0;
}
