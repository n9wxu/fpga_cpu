/* Section 1: a platform game that plays itself.
 *
 * "Bit", a little robot, runs through an endless generated world of 16x16
 * tiles: ground with pits, hurdles and dips, floating platforms, spinning
 * coins (some in arcs that follow the jump), clouds, hills, bushes and
 * slimes that patrol back and forth. An autopilot looks ahead in the tile
 * map and jumps pits, climbs hurdles, lands on platforms and stomps (or
 * hops over) slimes; if it ever falls into a pit it drops back in just
 * past it.
 *
 * Scrolling: the 640-pixel bitmap is exactly the screen, so the hardware
 * SCROLLX moves it and only the 2-pixel column (one byte wide) that wraps
 * round to the right edge is drawn anew from the tile map each time the
 * camera moves. Two bitmaps in turn (each keeps its own scroll position
 * and the rectangles its sprites covered, which are put back from the tile
 * map before the next frame's sprites are drawn). int is 16 bits: world
 * positions stay below 32768 (a run of 25 s is about 3000 pixels). */

#define PF_SPD   2                     /* run speed, pixels a step */
#define PF_G     6                     /* gravity, 1/16 pixel a step a step */
#define PF_JV    126                   /* jump speed, 1/16 pixel a step */
#define PF_GROUND 15                   /* ground top row (y 240) */
#define PF_GY    (PF_GROUND * 16)
#define PF_NE    6
#ifdef PF_DEBUG
#define PFD(s) printf("%s tick%u hx%d\n", s, pf_tick, pf_hx)
#else
#define PFD(s)
#endif

enum { PF_SKY, PF_GRASS, PF_DIRT, PF_BLOCK, PF_COIN, PF_CLOUD = 8, PF_BUSH = 11,
       PF_HILLT = 16, PF_HILLB = 32, PF_TREE = 68, PF_NTILES = 102 };

static uint8_t *pf_map;                /* [128][24] tile ids, a ring of columns */
static uint8_t *pf_tiles;              /* PF_NTILES x 128 bytes (16 rows of 8) */
static uint8_t pf_gt[128], pf_pt[128], pf_coin[128];   /* ground row, platform row, coin row (255: none) */
static uint8_t pf_cf;                  /* coin spin frame */
static const sprite_t *pf_sh[4], *pf_ss[2], *pf_sf, *pf_sp[2], *pf_sb[2];

/* ---- the picture of the tiles: drawn in BUF3 once, then cut into tiles ---------------- */
#define PF_NI __attribute__((noinline))
static PF_NI sprite_t *pf_mk(const char *a, int w, int h) { return spr_make(a, w, h, 0); }
static PF_NI void pf_sd(const sprite_t *s, int x, int y) { spr_draw(s, x, y); }
static PF_NI void pf_fr(int x, int y, int w, int h, uint8_t c) { fill_rect(x, y, w, h, c); }
static PF_NI void pf_hl(int x0, int x1, int y, uint8_t c) { hline(x0, x1, y, c); }
static PF_NI void pf_vl(int x, int y0, int y1, uint8_t c) { vline(x, y0, y1, c); }
static PF_NI void pf_ps(int x, int y, uint8_t c) { pset(x, y, c); }
static PF_NI void pf_el(int cx, int cy, int rx, int ry, uint8_t c, int f) { ellipse(cx, cy, rx, ry, c, f); }

static PF_NI void pf_dirt(int x, int y0, int y1, int k)
{
    pf_fr(x, y0, 16, y1 - y0 + 1, RED);
    for (int y = y0; y <= y1; y++)
        for (int i = 0; i < 16; i++)
            if ((((i + k) * 5 + y * 3) & 15) == 0) pf_ps(x + i, y, MAROON);
    for (int i = 0; i < 16; i += 5) pf_ps(x + (i + k) % 16, y0 + 5 + (k & 3), YELLOW);
}

