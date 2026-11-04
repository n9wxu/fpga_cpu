/* Section 4: the finale, the Tang pulling ahead, in time with "T1 Rush" (the generated 8-channel module):
 * the scenes and flashes follow the replayer's song position and beats.
 *   piano intro   a title, the fly-through fading in
 *   groove        the fly-through: a CPU texture-mapped tunnel at low resolution (160 x 90 texels, 4x4
 *                 chunky, the rows doubled by the blitter) whose walls are the T1 logo, after the FTA demos on
 *                 the IIGS; stars stream out as sprites; a big T1 on the sprite buffer layer strobes on kicks
 *   drum & bass   playfield B: a full-screen 256-colour vortex, moved only by the palette and warped by line
 *                 scroll on the beat; playfield A (double-buffered): a CPU texture-mapped cube; 64 shaded
 *                 spheres as hardware sprites, depth-sorted; blended text panels
 *   255 BPM       all of it, and the chip voices flood (SID effects over the module, 24 voices)
 *   the end       chord hits flash white; the last one swells into a 24-voice supersaw */

#define F_TT   (SCR_FREE + 0x00000UL)           /* tunnel texel table, 160 x 90 words: shade << 14 | v << 7 | u */
#define F_TEX  (SCR_FREE + 0x07100UL)           /* the tunnel's texture, 128 x 128, twice: lit, then shaded */
#define F_CTEX (SCR_FREE + 0x0F100UL)           /* the cube's texture, 64 x 64 */
#define F_BALL (SCR_FREE + 0x10100UL)           /* the sphere sprite, 32 x 32 */
#define F_STAR (SCR_FREE + 0x10500UL)           /* the star sprite, 8 x 8 */
#define F_RAMP (SCR_FREE + 0x10600UL)           /* vortex colour ramps: 4 sets x 3 flash levels x 256 words */
/* the sprite buffer layer, 4 bpp (115,200 bytes): just after the program's BSS, below the stack (0x40000) */
extern char _end[];
static uint32_t f_sbuf;
#define SBUF f_sbuf
#define F_CUBE 192                              /* 192-239: the cube's texture in three light levels */
#define F_SPH  240                              /* 240-255: the spheres */

static uint32_t f_pfa[2], f_vortex;
static int16_t f_bb[2][4];
static uint8_t f_cur;

