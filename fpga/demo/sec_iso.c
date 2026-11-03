/* Section 5: an isometric flyer that plays itself.
 *
 * The floor is a procedural isometric world (32x16 pixel diamond tiles, a
 * level map of 256 rows x 16 columns in scratch). The camera moves (2,-1)
 * pixels a frame: along one of the tile axes, so the hardware scroll does the
 * moving and only the newly exposed strips are drawn - a byte column at the
 * right and a row at the top - by evaluating the world there: floor from
 * 16x16-byte textures (a byte is two pixels, and in "pair" coordinates
 * u = k + y, v = k - y the tile edges are the multiples of 16), then the
 * raised objects (walls, turrets, tanks, dishes) as sprites, back to front.
 * The ship, its shadow, bullets and explosions are sprites with save and
 * restore; destroyed targets are painted over with the world again. */

/* set-up and per-frame bookkeeping is compiled for size, the pixel loops for speed */
#define ISO_SZ __attribute__((minsize, noinline))

/* gfx.c's helpers are inlined at every call: one out-of-line copy each */
__attribute__((noinline)) static void iso_tfill(int r, int c, int n, uint8_t ch, uint16_t a) { tfill(r, c, n, ch, a); }
__attribute__((noinline)) static void iso_tput(int r, int c, const char *t, uint16_t a) { tput(r, c, t, a); }
__attribute__((noinline)) static void iso_tnum(int r, int c, uint16_t v, int d, uint16_t a) { tnum(r, c, v, d, a); }
__attribute__((noinline)) static uint16_t iso_rnd(uint16_t n) { return rnd_n(n); }

#define ISO_K0  4096                    /* the camera's start: byte column ... */
#define ISO_Y0  1690                    /* ... and row of the world */
#define ISO_IU0 386                     /* tile column on the ship's line at lat 0 */
#define ISO_JV0 126                     /* tile row of map row 0 */
#define ISO_BX  320                     /* the ship's ground point on the screen */
#define ISO_BY  230

static uint8_t  *iso_tex;               /* 16 textures x 256 bytes */
static uint8_t  *iso_map;               /* [row 0..255][col 0..15]: texture | object << 4 */
static uint8_t  *iso_rh;                /* per row: height of the wall in it */
static uint16_t *iso_rf;                /* per row: columns open at ground level */
static int iso_k, iso_y, iso_bc, iso_br;    /* the window: world byte column / row, bitmap byte column / row */

typedef struct { const sprite_t *s; int16_t oy; } iso_ob_t;
static iso_ob_t iso_ob[6];
static const sprite_t *iso_sp_ship, *iso_sp_shad, *iso_sp_pb, *iso_sp_eb, *iso_sp_bm[4];

/* ---- the level --------------------------------------------------------------------- */
static uint8_t iso_at(int jv, int iu)
{
    int c = iu - (ISO_IU0 - 8);
    if ((unsigned)c > 15) return 0;
    return iso_map[(((jv - ISO_JV0) & 255) << 4) + c];
}

#define ISO_NT 72
static uint8_t iso_tr[ISO_NT], iso_tc[ISO_NT], iso_tk[ISO_NT], iso_ta[ISO_NT], iso_tcd[ISO_NT];
static int iso_nt;

ISO_SZ static void iso_put(int r, int c, int tex, int obj)
{
    uint8_t *m = &iso_map[((r & 255) << 4) + c];
    if (tex >= 0) *m = (uint8_t)((*m & 0xF0) | tex);
    if (obj >= 0) {
        *m = (uint8_t)((*m & 15) | obj << 4);
        if (obj >= 3 && iso_nt < ISO_NT) {
            iso_tr[iso_nt] = (uint8_t)r; iso_tc[iso_nt] = (uint8_t)c;
            iso_tk[iso_nt] = (uint8_t)obj; iso_ta[iso_nt] = 1; iso_tcd[iso_nt] = (uint8_t)(30 + iso_rnd(60));
            iso_nt++;
        }
    }
}

ISO_SZ static void iso_wall(int r, int obj, int gaps)
{
    for (int c = 1; c <= 14; c++)
        if (!((gaps >> c) & 1)) iso_put(r, c, -1, obj);
    iso_rh[r & 255] = (uint8_t)(obj == 1 ? 20 : 10);
    iso_rf[r & 255] = (uint16_t)(gaps & 0x7FFE);
}

ISO_SZ static void iso_scatter(int r0, int n, int obj)
{
    while (n-- > 0)
        for (int t = 0; t < 8; t++) {
            int r = r0 + iso_rnd(16), c = 2 + iso_rnd(12);
            uint8_t m = iso_map[((r & 255) << 4) + c];
            if ((m >> 4) || (m & 15) < 4 || (m & 15) > 5) continue;
            if (iso_rh[(r - 1) & 255] || iso_rh[r & 255] || iso_rh[(r + 1) & 255]) continue;
            iso_put(r, c, -1, obj);
            break;
        }
}

