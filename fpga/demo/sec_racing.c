/* Section 4: a third-person racer that plays itself.
 *
 * A pseudo-3D road: every scanline below the horizon is one slice of the
 * road at depth z = CZ / i (i = rows below the horizon). A row is drawn as
 * seven whole-byte runs (grass | rumble | road | lane dash | road | rumble |
 * grass) with 16-bit stores; the bands (grass, rumble strips, lane dashes)
 * come from (z + distance travelled), so they stream toward the viewer.
 * The curves are the cheap kind: the road's centre line is accumulated row
 * by row from the track's curvature at that row's depth. Hills are the
 * horizon rising and falling. The mountains are pre-drawn as a 640-wide
 * strip in BUF2 and copied behind the horizon, shifted sideways on curves
 * (only when the shift changes). Trees, poles, signs, rival cars and the
 * player's car are drawn from rectangles and spans, so they scale smoothly
 * with distance, far to near. Two bitmaps are drawn in turn.
 *
 * The autopilot holds a lane, changes lane to overtake and slows for a
 * blocked road. */

#define RC_NI __attribute__((noinline))
#define RC_COLD __attribute__((noinline, minsize))
#define RC_HYC    172                    /* the horizon row, before the hills */
#define RC_HYMAX  190                    /* the lowest it gets (mountain strip's last row) */
#define RC_MT     118                    /* the strip's first row */
#define RC_NR     218                    /* rows below the horizon, at most */
#define RC_CZ     24576u                 /* z = CZ / i */
#define RC_NOBJ   16
#define RC_NRIV   5
#define RC_IFAR   40                     /* rows nearer the horizon than this: no bands */
#define RC_PW     96                    /* the player's car: width, bottom row */
#define RC_PY     336

static void rc_rect(int x, int y, int w, int h, uint8_t c);

static const int8_t rc_curv[32] = {
    0, 0, 0, 1, 2, 3, 3, 3, 2, 1, 0, 0, 0, -1, -2, -3,
   -3, -3, -3, -2, -1, 0, 0, 0, 1, 2, 3, 3, 2, 0, 0, 0 };

static uint16_t *rc_z;                   /* z of row i */
static int16_t  *rc_cx;                  /* the road's centre x on row i, this frame */

/* the simulation */
static uint32_t rc_posf;                 /* distance, 8.8 */
static uint16_t rc_v;                    /* speed, 8.8 a frame */
static int      rc_plx;                  /* lateral position, +-128 = lane centres */
static int      rc_lane;                 /* the lane chosen */
static int      rc_lean, rc_leant;       /* x16 */
static int     rc_mx;                   /* mountain shift, 1/16 pixel (0..10239) */
static uint32_t rc_tf;                   /* frames since the start */
static uint16_t rc_passes, rc_flash;

static int16_t  rc_od[RC_NOBJ];          /* distance ahead, 1/8 unit */
static int8_t   rc_olat[RC_NOBJ];        /* lateral, 1/32 of the road half-width, signed */
static uint8_t  rc_otype[RC_NOBJ], rc_ocol[RC_NOBJ];
static int16_t  rc_rd[RC_NRIV];
static uint16_t rc_rv[RC_NRIV];
static int16_t  rc_rlane[RC_NRIV];
static uint8_t  rc_rcol[RC_NRIV];

static uint8_t *rc_gc[2];                /* per bitmap and row: the grass colour drawn */
static int16_t *rc_bl[2], *rc_br[2];     /* per bitmap and 8-row block: the span (bytes) things drew on */
static int16_t *rc_gl[2], *rc_gr[2];     /* ... and the grass edges (bytes) */
static int rc_cur;
static int rc_bws[2], rc_hyp[2];         /* per bitmap: mountain shift drawn, lowest horizon so far */
static int rc_top[2];                    /* per bitmap: the highest row things drew in (above the horizon: restored) */

/* ------------------------------------------------------------------ scenery */
RC_COLD static uint8_t rc_skyc(int y)
{
    if (y < 32) return BLUE;
    if (y < 58) return (y & 3) == 0 ? CYAN : BLUE;
    if (y < 80) return (y & 1) ? BLUE : CYAN;
    if (y < 98) return (y & 3) == 0 ? BLUE : CYAN;
    if (y < 142) return CYAN;
    if (y < 156) return (y & 3) == 0 ? WHITE : CYAN;
    if (y < 168) return (y & 1) ? WHITE : CYAN;
    return WHITE;
}

