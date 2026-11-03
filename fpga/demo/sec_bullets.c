/* Section 2: "Night Bloom", a bullet-hell shooter that plays itself.
 *
 * A mothership with two turrets fires four patterns in turn (a spiral, rings,
 * a sweeping curtain, a flower); a small ship at the bottom shoots back and
 * dodges by looking ahead: every bullet above it is extrapolated to the
 * moment it reaches the ship's row, the arrivals are summed in 8-pixel bins
 * and the ship steers to the safest bin near it. When the boss's health bar
 * is gone it blows up and the next one arrives.
 *
 * Drawing: BUF2 holds the nebula and the stars, BUF0/BUF1 are shown in turn;
 * each frame the little things drawn into a buffer two frames ago (bullets,
 * particles, background stars: a 4x4 footprint) and the bigger sprites (a
 * rectangle each) are copied back from BUF2, then everything is drawn again.
 * Positions are 12.4 fixed point, 16 bits (640 * 16 fits). */

#define BH_NI __attribute__((noinline))
#define BH_COLD __attribute__((noinline, minsize))
#define BH_MAXB   200                 /* bullets */
#define BH_MAXP   40                  /* particles */
#define BH_NSTAR  20
#define BH_MAXS   18                  /* player shots */
#define BH_MAXSM  (BH_MAXB + BH_MAXP + BH_NSTAR + BH_MAXS)
#define BH_MAXGR  8
#define BH_X0     128                 /* the playfield: x 128 .. 511 */
#define BH_X1     512

typedef struct { int16_t x, y; int8_t vx, vy; uint8_t k, g; } bh_b;   /* g: grazed / life */
typedef struct { int16_t x, y; } bh_xy;
typedef struct { int16_t x, y, w, h; } bh_rc;

static bh_b   *bh_bl;                 static int bh_nb;
static bh_b   *bh_pt;                 static int bh_np;
static bh_xy  *bh_sm[2];              static int bh_nsm[2];
static bh_rc  *bh_gr[2];              static int bh_ngr[2];
static uint16_t *bh_dg;               /* danger per 8-pixel bin (+1 pad) */
static sprite_t *bh_orb[7], *bh_sship, *bh_sfl[2], *bh_sshot, *bh_sboss, *bh_sturr;
static uint8_t  bh_rcp[128];

static int16_t  bh_stx[BH_NSTAR], bh_sty[BH_NSTAR];
static int16_t  bh_shx[BH_MAXS], bh_shy[BH_MAXS];
static int      bh_nsh;
static uint16_t bh_em[4];

static int      bh_bx, bh_by;         /* boss */
static int      bh_plx, bh_ply, bh_tgt;
static int      bh_state, bh_phase, bh_stt, bh_hp, bh_hpmax, bh_flash, bh_hpdrawn;
static uint16_t bh_gt, bh_ptm, bh_firet;
static int      bh_inv, bh_shield, bh_hits;
static uint16_t bh_score, bh_graze;
static uint8_t  bh_base, bh_base2;
static int      bh_dir;
#ifdef BH_DEBUG
static int      bh_maxnb, bh_ndeaths;
#endif

static const char *const bh_names[4] = { "WHIRLPOOL", "HALO ENGINE", "TIDE CURTAIN", "NIGHT BLOOM" };

/* ---- art ---------------------------------------------------------------------------- */
static const char bh_shiphalf[16][9] = {
    ".......7", "......77", "......37", ".....337", ".....367", "1....367", "1...3337", "11..3337",
    "711.3337", "71133337", "71133337", "74433337", "44.33337", "4..43377", "...44.77", "....4.67",
};
static const char *const bh_bosshalf[18] = {
    "7777777", "7733333333", "737333333333", "73733333333333", "733333333333333", "7333333333333333",
    "73333333333333333", "777777777777777777", "77555555555555555555", "75555555555555555555",
    "74444444444444466666", "76363636363636667777", "74444444444444466666", "75555555555555555555",
    "77555555555555555555", "77777555555555555555", "777777777777777", "7777777777",
};
static const char bh_turret[] = "..7777.." ".774477." "77466477" "74666647" "74666647" "77466477" ".774477." "..7777..";
static const char bh_flame0[] = ".66." ".64." ".44." "..4.";
static const char bh_flame1[] = ".66." ".66." ".64." ".44.";
static const char bh_shotart[] = ".77." ".37." ".37." ".33.";