ISO_SZ static void iso_sector(int r0, int kind)
{
    int g;
    for (int c = 2; c <= 13; c++) iso_put(r0, c, 6, -1);          /* a hazard stripe across the start */
    switch (kind) {
    case 0:  iso_scatter(r0, 3, 4); iso_scatter(r0, 3, 5); break;
    case 1:  iso_scatter(r0, 7, 3); iso_scatter(r0, 1, 4); break;
    case 2:
        g = 3 + iso_rnd(8); iso_wall(r0 + 8, 1, 3 << g);
        iso_scatter(r0, 3, 3); iso_scatter(r0, 1, 5); break;
    case 3:
        for (int r = 5; r <= 8; r++)
            for (int c = 1; c <= 14; c++) iso_put(r0 + r, c, 0, 0);
        iso_scatter(r0 + 9, 1, 5); break;
    case 4:
        iso_wall(r0 + 5, 2, 0);
        g = 2 + iso_rnd(9); iso_wall(r0 + 11, 1, 7 << g);
        iso_scatter(r0, 2, 4); iso_scatter(r0, 2, 3); break;
    case 5:
        g = 2 + iso_rnd(4); iso_wall(r0 + 4, 1, 3 << g);
        g = 8 + iso_rnd(4); iso_wall(r0 + 11, 1, 3 << g);
        iso_scatter(r0 + 6, 3, 3); break;
    case 6:
        for (int r = 6; r <= 9; r++)
            for (int c = 5; c <= 10; c++) iso_put(r0 + r, c, 7, -1);
        iso_scatter(r0, 4, 5); iso_scatter(r0, 3, 4); break;
    default:
        g = 2 + iso_rnd(9); iso_wall(r0 + 9, 1, (3 << g) | (3 << (g > 8 ? 3 : g + 3)));
        iso_scatter(r0, 4, 3); iso_scatter(r0, 3, 4); break;
    }
}

ISO_SZ static void iso_build(void)
{
    static const uint8_t seq[16] = { 0, 1, 2, 1, 3, 4, 5, 0, 2, 6, 5, 1, 7, 2, 4, 0 };
    iso_map = (uint8_t *)salloc(4096);
    iso_rh = (uint8_t *)salloc(256);
    iso_rf = (uint16_t *)salloc(512);
    iso_nt = 0;
    for (int r = 0; r < 256; r++) {
        iso_rh[r] = 0; iso_rf[r] = 0x7FFE;
        for (int c = 0; c < 16; c++) {
            int t = c == 0 ? 0 : c == 15 ? 11 : c == 1 ? 8 : c == 14 ? 10
                  : ((((r >> 1) + (c >> 1)) & 1) ? 5 : 4);
            if (t >= 4 && t <= 5 && ((r * 7 + c * 13) & 15) == 3) t = 9;
            iso_map[(r << 4) + c] = (uint8_t)t;
        }
    }
    for (int s = 0; s < 16; s++) iso_sector(s * 16, seq[s]);
}

/* ---- textures: 16x16 bytes (two pixels each), index a * 16 + b ----------------------- */
ISO_SZ static void iso_mktex(void)
{
    iso_tex = (uint8_t *)salloc(16 * 256);
    for (int t = 0; t < 16; t++)
        for (int a = 0; a < 16; a++)
            for (int b = 0; b < 16; b++) {
                int c = NAVY, c2 = -1;
                int d = (2 * a - 15 < 0 ? 15 - 2 * a : 2 * a - 15) + (2 * b - 15 < 0 ? 15 - 2 * b : 2 * b - 15);
                if (t < 4) c = BLACK;
                else if (t == 11) {                              /* the hull's side below the lower-right edge */
                    if (a < 10) {
                        c = a == 0 ? YELLOW : a == 9 ? BLACK : ((b & 7) == 0 ? NAVY : GREY);
                        if (a > 1 && a < 9 && (b & 7) == 4) c = TEAL;
                    } else c = BLACK;
                } else {
                    if (a == 1 || b == 1) c = TEAL;
                    if (t == 5 && a > 2 && a < 13 && b > 2 && b < 13) c = (a ^ b) & 1 ? NAVY : BLACK, c2 = (a ^ b) & 1 ? BLACK : NAVY;
                    if (t == 6) c = (a & 4) ? OLIVE : NAVY;
                    if (t == 7) c = TEAL, c2 = (a ^ b) & 1 ? BLACK : TEAL;
                    if (t == 8 && a <= 1) c = a ? OLIVE : YELLOW;
                    if (t == 10 && a >= 14) c = a == 15 ? YELLOW : OLIVE;
                    if (t == 9 && (((a & 3) == 2 && b > 2 && b < 13) || ((b & 3) == 2 && a > 2 && a < 13))) c = BLACK;
                    if (t == 12) c = d < 18 ? BLACK : d < 24 ? MAROON : c;
                    if (a == 0 || b == 0) { c = t == 7 ? WHITE : BLACK; c2 = -1; }
                }
                if (c2 < 0) c2 = c;
                iso_tex[(t << 8) + (a << 4) + b] = (uint8_t)(c << 4 | c2);
            }
    /* stars on the four space variants and in the hull tile's space part */
    for (int i = 0; i < 4 * 7; i++) {
        int t = i & 3, a = iso_rnd(16), b = iso_rnd(16);
        uint8_t col = (i & 4) ? GREY : (i & 8) ? CYAN : WHITE;
        iso_tex[(t << 8) + (a << 4) + b] = (uint8_t)((i & 1) ? col << 4 | col : col << 4);
        if (a >= 10) iso_tex[(11 << 8) + (a << 4) + b] = (uint8_t)(col << 4);
    }
}