/* ---- precomputation */
static uint8_t f_angle(int dx, int dy)          /* 0-255 for the vector (dx, dy) */
{
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
    uint8_t a = mx ? atan64[(uint16_t)(mn * 64) / (uint16_t)mx] : 0;
    if (ay > ax) a = (uint8_t)(64 - a);
    if (dx < 0) a = (uint8_t)(128 - a);
    if (dy < 0) a = (uint8_t)(256 - a);
    return a;
}
static uint16_t f_dist(int dx, int dy)
{
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
    return (uint16_t)(mx + (mn * 3 >> 3));
}
static void f_tables(void)
{
    int x, y;
    /* the tunnel: angle and depth per texel (aspect 16:9 in texels) */
    for (y = 0; y < 90; y++) {
        for (x = 0; x < 160; x++) {
            int dx = 2 * x - 159, dy = 2 * y - 89;
            uint16_t d = f_dist(dx, dy) + 1;
            uint32_t i = (uint32_t)y * 160 + x;
            *(volatile uint16_t *)(F_TT + 2 * i) = (uint16_t)((d < 50 ? 0x4000 : 0) | ((6000 / d) & 127) << 7 | (f_angle(dx, dy) >> 1));
        }
        poll();
    }
    /* the tunnel's texture: bands, and T1 twice around (colours 1-63) */
    for (y = 0; y < 128; y++)
        for (x = 0; x < 128; x++)
            *(volatile uint8_t *)(F_TEX + (uint32_t)y * 128 + x) = (uint8_t)(1 + ((x >> 3) + (y >> 3)) % 3 * 3 + ((y & 15) == 0 || (x & 15) == 0 ? 8 : 0));
    {
        const char *s = "T1T1";
        int n, r, b;
        for (n = 0; n < 4; n++) {
            const uint8_t *g = font8x14[(uint8_t)s[n]];
            for (r = 0; r < 14; r++)
                for (b = 0; b < 8; b++)
                    if ((g[r] >> (7 - b)) & 1) {
                        int px, py;
                        for (py = 0; py < 4; py++)
                            for (px = 0; px < 6; px++)
                                *(volatile uint8_t *)(F_TEX + (uint32_t)((r * 4 + py + 4 + 64 * (n >> 1)) & 127) * 128 +
                                                      (uint32_t)((b * 7 + px + 4 + 64 * (n & 1)) & 127)) = (uint8_t)(32 + r * 2);
                    }
            poll();
        }
    }
    {   /* the shaded copy: the same texels, colours 65-127 */
        uint16_t i;
        for (i = 0; i < 16384; i++)
            *(volatile uint8_t *)(F_TEX + 16384UL + i) = (uint8_t)(*(volatile uint8_t *)(F_TEX + i) + 64);
    }
    /* the cube's texture: a framed T1, 16 colours */
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            uint8_t c = (uint8_t)(((x ^ y) >> 3) & 1 ? 2 : 4);
            if (x < 3 || y < 3 || x > 60 || y > 60) c = 15;
            *(volatile uint8_t *)(F_CTEX + (uint32_t)y * 64 + x) = c;
        }
    {
        int n, r, b;
        for (n = 0; n < 2; n++) {
            const uint8_t *g = font8x14["T1"[n]];
            for (r = 0; r < 14; r++)
                for (b = 0; b < 8; b++)
                    if ((g[r] >> (7 - b)) & 1) {
                        int px, py;
                        for (py = 0; py < 3; py++)
                            for (px = 0; px < 3; px++)
                                *(volatile uint8_t *)(F_CTEX + (uint32_t)(r * 3 + py + 11) * 64 + (uint32_t)(b * 3 + px + 6 + n * 27)) = (uint8_t)(10 + (r >> 2));
                    }
        }
    }
    /* sprites: a shaded sphere and a star */
    for (y = 0; y < 32; y++)
        for (x = 0; x < 32; x++) {
            int dx = x - 16, dy = y - 16, c = 0;
            if (dx * dx + dy * dy < 240) {
                int lx = x - 11, ly = y - 10, l2 = lx * lx + ly * ly;
                c = F_SPH + (l2 < 256 ? l2 / 17 : 15);
            }
            *(volatile uint8_t *)(F_BALL + (uint32_t)y * 32 + x) = (uint8_t)c;
        }
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            int dx = 2 * x - 7, dy = 2 * y - 7, c = 0;
            if (dx * dx + dy * dy < 16) c = F_SPH;
            else if ((dx == -1 || dx == 1 || dy == -1 || dy == 1) && dx * dx + dy * dy < 60) c = F_SPH + 4;
            *(volatile uint8_t *)(F_STAR + (uint32_t)y * 8 + x) = (uint8_t)c;
        }
    /* the vortex: depth + 2 x angle, one quadrant computed, mirrored four ways (colours 1-191) */
    for (y = 0; y < 180; y++) {
        volatile uint8_t *r0 = (volatile uint8_t *)(f_vortex + (uint32_t)y * W);
        volatile uint8_t *r1 = (volatile uint8_t *)(f_vortex + (uint32_t)(359 - y) * W);
        int dy = 179 - y;
        for (x = 0; x < 320; x++) {
            int dx = 319 - x;
            uint16_t d = f_dist(dx, dy) + 4;
            uint8_t depth = (uint8_t)(24000u / d);
            uint8_t a = f_angle(dx, dy);        /* (dx, dy) in the first quadrant */
            uint8_t tl = (uint8_t)(128 - a), tr = a, bl = (uint8_t)(128 + a), br = (uint8_t)(256 - a);
            r0[x]       = (uint8_t)(1 + (uint8_t)(depth + 2 * tl) * 191 / 256);
            r0[639 - x] = (uint8_t)(1 + (uint8_t)(depth + 2 * tr) * 191 / 256);
            r1[x]       = (uint8_t)(1 + (uint8_t)(depth + 2 * bl) * 191 / 256);
            r1[639 - x] = (uint8_t)(1 + (uint8_t)(depth + 2 * br) * 191 / 256);
        }
        poll();
    }
    /* the vortex's colour ramps: 4 sets, each plain and at two flash levels */
    {
        uint16_t set, lv, i;
        for (set = 0; set < 4; set++) {
            for (i = 0; i < 256; i++) {
                uint8_t ph = (uint8_t)i;
                uint16_t c;
                switch (set) {
                case 0: c = hsv565(ph, 230, (uint8_t)(140 + SIN(ph * 2) / 2 + 60)); break;
                case 1: c = mix565(RGB(40, 0, 0), RGB(255, 220, 60), (uint16_t)(128 + SIN(ph))); break;
                case 2: c = mix565(RGB(0, 10, 60), RGB(150, 255, 255), (uint16_t)(128 + SIN(ph))); break;
                default: c = hsv565((uint8_t)(ph >> 1), 255, (uint8_t)(128 + SIN(ph * 3) / 2 + 60)); break;
                }
                for (lv = 0; lv < 3; lv++)
                    *(volatile uint16_t *)(F_RAMP + ((uint32_t)(set * 3 + lv) * 256 + i) * 2) =
                        lv ? mix565(c, RGB(255, 255, 255), (uint16_t)(lv * 90)) : c;
            }
            poll();
        }
    }
}

