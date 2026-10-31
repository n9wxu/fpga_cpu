; R_DrawColumn and R_DrawSpan by hand, for Doom built with a 32-bit int
; (fcc --int32): the same results as the C in src/r_draw.c (doomgeneric),
; RANGECHECK included. Also their low-detail versions (R_DrawColumnLow,
; R_DrawSpanLow: each texel two pixels wide), which write both pixels with
; one STW of the color times 0x0101.
;
; What the hand version does that the compiler doesn't (yet):
; - the colormap and texture pointers stay in registers (in C a byte store
;   could change them, so they are reloaded for every pixel);
; - the count is 16 bits (at most 320), one register and a DEC/BNE;
; - the next texel index is computed while the loads complete, so no
;   instruction waits for a load (the load-use stall);
; - the span's destination uses STB's post-increment.
; Inner loops: 8 instructions per column pixel, 10 per span pixel; low
; detail 9 per column pixel pair, 10 per span pixel pair.
;
; In variant A an absolute load ([symbol]) goes through p0, so r0/r1 hold
; nothing across one.

        .ifdef DOOM_640x480
SCREENWIDTH = 640
SCREENHEIGHT = 480
        .else
        .ifdef DOOM_HIRES
SCREENWIDTH = 640
SCREENHEIGHT = 350
        .else
SCREENWIDTH = 320
SCREENHEIGHT = 200
        .endif
        .endif

        .text

; ---- R_DrawColumn ----
; frac p1, fracstep p2, dc_source p3, dc_colormap p4, dest p5,
; count r13, texel/color r1, next index r12
        .globl  R_DrawColumn
R_DrawColumn:
        LDP     p1, [dc_yl]
        LDP     p2, [dc_yh]
        SUB     r0, r4, r2              ; count = dc_yh - dc_yl
        SBC     r1, r5, r3
        CMPI    r1, 0
        BLT     .Lcol_none              ; count < 0: nothing to draw
        ; RANGECHECK: (unsigned)dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT
        CMPI    r3, 0
        BLT     .Lcol_bad
        CMPI    r5, 0
        BNE     .Lcol_bad               ; dc_yh >= 65536 (or negative: dc_yl was too)
        CMPI    r4, SCREENHEIGHT
        BHS     .Lcol_bad
        LDP     p3, [dc_x]
        CMPI    r7, 0
        BNE     .Lcol_bad
        CMPI    r6, SCREENWIDTH
        BHS     .Lcol_bad
        PUSHP   p4
        PUSHP   p5
        PUSHP   lr
        SUB     r13, r4, r2             ; count (dc_yl, dc_yh < 200)
        ; dest = ylookup[dc_yl] + columnofs[dc_x]
        LEA     p4, [ylookup]
        LDP     p5, [p4 + r2*4]
        LEA     p4, [columnofs]
        LDP     p4, [p4 + r6*4]
        ADDPP   p5, p5, p4
        ; frac = dc_texturemid + (dc_yl - centery) * dc_iscale, mod 2^32.
        ; d = dc_yl - centery is a small signed number: d * s = dl*s + (dh*sl << 16)
        LDP     p2, [dc_iscale]         ; fracstep
        LDP     p3, [centery]
        SUB     r0, r2, r6              ; d, low half (dl)
        SBC     r12, r3, r7             ; d, high half (dh)
        MUL     r2, r0, r4              ; dl*sl, low
        MULHU   r3, r0, r4              ; dl*sl, high
        MUL     r6, r0, r5              ; dl*sh
        ADD     r3, r3, r6
        MUL     r6, r12, r4             ; dh*sl
        ADD     r3, r3, r6
        LDP     p3, [dc_texturemid]
        ADDPP   p1, p1, p3              ; frac
        LDP     p3, [dc_source]
        LDP     p4, [dc_colormap]
        ADDI    r13, r13, 1             ; count + 1 pixels (do ... while (count--))
        ANDI    r12, r3, 127            ; first texel index
.Lcol_loop:
        LDBU    r1, [p3 + r12.z*1]      ; texel
        ADDPP   p1, p1, p2              ; frac += fracstep
        ANDI    r12, r3, 127            ; next index, while the load completes
        LDBU    r1, [p4 + r1.z*1]       ; colormap[texel]
        ADDI    r13, r13, -1
        STB     r1, [p5]
        ADDPI   p5, p5, SCREENWIDTH
        BNE     .Lcol_loop
        POPP    lr
        POPP    p5
        POPP    p4
.Lcol_none:
        RET
.Lcol_bad:
        LDP     p3, [dc_x]
        ADDPI   sp, sp, -16
        LEA     p0, [.Lcol_msg]
        STP     p0, [sp]
        STP     p1, [sp + 4]
        STP     p2, [sp + 8]
        STP     p3, [sp + 12]
        JAL     I_Error                 ; does not return

; ---- R_DrawSpan ----
; position p1 (x part high, y part low), step p2, ds_source p3,
; ds_colormap p4, dest p5, count r13, texel/color r1, spot r12, x part r0
        .globl  R_DrawSpan
