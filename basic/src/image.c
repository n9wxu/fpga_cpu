/* Image decoders: baseline JPEG and PNG.
 *
 * Both stream from a file and hand finished rows of RGBA pixels, top to
 * bottom, to a sink (gfx.c has the sinks for PICTURE and SHEET). Working
 * memory comes from the free gap in the arena, between the arrays and
 * the string heap: decoder scratch is taken from the top of the gap, and
 * anything the sink keeps (a sprite sheet) from the bottom with
 * img_alloc(), like an array. Nothing here allocates strings, so the gap
 * cannot move while an image decodes, and the scratch is gone afterwards.
 *
 * JPEG: baseline and extended sequential Huffman, 8-bit samples, gray or
 * YCbCr (or Adobe RGB), sampling factors 1 or 2, restart markers.
 * Progressive, arithmetic and lossless files are refused.
 * PNG: every color type and bit depth, tRNS transparency. Interlaced
 * files are refused.
 *
 * Errors unwind with longjmp to img_load(), which closes the file before
 * raising the BASIC error. int is 16 bits on the target: anything that can
 * exceed 32767 uses int32_t / uint32_t.
 */
#include "basic.h"

#define IMG_STOP 0x7E /* not an error: the sink has all it wants */

static jmp_buf img_jb;
static int img_fh;

void img_fail(int e) { longjmp(img_jb, e + 1); }

/* ---- file bytes ---- */

static uint8_t fbuf[256];
static int16_t fpos, flen;

static int rdb(void)
{
    if (fpos >= flen) {
        int n = hal_fread(img_fh, fbuf, (int)sizeof fbuf);
        fpos = 0;
        flen = (int16_t)(n > 0 ? n : 0);
        if (n <= 0)
            return -1;
    }
    return fbuf[fpos++];
}

static uint8_t rd8(void)
{
    int b = rdb();
    if (b < 0)
        img_fail(E_IF);
    return (uint8_t)b;
}

static uint16_t rd16(void)
{
    uint16_t h = rd8();
    return (uint16_t)(h << 8 | rd8());
}

static uint32_t rd32(void)
{
    uint32_t h = rd16();
    return h << 16 | rd16();
}

static void skip(uint32_t n)
{
    while (n--)
        rd8();
}

/* ---- memory ---- */

static uint8_t *scr_lo; /* scratch grows down from the string heap to here */

static void *salloc(uint32_t n)
{
    n = (n + 3u) & ~3u;
    if (n > (uint32_t)(scr_lo - arr_end))
        img_fail(E_OM);
    scr_lo -= n;
    memset(scr_lo, 0, n);
    return scr_lo;
}

void *img_scratch(uint32_t n) { return salloc(n); }

uint8_t *img_alloc(uint32_t n)
{
    uint8_t *a = arr_end;
    uint32_t need = (n + 7u) & ~7u;
    if (need > (uint32_t)(scr_lo - arr_end))
        img_fail(E_OM);
    arr_end += need;
    return a;
}

static img_sink_t *sink;
static uint8_t *rgba;  /* one output row */
static uint16_t rgba_w;
static uint8_t *irow;  /* its palette indices (indexed PNG) */

static void start(uint16_t w, uint16_t h, int photo, uint16_t npal, const uint8_t *pal)
{
    img_info_t in;
    if (!w || !h)
        img_fail(E_IF);
    in.w = w;
    in.h = h;
    in.photo = (uint8_t)photo;
    in.npal = npal;
    in.pal = pal;
    sink->start(sink, &in);
    rgba = (uint8_t *)salloc((uint32_t)w * 4u);
    rgba_w = w;
    irow = npal ? (uint8_t *)salloc(w) : NULL;
}

static void emit(uint16_t y)
{
    if (sink->row(sink, y, rgba, irow))
        img_fail(IMG_STOP);
}

/* ---- JPEG ---- */