RC_COLD static void rc_sky(void)
{
    for (int y = 0; y < RC_MT; y++) rc_rect(0, y, W, 1, rc_skyc(y));
    for (int dy = -22; dy <= 22; dy++) {          /* the sun */
        int hw = 22;
        while (hw * hw + dy * dy > 22 * 22) hw--;
        rc_rect(500 - hw, 84 + dy, 2 * hw + 1, 1, YELLOW);
    }
    for (int k = 0; k < 7; k++) {                 /* clouds: three stacked bars */
        int cx = 40 + k * 95 + (int)rnd_n(40), cy = 40 + (int)rnd_n(60);
        int rx = 22 + (int)rnd_n(20);
        rc_rect(cx - rx, cy - 3, 2 * rx, 7, WHITE);
        rc_rect(cx - rx + 6, cy - 6, 2 * rx - 12, 13, WHITE);
        rc_rect(cx - 8, cy - 10, rx + 4, 20, WHITE);
    }
}

/* the mountain strip: BUF2, rows RC_MT..RC_HYMAX, 640 wide and wrapping */
RC_COLD static void rc_mount(void)
{
    gfx_target(BUF2);
    for (int y = RC_MT; y <= RC_HYMAX; y++) rc_rect(0, y, W, 1, rc_skyc(y));
    int base = RC_HYMAX;
    for (int x = 0; x < W; x++) {
        int m = (127 - (SIN(x * 6 / 5) < 0 ? -SIN(x * 6 / 5) : SIN(x * 6 / 5))) / 5
              + (SIN(x * 2) + 127) / 16 + (SIN(x * 14 / 5) + 127) / 32;
        int hf = 14 + m;
        rc_rect(x, base - hf, 1, hf + 1, MAGENTA);
        if (hf > 40) rc_rect(x, base - hf, 1, (hf - 38) / 2 + 1, WHITE);
        int hn = 4 + (SIN(x * 8 / 5) + 127) / 16 + (SIN(x * 18 / 5) + 127) / 40;
        rc_rect(x, base - hn, 1, hn + 1, GREEN);
    }
}

/* rows y0..y1 of the strip into bitmap dst, moved ws words (4 pixels) left */
RC_NI static void rc_strip(uint32_t dst, int ws, int y0, int y1)
{
    for (int y = y0; y <= y1; y++) {
        uint32_t o = rowoff[y];
        uint16_t *d = (uint16_t *)(dst + o);
        const uint16_t *s = (const uint16_t *)(BUF2 + o);
        const uint16_t *s1 = s + ws;
        int k = 160 - ws;
        while (k >= 4) { d[0] = s1[0]; d[1] = s1[1]; d[2] = s1[2]; d[3] = s1[3]; d += 4; s1 += 4; k -= 4; }
        while (k-- > 0) *d++ = *s1++;
        k = ws;
        while (k >= 4) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; d += 4; s += 4; k -= 4; }
        while (k-- > 0) *d++ = *s++;
    }
}

/* a filled rectangle, lean: per row, byte writes for narrow ones */
RC_NI static void rc_rect(int x, int y, int w, int h, uint8_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;
    int x1 = x + w - 1;
    int lo = x & 1, hi = !(x1 & 1);
    int nm = (x1 >> 1) - (x >> 1) + 1 - lo - hi;
    uint8_t cc = (uint8_t)(c << 4 | c), hc = (uint8_t)(c << 4);
    uint16_t vv = (uint16_t)(cc | cc << 8);
    uint8_t *p = g_buf + rowoff[y] + (x >> 1);
    for (; h > 0; h--, p += STRIDE) {
        uint8_t *q = p;
        if (lo) { *q = (uint8_t)((*q & 0xF0) | c); q++; }
        int n = nm;
        if (n >= 6 && ((uintptr_t)q & 1)) { *q++ = cc; n--; }
        if (n >= 6) {
            uint16_t *d = (uint16_t *)q;
            int k = n >> 1;
            while (k--) *d++ = vv;
            q = (uint8_t *)d;
            if (n & 1) *q++ = cc;
        } else {
            while (n--) *q++ = cc;
        }
        if (hi) *q = (uint8_t)((*q & 0x0F) | hc);
    }
}

/* ------------------------------------------------------------------ the road */
/* bytes b0..b1-1 of a row (2 pixels a byte) */
RC_NI static void rc_fb(uint8_t *r, int b0, int b1, uint8_t v)
{
    if (b0 < 0) b0 = 0;
    if (b1 > 320) b1 = 320;
    if (b1 <= b0) return;
    uint8_t *p = r + b0;
    int n = b1 - b0;
    if (b0 & 1) { *p++ = v; n--; }
    uint16_t vv = (uint16_t)(v | v << 8);
    uint16_t *q = (uint16_t *)p;
    int k = n >> 1;
    while (k >= 8) { q[0] = vv; q[1] = vv; q[2] = vv; q[3] = vv; q[4] = vv; q[5] = vv; q[6] = vv; q[7] = vv; q += 8; k -= 8; }
    while (k-- > 0) *q++ = vv;
    if (n & 1) *(uint8_t *)q = v;
}