/* ---- sprite art, drawn into a character canvas ----------------------------------------- */
static char *iso_cv;
static int iso_cw, iso_chh;

ISO_SZ static void iso_cvclear(int w, int h)
{
    iso_cw = w; iso_chh = h;
    for (int i = 0; i < w * h; i++) iso_cv[i] = '.';
}
static void iso_px(int x, int y, int c)
{
    if ((unsigned)x < (unsigned)iso_cw && (unsigned)y < (unsigned)iso_chh)
        iso_cv[y * iso_cw + x] = "0123456789abcdef"[c];
}
ISO_SZ static void iso_ell(int cx, int cy, int rx, int ry, int c)
{
    int rr = rx * rx * ry * ry;
    for (int y = -ry; y <= ry; y++)
        for (int x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rr) iso_px(cx + x, cy + y, c);
}
static const sprite_t *iso_cvspr(void) { return spr_make(iso_cv, iso_cw, iso_chh, 0); }

/* a box on a tile: 32 wide, 16 + hgt tall; the ground diamond's centre at row 8 + hgt */
ISO_SZ static const sprite_t *iso_box(int hgt, int ctop, int cl, int cr, int cr2, int cedge)
{
    iso_cvclear(32, 16 + hgt);
    for (int y = 0; y < 16 + hgt; y++)
        for (int x = 0; x < 32; x++) {
            int t = x < 16 ? x : 31 - x, hh = (t + 1) >> 1;
            int t0 = 8 - hh, t1 = 8 + hh;
            int c = -1;
            if (y >= t0 && y < t1) c = (y == t0 || y == t1 - 1) ? cedge : ctop;
            else if (y >= t1 && y < t1 + hgt) {
                c = x < 16 ? cl : ((x + y) & 1 ? cr : cr2);
                if (x == 15 || x == 16 || y == t1 + hgt - 1) c = BLACK;
                else if (y == t1) c = cedge;
                else if (hgt > 12 && (y - t1) % 8 == 4 && (x & 7) > 1 && (x & 7) < 6) c = BLACK;   /* slits */
            }
            if (c >= 0) iso_px(x, y, c);
        }
    return iso_cvspr();
}

ISO_SZ static const sprite_t *iso_mkturret(void)
{
    iso_cvclear(32, 30);
    iso_ell(16, 22, 13, 6, BLACK);
    iso_ell(16, 21, 12, 5, GREY);
    iso_ell(16, 21, 8, 3, NAVY);
    for (int i = 0; i < 12; i++) {                              /* the gun, pointing at the viewer */
        iso_px(14 - i, 15 + i / 2, BLACK); iso_px(15 - i, 15 + i / 2, GREY);
        iso_px(14 - i, 16 + i / 2, GREY);
    }
    iso_ell(16, 16, 8, 5, MAROON);
    iso_ell(16, 15, 7, 4, RED);
    iso_ell(14, 13, 3, 2, WHITE);
    return iso_cvspr();
}

ISO_SZ static const sprite_t *iso_mktank(void)
{
    iso_cvclear(32, 34);
    for (int y = 12; y <= 26; y++)
        for (int x = 6; x <= 26; x++)
            iso_px(x, y, y == 19 ? YELLOW : x < 10 ? WHITE : x < 20 ? GREEN : DGREEN);
    iso_ell(16, 26, 10, 5, DGREEN);
    iso_ell(16, 12, 10, 5, BLACK);
    iso_ell(16, 12, 9, 4, GREY);
    iso_ell(16, 12, 4, 2, WHITE);
    return iso_cvspr();
}

ISO_SZ static const sprite_t *iso_mkdish(void)
{
    iso_cvclear(32, 34);
    iso_ell(16, 26, 9, 4, GREY);
    for (int y = 14; y < 26; y++) iso_px(16, y, WHITE);
    iso_ell(16, 12, 13, 8, WHITE);
    iso_ell(17, 13, 10, 5, CYAN);
    iso_ell(17, 14, 5, 2, TEAL);
    for (int y = 4; y < 13; y++) iso_px(17, y, YELLOW);
    return iso_cvspr();
}

