; Extension X: CLZ rd, rs for zero, every single bit, all-ones and random
; values, with rd == rs.
; ext: yes
        .org    0x1000
start:  LA      sp, 0x0F0000
        LI      r4, 0
        CLZ     r2, r4
        LI      r14, 16
bits:   MOV     r5, r14
        DEC     r5
        MOVI    r4, 1
        SHL     r4, r4, r5
        CLZ     r2, r4
        CLZ     r4, r4          ; rd == rs
        DEC     r14
        BNE     bits
        LI      r4, 0xFFFF
        CLZ     r2, r4
        LI      r4, 0x7FFF
        CLZ     r2, r4
        LI      r4, 0x00FF
        CLZ     r2, r4
        LI      r4, 0xFF00
        CLZ     r2, r4
        LI      r6, 0x4321
        LI      r14, 100
loop:   LI      r8, 25173
        MUL     r6, r6, r8
        ADDI    r6, r6, 13849
        ANDI    r8, r14, 15
        LI      r9, 0xFFFF
        SHR     r9, r9, r8
        AND     r4, r6, r9
        CLZ     r2, r4
        CLZ     r3, r4
        MOV     r5, r4
        CLZ     r5, r5
        CLZ     r4, r4
        DEC     r14
        BNE     loop
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