R_DrawSpan:
        LDP     p1, [ds_x1]
        LDP     p2, [ds_x2]
        ; RANGECHECK: ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        ;             || (unsigned)ds_y > SCREENHEIGHT
        SUB     r0, r4, r2              ; count = ds_x2 - ds_x1
        SBC     r1, r5, r3
        CMPI    r1, 0
        BLT     .Lspan_bad
        CMPI    r3, 0
        BLT     .Lspan_bad
        CMPI    r5, 0
        BNE     .Lspan_bad
        CMPI    r4, SCREENWIDTH
        BHS     .Lspan_bad
        LDP     p3, [ds_y]
        CMPI    r7, 0
        BNE     .Lspan_bad
        CMPI    r6, SCREENHEIGHT + 1
        BHS     .Lspan_bad
        PUSHP   p4
        PUSHP   p5
        PUSHP   lr
        SUB     r13, r4, r2             ; count (ds_x1, ds_x2 < 320)
        ; dest = ylookup[ds_y] + columnofs[ds_x1]
        LEA     p4, [ylookup]
        LDP     p5, [p4 + r6*4]
        LEA     p4, [columnofs]
        LDP     p4, [p4 + r2*4]
        ADDPP   p5, p5, p4
        ; position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6) & 0xffff)
        ; high half: bits 6..21 of ds_xfrac; low half: bits 6..21 of ds_yfrac
        LDP     p2, [ds_xfrac]
        SHRI    r1, r4, 6
        SHLI    r12, r5, 10
        OR      r3, r1, r12
        LDP     p2, [ds_yfrac]
        SHRI    r1, r4, 6
        SHLI    r12, r5, 10
        OR      r2, r1, r12
        ; step the same way from ds_xstep, ds_ystep
        LDP     p3, [ds_xstep]
        SHRI    r1, r6, 6
        SHLI    r12, r7, 10
        OR      r5, r1, r12
        LDP     p3, [ds_ystep]
        SHRI    r1, r6, 6
        SHLI    r12, r7, 10
        OR      r4, r1, r12
        LDP     p3, [ds_source]
        LDP     p4, [ds_colormap]
        ADDI    r13, r13, 1             ; count + 1 pixels
.Lspan_loop:
        ; spot = ((position >> 4) & 0x0fc0) | (position >> 26)
        SHRI    r12, r2, 4
        ANDI    r12, r12, 0x0fc0
        SHRI    r0, r3, 10
        OR      r12, r12, r0
        LDBU    r1, [p3 + r12.z*1]      ; flat texel
        ADDPP   p1, p1, p2              ; position += step, while the load completes
        LDBU    r1, [p4 + r1.z*1]       ; colormap[texel]
        ADDI    r13, r13, -1
        STB     r1, [p5]+
        BNE     .Lspan_loop
        POPP    lr
        POPP    p5
        POPP    p4
        RET
.Lspan_bad:
        LDP     p3, [ds_y]
        ADDPI   sp, sp, -16
        LEA     p0, [.Lspan_msg]
        STP     p0, [sp]
        STP     p1, [sp + 4]
        STP     p2, [sp + 8]
        STP     p3, [sp + 12]
        JAL     I_Error                 ; does not return

; ---- R_DrawColumnLow ----
; As R_DrawColumn, at dest = ylookup[dc_yl] + columnofs[dc_x << 1], two
; pixels wide; 0x0101 in r0.
        .globl  R_DrawColumnLow
R_DrawColumnLow:
        LDP     p1, [dc_yl]
        LDP     p2, [dc_yh]
        SUB     r0, r4, r2              ; count = dc_yh - dc_yl
        SBC     r1, r5, r3
        CMPI    r1, 0
        BLT     .Lcl_none
        CMPI    r3, 0
        BLT     .Lcol_bad
        CMPI    r5, 0
        BNE     .Lcol_bad
        CMPI    r4, SCREENHEIGHT
        BHS     .Lcol_bad
        LDP     p3, [dc_x]
        CMPI    r7, 0
        BNE     .Lcol_bad
        CMPI    r6, SCREENWIDTH
        BHS     .Lcol_bad
        PUSHP   p4
        PUSHP   p5
        PUSHP   lr
        SUB     r13, r4, r2             ; count
        LEA     p4, [ylookup]
        LDP     p5, [p4 + r2*4]
        ADD     r6, r6, r6              ; dc_x << 1
        LEA     p4, [columnofs]
        LDP     p4, [p4 + r6*4]
        ADDPP   p5, p5, p4
        LDP     p2, [dc_iscale]         ; frac, as in R_DrawColumn
        LDP     p3, [centery]
        SUB     r0, r2, r6
        SBC     r12, r3, r7
        MUL     r2, r0, r4
        MULHU   r3, r0, r4
        MUL     r6, r0, r5
        ADD     r3, r3, r6
        MUL     r6, r12, r4
        ADD     r3, r3, r6
        LDP     p3, [dc_texturemid]
        ADDPP   p1, p1, p3
        LDP     p3, [dc_source]
        LDP     p4, [dc_colormap]
        MOVI    r0, 0x0101
        ADDI    r13, r13, 1             ; count + 1 pixel pairs
        ANDI    r12, r3, 127
        ANDI    r1, r10, 1
        BNE     .Lcl_odd                ; dest odd: no STW (never, in practice)
