; 16-bit division. a = r2, b = r3; result r2. Division by zero gives
; quotient 0xFFFF and remainder a, like the shift-subtract loop does.
        .text
; .Ludiv16: r2 = r2 / r3, r4 = r2 % r3. Uses r5.
.Ludiv16:
        MOVI    r4, 0
        MOVI    r5, 16
.Lu16_loop:
        ADD     r2, r2, r2              ; next dividend bit into C
        ADC     r4, r4, r4              ; remainder = remainder*2 + bit
        BCS     .Lu16_sub               ; overflowed 16 bits: certainly >= b
        CMP     r4, r3
        BLO     .Lu16_next
.Lu16_sub:
        SUB     r4, r4, r3
        ADDI    r2, r2, 1
.Lu16_next:
        ADDI    r5, r5, -1
        BNE     .Lu16_loop
        RET

        .globl  __udivhi3
__udivhi3:
        MOVP    p0, lr
        JAL     .Ludiv16
        JR      p0

        .globl  __umodhi3
__umodhi3:
        MOVP    p0, lr
        JAL     .Ludiv16
        MOV     r2, r4
        JR      p0

; Signed: divide magnitudes; the quotient is negative when the signs
; differ, the remainder takes the sign of a.
        .globl  __divhi3
__divhi3:
        MOVP    p0, lr
        XOR     r6, r2, r3              ; sign of the quotient in bit 15
        CMPI    r2, 0
        BGE     .Ld16_a
        NEG     r2, r2
.Ld16_a:
        CMPI    r3, 0
        BGE     .Ld16_b
        NEG     r3, r3
.Ld16_b:
        JAL     .Ludiv16
        CMPI    r6, 0
        BGE     .Ld16_ret
        NEG     r2, r2
.Ld16_ret:
        JR      p0

        .globl  __modhi3
__modhi3:
        MOVP    p0, lr
        MOV     r6, r2                  ; sign of the remainder
        CMPI    r2, 0
        BGE     .Lm16_a
        NEG     r2, r2
.Lm16_a:
        CMPI    r3, 0
        BGE     .Lm16_b
        NEG     r3, r3
.Lm16_b:
        JAL     .Ludiv16
        MOV     r2, r4
        CMPI    r6, 0
        BGE     .Lm16_ret
        NEG     r2, r2
.Lm16_ret:
        JR      p0