ISO_SZ static const sprite_t *iso_mkboom(int r)
{
    int w = 2 * r + 2;
    iso_cvclear(w, w);
    for (int y = 0; y < w; y++)
        for (int x = 0; x < w; x++) {
            int dx = 2 * x + 1 - w, dy = 2 * y + 1 - w;
            int d2 = (dx * dx + dy * dy) * 4, r2 = (w - 1) * (w - 1) * 4 / 4 + 1;  /* d2 / r2 in 0..4 */
            d2 = (dx * dx + dy * dy);
            r2 = r * r * 4;
            if (((x * 5 + y * 3) & 7) == 0 && d2 * 3 > r2 * 2) continue;       /* ragged rim */
            if (d2 > r2) continue;
            iso_px(x, y, d2 * 4 < r2 ? WHITE : d2 * 2 < r2 ? YELLOW : d2 * 4 < r2 * 3 ? RED : MAROON);
        }
    return iso_cvspr();
}

static const char iso_art_ship[] =
    ".............................ff."
    "...........................ff77."
    ".....................000fff7777."
    "..........000000033333ff777770.."
    "00000033333333333336ff7777711..."
    ".3333333333333111666677711110..."
    ".0333333333311fff66777111111...."
    "..0333333333ff77777111111110...."
    "..0333333fff777771111111111....."
    "...0333ff777773311111111110....."
    "....fff7777733333111111111......"
    "...44777733333333331111110......"
    "..64777333333333333331110......."
    "...44.....000000333333110......."
    "...................00000........"
    "................................";

ISO_SZ static void iso_mksprites(void)
{
    iso_cv = (char *)salloc(32 * 40);
    iso_ob[1].s = iso_box(20, GREY, RED, MAROON, BLACK, WHITE);   iso_ob[1].oy = 28;
    iso_ob[2].s = iso_box(10, YELLOW, OLIVE, OLIVE, BLACK, WHITE); iso_ob[2].oy = 18;
    iso_ob[3].s = iso_mkturret(); iso_ob[3].oy = 22;
    iso_ob[4].s = iso_mktank();   iso_ob[4].oy = 26;
    iso_ob[5].s = iso_mkdish();   iso_ob[5].oy = 26;
    iso_sp_ship = spr_make(iso_art_ship, 32, 16, 0);
    char *sh = (char *)salloc(32 * 16);
    for (int i = 0; i < 32 * 16; i++) sh[i] = iso_art_ship[i] != '.' && ((i + i / 32) & 1) ? '0' : '.';
    iso_sp_shad = spr_make(sh, 32, 16, 0);
    iso_cvclear(4, 4);
    iso_ell(1, 1, 1, 1, YELLOW); iso_px(1, 1, WHITE); iso_px(2, 2, YELLOW);
    iso_sp_pb = iso_cvspr();
    iso_cvclear(4, 4);
    iso_ell(1, 1, 1, 1, RED); iso_px(1, 1, WHITE); iso_px(2, 2, MAGENTA);
    iso_sp_eb = iso_cvspr();
    iso_sp_bm[0] = iso_mkboom(4);
    iso_sp_bm[1] = iso_mkboom(7);
    iso_sp_bm[2] = iso_mkboom(10);
    iso_sp_bm[3] = iso_mkboom(8);
}

/* ---- painting the world into the bitmap (window coordinates: world byte column k, row y) -- */
static int iso_colof(int k) { int c = iso_bc + (k - iso_k); return c >= 320 ? c - 320 : c; }
static int iso_rowof(int y) { int r = iso_br + (y - iso_y); return r >= 360 ? r - 360 : r; }

static const uint8_t *iso_texp(int iu, int jv)
{
    uint8_t m = iso_at(jv, iu) & 15;
    if (!m) m = (uint8_t)((iu * 3 + jv * 5) & 3);
    return iso_tex + (m << 8);
}

/* byte column k, rows y0 .. y1-1 */
static void iso_floor_col(int k, int y0, int y1)
{
    int u = k + y0, v = k - y0;
    int a = u & 15, b = v & 15, iu = u >> 4, jv = v >> 4;
    int by = iso_rowof(y0), n = y1 - y0;
    uint8_t *base = g_buf + iso_colof(k);
    uint8_t *p = base + rowoff[by];
    while (n > 0) {
        int run = 16 - a;
        if (b + 1 < run) run = b + 1;
        if (run > n) run = n;
        if (by + run > 360) run = 360 - by;
        const uint8_t *t = iso_texp(iu, jv);
        int idx = (a << 4) + b;
        for (int i = run; i > 0; i--) { *p = t[idx]; p += 320; idx += 15; }
        n -= run; by += run;
        if (by >= 360) { by = 0; p = base; }
        a += run; b -= run;
        if (a >= 16) { a -= 16; iu++; }
        if (b < 0) { b += 16; jv--; }
    }
}

