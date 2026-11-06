/* The Future: drawing at a resolution. A canvas is a window of an 8 bpp buffer whose pixels are b x b screen
 * pixels: the early eras draw at 128 x 72 or 160 x 90 on the same 640 x 360 screen, and a "pixel" is a
 * blitter fill. Shapes are spans, so a filled shape costs a blit a row whatever the block size. */

static uint32_t cv_buf;
static uint8_t cv_b = 1;
static int16_t cv_ox, cv_oy, cv_w, cv_h;        /* the window on the screen; its size in canvas pixels */

static uint16_t cv_inv;                         /* 65536 / b */
static void cv_set(uint32_t buf, uint8_t b, int ox, int oy, int w, int h)
{
    cv_buf = buf; cv_b = b; cv_ox = (int16_t)ox; cv_oy = (int16_t)oy; cv_w = (int16_t)w; cv_h = (int16_t)h;
    cv_inv = (uint16_t)(b == 1 ? 0 : 65536UL / b + 1);
}
/* the whole screen at block size b (the last column or row of a size that does not divide is left alone) */
static void cv_screen(uint32_t buf, uint8_t b) { cv_set(buf, b, 0, 0, W / b, H / b); }
static void cv_rect(int x, int y, int w, int h, uint8_t c)
{
    uint8_t b = cv_b;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > cv_w) w = cv_w - x;
    if (y + h > cv_h) h = cv_h - y;
    if (w <= 0 || h <= 0) return;
    blt_fill(cv_buf + (uint32_t)(cv_oy + y * b) * W + (uint32_t)(cv_ox + x * b), W, (uint16_t)(w * b), (uint16_t)(h * b), c);
}
static void cv_clear(uint8_t c) { cv_rect(0, 0, cv_w, cv_h, c); }
static void cv_pset(int x, int y, uint8_t c)
{
    if ((unsigned)x >= (unsigned)cv_w || (unsigned)y >= (unsigned)cv_h) return;
    if (cv_b == 1) *(volatile uint8_t *)(cv_buf + (uint32_t)(cv_oy + y) * W + (uint32_t)(cv_ox + x)) = c;
    else cv_rect(x, y, 1, 1, c);
}
static void cv_span(int x0, int x1, int y, uint8_t c)   /* x0..x1 inclusive */
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if ((unsigned)y >= (unsigned)cv_h || x1 < 0 || x0 >= cv_w) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= cv_w) x1 = cv_w - 1;
    if (cv_b == 1 && x1 - x0 < 48) { blt_wait(); hspan8(cv_buf, cv_ox + x0, cv_ox + x1, cv_oy + y, c); }
    else cv_rect(x0, y, x1 - x0 + 1, 1, c);
}
COLD static void cv_line(int x0, int y0, int x1, int y1, uint8_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    if (cv_b == 1) blt_wait();
    for (;;) {
        cv_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
    }
}
/* a flat triangle by edge stepping (16.16) */
COLD static void cv_tri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    int t, y;
    int32_t xa, xb, da, db;
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y0 > y2) { t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
    if (y1 > y2) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y2 == y0 || y2 < 0 || y0 >= cv_h) return;
    db = ((int32_t)(x2 - x0) << 16) / (y2 - y0);
    xb = ((int32_t)x0 << 16) + 0x8000;
    if (y1 > y0) {
        da = ((int32_t)(x1 - x0) << 16) / (y1 - y0);
        xa = ((int32_t)x0 << 16) + 0x8000;
        for (y = y0; y < y1; y++) {
            if (y >= 0) cv_span((int)(xa >> 16), (int)(xb >> 16), y, c);
            xa += da; xb += db;
        }
    }
    if (y2 > y1) {
        da = ((int32_t)(x2 - x1) << 16) / (y2 - y1);
        xa = ((int32_t)x1 << 16) + 0x8000;
        for (y = y1; y < y2 && y < cv_h; y++) {
            if (y >= 0) cv_span((int)(xa >> 16), (int)(xb >> 16), y, c);
            xa += da; xb += db;
        }
    }
}
static void cv_quad(const int16_t *p, uint8_t c)  /* x0 y0 .. x3 y3, convex */
{
    cv_tri(p[0], p[1], p[2], p[3], p[4], p[5], c);
    cv_tri(p[0], p[1], p[4], p[5], p[6], p[7], c);
}
static uint16_t isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1UL << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return (uint16_t)r;
}
COLD static void cv_ellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
    int y, y0 = -ry, y1 = ry;
    if (ry <= 0 || rx <= 0) return;
    if (y0 < -cy) y0 = -cy;                     /* only the rows on the canvas */
    if (y1 > cv_h - 1 - cy) y1 = cv_h - 1 - cy;
    for (y = y0; y <= y1; y++) {
        int w = (int)((int32_t)rx * isqrt32((uint32_t)((int32_t)ry * ry - (int32_t)y * y) << 8) / ((int32_t)ry << 4));
        cv_span(cx - w, cx + w, cy + y, c);
    }
}
static void cv_disc(int cx, int cy, int r, uint8_t c) { cv_ellipse(cx, cy, r, r, c); }

