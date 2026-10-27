; float __divsf3(float a, float b)
; a = p1 (r3 high, r2 low), b = p2 (r5:r4); result p1.
;
; Fast path when both operands are normal and the quotient is certainly
; normal or overflows only by rounding. Round to nearest even, bit-exact
; with compiler-rt. Anything else goes to compiler-rt's C version,
; __fcpu_divsf3_c.
;
; With significands ma, mb (24 bits) ordered so that mb <= ma < 2*mb, the
; first quotient bit is 1; 24 restoring steps on remainder ma - mb give 23
; fraction bits and the round bit, and a nonzero final remainder is the
; sticky bit. Extension X does each step with one DSTEP.
        .text
        .globl  __divsf3
__divsf3:
        SHLI    r6, r3, 1               ; ea
        SHRI    r6, r6, 8
        SHLI    r7, r5, 1               ; eb
        SHRI    r7, r7, 8
        CMPI    r6, 0
        BEQ     .Lslow
        CMPI    r7, 0
        BEQ     .Lslow
        CMPI    r6, 255
        BEQ     .Lslow
        CMPI    r7, 255
        BEQ     .Lslow
        SUB     r6, r6, r7
        ADDI    r6, r6, 127             ; e = ea - eb + 127, less 1 if ma < mb
        CMPI    r6, 2
        BLT     .Lslow                  ; could be subnormal
        CMPI    r6, 255
        BGE     .Lslow                  ; overflows
        PUSHP   p4
        XOR     r8, r3, r5
        ANDI    r8, r8, 0x8000          ; sign
        ANDI    r3, r3, 0x7F            ; ma = p1, mb = p2 (24 bits)
        ORI     r3, r3, 0x80
        ANDI    r5, r5, 0x7F
        ORI     r5, r5, 0x80
        CMP     r2, r4                  ; ma < mb: double ma, e - 1
        CPC     r3, r5
        BHS     .Lordered
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        ADDI    r6, r6, -1
.Lordered:
        SUB     r2, r2, r4              ; remainder = ma - mb (< mb)
        SBC     r3, r3, r5
        MOVI    r0, 1                   ; quotient = p0, leading 1
        MOVI    r1, 0
.ifdef FCPU_X_DSTEP
        ADDI    r9, r9, 0               ; C = 0: no dividend bits
        DSTEP   p1, p2
        ADC     r0, r0, r0              ; quotient bit in; C = 0 again
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
        DSTEP   p1, p2
        ADC     r0, r0, r0
        ADC     r1, r1, r1
.else
        MOVI    r9, 24
.Lstep:
        ADD     r0, r0, r0              ; quotient <<= 1
        ADC     r1, r1, r1
        ADD     r2, r2, r2              ; remainder <<= 1
        ADC     r3, r3, r3
        CMP     r2, r4
        CPC     r3, r5
        BLO     .Lnext
        SUB     r2, r2, r4
        SBC     r3, r3, r5
        ADDI    r0, r0, 1
.Lnext:
        ADDI    r9, r9, -1
        BNE     .Lstep
.endif
        ; p0 = 25 bits: 1, 23 fraction bits, round bit. Pack as
        ; ((e - 1) << 23) + significand, so a rounding carry ripples into
        ; the exponent (and from 254 to infinity).
        OR      r9, r2, r3              ; sticky
        ANDI    r7, r0, 1               ; round bit
        SHRI    r0, r0, 1               ; significand = p0 >> 1
        SHLI    r4, r1, 15
        OR      r0, r0, r4
        SHRI    r1, r1, 1
        ADDI    r6, r6, -1
        SHLI    r6, r6, 7
        ADD     r3, r6, r1              ; high half
        MOV     r2, r0                  ; low half
        CMPI    r7, 0
        BEQ     .Lpack
        CMPI    r9, 0
        BNE     .Lup
        ANDI    r9, r2, 1               ; a tie: round to even
        BEQ     .Lpack
.Lup:
        ADDI    r2, r2, 1
        MOVI    r9, 0
        ADC     r3, r3, r9
.Lpack:
        OR      r3, r3, r8
        POPP    p4
        RET
.Lslow:
        LA      p0, __fcpu_divsf3_c
        JR      p0
