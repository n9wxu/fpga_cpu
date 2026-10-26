; Hi-color demo: a 256-entry palette shown as a 16x16 grid of blocks.
; Red rises left to right, green top to bottom, blue is constant.
; On the 3-bit base hardware the colors are dithered (fsim default);
; --true-color shows the palette itself.  Index 0 is transparent, so the
; top-left block shows the backdrop.  Exits after 2 frames.

        .org    0x1000

IO      = 0xF00000
VID     = 0x200
BMP     = 0x0C0000

start:  LA      sp, 0x080000
        LA      p5, IO
        LA      p1, BMP
        STP     p1, [p5 + VID + 8]
        MOVI    r2, 5                   ; magenta backdrop
        STW     r2, [p5 + VID + 2]

        ; palette: i = g*16 + r  ->  RGB565 (r*2, g*4, 12)
        MOVI    r2, 0
        STW     r2, [p5 + VID + 0x10]   ; PAL_INDEX = 0
pal:    ANDI    r3, r2, 15
        SHLI    r3, r3, 12              ; red: (r*2) << 11
        SHRI    r4, r2, 4
        SHLI    r4, r4, 7               ; green: (g*4) << 5
        OR      r3, r3, r4
        ORI     r3, r3, 12              ; blue
        STW     r3, [p5 + VID + 0x12]   ; PAL_DATA, index auto-increments
        ADDI    r2, r2, 1
        CMPI    r2, 256
        BNE     pal

        ; bitmap: 16 bands of 22 rows, each 16 blocks of 40 pixels
        LA      p1, BMP
        MOVI    r6, 0                   ; band (green)
band:   MOVI    r7, 22
row:    MOVI    r8, 0                   ; block (red)
blk:    SHLI    r4, r6, 4
        OR      r4, r4, r8
        MOVI    r9, 40
px:     STB     r4, [p1]+
        DEC     r9
        BNE     px
        ADDI    r8, r8, 1
        CMPI    r8, 16
        BNE     blk
        DEC     r7
        BNE     row
        ADDI    r6, r6, 1
        CMPI    r6, 16
        BNE     band

        MOVI    r2, 0x06                ; bitmap on, hi-color
        STW     r2, [p5 + VID]
        LDW     r8, [p5 + 0x0A]
wait:   LDW     r2, [p5 + 0x0A]
        SUB     r2, r2, r8
        CMPI    r2, 2
        BNE     wait
        MOVI    r2, 0
        STW     r2, [p5 + 2]
