; Smallest end-to-end test: print a string and exit.
; expect: "Hello, world!\n"

        .org    0x1000
start:  LA      sp, 0x0F0000
        LA      p1, msg
        CALL    puts
        MOVI    r2, 0
        CALL    exit

msg:    .asciz  "Hello, world!\n"
        .align  2
        .include "lib.s"
