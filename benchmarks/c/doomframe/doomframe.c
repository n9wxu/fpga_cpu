/* doomframe: one Doom frame's work in 64 KB.
 *
 * Doom's renderer, cut down to fit a 16-bit address space without
 * banking: the BSP walk (R_RenderBSPNode, R_CheckBBox), wall clipping
 * and projection (R_AddLine, R_StoreWallRange, R_RenderSegLoop),
 * visplanes and floor/ceiling spans (R_FindPlane, R_CheckPlane,
 * R_DrawPlanes, R_MapPlane) and sprites clipped against the walls
 * (R_ProjectSprite, R_DrawSprite, R_DrawMaskedColumn), all with Doom's
 * fixed-point arithmetic. The code follows benchmarks/doom/src (id
 * Software, GPL v2); the changes are in README.md.
 *
 * The frame is 128x80 at 8 bits per pixel (10,240 bytes) and is drawn
 * into video memory: define VRAM_ADDR to the platform's frame buffer.
 * The map, textures and every start-up table come from gen_data.py, so
 * nothing worth timing happens before the first frame.
 *
 * Renders NUMVIEWS frames from different places in a small level. It
 * prints nothing: after each frame it stores the frame number in
 * frame_done, and the frames and their cost are read from the
 * simulator's log (README.md). -DREFERENCE builds it for the development
 * machine, writing each frame as frameN.raw and frameN.ppm to compare. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined REFERENCE || defined CHECKSUM
#include <stdio.h>
#endif

#define FRACBITS     16
#define FRACUNIT     ((fixed_t)1 << FRACBITS)
#define RANGECHECK

typedef int32_t fixed_t;
typedef uint32_t angle_t;
typedef unsigned char byte;
typedef byte lighttable_t;

/* the screen, table and texture sizes are in doomframe_data.h */
#define FINEMASK         (FINEANGLES - 1)
#define DBITS            (FRACBITS - SLOPEBITS)
#define ANG45            ((angle_t)0x20000000)
#define ANG90            ((angle_t)0x40000000)
#define ANG180           ((angle_t)0x80000000)
#define ANG270           ((angle_t)0xc0000000)

#define LIGHTSEGSHIFT    4
#define VIEWHEIGHT       (41 * FRACUNIT)
#define MINZ             (FRACUNIT * 4)

#define NF_SUBSECTOR     0x8000
#define ML_DONTPEGTOP    8
#define ML_DONTPEGBOTTOM 16
#define BOXTOP 0
#define BOXBOTTOM 1
#define BOXLEFT 2
#define BOXRIGHT 3
#define SIL_NONE   0
#define SIL_BOTTOM 1
#define SIL_TOP    2
#define SIL_BOTH   3

#define MAXDRAWSEGS   24
#define MAXVISPLANES  16
#define MAXOPENINGS   SCREENWIDTH
#define MAXVISSPRITES 4
#define MAXSEGS       32
#define VISROW_NONE   0xff

typedef struct { fixed_t x, y; } vertex_t;

struct mobj_s;
/* The map is const, as it would be in ROM; the renderer's two writes to
 * it in Doom go elsewhere: sector validcount to sectorvalid[], and the
 * automap's ML_MAPPED flag is left out */
typedef struct {
    fixed_t floorheight, ceilingheight;
    short floorpic, ceilingpic, lightlevel, index;
    const struct mobj_s *thinglist;
} sector_t;

typedef struct {
    fixed_t textureoffset, rowoffset;
    short toptexture, bottomtexture, midtexture;
    const sector_t *sector;
} side_t;

typedef struct {
    const vertex_t *v1, *v2;
    fixed_t dx, dy;
    short flags;
    short sidenum[2];
    const sector_t *frontsector, *backsector;
} line_t;

typedef struct {
    const vertex_t *v1, *v2;
    fixed_t offset;
    angle_t angle;
    const side_t *sidedef;
    const line_t *linedef;
    const sector_t *frontsector, *backsector;
} seg_t;

typedef struct {
    const sector_t *sector;
    short numlines, firstline;
} subsector_t;

typedef struct {
    fixed_t x, y, dx, dy;
    fixed_t bbox[2][4];
    unsigned short children[2];
} node_t;

typedef struct mobj_s {
    fixed_t x, y, z;
    angle_t angle;
    const struct mobj_s *snext;
} mobj_t;

struct view { fixed_t x, y; angle_t angle; };

#include "doomframe_data.h"

#define finecosine (&finesine[FINEANGLES / 4])

/* frames rendered: all the views, or fewer (FRAMES=0 measures start-up
 * alone, for simulators that only report a whole run's cycles) */
#ifndef FRAMES
#define FRAMES NUMVIEWS
#endif

typedef struct {
    int x1, x2;
    fixed_t scale1, scale2, scalestep;
    int silhouette;
    fixed_t bsilheight, tsilheight;
    short *sprtopclip, *sprbottomclip;
    const seg_t *curline;
} drawseg_t;

/* top and bottom have a pad byte each side: Doom reads top[minx - 1]
 * and top[maxx + 1] */
typedef struct {
    fixed_t height;
    int picnum, lightlevel, minx, maxx;
    byte topbuf[SCREENWIDTH + 2];
    byte bottombuf[SCREENWIDTH + 2];
} visplane_t;
#define TOP(pl, x)    ((pl)->topbuf[(x) + 1])
#define BOTTOM(pl, x) ((pl)->bottombuf[(x) + 1])

typedef struct {
    int x1, x2;
    fixed_t gx, gy, gz, gzt;
    fixed_t startfrac, scale, xiscale, texturemid, iscale;
    lighttable_t *colormap;
} vissprite_t;

typedef struct { int first, last; } cliprange_t;

/* ---- state (r_main.c, r_bsp.c, r_segs.c, r_plane.c, r_things.c, r_draw.c) ---- */
fixed_t viewx, viewy, viewz, viewsin, viewcos;
angle_t viewangle, clipangle;
int validcount = 1;
#define viewwidth  SCREENWIDTH
#define viewheight SCREENHEIGHT
#define centerx    (SCREENWIDTH / 2)
#define centery    (SCREENHEIGHT / 2)
#define centerxfrac ((fixed_t)centerx << FRACBITS)
#define centeryfrac ((fixed_t)centery << FRACBITS)
#define projection  centerxfrac

/* Doom's scalelight and zlight hold colormap pointers; here they hold
 * colormap numbers (scalelightmap, zlightmap), which halves them on a
 * 16-bit machine and quarters them where pointers are 32 bits */