/* row y, byte columns k0 .. k1-1 */
static void iso_floor_row(int y, int k0, int k1)
{
    int u = k0 + y, v = k0 - y;
    int a = u & 15, b = v & 15, iu = u >> 4, jv = v >> 4;
    uint8_t *row = g_buf + rowoff[iso_rowof(y)];
    int bc = iso_colof(k0), k = k0;
    while (k < k1) {
        int run = 16 - a;
        if (16 - b < run) run = 16 - b;
        if (run > k1 - k) run = k1 - k;
        if (bc + run > 320) run = 320 - bc;
        const uint8_t *t = iso_texp(iu, jv);
        int idx = (a << 4) + b;
        uint8_t *p = row + bc;
        for (int i = run; i > 0; i--) { *p++ = t[idx]; idx += 17; }
        k += run; bc += run;
        if (bc >= 320) bc -= 320;
        a += run; b += run;
        if (a >= 16) { a -= 16; iu++; }
        if (b >= 16) { b -= 16; jv++; }
    }
}

/* an object sprite of the tile with sprite column ks (16 bytes wide) and ground centre row yc,
 * clipped to the rectangle */
static void iso_blit(int o, int ks, int yc, int ck0, int ck1, int cy0, int cy1)
{
    const sprite_t *s = iso_ob[o].s;
    int top = yc - iso_ob[o].oy;
    int x0 = ks > ck0 ? ks : ck0, x1 = ks + 16 < ck1 ? ks + 16 : ck1;
    int y0 = top > cy0 ? top : cy0, y1 = top + s->h < cy1 ? top + s->h : cy1;
    int st = s->wb * 2;
    while (x0 < x1 && y0 < y1) {
        int n = x1 - x0, bc = iso_colof(x0);
        if (bc + n > 320) n = 320 - bc;
        const uint8_t *q = s->v[0] + (y0 - top) * st + (x0 - ks) * 2;
        int by = iso_rowof(y0);
        for (int y = y0; y < y1; y++, q += st) {
            uint8_t *p = g_buf + rowoff[by] + bc;
            const uint8_t *qq = q;
            for (int i = 0; i < n; i++, qq += 2) {
                uint8_t m = qq[1];
                if (m != 0xFF) p[i] = (uint8_t)((p[i] & m) | qq[0]);
            }
            if (++by == 360) by = 0;
        }
        x0 += n;
    }
}

static void iso_objs(int ck0, int ck1, int cy0, int cy1)
{
    int sa = (ck0 >> 3) - 1, sb = (ck1 - 1) >> 3;
    int d1 = (cy1 + 27) >> 3;
    for (int d = (cy0 - 8) >> 3; d <= d1; d++) {
        int s = sa;
        if ((s ^ d) & 1) s++;
        for (; s <= sb; s += 2) {
            int o = iso_at((s - d) >> 1, (s + d) >> 1) >> 4;
            if (o) iso_blit(o, s << 3, d << 3, ck0, ck1, cy0, cy1);
        }
    }
}

static void iso_paint(int ck0, int ck1, int cy0, int cy1)
{
    if ((ck1 - ck0) * 2 <= cy1 - cy0) for (int k = ck0; k < ck1; k++) iso_floor_col(k, cy0, cy1);
    else for (int y = cy0; y < cy1; y++) iso_floor_row(y, ck0, ck1);
    iso_objs(ck0, ck1, cy0, cy1);
}

/* ---- dynamic sprites: drawn with their background saved, put back in reverse ------------- */
#define ISO_MAXSV 28
typedef struct { uint8_t *sv; int16_t bc, br; uint8_t nb, h; } iso_sv_t;
static iso_sv_t iso_sv[ISO_MAXSV];
static int iso_nsv;
static uint8_t *iso_pool, *iso_poolp;

static void iso_dspr(const sprite_t *s, int sx, int sy)
{
    int w = s->w, h = s->h, nb = s->wb;
    if (sx < 8 || sy < 8 || sx + w > 632 || sy + h > 344 || iso_nsv >= ISO_MAXSV) return;
    int bx = iso_bc * 2 + sx;
    if (bx >= 640) bx -= 640;
    int bc = bx >> 1, by = iso_br + sy;
    if (by >= 360) by -= 360;
    const uint8_t *q = s->v[bx & 1];
    uint8_t *sv = iso_poolp;
    iso_poolp += nb * h;
    iso_sv_t *e = &iso_sv[iso_nsv++];
    e->sv = sv; e->bc = (int16_t)bc; e->br = (int16_t)by; e->nb = (uint8_t)nb; e->h = (uint8_t)h;
    for (int r = 0; r < h; r++) {
        uint8_t *rr = g_buf + rowoff[by];
        if (bc + nb <= 320) {
            uint8_t *p = rr + bc;
            for (int b = 0; b < nb; b++, q += 2) { uint8_t x = p[b]; *sv++ = x; p[b] = (uint8_t)((x & q[1]) | q[0]); }
        } else {
            for (int b = 0; b < nb; b++, q += 2) {
                int c = bc + b;
                if (c >= 320) c -= 320;
                uint8_t x = rr[c]; *sv++ = x; rr[c] = (uint8_t)((x & q[1]) | q[0]);
            }
        }
        if (++by == 360) by = 0;
    }
}

