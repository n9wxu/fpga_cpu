/* Overdrive: the dancer. A figure of thick limbs (quads) and a round head, posed on every beat: the pose
 * snaps in over the first 16th and holds with a bounce. Drawn at 2 x 2 pixels by blitter spans into
 * playfield B (two buffers, R0 and R1), over the plasma. Angles are 0-255, 0 pointing down, 64 right. */

/* torso, left upper arm, left forearm, right upper, right forearm, left thigh, left shin, right thigh,
 * right shin, the hip's height (+ down) */
typedef struct { uint8_t a[9]; int8_t dy; } pose_t;
static const pose_t poses[] = {
    {{128, 248, 252, 8, 4, 250, 0, 6, 0}, 0},                   /* 0 standing */
    {{128, 160, 150, 96, 106, 240, 246, 16, 10}, 0},            /* 1 arms up in a V */
    {{128, 192, 196, 64, 60, 220, 250, 36, 6}, 34},             /* 2 crouched, arms out */
    {{132, 220, 40, 120, 128, 250, 0, 20, 244}, 0},             /* 3 the right arm punches up */
    {{132, 164, 156, 92, 100, 210, 250, 46, 6}, -30},           /* 4 a jump, arms up in a V */
    {{136, 230, 220, 100, 90, 244, 0, 20, 8}, 6},               /* 5 the disco point */
    {{120, 176, 64, 80, 192, 234, 10, 22, 246}, 10},            /* 6 arms crossed behind the head */
    {{128, 200, 150, 56, 106, 236, 30, 20, 226}, 18},           /* 7 a kick-step */
};
#define N_POSE 8
/* the routine: a pose a beat, 16 beats; odd entries + 8 are mirrored */
static const uint8_t routine[2][16] = {
    {0, 1, 2, 1, 3, 11, 4, 2, 5, 13, 6, 2, 7, 15, 4, 1},
    {2, 4, 2, 4, 3, 11, 3, 11, 5, 13, 5, 13, 6, 7, 15, 4},
};
static void pose_get(uint8_t id, pose_t *o)
{
    const pose_t *p = &poses[id & 7];
    uint8_t i;
    if (!(id & 8)) { *o = *p; return; }
    o->dy = p->dy;                              /* the mirror: angles negated, left and right swapped */
    o->a[0] = (uint8_t)(0 - p->a[0]);
    for (i = 0; i < 2; i++) {
        o->a[1 + i] = (uint8_t)(0 - p->a[3 + i]); o->a[3 + i] = (uint8_t)(0 - p->a[1 + i]);
        o->a[5 + i] = (uint8_t)(0 - p->a[7 + i]); o->a[7 + i] = (uint8_t)(0 - p->a[5 + i]);
    }
}
static uint8_t lerp_a(uint8_t a, uint8_t b, uint16_t k) { return (uint8_t)(a + (int16_t)(((int16_t)(int8_t)(b - a) * (int16_t)k) >> 8)); }