static PF_NI void pf_atlas(void)
{
    gfx_target(BUF3);
    clear(BLUE);
    /* grass on dirt */
    pf_dirt(16, 5, 15, 0);
    pf_fr(16, 0, 16, 4, GREEN);
    for (int i = 0; i < 16; i += 2) { pf_ps(16 + i, 3, DGREEN); pf_ps(17 + i, 4, DGREEN); }
    pf_hl(16, 31, 0, YELLOW);
    pf_dirt(32, 0, 15, 7);
    /* block */
    pf_fr(48, 0, 16, 16, CYAN);
    pf_hl(48, 63, 0, WHITE); pf_vl(48, 0, 15, WHITE);
    pf_hl(48, 63, 15, TEAL); pf_vl(63, 0, 15, TEAL);
    pf_hl(49, 62, 14, TEAL); pf_vl(62, 1, 14, TEAL);
    pf_ps(51, 3, WHITE); pf_ps(60, 3, WHITE); pf_ps(51, 12, GREY); pf_ps(60, 12, GREY);
    /* coin, spinning (4 frames) */
    static const int8_t rx[4] = { 6, 4, 1, 4 };
    for (int f = 0; f < 4; f++) {
        int cx = 72 + 16 * f;
        pf_el(cx, 8, rx[f], 7, YELLOW, 1);
        pf_el(cx, 8, rx[f], 7, OLIVE, 0);
        if (rx[f] > 3) { pf_vl(cx - rx[f] / 2, 5, 10, WHITE); pf_vl(cx + 1, 5, 11, OLIVE); }
    }
    /* cloud, 3 tiles */
    pf_el(139, 9, 10, 6, WHITE, 1);
    pf_el(153, 7, 11, 7, WHITE, 1);
    pf_el(166, 9, 9, 6, WHITE, 1);
    pf_fr(136, 9, 32, 6, WHITE);
    pf_hl(133, 170, 15, GREY);
    /* bush, 2 tiles */
    pf_el(185, 9, 8, 6, GREEN, 1);
    pf_el(198, 8, 9, 7, GREEN, 1);
    pf_fr(180, 9, 26, 6, GREEN);
    pf_hl(177, 206, 14, DGREEN); pf_hl(177, 206, 15, DGREEN);
    pf_ps(184, 7, YELLOW); pf_ps(196, 5, RED); pf_ps(201, 9, YELLOW); pf_ps(190, 10, WHITE);
    /* hill, 4 x 2 tiles (atlas rows 1 and 2) */
    pf_el(32, 48, 30, 30, GREEN, 1);
    pf_el(32, 48, 30, 30, DGREEN, 0);
    /* tree, 2 x 3 tiles (atlas rows 4 to 6) */
    pf_fr(76, 96, 8, 16, RED);
    pf_vl(77, 96, 111, MAROON); pf_vl(81, 96, 111, MAROON);
    pf_el(80, 82, 15, 15, GREEN, 1);
    pf_el(80, 82, 15, 15, DGREEN, 0);
    for (int i = 0; i < 7; i++) pf_hl(69 + (i * 11) % 14, 73 + (i * 11) % 14, 70 + (i * 7) % 22, DGREEN);
    pf_ps(74, 76, YELLOW); pf_ps(86, 80, RED); pf_ps(78, 90, YELLOW); pf_ps(87, 88, RED);
    for (int i = 0; i < 9; i++) {
        int x = 12 + (i * 37) % 40, y = 24 + (i * 23) % 22;
        pf_hl(x, x + 2, y, DGREEN);
    }
}

static PF_NI void pf_cut(void)
{
    static const uint8_t ids[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 17, 18, 19, 32, 33, 34, 35, 68, 69, 84, 85, 100, 101, 0xFF };
    gfx_target(BUF3);
    for (const uint8_t *p = ids; *p != 0xFF; p++) {
        int id = *p, x = (id & 15) * 8, y = (id >> 4) * 16;
        for (int r = 0; r < 16; r++) {
            const uint8_t *s = (const uint8_t *)BUF3 + rowoff[y + r] + x;
            uint8_t *d = pf_tiles + id * 128 + r * 8;
            for (int j = 0; j < 8; j++) d[j] = s[j];
        }
    }
}

/* ---- sprites -------------------------------------------------------------------------- */
#define PF_TOP \
    "....44......" \
    ".....f......" \
    "..77777777.." \
    ".7777777777." \
    ".7000000007." \
    ".7033003307." \
    ".7000000007." \
    "..77777777.." \
    "...ffffff..." \
    "..44444444.." \
    "f4466666644f" \
    "f4466666644f" \
    "f.44444444.f" \
    "..44cccc44.."
