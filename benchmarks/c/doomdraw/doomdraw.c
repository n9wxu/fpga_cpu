/* Doom's two pixel loops on their own: R_DrawColumn (walls, sprites) and
 * R_DrawSpan (floors, ceilings), copied verbatim from
 * benchmarks/doom/src/r_draw.c (doomgeneric, GPL v2), RANGECHECK included
 * as Doom builds them. A driver feeds them a frame-like mix: wall columns
 * of every height and texture step, and floor spans of every length.
 *
 * Build with a 32-bit int (fcc --int32), like Doom. -DDRAW_ASM links the
 * hand-written versions (../../doom/port/r_draw_fcpu.s) instead of the C ones.
 *
 * Prints a checksum of the screen after each frame (compared with the
 * host build) and, on fpga_cpu, cycles per pixel for each loop. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __FCPU__
#include <fcpu.h>
#endif

#ifndef FRAMES
#define FRAMES 8
#endif

#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#define FRACBITS     16
#define FRACUNIT     (1 << FRACBITS)
#define RANGECHECK

typedef int fixed_t;
typedef unsigned char byte;
typedef byte lighttable_t;

byte *ylookup[SCREENHEIGHT];
int columnofs[SCREENWIDTH];
byte screen[SCREENWIDTH * SCREENHEIGHT];

lighttable_t *dc_colormap;
int dc_x, dc_yl, dc_yh;
fixed_t dc_iscale, dc_texturemid;
byte *dc_source;
int centery = 84;

int ds_y, ds_x1, ds_x2;
lighttable_t *ds_colormap;
fixed_t ds_xfrac, ds_yfrac, ds_xstep, ds_ystep;
byte *ds_source;

void I_Error(const char *fmt, ...)
{
    printf("I_Error: %s\n", fmt);
    exit(1);
}

#ifdef DRAW_ASM
void R_DrawColumn(void);
void R_DrawSpan(void);
#else
/* ---- from r_draw.c ---- */
void R_DrawColumn (void) 
{ 
    int			count; 
    byte*		dest; 
    fixed_t		frac;
    fixed_t		fracstep;	 
 
    count = dc_yh - dc_yl; 

    // Zero length, column does not exceed a pixel.
    if (count < 0) 
	return; 
				 
#ifdef RANGECHECK 
    if ((unsigned)dc_x >= SCREENWIDTH
	|| dc_yl < 0
	|| dc_yh >= SCREENHEIGHT) 
	I_Error ("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x); 
#endif 

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows? 
    dest = ylookup[dc_yl] + columnofs[dc_x];  

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do 
    {
	// Re-map color indices from wall texture column
	//  using a lighting/special effects LUT.
	*dest = dc_colormap[dc_source[(frac>>FRACBITS)&127]];
	
	dest += SCREENWIDTH; 
	frac += fracstep;
	
    } while (count--); 
} 

void R_DrawSpan (void) 
{ 
    unsigned int position, step;
    byte *dest;
    int count;
    int spot;
    unsigned int xtemp, ytemp;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1
	|| ds_x1<0
	|| ds_x2>=SCREENWIDTH
	|| (unsigned)ds_y>SCREENHEIGHT)
    {
	I_Error( "R_DrawSpan: %i to %i at %i",
		 ds_x1,ds_x2,ds_y);
    }
//	dscount++;
#endif

    // Pack position and step variables into a single 32-bit integer,
    // with x in the top 16 bits and y in the bottom 16 bits.  For
    // each 16-bit part, the top 6 bits are the integer part and the
    // bottom 10 bits are the fractional part of the pixel position.

    position = ((ds_xfrac << 10) & 0xffff0000)
             | ((ds_yfrac >> 6)  & 0x0000ffff);
    step = ((ds_xstep << 10) & 0xffff0000)
         | ((ds_ystep >> 6)  & 0x0000ffff);

    dest = ylookup[ds_y] + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

    do
    {
	// Calculate current texture index in u,v.
        ytemp = (position >> 4) & 0x0fc0;
        xtemp = (position >> 26);
        spot = xtemp | ytemp;

	// Lookup pixel from flat texture tile,
	//  re-index using light/colormap.
	*dest++ = ds_colormap[ds_source[spot]];

        position += step;

    } while (count--);
}
/* ---- end of r_draw.c ---- */
#endif

static uint32_t rs = 1;
static uint32_t rnd(void) { rs = rs * 1103515245u + 12345u; return rs >> 8; }

static byte texture[64 * 128];     /* 64 columns of 128 texels */
static byte flat[64 * 64];
static lighttable_t colormap[256];

