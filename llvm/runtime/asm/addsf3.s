; float __addsf3(float a, float b), float __subsf3(float a, float b)
; a = p1 (r3 high, r2 low), b = p2 (r5:r4); result p1.
;
; Fast path for normal operands, round to nearest even, bit-exact with
; compiler-rt. Anything else (a zero, subnormal, infinite or NaN operand
; other than adding +-0 to a normal, or a difference that could be
; subnormal) goes to compiler-rt's C version, built as __fcpu_addsf3_c.
;
; Working form: the 24-bit significand shifted left 7 (leading 1 at bit
; 30), so 7 guard bits sit below it and bit 31 catches the carry of an
; addition. Bits shifted out while aligning b fold into bit 0 (sticky).
        .text
        .globl  __subsf3
__subsf3:
        XORI    r5, r5, 0x8000          ; a - b = a + (-b)
        .globl  __addsf3
__addsf3:
        ANDI    r6, r3, 0x7FFF          ; |a| and |b|, high halves
        ANDI    r7, r5, 0x7FFF
        CMP     r2, r4                  ; order so that |a| >= |b|
        CPC     r6, r7
        BHS     .La_big
        MOVP    p0, p1
        MOVP    p1, p2
        MOVP    p2, p0
        MOV     r0, r6
        MOV     r6, r7
        MOV     r7, r0
.La_big:
        OR      r0, r7, r4
        BEQ     .Lb_zero
        SHRI    r6, r6, 7               ; ea
        SHRI    r7, r7, 7               ; eb
        CMPI    r6, 255
        BEQ     .Lslow                  ; a infinite or NaN
        CMPI    r7, 0
        BEQ     .Lslow                  ; b subnormal
        SUB     r0, r6, r7              ; d = ea - eb
        CMPI    r0, 26
        BHS     .Lret                   ; b is under half an ulp of a: a
        XOR     r1, r3, r5              ; bit 15: the signs differ
        BPL     .Lunpack
        CMPI    r6, 26                  ; a difference this small could be
        BLO     .Lslow                  ; subnormal
.Lunpack:
        PUSHP   p4
        ANDI    r8, r3, 0x8000          ; sign of the result (a's)
        CMPI    r1, 0
        BGE     .Lsame
        ORI     r8, r8, 1               ; bit 0: subtract
.Lsame:
        ANDI    r3, r3, 0x7F            ; a: (1.f) << 7
        ORI     r3, r3, 0x80
        SHLI    r3, r3, 7
        SHRI    r9, r2, 9
        OR      r3, r3, r9
        SHLI    r2, r2, 7
        ANDI    r5, r5, 0x7F            ; b likewise
        ORI     r5, r5, 0x80
        SHLI    r5, r5, 7
        SHRI    r9, r4, 9
        OR      r5, r5, r9
        SHLI    r4, r4, 7
        MOVI    r7, 0                   ; align b: r5:r4 >>= d, lost bits -> r7
        CMPI    r0, 16
        BLO     .Lalign
        MOV     r7, r4
        MOV     r4, r5
        MOVI    r5, 0
        ADDI    r0, r0, -16
.Lalign:
        CMPI    r0, 0
        BEQ     .Laligned
        MOVI    r9, 16
        SUB     r9, r9, r0              ; 16 - d
        SHL     r1, r4, r9
        OR      r7, r7, r1
        SHR     r4, r4, r0
        SHL     r1, r5, r9
        OR      r4, r4, r1
        SHR     r5, r5, r0
.Laligned:
        CMPI    r7, 0
        BEQ     .Lop
        ORI     r4, r4, 1               ; sticky
.Lop:
        ANDI    r9, r8, 1
        BNE     .Lsub
        ADD     r2, r2, r4
        ADC     r3, r3, r5
        CMPI    r3, 0
        BGE     .Lround                 ; no carry into bit 31
        ANDI    r1, r2, 1               ; shift right 1, keeping the sticky bit
        SHRI    r2, r2, 1
        SHLI    r9, r3, 15
        OR      r2, r2, r9
        OR      r2, r2, r1
        SHRI    r3, r3, 1
        ADDI    r6, r6, 1
        CMPI    r6, 255
        BEQ     .Linf
        BRA     .Lround
.Lsub:
        SUB     r2, r2, r4
        SBC     r3, r3, r5
        OR      r9, r2, r3
        BEQ     .Lzero                  ; exact cancellation: +0
        CMPI    r3, 0                   ; renormalize: a whole word first ...
        BNE     .Lnorm
        CMPI    r2, 0x4000
        BHS     .Lnorm
        MOV     r3, r2
        MOVI    r2, 0
        ADDI    r6, r6, -16
.Lnorm:                                 ; ... then bit by bit to bit 30
        ANDI    r9, r3, 0x4000
        BNE     .Lround
        ADD     r2, r2, r2
        ADC     r3, r3, r3
        ADDI    r6, r6, -1
        BRA     .Lnorm
.Lround:
        ; pack: ((e - 1) << 23) + significand (the leading 1 adds the
        ; last 1 to the exponent), so a rounding carry can ripple into the
        ; exponent, and from 254 to infinity
        ADDI    r6, r6, -1
        SHLI    r6, r6, 7
        SHRI    r9, r3, 7
        ADD     r1, r6, r9              ; high half
        SHLI    r9, r3, 9
        SHRI    r7, r2, 7
        OR      r0, r9, r7              ; low half
        ANDI    r9, r2, 0x40            ; round bit
        BEQ     .Lpack
        ANDI    r9, r2, 0x3F            ; sticky bits
        BNE     .Lup
        ANDI    r9, r0, 1               ; a tie: round to even
        BEQ     .Lpack
.Lup:
        ADDI    r0, r0, 1
        MOVI    r9, 0
        ADC     r1, r1, r9
.Lpack:
        ANDI    r9, r8, 0x8000
        OR      r3, r1, r9
        MOV     r2, r0
        POPP    p4
        RET
.Linf:
        ANDI    r9, r8, 0x8000
        ORI     r3, r9, 0x7F80
        MOVI    r2, 0
        POPP    p4
        RET
.Lzero:
        MOVI    r2, 0
        MOVI    r3, 0
        POPP    p4
        RET
.Lb_zero:                               ; b = +-0: a, if a is a normal number
        SHRI    r0, r6, 7
        CMPI    r0, 0
        BEQ     .Lslow
        CMPI    r0, 255
        BEQ     .Lslow
.Lret:
        RET
.Lslow:
        LA      p0, __fcpu_addsf3_c
        JR      p0
