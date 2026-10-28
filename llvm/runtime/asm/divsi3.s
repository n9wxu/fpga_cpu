; 32-bit division. a = p1 (r3:r2), b = p2 (r5:r4); result p1.
; p0 (r1:r0) is scratch: caller-saved, like p1-p3.
        .text

; __fcpu_udiv32: shift-and-subtract division, shared with divdi3.s.
;   in:  r1:r0 = remainder so far (must be < D), r3:r2 = 32 dividend bits,
;        r5:r4 = D
;   out: r3:r2 = quotient bits, r1:r0 = remainder
;   uses r6-r9 (callers save p4)
; __fcpu_udiv32z starts from remainder 0 (a plain 32 / 32 division).
; Steps whose quotient bit is certainly 0 are skipped 16 or 8 at a time:
; for Doom's 0xFFFFFFFF / scale that halves the loop. With extension X
; (fcc --ext) each step is one DSTEP plus two ADCs, unrolled.
        .globl  __fcpu_udiv32z
__fcpu_udiv32z:
        MOVI    r0, 0
        MOVI    r1, 0
        .globl  __fcpu_udiv32
__fcpu_udiv32:
        MOVI    r6, 32                  ; steps left
        CMPI    r1, 0                   ; skip 16 steps if remainder:N.hi < D
        BNE     .Ludiv32_loop
        CMP     r3, r4
        CPC     r0, r5
        BHS     .Ludiv32_byte
        MOV     r1, r0
        MOV     r0, r3
        MOV     r3, r2
        MOVI    r2, 0
        MOVI    r6, 16
.ifdef FCPU_X_DSTEP
; Extension X: DSTEP does one step. The dividend shifts out of r3:r2 into
; C, and the quotient bit shifts back in from C, by the ADC pair.
.Ludiv32_byte:                          ; skip 8 steps while (rem << 8 | N >> 24) < D
        SHRI    r7, r1, 8
        BNE     .Ludiv32_go
        SHLI    r8, r1, 8
        SHRI    r7, r0, 8
        OR      r8, r8, r7
        SHLI    r9, r0, 8
        SHRI    r7, r3, 8
        OR      r9, r9, r7
        CMP     r9, r4
        CPC     r8, r5
        BHS     .Ludiv32_go
        MOV     r1, r8
        MOV     r0, r9
        SHLI    r3, r3, 8
        SHRI    r7, r2, 8
        OR      r3, r3, r7
        SHLI    r2, r2, 8
        ADDI    r6, r6, -8
        BNE     .Ludiv32_byte
        RET
.Ludiv32_go:                            ; r6 = 32, 24, 16 or 8 steps left
        CMPI    r6, 16
        BEQ     .Lde16
        BLO     .Lde8
        CMPI    r6, 24
        BEQ     .Lde24
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        BRA     .Lds32
.Lde24:
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        BRA     .Lds24
.Lde16:
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        BRA     .Lds16
.Lde8:
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        BRA     .Lds8
.Lds32:
        DSTEP   p0, p2                  ; remainder*2 + C; subtract D if it fits; C = quotient bit
        ADC     r2, r2, r2              ; quotient bit in, next dividend bit out
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
.Lds24:
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
.Lds16:
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
.Lds8:
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        DSTEP   p0, p2
        ADC     r2, r2, r2
        ADC     r3, r3, r3
        RET
.Ludiv32_loop:                          ; (the 16-step skip above jumps here
        BRA     .Ludiv32_go             ;  when the remainder is too big for it)
.else
.Ludiv32_byte:                            ; skip 8 steps while (rem << 8 | N >> 24) < D
        SHRI    r7, r1, 8
        BNE     .Ludiv32_loop
        SHLI    r8, r1, 8
        SHRI    r7, r0, 8
        OR      r8, r8, r7
        SHLI    r9, r0, 8
        SHRI    r7, r3, 8
        OR      r9, r9, r7
        CMP     r9, r4
        CPC     r8, r5
        BHS     .Ludiv32_loop
        MOV     r1, r8
        MOV     r0, r9
        SHLI    r3, r3, 8
        SHRI    r7, r2, 8
        OR      r3, r3, r7
        SHLI    r2, r2, 8
        ADDI    r6, r6, -8
        BNE     .Ludiv32_byte
        RET
.Ludiv32_loop:
        ADD     r2, r2, r2              ; shift the dividend left ...
        ADC     r3, r3, r3
        ADC     r0, r0, r0              ; ... into the remainder
        ADC     r1, r1, r1
        BCS     .Ludiv32_sub              ; 33-bit remainder: certainly >= D
        CMP     r0, r4
        CPC     r1, r5
        BLO     .Ludiv32_next
.Ludiv32_sub:
        SUB     r0, r0, r4
        SBC     r1, r1, r5
        ADDI    r2, r2, 1               ; bit 0 was 0 after the shift: no carry
.Ludiv32_next:
        ADDI    r6, r6, -1
        BNE     .Ludiv32_loop
        RET
.endif

        .globl  __udivsi3
__udivsi3:
        PUSHP   lr
        PUSHP   p4
        JAL     __fcpu_udiv32z
        POPP    p4
        POPP    lr
        RET

        .globl  __umodsi3
__umodsi3:
        PUSHP   lr
        PUSHP   p4
        JAL     __fcpu_udiv32z
        MOVP    p1, p0
        POPP    p4
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
        PUSHP   p4
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
        JAL     __fcpu_udiv32z
        POP     r1
        CMPI    r1, 0
        BGE     .Ld32_ret
        JAL     .Lneg_p1
.Ld32_ret:
        POPP    p4
        POPP    lr
        RET

        .globl  __modsi3
__modsi3:
        PUSHP   lr
        PUSHP   p4
        PUSH    r3                      ; sign of the remainder
        CMPI    r3, 0
        BGE     .Lm32_a
        JAL     .Lneg_p1
.Lm32_a:
        CMPI    r5, 0
        BGE     .Lm32_b
        JAL     .Lneg_p2
.Lm32_b:
        JAL     __fcpu_udiv32z
        MOVP    p1, p0
        POP     r1
        CMPI    r1, 0
        BGE     .Lm32_ret
        JAL     .Lneg_p1
.Lm32_ret:
        POPP    p4
        POPP    lr
        RET
