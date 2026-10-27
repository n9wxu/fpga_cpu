; 8 bpp sprite layer: buffer 1 at +230,400, palette colors, index 0
; transparent. sim/tools/devtest.py checks the screenshot.
        .org    0x1000
start:  LA      sp, 0x050000
        LA      p5, 0xF00000
        MOVI    r2, 5
        STW     r2, [p5 + 0x210]        ; PAL_INDEX = 5
        LI      r2, 0xFD20
        STW     r2, [p5 + 0x212]        ; orange
        LA      p1, 0x053400
        STP     p1, [p5 + 0x20C]        ; VID_SPR
        LA      p1, 0x053400 + 230400 + 10 * 640 + 10
        MOVI    r6, 5
        STB     r6, [p1]                ; buffer 1, (10, 10)
        LA      p1, 0x053400 + 20 * 640 + 20
        STB     r6, [p1]                ; buffer 0, (20, 20)
        MOVI    r2, 0
        STW     r2, [p5 + 0x202]        ; black backdrop
        MOVI    r2, 0x58                ; sprites on, buffer 1 shown, 8 bpp
        STW     r2, [p5 + 0x200]
        MOVI    r2, 0
        STW     r2, [p5 + 2]
