; Memory above 1 MB (T1F with AW24, 8 MB of RAM): data across the old end of
; RAM at 0x100000, the top of the 8 MB, code run from high memory (a loop and
; branches there), and fetching that steps from 0x0FFFFF to 0x100000.  Exit 0
; is a pass; exit n says which check failed.
; xram: 7
        .org    0x1000
start:  LA      sp, 0x0F0000
        ; two words either side of 0x100000, then the pair across it (values in r12, r13;
        ; pointers p2 = r4:r5 and p3 = r6:r7; the pair read back into p4 = r8:r9)
        LA      p2, 0x0FFFFE
        LI      r12, 0x1357
        STW     r12, [p2]+
        LI      r13, 0x2468
        STW     r13, [p2]+              ; the pointer ends at 0x100002
        LA      p3, 0x0FFFFE
        LDP     p4, [p3]
        MOVI    r2, 1
        SUB     r11, r8, r12
        BNE     fail
        SUB     r11, r9, r13
        BNE     fail
        LA      p3, 0x100000
        LDBU    r10, [p3]
        MOVI    r2, 2
        CMPI    r10, 0x68
        BNE     fail
        ; the top of the 8 MB: a word and the last byte
        LA      p2, 0x7FFFFC
        LI      r12, 0x5AA5
        STW     r12, [p2]
        LA      p2, 0x7FFFFF
        MOVI    r13, 0x3C
        STB     r13, [p2]
        LA      p3, 0x7FFFFC
        LDW     r10, [p3]
        MOVI    r2, 3
        SUB     r11, r10, r12
        BNE     fail
        LDBU    r10, [p3 + 3]
        MOVI    r2, 4
        CMPI    r10, 0x3C
        BNE     fail
        ; an index that carries into bits 23:16
        LA      p2, 0x3FFFFF
        MOVI    r12, 2
        MOVI    r13, 0x47
        STB     r13, [p2 + r12*1]
        LA      p3, 0x400001
        LDBU    r10, [p3]
        MOVI    r2, 5
        CMPI    r10, 0x47
        BNE     fail
        ; code at 0x600000: a loop, then back
        LA      p1, s0
        LA      p2, 0x600000
        LA      p3, 0x600000
        LA      p4, back0
        MOVI    r10, (s0e - s0) / 2
        BRA     copy
back0:  MOVI    r2, 6
        CMPI    r5, 10
        BNE     fail
        ; code that runs from 0x0FFFF8 on over 0x100000
        LA      p1, s1
        LA      p2, 0x100000 - 8
        LA      p3, 0x100000 - 8
        LA      p4, back1
        MOVI    r10, (s1e - s1) / 2
        BRA     copy
back1:  MOVI    r2, 7
        CMPI    r5, 6
        BNE     fail
        MOVI    r2, 0
fail:   CALL    exit

        .org    0x2000
copy:   LDW     r11, [p1]+
        STW     r11, [p2]+
        DEC     r10
        BNE     copy
        JR      p3

s0:     MOVI    r5, 0                   ; runs at 0x600000
        MOVI    r3, 10
s0l:    ADDI    r5, r5, 1
        DEC     r3
        BNE     s0l
        BRA     s0f
        MOVI    r5, 99
s0f:    JR      p4
s0e:

s1:     MOVI    r5, 1                   ; runs at 0x0FFFF8: the last four parcels of the first MB
        ADDI    r5, r5, 1
        ADDI    r5, r5, 1
        ADDI    r5, r5, 1
        ADDI    r5, r5, 1               ; at 0x100000
        ADDI    r5, r5, 1
        JR      p4
s1e:

        .include "common.s"