#define CMAP(n) ((lighttable_t *)colormaps + ((unsigned)(n) << 8))
#define scalelight(l) (&scalelightmap[(l) * MAXLIGHTSCALE])
#define zlight(l)     (&zlightmap[(l) * MAXLIGHTZ])

const seg_t *curline;
const side_t *sidedef;
const line_t *linedef;
const sector_t *frontsector, *backsector;
int sectorvalid[NUMSECTORS];
drawseg_t drawsegs[MAXDRAWSEGS], *ds_p;
cliprange_t *newend, solidsegs[MAXSEGS];

int segtextured, markfloor, markceiling;
int toptexture, bottomtexture, midtexture;
angle_t rw_normalangle, rw_angle1, rw_centerangle;
int rw_x, rw_stopx;
fixed_t rw_offset, rw_distance, rw_scale, rw_scalestep;
fixed_t rw_midtexturemid, rw_toptexturemid, rw_bottomtexturemid;
fixed_t worldtop, worldbottom, worldhigh, worldlow;
fixed_t pixhigh, pixlow, pixhighstep, pixlowstep;
fixed_t topfrac, topstep, bottomfrac, bottomstep;
const byte *walllights;

visplane_t visplanes[MAXVISPLANES], *lastvisplane, *floorplane, *ceilingplane;
short openings[MAXOPENINGS], *lastopening;
short floorclip[SCREENWIDTH], ceilingclip[SCREENWIDTH];
short negonearray[SCREENWIDTH], screenheightarray[SCREENWIDTH];
int spanstart[SCREENHEIGHT];
const byte *planezlight;
fixed_t planeheight, basexscale, baseyscale;
fixed_t cachedheight[SCREENHEIGHT], cacheddistance[SCREENHEIGHT];
fixed_t cachedxstep[SCREENHEIGHT], cachedystep[SCREENHEIGHT];

vissprite_t vissprites[MAXVISSPRITES], *vissprite_p;
const byte *spritelights;
short clipbot[SCREENWIDTH], cliptop[SCREENWIDTH];
short *mfloorclip, *mceilingclip;
fixed_t spryscale, sprtopscreen;

byte *ylookup[SCREENHEIGHT];
int columnofs[SCREENWIDTH];
lighttable_t *dc_colormap;
int dc_x, dc_yl, dc_yh;
fixed_t dc_iscale, dc_texturemid;
const byte *dc_source;
int ds_y, ds_x1, ds_x2;
lighttable_t *ds_colormap;
fixed_t ds_xfrac, ds_yfrac, ds_xstep, ds_ystep;
const byte *ds_source;


#ifdef VRAM_ADDR
#define VRAM ((byte *)VRAM_ADDR)
#else
static byte vram[SCREENWIDTH * SCREENHEIGHT];
#define VRAM vram
#endif

/* stored at the end of every frame: the simulator log's frame marker */
volatile unsigned char frame_done;

/* The benchmark prints nothing: the frames are read back from video
 * memory, and the simulator's log gives the timing (README.md). Only the
 * REFERENCE build for the development machine writes files. */
#ifdef REFERENCE
void I_Error(const char *msg)
{
    printf("I_Error: %s\n", msg);
    exit(1);
}
#elif defined __SDCC
/* SDCC's library has no exit: mark the failure and stop */
#define I_Error(msg) do { frame_done = 0xff; for (;;) ; } while (0)
#else
#define I_Error(msg) exit(1)
#endif

static fixed_t fabs32(fixed_t x) { return x < 0 ? -x : x; }

/* ---- m_fixed.c ---- */
#ifndef FIXED32
fixed_t FixedMul(fixed_t a, fixed_t b)
{
    return ((int64_t)a * (int64_t)b) >> FRACBITS;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    if ((fabs32(a) >> 14) >= fabs32(b))
        return (a ^ b) < 0 ? INT32_MIN : INT32_MAX;
    return (fixed_t)(((int64_t)a << 16) / b);
}
#else
/* -DFIXED32: the same results without 64-bit arithmetic, as a port to a
 * machine whose compiler has slow (or no) 64-bit support would write
 * them. FixedMul from four 16x16 products, rounded down like the 64-bit
 * shift; FixedDiv by long division, truncated like the 64-bit divide. */
fixed_t FixedMul(fixed_t a, fixed_t b)
{
    uint32_t ua = a < 0 ? 0u - (uint32_t)a : (uint32_t)a;
    uint32_t ub = b < 0 ? 0u - (uint32_t)b : (uint32_t)b;
    uint16_t ah = (uint16_t)(ua >> 16), al = (uint16_t)ua;
    uint16_t bh = (uint16_t)(ub >> 16), bl = (uint16_t)ub;
    uint32_t lo = (uint32_t)al * bl;
    uint32_t r = ((uint32_t)ah * bh << 16) + (uint32_t)ah * bl + (uint32_t)al * bh + (lo >> 16);

    if ((a ^ b) < 0)
        r = 0u - r - ((uint16_t)lo != 0);
    return (fixed_t)r;
}

fixed_t FixedDiv(fixed_t a, fixed_t b)
{
    uint32_t ua, ub, q, r;
    int i;

    if ((fabs32(a) >> 14) >= fabs32(b))
        return (a ^ b) < 0 ? INT32_MIN : INT32_MAX;
    ua = a < 0 ? 0u - (uint32_t)a : (uint32_t)a;
    ub = b < 0 ? 0u - (uint32_t)b : (uint32_t)b;
    q = ua / ub;
    r = ua % ub;
    for (i = 0; i < 16; i++) {
        r <<= 1;
        q <<= 1;
        if (r >= ub) {
            r -= ub;
            q |= 1;
        }
    }
    return (a ^ b) < 0 ? -(fixed_t)q : (fixed_t)q;
}
#endif

/* ---- tables.c ---- */
int SlopeDiv(uint32_t num, uint32_t den)
{
    uint32_t ans;
    if (den < 512)
        return SLOPERANGE;
    ans = (num << (SLOPEBITS - 8)) / (den >> 8);
    return ans <= SLOPERANGE ? (int)ans : SLOPERANGE;
}