static const uint8_t zz[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

typedef struct {
    uint16_t fast[256]; /* first 8 bits: length << 8 | symbol; 0 = longer code */
    int32_t maxcode[17];
    uint16_t mincode[17];
    int16_t valptr[17];
    uint8_t vals[256];
} huff_t;

typedef struct {
    uint8_t id, h, v, tq, td, ta;
    int16_t dc;
    uint8_t *buf;   /* one MCU row of samples */
    uint16_t bw;    /* its width */
    uint16_t n;     /* real samples per row (the rest is MCU padding) */
    uint16_t nrows; /* real sample rows in the image */
    uint8_t *save;  /* the row kept for an output row that waits for the next MCU row */
    uint8_t *up;    /* one upsampled row */
} comp_t;

static huff_t *htab[8]; /* class << 2 | id */
static uint16_t *qtab[4];
static comp_t comp[3];
static uint8_t ncomp, adobe_rgb, marker;
static uint16_t jw, jh, rst_int;
static uint32_t bacc;
static int16_t bcnt;
static int16_t *cr_r, *cb_b;
static int32_t *cr_g, *cb_g;

static void bfill(void)
{
    while (bcnt <= 24) {
        uint32_t b = 0;
        if (!marker) {
            int c = rdb();
            if (c < 0)
                img_fail(E_IF);
            if (c == 0xFF) {
                int m = rd8();
                while (m == 0xFF)
                    m = rd8();
                if (m)
                    marker = (uint8_t)m; /* a marker ends the entropy data: pad with zeros */
                else
                    b = 0xFF;
            } else
                b = (uint32_t)c;
        }
        bacc |= b << (24 - bcnt);
        bcnt = (int16_t)(bcnt + 8);
    }
}

static uint16_t getbits(int16_t n)
{
    uint16_t v;
    bfill();
    v = (uint16_t)(bacc >> (32 - n));
    bacc <<= n;
    bcnt = (int16_t)(bcnt - n);
    return v;
}

static int32_t extend(uint16_t v, int16_t s)
{
    if (v < (1u << (s - 1)))
        return (int32_t)v - (int32_t)((1ul << s) - 1u);
    return v;
}

static uint8_t hdec(const huff_t *t)
{
    uint16_t f;
    int16_t l;
    if (!t)
        img_fail(E_IF);
    bfill();
    f = t->fast[bacc >> 24];
    if (f >> 8) {
        l = (int16_t)(f >> 8);
        bacc <<= l;
        bcnt = (int16_t)(bcnt - l);
        return (uint8_t)f;
    }
    for (l = 9; l <= 16; l++) {
        uint32_t code = bacc >> (32 - l);
        if ((int32_t)code <= t->maxcode[l]) {
            bacc <<= l;
            bcnt = (int16_t)(bcnt - l);
            return t->vals[t->valptr[l] + (uint16_t)(code - t->mincode[l])];
        }
    }
    img_fail(E_IF);
}

static void read_dht(uint16_t len)
{
    while (len > 2) {
        uint8_t tc = rd8(), bits[17];
        uint16_t i, l, k = 0, total = 0;
        uint32_t code = 0;
        huff_t *t;
        if ((tc >> 4) > 1 || (tc & 15) > 3)
            img_fail(E_IF);
        t = htab[(tc >> 4) << 2 | (tc & 3)];
        if (!t)
            t = htab[(tc >> 4) << 2 | (tc & 3)] = (huff_t *)salloc(sizeof(huff_t));
        for (l = 1; l <= 16; l++) {
            bits[l] = rd8();
            total = (uint16_t)(total + bits[l]);
        }
        if (total > 256 || len < 17u + total + 2u)
            img_fail(E_IF);
        for (i = 0; i < total; i++)
            t->vals[i] = rd8();
        memset(t->fast, 0, sizeof t->fast);
        for (l = 1; l <= 16; l++) {
            t->valptr[l] = (int16_t)k;
            t->mincode[l] = (uint16_t)code;
            for (i = 0; i < bits[l]; i++, k++, code++) {
                if (l <= 8) {
                    uint16_t j, base = (uint16_t)(code << (8 - l));
                    for (j = 0; j < (1u << (8 - l)); j++)
                        t->fast[base + j] = (uint16_t)(l << 8 | t->vals[k]);
                }
            }
            t->maxcode[l] = bits[l] ? (int32_t)code - 1 : -1;
            if (code > (1ul << l))
                img_fail(E_IF);
            code <<= 1;
        }
        len = (uint16_t)(len - 17u - total);
    }
}

static void read_dqt(uint16_t len)
{
    while (len > 2) {
        uint8_t pq = rd8();
        uint16_t i;
        if ((pq & 15) > 3 || (pq >> 4) > 1)
            img_fail(E_IF);
        if (!qtab[pq & 3])
            qtab[pq & 3] = (uint16_t *)salloc(64 * sizeof(uint16_t));
        for (i = 0; i < 64; i++)
            qtab[pq & 3][i] = (pq >> 4) ? rd16() : rd8(); /* zigzag order, as stored */
        len = (uint16_t)(len - ((pq >> 4) ? 129u : 65u));
    }
}

static int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

static void dblock(int16_t *blk, comp_t *c)
{
    const uint16_t *q = qtab[c->tq];
    int16_t k = 1, s;
    uint8_t rs;
    memset(blk, 0, 64 * sizeof(int16_t));
    s = hdec(htab[c->td]);
    if (s > 11)
        img_fail(E_IF);
    if (s)
        c->dc = sat16(c->dc + extend(getbits(s), s));
    blk[0] = sat16((int32_t)c->dc * q[0]);
    while (k < 64) {
        rs = hdec(htab[4 | c->ta]);
        s = rs & 15;
        if (!s) {
            if ((rs >> 4) != 15)
                break; /* end of block */
            k = (int16_t)(k + 16);
            continue;
        }
        k = (int16_t)(k + (rs >> 4));
        if (k > 63)
            img_fail(E_IF);
        blk[zz[k]] = sat16(extend(getbits(s), s) * q[k]);
        k++;
    }
}

static uint8_t clamp8(int32_t x) { return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x); }

/* Integer IDCT (the islow algorithm: 12-bit constants, two passes). The
 * first pass is clamped to 16 bits, which valid files never reach: it
 * keeps a damaged file's coefficients from overflowing the second pass. */
