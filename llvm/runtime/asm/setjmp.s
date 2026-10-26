; setjmp/longjmp: jmp_buf holds the callee-saved pairs p4, p5, lr and sp.
        .text
        .globl  setjmp
setjmp:
        STP     p4, [p1]
        STP     p5, [p1 + 4]
        STP     p6, [p1 + 8]
        STP     p7, [p1 + 12]
        MOVI    r2, 0
        RET

        .globl  longjmp
longjmp:
        LDP     p4, [p1]
        LDP     p5, [p1 + 4]
        LDP     p6, [p1 + 8]
        LDP     p7, [p1 + 12]
        MOV     r2, r4
        CMPI    r2, 0
        BNE     .Lret
        MOVI    r2, 1
.Lret:
        RET