/* two rows at once (r and r + STRIDE): bytes b0..b1-1 */
RC_NI static void rc_fb2(uint8_t *r, int b0, int b1, uint8_t v)
{
    if (b0 < 0) b0 = 0;
    if (b1 > 320) b1 = 320;
    if (b1 <= b0) return;
    uint8_t *p = r + b0;
    int n = b1 - b0;
    if (b0 & 1) { p[0] = v; p[STRIDE] = v; p++; n--; }
    uint16_t vv = (uint16_t)(v | v << 8);
    uint16_t *q = (uint16_t *)p;
    int k = n >> 1;
    while (k >= 4) {
        q[0] = vv; q[STRIDE / 2] = vv; q[1] = vv; q[STRIDE / 2 + 1] = vv;
        q[2] = vv; q[STRIDE / 2 + 2] = vv; q[3] = vv; q[STRIDE / 2 + 3] = vv;
        q += 4; k -= 4;
    }
    while (k-- > 0) { q[0] = vv; q[STRIDE / 2] = vv; q++; }
    if (n & 1) { ((uint8_t *)q)[0] = v; ((uint8_t *)q)[STRIDE] = v; }
}

/* The road in pairs of rows (the horizon row is even, so is each pair's
 * lower row). A pair already drawn in this bitmap with the same grass colour
 * keeps its grass: only the strip between the old and new grass edges is
 * redrawn. */
RC_NI static void rc_road(int hy, uint16_t pos)
{
    int b = rc_cur, hyold = rc_hyp[b];
    int n = H - hy;
    int32_t xa = 0;
    int dx = 0;
    int plx = rc_plx;
    uint8_t *gc = rc_gc[b];
    int16_t *gl = rc_gl[b], *gr = rc_gr[b], *bl = rc_bl[b], *br = rc_br[b];
    uint8_t *rp = g_buf + (uint32_t)(H - 2) * STRIDE;      /* the pair's upper row */
    for (int i = n; i >= 2; i -= 2, rp -= 2 * STRIDE) {
        uint16_t w = rc_z[i] + pos;
        int c = rc_curv[(w >> 8) & 31];
        xa += 2 * dx + 3 * c;
        dx += 2 * c;
        int hw = i + (i >> 3);
        int cx = 320 + (int)(xa >> 8) - ((plx * hw) >> 8);
        rc_cx[i >> 1] = (int16_t)cx;
        int cb = cx >> 1, hb = hw >> 1, rb = (hb >> 3) + 1;
        int e1 = cb - hb, e2 = cb + hb;
        uint8_t g, rum;
        int dash = 0;
        if (i >= RC_IFAR) {
            g = (w & 32) ? GREEN : DGREEN;
            rum = (w & 16) ? RED : WHITE;
            dash = (w & 32) && i >= 12;
        } else {
            g = (i & 2) ? GREEN : DGREEN;
            rum = (i & 2) ? RED : WHITE;
        }
        int y = hy + i;
        int e0 = e1 - rb, e3 = e2 + rb;
        if (e0 < 0) e0 = 0; else if (e0 > 320) e0 = 320;
        if (e3 < 0) e3 = 0; else if (e3 > 320) e3 = 320;
        int lo = 0, hi = 320;
        if (gc[y] == g && y > hyold) {
            lo = gl[y] < e0 ? gl[y] : e0;
            hi = gr[y] > e3 ? gr[y] : e3;
            int k = (y - 2) >> 3, k2 = (y - 1) >> 3;   /* the pair is rows y-2, y-1 */
            if (bl[k] < lo) lo = bl[k];
            if (br[k] > hi) hi = br[k];
            if (bl[k2] < lo) lo = bl[k2];
            if (br[k2] > hi) hi = br[k2];
        }
        gc[y] = g; gl[y] = (int16_t)e0; gr[y] = (int16_t)e3;
        if (lo < e0) rc_fb2(rp, lo, e0, g * 17);
        rc_fb2(rp, e0, e1, rum * 17);
        if (dash) {
            int lw = (hb >> 6) + 1;
            rc_fb2(rp, e1, cb - lw, GREY * 17);
            rc_fb2(rp, cb - lw, cb + lw, WHITE * 17);
            rc_fb2(rp, cb + lw, e2, GREY * 17);
        } else {
            rc_fb2(rp, e1, e2, GREY * 17);
        }
        rc_fb2(rp, e2, e3, rum * 17);
        if (e3 < hi) rc_fb2(rp, e3, hi, g * 17);
    }
    for (int k = 0; k < 46; k++) { bl[k] = 320; br[k] = 0; }
}

