; 32-bit division. a = p1 (r3:r2), b = p2 (r5:r4); result p1.
; p0 (r1:r0) is scratch: the compiler never keeps values in it.
        .text
; .Ludiv32: p1 = p1 / p2, p3 = p1 % p2. Uses r0.
.Ludiv32:
        MOVI    r6, 0
        MOVI    r7, 0
        MOVI    r0, 32
.Lu32_loop:
        ADD     r2, r2, r2              ; shift the dividend left ...
        ADC     r3, r3, r3
        ADC     r6, r6, r6              ; ... into the remainder
        ADC     r7, r7, r7
        BCS     .Lu32_sub
        CMP     r6, r4
        CPC     r7, r5
        BLO     .Lu32_next
.Lu32_sub:
        SUB     r6, r6, r4
        SBC     r7, r7, r5
        ADDI    r2, r2, 1               ; bit 0 was 0 after the shift: no carry
.Lu32_next:
        ADDI    r0, r0, -1
        BNE     .Lu32_loop
        RET

        .globl  __udivsi3
__udivsi3:
        PUSHP   lr
        JAL     .Ludiv32
        POPP    lr
        RET

        .globl  __umodsi3
__umodsi3:
        PUSHP   lr
        JAL     .Ludiv32
        MOVP    p1, p3
        POPP    lr
        RET

; .Lneg_p1 / .Lneg_p2: negate a pair in place. Uses r1.
.Lneg_p1:
        MOVI    r1, 0
        SUB     r2, r1, r2
        SBC     r3, r1, r3
        RET
.Lneg_p2:
        MOVI    r1, 0
        SUB     r4, r1, r4
        SBC     r5, r1, r5
        RET

        .globl  __divsi3
__divsi3:
        PUSHP   lr
        XOR     r1, r3, r5
        PUSH    r1                      ; bit 15 = sign of the quotient
        CMPI    r3, 0
        BGE     .Ld32_a
        JAL     .Lneg_p1
.Ld32_a:
        CMPI    r5, 0
        BGE     .Ld32_b
        JAL     .Lneg_p2
.Ld32_b:
        JAL     .Ludiv32
        POP     r1
        CMPI    r1, 0
        BGE     .Ld32_ret
        JAL     .Lneg_p1
.Ld32_ret:
        POPP    lr
        RET

        .globl  __modsi3
__modsi3:
        PUSHP   lr
        PUSH    r3                      ; sign of the remainder
        CMPI    r3, 0
        BGE     .Lm32_a
        JAL     .Lneg_p1
.Lm32_a:
        CMPI    r5, 0
        BGE     .Lm32_b
        JAL     .Lneg_p2
.Lm32_b:
        JAL     .Ludiv32
        MOVP    p1, p3
        POP     r1
        CMPI    r1, 0
        BGE     .Lm32_ret
        JAL     .Lneg_p1
.Lm32_ret:
        POPP    lr
        RET