static uint32_t cycles(void)
{
#ifdef __FCPU__
    uint16_t hi, lo;
    do { hi = IO16(0xF00006); lo = IO16(0xF00004); } while (IO16(0xF00006) != hi);
    return (uint32_t)hi << 16 | lo;
#else
    return 0;
#endif
}

int main(void)
{
    uint32_t colpix = 0, spanpix = 0, colcyc = 0, spancyc = 0, t;
    int f, i;
    for (i = 0; i < SCREENHEIGHT; i++) ylookup[i] = screen + i * SCREENWIDTH;
    for (i = 0; i < SCREENWIDTH; i++) columnofs[i] = i;
    for (i = 0; i < (int)sizeof texture; i++) texture[i] = (byte)rnd();
    for (i = 0; i < (int)sizeof flat; i++) flat[i] = (byte)rnd();
    for (i = 0; i < 256; i++) colormap[i] = (byte)(i * 7 + 3);
    dc_colormap = ds_colormap = colormap;
    ds_source = flat;
    for (f = 0; f < FRAMES; f++) {
        static struct { int yl, yh; fixed_t iscale, mid; byte *src; } col[SCREENWIDTH];
        static struct { int x1, x2; fixed_t xf, yf, xs, ys; } span[SCREENHEIGHT / 2];
        uint32_t h = 2166136261u;
        /* the frame's parameters first, so only the draw calls are timed:
         * walls, one column per screen x, of every height and texture step */
        for (i = 0; i < SCREENWIDTH; i++) {
            int height = 2 + (int)(rnd() % 160), yl = 84 - height / 2 + (int)(rnd() % 16) - 8;
            if (yl < 0) yl = 0;
            col[i].yl = yl;
            col[i].yh = yl + height >= SCREENHEIGHT ? SCREENHEIGHT - 1 : yl + height;
            col[i].iscale = (fixed_t)(0x2000 + rnd() % 0x40000);
            col[i].mid = (fixed_t)(rnd() % (128 * FRACUNIT));
            col[i].src = texture + (rnd() % 64) * 128;
        }
        /* floor and ceiling: spans of every length */
        for (i = 0; i < SCREENHEIGHT / 2; i++) {
            span[i].x1 = (int)(rnd() % 160);
            span[i].x2 = span[i].x1 + (int)(rnd() % 160);
            span[i].xf = (fixed_t)rnd();
            span[i].yf = (fixed_t)rnd();
            span[i].xs = (fixed_t)(rnd() % 0x30000) - 0x18000;
            span[i].ys = (fixed_t)(rnd() % 0x30000) - 0x18000;
        }
        t = cycles();
        for (dc_x = 0; dc_x < SCREENWIDTH; dc_x++) {
            dc_yl = col[dc_x].yl;
            dc_yh = col[dc_x].yh;
            dc_iscale = col[dc_x].iscale;
            dc_texturemid = col[dc_x].mid;
            dc_source = col[dc_x].src;
            R_DrawColumn();
        }
        colcyc += cycles() - t;
        for (i = 0; i < SCREENWIDTH; i++)
            colpix += (uint32_t)(col[i].yh - col[i].yl + 1);
        t = cycles();
        for (i = 0; i < SCREENHEIGHT / 2; i++) {
            ds_y = 2 * i;
            ds_x1 = span[i].x1;
            ds_x2 = span[i].x2;
            ds_xfrac = span[i].xf;
            ds_yfrac = span[i].yf;
            ds_xstep = span[i].xs;
            ds_ystep = span[i].ys;
            R_DrawSpan();
        }
        spancyc += cycles() - t;
        for (i = 0; i < SCREENHEIGHT / 2; i++)
            spanpix += (uint32_t)(span[i].x2 - span[i].x1 + 1);
        for (i = 0; i < (int)sizeof screen; i++)
            h = (h ^ screen[i]) * 16777619u;
        printf("frame %d: %08lx\n", f, (unsigned long)h);
    }
    printf("column pixels: %lu\nspan pixels: %lu\n", (unsigned long)colpix, (unsigned long)spanpix);
#ifdef __FCPU__
    printf("column cycles/pixel: %lu.%02lu\n", (unsigned long)(colcyc / colpix),
           (unsigned long)(colcyc % colpix * 100 / colpix));
    printf("span cycles/pixel: %lu.%02lu\n", (unsigned long)(spancyc / spanpix),
           (unsigned long)(spancyc % spanpix * 100 / spanpix));
#endif
    return 0;
}