static BH_NI void bh_tp(int row, int col, const char *t, uint16_t a) { tput(row, col, t, a); }
static BH_NI void bh_tn(int row, int col, uint16_t v, int d, uint16_t a) { tnum(row, col, v, d, a); }
static BH_NI void bh_tf(int row, int col, int n, uint8_t ch, uint16_t a) { tfill(row, col, n, ch, a); }
static BH_NI void bh_ps(int x, int y, uint8_t c) { pset(x, y, c); }
static BH_NI void bh_fr(int x, int y, int w, int h, uint8_t c) { fill_rect(x, y, w, h, c); }
static BH_NI void bh_hl(int x0, int x1, int y, uint8_t c) { hline(x0, x1, y, c); }
static BH_NI void bh_vl(int x, int y0, int y1, uint8_t c) { vline(x, y0, y1, c); }
static BH_NI void bh_el(int x, int y, int rx, int ry, uint8_t c, int f) { ellipse(x, y, rx, ry, c, f); }
static BH_NI void bh_ci(int x, int y, int r, uint8_t c, int f) { ellipse(x, y, r, r, c, f); }
static BH_NI sprite_t *bh_mk(const char *art, int w, int h) { return spr_make(art, w, h, 0); }

static BH_COLD void bh_make_art(void)
{
    char a[16 * 16], o[16];
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 8; x++) {
            a[y * 16 + x] = bh_shiphalf[y][x];
            a[y * 16 + 15 - x] = bh_shiphalf[y][x];
        }
    bh_sship = bh_mk(a, 16, 16);
    bh_sfl[0] = bh_mk(bh_flame0, 4, 4);
    bh_sfl[1] = bh_mk(bh_flame1, 4, 4);
    bh_sshot = bh_mk(bh_shotart, 4, 4);
    /* orbs: outer colour, core colour */
    static const uint8_t oc[7][2] = { { RED, YELLOW }, { MAGENTA, WHITE }, { CYAN, WHITE }, { YELLOW, WHITE },
                                      { GREEN, WHITE }, { WHITE, WHITE }, { YELLOW, YELLOW } };
    for (int k = 0; k < 7; k++) {
        char O = (char)('0' + oc[k][0]), C = (char)('0' + oc[k][1]);
        const char t[16] = { '.', O, O, '.', O, C, C, O, O, C, C, O, '.', O, O, '.' };
        for (int i = 0; i < 16; i++) o[i] = t[i];
        bh_orb[k] = bh_mk(o, 4, 4);
    }
    /* the mothership (40x18, the left half is drawn here, right-aligned) and a turret (8x8) */
    {
        char *art = (char *)salloc(40 * 18);
        for (int y = 0; y < 18; y++) {
            const char *r = bh_bosshalf[y];
            int n = 0;
            while (r[n]) n++;
            for (int x = 0; x < 20; x++) {
                char c = x >= 20 - n ? r[x - (20 - n)] : '.';
                art[y * 40 + x] = c;
                art[y * 40 + 39 - x] = c;
            }
        }
        bh_sboss = bh_mk(art, 40, 18);
        bh_sturr = bh_mk(bh_turret, 8, 8);
    }
}

/* ---- background: nebula and stars in BUF2 -------------------------------------------- */
static BH_COLD void bh_background(void)
{
    gfx_target(BUF2);
    clear(BLACK);
    for (int i = 0; i < 6; i++) bh_el((int)rnd_n(640), (int)rnd_n(300), 70 + (int)rnd_n(110), 20 + (int)rnd_n(50), NAVY, 1);
    for (int i = 0; i < 3; i++) bh_el((int)rnd_n(640), (int)rnd_n(300), 30 + (int)rnd_n(50), 10 + (int)rnd_n(22), PURPLE, 1);
    for (int i = 0; i < 12000; i++) {                      /* dust: knock holes in the clouds */
        int x = (int)rnd_n(640), y = (int)rnd_n(360);
        if (pget(x, y)) bh_ps(x, y, BLACK);
    }
    static const uint8_t sc[6] = { GREY, GREY, WHITE, CYAN, YELLOW, BLUE };
    for (int i = 0; i < 100; i++) bh_ps(BH_X0 + (int)rnd_n(384), (int)rnd_n(360), sc[rnd_n(6)]);
    /* the bezel: two side panels with a radar each */
    bh_fr(0, 0, BH_X0, 360, BLACK);
    bh_fr(BH_X1, 0, 640 - BH_X1, 360, BLACK);
    for (int y = 0; y < 360; y += 6) { bh_hl(0, BH_X0 - 5, y, NAVY); bh_hl(BH_X1 + 4, 639, y, NAVY); }
    for (int k = 0; k < 2; k++) {
        int cx = k ? 576 : 64;
        for (int r = 20; r <= 60; r += 20) bh_ci(cx, 275, r, BLUE, 0);
        bh_hl(cx - 62, cx + 62, 275, BLUE);
        bh_vl(cx, 213, 337, BLUE);
        bh_ci(cx + (k ? -17 : 23), 255 + k * 30, 2, GREEN, 1);
    }
    bh_vl(BH_X0 - 1, 0, 359, CYAN); bh_vl(BH_X0 - 2, 0, 359, BLUE);
    bh_vl(BH_X1, 0, 359, CYAN);     bh_vl(BH_X1 + 1, 0, 359, BLUE);
}

