; long long __muldi3(long long a, long long b): 64 x 64 -> low 64 bits.
; a = r5:r4:r3:r2 (p1 low, p2 high), b = r7:r6 (p3) low, high half on the
; stack at [sp]. Result in p1 (low) and p2 (high).
;
; Replaces compiler-rt's C version, which built this from four calls to
; __mulsi3. With a = AH:AL and b = BH:BL (32-bit halves):
;   a * b mod 2^64 = AL*BL + ((AL*BH + AH*BL) mod 2^32) << 32
; AL*BL takes four 16x16 products (MUL low half, MULHU high half), each
; cross term three. No branches: about 45 instructions.
; Callers: FixedMul in Doom ((int64)a * b >> 16), and soft float's
; 32 x 32 -> 64 mantissa products.
; MUL and MULHU leave the flags alone, which the carry chains rely on.
        .text
        .globl  __muldi3
__muldi3:
        LDP     p0, [sp]                ; r1:r0 = BH
        PUSHP   p4
        PUSHP   p5
        ; X = r9:r8 = low 32 bits of AL*BH + AH*BL
        MUL     r8, r2, r0              ; a0*b2
        MULHU   r9, r2, r0
        MUL     r10, r2, r1             ; a0*b3 (low half only)
        ADD     r9, r9, r10
        MUL     r10, r3, r0             ; a1*b2
        ADD     r9, r9, r10
        MUL     r10, r4, r6             ; a2*b0
        MULHU   r11, r4, r6
        ADD     r8, r8, r10
        ADC     r9, r9, r11
        MUL     r10, r4, r7             ; a2*b1
        ADD     r9, r9, r10
        MUL     r10, r5, r6             ; a3*b0
        ADD     r9, r9, r10
        ; AL*BL: L0..L3 / H0..H3 are the low / high halves of
        ; a0*b0, a0*b1, a1*b0, a1*b1
        MUL     r0, r2, r6              ; L0 = word 0
        MULHU   r1, r2, r6              ; H0
        MUL     r4, r2, r7              ; L1
        MULHU   r5, r2, r7              ; H1
        MUL     r10, r3, r6             ; L2
        MULHU   r11, r3, r6             ; H2
        MUL     r2, r3, r7              ; L3
        MULHU   r3, r3, r7              ; H3
        ; words 1..3 = (H0, H1, H3) + (L1, H2, 0) + (L2, L3, 0) + (0, X)
        ADD     r1, r1, r4
        ADC     r5, r5, r11
        MOVI    r4, 0                   ; MOVI keeps the carry
        ADC     r3, r3, r4
        ADD     r1, r1, r10
        ADC     r5, r5, r2
        ADC     r3, r3, r4
        ADD     r5, r5, r8
        ADC     r3, r3, r9
        MOV     r4, r5                  ; word 2
        MOV     r5, r3                  ; word 3
        MOVP    p1, p0                  ; words 0, 1
        POPP    p5
        POPP    p4
        RET
