; Recursive Fibonacci: call/return, stack push/pop of lr and callee-saved registers.
; expect: "6765\n"

        .org    0x1000
start:  LA      sp, 0x0F0000
        MOVI    r2, 20
        CALL    fib
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

; fib(r2) -> r2
fib:    CMPI    r2, 2
        BLT     fib_base
        PUSHP   lr
        PUSH    r8
        PUSH    r9
        MOV     r8, r2
        ADDI    r2, r8, -1
        CALL    fib
        MOV     r9, r2
        ADDI    r2, r8, -2
        CALL    fib
        ADD     r2, r2, r9
        POP     r9
        POP     r8
        POPP    lr
fib_base:
        RET

        .include "lib.s"