static void iso_undo(void)
{
    while (iso_nsv > 0) {
        iso_sv_t *e = &iso_sv[--iso_nsv];
        const uint8_t *sv = e->sv;
        int bc = e->bc, by = e->br, nb = e->nb;
        for (int r = 0; r < e->h; r++) {
            uint8_t *rr = g_buf + rowoff[by];
            if (bc + nb <= 320) {
                uint8_t *p = rr + bc;
                for (int b = 0; b < nb; b++) p[b] = *sv++;
            } else {
                for (int b = 0; b < nb; b++) {
                    int c = bc + b;
                    if (c >= 320) c -= 320;
                    rr[c] = *sv++;
                }
            }
            if (++by == 360) by = 0;
        }
    }
    iso_poolp = iso_pool;
}

/* ---- the game ------------------------------------------------------------------------------ */
#define ISO_NB 12
#define ISO_NE 5
typedef struct { int16_t x, y, vx, vy; int16_t life; int8_t kind, tgt; } iso_bul_t;   /* 1/16 pixel */
static iso_bul_t iso_bul[ISO_NB];
static int16_t iso_ex[ISO_NE], iso_ey[ISO_NE];
static int8_t  iso_eage[ISO_NE];
static int iso_latq, iso_altq;          /* lateral offset and altitude, in 1/8 pixel */
static uint16_t iso_t;
static int iso_score, iso_fuel, iso_shield, iso_cool, iso_hud_alt;

ISO_SZ static void iso_boom(int x, int y, int age)
{
    for (int i = 0; i < ISO_NE; i++)
        if (iso_eage[i] < 0) { iso_ex[i] = (int16_t)x; iso_ey[i] = (int16_t)y; iso_eage[i] = (int8_t)age; return; }
}

ISO_SZ static void iso_shoot(int x0, int y0, int tx, int ty, int kind, int tgt, int speed)
{
    for (int i = 0; i < ISO_NB; i++) {
        iso_bul_t *b = &iso_bul[i];
        if (b->life > 0) continue;
        int dx = tx - x0, dy = ty - y0;
        int m = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
        if (m < 4) m = 4;
        b->x = (int16_t)(x0 * 16); b->y = (int16_t)(y0 * 16);
        b->vx = (int16_t)((int32_t)dx * (16 * speed) / m - 32);        /* the world moves under it */
        b->vy = (int16_t)((int32_t)dy * (16 * speed) / m + 16);
        b->life = (int16_t)(m / speed + 1);
        b->kind = (int8_t)kind; b->tgt = (int8_t)tgt;
        return;
    }
}

ISO_SZ static void iso_kill(int t)
{
    int r = iso_tr[t], c = iso_tc[t];
    int iu = ISO_IU0 - 8 + c, jv = ISO_JV0 + r;
    int ks = 8 * (iu + jv), yc = 8 * (iu - jv);
    iso_ta[t] = 0;
    iso_put(r, c, 12, 0);
    int x0 = ks > iso_k ? ks : iso_k, x1 = ks + 16 < iso_k + 320 ? ks + 16 : iso_k + 320;
    int y0 = yc - 28 > iso_y ? yc - 28 : iso_y, y1 = yc + 8 < iso_y + 360 ? yc + 8 : iso_y + 360;
    if (x0 < x1 && y0 < y1) iso_paint(x0, x1, y0, y1);
    iso_score += iso_tk[t] == 3 ? 3 : 2;
    if (iso_tk[t] == 4) { iso_fuel += 12; if (iso_fuel > 99) iso_fuel = 99; }
}

static int iso_hgt(int r, int u)
{
    int c = (u >> 4) - (ISO_IU0 - 8);
    if ((unsigned)c > 15) return 0;
    int o = iso_map[((r & 255) << 4) + c] >> 4;
    return o == 1 ? 20 : o == 2 ? 10 : 0;
}