/* a thing drew in rows y0..y1, pixels x0..x1: redraw that next time this bitmap is used */
RC_NI static void rc_mark(int y0, int y1, int x0, int x1)
{
    if (y0 < 0) y0 = 0;
    if (y1 >= H) y1 = H - 1;
    int xl = x0 < 0 ? 0 : x0 >> 1, xr = x1 >= W ? 320 : (x1 >> 1) + 1;
    int16_t *l = rc_bl[rc_cur], *r = rc_br[rc_cur];
    if (y0 < rc_top[rc_cur]) rc_top[rc_cur] = y0;
    for (int k = y0 >> 3; k <= (y1 >> 3); k++) {
        if (xl < l[k]) l[k] = (int16_t)xl;
        if (xr > r[k]) r[k] = (int16_t)xr;
    }
}

/* ------------------------------------------------------------------ things */
/* rows y, y+1 of a blob centred on byte bx, half-width hb bytes, colour c */
RC_NI static void rc_row2(int y, int bx, int hb, uint8_t c, int two)
{
    uint8_t cc = (uint8_t)(c << 4 | c);
    if (y >= 0 && y + 1 < H && two) rc_fb2(g_buf + rowoff[y], bx - hb, bx + hb + 1, cc);
    else {
        if ((unsigned)y < H) rc_fb(g_buf + rowoff[y], bx - hb, bx + hb + 1, cc);
        if (two && (unsigned)(y + 1) < H) rc_fb(g_buf + rowoff[y + 1], bx - hb, bx + hb + 1, cc);
    }
}

/* a cone with a serrated edge, apex at row top, ch rows, base half-width bw pixels */
RC_NI static void rc_cone(int x, int top, int ch, int bw, uint8_t c, uint8_t c2)
{
    if (ch < 2) return;
    int bx = x >> 1;
    int tl = ((ch * 85) >> 8) + 1;
    int s = (bw << 8) / ch;                      /* 8.8 pixels a row */
    int fs = (26 << 8) / tl, f = 38 << 8, lin = s >> 1, k = 0;
    for (int r = 0; r < ch; r += 2) {
        int hh = (((lin >> 8) * (f >> 8)) >> 6);
        rc_row2(top + r, bx, hh >> 1, c, r + 1 < ch);
        lin += 2 * s; f += 2 * fs; k += 2;
        if (k >= tl) { k = 0; f = 38 << 8; { uint8_t t = c; c = c2; c2 = t; } }
    }
}

RC_NI static void rc_pine(int x, int y, int h)
{
    int tw = (h >> 5) + 1, th = (h >> 3) + 1;
    rc_rect(x - tw, y - th, 2 * tw + 1, th, OLIVE);
    rc_cone(x, y - h, h - th, (h * 3) >> 3, TEAL, DGREEN);
}

RC_NI static void rc_round(int x, int y, int h, uint8_t c1, uint8_t c2)
{
    int tw = (h >> 5) + 1, th = (h >> 2) + 1;
    rc_rect(x - tw, y - th, 2 * tw + 1, th, OLIVE);
    int ch = h - th, bw = (h * 3) >> 3;
    if (ch < 2) return;
    int step = (int)(128u / (uint16_t)ch), bx = x >> 1;
    int a = step >> 1, top = y - h;
    for (int r = 0; r < ch; r += 2, a += 2 * step) {
        int hh = (SIN(a) * bw) >> 7;
        rc_row2(top + r, bx, hh >> 1, r < (ch * 2) / 5 ? c1 : c2, r + 1 < ch);
    }
}

RC_NI static void rc_pole(int x, int y, int h, int side)
{
    int pw = (h >> 5) + 1;
    int arm = h >> 2;
    rc_rect(x, y - h, pw, h, GREY);
    if (side > 0) {
        rc_rect(x - arm, y - h, arm, pw, GREY);
        rc_rect(x - arm, y - h + pw, pw + 1, pw + 1, YELLOW);
    } else {
        rc_rect(x + pw, y - h, arm, pw, GREY);
        rc_rect(x + pw + arm - pw - 1, y - h + pw, pw + 1, pw + 1, YELLOW);
    }
}

RC_NI static void rc_sign(int x, int y, int h, uint8_t col)
{
    int pw = (h >> 5) + 1;
    int pwid = (h >> 2) + 3, phgt = (h >> 3) + 3;
    int top = y - (h * 3) / 4;
    rc_rect(x, top, pw, y - top, GREY);
    rc_rect(x - pwid / 2, top - phgt, pwid, phgt, WHITE);
    if (pwid > 6) rc_rect(x - pwid / 2 + 1, top - phgt + 1, pwid - 2, phgt - 2, col);
}