/* ---- bullets ---------------------------------------------------------------------------- */
static BH_NI void bh_spawn(int px, int py, uint8_t ang, int sp, uint8_t kind)
{
    if (bh_nb >= BH_MAXB) return;
    bh_b *b = &bh_bl[bh_nb++];
    b->x = (int16_t)(px << 4); b->y = (int16_t)(py << 4);
    b->vx = (int8_t)((COS(ang) * sp) >> 7);
    b->vy = (int8_t)((SIN(ang) * sp) >> 7);
    b->k = kind; b->g = 0;
}

static BH_COLD void bh_ring(int px, int py, int cnt, uint8_t off, int sp, uint8_t kind)
{
    int step = 4096 / cnt;
    for (int k = 0; k < cnt; k++) bh_spawn(px, py, (uint8_t)(off + ((k * step) >> 4)), sp, kind);
}

static BH_COLD void bh_burst(int x, int y, int cnt, int sp)
{
    while (cnt-- > 0 && bh_np < BH_MAXP) {
        bh_b *p = &bh_pt[bh_np++];
        uint8_t a = (uint8_t)rnd16();
        int s = 6 + (int)rnd_n((uint16_t)sp), r = (int)rnd_n(3);
        p->x = (int16_t)(x << 4); p->y = (int16_t)(y << 4);
        p->vx = (int8_t)((COS(a) * s) >> 7); p->vy = (int8_t)((SIN(a) * s) >> 7);
        p->k = r == 0 ? 5 : (r == 1 ? 6 : 0);
        p->g = (uint8_t)(12 + rnd_n(24));
    }
}

/* angle (256 to the turn, 64 = down) from a vector */
static BH_COLD uint8_t bh_aim(int dx, int dy)
{
    int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, a, t;
    if (!ax && !ay) return 64;
    if (ax >= ay) { t = (ay << 8) / ax; a = (32 * t + ((11 * t * (256 - t)) >> 8)) >> 8; }
    else          { t = (ax << 8) / ay; a = 64 - ((32 * t + ((11 * t * (256 - t)) >> 8)) >> 8); }
    if (dx < 0) a = 128 - a;
    if (dy < 0) a = -a;
    return (uint8_t)a;
}

static BH_NI int bh_due(int i, uint16_t per)
{
    if (bh_ptm < bh_em[i]) return 0;
    bh_em[i] = (uint16_t)(bh_em[i] + per);
    if (bh_em[i] <= bh_ptm) bh_em[i] = (uint16_t)(bh_ptm + per);
    return 1;
}

static BH_COLD void bh_fan(int x, int y, int n, int gap, int sp, uint8_t kind)
{
    uint8_t a = bh_aim(bh_plx - x, bh_ply - y);
    for (int k = 0; k < n; k++) bh_spawn(x, y, (uint8_t)(a + (k - (n >> 1)) * gap), sp, kind);
}