#define IDCT_1D(s0, s1, s2, s3, s4, s5, s6, s7)                                                     \
    p2 = s2;                                                                                       \
    p3 = s6;                                                                                       \
    p1 = (p2 + p3) * 2217;                                                                         \
    t2 = p1 + p3 * -7568;                                                                          \
    t3 = p1 + p2 * 3135;                                                                           \
    p2 = s0;                                                                                       \
    p3 = s4;                                                                                       \
    t0 = (p2 + p3) * 4096;                                                                         \
    t1 = (p2 - p3) * 4096;                                                                         \
    x0 = t0 + t3;                                                                                  \
    x3 = t0 - t3;                                                                                  \
    x1 = t1 + t2;                                                                                  \
    x2 = t1 - t2;                                                                                  \
    t0 = s7;                                                                                       \
    t1 = s5;                                                                                       \
    t2 = s3;                                                                                       \
    t3 = s1;                                                                                       \
    p3 = t0 + t2;                                                                                  \
    p4 = t1 + t3;                                                                                  \
    p1 = t0 + t3;                                                                                  \
    p2 = t1 + t2;                                                                                  \
    p5 = (p3 + p4) * 4816;                                                                         \
    t0 = t0 * 1223;                                                                                \
    t1 = t1 * 8410;                                                                                \
    t2 = t2 * 12586;                                                                               \
    t3 = t3 * 6149;                                                                                \
    p1 = p5 + p1 * -3686;                                                                          \
    p2 = p5 + p2 * -10498;                                                                         \
    p3 = p3 * -8035;                                                                               \
    p4 = p4 * -1598;                                                                               \
    t3 += p1 + p4;                                                                                 \
    t2 += p2 + p3;                                                                                 \
    t1 += p2 + p4;                                                                                 \
    t0 += p1 + p3;

static void idct(const int16_t *d, uint8_t *out, uint16_t stride)
{
    int32_t v[64], *w = v, t0, t1, t2, t3, p1, p2, p3, p4, p5, x0, x1, x2, x3;
    int16_t i;
    for (i = 0; i < 8; i++, d++, w++) {
        if (!d[8] && !d[16] && !d[24] && !d[32] && !d[40] && !d[48] && !d[56]) {
            int32_t dc = sat16((int32_t)d[0] * 4);
            w[0] = w[8] = w[16] = w[24] = w[32] = w[40] = w[48] = w[56] = dc;
            continue;
        }
        IDCT_1D((int32_t)d[0], (int32_t)d[8], (int32_t)d[16], (int32_t)d[24], (int32_t)d[32],
                (int32_t)d[40], (int32_t)d[48], (int32_t)d[56])
        x0 += 512;
        x1 += 512;
        x2 += 512;
        x3 += 512;
        w[0] = sat16((x0 + t3) >> 10);
        w[56] = sat16((x0 - t3) >> 10);
        w[8] = sat16((x1 + t2) >> 10);
        w[48] = sat16((x1 - t2) >> 10);
        w[16] = sat16((x2 + t1) >> 10);
        w[40] = sat16((x2 - t1) >> 10);
        w[24] = sat16((x3 + t0) >> 10);
        w[32] = sat16((x3 - t0) >> 10);
    }
    for (i = 0, w = v; i < 8; i++, w += 8, out += stride) {
        IDCT_1D(w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7])
        x0 += 65536L + (128L << 17);
        x1 += 65536L + (128L << 17);
        x2 += 65536L + (128L << 17);
        x3 += 65536L + (128L << 17);
        out[0] = clamp8((x0 + t3) >> 17);
        out[7] = clamp8((x0 - t3) >> 17);
        out[1] = clamp8((x1 + t2) >> 17);
        out[6] = clamp8((x1 - t2) >> 17);
        out[2] = clamp8((x2 + t1) >> 17);
        out[5] = clamp8((x2 - t1) >> 17);
        out[3] = clamp8((x3 + t0) >> 17);
        out[4] = clamp8((x3 - t0) >> 17);
    }
}

static void color_tables(void)
{
    int16_t i;
    cr_r = (int16_t *)salloc(256 * sizeof(int16_t));
    cb_b = (int16_t *)salloc(256 * sizeof(int16_t));
    cr_g = (int32_t *)salloc(256 * sizeof(int32_t));
    cb_g = (int32_t *)salloc(256 * sizeof(int32_t));
    for (i = 0; i < 256; i++) {
        int32_t x = i - 128;
        cr_r[i] = (int16_t)(((int32_t)91881 * x + 32768) >> 16);  /* 1.402 */
        cb_b[i] = (int16_t)(((int32_t)116130 * x + 32768) >> 16); /* 1.772 */
        cr_g[i] = (int32_t)-46802 * x;                            /* 0.71414 */
        cb_g[i] = (int32_t)-22554 * x + 32768;                    /* 0.34414 */
    }
}

static void restart(void)
{
    int16_t i;
    bacc = 0;
    bcnt = 0;
    if (!marker) { /* find the next marker */
        for (;;) {
            int b = rdb();
            if (b < 0)
                img_fail(E_IF);
            if (b == 0xFF) {
                do
                    b = rd8();
                while (b == 0xFF);
                if (b) {
                    marker = (uint8_t)b;
                    break;
                }
            }
        }
    }
    if (marker < 0xD0 || marker > 0xD7)
        img_fail(E_IF);
    marker = 0;
    for (i = 0; i < 3; i++)
        comp[i].dc = 0;
}

/* Chroma upsampling, the same as libjpeg's default ("fancy"): each output
 * sample is 3/4 of the nearer input sample plus 1/4 of the next one, across
 * and down. t is this input row; nr the vertical neighbor (NULL: no
 * vertical step), above the output row if upper is set. */