ISO_SZ static void iso_game(int n)
{
    int lat = iso_latq >> 3, alt = iso_altq >> 3;
    int gx = ISO_BX + 2 * lat, gy = ISO_BY + lat;
    int u = ISO_K0 + ISO_Y0 + (gx >> 1) + gy;
    int v = iso_k - iso_y + (gx >> 1) - gy;
    int rs = (v >> 4) - ISO_JV0;
    int cc = 8 + (lat >> 3);

    /* the autopilot: weave, find the way through the next wall, climb when it can't */
    int wl = (SIN(iso_t * 3) * 40) >> 7, wa = 20 + ((SIN(iso_t * 2 + 64) * 6) >> 7), need = 0;
    for (int r = rs - 1; r <= rs + 7; r++) {
        int h = iso_rh[r & 255];
        if (!h) continue;
        uint16_t f = iso_rf[r & 255];
        if (r <= rs + 1) { if (!((f >> cc) & 1) || !((f >> (cc + 1)) & 1)) need = h + 12; continue; }
        int best = -1, bd = 999;
        for (int c = 2; c <= 12; c++)
            if (((f >> c) & 1) && ((f >> (c + 1)) & 1)) {
                int dd = 8 * (c - 7) - lat; if (dd < 0) dd = -dd;
                if (dd < bd) { bd = dd; best = c; }
            }
        if (best >= 0 && bd <= (r - rs - 1) * 8) { wl = 8 * (best - 7); wa = 9; }
        else { need = h + 12; }
        break;
    }
    if (wa < need) wa = need;
    int dl = wl * 8 - iso_latq, da = wa * 8 - iso_altq;
    int sl = 12 * n, sa = 6 * n;
    iso_latq += dl > sl ? sl : dl < -sl ? -sl : dl;
    iso_altq += da > sa ? sa : da < -sa ? -sa : da;
    lat = iso_latq >> 3; alt = iso_altq >> 3;
    gx = ISO_BX + 2 * lat; gy = ISO_BY + lat;
    int shx = gx, shy = gy - iso_hgt(v >> 4, u + 2 * (lat - (iso_latq >> 3)));

    /* targets in view: fire at one ahead, turrets fire back */
    int sx = gx, sy = gy - alt - 2;
    int ibest = -1, bestd = 9999;
    int ev = 0;
    for (int i = 0; i < ISO_NB; i++) if (iso_bul[i].life > 0 && iso_bul[i].kind) ev++;
    if (iso_cool > 0) iso_cool -= n;
    for (int t = 0; t < iso_nt; t++) {
        if (!iso_ta[t]) continue;
        int iu = ISO_IU0 - 8 + iso_tc[t], jv = ISO_JV0 + iso_tr[t];
        int tx = 2 * (8 * (iu + jv + 1) - iso_k), ty = 8 * (iu - jv) - iso_y;
        if (tx < 70 || tx > 570 || ty < 60 || ty > 300) continue;
        int du = 16 * iu + 8 - u, dv = 16 * jv + 8 - v;
        if (dv > 24 && dv < 300 && du > -60 && du < 60 && dv < bestd) { bestd = dv; ibest = t; }
        if (iso_tk[t] == 3) {
            iso_tcd[t] = (uint8_t)(iso_tcd[t] > n ? iso_tcd[t] - n : 0);
            if (!iso_tcd[t] && ev < 4 && dv > -150 && dv < 330) {
                iso_shoot(tx - 6, ty - 8, sx + 30, sy - 15, 1, -1, 4);
                iso_tcd[t] = (uint8_t)(70 + iso_rnd(80));
                ev++;
            }
        }
    }
    if (ibest >= 0 && iso_cool <= 0) {
        int t = ibest;
        int iu = ISO_IU0 - 8 + iso_tc[t], jv = ISO_JV0 + iso_tr[t];
        iso_shoot(sx + 4, sy - 2, 2 * (8 * (iu + jv + 1) - iso_k), 8 * (iu - jv) - iso_y - 6, 0, t, 8);
        iso_cool = 14;
    }

    /* bullets */
    for (int i = 0; i < ISO_NB; i++) {
        iso_bul_t *b = &iso_bul[i];
        if (b->life <= 0) continue;
        b->x += b->vx * n; b->y += b->vy * n; b->life -= n;
        int bx = b->x >> 4, by = b->y >> 4;
        if (b->kind == 0) {
            if (b->life <= 0) {
                if (b->tgt >= 0 && iso_ta[(int)b->tgt]) { iso_kill(b->tgt); iso_boom(bx, by, 0); }
                else iso_boom(bx, by, 4);
            }
        } else {
            int dx = bx - sx, dy = by - (sy - 2);
            if (dx < 10 && dx > -10 && dy < 7 && dy > -7) {
                b->life = 0; iso_boom(sx, sy, 4);
                if (iso_shield > 1) iso_shield--;
            }
        }
        if (bx < 12 || bx > 628 || by < 12 || by > 336) b->life = 0;
    }
    for (int i = 0; i < ISO_NE; i++)
        if (iso_eage[i] >= 0) {
            iso_ex[i] -= 2 * n; iso_ey[i] += n; iso_eage[i] += n;
            if (iso_eage[i] >= 14) iso_eage[i] = -1;
        }

    /* draw: shadow, ship, bullets, explosions */
    iso_dspr(iso_sp_shad, shx - 16, shy - 8);
    iso_dspr(iso_sp_ship, gx - 16, gy - 8 - alt);
    for (int i = 0; i < ISO_NB; i++)
        if (iso_bul[i].life > 0) iso_dspr(iso_bul[i].kind ? iso_sp_eb : iso_sp_pb, (iso_bul[i].x >> 4) - 2, (iso_bul[i].y >> 4) - 2);
    for (int i = 0; i < ISO_NE; i++)
        if (iso_eage[i] >= 0) {
            static const uint8_t fr[7] = { 0, 1, 2, 3, 3, 2, 1 };
            const sprite_t *s = iso_sp_bm[fr[iso_eage[i] >> 1]];
            iso_dspr(s, iso_ex[i] - s->w / 2, iso_ey[i] - s->h / 2);
        }
    iso_hud_alt = alt;
}