static BH_COLD void bh_fire(void)
{
    int x = bh_bx, y = bh_by + 6;
    switch (bh_phase & 3) {
    case 0:
        if (bh_due(0, 3)) {
            bh_base = (uint8_t)(bh_base + 9);
            for (int k = 0; k < 4; k++) bh_spawn(x, y, (uint8_t)(bh_base + k * 64), 32, (uint8_t)(k & 1 ? 3 : 0));
        }
        if (bh_due(1, 6)) {
            bh_base2 = (uint8_t)(bh_base2 - 13);
            bh_spawn(x, y, bh_base2, 22, 2);
            bh_spawn(x, y, (uint8_t)(bh_base2 + 128), 22, 2);
        }
        break;
    case 1:
        if (bh_due(0, 40)) { bh_ring(x, y, 30, bh_base, 24, 1); bh_base = (uint8_t)(bh_base + 7); }
        if (bh_due(1, 40)) bh_ring(x, y, 30, (uint8_t)(bh_base + 4), 17, 3);
        if (bh_due(2, 28)) bh_fan(x, y, 5, 7, 52, 0);
        break;
    case 2:
        if (bh_due(0, 4)) {
            uint8_t a = (uint8_t)(64 + (SIN(bh_ptm * 3) >> 2));
            for (int j = -1; j <= 1; j++) bh_spawn(x, y, (uint8_t)(a + j * 9), 30, (uint8_t)(j + 1));
        }
        if (bh_due(1, 3)) bh_spawn(x, y, (uint8_t)(64 - (SIN(bh_ptm * 3) >> 2)), 24, 2);
        if (bh_due(2, 60)) bh_ring(x, y, 24, 0, 24, 4);
        break;
    default:
        if (bh_due(0, 5)) {
            bh_base = (uint8_t)(bh_base + bh_dir * 4);
            for (int k = 0; k < 8; k++) bh_spawn(x, y, (uint8_t)(bh_base + k * 32), 18 + 10 * (k & 1), (uint8_t)(k & 1 ? 1 : 0));
        }
        if (bh_due(1, 120)) bh_dir = -bh_dir;
        if (bh_due(2, 56)) bh_fan(x, y, 7, 6, 54, 3);
        break;
    }
    /* the turrets, now and then */
    if (bh_due(3, 40)) {
        bh_fan(bh_bx - 42, bh_by + 2, 3, 6, 44, 3);
        bh_fan(bh_bx + 42, bh_by + 2, 3, 6, 44, 3);
    }
}

/* a line centred in the playfield's 48 text columns, or cleared */
static BH_COLD void bh_tc(int row, const char *t)
{
    int n = 0;
    bh_tf(row, 16, 48, ' ', A_CLEAR);
    while (t[n]) n++;
    bh_tp(row, 16 + (48 - n) / 2, t, ATTR(YELLOW, BLACK));
}

static BH_COLD void bh_set_phase(int ph)
{
    bh_phase = ph;
    bh_state = 0; bh_stt = 0; bh_ptm = 0;
    bh_hpmax = 60 + 6 * (ph > 5 ? 5 : ph); bh_hp = bh_hpmax;
    bh_em[0] = 0; bh_em[1] = 20; bh_em[2] = 40; bh_em[3] = 70;
    bh_dir = 1;
    bh_by = -30;
    bh_tf(8, 65, 14, ' ', A_CLEAR);
    bh_tp(8, 66, bh_names[ph & 3], A_CLEAR | YELLOW);
    bh_tn(5, 66, (uint16_t)(ph + 1), 2, A_CLEAR | WHITE);
    bh_tc(6, bh_names[ph & 3]);
    bh_hpdrawn = -1;
}

/* ---- the game step ---------------------------------------------------------------------- */
static BH_COLD void bh_player_hit(void)
{
    bh_hits++;
    bh_inv = 110;
    bh_burst(bh_plx, bh_ply, 14, 30);
    if (--bh_shield < 0) bh_shield = 3;                   /* a continue */
}

static BH_COLD void bh_boss_die(void)
{
    bh_state = 2; bh_stt = 0;
    bh_nb = 0;                                            /* the bullets are cancelled */
    bh_burst(bh_bx, bh_by, BH_MAXP, 50);
    bh_score = (uint16_t)(bh_score + 100);
    bh_tc(6, " BOSS DOWN ");
#ifdef BH_DEBUG
    bh_ndeaths++;
#endif
}