/* a car seen from behind, w pixels wide, its wheels on row yb; lean -3..3 */
RC_NI static void rc_car(int x, int yb, int w, uint8_t body, uint8_t lamp, int lean, int stripe)
{
    int hl = w >> 1, wl = (w >> 3) + 1, bh = (w >> 2) + 1;
    int ls = lean * ((w >> 5) + (w >= 32 ? 0 : 0)), bs = ls >> 1;
    int by = yb - wl + 1 - bh;
    int cw = (w >> 1) + (w >> 3), chh = (w >> 2) - (w >> 4) + 1;
    int cx0 = x - (cw >> 1) + ls, cy0 = by - chh;
    rc_rect(x - hl - 2, yb - 1, w + 4, 2 + (w >> 5), BLACK);
    rc_rect(x - hl, yb - wl + 1, wl, wl, BLACK);
    rc_rect(x + hl - wl, yb - wl + 1, wl, wl, BLACK);
    rc_rect(x - hl + 1 + bs, by, w - 2, bh, body);
    rc_rect(cx0 + (w >> 4), cy0, cw - (w >> 3), 1 + (w >> 5), body);
    rc_rect(cx0, cy0 + (w >> 5) + 1, cw, chh, body);
    if (w >= 14) {
        int in = 1 + (w >> 5);
        rc_rect(cx0 + in, cy0 + (w >> 5) + 1 + (w >> 6), cw - 2 * in, chh - (w >> 5) - 1 - (w >> 6), CYAN);
    }
    if (stripe) {
        int sw = (w >> 4) + 1;
        rc_rect(x + bs - sw, by, 2 * sw, bh, stripe);
        if (w >= 14) rc_rect(x + ls * 3 / 4 - sw, cy0 + (w >> 5) + 1, 2 * sw, chh, stripe);
    }
    int lw = (w >> 3) + 1, lh = (w >> 4) + 1;
    rc_rect(x - hl + 1 + bs + (w >> 4), by + 1 + (w >> 5), lw, lh, lamp);
    rc_rect(x + hl - 1 + bs - (w >> 4) - lw, by + 1 + (w >> 5), lw, lh, lamp);
    rc_rect(x - hl + 1 + bs, by + bh - 1 - (w >> 5), w - 2, 1 + (w >> 5), BLACK);
    if (w >= 24) rc_rect(x + bs - (w >> 4), by + (bh >> 1), (w >> 3), (w >> 4), WHITE);
}

/* The player's car: seven leans pre-drawn on transparent (8) in BUF3 (4
 * across, 2 down, 160 x 80 pixels each). Each row's span of 16-bit words
 * that holds car is noted, and only that is copied: the words inside it
 * whole, the two at its ends nibble by nibble (8 keeps what is beneath). */
#define RC_BX   128
#define RC_BH   70
#define RC_BW   (RC_BX / 4)                     /* 16-bit words a row */
static uint8_t *rc_span;                        /* [lean][row]: first word, last word + 1 */
RC_COLD static void rc_pre(void)
{
    gfx_target(BUF3);
    rc_span = (uint8_t *)salloc(7 * RC_BH * 2);
    for (int sl = 0; sl < 7; sl++) {
        int sx = (sl & 3) * 160, sy = (sl >> 2) * 80;
        rc_rect(sx, sy, RC_BX, RC_BH, TRANSP);
        rc_car(sx + RC_BX / 2 + (sl - 3) * 2, sy + RC_BH - 5, RC_PW, RED, YELLOW, sl - 3, WHITE);
        for (int r = 0; r < RC_BH; r++) {
            const uint16_t *w = (const uint16_t *)(BUF3 + rowoff[sy + r] + (uint32_t)(sx >> 1));
            int a0 = RC_BW, a1 = 0;
            for (int k = 0; k < RC_BW; k++)
                if (w[k] != 0x8888) { if (a0 == RC_BW) a0 = k; a1 = k + 1; }
            uint8_t *sp = rc_span + (sl * RC_BH + r) * 2;
            sp[0] = (uint8_t)a0; sp[1] = (uint8_t)a1;
        }
    }
}

/* d gets s's nibbles, except where s has 8 */
static uint16_t rc_merge(uint16_t d, uint16_t s)
{
    for (int k = 0; k < 16; k += 4)
        if (((s >> k) & 15) != 8) d = (uint16_t)((d & ~(15u << k)) | (s & (15u << k)));
    return d;
}

