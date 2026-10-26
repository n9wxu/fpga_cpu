; Bresenham lines into a 640x360 8bpp framebuffer, then a word checksum
; of the whole frame. Pointer stepping with ADDP (pair + signed register),
; the drawing engine's job done in software.
; expect: "38975\n"

FB = 0x080000
W = 640
H = 360
NLINES = 126

        .org    0x1000
start:  LA      sp, 0x0F0000
        LA      p5, endpoints
        MOVI    r9, 0                   ; color
        LI      r12, NLINES
lnext:  LDW     r4, [p5]+               ; x1
        LDW     r5, [p5]+               ; y1
        LA      p0, FB + 180 * W + 320  ; start at the center
        ADDI    r4, r4, -320            ; dx
        MOVI    r6, 1                   ; x step
        TST     r4
        BGE     dxpos
        NEG     r4, r4
        MOVI    r6, -1
dxpos:  ADDI    r5, r5, -180            ; dy
        LI      r7, W                   ; y step (bytes)
        TST     r5
        BGE     dypos
        NEG     r5, r5
        LI      r7, -W
dypos:  MOV     r2, r4                  ; steps = max(|dx|, |dy|)
        CMP     r4, r5
        BGE     cnt_ok
        MOV     r2, r5
cnt_ok: NEG     r5, r5                  ; dy = -|dy|
        ADD     r3, r4, r5              ; err = dx + dy
        INC     r2
pix:    STB     r9, [p0]
        ADD     r8, r3, r3              ; e2 = 2 * err
        CMP     r8, r5
        BLT     nox
        ADD     r3, r3, r5
        ADDP    p0, p0, r6
nox:    CMP     r8, r4
        BGT     noy
        ADD     r3, r3, r4
        ADDP    p0, p0, r7
noy:    DEC     r2
        BNE     pix
        INC     r9
        DEC     r12
        BNE     lnext
        ; checksum the frame
        LA      p0, FB
        MOVI    r4, 0
        LI      r6, H
row:    LI      r5, W / 2
col:    LDW     r3, [p0]+
        ADD     r4, r4, r3
        DEC     r5
        BNE     col
        DEC     r6
        BNE     row
        MOV     r2, r4
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

endpoints:
        .word   0, 0, 16, 0, 32, 0, 48, 0, 64, 0, 80, 0
        .word   96, 0, 112, 0, 128, 0, 144, 0, 160, 0, 176, 0
        .word   192, 0, 208, 0, 224, 0, 240, 0, 256, 0, 272, 0
        .word   288, 0, 304, 0, 320, 0, 336, 0, 352, 0, 368, 0
        .word   384, 0, 400, 0, 416, 0, 432, 0, 448, 0, 464, 0
        .word   480, 0, 496, 0, 512, 0, 528, 0, 544, 0, 560, 0
        .word   576, 0, 592, 0, 608, 0, 624, 0, 0, 359, 16, 359
        .word   32, 359, 48, 359, 64, 359, 80, 359, 96, 359, 112, 359
        .word   128, 359, 144, 359, 160, 359, 176, 359, 192, 359, 208, 359
        .word   224, 359, 240, 359, 256, 359, 272, 359, 288, 359, 304, 359
        .word   320, 359, 336, 359, 352, 359, 368, 359, 384, 359, 400, 359
        .word   416, 359, 432, 359, 448, 359, 464, 359, 480, 359, 496, 359
        .word   512, 359, 528, 359, 544, 359, 560, 359, 576, 359, 592, 359
        .word   608, 359, 624, 359, 0, 0, 0, 16, 0, 32, 0, 48
        .word   0, 64, 0, 80, 0, 96, 0, 112, 0, 128, 0, 144
        .word   0, 160, 0, 176, 0, 192, 0, 208, 0, 224, 0, 240
        .word   0, 256, 0, 272, 0, 288, 0, 304, 0, 320, 0, 336
        .word   0, 352, 639, 0, 639, 16, 639, 32, 639, 48, 639, 64
        .word   639, 80, 639, 96, 639, 112, 639, 128, 639, 144, 639, 160
        .word   639, 176, 639, 192, 639, 208, 639, 224, 639, 240, 639, 256
        .word   639, 272, 639, 288, 639, 304, 639, 320, 639, 336, 639, 352

        .include "lib.s"
