/* fpga_cpu simulator: the high tier's video extensions and blitter (docs/io.md,
 * "Video extensions", "Blitter"). The registers are here; video.c draws the
 * layers. A blit is done at once when BLT_CMD is written (BLT_STAT never reads
 * busy), the effect the hardware has once it finishes. */
#include "cpu.h"

extern int video_hicol8;
uint64_t vidx_flip_frame;              /* the frame a buffer write takes effect in */

static uint32_t r32(const uint16_t *r, int lo) { return r[lo] | (uint32_t)r[lo + 1] << 16; }

int vidx_flip_pending(const cpu_t *c) { return (uint64_t)dev_frame(c->st.cycles) < vidx_flip_frame; }

/* a buffer register was written (VID_BMP, VID_SPR, VID_CTRL bit 4, VID_BMPB, VID_LSCR) */
void vidx_buffer_written(const cpu_t *c) { vidx_flip_frame = (uint64_t)dev_frame(c->st.cycles) + 1; }

static void blit(cpu_t *c)
{
    uint16_t *b = c->dev.blt;
    uint32_t src = r32(b, BL_SRC_LO) & 0xFFFFFF, dst = r32(b, BL_DST_LO) & 0xFFFFFF;
    int32_t ss = (int16_t)b[BL_SSTRIDE], ds = (int16_t)b[BL_DSTRIDE];
    uint32_t w = b[BL_W] & 0xFFF, h = b[BL_H];
    uint8_t key = (uint8_t)b[BL_KEY], fill = (uint8_t)b[BL_FILL];
    int is_fill = b[BL_CMD] & 1, keyed = (b[BL_CMD] >> 1) & 1;
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t d = (dst + x) & 0xFFFFFF;
            uint8_t v;
            if (is_fill)
                v = fill;
            else {
                uint32_t s = (src + x) & 0xFFFFFF;
                v = s < c->ram_size ? c->sram[s] : 0;      /* all of RAM, as the Tang's blitter */
                if (keyed && v == key)
                    continue;
            }
            if (d < c->ram_size)
                c->sram[d] = v;
        }
        src += (uint32_t)ss;
        dst += (uint32_t)ds;
    }
}

uint16_t vidx_read(cpu_t *c, uint32_t a)
{
    if (!video_hicol8)
        return 0;
    if (a >= IO_VIDX && a < IO_VIDX + 2 * VX__COUNT) {
        int r = (int)(a - IO_VIDX) / 2;
        switch (r) {
        case VX_STAT: {
            uint32_t line = dev_line(c->st.cycles);
            return (uint16_t)(vidx_flip_pending(c) | (line >= VISIBLE_LINES) << 1);
        }
        case VX_SPR_OVER: return 0;         /* the drawing below has no line limit */
        default: return 0;                  /* the rest are write-only */
        }
    }
    return 0;                               /* the blitter: BLT_STAT reads 0 (never busy), the rest are write-only */
}

/* 1 if the address was one of ours */
int vidx_write(cpu_t *c, uint32_t a, uint16_t v)
{
    iodev_t *d = &c->dev;
    if (a >= IO_VIDX && a < IO_VIDX + 2 * VX__COUNT) {
        int r = (int)(a - IO_VIDX) / 2;
        if (!video_hicol8)
            return 1;
        switch (r) {
        case VX_STAT: case VX_SPR_OVER: break;
        case VX_SPR_DATA:
            d->spr_attr[d->vidx[VX_SPR_INDEX] & 0xFF] = v;
            d->vidx[VX_SPR_INDEX] = (uint16_t)((d->vidx[VX_SPR_INDEX] + 1) & 0xFF);
            break;
        case VX_BMPB_LO: case VX_BMPB_HI: case VX_LSCR_LO: case VX_LSCR_HI:
            vidx_buffer_written(c);
            d->vidx[r] = v;
            break;
        default: d->vidx[r] = v;
        }
        return 1;
    }
    if (a >= IO_BLT && a < IO_BLT + 2 * BL__COUNT) {
        int r = (int)(a - IO_BLT) / 2;
        if (!video_hicol8)
            return 1;
        if (r == BL_CMD) {
            d->blt[r] = v;
            blit(c);
        } else if (r != BL_STAT)
            d->blt[r] = v;
        return 1;
    }
    return 0;
}