static BH_NI void bh_step(int n)
{
    bh_gt = (uint16_t)(bh_gt + n);
    /* ---- boss */
    if (bh_state == 0) {
        bh_stt += n;
        bh_by = -30 + ((bh_stt * 3) >> 1);
        bh_bx += (320 - bh_bx) >> 4;
        if (bh_stt >= 64) {
            bh_state = 1; bh_ptm = 0;
            bh_tc(5, "");
            bh_tc(6, "");
        }
    } else if (bh_state == 1) {
        int tx;
        bh_ptm = (uint16_t)(bh_ptm + n);
        switch (bh_phase & 3) {
        case 0:  tx = 320 + SIN(bh_ptm >> 1); break;
        case 1:  tx = 320 + SIN(bh_ptm >> 2); break;
        case 2:  tx = 320 + ((SIN(bh_ptm) * 9) >> 3); break;
        default: tx = 320 + (SIN(bh_ptm >> 1) >> 1); break;
        }
        for (int i = 0; i < n; i++) bh_bx += (tx - bh_bx) >> 3;
        bh_by = 62 + (SIN(bh_gt * 3) >> 4);
        bh_fire();
    } else {
        bh_stt += n;
        if ((bh_stt & 7) < n) bh_burst(bh_bx + (int)rnd_n(40) - 20, bh_by + (int)rnd_n(24) - 12, 10, 36);
        if (bh_stt >= 80) {
            bh_set_phase(bh_phase + 1);
        }
    }
    if (bh_flash > 0) bh_flash -= n;
    if (bh_inv > 0) bh_inv -= n;

    /* ---- player: shots */
    if (bh_state != 2) {
        if (bh_firet <= (uint16_t)n) {
            bh_firet = 4;
            if (bh_nsh <= BH_MAXS - 3) {
                bh_shx[bh_nsh] = (int16_t)bh_plx; bh_shy[bh_nsh++] = (int16_t)(bh_ply - 14);
                bh_shx[bh_nsh] = (int16_t)(bh_plx - 5); bh_shy[bh_nsh++] = (int16_t)(bh_ply - 10);
                bh_shx[bh_nsh] = (int16_t)(bh_plx + 5); bh_shy[bh_nsh++] = (int16_t)(bh_ply - 10);
            }
        } else bh_firet = (uint16_t)(bh_firet - n);
    }
    for (int i = 0; i < bh_nsh; ) {
        int y = bh_shy[i] - 9 * n;
        if (y < -4) { bh_nsh--; bh_shx[i] = bh_shx[bh_nsh]; bh_shy[i] = bh_shy[bh_nsh]; continue; }
        bh_shy[i] = (int16_t)y;
        if (bh_state == 1) {
            int dx = bh_shx[i] - bh_bx, dy = y - bh_by;
            if (dx >= -22 && dx <= 22 && dy >= -12 && dy <= 10) {
                bh_nsh--; bh_shx[i] = bh_shx[bh_nsh]; bh_shy[i] = bh_shy[bh_nsh];
                bh_flash = 3;
                bh_score++;
                if (--bh_hp <= 0) { bh_boss_die(); break; }
                continue;
            }
        }
        i++;
    }

    /* ---- bullets: move, collide, and note where they will cross the ship's row */
    {
        uint16_t *dg = bh_dg;
        int plx = bh_plx, ply = bh_ply, hit = 0;
        for (int i = 0; i < 82; i++) dg[i] = 0;
        for (int i = 0; i < bh_nb; ) {
            bh_b *b = &bh_bl[i];
            int x = b->x + b->vx * n, y = b->y + b->vy * n;
            int px = x >> 4, py = y >> 4;
            if (px < BH_X0 + 2 || px > BH_X1 - 2 || py < 2 || py > 358) { *b = bh_bl[--bh_nb]; continue; }
            b->x = (int16_t)x; b->y = (int16_t)y;
            int dy = ply - py;
            if ((unsigned)(dy + 14) < 140) {
                if (dy >= -12 && dy <= 12) {
                    int dx = px - plx;
                    if ((unsigned)(dx + 12) <= 24) {
                        if (dx >= -3 && dx <= 3 && dy >= -3 && dy <= 3) {
                            if (!bh_inv) { hit = 1; *b = bh_bl[--bh_nb]; continue; }
                        } else if (!b->g) {
                            b->g = 1; bh_graze++; bh_score++;
                        }
                    }
                    if ((unsigned)(dx + 40) <= 80) {
                        int lo = (px - 6) >> 3, hi = (px + 6) >> 3;
                        if (lo < 0) lo = 0;
                        if (hi > 79) hi = 79;
                        for (int k = lo; k <= hi; k++) dg[k + 1] += 8;
                    }
                } else if (dy > 12 && b->vy > 0) {
                    int tt = (int)((uint16_t)(dy * bh_rcp[b->vy]) >> 7);
                    int ax = (x + b->vx * tt) >> 4;
                    int lo = (ax - 6) >> 3, hi = (ax + 6) >> 3;
                    int w = tt < 10 ? 8 : (tt < 28 ? 4 : (tt < 60 ? 2 : 1));
                    if (lo < 0) lo = 0;
                    if (hi > 79) hi = 79;
                    for (int k = lo; k <= hi; k++) dg[k + 1] += (uint16_t)w;
                }
            }
            i++;
        }
        if (hit) bh_player_hit();
    }

    /* ---- the autopilot */
    {
        int cur = bh_plx >> 3, best = cur, bc = 32000;
        int lo = cur - 10, hi = cur + 10, tb = bh_bx >> 3;
        if (lo < 17) lo = 17;
        if (hi > 62) hi = 62;
        for (int b = lo; b <= hi; b++) {
            int c = ((int)bh_dg[b] + 2 * (int)bh_dg[b + 1] + (int)bh_dg[b + 2]) * 3 + ((b > cur ? b - cur : cur - b) >> 1)
                  + (b > tb ? b - tb : tb - b) * 2;
            if (c < bc) { bc = c; best = b; }
        }
        bh_tgt = best * 8 + 4;
        int d = bh_tgt - bh_plx, sp = 5 * n;
        if (d > sp) d = sp; else if (d < -sp) d = -sp;
        bh_plx += d;
        int ty = 312 + (SIN(bh_gt) >> 3);
        int dyy = ty - bh_ply;
        if (dyy > n) dyy = n; else if (dyy < -n) dyy = -n;
        bh_ply += dyy;
    }

    /* ---- particles, stars */
    for (int i = 0; i < bh_np; ) {
        bh_b *p = &bh_pt[i];
        int x = p->x + p->vx * n, y = p->y + p->vy * n;
        int px = x >> 4, py = y >> 4;
        if (p->g <= n || px < BH_X0 + 2 || px > BH_X1 - 2 || py < 2 || py > 358) { *p = bh_pt[--bh_np]; continue; }
        p->x = (int16_t)x; p->y = (int16_t)y; p->g = (uint8_t)(p->g - n);
        i++;
    }
    for (int i = 0; i < BH_NSTAR; i++) {
        int y = bh_sty[i] + (1 + (i % 3)) * n;
        if (y > 356) { y = 0; bh_stx[i] = (int16_t)(BH_X0 + rnd_n(380)); }
        bh_sty[i] = (int16_t)y;
    }
}