/* ---- the tunnel: one frame into buf (a texel: one table word, an add, a mask, a texture byte, two stores) */
COLD static void f_tunnel(uint32_t buf, uint8_t du, uint8_t dv)
{
    int y, x;
    const uint16_t *tp = (const uint16_t *)F_TT;
    const uint8_t *tex = (const uint8_t *)F_TEX;
    uint16_t dofs = (uint16_t)((uint16_t)(dv & 127) << 7 | du);
    for (y = 0; y < 90; y++) {
        volatile uint16_t *p = (volatile uint16_t *)(buf + (uint32_t)y * 4 * W);
        for (x = 0; x < 160; x += 2) {
            uint16_t w0 = tp[0], w1 = tp[1];
            uint8_t c0 = tex[(w0 & 0x4000) | ((uint16_t)(w0 + dofs) & 0x3FFF)];
            uint8_t c1 = tex[(w1 & 0x4000) | ((uint16_t)(w1 + dofs) & 0x3FFF)];
            volatile uint8_t *pb = (volatile uint8_t *)p;    /* (byte stores: no multiply to double a byte) */
            pb[0] = c0; pb[1] = c0; pb[2] = c0; pb[3] = c0;
            pb[4] = c1; pb[5] = c1; pb[6] = c1; pb[7] = c1;
            p += 4;
            tp += 2;
        }
        blt_copy(buf + (uint32_t)y * 4 * W, 0, buf + (uint32_t)(y * 4 + 1) * W, W, W, 3, 0, 0);
        if (!(y & 15)) poll();
    }
}
COLD static void f_tunnel_pal(uint16_t t, uint16_t k, uint16_t flash)
{
    static uint16_t buf[192];
    uint8_t band, c;
    for (band = 0; band < 2; band++)
        for (c = 0; c < 64; c++) {
            uint16_t col;
            if (c >= 32) col = hsv565((uint8_t)(t * 2 + c * 4), 200, 255);              /* the letters */
            else if (c >= 9) col = RGB(255, 255, 255);                                  /* the grid */
            else col = hsv565((uint8_t)(150 + c * 8 + (t >> 1)), 220, (uint8_t)(90 + c * 12));
            col = dim565(col, (uint16_t)(256 - band * 110));
            if (flash) col = mix565(col, RGB(255, 255, 255), flash);
            buf[band * 64 + c] = dim565(col, k);
        }
    buf[0] = 0;
    pal_run(1, buf + 1, 127);
}

/* ---- the vortex's palette: 191 colours from a precomputed ramp, turning (flash: a brighter ramp) */
static uint8_t f_vmap[191];                     /* entry -> ramp phase (i x 8/3) */
COLD static void f_vortex_pal(uint16_t t, uint8_t set, uint16_t k, uint16_t flash)
{
    const uint16_t *ramp = (const uint16_t *)(F_RAMP + (uint32_t)((set & 3) * 3 + (flash > 160 ? 2 : flash > 60 ? 1 : 0)) * 512);
    uint8_t off = (uint8_t)(t * 3), i;
    if (!f_vmap[1])
        for (i = 0; i < 191; i++) f_vmap[i] = (uint8_t)(i * 8 / 3);
    IO(0x210) = 1;
    if (k == 256)
        for (i = 0; i < 191; i++) IO(0x212) = ramp[(uint8_t)(f_vmap[i] + off)];
    else
        for (i = 0; i < 191; i++) IO(0x212) = dim565(ramp[(uint8_t)(f_vmap[i] + off)], k);
}

/* ---- the cube: affine texture mapping, two pixels a texel */
/* a textured span, two pixels a texel; in its own function so its loop keeps everything in registers (inside
 * f_textri the compiler spilled to the stack every texel, and each spill is an SDRAM write on the Tang) */
