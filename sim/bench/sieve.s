; Sieve of Eratosthenes over 8192 byte flags, repeated 4 times.
; Byte stores/loads with register index, tight compare-and-branch loops.
; expect: "1028\n"

N = 8192

        .org    0x1000
start:  LA      sp, 0x0F0000
        MOVI    r8, 4                   ; repetitions
again:  LA      p1, flags
        LI      r4, N
        MOVI    r5, 1
clr:    STB     r5, [p1]+               ; flags[] = 1
        DEC     r4
        BNE     clr
        LA      p1, flags
        MOVI    r5, 0
        STB     r5, [p1]                ; 0 and 1 are not prime
        STB     r5, [p1 + 1]
        MOVI    r6, 2                   ; i
        MOVI    r7, 0                   ; prime count
iloop:  CMPI    r6, N
        BHS     idone
        LDBU    r5, [p1 + r6]
        TST     r5
        BEQ     inext
        INC     r7
        ADD     r9, r6, r6              ; j = 2i
        MOVI    r5, 0
jloop:  CMPI    r9, N
        BHS     inext
        STB     r5, [p1 + r9]
        ADD     r9, r9, r6
        BRA     jloop
inext:  INC     r6
        BRA     iloop
idone:  DEC     r8
        BNE     again
        MOV     r2, r7
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

        .include "lib.s"
flags:  .space  N