RC_COLD static void rc_player(int lean)
{
    int sl = lean + 3;
    uint32_t so = rowoff[(sl >> 2) * 80] + (uint32_t)((sl & 3) * 80);
    uint32_t doff = rowoff[RC_PY - RC_BH + 5] + (uint32_t)((320 - RC_BX / 2) >> 1);
    const uint8_t *sp = rc_span + sl * RC_BH * 2;
    for (int r = 0; r < RC_BH; r++, so += STRIDE, doff += STRIDE, sp += 2) {
        int a0 = sp[0], a1 = sp[1];
        if (a0 >= a1) continue;
        const uint16_t *s = (const uint16_t *)(BUF3 + so);
        uint16_t *d = (uint16_t *)(g_buf + doff);
        d[a0] = rc_merge(d[a0], s[a0]);
        if (a1 - 1 > a0) d[a1 - 1] = rc_merge(d[a1 - 1], s[a1 - 1]);
        for (int k = a0 + 1; k < a1 - 1; k++) d[k] = s[k];
    }
}

RC_NI static void rc_things(int hy, int n)
{
    uint16_t key[RC_NOBJ + RC_NRIV];
    int nk = 0;
    for (int j = 0; j < RC_NOBJ + RC_NRIV; j++) {
        int dd = j < RC_NOBJ ? rc_od[j] : rc_rd[j - RC_NOBJ];
        int d = dd >> 3;
        if (d < 118 || d > 2600) continue;
        int lo = 3, hi = n;
        if (rc_z[hi] > d) continue;               /* nearer than the bottom row */
        while (lo < hi) { int mid = (lo + hi) >> 1; if ((int)rc_z[mid] > d) lo = mid + 1; else hi = mid; }
        int i = lo;
        uint16_t k = (uint16_t)(i << 8 | j);
        int p = nk++;
        while (p > 0 && key[p - 1] > k) { key[p] = key[p - 1]; p--; }
        key[p] = k;
    }
    for (int q = 0; q < nk; q++) {
        int j = key[q] & 255, i = key[q] >> 8;
        int y = hy + i, hw = i + (i >> 3), cx = rc_cx[i >> 1];
        if (j < RC_NOBJ) {
            int lat = rc_olat[j];
            int x = cx + ((lat * hw) >> 5);
            int h = i < 80 ? i : 80;
            if (x < -h || x > W + h) continue;
            rc_mark(y - h - 3, y + 2, x - ((h * 3) >> 3) - 4, x + ((h * 3) >> 3) + 4);
            switch (rc_otype[j]) {
            case 0: rc_pine(x, y, h); break;
            case 1: rc_round(x, y, h, rc_ocol[j], rc_ocol[j] == YELLOW ? OLIVE : rc_ocol[j] == RED ? MAROON : TEAL); break;
            case 2: rc_pole(x, y, h, lat); break;
            default: rc_sign(x, y, (h * 3) >> 2, rc_ocol[j]); break;
            }
        } else {
            int r = j - RC_NOBJ;
            int x = cx + ((rc_rlane[r] * hw) >> 8);
            rc_mark(y - (i >> 1) - 5, y + 3, x - ((i * 5) >> 4) - 6, x + ((i * 5) >> 4) + 6);
            rc_car(x, y, (i * 5) >> 3, rc_rcol[r], RED, 0, 0);
        }
    }
}

/* ------------------------------------------------------------------ drawing a frame */
RC_COLD static void rc_frame(int b, uint32_t buf)
{
    rc_cur = b;
    uint16_t pos = (uint16_t)(rc_posf >> 8);
    int hy = (RC_HYC + ((SIN(pos >> 3) * 4) >> 6) + (SIN(pos >> 2) >> 5)) & ~1;
    int ws = (int)(rc_mx >> 6);
#ifdef RC_FULL                                   /* test: no caches, everything drawn again */
    rc_bws[b] = -1;
    for (int y = 0; y < H; y++) rc_gc[b][y] = 0xFF;
#endif
    /* The strip ends a row above the road (row hy is the road's top row,
     * and a cached road row would keep what the strip put there). */
    if (ws != rc_bws[b]) {
        rc_strip(buf, ws, RC_MT, hy - 1);
        rc_bws[b] = ws;
    } else {
        /* rows the horizon left (from the old road top row), and rows
         * above the horizon that things drew in */
        int y0 = rc_top[b] < rc_hyp[b] ? rc_top[b] : rc_hyp[b];
        if (y0 < RC_MT) y0 = RC_MT;
        if (y0 <= hy - 1) rc_strip(buf, ws, y0, hy - 1);
    }
    rc_top[b] = H;
    rc_road(hy, pos);
    rc_hyp[b] = hy;
    rc_things(hy, H - hy);
    int lean = (rc_lean + (rc_lean < 0 ? -8 : 8)) / 16;
    rc_mark(RC_PY - RC_BH + 5, RC_PY + 5, 320 - RC_BX / 2, 320 + RC_BX / 2);
    rc_player(lean);
}