COLD static void f_span(volatile uint16_t *pw, int n, uint16_t u, uint16_t v, int16_t du, int16_t dv, uint8_t bank)
{
    /* byte stores, not c * 257 into a word (a multiply is about 8 cycles on T1, a store about 1); four texels a
     * pass to share the loop's overhead */
    const uint8_t *tex = (const uint8_t *)F_CTEX;
    volatile uint8_t *p = (volatile uint8_t *)pw;
    uint8_t c;
#define TX() c = (uint8_t)(tex[((v >> 3) & 0x0FC0) | (u >> 9)] + bank); p[0] = c; p[1] = c; p += 2; \
             u = (uint16_t)(u + du); v = (uint16_t)(v + dv)
    while (n >= 4) {
        TX(); TX(); TX(); TX();
        n -= 4;
    }
    while (n--) {
        TX();
    }
#undef TX
}
static int16_t f_du2, f_dv2;                    /* the triangle's texture step per two pixels */
COLD static void f_textri(uint32_t buf, int32_t *a, int32_t *b, int32_t *c, uint8_t bank)
{
    /* a, b, c: x, y, u, v (u, v in 64ths of the texture, 16.16 later) */
    int32_t *t;
    int y, y0, y1, y2;
    int32_t xa, xb, ua, ub, va, vb, dxa, dxb, dua, dub, dva, dvb;
    if (a[1] > b[1]) { t = a; a = b; b = t; }
    if (a[1] > c[1]) { t = a; a = c; c = t; }
    if (b[1] > c[1]) { t = b; b = c; c = t; }
    y0 = (int)a[1]; y1 = (int)b[1]; y2 = (int)c[1];
    if (y2 == y0) return;
    /* the texture's gradient across x is the same over the whole triangle (affine): two divisions here, none a
     * span; in 7.9 fixed point, per two pixels */
    {
        int32_t den = (c[0] - a[0]) * (b[1] - a[1]) - (b[0] - a[0]) * (c[1] - a[1]);
        if (den == 0) return;
        f_du2 = (int16_t)(((((c[2] - a[2]) * (b[1] - a[1]) - (b[2] - a[2]) * (c[1] - a[1])) << 9) / den) * 2);
        f_dv2 = (int16_t)(((((c[3] - a[3]) * (b[1] - a[1]) - (b[3] - a[3]) * (c[1] - a[1])) << 9) / den) * 2);
    }
    dxb = ((c[0] - a[0]) << 16) / (y2 - y0); dub = ((c[2] - a[2]) << 16) / (y2 - y0); dvb = ((c[3] - a[3]) << 16) / (y2 - y0);
    xb = a[0] << 16; ub = a[2] << 16; vb = a[3] << 16;
    for (y = y0; y < y2; y++) {
        if (y == y0 || y == y1) {
            int32_t *s = y < y1 ? a : b, *e = y < y1 ? b : c;
            int ys = (int)s[1], ye = (int)e[1];
            if (ye == ys) continue;
            dxa = ((e[0] - s[0]) << 16) / (ye - ys); dua = ((e[2] - s[2]) << 16) / (ye - ys); dva = ((e[3] - s[3]) << 16) / (ye - ys);
            xa = s[0] << 16; ua = s[2] << 16; va = s[3] << 16;
        }
        if ((unsigned)y < H && !(y & 1)) {     /* even rows only: the blitter doubles them (f_double) */
            int32_t xl = xa, xr = xb, ul = ua, ur = ub, vl = va, vr = vb;
            if (xl > xr) { int32_t q; q = xl; xl = xr; xr = q; q = ul; ul = ur; ur = q; q = vl; vl = vr; vr = q; }
            {
                int x0 = (int)(xl >> 16) & ~1, x1 = (int)(xr >> 16);
                int n = (x1 - x0) >> 1;
                if (n > 0 && x0 >= 0 && x1 < W)
                    /* 16-bit fixed point, 9 fraction bits: the texture is 64 x 64 */
                    f_span((volatile uint16_t *)(buf + (uint32_t)y * W + (uint32_t)x0), n, (uint16_t)(ul >> 7), (uint16_t)(vl >> 7),
                           f_du2, f_dv2, bank);
                (void)ur; (void)vr;
            }
        }
        xa += dxa; ua += dua; va += dva;
        xb += dxb; ub += dub; vb += dvb;
    }
    poll();
}
/* copy the box's even rows onto the odd rows below them: one blit, both strides two rows */
static void f_double(uint32_t buf, int16_t *bb)
{
    int y0 = bb[1] & ~1, y1 = bb[3] + 1, x0 = bb[0] < 0 ? 0 : bb[0], x1 = bb[2] + 2;
    if (y0 < 0) y0 = 0;
    if (y1 > H - 1) y1 = H - 1;
    if (x1 > W) x1 = W;
    if (y1 <= y0 || x1 <= x0) return;
    blt_copy(buf + (uint32_t)y0 * W + (uint32_t)x0, 2 * W, buf + (uint32_t)(y0 + 1) * W + (uint32_t)x0, 2 * W,
             (uint16_t)(x1 - x0), (uint16_t)((y1 - y0 + 1) / 2), 0, 0);
}
static const int8_t cube_v[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
static const uint8_t cube_f[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {4, 5, 1, 0}, {3, 2, 6, 7}};
COLD static void f_cube(uint32_t buf, uint16_t f, int cx, int cy, int size, int16_t *bb)
{
    int32_t P[8][2], Z[8], RX[8], RY[8];
    uint8_t i, ax = (uint8_t)f, ay = (uint8_t)(f * 3 / 2), az = (uint8_t)(f / 2);
    int16_t sa = SIN16(ax), ca = COS16(ax), sb = SIN16(ay), cb = COS16(ay), sc = SIN16(az), cc = COS16(az);
    bb[0] = 640; bb[1] = 360; bb[2] = 0; bb[3] = 0;
    for (i = 0; i < 8; i++) {
        int32_t x = cube_v[i][0] * size, y = cube_v[i][1] * size, z = cube_v[i][2] * size;
        int32_t y1 = (y * ca - z * sa) >> 14, z1 = (y * sa + z * ca) >> 14;
        int32_t x2 = (x * cb + z1 * sb) >> 14, z2 = (-x * sb + z1 * cb) >> 14;
        int32_t x3 = (x2 * cc - y1 * sc) >> 14, y3 = (x2 * sc + y1 * cc) >> 14;
        int32_t d = 500 + z2;
        RX[i] = x3; RY[i] = y3; Z[i] = z2;
        P[i][0] = cx + x3 * 400 / d;
        P[i][1] = cy + y3 * 400 / d;
        if (P[i][0] < bb[0]) bb[0] = (int16_t)P[i][0];
        if (P[i][0] > bb[2]) bb[2] = (int16_t)P[i][0];
        if (P[i][1] < bb[1]) bb[1] = (int16_t)P[i][1];
        if (P[i][1] > bb[3]) bb[3] = (int16_t)P[i][1];
    }
    for (i = 0; i < 6; i++) {
        const uint8_t *q = cube_f[i];
        int32_t ex = P[q[1]][0] - P[q[0]][0], ey = P[q[1]][1] - P[q[0]][1];
        int32_t gx = P[q[3]][0] - P[q[0]][0], gy = P[q[3]][1] - P[q[0]][1];
        if (ex * gy - ey * gx <= 0) continue;
        {
            int32_t nz = Z[q[0]] + Z[q[1]] + Z[q[2]] + Z[q[3]], ny = RY[q[0]] + RY[q[1]] + RY[q[2]] + RY[q[3]];
            int32_t l = -nz - ny / 2;
            uint8_t bank = (uint8_t)(F_CUBE + (l > size * 2 ? 32 : l > 0 ? 16 : 0));
            int32_t A[4] = {P[q[0]][0], P[q[0]][1], 0, 0}, B[4] = {P[q[1]][0], P[q[1]][1], 63, 0};
            int32_t C[4] = {P[q[2]][0], P[q[2]][1], 63, 63}, D[4] = {P[q[3]][0], P[q[3]][1], 0, 63};
            int32_t A2[4] = {A[0], A[1], A[2], A[3]}, C2[4] = {C[0], C[1], C[2], C[3]};
            f_textri(buf, A, B, C, bank);
            f_textri(buf, A2, C2, D, bank);
        }
    }
    (void)RX;
}
COLD static void f_cube_pal(uint16_t k, uint16_t flash)
{
    static const uint16_t base[16] = {0, 0, RGB(30, 60, 160), 0, RGB(20, 30, 90), 0, 0, 0, 0, 0,
                                      RGB(255, 240, 120), RGB(255, 200, 80), RGB(255, 150, 60), RGB(255, 110, 40), 0, RGB(200, 220, 255)};
    uint8_t b, i;
    for (b = 0; b < 3; b++)
        for (i = 0; i < 16; i++) {
            uint16_t c = dim565(base[i], (uint16_t)(130 + b * 63));
            if (flash) c = mix565(c, RGB(255, 255, 255), flash);
            pal565((uint8_t)(F_CUBE + b * 16 + i), dim565(c, k));
        }
}

/* ---- the big T1 on the sprite buffer layer (4 bpp: 7 white, 0 black edge, 8 transparent) */
COLD static void f_cell(int x0, int y0, uint16_t w, uint16_t h, uint8_t v)
{
    blt_fill(SBUF + (uint32_t)y0 * 320 + (uint32_t)(x0 >> 1), 320, w, h, v);
}
COLD static void f_logo(void)
{
    int n, r, b, pass;
    f_sbuf = ((uint32_t)_end + 0xFFFUL) & ~0xFFFUL;
    if (f_sbuf + 115200UL > 0x40000UL - 0x1800UL) {    /* no room: no logo (6 KB kept for the stack) */
        f_sbuf = 0;
        return;
    }
    blt_fill(SBUF, 320, 320, 360, 0x88);
    for (pass = 0; pass < 2; pass++)            /* the black edge, then the letters */
        for (n = 0; n < 2; n++) {
            const uint8_t *g = font8x14[n ? '1' : 'T'];
            for (r = 0; r < 14; r++) {
                uint8_t bits = g[r];
                for (b = 0; b < 8; b++, bits = (uint8_t)(bits << 1))
                    if (bits & 0x80) {
                        int x0 = 176 + n * 160 + b * 18, y0 = 60 + r * 17;
                        if (pass == 0) f_cell(x0 - 2, y0 - 2, 11, 21, 0x00);
                        else f_cell(x0, y0, 8, 15, (uint8_t)(r < 7 ? 0x77 : 0x66));
                    }
            }
            poll();
        }
    blt_wait();
    IO(0x20C) = (uint16_t)SBUF; IO(0x20E) = (uint16_t)(SBUF >> 16);
}

/* ---- the spheres: 64 sprites, rotated, sorted near first (a lower number is in front) */
COLD static void f_spheres(uint16_t f, int cx, int cy, int r)
{
    static int16_t zs[64];
    static uint8_t ord[64];
    static int16_t sx[64], sy[64];
    uint8_t i, j, a = (uint8_t)f, b = (uint8_t)(f * 2 / 3);
    int16_t sa = SIN16(a), ca = COS16(a), sb = SIN16(b), cb = COS16(b);
    for (i = 0; i < 64; i++) {
        int32_t x = sph64[i][0], y = sph64[i][1], z = sph64[i][2];
        int32_t x1 = (x * ca + z * sa) >> 14, z1 = (-x * sa + z * ca) >> 14;
        int32_t y2 = (y * cb - z1 * sb) >> 14, z2 = (y * sb + z1 * cb) >> 14;
        int32_t d = 400 + z2;
        sx[i] = (int16_t)(cx + x1 * r / 120 * 400 / d - 16);
        sy[i] = (int16_t)(cy + y2 * r / 120 * 400 / d - 16);
        zs[i] = (int16_t)z2;
        ord[i] = i;
    }
    for (i = 1; i < 64; i++) {                  /* insertion sort by depth, near first */
        uint8_t o = ord[i];
        j = i;
        while (j > 0 && zs[ord[j - 1]] > zs[o]) { ord[j] = ord[j - 1]; j--; }
        ord[j] = o;
    }
    for (i = 0; i < 64; i++) spr_xy(i, sx[ord[i]], sy[ord[i]], 32);
}

static void f_text(int row, const char *s, uint8_t bg)
{
    int n = strlen16(s);
    tstr((80 - n) / 2, row, s, TA(7, bg) | TBLEND);
}

static void sec_finale(void)
{
    uint32_t t0, tend = 0;
    uint16_t f = 0, flash = 0;
    uint8_t i, scene = 0xFF, lastrow = 0xFF, dv = 0, du = 0;
    screen_reset();
    f_pfa[0] = R0; f_pfa[1] = R1; f_vortex = R2;
    f_cur = 0;
    for (i = 0; i < 2; i++) { f_bb[i][0] = 0; f_bb[i][1] = 0; f_bb[i][2] = 0; f_bb[i][3] = 0; }
    vid(VC_TEXT);
    f_text(10, " 1980S: THAT WAS THEN. ", 1);
    f_tables();                                 /* (a second or so of CPU) */
    f_logo();
    for (i = 0; i < 16; i++) pal565((uint8_t)(F_SPH + i), mix565(RGB(255, 255, 255), RGB(60, 120, 255), (uint16_t)(i * 17)));
    clear8(f_pfa[0], 1); clear8(f_pfa[1], 1);
    f_text(12, " THIS IS NOW: T1 ON A TANG NANO 20K ", 4);
    mod_play(mod_finale);
    pl.sep = 100;
    t0 = ticks50;
    while (1) {
        uint32_t t = ticks50 - t0;
        uint8_t pos = pl.ppos, row = pl.prow;
        uint8_t ns = pos == 0 ? 0 : pos <= 2 ? 1 : pos <= 6 ? 2 : pos <= 9 ? 3 : 4;
        uint8_t beat = 0;
        uint32_t back = f_cur ? f_pfa[0] : f_pfa[1];
        int16_t *bb = f_cur ? f_bb[0] : f_bb[1];
        f = (uint16_t)(t * 6 / 5);
        if (row != lastrow) {                   /* the beat: kicks on rows 0 and 10 of a bar (and every 4th row early on) */
            uint8_t r16 = (uint8_t)(row & 15);
            if (ns >= 2 ? (r16 == 0 || r16 == 10) : (r16 & 3) == 0) beat = 1;
            if (ns == 4 && (row & 7) == 0 && row <= 32) beat = 2;
            lastrow = row;
        }
        if (beat) flash = beat == 2 ? 230 : 110;
        else if (flash > 24) flash = (uint16_t)(flash - 24);
        else flash = 0;

        if (ns != scene) {                      /* scene changes, on the song's patterns */
            scene = ns;
#ifdef DEBUG
            IO(0) = (uint8_t)('0' + ns); IO(0) = (uint8_t)('a' + pos); IO(0) = ' ';
#endif
            tclear();
            if (ns == 1) {
                vid(VC_BMP | VC_HI | VC_TEXT);
                vid2(VC2_SPR);
                for (i = 0; i < 48; i++) spr_set(i, 0, -32, 8, F_STAR, 0, 0);
                f_text(22, " FLYING THROUGH THE LOGO: A TEXTURE-MAPPED TUNNEL, EVERY TEXEL BY THE CPU ", 1);
            } else if (ns == 2) {
                spr_alloff();
                clear8(f_pfa[0], 0); clear8(f_pfa[1], 0);
                f_bb[0][2] = f_bb[1][2] = 0;
                set_pfb(f_vortex);
                {
                    uint16_t y;
                    for (y = 0; y < 360; y++) {
                        volatile uint16_t *e = (volatile uint16_t *)(LSCR + (uint32_t)y * 4);
                        e[0] = 0; e[1] = 0;
                    }
                }
                set_lscr(LSCR);
                vid(VC_BMP | VC_HI | VC_TEXT);
#ifndef XPFB
#define XPFB VC2_PFB
#endif
#ifndef XSPR
#define XSPR VC2_SPR
#endif
                vid2(XPFB | VC2_LSCR | XSPR);
                for (i = 0; i < 64; i++) spr_set(i, 0, -32, 32, F_BALL, 2, 0);
                f_text(1, " 640 X 360 - 256 OF 65,536 COLOURS - TWO PLAYFIELDS - LINE SCROLL ", 1);
                f_text(22, " 64 HARDWARE SPRITES - A BLITTER - A TEXTURE-MAPPED CUBE BY THE CPU ", 5);
            } else if (ns == 3) {
                sidfx_start();
                flood(21);
                f_text(1, " 32 VOICES: 8 SAMPLED CHANNELS + 24 SID VOICES - 48 KHZ STEREO OVER HDMI ", 4);
                f_text(22, " 255 BPM. THE T1 IS STILL KEEPING UP. ", 2);
            } else if (ns == 4) {
                sidfx_stop();
                f_text(11, " T1 - TANG NANO 20K - 2026 ", 1);
                tend = t;
            }
        }

        if (scene <= 1) {
            /* the fly-through: speed with the tempo */
            uint16_t k = scene == 0 ? (uint16_t)(t < 300 ? t * 256 / 300 : 256) : 256;
            dv = (uint8_t)(dv + (scene == 0 ? 1 : 2 + (beat ? 2 : 0)));
            du = (uint8_t)(du + (SIN(f) > 0 ? 1 : 0));
            f_tunnel(back, du, dv);
            f_tunnel_pal(f, k, scene == 1 ? flash : 0);
            if (scene == 1) {
                for (i = 0; i < 48; i++) {      /* stars: outwards from the centre, faster on the beat */
                    uint8_t a = (uint8_t)(i * 53 + (i >> 3));
                    uint16_t rr = (uint16_t)(((f * (3 + (i & 3))) + i * 37) % 260);
                    spr_xy(i, 316 + COS(a) * rr / 100, 176 + SIN(a) * rr / 180, 8);
                }
                vid((uint16_t)(VC_BMP | VC_HI | VC_TEXT | (flash > 100 && f_sbuf ? 0x08 : 0)));   /* the T1 strobes */
            }
            blt_wait();
            set_pfa(back);
            if (scene == 0 && t < 2) vid(VC_BMP | VC_HI | VC_TEXT);
        } else {
            /* the vortex (palette), its wobble (line scroll), the cube (CPU), the spheres (sprites) */
            int cx = 320 + SIN(f) * (scene == 3 ? 120 : 60) / 128, cy = 180 + SIN(f * 2) * 30 / 128;
            int size = scene == 3 ? 95 + (flash >> 3) : 80 + (flash >> 4);
            uint16_t k = scene == 4 ? (uint16_t)(t - tend < 150 ? 256 : t - tend > 250 ? 0 : (250 - (t - tend)) * 256 / 100) : 256;
            if (bb[2] > bb[0]) rect8(back, bb[0], bb[1], bb[2] - bb[0] + 2, bb[3] - bb[1] + 2, 0);
            blt_wait();
#ifndef NOCUBE
#ifdef FPS
            {
                static uint32_t cyc_sum; static uint16_t cyc_n;
                uint16_t h0 = IO(0x6), l0 = IO(0x4), h1, l1;
                f_cube(back, (uint16_t)(f * (scene == 3 ? 2 : 1)), cx, cy, size, bb);
                f_double(back, bb);
                h1 = IO(0x6); l1 = IO(0x4);
                cyc_sum += ((uint32_t)h1 << 16 | l1) - ((uint32_t)h0 << 16 | l0);
                if (++cyc_n == 16) {            /* "c nnnnn": f_cube's T1 cycles a frame, in thousands */
                    uint16_t k = (uint16_t)(cyc_sum / 16000);
                    IO(0) = 'c'; IO(0) = ' ';
                    IO(0) = (uint8_t)('0' + k / 1000 % 10); IO(0) = (uint8_t)('0' + k / 100 % 10);
                    IO(0) = (uint8_t)('0' + k / 10 % 10); IO(0) = (uint8_t)('0' + k % 10); IO(0) = '\n';
                    cyc_sum = 0; cyc_n = 0;
                }
            }
#else
            f_cube(back, (uint16_t)(f * (scene == 3 ? 2 : 1)), cx, cy, size, bb);
            f_double(back, bb);
#endif
#endif
            f_spheres((uint16_t)(f * (scene == 3 ? 3 : 2)), 320 - SIN(f) * 60 / 128, 180, scene == 3 ? 165 : 140);
            f_vortex_pal((uint16_t)(f * (scene == 3 ? 3 : 1)), (uint8_t)(pos >> 1), k, flash);
            f_cube_pal(k, flash);
            {
                uint16_t y, amp = (uint16_t)(6 + (flash >> 3) + (scene == 3 ? 10 : 0));
                for (y = 0; y < 360; y += 2) {
                    volatile uint16_t *e = (volatile uint16_t *)(LSCR + (uint32_t)y * 4);
                    uint16_t o = (uint16_t)((640 + SIN(y * 3 + f * 4) * amp / 128) % 640);
                    e[1] = o; e[3] = o;
                }
            }
            vid((uint16_t)(VC_BMP | VC_HI | VC_TEXT | ((scene == 3 && flash > 150 && f_sbuf) ? 0x08 : 0)));
            if (scene == 4 && row >= 32 && !w_on) {
                static const uint8_t em[5] = {52, 55, 59, 64, 67};
                saw_chord(em, 5);
            }
            set_pfa(back);
            if (scene == 4 && t - tend > 260) break;
        }
        f_cur ^= 1;
        wait_flip();
        fps_count((char)('0' + scene));
        if (t > 4000) break;                    /* (a guard) */
    }
    saw_release();
    mod_stop();
    sidfx_stop();
    spr_alloff();
    tclear();
    vid(0);
    vid2(0);
    {
        uint8_t k;
        for (k = 0; k < 60; k++) vsync();
    }
    w_on = 0;
}