/* text: the 3 x 5 font (font35, scale s) and the 8 x 14 one; runs of set pixels are rectangles */
static int cv_text35(int x, int y, const char *str, uint8_t c, int s)
{
    for (; *str; str++, x += 4 * s) {
        uint16_t g = font35[((uint8_t)*str - 32) & 127];
        int r, k;
        for (r = 0; r < 5; r++) {
            uint8_t bits = (uint8_t)((g >> (12 - 3 * r)) & 7);
            for (k = 0; k < 3; k++)
                if (bits & (4 >> k)) {
                    int n = 1;
                    while (k + n < 3 && (bits & (4 >> (k + n)))) n++;
                    cv_rect(x + k * s, y + r * s, n * s, s, c);
                    k += n;
                }
        }
    }
    return x;
}
static int text35_w(const char *s, int sc) { return (strlen16(s) * 4 - 1) * sc; }
COLD static int cv_text8(int x, int y, const char *str, uint8_t c, int s)
{
    for (; *str; str++, x += 8 * s) {
        const uint8_t *g = font8x14[(uint8_t)*str];
        int r, k;
        for (r = 0; r < 14; r++) {
            uint8_t bits = g[r];
            for (k = 0; k < 8; k++)
                if (bits & (0x80 >> k)) {
                    int n = 1;
                    while (k + n < 8 && (bits & (0x80 >> (k + n)))) n++;
                    cv_rect(x + k * s, y + r * s, n * s, s, c);
                    k += n;
                }
        }
        poll();
    }
    return x;
}
static int text8_w(const char *s, int sc) { return strlen16(s) * 8 * sc; }

/* pixel art: rows of characters, '.' clear, '1'-'9' and 'a'-'z' colour base + n */
COLD static void cv_art(int x, int y, const char *const *rows, int nr, uint8_t base, uint8_t flip)
{
    int r;
    for (r = 0; r < nr; r++) {
        const char *s = rows[r];
        int n = strlen16(s), k = 0;
        while (k < n) {
            char ch = s[flip ? n - 1 - k : k];
            int run = 1;
            if (ch == '.') { k++; continue; }
            while (k + run < n && s[flip ? n - 1 - k - run : k + run] == ch) run++;
            cv_rect(x + k, y + r, run, 1, (uint8_t)(base + (ch <= '9' ? ch - '0' : ch - 'a' + 10)));
            k += run;
        }
    }
}

