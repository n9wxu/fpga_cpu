; Self-modifying code: a store to the next instruction (or the one after)
; is seen by it, with and without SYNC, as in the simulator.
        .org    0x1000
start:  LA      sp, 0x0F0000
        ; patch the immediate byte of the next 16-bit MOVI
        LA      p2, p1a
        MOVI    r3, 0x55
        STB     r3, [p2]
p1a:    MOVI    r4, 1
        ; patch the second parcel of the next 32-bit MOVI
        LA      p2, p2a + 2
        LI      r3, 0x1234
        STW     r3, [p2]
p2a:    MOVI    r5, 0x7777
        ; patch the whole next parcel: MOVI r7, 9
        LA      p2, p3a
        LI      r3, 0x2709
        STW     r3, [p2]
p3a:    MOVI    r7, 1
        ; the same through a pointer that moves: STB [p]+ and STW -[p]
        LA      p2, p4a
        MOVI    r3, 0x66
        STB     r3, [p2]+
p4a:    MOVI    r8, 1
        LA      p2, p5a + 2
        LI      r3, 0x2621
        STW     r3, -[p2]
p5a:    NOP
        NOP
        ; patch two instructions ahead, then a SYNC
        LA      p2, p6a
        LI      r3, 0x2A0B
        STW     r3, [p2]
        NOP
p6a:    MOVI    r9, 1
        LA      p2, p7a
        LI      r3, 0x2B0C
        STW     r3, [p2]
        SYNC
p7a:    MOVI    r10, 1
        ; a 32-bit store (STP) over the next two instructions
        LA      p2, p8a
        LI      r2, 0x2C0D
        LI      r3, 0x2D0E
        STP     p1, [p2]
p8a:    MOVI    r12, 1
        MOVI    r13, 1
        ; patching the store's own next-instruction through a long displacement
        LA      p2, p9a - 20
        LI      r3, 0x2E0F
        STW     r3, [p2 + 20]
p9a:    MOVI    r11, 1
        ; a pair store that starts on the store itself and reaches the next
        ; instruction on its third byte
        LA      p2, p11a - 2
        LI      r2, 0x1111
        LI      r3, 0x2910
        STP     p1, [p2]
p11a:   MOVI    r12, 1
        ; a byte store to the second byte of the next parcel, and a word
        ; store to the parcel before it (not the next one: no effect)
        LA      p2, p12a + 1
        MOVI    r3, 0x2A
        STB     r3, [p2]
p12a:   MOVI    r5, 7
        ; a loop that rewrites its own body once
        LA      p2, lpa
        LI      r3, 0x1E01      ; ADDI r14, 1  (ADDI rd, imm8 = 0001 dddd iiiiiiii)
        MOVI    r14, 0
        MOVI    r6, 3
lp:     STW     r3, [p2]
lpa:    ADDI    r14, 2
        DEC     r6
        BNE     lp
        MOVI    r2, 0
        CALL    exit
        .include "common.s"
