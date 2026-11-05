/* The Future: flat-shaded 3D. Meshes of triangles and quads in one vertex and face pool, a camera (position,
 * yaw, pitch), perspective, back faces culled, faces sorted far to near in depth buckets and filled with
 * the canvas (so at any resolution). A face's colour is its material's 16-step palette ramp plus a light
 * level worked out once from its normal (the sun does not move); moving meshes re-light theirs. */

#define G_MAXV 900
#define G_MAXF 800
#define G_NB 64                                 /* depth buckets */
typedef struct { int16_t x, y, z; } vec3;
typedef struct { uint16_t v[4]; uint8_t col, nv; uint16_t next; } face;

static vec3 g_v[G_MAXV];
static int16_t g_sx[G_MAXV], g_sy[G_MAXV];
static int32_t g_z[G_MAXV];
static face g_f[G_MAXF];
static uint16_t g_nv, g_nf;
static uint16_t g_bucket[G_NB];
static int32_t g_cx, g_cy, g_cz;                /* the camera */
static int16_t g_sya, g_cya, g_sp, g_cp;        /* sin/cos of its yaw and pitch (Q14) */
static int16_t g_focal = 400, g_mx = 320, g_my = 180;
static int32_t g_zfar = 16000;
static int16_t g_ortho;                         /* non-zero: orthographic, screen units per 256 world units */
static const int16_t g_light[3] = {-7600, 10400, -9400};    /* the sun: left, high, behind the viewer (Q14) */

static void g_reset(void) { g_nv = 0; g_nf = 0; }
static uint16_t g_vert(int x, int y, int z)
{
    vec3 *v = &g_v[g_nv < G_MAXV - 1 ? g_nv : G_MAXV - 1];
    if (g_nv >= G_MAXV - 1) { IO(0) = 'V'; return (uint16_t)(G_MAXV - 1); }
    v->x = (int16_t)x; v->y = (int16_t)y; v->z = (int16_t)z;
    return g_nv++;
}
/* the light level 0-15 for a normal (any length) */
static uint8_t g_shade(int32_t nx, int32_t ny, int32_t nz)
{
    int32_t l = (int32_t)isqrt32((uint32_t)(nx * nx + ny * ny + nz * nz));
    int32_t d;
    if (!l) return 8;
    d = (nx * g_light[0] + ny * g_light[1] + nz * g_light[2]) / l;     /* Q14 */
    if (d < 0) d = 0;
    return (uint8_t)(3 + (d * 12 >> 14));
}
/* a face; the normal given (or 0, 0, 0: from the first three vertices) */
static void g_face(uint16_t a, uint16_t b, uint16_t c, uint16_t d, uint8_t ramp, int32_t nx, int32_t ny, int32_t nz)
{
    face *f;
    if (g_nf >= G_MAXF) { IO(0) = 'F'; return; }
    f = &g_f[g_nf++];
    if (!nx && !ny && !nz) {
        int32_t ux = g_v[b].x - g_v[a].x, uy = g_v[b].y - g_v[a].y, uz = g_v[b].z - g_v[a].z;
        int32_t vx = g_v[c].x - g_v[a].x, vy = g_v[c].y - g_v[a].y, vz = g_v[c].z - g_v[a].z;
        nx = (uy * vz - uz * vy) >> 6; ny = (uz * vx - ux * vz) >> 6; nz = (ux * vy - uy * vx) >> 6;
        nx = -nx; ny = -ny; nz = -nz;
    }
    f->v[0] = a; f->v[1] = b; f->v[2] = c; f->v[3] = d;
    f->nv = (uint8_t)(d == 0xFFFF ? 3 : 4);
    f->col = (uint8_t)(ramp + g_shade(nx, ny, nz));
}