static const char pf_top[] = PF_TOP;
static const char pf_legs[4][49] = {
    "..55....55..""..55....55..""..55....55.."".5555..5555.",
    "..55...55..."".55.....55.."".55......55."  "555.....5555",
    "...555555..."  "...55..55...""...55..55...""..555..555..",
    "..55....55..""..55555555..""............""............",
};
static const char pf_slime[] =
    "....555555...."
    "..5555555555.."
    ".555555555555."
    ".557755557755."
    ".550755550755."
    "55555555555555"
    "55555555555555"
    "5dddddddddddd5"
    "dddddddddddddd"
    "..dd......dd..";
static const char pf_slime2[] =
    "....555555...."
    "..5555555555.."
    ".555555555555."
    ".557755557755."
    ".550755550755."
    "55555555555555"
    "55555555555555"
    "5dddddddddddd5"
    "dddddddddddddd"
    "...dd....dd...";
static const char pf_flat[] =
    "..dddddddddd.."
    ".555555555555."
    ".557755557755."
    "55555555555555"
    "dddddddddddddd";

static const char pf_spiky[] =
    "..7..7..7..7.."
    "..7..7..7..7.."
    ".444444444444."
    ".447744447744."
    ".440744440744."
    "44444444444444"
    "44444444444444"
    "4cccccccccccc4"
    "cccccccccccccc"
    "..cc......cc..";
static const char pf_spiky2[] =
    "..7..7..7..7.."
    "..7..7..7..7.."
    ".444444444444."
    ".447744447744."
    ".440744440744."
    "44444444444444"
    "44444444444444"
    "4cccccccccccc4"
    "cccccccccccccc"
    "...cc....cc...";
static const char pf_bird0[] =
    "0..........0"
    ".00......00."
    "..000..000.."
    "....0000...."
    "....0000...."
    "............";
static const char pf_bird1[] =
    "............"
    "....0000...."
    "..00000000.."
    ".00..00..00."
    "0....00....0"
    "............";

static PF_NI void pf_sprites(void)
{
    char *a = (char *)salloc(217);
    for (int f = 0; f < 4; f++) {
        for (int i = 0; i < 168; i++) a[i] = pf_top[i];
        for (int i = 0; i < 48; i++) a[168 + i] = pf_legs[f][i];
        pf_sh[f] = pf_mk(a, 12, 18);
    }
    pf_ss[0] = pf_mk(pf_slime, 14, 10);
    pf_ss[1] = pf_mk(pf_slime2, 14, 10);
    pf_sf = pf_mk(pf_flat, 14, 5);
    pf_sp[0] = pf_mk(pf_spiky, 14, 10);
    pf_sp[1] = pf_mk(pf_spiky2, 14, 10);
    pf_sb[0] = pf_mk(pf_bird0, 12, 6);
    pf_sb[1] = pf_mk(pf_bird1, 12, 6);
}

/* ---- the world: generated a column at a time, a segment at a time ----------------------- *
 * kinds: 0 plain run, 1 slimes, 2 platform, 3 pit, 4 hurdle, 5 dip. An obstacle always
 * has a flat run before and after it. */
static uint16_t pf_gc;                 /* the next column to make */
static uint8_t  pf_gk, pf_gi, pf_glen, pf_nk, pf_gvar;
static uint8_t  pf_clleft, pf_clrow, pf_clcd;

/* slimes */
typedef struct { int16_t x, xa, xb; int8_t dir; uint8_t st, t, hop, ty; } pf_en_t;
static pf_en_t pf_en[PF_NE];
static int16_t pf_bx[2], pf_by[2];

static PF_NI void pf_spawn(int x, int xa, int xb, int dir, int ty)
{
    for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++)
        if (!en->st) {
            en->x = (int16_t)x; en->xa = (int16_t)xa; en->xb = (int16_t)xb;
            en->dir = (int8_t)dir; en->st = 1; en->ty = (uint8_t)ty;
            en->hop = (uint8_t)(ty || rnd_n(4) == 0);
            return;
        }
}

