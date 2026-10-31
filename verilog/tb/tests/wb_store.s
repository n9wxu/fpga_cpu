; A store's data is read before the instruction's own pointer write-back:
; STW r4,[p2]+ with r4 in p2 stores the old r4, STP p3,-[p3] the old p3.
; (For a load the loaded value wins.) p2 = r5:r4, p3 = r7:r6.
buf = 0x6000

        .org    0x1000
start:  LA      sp, 0x0F0000
        LA      p1, buf                 ; p1 = r3:r2 reads the stores back
        ; 16-bit STW [p]+ and STW -[p], the data register in the pointer
        LA      p2, buf + 0x10
        STW     r4, [p2]+
        LDW     r8, [p1 + 0x10]
        STW     r5, [p2]+
        LDW     r8, [p1 + 0x12]
        STW     r4, -[p2]
        LDW     r8, [p1 + 0x12]
        STW     r5, -[p2]
        LDW     r8, [p1 + 0x10]
        LA      p2, buf + 0x20
        STB     r4, [p2]+
        LDBU    r8, [p1 + 0x20]
        STB     r5, [p2]+
        LDBU    r8, [p1 + 0x21]
        ; 32-bit forms
        LA      p2, buf + 0x30
        STW     r4, [p2]+
        LDW     r8, [p1 + 0x30]
        STW     r5, -[p2]
        LDW     r8, [p1 + 0x30]
        LA      p2, buf + 0x40
        STB     r5, [p2]+
        LDBU    r8, [p1 + 0x40]
        LA      p2, buf + 0x50
        STB     r4, -[p2]
        LDBU    r8, [p1 + 0x4F]
        ; STP with the data pair being the pointer
        LA      p3, buf + 0x60
        STP     p3, [p3]+
        LDP     p4, [p1 + 0x60]
        LA      p3, buf + 0x74
        STP     p3, -[p3]
        LDP     p4, [p1 + 0x70]
        ; STP with half of the pair in the pointer
        LA      p2, buf + 0x80
        STP     p1, [p2]+
        LDP     p4, [p1 + 0x80]
        ; data register in a different pair: ordinary
        LA      p2, buf + 0x90
        LI      r6, 0x1357
        STW     r6, [p2]+
        LDW     r8, [p1 + 0x90]
        ; loads: the loaded value wins over the write-back
        LA      p2, buf + 0x10
        LDW     r4, [p2]+
        LA      p2, buf + 0x10
        LDW     r5, [p2]+
        LA      p2, buf + 0x10
        LDBU    r4, [p2]+
        LA      p3, buf + 0x60
        LDP     p3, [p3]+
        LA      p3, buf + 0x64
        LDP     p3, -[p3]
        ; the scaled-index and displacement forms never wrote back
        LA      p2, buf + 0xA0
        MOVI    r6, 4
        STW     r4, [p2 + r6*2]
        STW     r5, [p2 + 6]
        LDW     r8, [p1 + 0xA8]
        LDW     r8, [p1 + 0xA6]
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