/* ---- r_draw.c ---- */
void R_DrawColumn(void)
{
    int count;
    byte *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;
#ifdef RANGECHECK
    if ((unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn");
#endif
    dest = ylookup[dc_yl] + columnofs[dc_x];
    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;
    do {
        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & (TEXH - 1)]];
        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
}

/* Doom's packed span loop (doomgeneric) for a 32x32 flat: x and y are
 * 5.11 fixed point in the two halves of position */
void R_DrawSpan(void)
{
    uint32_t position, step;
    byte *dest;
    int count, spot;
    unsigned xtemp, ytemp;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH || (unsigned)ds_y > SCREENHEIGHT)
        I_Error("R_DrawSpan");
#endif
    position = (((uint32_t)ds_xfrac << 11) & 0xffff0000) | (((uint32_t)ds_yfrac >> 5) & 0x0000ffff);
    step = (((uint32_t)ds_xstep << 11) & 0xffff0000) | (((uint32_t)ds_ystep >> 5) & 0x0000ffff);
    dest = ylookup[ds_y] + columnofs[ds_x1];
    count = ds_x2 - ds_x1;
    do {
        ytemp = (unsigned)(position >> 6) & 0x03e0;
        xtemp = (unsigned)(position >> 27);
        spot = xtemp | ytemp;
        *dest++ = ds_colormap[ds_source[spot]];
        position += step;
    } while (count--);
}

/* ---- r_main.c ---- */
int R_PointOnSide(fixed_t x, fixed_t y, const node_t *node)
{
    fixed_t dx, dy, left, right;

    if (!node->dx) {
        if (x <= node->x)
            return node->dy > 0;
        return node->dy < 0;
    }
    if (!node->dy) {
        if (y <= node->y)
            return node->dx < 0;
        return node->dx > 0;
    }
    dx = x - node->x;
    dy = y - node->y;
    if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000) {
        if ((node->dy ^ dx) & 0x80000000)
            return 1;
        return 0;
    }
    left = FixedMul(node->dy >> FRACBITS, dx);
    right = FixedMul(dy, node->dx >> FRACBITS);
    if (right < left)
        return 0;
    return 1;
}

int R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
    fixed_t lx, ly, ldx, ldy, dx, dy, left, right;

    lx = line->v1->x;
    ly = line->v1->y;
    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;
    if (!ldx) {
        if (x <= lx)
            return ldy > 0;
        return ldy < 0;
    }
    if (!ldy) {
        if (y <= ly)
            return ldx < 0;
        return ldx > 0;
    }
    dx = x - lx;
    dy = y - ly;
    if ((ldy ^ ldx ^ dx ^ dy) & 0x80000000) {
        if ((ldy ^ dx) & 0x80000000)
            return 1;
        return 0;
    }
    left = FixedMul(ldy >> FRACBITS, dx);
    right = FixedMul(dy, ldx >> FRACBITS);
    if (right < left)
        return 0;
    return 1;
}

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;
    if (!x && !y)
        return 0;
    if (x >= 0) {
        if (y >= 0) {
            if (x > y)
                return tantoangle[SlopeDiv(y, x)];                 /* octant 0 */
            return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];          /* octant 1 */
        }
        y = -y;
        if (x > y)
            return -tantoangle[SlopeDiv(y, x)];                     /* octant 8 */
        return ANG270 + tantoangle[SlopeDiv(x, y)];                 /* octant 7 */
    }
    x = -x;
    if (y >= 0) {
        if (x > y)
            return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];         /* octant 3 */
        return ANG90 + tantoangle[SlopeDiv(x, y)];                  /* octant 2 */
    }
    y = -y;
    if (x > y)
        return ANG180 + tantoangle[SlopeDiv(y, x)];                 /* octant 4 */
    return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];                 /* octant 5 */
}

fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    int angle;
    fixed_t dx, dy, temp, frac;

    dx = fabs32(x - viewx);
    dy = fabs32(y - viewy);
    if (dy > dx) {
        temp = dx;
        dx = dy;
        dy = temp;
    }
    frac = dx != 0 ? FixedDiv(dy, dx) : 0;
    angle = (int)((tantoangle[frac >> DBITS] + ANG90) >> ANGLETOFINESHIFT);
    return FixedDiv(dx, finesine[angle]);       /* finesine used as cosine */
}

fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    fixed_t scale, sinea, sineb, num, den;
    angle_t anglea, angleb;

    anglea = ANG90 + (visangle - viewangle);
    angleb = ANG90 + (visangle - rw_normalangle);
    sinea = finesine[anglea >> ANGLETOFINESHIFT];   /* both sines are always positive */
    sineb = finesine[angleb >> ANGLETOFINESHIFT];
    num = FixedMul(projection, sineb);
    den = FixedMul(rw_distance, sinea);
    if (den > num >> 16) {
        scale = FixedDiv(num, den);
        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < 256)
            scale = 256;
    } else
        scale = 64 * FRACUNIT;
    return scale;
}

const subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
    unsigned nodenum = ROOTNODE;
    while (!(nodenum & NF_SUBSECTOR)) {
        const node_t *node = &nodes[nodenum];
        nodenum = node->children[R_PointOnSide(x, y, node)];
    }
    return &subsectors[nodenum & ~NF_SUBSECTOR];
}

/* ---- r_plane.c ---- */
void R_MapPlane(int y, int x1, int x2)
{
    angle_t angle;
    fixed_t distance, length;
    uint32_t index;

#ifdef RANGECHECK
    if (x2 < x1 || x1 < 0 || x2 >= viewwidth || y > viewheight)
        I_Error("R_MapPlane");
#endif
    if (planeheight != cachedheight[y]) {
        cachedheight[y] = planeheight;
        distance = cacheddistance[y] = FixedMul(planeheight, yslope[y]);
        ds_xstep = cachedxstep[y] = FixedMul(distance, basexscale);
        ds_ystep = cachedystep[y] = FixedMul(distance, baseyscale);
    } else {
        distance = cacheddistance[y];
        ds_xstep = cachedxstep[y];
        ds_ystep = cachedystep[y];
    }
    length = FixedMul(distance, distscale[x1]);
    angle = (viewangle + xtoviewangle[x1]) >> ANGLETOFINESHIFT;
    ds_xfrac = viewx + FixedMul(finecosine[angle], length);
    ds_yfrac = -viewy - FixedMul(finesine[angle], length);
    index = (uint32_t)distance >> LIGHTZSHIFT;
    if (index >= MAXLIGHTZ)
        index = MAXLIGHTZ - 1;
    ds_colormap = CMAP(planezlight[index]);
    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;
    R_DrawSpan();
}

void R_ClearPlanes(void)
{
    int i;
    angle_t angle;

    for (i = 0; i < viewwidth; i++) {
        floorclip[i] = viewheight;
        ceilingclip[i] = -1;
    }
    lastvisplane = visplanes;
    lastopening = openings;
    memset(cachedheight, 0, sizeof cachedheight);
    angle = (viewangle - ANG90) >> ANGLETOFINESHIFT;  /* left to right mapping */
    basexscale = FixedDiv(finecosine[angle], centerxfrac);
    baseyscale = -FixedDiv(finesine[angle], centerxfrac);
}

visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel)
{
    visplane_t *check;

    for (check = visplanes; check < lastvisplane; check++)
        if (height == check->height && picnum == check->picnum && lightlevel == check->lightlevel)
            return check;
    if (lastvisplane - visplanes == MAXVISPLANES)
        I_Error("R_FindPlane: no more visplanes");
    lastvisplane++;
    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;
    memset(check->topbuf, VISROW_NONE, sizeof check->topbuf);
    return check;
}

visplane_t *R_CheckPlane(visplane_t *pl, int start, int stop)
{
    int intrl, intrh, unionl, unionh, x;

    if (start < pl->minx) {
        intrl = pl->minx;
        unionl = start;
    } else {
        unionl = pl->minx;
        intrl = start;
    }
    if (stop > pl->maxx) {
        intrh = pl->maxx;
        unionh = stop;
    } else {
        unionh = pl->maxx;
        intrh = stop;
    }
    for (x = intrl; x <= intrh; x++)
        if (TOP(pl, x) != VISROW_NONE)
            break;
    if (x > intrh) {
        pl->minx = unionl;
        pl->maxx = unionh;
        return pl;                      /* use the same one */
    }
    if (lastvisplane - visplanes == MAXVISPLANES)
        I_Error("R_CheckPlane: no more visplanes");
    lastvisplane->height = pl->height;  /* make a new visplane */
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;
    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;
    memset(pl->topbuf, VISROW_NONE, sizeof pl->topbuf);
    return pl;
}

void R_MakeSpans(int x, int t1, int b1, int t2, int b2)
{
    while (t1 < t2 && t1 <= b1) {
        R_MapPlane(t1, spanstart[t1], x - 1);
        t1++;
    }
    while (b1 > b2 && b1 >= t1) {
        R_MapPlane(b1, spanstart[b1], x - 1);
        b1--;
    }
    while (t2 < t1 && t2 <= b2) {
        spanstart[t2] = x;
        t2++;
    }
    while (b2 > b1 && b2 >= t2) {
        spanstart[b2] = x;
        b2--;
    }
}

void R_DrawPlanes(void)
{
    visplane_t *pl;
    int light, x, stop;

#ifdef RANGECHECK
    if (lastopening - openings > MAXOPENINGS)
        I_Error("R_DrawPlanes: opening overflow");
#endif
    for (pl = visplanes; pl < lastvisplane; pl++) {
        if (pl->minx > pl->maxx)
            continue;
        ds_source = flats[pl->picnum];
        planeheight = fabs32(pl->height - viewz);
        light = pl->lightlevel >> LIGHTSEGSHIFT;
        if (light >= LIGHTLEVELS)
            light = LIGHTLEVELS - 1;
        planezlight = zlight(light);
        TOP(pl, pl->maxx + 1) = VISROW_NONE;
        TOP(pl, pl->minx - 1) = VISROW_NONE;
        stop = pl->maxx + 1;
        for (x = pl->minx; x <= stop; x++)
            R_MakeSpans(x, TOP(pl, x - 1), BOTTOM(pl, x - 1), TOP(pl, x), BOTTOM(pl, x));
    }
}

/* ---- r_segs.c ---- */
#define HEIGHTBITS 12
#define HEIGHTUNIT (1 << HEIGHTBITS)

static const byte *R_GetColumn(int tex, int col)
{
    return walltextures[tex - 1] + (col & (TEXW - 1)) * TEXH;
}