/* ---- drawing ---------------------------------------------------------------------------- */
static void bh_draw4(const sprite_t *s, int x, int y)
{
    const uint8_t *q = s->v[x & 1];
    uint8_t *p = g_buf + rowoff[y] + (x >> 1);
    if (x & 1) {
        for (int r = 0; r < 4; r++, q += 6, p += STRIDE) {
            p[0] = (uint8_t)((p[0] & q[1]) | q[0]);
            p[1] = (uint8_t)((p[1] & q[3]) | q[2]);
            p[2] = (uint8_t)((p[2] & q[5]) | q[4]);
        }
    } else {
        for (int r = 0; r < 4; r++, q += 6, p += STRIDE) {
            p[0] = (uint8_t)((p[0] & q[1]) | q[0]);
            p[1] = (uint8_t)((p[1] & q[3]) | q[2]);
        }
    }
}

static BH_NI void bh_restore_small(uint32_t buf, const bh_xy *l, int n)
{
    for (int i = 0; i < n; i++) {
        uint32_t o = rowoff[l[i].y] + (uint32_t)(l[i].x >> 1);
        uint8_t *d = (uint8_t *)(buf + o);
        const uint8_t *s = (const uint8_t *)(BUF2 + o);
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        d[320] = s[320]; d[321] = s[321]; d[322] = s[322];
        d[640] = s[640]; d[641] = s[641]; d[642] = s[642];
        d[960] = s[960]; d[961] = s[961]; d[962] = s[962];
    }
}

static BH_NI void bh_rect(int c, int x, int y, int w, int h)
{
    bh_rc *r = &bh_gr[c][bh_ngr[c]++];
    r->x = (int16_t)x; r->y = (int16_t)y; r->w = (int16_t)w; r->h = (int16_t)h;
}