static const uint8_t *upsample(comp_t *c, const uint8_t *t, const uint8_t *nr, int upper, int hx)
{
    uint8_t *o = c->up;
    uint16_t i, n = c->n;
    int16_t last, cur, next;
    if (!hx) {
        if (!nr)
            return t;
        for (i = 0; i < n; i++)
            o[i] = (uint8_t)((3 * t[i] + nr[i] + (upper ? 1 : 2)) >> 2);
        return o;
    }
    if (n == 1) {
        o[0] = o[1] = t[0];
        return o;
    }
    if (!nr) {
        o[0] = t[0];
        o[1] = (uint8_t)((3 * t[0] + t[1] + 2) >> 2);
        for (i = 1; i + 1u < n; i++) {
            o[2 * i] = (uint8_t)((3 * t[i] + t[i - 1] + 1) >> 2);
            o[2 * i + 1] = (uint8_t)((3 * t[i] + t[i + 1] + 2) >> 2);
        }
        o[2 * n - 2] = (uint8_t)((3 * t[n - 1] + t[n - 2] + 1) >> 2);
        o[2 * n - 1] = t[n - 1];
        return o;
    }
    cur = (int16_t)(3 * t[0] + nr[0]);
    next = (int16_t)(3 * t[1] + nr[1]);
    o[0] = (uint8_t)((cur * 4 + 8) >> 4);
    o[1] = (uint8_t)((cur * 3 + next + 7) >> 4);
    last = cur;
    cur = next;
    for (i = 1; i + 1u < n; i++) {
        next = (int16_t)(3 * t[i + 1] + nr[i + 1]);
        o[2 * i] = (uint8_t)((cur * 3 + last + 8) >> 4);
        o[2 * i + 1] = (uint8_t)((cur * 3 + next + 7) >> 4);
        last = cur;
        cur = next;
    }
    o[2 * n - 2] = (uint8_t)((cur * 3 + last + 8) >> 4);
    o[2 * n - 1] = (uint8_t)((cur * 4 + 7) >> 4);
    return o;
}

static uint8_t hmax, vmax, vctx;
static uint16_t my_cur; /* MCU row being emitted */

/* Emit image row y: row ly of the current MCU row, or (saved) the last row
 * of the previous one, whose lower neighbor is row 0 of the current one. */
static void jrow(uint16_t y, uint16_t ly, int saved)
{
    const uint8_t *p[3];
    uint8_t *o = rgba;
    uint16_t i, x;
    for (i = 0; i < ncomp; i++) {
        comp_t *c = &comp[i];
        const uint8_t *t, *nr = NULL;
        int upper = !(ly & 1);
        if (c->v == vmax) {
            t = saved ? c->save : c->buf + (uint32_t)ly * c->bw;
        } else {
            uint16_t j = (uint16_t)(ly >> 1), gj = (uint16_t)((saved ? my_cur - 1u : my_cur) * 8u + j);
            t = saved ? c->save : c->buf + (uint32_t)j * c->bw;
            if (upper)
                nr = j ? t - c->bw : my_cur ? c->save : t; /* image top: itself */
            else if (gj + 1u >= c->nrows)
                nr = t; /* image bottom: itself */
            else
                nr = saved ? c->buf : t + c->bw;
        }
        p[i] = upsample(c, t, nr, upper, c->h != hmax);
    }
    if (ncomp == 1) {
        for (x = 0; x < jw; x++, o += 4)
            o[0] = o[1] = o[2] = p[0][x], o[3] = 255;
    } else if (adobe_rgb) {
        for (x = 0; x < jw; x++, o += 4)
            o[0] = p[0][x], o[1] = p[1][x], o[2] = p[2][x], o[3] = 255;
    } else {
        for (x = 0; x < jw; x++, o += 4) {
            uint8_t a = p[0][x], b = p[1][x], c = p[2][x];
            o[0] = clamp8((int32_t)a + cr_r[c]);
            o[1] = clamp8((int32_t)a + ((cb_g[b] + cr_g[c]) >> 16));
            o[2] = clamp8((int32_t)a + cb_b[b]);
            o[3] = 255;
        }
    }
    emit(y);
}

