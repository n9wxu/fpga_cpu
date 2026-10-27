; float __mulsf3(float a, float b)
; a = p1 (r3 high, r2 low), b = p2 (r5:r4); result p1.
;
; Fast path when both operands are normal and the result is certainly
; normal or overflows only by rounding; a zero times a finite number is
; handled here too. Round to nearest even, bit-exact with compiler-rt.
; Everything else goes to compiler-rt's C version, __fcpu_mulsf3_c.
;
; The 24 x 24-bit significand product takes five 16 x 16 MUL/MULHU pairs'
; worth of products (the top bytes are 8 bits wide).
        .text
        .globl  __mulsf3
__mulsf3:
        SHLI    r6, r3, 1               ; ea
        SHRI    r6, r6, 8
        SHLI    r7, r5, 1               ; eb
        SHRI    r7, r7, 8
        CMPI    r6, 0
        BEQ     .La_small
        CMPI    r7, 0
        BEQ     .Lb_small
        CMPI    r6, 255
        BEQ     .Lslow
        CMPI    r7, 255
        BEQ     .Lslow
        ADD     r6, r6, r7
        ADDI    r6, r6, -127            ; e = ea + eb - 127 (biased)
        CMPI    r6, 1
        BLT     .Lslow                  ; could be subnormal
        CMPI    r6, 254
        BGE     .Lslow                  ; overflows
        XOR     r7, r3, r5
        ANDI    r7, r7, 0x8000          ; sign
        ANDI    r3, r3, 0x7F            ; significands: mh:ml, nh:nl,
        ORI     r3, r3, 0x80            ; mh and nh 8 bits
        ANDI    r5, r5, 0x7F
        ORI     r5, r5, 0x80
        PUSHP   p4
        ; P = r8:r1:r0 (48 bits) = ml*nl + (mh*nl + nh*ml) << 16 + mh*nh << 32
.ifdef FCPU_X_MULPU
        MULPU   p0, r2, r4
.else
        MUL     r0, r2, r4
        MULHU   r1, r2, r4
.endif
        MUL     r8, r3, r5              ; mh*nh < 2^16
        MUL     r9, r3, r4
        ADD     r1, r1, r9
        MULHU   r9, r3, r4
        ADC     r8, r8, r9
        MUL     r9, r5, r2
        ADD     r1, r1, r9
        MULHU   r9, r5, r2
        ADC     r8, r8, r9
        CMPI    r8, 0                   ; P in [2^46, 2^48): leading 1 at bit 47?
        BLT     .Ltop
        ADD     r0, r0, r0              ; no: shift left 1
        ADC     r1, r1, r1
        ADC     r8, r8, r8
        ADDI    r6, r6, -1
.Ltop:
        ; significand = P[47:24] = r8 : r1.hi; round bit P[23], sticky P[22:0].
        ; Packed as ((e + 1 - 1) << 23) + significand: the leading 1 adds
        ; the last 1 to the exponent, and a rounding carry ripples into it
        ; (254 -> infinity).
        SHLI    r6, r6, 7
        SHRI    r9, r8, 8
        ADD     r3, r6, r9              ; high half
        SHLI    r9, r8, 8
        SHRI    r2, r1, 8
        OR      r2, r2, r9              ; low half
        ANDI    r9, r1, 0x80
        BEQ     .Lpack
        ANDI    r9, r1, 0x7F
        OR      r9, r9, r0
        BNE     .Lup
        ANDI    r9, r2, 1               ; a tie: round to even
        BEQ     .Lpack
.Lup:
        ADDI    r2, r2, 1
        MOVI    r9, 0
        ADC     r3, r3, r9
.Lpack:
        OR      r3, r3, r7
        POPP    p4
        RET
.La_small:                              ; a zero or subnormal
        ANDI    r0, r3, 0x7FFF
        OR      r0, r0, r2
        BNE     .Lslow
        CMPI    r7, 255
        BEQ     .Lslow                  ; 0 * inf, 0 * NaN
        BRA     .Lzero
.Lb_small:
        ANDI    r0, r5, 0x7FFF
        OR      r0, r0, r4
        BNE     .Lslow
        CMPI    r6, 255
        BEQ     .Lslow
.Lzero:
        XOR     r3, r3, r5
        ANDI    r3, r3, 0x8000
        MOVI    r2, 0
        RET
.Lslow:
        LA      p0, __fcpu_mulsf3_c
        JR      p0