#ifdef BH_DEBUG
static uint32_t bh_pa[9]; static uint32_t bh_pl; static uint32_t bh_fmax, bh_fsum, bh_ft, bh_fover;
#define PR(i) { uint32_t t_ = instret(); if (i) { bh_pa[i-1] += (t_ - bh_pl); bh_ft += (t_ - bh_pl); } bh_pl = t_; }
#else
#define PR(i)
#endif
static BH_NI void bh_render(int c)
{
    uint32_t buf = c ? BUF1 : BUF0;
    bh_xy *sm = bh_sm[c];
    int nsm = 0;
    PR(0)
    bh_restore_small(buf, sm, bh_nsm[c]);
    PR(1)
    for (int i = 0; i < bh_ngr[c]; i++) {
        bh_rc *r = &bh_gr[c][i];
        copy_rect(buf, BUF2, r->x, r->y, r->w, r->h);
    }
    bh_ngr[c] = 0;
    PR(2)
    /* background stars */
    for (int i = 0; i < BH_NSTAR; i++) {
        int x = bh_stx[i], y = bh_sty[i], sp = i % 3;
        uint8_t col = sp == 0 ? BLUE : (sp == 1 ? CYAN : WHITE);
        pset(x, y, col);
        if (sp) pset(x, y + 1, col);
        sm[nsm].x = (int16_t)x; sm[nsm++].y = (int16_t)y;
    }
    PR(3)
    /* boss and turrets */
    {
        int shake = bh_state == 2 ? (int)(bh_gt & 3) - 1 : 0;
        int x = bh_bx + shake, y = bh_by;
        int white = bh_flash > 0 || (bh_state == 2 && (bh_gt & 2));
        sprite_t *sp[3] = { bh_sturr, bh_sturr, bh_sboss };
        int sx[3] = { x - 46, x + 38, x - 20 }, sy[3] = { y - 2, y - 2, y - 9 };
        int sw[3] = { 8, 8, 40 }, sh[3] = { 8, 8, 18 };
        for (int k = 0; k < 3; k++) {
            if (white && k == 2) spr_draw_solid(sp[k], sx[k], sy[k], WHITE);
            else spr_draw(sp[k], sx[k], sy[k]);
            bh_rect(c, sx[k], sy[k], sw[k], sh[k]);
        }
    }
    PR(4)
    /* shots */
    for (int i = 0; i < bh_nsh; i++) {
        int x = bh_shx[i] - 2, y = bh_shy[i] - 2;
        if (y < 0) continue;
        bh_draw4(bh_sshot, x, y);
        sm[nsm].x = (int16_t)x; sm[nsm++].y = (int16_t)y;
    }
    PR(5)
    /* the ship */
    if (!(bh_inv & 4) || bh_state == 2) {
        int x = bh_plx - 8, y = bh_ply - 8, f = (int)(bh_gt >> 1) & 1;
        if (bh_state != 2) {
            spr_draw(bh_sfl[f], x + 3, y + 15);
            spr_draw(bh_sfl[f], x + 10, y + 15);
        }
        spr_draw(bh_sship, x, y);
    }
    bh_rect(c, bh_plx - 8, bh_ply - 8, 16, 24);
    PR(6)
    /* bullets, then the particles */
    for (int i = 0; i < bh_nb; i++) {
        const bh_b *b = &bh_bl[i];
        int x = (b->x >> 4) - 2, y = (b->y >> 4) - 2;
        bh_draw4(bh_orb[b->k], x, y);
        sm[nsm].x = (int16_t)x; sm[nsm++].y = (int16_t)y;
    }
    for (int i = 0; i < bh_np; i++) {
        const bh_b *b = &bh_pt[i];
        int x = (b->x >> 4) - 2, y = (b->y >> 4) - 2;
        bh_draw4(bh_orb[b->k], x, y);
        sm[nsm].x = (int16_t)x; sm[nsm++].y = (int16_t)y;
    }
    bh_nsm[c] = nsm;
    PR(7)
#ifdef BH_DEBUG
    if (bh_nb > bh_maxnb) bh_maxnb = bh_nb;
#endif
}

/* ---- the text layer ---------------------------------------------------------------------- */
static BH_COLD void bh_hud_static(void)
{
    bh_tp(1, 1, "SCORE", A_CLEAR | CYAN);
    bh_tp(2, 8, "0", A_CLEAR | WHITE);
    bh_tp(4, 1, "GRAZE", A_CLEAR | CYAN);
    bh_tp(7, 1, "SHIELD", A_CLEAR | CYAN);
    bh_tp(10, 1, "HITS", A_CLEAR | CYAN);
    bh_tp(1, 66, "BULLETS", A_CLEAR | CYAN);
    bh_tp(4, 66, "PHASE", A_CLEAR | CYAN);
    bh_tp(7, 66, "BOSS", A_CLEAR | RED);
    bh_tp(11, 66, "SHOTS", A_CLEAR | CYAN);
}