/* ------------------------------------------------------------------ the world */
RC_COLD static void rc_obj_new(int j, int d)
{
    uint16_t r = rnd16();
    int lat = 45 + (int)(r & 15);
    rc_olat[j] = (int8_t)((r & 0x100) ? lat : -lat);
    uint16_t t = rnd_n(10);
    rc_otype[j] = t < 4 ? 0 : t < 7 ? 1 : t < 9 ? 2 : 3;
    rc_ocol[j] = (uint8_t)(t == 4 ? YELLOW : t == 5 ? RED : t == 6 ? DGREEN : t == 7 ? BLUE : t == 8 ? MAGENTA : RED);
    rc_od[j] = (int16_t)(d << 3);
}

static const uint8_t rc_colors[5] = { YELLOW, CYAN, WHITE, MAGENTA, BLUE };

RC_COLD static void rc_riv_new(int r, int d)
{
    int lane = (rnd16() & 1) ? 128 : -128;
    for (int k = 0; k < RC_NRIV; k++) {          /* keep clear of a car in the same lane */
        if (k == r) continue;
        int dk = rc_rd[k] >> 3;
        if (dk > d - 450 && dk < d + 450 && d < 3200) d += 450;
    }
    rc_rlane[r] = (int16_t)lane;
    rc_rd[r] = (int16_t)(d << 3);
    rc_rv[r] = (uint16_t)(380 + rnd_n(300));
    rc_rcol[r] = rc_colors[rnd_n(5)];
}

RC_COLD static void rc_init(void)
{
    rc_z = (uint16_t *)salloc((RC_NR + 2) * 2);
    rc_cx = (int16_t *)salloc((RC_NR + 2) * 2);
    for (int i = 1; i <= RC_NR; i++) rc_z[i] = (uint16_t)(RC_CZ / (uint16_t)i);
    rc_z[0] = 0;
    for (int b = 0; b < 2; b++) {
        rc_gc[b] = (uint8_t *)salloc(H);
        rc_bl[b] = (int16_t *)salloc(48 * 2);
        rc_br[b] = (int16_t *)salloc(48 * 2);
        for (int k = 0; k < 48; k++) { rc_bl[b][k] = 320; rc_br[b][k] = 0; }
        rc_gl[b] = (int16_t *)salloc(H * 2);
        rc_gr[b] = (int16_t *)salloc(H * 2);
        for (int y = 0; y < H; y++) rc_gc[b][y] = 255;
    }
    rc_posf = 0; rc_v = 500;
    rc_plx = rc_lane = 128;
    rc_lean = rc_leant = 0;
    rc_mx = 0; rc_tf = 0; rc_passes = 0; rc_flash = 0;
    for (int j = 0; j < RC_NOBJ; j++) rc_obj_new(j, 140 + j * 150 + (int)rnd_n(90));
    for (int r = 0; r < RC_NRIV; r++) { rc_rd[r] = 30000; }
    for (int r = 0; r < RC_NRIV; r++) rc_riv_new(r, 600 + r * 420 + (int)rnd_n(200));
    rc_bws[0] = rc_bws[1] = 0;
    rc_hyp[0] = rc_hyp[1] = RC_HYMAX;
    rc_top[0] = rc_top[1] = H;
}

