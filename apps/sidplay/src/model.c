#include <string.h>
#include "model.h"
#include "tables.h"
#include "layout.h"

static inst_t insts[N_INST];
static uint8_t n_inst = 1;                      /* slot 0 unused */
#ifdef HOST
static row_t rows[N_ROWS];
#else
#define rows ((row_t *)SDR_ROWS)                /* the row ring lives in SDRAM */
#endif
static uint16_t n_rows;
static uint8_t speed = 6;
static uint32_t ev_first;
static uint8_t ev_have;
static uint16_t cnt[13];
static uint16_t n_ev;
static uint8_t first_len;                       /* the first row ends on the grid: its length */
static const uint16_t *nf;

typedef struct {
    uint16_t pit_prev, first_pit, last_pit, pmin, pmax;
    uint8_t gate_prev, gate0, nfr;
    uint8_t rise, rnote, rinst, rwave, fell;
    uint8_t njump, nsmall, nup, ndn, nsc, noffs;
    int8_t lastsign, offs[2];
    int16_t sumd;
    uint8_t base_note, pw0, pw1, wave_now;
} vacc_t;
static vacc_t va[3];
static uint32_t row_f0;
static uint8_t row_n;

void model_init(void)
{
    memset(insts, 0, sizeof insts);
    n_inst = 1;
    n_rows = 0;
    speed = 6;
    ev_have = 0;
    n_ev = 0;
    first_len = 0;
    memset(cnt, 0, sizeof cnt);
    memset(va, 0, sizeof va);
    row_n = 0;
    row_f0 = 0;
    nf = pl_clock_ntsc ? note_ntsc : note_pal;
}

const char *note_name(uint8_t n)
{
    static char b[4];
    static const char nm[24] = "C-C#D-D#E-F-F#G-G#A-A#B-";
    uint8_t k = (uint8_t)(n - 1), o = k / 12, s = k % 12;
    b[0] = nm[s * 2];
    b[1] = nm[s * 2 + 1];
    b[2] = (char)('0' + o);
    b[3] = 0;
    return b;
}

/* pitch in 1/256 semitone: note * 256 + fraction (linear between the table's notes) */
static uint16_t pitch(uint16_t fn)
{
    uint8_t lo = 0, hi = 83;
    uint32_t fr;
    if (fn <= nf[0])
        return 0;
    if (fn >= nf[83])
        return 83 * 256;
    while (hi - lo > 1) {
        uint8_t mid = (lo + hi) >> 1;
        if (nf[mid] <= fn)
            lo = mid;
        else
            hi = mid;
    }
    fr = (uint32_t)(fn - nf[lo]) * 256UL / (uint32_t)(nf[lo + 1] - nf[lo]);
    return (uint16_t)(lo * 256 + fr);
}

static uint8_t get_inst(uint8_t ctrl, uint8_t ad, uint8_t sr)
{
    uint8_t i, c = ctrl & 0xF6;
    for (i = 1; i < n_inst; i++)
        if (insts[i].ctrl == c && insts[i].ad == ad && insts[i].sr == sr)
            return i;
    if (n_inst >= N_INST)
        return N_INST - 1;
    insts[n_inst].ctrl = c;
    insts[n_inst].ad = ad;
    insts[n_inst].sr = sr;
    return n_inst++;
}

/* Speed (play calls per row): the grid that all note starts fall on. Every gate rise votes for the
 * divisors of its distance from the first rise; the speed is the largest g (2..12) that 90% of the rises
 * agree with, and it only changes if the current one drops below 80%. */

static void speed_eval(void)
{
    uint8_t g, best = 1;
    if (n_ev < 4)
        return;
    if (speed >= 2 && (uint32_t)cnt[speed] * 100 >= (uint32_t)n_ev * 80)
        return;
    for (g = 12; g >= 2; g--)
        if ((uint32_t)cnt[g] * 100 >= (uint32_t)n_ev * 90) {
            best = g;
            break;
        }
    speed = best;
}

static void note_event(uint32_t fr)
{
    uint8_t g;
    uint32_t d;
    if (!ev_have) {
        ev_have = 1;
        ev_first = fr;
    }
    d = fr - ev_first;
    n_ev++;
    for (g = 2; g <= 12; g++)
        if (d % g == 0)
            cnt[g]++;
    speed_eval();
}

void model_detect(const frame_t *ring, uint16_t n)  /* look at the first n frames before they are fed */
{
    uint16_t i;
    uint8_t v;
    for (i = 0; i < n; i++)
        for (v = 0; v < 3; v++)
            if ((ring[i].rise >> v) & 1)
                note_event(i);
    if (ev_have && speed >= 2)
        first_len = (uint8_t)(ev_first % speed);
    n_ev = 0;                                   /* the real count starts again as the frames are fed */
    ev_have = 0;
    memset(cnt, 0, sizeof cnt);
}