static int16_t dq[8];
static void limb2(int x0, int y0, uint8_t a, int len, int w0, int w1, int *x1, int *y1, uint8_t c)
{                                               /* half widths w0 at the start, w1 at the end */
    int ex = x0 + (SIN(a) * len >> 7), ey = y0 + (COS(a) * len >> 7);
    int nx = COS(a), ny = -SIN(a);
    dq[0] = (int16_t)((x0 + (nx * w0 >> 7)) >> 1); dq[1] = (int16_t)((y0 + (ny * w0 >> 7)) >> 1);
    dq[2] = (int16_t)((ex + (nx * w1 >> 7)) >> 1); dq[3] = (int16_t)((ey + (ny * w1 >> 7)) >> 1);
    dq[4] = (int16_t)((ex - (nx * w1 >> 7)) >> 1); dq[5] = (int16_t)((ey - (ny * w1 >> 7)) >> 1);
    dq[6] = (int16_t)((x0 - (nx * w0 >> 7)) >> 1); dq[7] = (int16_t)((y0 - (ny * w0 >> 7)) >> 1);
    cv_quad(dq, c);
    *x1 = ex; *y1 = ey;
}
static void limb(int x0, int y0, uint8_t a, int len, int w, int *x1, int *y1, uint8_t c) { limb2(x0, y0, a, len, w, w, x1, y1, c); }
/* a dancer at (x, hip y), size k/64 */
static int16_t d_box[4];                        /* the drawn extent: x0 y0 x1 y1 (canvas pixels) */
static void dancer(const pose_t *p, int x, int y, int k, uint8_t c)
{
    int sx, sy, ex, ey, hx, hy;
    int L_t = 96 * k >> 6, L_u = 56 * k >> 6, L_l = 52 * k >> 6, L_th = 74 * k >> 6, L_sh = 72 * k >> 6;
    int w_t = 22 * k >> 6, w_a = 9 * k >> 6, w_l = 12 * k >> 6;
    y += p->dy * k >> 6;
    limb2(x, y, p->a[0], L_t, w_t * 2 / 3, w_t, &sx, &sy, c); /* the torso: narrow hips, wide shoulders */
    limb(x - (w_t >> 1), y, p->a[5], L_th, w_l, &ex, &ey, c);
    limb(ex, ey, p->a[6], L_sh, w_l - 2, &ex, &ey, c);
    limb(x + (w_t >> 1), y, p->a[7], L_th, w_l, &ex, &ey, c);
    limb(ex, ey, p->a[8], L_sh, w_l - 2, &ex, &ey, c);
    {
        int ox = COS(p->a[0]) * (w_t - 4) >> 7, oy = -(SIN(p->a[0]) * (w_t - 4) >> 7);
        limb(sx - ox, sy - oy, p->a[1], L_u, w_a, &ex, &ey, c);
        limb(ex, ey, p->a[2], L_l, w_a - 1, &ex, &ey, c);
        limb(sx + ox, sy + oy, p->a[3], L_u, w_a, &ex, &ey, c);
        limb(ex, ey, p->a[4], L_l, w_a - 1, &ex, &ey, c);
    }
    limb(sx, sy, p->a[0], 12 * k >> 6, 5 * k >> 6, &hx, &hy, c);   /* the neck */
    hx = sx + (SIN(p->a[0]) * (28 * k >> 6) >> 7); hy = sy + (COS(p->a[0]) * (28 * k >> 6) >> 7);
    cv_disc(hx >> 1, hy >> 1, (15 * k >> 6) >> 1, c);
    {                                           /* the extent: generous, from the limb lengths */
        int r = (L_t + L_u + L_l + 30 * k / 64) >> 1, r2 = (L_th + L_sh + 40 * k / 64) >> 1;
        int x0 = (x >> 1) - r, x1 = (x >> 1) + r, y0 = (y >> 1) - r, y1 = (y >> 1) + r2;
        if (x0 < d_box[0]) d_box[0] = (int16_t)x0;
        if (y0 < d_box[1]) d_box[1] = (int16_t)y0;
        if (x1 > d_box[2]) d_box[2] = (int16_t)x1;
        if (y1 > d_box[3]) d_box[3] = (int16_t)y1;
    }
}
/* the pose now: this beat's, snapping in from the last one over the first 16th */
static void pose_now(uint8_t set, pose_t *o)
{
    uint16_t beat = (uint16_t)(T >> 10);
    pose_t a, b;
    uint16_t k = beat_pos < 256 ? beat_pos : 256;
    uint8_t i;
    pose_get(routine[set][(beat - 1) & 15], &a);
    pose_get(routine[set][beat & 15], &b);
    k = (uint16_t)(256 - (((256 - k) * (256 - k)) >> 8));   /* ease out */
    for (i = 0; i < 9; i++) o->a[i] = lerp_a(a.a[i], b.a[i], k);
    o->dy = (int8_t)(a.dy + (((int16_t)(b.dy - a.dy) * (int16_t)k) >> 8));
    o->dy = (int8_t)(o->dy + (SIN((uint8_t)(beat_pos >> 2)) < 0 ? 0 : (SIN((uint8_t)(beat_pos >> 2)) >> 4)));   /* bounce */
}
static int16_t box_prev[2][4];
static uint8_t box_valid[2];
static void dance_clear(uint8_t which, uint32_t buf)    /* clear what this buffer showed two frames ago */
{
    cv_set(buf, 2, 0, 0, 320, 180);
    if (box_valid[which]) {
        int16_t *b = box_prev[which];
        cv_rect(b[0], b[1], b[2] - b[0] + 1, b[3] - b[1] + 1, 0);
    }
    d_box[0] = 320; d_box[1] = 180; d_box[2] = -1; d_box[3] = -1;
}
static void dance_done(uint8_t which)
{
    uint8_t i;
    for (i = 0; i < 4; i++) box_prev[which][i] = d_box[i];
    box_valid[which] = d_box[2] >= 0;
}
