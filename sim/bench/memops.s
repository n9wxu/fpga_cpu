; Block memory operations: fill, then copy 8 KB three ways
; (bytes, 16-bit words, 32-bit pairs) and checksum each copy.
; expect: "59392 59392 59392\n"

N = 8192                                ; bytes

        .org    0x1000
start:  LA      sp, 0x0F0000
        ; fill src[i] = i*7 + 3
        LA      p1, src
        MOVI    r4, 3
        LI      r5, N
fill:   STB     r4, [p1]+
        ADDI    r4, r4, 7
        DEC     r5
        BNE     fill
        ; byte copy
        LA      p1, src
        LA      p2, dst1
        LI      r6, N
cb:     LDBU    r7, [p1]+
        STB     r7, [p2]+
        DEC     r6
        BNE     cb
        ; word copy
        LA      p1, src
        LA      p2, dst2
        LI      r6, N / 2
cw:     LDW     r7, [p1]+
        STW     r7, [p2]
        ADDPI   p2, p2, 2
        DEC     r6
        BNE     cw
        ; pair copy
        LA      p1, src
        LA      p4, dst3
        LI      r6, N / 4
cp:     LDP     p2, [p1]+
        STP     p2, [p4]+
        DEC     r6
        BNE     cp
        ; checksums
        LA      p1, dst1
        CALL    sum
        CALL    print_dec
        MOVI    r2, ' '
        CALL    putc
        LA      p1, dst2
        CALL    sum
        CALL    print_dec
        MOVI    r2, ' '
        CALL    putc
        LA      p1, dst3
        CALL    sum
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

; sum: Fletcher-style checksum of the N/2 words at p1 -> r2
; (a += word; b += a; result b). Clobbers r4-r7, p1.
sum:    MOVI    r4, 0
        MOVI    r7, 0
        LI      r5, N / 2
sum1:   LDW     r6, [p1]+
        ADD     r4, r4, r6
        ADD     r7, r7, r4
        DEC     r5
        BNE     sum1
        MOV     r2, r7
        RET

        .include "lib.s"
        .align  4
src:    .space  N
dst1:   .space  N
dst2:   .space  N
dst3:   .space  N