void R_RenderSegLoop(void)
{
    angle_t angle;
    uint32_t index;
    int yl, yh, mid, top, bottom;
    fixed_t texturecolumn;

    for (; rw_x < rw_stopx; rw_x++) {
        /* mark floor / ceiling areas */
        yl = (int)((topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS);
        if (yl < ceilingclip[rw_x] + 1)         /* no space above wall? */
            yl = ceilingclip[rw_x] + 1;
        if (markceiling) {
            top = ceilingclip[rw_x] + 1;
            bottom = yl - 1;
            if (bottom >= floorclip[rw_x])
                bottom = floorclip[rw_x] - 1;
            if (top <= bottom) {
                TOP(ceilingplane, rw_x) = top;
                BOTTOM(ceilingplane, rw_x) = bottom;
            }
        }
        yh = (int)(bottomfrac >> HEIGHTBITS);
        if (yh >= floorclip[rw_x])
            yh = floorclip[rw_x] - 1;
        if (markfloor) {
            top = yh + 1;
            bottom = floorclip[rw_x] - 1;
            if (top <= ceilingclip[rw_x])
                top = ceilingclip[rw_x] + 1;
            if (top <= bottom) {
                TOP(floorplane, rw_x) = top;
                BOTTOM(floorplane, rw_x) = bottom;
            }
        }
        /* texturecolumn and lighting are independent of wall tiers */
        if (segtextured) {
            angle = (rw_centerangle + xtoviewangle[rw_x]) >> ANGLETOFINESHIFT;
            texturecolumn = rw_offset - FixedMul(finetangent[angle], rw_distance);
            texturecolumn >>= FRACBITS;
            index = (uint32_t)rw_scale >> LIGHTSCALESHIFT;
            if (index >= MAXLIGHTSCALE)
                index = MAXLIGHTSCALE - 1;
            dc_colormap = CMAP(walllights[index]);
            dc_x = rw_x;
            dc_iscale = 0xffffffffu / (uint32_t)rw_scale;
        } else
            texturecolumn = 0;
        /* draw the wall tiers */
        if (midtexture) {
            dc_yl = yl;                         /* single sided line */
            dc_yh = yh;
            dc_texturemid = rw_midtexturemid;
            dc_source = R_GetColumn(midtexture, (int)texturecolumn);
            R_DrawColumn();
            ceilingclip[rw_x] = viewheight;
            floorclip[rw_x] = -1;
        } else {
            if (toptexture) {                   /* two sided line: top wall */
                mid = (int)(pixhigh >> HEIGHTBITS);
                pixhigh += pixhighstep;
                if (mid >= floorclip[rw_x])
                    mid = floorclip[rw_x] - 1;
                if (mid >= yl) {
                    dc_yl = yl;
                    dc_yh = mid;
                    dc_texturemid = rw_toptexturemid;
                    dc_source = R_GetColumn(toptexture, (int)texturecolumn);
                    R_DrawColumn();
                    ceilingclip[rw_x] = mid;
                } else
                    ceilingclip[rw_x] = yl - 1;
            } else if (markceiling)
                ceilingclip[rw_x] = yl - 1;     /* no top wall */
            if (bottomtexture) {                /* bottom wall */
                mid = (int)((pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS);
                pixlow += pixlowstep;
                if (mid <= ceilingclip[rw_x])   /* no space above wall? */
                    mid = ceilingclip[rw_x] + 1;
                if (mid <= yh) {
                    dc_yl = mid;
                    dc_yh = yh;
                    dc_texturemid = rw_bottomtexturemid;
                    dc_source = R_GetColumn(bottomtexture, (int)texturecolumn);
                    R_DrawColumn();
                    floorclip[rw_x] = mid;
                } else
                    floorclip[rw_x] = yh + 1;
            } else if (markfloor)
                floorclip[rw_x] = yh + 1;       /* no bottom wall */
        }
        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;
    }
}

void R_StoreWallRange(int start, int stop)
{
    fixed_t hyp, sineval, vtop;
    angle_t distangle, offsetangle;
    int lightnum;

    if (ds_p == &drawsegs[MAXDRAWSEGS])     /* don't overflow and crash */
        return;
#ifdef RANGECHECK
    if (start >= viewwidth || start > stop)
        I_Error("Bad R_RenderWallRange");
#endif
    sidedef = curline->sidedef;
    linedef = curline->linedef;

    /* calculate rw_distance for scale calculation */
    rw_normalangle = curline->angle + ANG90;
    offsetangle = (angle_t)fabs32((fixed_t)(rw_normalangle - rw_angle1));
    if (offsetangle > ANG90)
        offsetangle = ANG90;
    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist(curline->v1->x, curline->v1->y);
    sineval = finesine[distangle >> ANGLETOFINESHIFT];
    rw_distance = FixedMul(hyp, sineval);

    ds_p->x1 = rw_x = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop + 1;

    /* calculate scale at both ends and step */
    ds_p->scale1 = rw_scale = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[start]);
    if (stop > start) {
        ds_p->scale2 = R_ScaleFromGlobalAngle(viewangle + xtoviewangle[stop]);
        ds_p->scalestep = rw_scalestep = (ds_p->scale2 - rw_scale) / (stop - start);
    } else
        ds_p->scale2 = ds_p->scale1;

    /* calculate texture boundaries and decide if floor / ceiling marks are needed */
    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;
    midtexture = toptexture = bottomtexture = 0;

    if (!backsector) {
        midtexture = sidedef->midtexture;   /* single sided line */
        markfloor = markceiling = 1;        /* a single sided line is terminal, so it must mark ends */
        if (linedef->flags & ML_DONTPEGBOTTOM) {
            vtop = frontsector->floorheight + TEXH * FRACUNIT;
            rw_midtexturemid = vtop - viewz;    /* bottom of texture at bottom */
        } else
            rw_midtexturemid = worldtop;        /* top of texture at top */
        rw_midtexturemid += sidedef->rowoffset;
        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = screenheightarray;
        ds_p->sprbottomclip = negonearray;
        ds_p->bsilheight = INT32_MAX;
        ds_p->tsilheight = INT32_MIN;
    } else {
        /* two sided line */
        ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
        ds_p->silhouette = 0;
        if (frontsector->floorheight > backsector->floorheight) {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = frontsector->floorheight;
        } else if (backsector->floorheight > viewz) {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = INT32_MAX;
        }
        if (frontsector->ceilingheight < backsector->ceilingheight) {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = frontsector->ceilingheight;
        } else if (backsector->ceilingheight < viewz) {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = INT32_MIN;
        }
        if (backsector->ceilingheight <= frontsector->floorheight) {
            ds_p->sprbottomclip = negonearray;
            ds_p->bsilheight = INT32_MAX;
            ds_p->silhouette |= SIL_BOTTOM;
        }
        if (backsector->floorheight >= frontsector->ceilingheight) {
            ds_p->sprtopclip = screenheightarray;
            ds_p->tsilheight = INT32_MIN;
            ds_p->silhouette |= SIL_TOP;
        }
        worldhigh = backsector->ceilingheight - viewz;
        worldlow = backsector->floorheight - viewz;
        markfloor = worldlow != worldbottom || backsector->floorpic != frontsector->floorpic
                    || backsector->lightlevel != frontsector->lightlevel;
        markceiling = worldhigh != worldtop || backsector->ceilingpic != frontsector->ceilingpic
                      || backsector->lightlevel != frontsector->lightlevel;
        if (backsector->ceilingheight <= frontsector->floorheight
            || backsector->floorheight >= frontsector->ceilingheight)
            markceiling = markfloor = 1;    /* closed door */
        if (worldhigh < worldtop) {
            toptexture = sidedef->toptexture;   /* top texture */
            if (linedef->flags & ML_DONTPEGTOP)
                rw_toptexturemid = worldtop;    /* top of texture at top */
            else {
                vtop = backsector->ceilingheight + TEXH * FRACUNIT;
                rw_toptexturemid = vtop - viewz;    /* bottom of texture */
            }
        }
        if (worldlow > worldbottom) {
            bottomtexture = sidedef->bottomtexture; /* bottom texture */
            if (linedef->flags & ML_DONTPEGBOTTOM)
                rw_bottomtexturemid = worldtop;
            else
                rw_bottomtexturemid = worldlow;     /* top of texture at top */
        }
        rw_toptexturemid += sidedef->rowoffset;
        rw_bottomtexturemid += sidedef->rowoffset;
    }

    /* calculate rw_offset (only needed for textured lines) */
    segtextured = midtexture | toptexture | bottomtexture;
    if (segtextured) {
        offsetangle = rw_normalangle - rw_angle1;
        if (offsetangle > ANG180)
            offsetangle = -offsetangle;
        if (offsetangle > ANG90)
            offsetangle = ANG90;
        sineval = finesine[offsetangle >> ANGLETOFINESHIFT];
        rw_offset = FixedMul(hyp, sineval);
        if (rw_normalangle - rw_angle1 < ANG180)
            rw_offset = -rw_offset;
        rw_offset += sidedef->textureoffset + curline->offset;
        rw_centerangle = ANG90 + viewangle - rw_normalangle;

        /* calculate light table: different tables for horizontal / vertical / diagonal */
        lightnum = frontsector->lightlevel >> LIGHTSEGSHIFT;
        if (curline->v1->y == curline->v2->y)
            lightnum--;
        else if (curline->v1->x == curline->v2->x)
            lightnum++;
        if (lightnum < 0)
            walllights = scalelight(0);
        else if (lightnum >= LIGHTLEVELS)
            walllights = scalelight(LIGHTLEVELS - 1);
        else
            walllights = scalelight(lightnum);
    }

    /* a floor / ceiling plane on the wrong side of the view plane is invisible */
    if (frontsector->floorheight >= viewz)
        markfloor = 0;
    if (frontsector->ceilingheight <= viewz)
        markceiling = 0;

    /* calculate incremental stepping values for texture edges */
    worldtop >>= 4;
    worldbottom >>= 4;
    topstep = -FixedMul(rw_scalestep, worldtop);
    topfrac = (centeryfrac >> 4) - FixedMul(worldtop, rw_scale);
    bottomstep = -FixedMul(rw_scalestep, worldbottom);
    bottomfrac = (centeryfrac >> 4) - FixedMul(worldbottom, rw_scale);
    if (backsector) {
        worldhigh >>= 4;
        worldlow >>= 4;
        if (worldhigh < worldtop) {
            pixhigh = (centeryfrac >> 4) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(rw_scalestep, worldhigh);
        }
        if (worldlow > worldbottom) {
            pixlow = (centeryfrac >> 4) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(rw_scalestep, worldlow);
        }
    }

    /* render it */
    if (markceiling)
        ceilingplane = R_CheckPlane(ceilingplane, rw_x, rw_stopx - 1);
    if (markfloor)
        floorplane = R_CheckPlane(floorplane, rw_x, rw_stopx - 1);
    R_RenderSegLoop();

    /* save sprite clipping info */
    if ((ds_p->silhouette & SIL_TOP) && !ds_p->sprtopclip) {
        memcpy(lastopening, ceilingclip + start, sizeof(short) * (rw_stopx - start));
        ds_p->sprtopclip = lastopening - start;
        lastopening += rw_stopx - start;
    }
    if ((ds_p->silhouette & SIL_BOTTOM) && !ds_p->sprbottomclip) {
        memcpy(lastopening, floorclip + start, sizeof(short) * (rw_stopx - start));
        ds_p->sprbottomclip = lastopening - start;
        lastopening += rw_stopx - start;
    }
    ds_p++;
}

/* ---- r_bsp.c ---- */
void R_ClearDrawSegs(void)
{
    ds_p = drawsegs;
}

void R_ClipSolidWallSegment(int first, int last)
{
    cliprange_t *next, *start;

    /* find the first range that touches the range (adjacent pixels are touching) */
    start = solidsegs;
    while (start->last < first - 1)
        start++;
    if (first < start->first) {
        if (last < start->first - 1) {
            /* post is entirely visible (above start), so insert a new clippost */
            R_StoreWallRange(first, last);
            next = newend;
            newend++;
            while (next != start) {
                *next = *(next - 1);
                next--;
            }
            next->first = first;
            next->last = last;
            return;
        }
        R_StoreWallRange(first, start->first - 1);  /* a fragment above *start */
        start->first = first;
    }
    if (last <= start->last)                /* bottom contained in start? */
        return;
    next = start;
    while (last >= (next + 1)->first - 1) {
        R_StoreWallRange(next->last + 1, (next + 1)->first - 1);   /* a fragment between two posts */
        next++;
        if (last <= next->last) {
            start->last = next->last;       /* bottom is contained in next */
            goto crunch;
        }
    }
    R_StoreWallRange(next->last + 1, last); /* a fragment after *next */
    start->last = last;
crunch:
    if (next == start)
        return;                             /* post just extended past the bottom of one post */
    while (next++ != newend)
        *++start = *next;                   /* remove a post */
    newend = start + 1;
}

void R_ClipPassWallSegment(int first, int last)
{
    cliprange_t *start;

    start = solidsegs;
    while (start->last < first - 1)
        start++;
    if (first < start->first) {
        if (last < start->first - 1) {
            R_StoreWallRange(first, last);  /* post is entirely visible (above start) */
            return;
        }
        R_StoreWallRange(first, start->first - 1);
    }
    if (last <= start->last)
        return;
    while (last >= (start + 1)->first - 1) {
        R_StoreWallRange(start->last + 1, (start + 1)->first - 1);
        start++;
        if (last <= start->last)
            return;
    }
    R_StoreWallRange(start->last + 1, last);
}

/* Doom's sentinels are +-0x7fffffff; columns fit a 16-bit int */
void R_ClearClipSegs(void)
{
    solidsegs[0].first = -0x7fff;
    solidsegs[0].last = -1;
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fff;
    newend = solidsegs + 2;
}

void R_AddLine(const seg_t *line)
{
    int x1, x2;
    angle_t angle1, angle2, span, tspan;

    curline = line;
    angle1 = R_PointToAngle(line->v1->x, line->v1->y);
    angle2 = R_PointToAngle(line->v2->x, line->v2->y);
    span = angle1 - angle2;                 /* clip to view edges */
    if (span >= ANG180)                     /* back side? */
        return;
    rw_angle1 = angle1;                     /* global angle needed by segcalc */
    angle1 -= viewangle;
    angle2 -= viewangle;
    tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span)                  /* totally off the left edge? */
            return;
        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span)
            return;
        angle2 = -clipangle;
    }
    /* the seg is in the view range, but not necessarily visible */
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];
    if (x1 == x2)                           /* does not cross a pixel? */
        return;
    backsector = line->backsector;
    if (!backsector)                        /* single sided line? */
        goto clipsolid;
    if (backsector->ceilingheight <= frontsector->floorheight
        || backsector->floorheight >= frontsector->ceilingheight)
        goto clipsolid;                     /* closed door */
    if (backsector->ceilingheight != frontsector->ceilingheight
        || backsector->floorheight != frontsector->floorheight)
        goto clippass;                      /* window */
    /* reject empty lines used for triggers and special events */
    if (backsector->ceilingpic == frontsector->ceilingpic && backsector->floorpic == frontsector->floorpic
        && backsector->lightlevel == frontsector->lightlevel && curline->sidedef->midtexture == 0)
        return;