static void jpeg_scan(void)
{
    uint8_t ns = rd8(), pend = 0;
    uint16_t i, j, mx, my, mcux, mcuy, y = 0, pend_y = 0, todo = rst_int;
    int16_t *blk;
    if (ns != ncomp)
        img_fail(E_IF); /* non-interleaved scans of a color image */
    for (i = 0; i < ns; i++) {
        uint8_t id = rd8(), t = rd8();
        for (j = 0; j < ncomp && comp[j].id != id; j++)
            ;
        if (j == ncomp || (t >> 4) > 3 || (t & 15) > 3)
            img_fail(E_IF);
        comp[j].td = (uint8_t)(t >> 4);
        comp[j].ta = (uint8_t)(t & 15);
    }
    skip(3); /* Ss, Se, Ah/Al: fixed for sequential */
    if (ncomp == 1)
        comp[0].h = comp[0].v = 1; /* one component: 8x8 MCUs whatever the factors */
    hmax = vmax = 1;
    vctx = 0;
    for (i = 0; i < ncomp; i++) {
        if (comp[i].h > hmax)
            hmax = comp[i].h;
        if (comp[i].v > vmax)
            vmax = comp[i].v;
    }
    mcux = (uint16_t)((jw + 8u * hmax - 1u) / (8u * hmax));
    mcuy = (uint16_t)((jh + 8u * vmax - 1u) / (8u * vmax));
    for (i = 0; i < ncomp; i++) {
        comp_t *c = &comp[i];
        if (!qtab[c->tq])
            img_fail(E_IF);
        c->bw = (uint16_t)(mcux * c->h * 8u);
        c->buf = (uint8_t *)salloc((uint32_t)c->bw * c->v * 8u);
        c->save = (uint8_t *)salloc(c->bw);
        c->up = (uint8_t *)salloc(2u * c->bw + 2u); /* horizontal upsampling doubles a row */
        c->n = (uint16_t)(((uint32_t)jw * c->h + hmax - 1u) / hmax);
        c->nrows = (uint16_t)(((uint32_t)jh * c->v + vmax - 1u) / vmax);
        c->dc = 0;
        if (c->v != vmax)
            vctx = 1; /* vertical upsampling needs the next MCU row for the last output row */
    }
    blk = (int16_t *)salloc(64 * sizeof(int16_t));
    if (ncomp == 3 && !adobe_rgb)
        color_tables();
    bacc = 0;
    bcnt = 0;
    for (my = 0; my < mcuy; my++) {
        uint16_t ly;
        my_cur = my;
        for (mx = 0; mx < mcux; mx++) {
            if (rst_int) {
                if (!todo) {
                    restart();
                    todo = rst_int;
                }
                todo--;
            }
            for (i = 0; i < ncomp; i++) {
                comp_t *c = &comp[i];
                uint16_t bx, by;
                for (by = 0; by < c->v; by++)
                    for (bx = 0; bx < c->h; bx++) {
                        dblock(blk, c);
                        idct(blk, c->buf + (uint32_t)by * 8u * c->bw + (uint32_t)(mx * c->h + bx) * 8u, c->bw);
                    }
            }
        }
        if (pend) { /* the last row of the previous MCU row, now that its lower neighbor is here */
            jrow(pend_y, (uint16_t)(8u * vmax - 1u), 1);
            pend = 0;
        }
        for (ly = 0; ly < 8u * vmax && y < jh; ly++, y++) {
            if (vctx && ly == 8u * vmax - 1u && my + 1u < mcuy) {
                for (i = 0; i < ncomp; i++) /* keep it: the next MCU row overwrites the buffers */
                    memcpy(comp[i].save, comp[i].buf + (uint32_t)(comp[i].v == vmax ? ly : ly >> 1) * comp[i].bw,
                           comp[i].bw);
                pend = 1;
                pend_y = y;
                continue;
            }
            jrow(y, ly, 0);
        }
    }
}

static void jpeg_load(void)
{
    uint16_t i;
    if (rd8() != 0xD8)
        img_fail(E_IF);
    memset(htab, 0, sizeof htab);
    memset(qtab, 0, sizeof qtab);
    ncomp = 0;
    adobe_rgb = 0;
    marker = 0;
    rst_int = 0;
    for (;;) {
        uint8_t m;
        uint16_t len;
        while (rd8() != 0xFF)
            ; /* tolerate junk between segments */
        do
            m = rd8();
        while (m == 0xFF);
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01)
            continue; /* parameterless */
        if (m == 0xD9)
            img_fail(E_IF); /* EOI before any scan */
        len = rd16();
        if (len < 2)
            img_fail(E_IF);
        switch (m) {
        case 0xC0:
        case 0xC1: {
            if (ncomp || rd8() != 8)
                img_fail(E_IF);
            jh = rd16();
            jw = rd16();
            ncomp = rd8();
            if (ncomp != 1 && ncomp != 3)
                img_fail(E_IF);
            for (i = 0; i < ncomp; i++) {
                uint8_t hv;
                comp[i].id = rd8();
                hv = rd8();
                comp[i].h = (uint8_t)(hv >> 4);
                comp[i].v = (uint8_t)(hv & 15);
                comp[i].tq = rd8();
                if (comp[i].h < 1 || comp[i].h > 2 || comp[i].v < 1 || comp[i].v > 2 || comp[i].tq > 3)
                    img_fail(E_IF);
            }
            skip((uint32_t)len - 8u - 3u * ncomp);
            start(jw, jh, 1, 0, NULL);
            break;
        }
        case 0xC4: read_dht(len); break;
        case 0xDB: read_dqt(len); break;
        case 0xDD:
            rst_int = rd16();
            skip((uint32_t)len - 4u);
            break;
        case 0xDA:
            if (!ncomp)
                img_fail(E_IF);
            jpeg_scan();
            return;
        case 0xEE: { /* Adobe: transform 0 means the components are RGB */
            uint8_t b[12];
            if (len >= 14) {
                for (i = 0; i < 12; i++)
                    b[i] = rd8();
                if (!memcmp(b, "Adobe", 5) && b[11] == 0)
                    adobe_rgb = 1;
                skip((uint32_t)len - 14u);
            } else
                skip((uint32_t)len - 2u);
            break;
        }
        default:
            if ((m >= 0xC2 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC)
                img_fail(E_IF); /* progressive, lossless, arithmetic */
            skip((uint32_t)len - 2u);
            break;
        }
    }
}

/* ---- PNG ---- */

typedef struct {
    uint16_t cnt[16];
    uint16_t sym[288];
} ztree_t;

