; Extension X: DSTEP pd, ps. Single steps with both carry-ins and with the
; bit shifted out of pd, pd == ps, then the 32-bit restoring division loop
; it exists for (the quotient comes back through C).
; ext: yes
        .org    0x1000
start:  LA      sp, 0x0F0000
        LI      r6, 0x7777
        LI      r7, 0x1111
        LI      r14, 80
steps:  LI      r8, 25173
        MUL     r6, r6, r8
        ADDI    r6, r6, 13849
        LI      r8, 4093
        MUL     r7, r7, r8
        ADDI    r7, r7, 1
        MOV     r2, r6
        MOV     r3, r7
        MOV     r4, r7
        MOV     r5, r6
        ANDI    r8, r14, 1
        NEG     r8, r8          ; C = 1 when the count is odd
        DSTEP   p1, p2
        MOV     r2, r6
        MOV     r3, r7
        MOV     r4, r6
        MOV     r5, r7
        SUB     r8, r8, r8      ; C = 1
        DSTEP   p1, p1          ; pd == ps
        MOV     r2, r6
        LI      r3, 0x8000      ; the bit shifted out of pd counts
        LI      r4, 7
        LI      r5, 0
        CMPI    r8, 1
        DSTEP   p1, p2
        MOV     r2, r6
        LI      r3, 0x4000
        LI      r4, 0
        LI      r5, 0x4000
        DSTEP   p1, p2          ; equal after the shift: subtract, remainder 0
        DEC     r14
        BNE     steps
        ; unsigned 32 / 32 divide: p3 / p2, quotient in p3, remainder in p1
        LI      r6, 0x5678
        LI      r7, 0x1234
        LI      r4, 0x0033
        LI      r5, 0
        LI      r2, 0
        LI      r3, 0
        LI      r14, 8
        ADD     r6, r6, r6      ; C = the dividend's top bit
        ADC     r7, r7, r7
div:    DSTEP   p1, p2
        ADC     r6, r6, r6
        ADC     r7, r7, r7
        DSTEP   p1, p2
        ADC     r6, r6, r6
        ADC     r7, r7, r7
        DSTEP   p1, p2
        ADC     r6, r6, r6
        ADC     r7, r7, r7
        DSTEP   p1, p2
        ADC     r6, r6, r6
        ADC     r7, r7, r7
        MFSR    r8, flags
        DEC     r14
        BEQ     divdone
        MTSR    flags, r8
        BRA     div
divdone:
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