RC_COLD static void rc_sim(uint16_t dt)
{
    uint16_t pos = (uint16_t)(rc_posf >> 8);
    int c0 = rc_curv[((pos + 170) >> 8) & 31];
    int d4 = dt > 4 ? 4 : dt;
    int ac0 = c0 < 0 ? -c0 : c0;

    /* lane choice: stay, change to overtake, or slow for a blocked road */
    int li = rc_lane > 0;
    int dmin[2] = { 9999, 9999 };
    uint16_t vblock = 0;
    for (int r = 0; r < RC_NRIV; r++) {
        int d = rc_rd[r] >> 3;
        int l = rc_rlane[r] > 0;
        if (d > 100 && d < 1000 && d < dmin[l]) { dmin[l] = d; if (l == li) vblock = rc_rv[r]; }
    }
    int vt = 1500 - ac0 * 90;
    if (dmin[li] < 800) {
        if (dmin[1 - li] > dmin[li] + 200) rc_lane = -rc_lane;
        else if (dmin[li] < 420) { int vb = vblock - 20; if (vt > vb) vt = vb; }
    }
    if (rc_v < vt) { rc_v += 6 * d4; if (rc_v > vt) rc_v = (uint16_t)vt; }
    else { rc_v -= 12 * d4; if (rc_v < vt) rc_v = (uint16_t)vt; }

    /* steering */
    int st = rc_lane - rc_plx;
    int mv = 4 * d4;
    if (st > mv) st = mv; else if (st < -mv) st = -mv;
    rc_plx += st;
    int lt = (rc_lane - rc_plx) / 36 + c0;
    if (lt > 3) lt = 3; else if (lt < -3) lt = -3;
    rc_leant = lt * 16;
    rc_lean += (rc_leant - rc_lean) >> 2;

    rc_posf += (uint32_t)(rc_v * d4);
    rc_mx += (c0 * (int)rc_v * d4) >> 9;
    while (rc_mx < 0) rc_mx += 10240;
    while (rc_mx >= 10240) rc_mx -= 10240;

    int adv = (rc_v * d4) >> 5;
    for (int j = 0; j < RC_NOBJ; j++) {
        rc_od[j] -= adv;
        if (rc_od[j] < (100 << 3)) rc_obj_new(j, 2350 + (int)rnd_n(300));
    }
    for (int r = 0; r < RC_NRIV; r++) {
        rc_rd[r] -= (((int)rc_v - (int)rc_rv[r]) * d4) >> 5;
        if (rc_rd[r] < (110 << 3)) {
            rc_passes++; rc_flash = 70;
            rc_riv_new(r, 1500 + (int)rnd_n(500));
        }
    }
    if (rc_flash > dt) rc_flash -= dt; else rc_flash = 0;
}

/* ------------------------------------------------------------------ HUD */
#define RC_HUD ATTR(WHITE, BLACK)
#define RC_HUDV ATTR(YELLOW, BLACK)

RC_COLD static void rc_hud_static(void)
{
    tfill(0, 0, 80, ' ', RC_HUD);
    tfill(1, 0, 80, ' ', RC_HUD);
    tput(0, 2, "SPEED", ATTR(CYAN, BLACK));
    tput(0, 22, "TIME", ATTR(CYAN, BLACK));
    tput(0, 42, "POSITION", ATTR(CYAN, BLACK));
    tput(0, 62, "OVERTAKES", ATTR(CYAN, BLACK));
    tput(1, 6, "km/h", RC_HUD);
    tput(1, 45, "of 8", RC_HUD);
}

RC_COLD static void rc_hud(void)
{
    uint16_t kmh = (uint16_t)(rc_v * 30u) >> 8;
    tnum(1, 2, kmh, 3, RC_HUDV);
    uint32_t tenths = rc_tf / 6;
    uint16_t secs = (uint16_t)(tenths / 10), tn = (uint16_t)(tenths % 10);
    tnum(1, 22, secs / 60, 1, RC_HUDV);
    tput(1, 23, ":", RC_HUDV);
    tnum(1, 24, secs % 60, 2, RC_HUDV);
    tput(1, 26, ".", RC_HUDV);
    tnum(1, 27, tn, 1, RC_HUDV);
    int pos = 8 - (int)((rc_passes * 3) >> 2);
    if (pos < 1) pos = 1;
    tnum(1, 42, (uint16_t)pos, 1, RC_HUDV);
    tnum(1, 62, rc_passes, 2, RC_HUDV);
    if (rc_flash) tcenter(4, " OVERTAKE! ", ATTR(YELLOW, RED));
    else tfill(4, 30, 20, ' ', A_CLEAR);
}

/* ------------------------------------------------------------------ the section */
static void rc_run(int32_t frames)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    rc_init();
    uint32_t bufs[2] = { BUF0, BUF1 };
    gfx_target(BUF0); rc_sky();                  /* the sky once (its clouds are random), then copied */
    copy_rect(BUF1, BUF0, 0, 0, W, RC_MT);
    rc_mount();
    rc_pre();
    rc_strip(BUF0, 0, RC_MT, RC_HYMAX);
    rc_strip(BUF1, 0, RC_MT, RC_HYMAX);
    rc_hud_static();
    int cur = 0;
    uint16_t fc = 0;
    sec_begin("racing", frames);
    while (sec_running()) {
        gfx_target(bufs[cur]);
        rc_frame(cur, bufs[cur]);
        uint16_t n = frame_wait();
        gfx_show(bufs[cur]);
        cur ^= 1;
        rc_sim(n);
        rc_tf += n;
        if (!(fc++ & 3)) rc_hud();
    }
    tclear();
}