/* coins along a jump's path (the same arithmetic as the hero's flight) */
static PF_NI void pf_arc(int x, int fy, int mincol)
{
    int vy = -PF_JV, yq = fy * 16;
    for (int t = 0; t < 48; t++) {
        x += PF_SPD; yq += vy; vy += PF_G;
        if ((x & 15) == 8) {
            int col = x >> 4, row = ((yq >> 4) - 11) >> 4;
            if (col >= mincol && row >= 2 && row <= 13 && pf_coin[col & 127] == 255)
                pf_coin[col & 127] = (uint8_t)row;
        }
    }
}

static void pf_choose(void)
{
    uint16_t r = rnd_n(10);
#ifdef PF_ALLENEMY
    pf_nk = pf_gk >= 3 ? 1 : 3; return;
#endif
    if (pf_gk >= 3) pf_nk = r < 5 ? 1 : r < 7 ? 2 : 0;
    else if (r >= 9) pf_nk = (uint8_t)rnd_n(3);
    else pf_nk = r < 3 ? 3 : r < 6 ? 4 : r < 8 ? 5 : 3;
}

static PF_NI void pf_newseg(void)
{
    pf_gk = pf_nk; pf_gi = 0;
    uint8_t v = pf_gvar = (uint8_t)rnd16();
    int c = pf_gc, len;
    switch (pf_gk) {
    case 0:  len = 9 + (v & 3); break;
    case 1:  len = 16; break;
    case 2:  len = 14; break;
    case 3:  len = 2 + (v & 1); break;
    case 4:  len = 4 + (v & 1) * 2; break;
    default: len = 5; break;
    }
    pf_glen = (uint8_t)len;
    pf_choose();
    if (pf_gk == 1) {
        int x0 = c * 16;
        pf_spawn(x0 + 80, x0 + 48, x0 + 208, -1, (v >> 2) & 1);
        if (v & 2) pf_spawn(x0 + 160, x0 + 48, x0 + 208, 1, 0);
    }
    if (pf_gk == 2) pf_arc((c + 2 + (v & 1)) * 16 - 40, PF_GY, c);
    if (pf_gk == 5) pf_arc((c + len) * 16 - 16, 17 * 16, c);
    if (pf_gk < 3 && pf_nk >= 3 && pf_nk != 5) pf_arc((c + len) * 16 - 16, PF_GY, c);
}

static PF_NI void pf_gcol(void)
{
    int c = pf_gc, s = c & 127;
    uint8_t *m = pf_map + s * 24;
    if (pf_gi >= pf_glen) pf_newseg();
    int i = pf_gi++, v = pf_gvar, gt = PF_GROUND, pt = 255;
    uint8_t cr = pf_coin[s];
    for (int r = 0; r < 24; r++) m[r] = PF_SKY;
    pf_coin[(c + 40) & 127] = 255;
    switch (pf_gk) {
    case 0:
        if (!(v & 1) || (v & 3) == 3) {
            if (i >= 1 && i <= 4) { m[PF_GROUND - 2] = (uint8_t)(PF_HILLT + i - 1); m[PF_GROUND - 1] = (uint8_t)(PF_HILLB + i - 1); }
        }
        if ((v & 3) == 3 || (v & 3) == 2) {
            if (i == 6 || i == 7) for (int r = 0; r < 3; r++) m[PF_GROUND - 3 + r] = (uint8_t)(PF_TREE + 16 * r + i - 6);
        }
        if ((v & 3) == 1 && (i == 2 || i == 3)) m[PF_GROUND - 1] = (uint8_t)(PF_BUSH + i - 2);
        if (i >= 8 + (v & 1) * 0 && i < pf_glen - 2 && cr == 255) cr = PF_GROUND - 1 - (v & 1) * 2;
        if (i >= 5 && i < 8 && (v & 3) < 2 && cr == 255) cr = PF_GROUND - 1;
        break;
    case 1:
        if (i == 12 || i == 13) m[PF_GROUND - 1] = (uint8_t)(PF_BUSH + i - 12);
        if (v & 8) {
            if (i == 0 || i == 1) for (int r = 0; r < 3; r++) m[PF_GROUND - 3 + r] = (uint8_t)(PF_TREE + 16 * r + i);
        }
        break;
    case 2: {
        int off = 2 + (v & 1), plen = 5 + ((v >> 1) & 1), prow = 11 + ((v >> 2) & 1);
        if (i >= off && i < off + plen) {
            pt = prow; m[prow] = PF_BLOCK;
            if (i > off && i < off + plen - 1 && cr == 255) cr = (uint8_t)(prow - 1);
        }
        break;
    }
    case 3:  gt = 255; break;
    case 4:  gt = (i == 0 || i == pf_glen - 1) ? PF_GROUND - 1 : PF_GROUND - 2; break;
    default: gt = 17; break;
    }
    if (gt != 255) {
        m[gt] = PF_GRASS;
        for (int r = gt + 1; r < 23; r++) m[r] = PF_DIRT;
    }
    /* clouds */
    if (!pf_clleft && --pf_clcd == 0) { pf_clleft = 3; pf_clrow = (uint8_t)(2 + rnd_n(5)); pf_clcd = (uint8_t)(4 + rnd_n(7)); }
    if (pf_clleft) { m[pf_clrow] = (uint8_t)(PF_CLOUD + 3 - pf_clleft); pf_clleft--; }
    if (cr != 255 && m[cr] == PF_SKY) m[cr] = PF_COIN; else cr = 255;
    pf_coin[s] = cr;
    pf_gt[s] = (uint8_t)gt; pf_pt[s] = (uint8_t)pt;
}