static BH_COLD void bh_hud(void)
{
    bh_tn(2, 3, bh_score, 5, A_CLEAR | WHITE);
    bh_tn(5, 3, bh_graze, 4, A_CLEAR | WHITE);
    for (int i = 0; i < 4; i++)
        bh_tp(8, 1 + i, i < bh_shield ? "#" : ".", A_CLEAR | (i < bh_shield ? GREEN : RED));
    bh_tn(11, 1, (uint16_t)bh_hits, 3, A_CLEAR | WHITE);
    bh_tn(2, 66, (uint16_t)bh_nb, 3, A_CLEAR | WHITE);
    bh_tn(12, 66, (uint16_t)bh_nsh, 2, A_CLEAR | WHITE);
    int cells = bh_state == 2 ? 0 : (bh_hp * 48) / bh_hpmax;
    if (cells != bh_hpdrawn) {
        bh_tf(0, 16, cells, 0xDB, ATTR(RED, BLACK));
        bh_tf(0, 16 + cells, 48 - cells, 0xB0, ATTR(BLUE, BLACK));
        bh_hpdrawn = cells;
    }
}

/* ---- the section --------------------------------------------------------------------------- */
static BH_COLD void bh_init(void)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    bh_make_art();
    bh_bl = (bh_b *)salloc(BH_MAXB * sizeof(bh_b));
    bh_pt = (bh_b *)salloc(BH_MAXP * sizeof(bh_b));
    for (int c = 0; c < 2; c++) {
        bh_sm[c] = (bh_xy *)salloc(BH_MAXSM * sizeof(bh_xy));
        bh_gr[c] = (bh_rc *)salloc(BH_MAXGR * sizeof(bh_rc));
        bh_nsm[c] = bh_ngr[c] = 0;
    }
    bh_dg = (uint16_t *)salloc(84 * sizeof(uint16_t));
    for (int v = 1; v < 128; v++) { int r = 2048 / v; bh_rcp[v] = (uint8_t)(r > 255 ? 255 : r); }
    bh_rcp[0] = 255;
    bh_background();
    copy_buf(BUF0, BUF2);
    copy_buf(BUF1, BUF2);
    for (int i = 0; i < BH_NSTAR; i++) { bh_stx[i] = (int16_t)(BH_X0 + rnd_n(380)); bh_sty[i] = (int16_t)rnd_n(356); }
    bh_nb = bh_np = bh_nsh = 0;
    bh_bx = 320; bh_plx = 320; bh_ply = 312;
    bh_gt = 0; bh_firet = 0; bh_inv = 0; bh_flash = 0; bh_shield = 3; bh_hits = 0;
    bh_score = 0; bh_graze = 0; bh_base = bh_base2 = 0;
#ifdef BH_DEBUG
    bh_maxnb = 0; bh_ndeaths = 0;
#endif
    tclear();
    bh_hud_static();
    bh_set_phase(0);
    gfx_show(BUF0);

}

static void bh_run(int32_t frames)
{
    bh_init();
    static const uint32_t bufs[2] = { BUF0, BUF1 };
    int cur = 1;
    int n = 1, fc = 0;
    sec_begin("bullet hell", frames);
    while (sec_running()) {
#ifdef BH_DEBUG
        bh_ft = 0;
#endif
        PR(0) bh_step(n); PR(8)
        gfx_target(bufs[cur]);
        bh_render(cur);
#ifdef BH_DEBUG
        { bh_fsum += bh_ft; if (bh_ft > bh_fmax) bh_fmax = bh_ft; if (bh_ft > 66000) bh_fover++; }
#endif
        n = frame_wait();
        gfx_show(bufs[cur]);
        cur ^= 1;
        if (n > 4) n = 4;
        if (!(++fc & 3)) bh_hud();
    }
    SCROLLX = 0;
    SCROLLY = 0;
#ifdef BH_DEBUG
    for (int i = 0; i < 9; i++) printf("%d:%lu ", i, (unsigned long)(bh_pa[i] / frames_drawn));
    printf("avg %lu max %lu over66k %lu ", (unsigned long)(bh_fsum / frames_drawn), (unsigned long)bh_fmax, (unsigned long)bh_fover);
    printf("bh: max bullets %d, hits %d, graze %d, score %u, deaths %d, phase %d\n",
           bh_maxnb, bh_hits, (int)bh_graze, (unsigned)bh_score, bh_ndeaths, bh_phase);
#endif
}