ISO_SZ static void iso_bar(int row, int col, int n, int max, int fg)
{
    iso_tfill(row, col, max, '\xB0', ATTR(GREY, BLACK));
    iso_tfill(row, col, n, '\xDB', ATTR(fg, BLACK));
}

ISO_SZ static void iso_hud(void)
{
    int al = iso_hud_alt >> 2;
    for (int r = 0; r < 16; r++)
        iso_tfill(5 + r, 76, 2, 15 - r <= al ? '\xDB' : '\xB0', ATTR(15 - r > 10 ? RED : 15 - r > 5 ? YELLOW : GREEN, BLACK));
    iso_tnum(0, 8, (uint16_t)iso_score, 5, ATTR(YELLOW, BLACK));
    iso_bar(1, 7, (iso_fuel + 9) / 10, 10, iso_fuel < 25 ? RED : GREEN);
    iso_bar(1, 29, iso_shield, 5, CYAN);
}

static void iso_step(int n)
{
    iso_k += n; iso_y -= n;
    iso_bc += n; if (iso_bc >= 320) iso_bc -= 320;
    iso_br -= n; if (iso_br < 0) iso_br += 360;
    SCROLLX = (uint16_t)(iso_bc * 2);
    SCROLLY = (uint16_t)iso_br;
}

ISO_SZ static void iso_run(int32_t frames)
{
    salloc_reset();
    gfx_mode16();
    tclear();
    iso_mktex();
    iso_build();
    iso_mksprites();
    iso_pool = iso_poolp = (uint8_t *)salloc(4096);
    iso_nsv = 0;

    uint32_t buf = g_shown == BUF0 ? BUF1 : BUF0;
    gfx_target(buf);
    iso_k = ISO_K0; iso_y = ISO_Y0;
    iso_bc = ISO_K0 % 320; iso_br = ISO_Y0 % 360;
    iso_paint(iso_k, iso_k + 320, iso_y, iso_y + 360);
    SCROLLX = (uint16_t)(iso_bc * 2);
    SCROLLY = (uint16_t)iso_br;

    for (int i = 0; i < ISO_NB; i++) iso_bul[i].life = 0;
    for (int i = 0; i < ISO_NE; i++) iso_eage[i] = -1;
    iso_latq = 0; iso_altq = 20 * 8; iso_t = 0; iso_score = 0; iso_fuel = 80; iso_shield = 5; iso_cool = 0;
    iso_hud_alt = 20;
    iso_tfill(0, 0, 80, ' ', ATTR(WHITE, BLACK));
    iso_tfill(1, 0, 80, ' ', ATTR(WHITE, BLACK));
    iso_tput(0, 1, "SCORE", ATTR(WHITE, BLACK));
    iso_tput(0, 13, "00", ATTR(YELLOW, BLACK));
    iso_tput(0, 22, "ISO STATION RAID", ATTR(CYAN, BLACK));
    iso_tput(1, 1, "FUEL", ATTR(WHITE, BLACK));
    iso_tput(1, 20, "SHIELD", ATTR(WHITE, BLACK));
    iso_tput(4, 75, "ALT", ATTR(WHITE, BLACK));
    iso_hud();
    gfx_show(buf);
    VID_CTRL = VC_TEXT | VC_BMP | VC_C16;

    sec_begin("isometric flyer", frames);
    uint16_t fuelt = 0, regen = 0;
    while (sec_running()) {
        int n = frame_wait();
        if (n > 4) n = 4;
#ifdef ISO_TESTN
        n = ISO_TESTN;
#endif
        iso_step(n);
        iso_paint(iso_k, iso_k + 320, iso_y, iso_y + n);                       /* the new top row(s) */
        iso_paint(iso_k + 320 - n, iso_k + 320, iso_y + n, iso_y + 360);       /* the new right column(s) */
        iso_undo();
        iso_game(n);
        iso_t += n;
        fuelt += n; regen += n;
        if (fuelt >= 24) { fuelt -= 24; if (iso_fuel > 8) iso_fuel--; }
        if (regen >= 240) { regen = 0; if (iso_shield < 5) iso_shield++; }
        if (!(iso_t & 3)) iso_hud();
    }
#ifndef ISO_KEEP
    SCROLLX = 0;
    SCROLLY = 0;
#endif
}