static void pf_ensure(uint16_t col) { while (pf_gc <= col) { pf_gcol(); pf_gc++; } }

/* ---- the two bitmaps -------------------------------------------------------------------- */
static const uint32_t pf_bufs[2] = { BUF0, BUF1 };
static uint16_t pf_fill[2];            /* world byte column drawn up to (exclusive) */
static int16_t  pf_bbf[2];             /* its bitmap byte column */
#define PF_NR 12
static int16_t  pf_rw[2][PF_NR], pf_ry0[2][PF_NR], pf_ry1[2][PF_NR];
static uint8_t  pf_rn[2][PF_NR], pf_rc[2];
static uint8_t  pf_bcf[2];
static uint8_t  pf_dn[2], pf_dcol[2][8], pf_drow[2][8];

/* world byte column wb -> its byte column in bitmap k */
static int pf_bb(int k, uint16_t wb)
{
    int bb = pf_bbf[k] - (int)(pf_fill[k] - wb);
    if (bb < 0) bb += 320;
    return bb;
}

/* one byte column (2 pixels) of the world, rows y0..y1, into the bitmap at byte column bb */
static PF_NI void pf_col(uint8_t *buf, uint16_t wb, int bb, int y0, int y1)
{
    const uint8_t *m = pf_map + (uint16_t)((wb >> 3) & 127) * 24;
    uint16_t j = wb & 7;
    int t0 = y0 >> 4, t1 = y1 >> 4;
    for (int t = t0; t <= t1; t++) {
        uint8_t id = m[t];
        if (id == PF_COIN) id = (uint8_t)(PF_COIN + pf_cf);
        const uint8_t *s = pf_tiles + (uint16_t)id * 128 + j;
        int r0 = (t == t0) ? (y0 & 15) : 0, r1 = (t == t1) ? (y1 & 15) : 15;
        s += r0 * 8;
        uint8_t *d = buf + rowoff[t * 16 + r0] + bb;
        for (int r = r0; r <= r1; r++, s += 8, d += STRIDE) *d = *s;
    }
}

static PF_NI void pf_cell(int k, int col, int row)
{
    uint16_t lo = pf_fill[k] - 320;
    for (int j = 0; j < 8; j++) {
        uint16_t wb = (uint16_t)(col * 8 + j);
        if (wb < lo || wb >= pf_fill[k]) continue;
        pf_col((uint8_t *)pf_bufs[k], wb, pf_bb(k, wb), row * 16, row * 16 + 15);
    }
}

static PF_NI void pf_extend(int k, uint16_t target)
{
    pf_ensure((target >> 3) + 1);
    uint8_t *b = (uint8_t *)pf_bufs[k];
    while (pf_fill[k] < target) {
        pf_col(b, pf_fill[k], pf_bbf[k], 0, 359);
        pf_fill[k]++;
        if (++pf_bbf[k] == 320) pf_bbf[k] = 0;
    }
}

