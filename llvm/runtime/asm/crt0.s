; C start-up code: stack, BSS, default video layout (docs/io.md), main, exit.
        .text
        .globl  _start
_start:
        LA      sp, 0x053400            ; stack grows down from the sprite buffers
        LA      p1, __bss_start
        LA      p2, __bss_end
        MOVI    r6, 0
.Lclear:
        CMP     r2, r4
        CPC     r3, r5
        BHS     .Ldone
        STW     r6, [p1]+
        BRA     .Lclear
.Ldone:
        LA      p1, 0x0FC000            ; text buffer
        STP     p1, [0xF00204]
        LA      p1, 0x0C3C00            ; bitmap
        STP     p1, [0xF00208]
        LA      p1, 0x053400            ; sprite buffers (2 x 230,400 for 8 bpp)
        STP     p1, [0xF0020C]
        MOVI    r2, 0x21                ; text layer and cursor on
        STW     r2, [0xF00200]
        MOVI    r2, 0
        MOVI    r4, 0
        MOVI    r5, 0
        CALL    main
        .globl  exit
exit:
        STW     r2, [0xF00002]          ; simulator: stop with the exit code
.Lhang:
        BRA     .Lhang