/* meshes */
static void g_box(int x0, int y0, int z0, int x1, int y1, int z1, uint8_t ramp)
{
    uint16_t b = g_nv;
    g_vert(x0, y0, z0); g_vert(x1, y0, z0); g_vert(x1, y1, z0); g_vert(x0, y1, z0);
    g_vert(x0, y0, z1); g_vert(x1, y0, z1); g_vert(x1, y1, z1); g_vert(x0, y1, z1);
    g_face(b + 0, b + 1, b + 2, b + 3, ramp, 0, 0, -1);           /* -z */
    g_face(b + 5, b + 4, b + 7, b + 6, ramp, 0, 0, 1);            /* +z */
    g_face(b + 4, b + 0, b + 3, b + 7, ramp, -1, 0, 0);           /* -x */
    g_face(b + 1, b + 5, b + 6, b + 2, ramp, 1, 0, 0);            /* +x */
    g_face(b + 3, b + 2, b + 6, b + 7, ramp, 0, 1, 0);            /* top */
}
/* a dome: segs around, rings up; window rings in a second ramp */
static void g_dome(int cx, int cz, int r, uint8_t segs, uint8_t rings, uint8_t ramp, uint8_t ramp2)
{
    uint16_t b = g_nv, top;
    uint8_t i, j;
    for (j = 0; j < rings; j++) {
        uint8_t e = (uint8_t)(j * 64 / rings);
        int32_t rr = (int32_t)r * COS16(e) >> 14, h = (int32_t)r * SIN16(e) >> 14;
        for (i = 0; i < segs; i++) {
            uint8_t a = (uint8_t)(i * 256 / segs);
            g_vert(cx + (int)(rr * COS16(a) >> 14), (int)h, cz + (int)(rr * SIN16(a) >> 14));
        }
    }
    top = g_vert(cx, r, cz);
    for (j = 0; j < rings; j++)
        for (i = 0; i < segs; i++) {
            uint16_t a0 = (uint16_t)(b + j * segs + i), a1 = (uint16_t)(b + j * segs + (i + 1) % segs);
            uint8_t e = (uint8_t)((j * 2 + 1) * 32 / rings), a = (uint8_t)((i * 2 + 1) * 128 / segs);
            int32_t nx = (int32_t)COS16(e) * COS16(a) >> 14, ny = SIN16(e), nz = (int32_t)COS16(e) * SIN16(a) >> 14;
            uint8_t rp = (j == rings / 2 && (i & 1)) ? ramp2 : ramp;
            if (j + 1 < rings) g_face(a0, a1, (uint16_t)(a1 + segs), (uint16_t)(a0 + segs), rp, nx, ny, nz);
            else g_face(a0, a1, top, 0xFFFF, rp, nx, ny, nz);
        }
}
/* a cylinder (or cone: radius r0 at x0, r1 at x1) lying along x from x0 to x1 */
static void g_cone(int x0, int x1, int cy, int cz, int r0, int r1, uint8_t segs, uint8_t ramp)
{
    uint16_t b = g_nv;
    uint8_t i;
    for (i = 0; i < segs; i++) {
        uint8_t a = (uint8_t)(i * 256 / segs);
        int16_t sa = SIN16(a), ca = COS16(a);
        g_vert(x0, cy + (int)((int32_t)r0 * sa >> 14), cz + (int)((int32_t)r0 * ca >> 14));
        g_vert(x1, cy + (int)((int32_t)r1 * sa >> 14), cz + (int)((int32_t)r1 * ca >> 14));
    }
    for (i = 0; i < segs; i++) {
        uint8_t a = (uint8_t)((i * 2 + 1) * 128 / segs), k = (uint8_t)((i + 1) % segs);
        g_face((uint16_t)(b + 2 * i), (uint16_t)(b + 2 * k), (uint16_t)(b + 2 * k + 1), (uint16_t)(b + 2 * i + 1), ramp, 0, SIN16(a), COS16(a));
    }
    for (i = 1; i + 1 < segs; i++) {           /* the end caps */
        g_face(b, (uint16_t)(b + 2 * i + 2), (uint16_t)(b + 2 * i), 0xFFFF, ramp, -1, 0, 0);
        g_face(b + 1, (uint16_t)(b + 2 * i + 1), (uint16_t)(b + 2 * i + 3), 0xFFFF, ramp, 1, 0, 0);
    }
}
static void g_tube(int x0, int x1, int cy, int cz, int r, uint8_t segs, uint8_t ramp) { g_cone(x0, x1, cy, cz, r, r, segs, ramp); }
/* a flat disc on the ground (n sides) */
static void g_disc(int cx, int y, int cz, int r, uint8_t n, uint8_t ramp)
{
    uint16_t c = g_vert(cx, y, cz), b = g_nv;
    uint8_t i;
    for (i = 0; i < n; i++) {
        uint8_t a = (uint8_t)(i * 256 / n);
        g_vert(cx + (int)((int32_t)r * COS16(a) >> 14), y, cz + (int)((int32_t)r * SIN16(a) >> 14));
    }
    for (i = 0; i < n; i++) g_face(c, (uint16_t)(b + i), (uint16_t)(b + (i + 1) % n), 0xFFFF, ramp, 0, 1, 0);
}