/* ---- the game state --------------------------------------------------------------------- */
static int16_t  pf_hx, pf_cam, pf_camm;     /* hero x (world), camera x, camera mod 640 */
static int16_t  pf_fy, pf_vy;               /* feet y, 1/16 pixel; speed */
static uint8_t  pf_gnd, pf_inv;
static uint16_t pf_tick, pf_score, pf_coins, pf_stomps;
static uint8_t  pf_hud;

static void pf_jump(void) { pf_vy = -PF_JV; pf_gnd = 0; }

static PF_NI void pf_respawn(void)
{
    PFD("respawn");
    int c = pf_hx >> 4;
    while (pf_gt[c & 127] == 255 || pf_gt[c & 127] > PF_GROUND) c++;
    pf_hx = (int16_t)(c * 16 + 8);
    pf_fy = -40 * 16; pf_vy = 0; pf_gnd = 0; pf_inv = 90;
}

static PF_NI void pf_collect(int c)
{
    int row = pf_coin[c & 127];
    pf_coin[c & 127] = 255;
    pf_map[(c & 127) * 24 + row] = PF_SKY;
    for (int k = 0; k < 2; k++)
        if (pf_dn[k] < 8) { pf_dcol[k][pf_dn[k]] = (uint8_t)c; pf_drow[k][pf_dn[k]++] = (uint8_t)row; }
    pf_coins++; pf_score += 10; pf_hud = 1;
}

static PF_NI void pf_step(void)
{
    pf_tick++;
    /* slimes */
    for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++) {
        if (en->st == 1) {
            if (pf_tick & 1) {
                en->x += en->dir;
                if (en->x <= en->xa) en->dir = 1;
                if (en->x >= en->xb) en->dir = -1;
            }
        } else if (en->st == 2 && --en->t == 0) en->st = 0;
        if (en->st && en->x < pf_hx - 300) en->st = 0;
    }
    for (int i = 0; i < 2; i++) {
        pf_bx[i] -= 1;
        if (pf_bx[i] < pf_cam - 40) { pf_bx[i] = (int16_t)(pf_cam + 680 + rnd_n(260)); pf_by[i] = (int16_t)(50 + rnd_n(90)); }
    }
    pf_hx += PF_SPD;
    if (pf_inv) pf_inv--;
    int fyp = pf_fy >> 4;
    if (pf_gnd) {
        /* the autopilot */
        int c1 = ((pf_hx + 16) >> 4) & 127, c2 = ((pf_hx + 40) >> 4) & 127, c3 = ((pf_hx + 38) >> 4) & 127;
        int go = 0;
        if (pf_gt[c1] == 255 || pf_gt[c1] * 16 < fyp - 4) go = 1;
        if (pf_pt[c2] != 255 && pf_pt[c3] == 255) {
            int d = fyp - pf_pt[c2] * 16;
            if (d > 8 && d <= 80) go = 1;
        }
        for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++) {
            if (en->st != 1 || fyp != PF_GY) continue;
            int dx = en->x - pf_hx;
            if (dx > 0 && dx <= 30) go = 1;
            if (en->hop) { if (dx > 0 && dx + en->dir * 20 <= 60) go = 1; }
            else { int p = en->x + en->dir * 19 - (pf_hx + 80); if (p >= -3 && p <= 3) go = 1; }
        }
        if (go) { pf_jump(); PFD("jump"); }
    }
    if (pf_gnd) {
        int c = (pf_hx >> 4) & 127, best = -1;
        if (pf_gt[c] != 255) { int t = pf_gt[c] * 16; if (t >= fyp - 18 && t <= fyp + 3) best = t; }
        if (pf_pt[c] != 255) { int t = pf_pt[c] * 16; if (t >= fyp - 2 && t <= fyp + 3) best = t; }
        if (best < 0) { pf_gnd = 0; pf_vy = 0; }
        else pf_fy = (int16_t)(best * 16);
    } else {
        int old = pf_fy >> 4;
        pf_fy += pf_vy; pf_vy += PF_G;
        int nw = pf_fy >> 4;
        if (pf_vy > 0 && nw >= old) {
            int c = (pf_hx >> 4) & 127, best = 9999;
            if (pf_gt[c] != 255) { int t = pf_gt[c] * 16; if (nw >= t && old <= t + 20) best = t; }
            if (pf_pt[c] != 255) { int t = pf_pt[c] * 16; if (nw >= t && old <= t && t < best) best = t; }
            if (best < 9999) { pf_fy = (int16_t)(best * 16); pf_vy = 0; pf_gnd = 1; }
        }
        if ((pf_fy >> 4) > 380) pf_respawn();
    }
    fyp = pf_fy >> 4;
    /* slimes against the hero */
    for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++) {
        if (en->st != 1) continue;
        int dx = pf_hx - en->x;
        if (dx < 0) dx = -dx;
        if (dx < 12 && fyp > PF_GY - 16 && fyp < PF_GY + 6) {
            if (!en->ty && pf_vy > 0 && !pf_gnd && fyp < PF_GY - 3) {
                en->st = 2; en->t = 24; pf_vy = -86; pf_gnd = 0;
                pf_stomps++; pf_score += 100; PFD("stomp"); pf_hud = 1;
            } else if (!pf_inv) {
                PFD("hurt");

                pf_inv = 90; pf_vy = -70; pf_gnd = 0;
            }
        }
    }
    /* coins */
    for (int k = -5; k <= 5; k += 10) {
        int c = (pf_hx + k) >> 4, r = pf_coin[c & 127];
        if (r != 255) {
            int ry = r * 16 + 8, dx = pf_hx - (c * 16 + 8);
            if (dx < 0) dx = -dx;
            if (dx <= 13 && ry > fyp - 24 && ry < fyp + 4) pf_collect(c);
        }
    }
}

