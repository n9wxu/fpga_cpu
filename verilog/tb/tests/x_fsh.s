; Extension X: FSHR / FSHL rd, ps, rt for every shift count and random
; pairs, with rd in the source pair (either register) and rt aliasing.
; ext: yes
        .org    0x1000
start:  LA      sp, 0x0F0000
        LI      r6, 0x2468
        LI      r7, 0x1357
        LI      r14, 40
loop:   LI      r8, 25173
        MUL     r6, r6, r8
        ADDI    r6, r6, 13849
        LI      r8, 4093
        MUL     r7, r7, r8
        ADDI    r7, r7, 1
        MOV     r10, r14        ; shift count 0..39 (uses the low 4 bits)
        MOV     r2, r6
        MOV     r3, r7
        FSHR    r4, p1, r10
        FSHL    r5, p1, r10
        FSHR    r2, p1, r10     ; rd = low register of the source
        MOV     r2, r6
        FSHL    r2, p1, r10
        MOV     r2, r6
        FSHR    r3, p1, r10     ; rd = high register of the source
        MOV     r3, r7
        FSHL    r3, p1, r10
        MOV     r3, r7
        MOV     r11, r14        ; count register inside the pair
        FSHR    r10, p5, r11
        MOV     r10, r6
        MOV     r11, r14
        FSHL    r11, p5, r11
        MOV     r10, r6
        MOV     r11, r14
        FSHR    r10, p5, r10
        MOV     r10, r6
        FSHL    r10, p5, r11
        DEC     r14
        BNE     loop
        LI      r2, 0x8001
        LI      r3, 0x8001
        LI      r10, 0
        FSHR    r4, p1, r10
        FSHL    r4, p1, r10
        LI      r10, 16
        FSHR    r4, p1, r10     ; count 16 is count 0
        FSHL    r4, p1, r10
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
