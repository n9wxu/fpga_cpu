; setjmp/longjmp: jmp_buf holds the callee-saved pairs p4, p5, lr and sp.
        .text
        .globl  setjmp
setjmp:
        STP     p4, [p1]
        STP     p5, [p1 + 4]
        STP     p6, [p1 + 8]
        STP     p7, [p1 + 12]
        MOVI    r2, 0
        MOVI    r3, 0                   ; high half, for a 32-bit int
        RET

        .globl  longjmp
longjmp:
        LDP     p4, [p1]
        LDP     p5, [p1 + 4]
        LDP     p6, [p1 + 8]
        LDP     p7, [p1 + 12]
        MOV     r2, r4
        MOV     r3, r5                  ; high half, for a 32-bit int (garbage with 16-bit int)
        CMPI    r2, 0                   ; val 0 returns 1: tested on the low half only, which is
        BNE     .Lret                   ; exact for 16-bit int (val 0x10000 also counts as 0)
        MOVI    r2, 1
        MOVI    r3, 0
.Lret:
        RET