.Lcl_loop:
        LDBU    r1, [p3 + r12.z*1]      ; texel
        ADDPP   p1, p1, p2
        ANDI    r12, r3, 127
        LDBU    r1, [p4 + r1.z*1]       ; colormap[texel]
        ADDI    r13, r13, -1
        MUL     r1, r1, r0              ; both pixels (MUL sets no flags)
        STW     r1, [p5]
        ADDPI   p5, p5, SCREENWIDTH
        BNE     .Lcl_loop
.Lcl_done:
        POPP    lr
        POPP    p5
        POPP    p4
.Lcl_none:
        RET
.Lcl_odd:
        LDBU    r1, [p3 + r12.z*1]
        ADDPP   p1, p1, p2
        ANDI    r12, r3, 127
        LDBU    r1, [p4 + r1.z*1]
        ADDI    r13, r13, -1
        STB     r1, [p5]
        STB     r1, [p5 + 1]
        ADDPI   p5, p5, SCREENWIDTH
        BNE     .Lcl_odd
        BRA     .Lcl_done

; ---- R_DrawSpanLow ----
; As R_DrawSpan, two pixels per texel; like the C it leaves ds_x1 and
; ds_x2 doubled. 0x0101 in r0; the spot's x part goes through r1.
        .globl  R_DrawSpanLow
R_DrawSpanLow:
        LDP     p1, [ds_x1]
        LDP     p2, [ds_x2]
        SUB     r0, r4, r2              ; count = ds_x2 - ds_x1
        SBC     r1, r5, r3
        CMPI    r1, 0
        BLT     .Lspan_bad
        CMPI    r3, 0
        BLT     .Lspan_bad
        CMPI    r5, 0
        BNE     .Lspan_bad
        CMPI    r4, SCREENWIDTH
        BHS     .Lspan_bad
        LDP     p3, [ds_y]
        CMPI    r7, 0
        BNE     .Lspan_bad
        CMPI    r6, SCREENHEIGHT + 1
        BHS     .Lspan_bad
        PUSHP   p4
        PUSHP   p5
        PUSHP   lr
        SUB     r13, r4, r2             ; count
        ADD     r2, r2, r2              ; ds_x1 <<= 1
        ADD     r4, r4, r4              ; ds_x2 <<= 1 (both < 640: high halves 0)
        STP     p1, [ds_x1]
        STP     p2, [ds_x2]
        LEA     p4, [ylookup]
        LDP     p5, [p4 + r6*4]
        LEA     p4, [columnofs]
        LDP     p4, [p4 + r2*4]
        ADDPP   p5, p5, p4
        LDP     p2, [ds_xfrac]          ; position and step, as in R_DrawSpan
        SHRI    r1, r4, 6
        SHLI    r12, r5, 10
        OR      r3, r1, r12
        LDP     p2, [ds_yfrac]
        SHRI    r1, r4, 6
        SHLI    r12, r5, 10
        OR      r2, r1, r12
        LDP     p3, [ds_xstep]
        SHRI    r1, r6, 6
        SHLI    r12, r7, 10
        OR      r5, r1, r12
        LDP     p3, [ds_ystep]
        SHRI    r1, r6, 6
        SHLI    r12, r7, 10
        OR      r4, r1, r12
        LDP     p3, [ds_source]
        LDP     p4, [ds_colormap]
        MOVI    r0, 0x0101
        ADDI    r13, r13, 1             ; count + 1 texels
        ANDI    r1, r10, 1
        BNE     .Lsl_odd
.Lsl_loop:
        SHRI    r12, r2, 4
        ANDI    r12, r12, 0x0fc0
        SHRI    r1, r3, 10
        OR      r12, r12, r1
        LDBU    r1, [p3 + r12.z*1]      ; flat texel
        ADDPP   p1, p1, p2
        LDBU    r1, [p4 + r1.z*1]       ; colormap[texel]
        ADDI    r13, r13, -1
        MUL     r1, r1, r0
        STW     r1, [p5]+
        BNE     .Lsl_loop
.Lsl_done:
        POPP    lr
        POPP    p5
        POPP    p4
        RET
.Lsl_odd:
        SHRI    r12, r2, 4
        ANDI    r12, r12, 0x0fc0
        SHRI    r1, r3, 10
        OR      r12, r12, r1
        LDBU    r1, [p3 + r12.z*1]
        ADDPP   p1, p1, p2
        LDBU    r1, [p4 + r1.z*1]
        ADDI    r13, r13, -1
        STB     r1, [p5]+
        STB     r1, [p5]+
        BNE     .Lsl_odd
        BRA     .Lsl_done

        .section .rodata
.Lcol_msg:
        .asciz  "R_DrawColumn: %i to %i at %i"
.Lspan_msg:
        .asciz  "R_DrawSpan: %i to %i at %i"