static void close_row(uint8_t gate_end[3], uint32_t f0, uint8_t len)
{
    row_t *r = &rows[n_rows % N_ROWS];
    uint8_t v;
    r->frame0 = f0;
    r->len = len;
    for (v = 0; v < 3; v++) {
        vacc_t *a = &va[v];
        cell_t *c = &r->c[v];
        uint8_t arp = a->njump >= 2;
        memset(c, 0, sizeof *c);
        if (a->rise) {
            c->note = a->rnote + 1;
            c->inst = a->rinst;
            c->wave = a->rwave;
        } else if (a->fell && !gate_end[v] && a->gate0) {
            c->note = 0xFE;
        } else if (a->njump == 1 && gate_end[v] && a->gate0 && !a->fell && a->last_pit > 256) {
            c->note = (uint8_t)((a->last_pit + 128) >> 8) + 1;      /* a legato change */
        }
        if (arp) {
            c->fxc = '0';
            c->fxv = (uint8_t)((a->offs[0] & 15) << 4 | (a->noffs > 1 ? (a->offs[1] & 15) : 0));
        } else if (a->nsmall >= 3 && a->nup >= 3 && a->ndn <= 1 && a->sumd >= 160) {
            int16_t m = a->sumd / a->nsmall;
            c->fxc = '1';
            c->fxv = m > 255 ? 255 : (uint8_t)m;
        } else if (a->nsmall >= 3 && a->ndn >= 3 && a->nup <= 1 && a->sumd <= -160) {
            int16_t m = -(a->sumd / a->nsmall);
            c->fxc = '2';
            c->fxv = m > 255 ? 255 : (uint8_t)m;
        } else if (a->nsmall >= 3 && (a->pmax - a->pmin) < 200 && a->nsc >= 1) {
            uint16_t d = (a->pmax - a->pmin) >> 4;     /* depth in 1/16 semitone */
            c->fxc = '4';
            c->fxv = (uint8_t)((a->nsc > 14 ? 15 : a->nsc + 1) << 4 | (d > 15 ? 15 : d));
        } else if ((a->wave_now & 0x40) && a->pw0 != a->pw1) {
            c->fxc = 'P';
            c->fxv = a->pw1;
        }
        /* start the next row */
        a->rise = a->fell = 0;
        a->njump = a->nsmall = a->nup = a->ndn = a->nsc = a->noffs = 0;
        a->lastsign = 0;
        a->sumd = 0;
        a->nfr = 0;
    }
    n_rows++;
}

void model_feed(const frame_t *f, uint32_t fr)
{
    uint8_t v, gate_end[3];
    if (row_n == 0)
        row_f0 = fr;
    for (v = 0; v < 3; v++) {
        vacc_t *a = &va[v];
        const uint8_t *q = f->r + v * 7;
        uint8_t ctrl = q[4], gate = ctrl & 1;
        uint16_t fn = (uint16_t)(q[0] + q[1] * 256u), pit = fn ? pitch(fn) : a->pit_prev;
        uint8_t pw8 = (uint8_t)((q[3] & 15) << 4 | q[2] >> 4);
        gate_end[v] = gate;
        if (a->nfr == 0) {
            a->gate0 = a->gate_prev;
            a->first_pit = a->pmin = a->pmax = pit;
            a->base_note = (uint8_t)((pit + 128) >> 8);
            a->pw0 = pw8;
        }
        a->wave_now = ctrl;
        a->pw1 = pw8;
        if (((f->rise >> v) & 1) && !a->rise) {
            a->rise = 1;
            a->rnote = (uint8_t)((pit + 128) >> 8);
            a->rinst = get_inst(ctrl, q[5], q[6]);
            a->rwave = ctrl & 0xF6;
            a->pit_prev = pit;                  /* the note start is not a pitch jump */
            a->base_note = a->rnote;
            a->first_pit = a->pmin = a->pmax = pit;
            note_event(fr);
        } else if (a->gate_prev && gate) {
            int16_t d = (int16_t)(pit - a->pit_prev);
            uint16_t ad = d < 0 ? -d : d;
            if (ad >= 192) {
                int8_t off = (int8_t)(((pit + 128) >> 8) - a->base_note);
                a->njump++;
                if (off != 0 && a->noffs < 2 && (a->noffs == 0 || a->offs[0] != off))
                    a->offs[a->noffs++] = off;
            } else if (ad > 0) {
                int8_t sg = d > 0 ? 1 : -1;
                a->sumd += d;
                a->nsmall++;
                if (d > 0) a->nup++; else a->ndn++;
                if (a->lastsign && sg != a->lastsign)
                    a->nsc++;
                a->lastsign = sg;
            }
            if (pit < a->pmin) a->pmin = pit;
            if (pit > a->pmax) a->pmax = pit;
        }
        if ((f->fall >> v) & 1)
            a->fell = 1;
        a->last_pit = pit;
        a->pit_prev = pit;
        a->gate_prev = gate;
        a->nfr++;
    }
    row_n++;
    if (row_n >= (first_len ? first_len : speed)) {
        close_row(gate_end, row_f0, row_n);
        row_n = 0;
        first_len = 0;
    }
}

uint16_t model_nrows(void) { return n_rows; }

const row_t *model_row(uint16_t a)
{
    if (a >= n_rows || (uint16_t)(n_rows - a) > N_ROWS)
        return 0;
    return &rows[a % N_ROWS];
}

int16_t model_row_at(uint32_t frame)
{
    int16_t lo = (n_rows > N_ROWS) ? (int16_t)(n_rows - N_ROWS) : 0, hi = (int16_t)n_rows - 1, mid, ans = -1;
    while (lo <= hi) {                          /* last row with frame0 <= frame */
        mid = (lo + hi) / 2;
        if (rows[(uint16_t)mid % N_ROWS].frame0 <= frame) {
            ans = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return ans;
}

uint8_t model_speed(void) { return speed; }
uint8_t model_ninst(void) { return n_inst; }
const inst_t *model_inst(uint8_t i) { return &insts[i]; }

uint8_t model_nearest(uint16_t fn)              /* note + 1 of the nearest note, 0 if below the table */
{
    const uint16_t *t = pl_clock_ntsc ? note_ntsc : note_pal;
    uint16_t p;
    if (fn < t[0] / 2)
        return 0;
    nf = t;
    p = pitch(fn);
    return (uint8_t)(((p + 128) >> 8) + 1);
}
