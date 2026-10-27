; int __clzsi2(unsigned long a): leading zero bits of a 32-bit value, a = p1
; (r3:r2); the result in r2 (32 for 0; compiler-rt's callers never pass 0).
; compiler-rt's int <-> float conversions call it. Extension X: CLZ.
        .text
        .globl  __clzsi2
__clzsi2:
.ifdef FCPU_X_CLZ
        CMPI    r3, 0
        BEQ     .Llow
        CLZ     r2, r3
        RET
.Llow:
        CLZ     r2, r2
        ADDI    r2, r2, 16
        RET
.else
        MOVI    r4, 0                   ; count
        CMPI    r3, 0
        BNE     .Lhi
        MOV     r3, r2                  ; high half is zero: count 16, look at the low half
        MOVI    r4, 16
        CMPI    r3, 0
        BNE     .Lhi
        MOVI    r2, 32
        RET
.Lhi:                                   ; binary search over r3 (nonzero)
        SHRI    r5, r3, 8
        BNE     .L8
        ADDI    r4, r4, 8
        SHLI    r3, r3, 8
.L8:
        SHRI    r5, r3, 12
        BNE     .L4
        ADDI    r4, r4, 4
        SHLI    r3, r3, 4
.L4:
        SHRI    r5, r3, 14
        BNE     .L2
        ADDI    r4, r4, 2
        SHLI    r3, r3, 2
.L2:
        SHRI    r5, r3, 15
        BNE     .L1
        ADDI    r4, r4, 1
.L1:
        MOV     r2, r4
        RET
.endif