clippass:
    R_ClipPassWallSegment(x1, x2 - 1);
    return;
clipsolid:
    R_ClipSolidWallSegment(x1, x2 - 1);
}

static const signed char checkcoord[12][4] = {
    {3, 0, 2, 1}, {3, 0, 2, 0}, {3, 1, 2, 0}, {0}, {2, 0, 2, 1}, {0, 0, 0, 0},
    {3, 1, 3, 0}, {0}, {2, 0, 3, 1}, {2, 1, 3, 1}, {2, 1, 3, 0}};

int R_CheckBBox(const fixed_t *bspcoord)
{
    int boxx, boxy, boxpos, sx1, sx2;
    fixed_t x1, y1, x2, y2;
    angle_t angle1, angle2, span, tspan;
    cliprange_t *start;

    /* find the corners of the box that define the edges from the current viewpoint */
    if (viewx <= bspcoord[BOXLEFT])
        boxx = 0;
    else if (viewx < bspcoord[BOXRIGHT])
        boxx = 1;
    else
        boxx = 2;
    if (viewy >= bspcoord[BOXTOP])
        boxy = 0;
    else if (viewy > bspcoord[BOXBOTTOM])
        boxy = 1;
    else
        boxy = 2;
    boxpos = (boxy << 2) + boxx;
    if (boxpos == 5)
        return 1;
    x1 = bspcoord[checkcoord[boxpos][0]];
    y1 = bspcoord[checkcoord[boxpos][1]];
    x2 = bspcoord[checkcoord[boxpos][2]];
    y2 = bspcoord[checkcoord[boxpos][3]];

    /* check the clip list for an open space */
    angle1 = R_PointToAngle(x1, y1) - viewangle;
    angle2 = R_PointToAngle(x2, y2) - viewangle;
    span = angle1 - angle2;
    if (span >= ANG180)                     /* sitting on a line? */
        return 1;
    tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span)
            return 0;
        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;
        if (tspan >= span)
            return 0;
        angle2 = -clipangle;
    }
    /* find the first clippost that touches the source post */
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    sx1 = viewangletox[angle1];
    sx2 = viewangletox[angle2];
    if (sx1 == sx2)                         /* does not cross a pixel */
        return 0;
    sx2--;
    start = solidsegs;
    while (start->last < sx2)
        start++;
    if (sx1 >= start->first && sx2 <= start->last)
        return 0;                           /* the clippost contains the new span */
    return 1;
}