/* the camera */
static void g_camera(int32_t x, int32_t y, int32_t z, uint8_t yaw, uint8_t pitch)
{
    g_cx = x; g_cy = y; g_cz = z;
    g_sya = SIN16(yaw); g_cya = COS16(yaw); g_sp = SIN16(pitch); g_cp = COS16(pitch);
}
/* a camera at distance d looking at the origin along yaw and pitch */
static void g_look(uint8_t yaw, uint8_t pitch, int32_t d)
{
    int32_t h = (d * COS16(pitch)) >> 14;
    g_camera(-(h * SIN16(yaw) >> 14), -(d * SIN16(pitch) >> 14), -(h * COS16(yaw) >> 14), yaw, pitch);
}
/* where a direction (dx, dy, dz) from the camera lands on the screen; 0 if behind */
static uint8_t g_project(int32_t dx, int32_t dy, int32_t dz, int16_t *sx, int16_t *sy, int32_t *zo)
{
    int32_t x1 = (dx * g_cya - dz * g_sya) >> 14, z1 = (dx * g_sya + dz * g_cya) >> 14;
    int32_t y2 = (dy * g_cp - z1 * g_sp) >> 14, z2 = (dy * g_sp + z1 * g_cp) >> 14;
    int32_t inv;
    *zo = z2;
    if (z2 < 40) return 0;
    if (g_ortho) {
        *sx = (int16_t)(g_mx + (x1 * g_ortho >> 8));
        *sy = (int16_t)(g_my - (y2 * g_ortho >> 8));
        return 1;
    }
    inv = ((int32_t)g_focal << 14) / z2;
    *sx = (int16_t)(g_mx + (x1 * inv >> 14));
    *sy = (int16_t)(g_my - (y2 * inv >> 14));
    return 1;
}
static void g_transform(uint16_t from)
{
    uint16_t i;
    for (i = from; i < g_nv; i++) {
        const vec3 *v = &g_v[i];
        if (!g_project(v->x - g_cx, v->y - g_cy, v->z - g_cz, &g_sx[i], &g_sy[i], &g_z[i])) g_z[i] = -1;
    }
}
/* drawing: g_sort culls and buckets faces from..g_nf by depth; g_paint draws n buckets, far to near, and
 * returns 1 when all are done (so a big picture can be drawn a little each frame) */
static int8_t g_next = -1;
static void g_sort(uint16_t from)
{
    uint16_t i;
    int b;
    for (i = 0; i < G_NB; i++) g_bucket[i] = 0xFFFF;
    for (i = from; i < g_nf; i++) {
        face *f = &g_f[i];
        int32_t z = 0, ex, ey, gx, gy;
        uint8_t k, ok = 1;
        for (k = 0; k < f->nv; k++) {
            int32_t zz = g_z[f->v[k]];
            if (zz < 0) ok = 0;
            if (zz > z) z = zz;
        }
        if (!ok) continue;
        ex = g_sx[f->v[1]] - g_sx[f->v[0]]; ey = g_sy[f->v[1]] - g_sy[f->v[0]];
        gx = g_sx[f->v[2]] - g_sx[f->v[0]]; gy = g_sy[f->v[2]] - g_sy[f->v[0]];
        if (ex * gy - ey * gx >= 0) continue;   /* facing away */
        b = (int)(z * G_NB / g_zfar);
        if (b >= G_NB) b = G_NB - 1;
        if (b < 0) b = 0;
        f->next = g_bucket[b];
        g_bucket[b] = i;
    }
    g_next = G_NB - 1;
}
static uint8_t g_paint(uint8_t nb)
{
    while (g_next >= 0 && nb--) {
        uint16_t j = g_bucket[g_next];
        while (j != 0xFFFF) {
            face *f = &g_f[j];
            const uint16_t *v = f->v;
            if (cv_b == 1) {
                cv_tri(g_sx[v[0]], g_sy[v[0]], g_sx[v[1]], g_sy[v[1]], g_sx[v[2]], g_sy[v[2]], f->col);
                if (f->nv == 4) cv_tri(g_sx[v[0]], g_sy[v[0]], g_sx[v[2]], g_sy[v[2]], g_sx[v[3]], g_sy[v[3]], f->col);
            } else {
                wtri(g_sx[v[0]], g_sy[v[0]], g_sx[v[1]], g_sy[v[1]], g_sx[v[2]], g_sy[v[2]], f->col);
                if (f->nv == 4) wtri(g_sx[v[0]], g_sy[v[0]], g_sx[v[2]], g_sy[v[2]], g_sx[v[3]], g_sy[v[3]], f->col);
            }
            j = f->next;
        }
        g_next--;
        poll();
    }
    return (uint8_t)(g_next < 0);
}
static void g_draw(uint16_t from) { g_sort(from); g_paint(G_NB); }
/* a 16-step ramp from near black through the key colour to a highlight */
static void g_ramp(uint8_t base, uint16_t key, uint16_t k)
{
    uint8_t i;
    for (i = 0; i < 16; i++) {
        uint16_t c = i < 12 ? dim565(key, (uint16_t)(24 + i * 20)) : mix565(key, RGB(255, 255, 255), (uint16_t)((i - 11) * 40));
        pal565((uint8_t)(base + i), dim565(c, k));
    }
}
