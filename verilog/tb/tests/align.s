; Alignment: words and pairs must be on even addresses (fault, exit code 3
; in fsim, halt FD in T1); bytes may be anywhere.
; entries: 0x1000 0x1100 0x1200 0x1300 0x1400 0x1500 0x1600 0x1700 0x1800 0x1900 0x1A00 0x1B00
buf = 0x6000

        .org    0x1000                 ; legal: bytes at odd addresses, words and pairs on even ones,
        LA      sp, 0x0F0000           ; a pair at 2 mod 4, LEA of an odd address
        LA      p2, buf + 1
        LDBU    r3, [p2]
        STB     r3, [p2]+
        LDBU    r3, [p2]
        LA      p2, buf + 2
        LDP     p1, [p2]
        STP     p1, [p2 + 4]
        LDW     r6, [p2]+
        STW     r6, -[p2]
        LA      p2, buf + 5
        LEA     p3, [p2 + 1]
        MOVI    r2, 0
        CALL    exit

        .org    0x1100                 ; LDW at an odd address
        LA      p2, buf + 1
        LDW     r3, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1200                 ; STW at an odd address
        LA      p2, buf + 3
        STW     r3, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1300                 ; LDP at an odd address
        LA      p2, buf + 1
        LDP     p1, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1400                 ; STP at an odd address
        LA      p2, buf + 7
        STP     p1, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1500                 ; LDW [p]+ at an odd address
        LA      p2, buf + 1
        LDW     r3, [p2]+
        MOVI    r2, 0
        CALL    exit

        .org    0x1600                 ; STW -[p] ending on an odd address
        LA      p2, buf + 4
        STW     r3, -[p2]
        STW     r3, -[p2]
        STB     r3, -[p2]
        STW     r3, -[p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1700                 ; LDW [p + r*2] from an odd base
        LA      p2, buf + 1
        MOVI    r4, 8
        LDW     r3, [p2 + r4*2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1800                 ; LDW [p + 3]: odd displacement (32-bit form)
        LA      p2, buf
        LDW     r3, [p2 + 3]
        MOVI    r2, 0
        CALL    exit

        .org    0x1900                 ; LDP [p]+ (32-bit form) at an odd address
        LA      p2, buf + 9
        LDP     p1, [p2]+
        MOVI    r2, 0
        CALL    exit

        .org    0x1A00                 ; STP [p + r*4] with an odd index
        LA      p2, buf
        MOVI    r4, 3
        STP     p1, [p2 + r4*1]
        MOVI    r2, 0
        CALL    exit

        .org    0x1B00                 ; LDBS then LDW from the same odd pointer
        LA      p2, buf + 1
        LDBS    r3, [p2]
        LDBU    r3, [p2]
        STB     r3, [p2]
        LDW     r3, [p2]
        MOVI    r2, 0
        CALL    exit

        .include "common.s"