/* ---- r_things.c ---- */
void R_ClearSprites(void)
{
    vissprite_p = vissprites;
}

void R_ProjectSprite(const mobj_t *thing)
{
    fixed_t tr_x, tr_y, gxt, gyt, tx, tz, xscale, iscale;
    int x1, x2, index;
    unsigned rot;
    int flip;
    vissprite_t *vis;
    angle_t ang;

    /* transform the origin point */
    tr_x = thing->x - viewx;
    tr_y = thing->y - viewy;
    gxt = FixedMul(tr_x, viewcos);
    gyt = -FixedMul(tr_y, viewsin);
    tz = gxt - gyt;
    if (tz < MINZ)                          /* thing is behind the view plane? */
        return;
    xscale = FixedDiv(projection, tz);
    gxt = -FixedMul(tr_x, viewsin);
    gyt = FixedMul(tr_y, viewcos);
    tx = -(gyt + gxt);
    if (fabs32(tx) > (tz << 2))             /* too far off the side? */
        return;

    /* choose a rotation based on the player's view; one patch, mirrored for 5-7 */
    ang = R_PointToAngle(thing->x, thing->y);
    rot = (unsigned)((ang - thing->angle + (angle_t)(ANG45 / 2) * 9) >> 29);
    flip = rot >= 5;

    /* calculate the edges of the shape */
    tx -= (SPRW / 2) * FRACUNIT;
    x1 = (int)((centerxfrac + FixedMul(tx, xscale)) >> FRACBITS);
    if (x1 > viewwidth)                     /* off the right side? */
        return;
    tx += SPRW * FRACUNIT;
    x2 = (int)(((centerxfrac + FixedMul(tx, xscale)) >> FRACBITS) - 1);
    if (x2 < 0)                             /* off the left side? */
        return;
    if (vissprite_p == &vissprites[MAXVISSPRITES])
        return;

    /* store the information in a vissprite */
    vis = vissprite_p++;
    vis->scale = xscale;
    vis->gx = thing->x;
    vis->gy = thing->y;
    vis->gz = thing->z;
    vis->gzt = thing->z + SPRH * FRACUNIT;
    vis->texturemid = vis->gzt - viewz;
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= viewwidth ? viewwidth - 1 : x2;
    iscale = FixedDiv(FRACUNIT, xscale);
    vis->iscale = iscale;
    if (flip) {
        vis->startfrac = SPRW * FRACUNIT - 1;
        vis->xiscale = -iscale;
    } else {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }
    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    /* diminished light */
    index = (int)(xscale >> LIGHTSCALESHIFT);
    if (index >= MAXLIGHTSCALE)
        index = MAXLIGHTSCALE - 1;
    vis->colormap = CMAP(spritelights[index]);
}

void R_AddSprites(const sector_t *sec)
{
    const mobj_t *thing;
    int lightnum;

    /* a sector can be in several subsectors: add its things once */
    if (sectorvalid[sec->index] == validcount)
        return;
    sectorvalid[sec->index] = validcount;
    lightnum = sec->lightlevel >> LIGHTSEGSHIFT;
    spritelights = scalelight(lightnum >= LIGHTLEVELS ? LIGHTLEVELS - 1 : lightnum);
    for (thing = sec->thinglist; thing; thing = thing->snext)
        R_ProjectSprite(thing);
}

void R_DrawMaskedColumn(const byte *column)
{
    fixed_t topscreen, bottomscreen, basetexturemid;

    basetexturemid = dc_texturemid;
    for (; column[0] != 0xff;) {
        /* calculate unclipped screen coordinates for the post */
        topscreen = sprtopscreen + spryscale * column[0];
        bottomscreen = topscreen + spryscale * column[1];
        dc_yl = (int)((topscreen + FRACUNIT - 1) >> FRACBITS);
        dc_yh = (int)((bottomscreen - 1) >> FRACBITS);
        if (dc_yh >= mfloorclip[dc_x])
            dc_yh = mfloorclip[dc_x] - 1;
        if (dc_yl <= mceilingclip[dc_x])
            dc_yl = mceilingclip[dc_x] + 1;
        if (dc_yl <= dc_yh) {
            dc_source = column + 3;
            dc_texturemid = basetexturemid - ((fixed_t)column[0] << FRACBITS);
            R_DrawColumn();
        }
        column += column[1] + 4;
    }
    dc_texturemid = basetexturemid;
}

void R_DrawVisSprite(const vissprite_t *vis)
{
    int texturecolumn;
    fixed_t frac;

    dc_colormap = vis->colormap;
    dc_iscale = vis->iscale;
    dc_texturemid = vis->texturemid;
    frac = vis->startfrac;
    spryscale = vis->scale;
    sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
    for (dc_x = vis->x1; dc_x <= vis->x2; dc_x++, frac += vis->xiscale) {
        texturecolumn = (int)(frac >> FRACBITS);
#ifdef RANGECHECK
        if (texturecolumn < 0 || texturecolumn >= SPRW)
            I_Error("R_DrawSpriteRange: bad texturecolumn");
#endif
        R_DrawMaskedColumn(sprite_posts + sprite_columnofs[texturecolumn]);
    }
}

