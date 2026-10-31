; Extension X: MULPU pd, rs, rt (32-bit unsigned product), edge cases and a
; random loop, including the destination pair holding its own operands.
; ext: yes
        .org    0x1000
start:  LA      sp, 0x0F0000
        LI      r4, 0
        LI      r5, 0x1234
        MULPU   p1, r4, r5
        MULPU   p1, r5, r4
        MULPU   p1, r4, r4
        LI      r4, 0xFFFF
        LI      r5, 0xFFFF
        MULPU   p1, r4, r5
        LI      r4, 0x8000
        LI      r5, 0x8000
        MULPU   p1, r4, r5
        LI      r4, 1
        LI      r5, 0xABCD
        MULPU   p1, r4, r5
        MULPU   p1, r5, r4
        LI      r4, 0x100
        LI      r5, 0x100
        MULPU   p1, r4, r5
        LI      r4, 0xFF
        LI      r5, 0xFF
        MULPU   p1, r4, r5
        LI      r4, 0xFF00
        LI      r5, 0x00FF
        MULPU   p1, r4, r5
        MULPU   p1, r5, r4
        LI      r6, 0x1234
        LI      r7, 0xBEEF
        LI      r14, 120
loop:   LI      r8, 25173
        MUL     r6, r6, r8
        ADDI    r6, r6, 13849
        LI      r8, 4093
        MUL     r7, r7, r8
        ADDI    r7, r7, 1
        ANDI    r8, r14, 15
        LI      r9, 0xFFFF
        SHR     r9, r9, r8
        SHRI    r8, r14, 4
        ANDI    r8, r8, 15
        LI      r12, 0xFFFF
        SHR     r12, r12, r8
        AND     r4, r6, r9
        AND     r5, r7, r12
        MULPU   p1, r4, r5
        MULPU   p5, r5, r4
        MULPU   p1, r4, r4
        MOV     r2, r4
        MOV     r3, r5
        MULPU   p1, r2, r3
        MOV     r2, r4
        MOV     r3, r5
        MULPU   p1, r3, r2
        MOV     r2, r4
        MULPU   p1, r2, r2
        MOV     r10, r4
        MOV     r11, r5
        MULPU   p5, r10, r11
        MOV     r10, r4
        MOV     r11, r5
        MULPU   p5, r11, r10
        MOV     r10, r4
        MULPU   p5, r10, r10
        DEC     r14
        BNE     loop
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