/* ---------------------------------------------------------------- double buffering of playfield A */
static uint32_t fb[2] = {R0, R1};
static uint8_t fb_cur;                          /* the buffer being drawn */
static uint32_t fb_back(void) { return fb[fb_cur]; }
#ifdef FPS
static uint16_t fps_n, fps_f0;
#endif
static void fb_flip(void)
{
#ifdef FPS
    uint16_t f = FRAME;                         /* -DFPS: frames a second on the console every 2 s */
    fps_n++;
    if ((uint16_t)(f - fps_f0) >= 120) {
        uint16_t r = (uint16_t)(fps_n / 2), ph = (uint16_t)(q_cur >> 7);
        IO(0) = 'p'; IO(0) = (uint8_t)('0' + ph / 10); IO(0) = (uint8_t)('0' + ph % 10); IO(0) = ' ';
        IO(0) = (uint8_t)('0' + r / 10); IO(0) = (uint8_t)('0' + r % 10); IO(0) = '\n';
        fps_n = 0; fps_f0 = f;
    }
#endif
    blt_wait();
    set_pfa(fb[fb_cur]);
    fb_cur ^= 1;
    wait_flip();
}

/* time: the song position (16ths x 256) and helpers; a phrase is 128 16ths, a bar 16, a beat 4 */
#define PHRASE(p) ((uint32_t)(p) << 15)
static uint8_t cur_phrase(void) { return (uint8_t)(q_cur >> 7); }
static uint16_t since_kick(void)                /* 16ths x 256 since the last kick (flashes) */
{
    uint32_t m = mpos(), k = (uint32_t)kick_step << 8;
    return kick_step == 0xFFFF || m < k ? 0xFFFF : (uint16_t)(m - k > 0xFFFF ? 0xFFFF : m - k);
}
static uint16_t ramp16(uint32_t now, uint32_t t0, uint32_t len)   /* 0..256 over [t0, t0+len] */
{
    if (now <= t0) return 0;
    if (now >= t0 + len) return 256;
    return (uint16_t)(((now - t0) << 8) / len);
}

/* ---------------------------------------------------------------- world coordinates (640 x 360) */
/* the scenes draw in screen units and the canvas rounds them to its pixels, so a shape keeps its size
 * while the pixels get finer from era to era */
static int wd(int v) { return cv_b == 1 ? v : (int)(((int32_t)v * cv_inv) >> 16); }
static void wrect(int x, int y, int w, int h, uint8_t c)
{
    int x0 = wd(x), y0 = wd(y), x1 = wd(x + w), y1 = wd(y + h);
    cv_rect(x0, y0, x1 > x0 ? x1 - x0 : 1, y1 > y0 ? y1 - y0 : 1, c);
}
static void wpset(int x, int y, uint8_t c) { cv_pset(wd(x), wd(y), c); }
static void wtri(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t c)
{
    cv_tri(wd(x0), wd(y0), wd(x1), wd(y1), wd(x2), wd(y2), c);
}
static void wline(int x0, int y0, int x1, int y1, uint8_t c) { cv_line(wd(x0), wd(y0), wd(x1), wd(y1), c); }
static void wellipse(int cx, int cy, int rx, int ry, uint8_t c)
{
    int a = wd(rx), b = wd(ry);
    cv_ellipse(wd(cx), wd(cy), a ? a : 1, b ? b : 1, c);
}
static void wdisc(int cx, int cy, int r, uint8_t c) { wellipse(cx, cy, r, r, c); }
static int wtext35(int x, int y, const char *s, uint8_t c, int px)   /* px: the font's pixel in screen units */
{
    int sc = wd(px);
    return cv_text35(wd(x), wd(y), s, c, sc ? sc : 1) * cv_b;
}
/* a polygon (a fan from the first point) turned by angle a (0-255) and scaled by k/64, at (x, y) */
static void wshape(const int8_t *pt, uint8_t n, int x, int y, uint8_t a, int k, uint8_t c)
{
    int16_t px[12], py[12];
    int32_t ca = (int32_t)COS(a) * k, sa = (int32_t)SIN(a) * k;
    uint8_t i;
    for (i = 0; i < n; i++) {
        int32_t u = pt[2 * i], v = pt[2 * i + 1];
        px[i] = (int16_t)(x + ((u * ca - v * sa) >> 13));
        py[i] = (int16_t)(y + ((u * sa + v * ca) >> 13));
    }
    for (i = 2; i < n; i++) wtri(px[0], py[0], px[i - 1], py[i - 1], px[i], py[i], c);
}