static const uint16_t len_base[29] = { 3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                       31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[30] = { 1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                        193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                        6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
static const uint8_t clen_order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static uint32_t idat_left;
static uint16_t zbuf;
static int16_t zcnt;
static uint8_t *win; /* 32 KB LZ77 window */
static uint16_t wpos;
static ztree_t *lt, *dt;

/* PNG image state */
static uint16_t pw, ph, py;
static uint8_t pdepth, ptype, pch, pbpp; /* pbpp: bytes per complete pixel, at least 1 */
static uint8_t *cur, *prv;               /* filter byte + row */
static uint16_t rowlen, rpos;
static uint8_t *pal; /* 256 x RGBA */
static uint16_t npal, key[3];
static uint8_t haskey;

static uint8_t zbyte(void)
{
    while (!idat_left) {
        uint32_t len, type;
        skip(4); /* CRC of the previous chunk */
        len = rd32();
        type = rd32();
        if (type != 0x49444154UL) /* IDAT */
            img_fail(E_IF);
        idat_left = len;
    }
    idat_left--;
    return rd8();
}

static uint16_t zbits(int16_t n)
{
    uint16_t v = 0;
    int16_t i;
    for (i = 0; i < n; i++) {
        if (!zcnt) {
            zbuf = zbyte();
            zcnt = 8;
        }
        v = (uint16_t)(v | (zbuf & 1u) << i);
        zbuf >>= 1;
        zcnt--;
    }
    return v;
}

static void zbuild(ztree_t *t, const uint8_t *lens, uint16_t n)
{
    uint16_t offs[16], i, sum = 0;
    memset(t->cnt, 0, sizeof t->cnt);
    for (i = 0; i < n; i++)
        t->cnt[lens[i]]++;
    t->cnt[0] = 0;
    for (i = 0; i < 16; i++) {
        offs[i] = sum;
        sum = (uint16_t)(sum + t->cnt[i]);
    }
    for (i = 0; i < n; i++)
        if (lens[i])
            t->sym[offs[lens[i]]++] = i;
}

static uint16_t zdec(const ztree_t *t)
{
    int16_t base = 0, off = 0, len;
    for (len = 1; len < 16; len++) {
        off = (int16_t)(2 * off + zbits(1));
        if (off < (int16_t)t->cnt[len])
            return t->sym[base + off];
        base = (int16_t)(base + t->cnt[len]);
        off = (int16_t)(off - t->cnt[len]);
    }
    img_fail(E_IF);
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int16_t p = (int16_t)(a + b - c), pa = (int16_t)(p - a), pb = (int16_t)(p - b), pc = (int16_t)(p - c);
    if (pa < 0) pa = (int16_t)-pa;
    if (pb < 0) pb = (int16_t)-pb;
    if (pc < 0) pc = (int16_t)-pc;
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

/* raw sample i of the current row, at the file's bit depth */
static uint16_t sample(const uint8_t *r, uint32_t i)
{
    if (pdepth == 8)
        return r[i];
    if (pdepth == 16)
        return (uint16_t)(r[2 * i] << 8 | r[2 * i + 1]);
    {
        uint32_t bit = i * pdepth;
        return (uint16_t)((r[bit >> 3] >> (8 - pdepth - (bit & 7))) & ((1u << pdepth) - 1u));
    }
}

static uint8_t to8(uint16_t v)
{
    switch (pdepth) {
    case 1: return v ? 255 : 0;
    case 2: return (uint8_t)(v * 85u);
    case 4: return (uint8_t)(v * 17u);
    case 16: return (uint8_t)(v >> 8);
    }
    return (uint8_t)v;
}

static void png_row(void)
{
    uint8_t *x = cur + 1, *p = prv + 1, f = cur[0], *o = rgba;
    uint16_t i, j;
    for (i = 0; i < rowlen; i++) {
        uint8_t a = i >= pbpp ? x[i - pbpp] : 0, b = p[i], c = i >= pbpp ? p[i - pbpp] : 0;
        switch (f) {
        case 0: break;
        case 1: x[i] = (uint8_t)(x[i] + a); break;
        case 2: x[i] = (uint8_t)(x[i] + b); break;
        case 3: x[i] = (uint8_t)(x[i] + ((a + b) >> 1)); break;
        case 4: x[i] = (uint8_t)(x[i] + paeth(a, b, c)); break;
        default: img_fail(E_IF);
        }
    }
    for (j = 0; j < pw; j++, o += 4) {
        uint32_t s = (uint32_t)j * pch;
        uint16_t v0 = sample(x, s);
        switch (ptype) {
        case 0:
            o[0] = o[1] = o[2] = to8(v0);
            o[3] = (uint8_t)(haskey && v0 == key[0] ? 0 : 255);
            break;
        case 2: {
            uint16_t v1 = sample(x, s + 1), v2 = sample(x, s + 2);
            o[0] = to8(v0);
            o[1] = to8(v1);
            o[2] = to8(v2);
            o[3] = (uint8_t)(haskey && v0 == key[0] && v1 == key[1] && v2 == key[2] ? 0 : 255);
            break;
        }
        case 3:
            if (v0 >= npal)
                img_fail(E_IF);
            memcpy(o, pal + 4u * v0, 4);
            irow[j] = (uint8_t)v0;
            break;
        case 4:
            o[0] = o[1] = o[2] = to8(v0);
            o[3] = to8(sample(x, s + 1));
            break;
        default:
            o[0] = to8(v0);
            o[1] = to8(sample(x, s + 1));
            o[2] = to8(sample(x, s + 2));
            o[3] = to8(sample(x, s + 3));
            break;
        }
    }
    emit(py);
    if (++py == ph)
        img_fail(IMG_STOP);
    x = prv;
    prv = cur;
    cur = x;
}

static void zout(uint8_t b)
{
    win[wpos++ & 0x7FFF] = b;
    cur[rpos++] = b;
    if (rpos == rowlen + 1u) {
        png_row();
        rpos = 0;
    }
}

static void inflate(void)
{
    uint8_t *lens = (uint8_t *)salloc(288 + 32);
    uint8_t cmf = zbyte(), flg = zbyte(), last;
    if ((cmf & 15) != 8 || (uint16_t)((uint16_t)cmf << 8 | flg) % 31u || (flg & 0x20))
        img_fail(E_IF);
    win = (uint8_t *)salloc(32768u);
    lt = (ztree_t *)salloc(sizeof(ztree_t));
    dt = (ztree_t *)salloc(sizeof(ztree_t));
    wpos = 0;
    zcnt = 0;
    do {
        uint16_t type, i;
        last = (uint8_t)zbits(1);
        type = zbits(2);
        if (type == 0) { /* stored */
            uint16_t n;
            zcnt = 0;
            n = zbyte();
            n = (uint16_t)(n | (uint16_t)zbyte() << 8);
            zbyte();
            zbyte();
            while (n--)
                zout(zbyte());
            continue;
        }
        if (type == 1) {
            for (i = 0; i < 288; i++)
                lens[i] = (uint8_t)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
            zbuild(lt, lens, 288);
            for (i = 0; i < 30; i++)
                lens[i] = 5;
            zbuild(dt, lens, 30);
        } else if (type == 2) {
            uint16_t hlit = (uint16_t)(zbits(5) + 257u), hdist = (uint16_t)(zbits(5) + 1u);
            uint16_t hclen = (uint16_t)(zbits(4) + 4u), n = 0;
            memset(lens, 0, 19);
            for (i = 0; i < hclen; i++)
                lens[clen_order[i]] = (uint8_t)zbits(3);
            zbuild(lt, lens, 19);
            while (n < hlit + hdist) {
                uint16_t s = zdec(lt), rep = 0;
                uint8_t v = 0;
                if (s < 16) {
                    lens[n++] = (uint8_t)s;
                    continue;
                }
                if (s == 16) {
                    if (!n)
                        img_fail(E_IF);
                    v = lens[n - 1];
                    rep = (uint16_t)(3u + zbits(2));
                } else if (s == 17)
                    rep = (uint16_t)(3u + zbits(3));
                else
                    rep = (uint16_t)(11u + zbits(7));
                if (n + rep > hlit + hdist)
                    img_fail(E_IF);
                while (rep--)
                    lens[n++] = v;
            }
            zbuild(lt, lens, hlit);
            zbuild(dt, lens + hlit, hdist);
        } else
            img_fail(E_IF);
        for (;;) {
            uint16_t s = zdec(lt), len, dist;
            if (s < 256) {
                zout((uint8_t)s);
                continue;
            }
            if (s == 256)
                break;
            s = (uint16_t)(s - 257u);
            if (s >= 29)
                img_fail(E_IF);
            len = (uint16_t)(len_base[s] + zbits(len_extra[s]));
            s = zdec(dt);
            if (s >= 30)
                img_fail(E_IF);
            dist = (uint16_t)(dist_base[s] + zbits(dist_extra[s]));
            while (len--)
                zout(win[(uint16_t)(wpos - dist) & 0x7FFF]);
        }
    } while (!last);
    img_fail(E_IF); /* the data ended before the last row */
}

static void png_load(void)
{
    static const uint8_t sig[7] = { 'P', 'N', 'G', 13, 10, 26, 10 };
    uint8_t b[7], trns[256];
    uint16_t ntrns = 0, i;
    int ihdr = 0;
    for (i = 0; i < 7; i++)
        b[i] = rd8();
    if (memcmp(b, sig, 7))
        img_fail(E_IF);
    pal = (uint8_t *)salloc(256 * 4);
    npal = 0;
    haskey = 0;
    for (;;) {
        uint32_t len = rd32(), type = rd32();
        if (type == 0x49484452UL) { /* IHDR */
            uint32_t w = rd32(), h = rd32();
            uint8_t ok;
            pdepth = rd8();
            ptype = rd8();
            if (len != 13 || w > 65535UL || h > 65535UL || rd8() || rd8())
                img_fail(E_IF);
            if (rd8())
                img_fail(E_IF); /* interlaced */
            pw = (uint16_t)w;
            ph = (uint16_t)h;
            switch (ptype) {
            case 0: pch = 1; ok = pdepth == 1 || pdepth == 2 || pdepth == 4 || pdepth == 8 || pdepth == 16; break;
            case 2: pch = 3; ok = pdepth == 8 || pdepth == 16; break;
            case 3: pch = 1; ok = pdepth == 1 || pdepth == 2 || pdepth == 4 || pdepth == 8; break;
            case 4: pch = 2; ok = pdepth == 8 || pdepth == 16; break;
            case 6: pch = 4; ok = pdepth == 8 || pdepth == 16; break;
            default: ok = 0;
            }
            if (!ok)
                img_fail(E_IF);
            ihdr = 1;
        } else if (!ihdr) {
            img_fail(E_IF);
        } else if (type == 0x504C5445UL) { /* PLTE */
            if (len % 3u || len > 768)
                img_fail(E_IF);
            npal = (uint16_t)(len / 3u);
            for (i = 0; i < npal; i++) {
                pal[4 * i] = rd8();
                pal[4 * i + 1] = rd8();
                pal[4 * i + 2] = rd8();
                pal[4 * i + 3] = 255;
            }
        } else if (type == 0x74524E53UL) { /* tRNS */
            if (ptype == 3) {
                if (len > 256)
                    img_fail(E_IF);
                ntrns = (uint16_t)len;
                for (i = 0; i < ntrns; i++)
                    trns[i] = rd8();
            } else if ((ptype == 0 && len == 2) || (ptype == 2 && len == 6)) {
                for (i = 0; i < len / 2u; i++)
                    key[i] = rd16();
                haskey = 1;
            } else
                skip(len);
        } else if (type == 0x49444154UL) { /* IDAT */
            uint32_t bits = (uint32_t)pw * pch * pdepth;
            if (ptype == 3 && !npal)
                img_fail(E_IF);
            for (i = 0; i < ntrns && i < npal; i++)
                pal[4 * i + 3] = trns[i];
            if ((bits + 7u) / 8u > 65000UL)
                img_fail(E_OM);
            rowlen = (uint16_t)((bits + 7u) / 8u);
            pbpp = (uint8_t)(pch * pdepth >= 8 ? pch * pdepth / 8u : 1u);
            start(pw, ph, 0, ptype == 3 ? npal : 0, pal);
            cur = (uint8_t *)salloc(rowlen + 1u);
            prv = (uint8_t *)salloc(rowlen + 1u);
            rpos = 0;
            py = 0;
            idat_left = len;
            inflate();
            return;
        } else if (type == 0x49454E44UL) { /* IEND */
            img_fail(E_IF);
        } else {
            skip(len);
        }
        skip(4); /* CRC */
    }
}

/* ---- palettes ----
 * PALETTE LOAD reads an indexed PNG's own palette (every entry, in order),
 * the distinct colors of any other image in the order they appear (a
 * palette strip), or a GIMP (.GPL) or JASC (.PAL) palette text file.
 */

static uint8_t (*pal_out)[3];
static int pal_n, pal_max;

static void pal_add(uint8_t r, uint8_t g, uint8_t b, int dedup)
{
    int i;
    if (dedup)
        for (i = 0; i < pal_n; i++)
            if (pal_out[i][0] == r && pal_out[i][1] == g && pal_out[i][2] == b)
                return;
    if (pal_n >= pal_max)
        img_fail(E_TC);
    pal_out[pal_n][0] = r;
    pal_out[pal_n][1] = g;
    pal_out[pal_n][2] = b;
    pal_n++;
}

static uint8_t pal_indexed;

static void pal_start(img_sink_t *s, const img_info_t *in)
{
    uint16_t i;
    (void)s;
    pal_indexed = in->npal != 0;
    /* indexed files are often padded to 256 entries: take what fits */
    for (i = 0; i < in->npal && pal_n < pal_max; i++)
        pal_add(in->pal[4 * i], in->pal[4 * i + 1], in->pal[4 * i + 2], 0);
}

static int pal_row(img_sink_t *s, uint16_t y, const uint8_t *px, const uint8_t *idx)
{
    uint16_t x;
    (void)s;
    (void)y;
    (void)idx;
    if (pal_indexed)
        return 1;
    for (x = 0; x < rgba_w; x++, px += 4)
        if (px[3] >= 128)
            pal_add(px[0], px[1], px[2], 1);
    return 0;
}

/* One text line into buf; returns 0 at the end of the file. */
static int rdline(char *buf, int max)
{
    int n = 0, c;
    while ((c = rdb()) >= 0 && c != '\n')
        if (c != '\r' && n < max - 1)
            buf[n++] = (char)c;
    buf[n] = 0;
    return c >= 0 || n > 0;
}

static void pal_text(void)
{
    char line[96];
    while (rdline(line, (int)sizeof line)) {
        int v[3], k = 0;
        const char *q = line;
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q < '0' || *q > '9')
            continue; /* names, comments, Columns: ... */
        while (k < 3 && *q >= '0' && *q <= '9') {
            int x = 0;
            while (*q >= '0' && *q <= '9')
                x = x * 10 + (*q++ - '0');
            if (x > 255)
                img_fail(E_IF);
            v[k++] = x;
            while (*q == ' ' || *q == '\t')
                q++;
        }
        if (k == 3)
            pal_add((uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], 0);
        /* JASC's version and count lines have one number each: skipped */
    }
}

/* ---- entry ---- */

static void (*job)(void);

static void run(const char *name)
{
    int e;
    img_fh = hal_fopen(name, 0);
    if (img_fh < 0)
        error(E_FNF);
    fpos = flen = 0;
    str_gc();
    scr_lo = str_bot;
    e = setjmp(img_jb);
    if (!e) {
        job();
        e = IMG_STOP + 1;
    }
    hal_fclose(img_fh);
    if (e != IMG_STOP + 1)
        error(e - 1);
}

static void image_job(void)
{
    uint8_t b = rd8();
    if (b == 0xFF)
        jpeg_load();
    else if (b == 0x89)
        png_load();
    else
        img_fail(E_IF);
}

void img_load(const char *name, img_sink_t *s)
{
    sink = s;
    job = image_job;
    run(name);
}

static void palette_job(void)
{
    static img_sink_t ps = { pal_start, pal_row };
    char head[12];
    int i, b = rd8();
    if (b == 0xFF || b == 0x89) {
        sink = &ps;
        if (b == 0xFF)
            jpeg_load();
        else
            png_load();
        return;
    }
    head[0] = (char)b;
    for (i = 1; i < 8; i++)
        head[i] = (char)rd8();
    head[8] = 0;
    if (memcmp(head, "GIMP Pal", 8) && memcmp(head, "JASC-PAL", 8))
        img_fail(E_IF);
    while (rdb() >= 0 && fbuf[fpos - 1] != '\n')
        ; /* rest of the header line */
    pal_text();
}

int img_palette(const char *name, uint8_t (*out)[3], int max)
{
    pal_out = out;
    pal_n = 0;
    pal_max = max;
    job = palette_job;
    run(name);
    return pal_n;
}