void R_DrawSprite(const vissprite_t *spr)
{
    drawseg_t *ds;
    int x, r1, r2, silhouette;
    fixed_t scale, lowscale;

    for (x = spr->x1; x <= spr->x2; x++)
        clipbot[x] = cliptop[x] = -2;

    /* scan drawsegs from end to start for obscuring segs: the first
     * drawseg that has a greater scale is the clip seg */
    for (ds = ds_p - 1; ds >= drawsegs; ds--) {
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 || !ds->silhouette)
            continue;                       /* does not cover the sprite */
        r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;
        if (ds->scale1 > ds->scale2) {
            lowscale = ds->scale2;
            scale = ds->scale1;
        } else {
            lowscale = ds->scale1;
            scale = ds->scale2;
        }
        if (scale < spr->scale || (lowscale < spr->scale && !R_PointOnSegSide(spr->gx, spr->gy, ds->curline)))
            continue;                       /* the seg is behind the sprite */
        /* clip this piece of the sprite */
        silhouette = ds->silhouette;
        if (spr->gz >= ds->bsilheight)
            silhouette &= ~SIL_BOTTOM;
        if (spr->gzt <= ds->tsilheight)
            silhouette &= ~SIL_TOP;
        if (silhouette == 1) {
            for (x = r1; x <= r2; x++)
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
        } else if (silhouette == 2) {
            for (x = r1; x <= r2; x++)
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
        } else if (silhouette == 3) {
            for (x = r1; x <= r2; x++) {
                if (clipbot[x] == -2)
                    clipbot[x] = ds->sprbottomclip[x];
                if (cliptop[x] == -2)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }
    }
    /* all clipping has been performed, so draw the sprite: check for unclipped columns */
    for (x = spr->x1; x <= spr->x2; x++) {
        if (clipbot[x] == -2)
            clipbot[x] = viewheight;
        if (cliptop[x] == -2)
            cliptop[x] = -1;
    }
    mfloorclip = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite(spr);
}

/* sprites back to front (R_SortVisSprites), then masked midtextures (none here) */
void R_DrawMasked(void)
{
    vissprite_t *a, *b, t;

    for (a = vissprites; a < vissprite_p; a++)
        for (b = a + 1; b < vissprite_p; b++)
            if (b->scale < a->scale) {
                t = *a;
                *a = *b;
                *b = t;
            }
    for (a = vissprites; a < vissprite_p; a++)
        R_DrawSprite(a);
}

/* ---- r_bsp.c: the walk ---- */
void R_Subsector(unsigned num)
{
    int count;
    const seg_t *line;
    const subsector_t *sub;

#ifdef RANGECHECK
    if (num >= NUMSUBSECTORS)
        I_Error("R_Subsector");
#endif
    sub = &subsectors[num];
    frontsector = sub->sector;
    count = sub->numlines;
    line = &segs[sub->firstline];
    if (frontsector->floorheight < viewz)
        floorplane = R_FindPlane(frontsector->floorheight, frontsector->floorpic, frontsector->lightlevel);
    else
        floorplane = NULL;
    if (frontsector->ceilingheight > viewz)
        ceilingplane = R_FindPlane(frontsector->ceilingheight, frontsector->ceilingpic, frontsector->lightlevel);
    else
        ceilingplane = NULL;
    R_AddSprites(frontsector);
    while (count--) {
        R_AddLine(line);
        line++;
    }
}

void R_RenderBSPNode(unsigned bspnum)
{
    const node_t *bsp;
    int side;

    if (bspnum & NF_SUBSECTOR) {            /* found a subsector? */
        R_Subsector(bspnum & ~NF_SUBSECTOR);
        return;
    }
    bsp = &nodes[bspnum];
    side = R_PointOnSide(viewx, viewy, bsp);    /* which side the view point is on */
    R_RenderBSPNode(bsp->children[side]);       /* recursively divide front space */
    if (R_CheckBBox(bsp->bbox[side ^ 1]))       /* possibly divide back space */
        R_RenderBSPNode(bsp->children[side ^ 1]);
}

/* ---- r_main.c: the frame ---- */
void R_SetupFrame(const struct view *v)
{
    viewx = v->x;
    viewy = v->y;
    viewangle = v->angle;
    viewz = R_PointInSubsector(viewx, viewy)->sector->floorheight + VIEWHEIGHT;
    viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];
    validcount++;
}

void R_RenderPlayerView(const struct view *v)
{
    R_SetupFrame(v);
    R_ClearClipSegs();
    R_ClearDrawSegs();
    R_ClearPlanes();
    R_ClearSprites();
    R_RenderBSPNode(ROOTNODE);
    R_DrawPlanes();
    R_DrawMasked();
}

/* the start-up work Doom does in R_Init that gen_data.py could not: pointers */
void R_Init(void)
{
    int i;

    for (i = 0; i < SCREENHEIGHT; i++)
        ylookup[i] = VRAM + i * SCREENWIDTH;
    for (i = 0; i < SCREENWIDTH; i++) {
        columnofs[i] = i;
        negonearray[i] = -1;
        screenheightarray[i] = viewheight;
    }
    clipangle = xtoviewangle[0];
    memset(VRAM, 0, SCREENWIDTH * SCREENHEIGHT);
}

#ifdef CHECKSUM
/* for simulators without a log: print an FNV-1a hash of each frame, to
 * compare with the reference frames (not for timing) */
static void put_hash(void)
{
    uint32_t h = 2166136261u;
    unsigned i;
    int b;

    for (i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
        h = (h ^ VRAM[i]) * 16777619u;
    for (b = 28; b >= 0; b -= 4)
        putchar("0123456789abcdef"[(h >> b) & 15]);
    putchar('\n');
}
#endif

#ifdef REFERENCE
/* the development machine's build writes each frame as frameN.raw (the
 * 8-bit pixels, as video memory holds them) and frameN.ppm */
static void write_frame(int f)
{
    char name[32];
    FILE *fp;
    int i;

    sprintf(name, "frame%d.raw", f);
    fp = fopen(name, "wb");
    fwrite(VRAM, 1, SCREENWIDTH * SCREENHEIGHT, fp);
    fclose(fp);
    sprintf(name, "frame%d.ppm", f);
    fp = fopen(name, "wb");
    fprintf(fp, "P6\n%d %d\n255\n", SCREENWIDTH, SCREENHEIGHT);
    for (i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
        fwrite(&palette[VRAM[i] * 3], 1, 3, fp);
    fclose(fp);
}
#endif

int main(void)
{
    int f;

    R_Init();
    frame_done = 0;                         /* the end of start-up */
    for (f = 0; f < FRAMES; f++) {
        R_RenderPlayerView(&views[f]);
        frame_done = (unsigned char)(f + 1);
#ifdef CHECKSUM
        put_hash();
#endif
#ifdef REFERENCE
        write_frame(f);
#endif
    }
    return 0;
}
