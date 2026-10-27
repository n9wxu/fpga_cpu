; 64-bit division: __udivdi3, __umoddi3, __divdi3, __moddi3.
; a = r5:r4:r3:r2 (p1 low, p2 high), b = r7:r6 (p3) low, high half on the
; stack at [sp]. Result in p1 (low) and p2 (high).
;
; Replaces compiler-rt's C versions, which went through __udivmoddi4 and
; 32-bit __udivsi3 calls (about 4,900 cycles for Doom's FixedDiv,
; ((int64)a << 16) / b). A divisor that fits in 32 bits, the common case,
; takes at most two 32-step shift-and-subtract loops here. A wider divisor
; goes to compiler-rt's __udivmoddi4. The 32-step loop is __fcpu_udiv32
; in divsi3.s.
        .text

; udivmod64: unsigned N / D. N = r5:r4:r3:r2, D = r1:r0:r7:r6.
; Returns the quotient in p2:p1 and the remainder in p0:p3.
; Uses p4, p5 (saved) and lr.
udivmod64:
        PUSHP   p4
        PUSHP   p5
        PUSHP   lr
        OR      r8, r0, r1
        BNE     .Lwide
        ; 32-bit divisor (__fcpu_udiv32 in divsi3.s wants it in p2). If N's
        ; high half is >= D the quotient needs more than 32 bits: divide
        ; the high half first.
        CMP     r4, r6
        CPC     r5, r7
        BLO     .Lsmall
        MOVP    p5, p1                  ; keep N's low half
        MOVP    p1, p2                  ; divide the high half
        MOVP    p2, p3
        JAL     __fcpu_udiv32z
        MOVP    p4, p1                  ; quotient's high half -> p5,
        MOVP    p1, p5                  ; N's low half -> p1
        MOVP    p5, p4
        BRA     .Llow
.Lsmall:
        MOVP    p0, p2                  ; N's high half is the remainder so far
        MOVP    p2, p3
        MOVI    r10, 0                  ; quotient's high half is 0
        MOVI    r11, 0
.Llow:
        JAL     __fcpu_udiv32           ; quotient's low half
        MOVP    p2, p5
        MOVP    p3, p0
        MOVI    r0, 0
        MOVI    r1, 0
        BRA     .Ldone
.Lwide:
        ; __udivmoddi4(N, D, &rem): D's high half and &rem go on the stack
        ADDPI   sp, sp, -16
        STP     p0, [sp]
        LEA     p0, [sp + 8]
        STP     p0, [sp + 4]
        JAL     __udivmoddi4
        LDP     p3, [sp + 8]
        LDP     p0, [sp + 12]
        ADDPI   sp, sp, 16
.Ldone:
        POPP    lr
        POPP    p5
        POPP    p4
        RET

        .globl  __udivdi3
__udivdi3:
        PUSHP   lr
        LDP     p0, [sp + 4]
        JAL     udivmod64
        POPP    lr
        RET

        .globl  __umoddi3
__umoddi3:
        PUSHP   lr
        LDP     p0, [sp + 4]
        JAL     udivmod64
        MOVP    p1, p3
        MOVP    p2, p0
        POPP    lr
        RET

; Signed: divide the magnitudes. The quotient is negative when the signs
; differ, the remainder takes the dividend's sign (C rounds toward zero).
        .globl  __divdi3
__divdi3:
        PUSHP   lr
        PUSHP   p4
        LDP     p0, [sp + 8]
        XOR     r9, r5, r1              ; bit 15: sign of the quotient
        JAL     .Labs
        PUSH    r9
        JAL     udivmod64
        POP     r9
        CMPI    r9, 0
        BGE     .Ldiv_pos
        JAL     .Lneg_n
.Ldiv_pos:
        POPP    p4
        POPP    lr
        RET

        .globl  __moddi3
__moddi3:
        PUSHP   lr
        PUSHP   p4
        LDP     p0, [sp + 8]
        MOV     r9, r5                  ; bit 15: sign of the remainder
        JAL     .Labs
        PUSH    r9
        JAL     udivmod64
        MOVP    p1, p3
        MOVP    p2, p0
        POP     r9
        CMPI    r9, 0
        BGE     .Lmod_pos
        JAL     .Lneg_n
.Lmod_pos:
        POPP    p4
        POPP    lr
        RET

; .Labs: N = |N|, D = |D|. Uses r8.
.Labs:
        MOVI    r8, 0
        CMPI    r5, 0
        BGE     .Labs_d
        SUB     r2, r8, r2
        SBC     r3, r8, r3
        SBC     r4, r8, r4
        SBC     r5, r8, r5
.Labs_d:
        CMPI    r1, 0
        BGE     .Labs_done
        SUB     r6, r8, r6
        SBC     r7, r8, r7
        SBC     r0, r8, r0
        SBC     r1, r8, r1
.Labs_done:
        RET

; .Lneg_n: r5:r4:r3:r2 = -r5:r4:r3:r2. Uses r8.
.Lneg_n:
        MOVI    r8, 0
        SUB     r2, r8, r2
        SBC     r3, r8, r3
        SBC     r4, r8, r4
        SBC     r5, r8, r5
        RET
