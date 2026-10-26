; 32-bit arithmetic on register pairs: sum of squares with MUL/MULHU and
; ADD/ADC, then 32-bit shift-subtract division (CMP/CPC, SUB/SBC).
; Repeated 20 times.
; expect: "13D6A2DC 02D584FA 00000006\n"

        .org    0x1000
start:  LA      sp, 0x0F0000
        MOVI    r8, 20                  ; repetitions
again:  MOVI    r2, 0                   ; p1 = sum (r3:r2)
        MOVI    r3, 0
        MOVI    r6, 0                   ; i
        LI      r7, 1000
sq:     MUL     r4, r6, r6              ; p2 = i*i (r5:r4)
        MULHU   r5, r6, r6
        ADD     r2, r2, r4
        ADC     r3, r3, r5
        INC     r6
        CMP     r6, r7
        BLO     sq
        DEC     r8
        BNE     again
        MOVP    p4, p1                  ; keep the sum
        CALL    print_hex32
        MOVI    r2, ' '
        CALL    putc
        MOVP    p1, p4
        MOVI    r4, 7                   ; divide by 7
        MOVI    r5, 0
        CALL    udiv32
        MOVP    p5, p3                  ; remainder
        CALL    print_hex32             ; quotient
        MOVI    r2, ' '
        CALL    putc
        MOVP    p1, p5
        CALL    print_hex32
        CALL    newline
        MOVI    r2, 0
        CALL    exit

; udiv32: p1 / p2 -> quotient p1, remainder p3. Clobbers r0.
udiv32: MOVI    r6, 0
        MOVI    r7, 0
        MOVI    r0, 32
ud_loop:
        ADD     r2, r2, r2              ; shift remainder:quotient left
        ADC     r3, r3, r3
        ADC     r6, r6, r6
        ADC     r7, r7, r7
        CMP     r6, r4                  ; remainder >= divisor ?
        CPC     r7, r5
        BLO     ud_skip
        SUB     r6, r6, r4
        SBC     r7, r7, r5
        INC     r2
ud_skip:
        DEC     r0
        BNE     ud_loop
        RET

        .include "lib.s"