/* ---- drawing ---------------------------------------------------------------------------- */
static PF_NI void pf_sprite(int k, const sprite_t *s, int wx, int y)
{
    uint16_t wb = (uint16_t)(wx >> 1);
    if (wb < (uint16_t)(pf_fill[k] - 320) || wb + s->wb > pf_fill[k]) return;
    int bx = pf_bb(k, wb) * 2 + (wx & 1);
    pf_sd(s, bx, y);
    if (bx + s->w > W) pf_sd(s, bx - W, y);
    int i = pf_rc[k];
    if (i < PF_NR) {
        pf_rw[k][i] = (int16_t)wb; pf_rn[k][i] = (uint8_t)s->wb;
        pf_ry0[k][i] = (int16_t)(y < 0 ? 0 : y);
        pf_ry1[k][i] = (int16_t)(y + s->h > H ? H - 1 : y + s->h - 1);
        pf_rc[k] = (uint8_t)(i + 1);
    }
}

static PF_NI void pf_hudshow(void)
{
    tnum(0, 9, pf_coins, 3, ATTR(YELLOW, BLACK));
    tnum(0, 26, pf_score, 5, ATTR(WHITE, BLACK));
    tnum(0, 43, pf_stomps, 2, ATTR(GREEN, BLACK));
}

static PF_NI void pf_render(int k)
{
    uint8_t *b = (uint8_t *)pf_bufs[k];
    gfx_target(pf_bufs[k]);
    /* put back what the sprites covered */
    for (int i = 0; i < pf_rc[k]; i++)
        for (int j = 0; j < pf_rn[k][i]; j++) {
            uint16_t wb = (uint16_t)(pf_rw[k][i] + j);
            pf_col(b, wb, pf_bb(k, wb), pf_ry0[k][i], pf_ry1[k][i]);
        }
    pf_rc[k] = 0;
    /* the columns that came round to the right edge */
    pf_extend(k, (uint16_t)((pf_cam >> 1) + 320));
    /* collected coins, spinning coins */
    for (int i = 0; i < pf_dn[k]; i++) pf_cell(k, pf_dcol[k][i], pf_drow[k][i]);
    pf_dn[k] = 0;
    if (pf_bcf[k] != pf_cf) {
        pf_bcf[k] = pf_cf;
        int c0 = pf_cam >> 4;
        for (int c = c0; c < c0 + 41; c++)
            if (pf_coin[c & 127] != 255) pf_cell(k, c, pf_coin[c & 127]);
    }
    /* sprites */
    for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++) {
        if (en->st == 1) pf_sprite(k, (en->ty ? pf_sp : pf_ss)[(pf_tick >> 4) & 1], en->x - 7, PF_GY - 10);
        else if (en->st == 2) pf_sprite(k, pf_sf, en->x - 7, PF_GY - 5);
    }
    for (int i = 0; i < 2; i++)
        pf_sprite(k, pf_sb[((pf_tick >> 3) + i) & 1], pf_bx[i], pf_by[i] + (SIN(pf_tick * 3 + i * 90) >> 4));
    if (!(pf_inv & 4)) {
        int f = !pf_gnd ? 3 : 1 + ((pf_hx >> 4) & 1);
        int bob = (pf_gnd && ((pf_hx >> 3) & 1)) ? 1 : 0;
        pf_sprite(k, pf_sh[f], pf_hx - 6, (pf_fy >> 4) - 18 + bob);
    }
}

static PF_NI void pf_run(int32_t frames)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    pf_map = (uint8_t *)salloc(128 * 24);
    pf_tiles = (uint8_t *)salloc(PF_NTILES * 128);
    pf_atlas();
    pf_cut();
    pf_sprites();
    for (int i = 0; i < 128; i++) { pf_coin[i] = 255; pf_gt[i] = PF_GROUND; pf_pt[i] = 255; }
    for (pf_en_t *en = pf_en; en < pf_en + PF_NE; en++) en->st = 0;
    for (int i = 0; i < 2; i++) { pf_bx[i] = (int16_t)(300 + 260 * i); pf_by[i] = (int16_t)(70 + 40 * i); }
#ifdef PF_SEED
    rnd_state = PF_SEED;
#endif
    pf_gc = 0; pf_gk = 3; pf_nk = 0; pf_gi = pf_glen = 0;
    pf_clleft = 0; pf_clcd = 3;
    pf_cf = 0; pf_tick = 0;
    pf_hx = 48; pf_cam = pf_camm = 0;
    pf_fy = PF_GY * 16; pf_vy = 0; pf_gnd = 1; pf_inv = 0;
    pf_score = pf_coins = pf_stomps = 0;
    for (int k = 0; k < 2; k++) {
        pf_fill[k] = 0; pf_bbf[k] = 0; pf_rc[k] = 0; pf_dn[k] = 0; pf_bcf[k] = 0;
    }
    tput(0, 2, "COINS", ATTR(CYAN, BLACK));
    tput(0, 19, "SCORE", ATTR(CYAN, BLACK));
    tput(0, 36, "STOMPS", ATTR(CYAN, BLACK));
    tput(0, 62, "BIT'S BIG RUN", ATTR(YELLOW, BLACK));
    pf_hudshow();
    /* both bitmaps start with the first screenful */
    for (int k = 0; k < 2; k++) { gfx_target(pf_bufs[k]); pf_extend(k, 320); }
    int k = 0;
    gfx_show(BUF0);
    sec_begin("platform", frames);
    uint16_t n = 1;
    while (sec_running()) {
        if (n > 4) n = 4;
        for (uint16_t i = 0; i < n; i++) pf_step();
        pf_cf = (uint8_t)((pf_tick >> 3) & 3);
        int tgt = pf_hx - 200, dc = (tgt - pf_cam) & ~1;
        if (dc > 12) dc = 12;
        if (dc > 0) {
            pf_cam += dc;
            pf_camm += dc;
            if (pf_camm >= W) pf_camm -= W;
        }
        pf_render(k);
        if (pf_hud) { pf_hud = 0; pf_hudshow(); }
        n = frame_wait();
        gfx_show(pf_bufs[k]);
        SCROLLX = (uint16_t)pf_camm;
        k ^= 1;
#ifdef PF_DEBUG
        if ((pf_tick & 63) == 0) printf("t%u hx%d cam%d camm%d fy%d gnd%d\n", pf_tick, pf_hx, pf_cam, pf_camm, pf_fy>>4, pf_gnd);
#endif
    }
#ifndef PF_KEEP
    SCROLLX = 0;
#endif
    SCROLLY = 0;
}
