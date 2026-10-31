; The top of RAM: nothing exists at 0x100000.  Code copied to the last bytes
; of RAM may end there (a 16-bit instruction in the last parcel is fine and
; so is a jump back out of it), running on faults; data accesses may touch
; 0xFFFFF but not step over it.  Exit code 3 / halt FD is the fault.
; entries: 0x1000 0x1100 0x1200 0x1300 0x1400 0x1500 0x1600 0x1700 0x1800 0x1900 0x1A00 0x1B00 0x1C00 0x1D00 0x1E00 0x1F00

        .org    0x1000                 ; falls off the end of RAM: fault after the last instruction
        LA      p1, s0
        LA      p2, 0x100000 - (s0e - s0)
        LA      p3, 0x100000 - (s0e - s0)
        MOVI    r10, (s0e - s0) / 2
        BRA     copy

        .org    0x1100                 ; JR in the last parcel jumps back out
        LA      p1, s1
        LA      p2, 0x100000 - (s1e - s1)
        LA      p3, 0x100000 - (s1e - s1)
        LA      p4, fin
        MOVI    r10, (s1e - s1) / 2
        BRA     copy

        .org    0x1200                 ; B/16 in the last parcel, taken, back to a JR
        LA      p1, s2
        LA      p2, 0x100000 - (s2e - s2)
        LA      p3, 0x100000 - (s2e - s2) + 2
        LA      p4, fin
        MOVI    r10, (s2e - s2) / 2
        BRA     copy

        .org    0x1300                 ; JAL in the last two parcels: lr = 0x100000
        LA      p1, s3
        LA      p2, 0x100000 - (s3e - s3)
        LA      p3, 0x100000 - (s3e - s3) + 2
        LA      p4, fin
        MOVI    r10, (s3e - s3) / 2
        BRA     copy

        .org    0x1400                 ; B/32 whose offset from the instruction ends at 0x100000 + 0
        LA      p1, s4                 ; but whose target, 0xFFFFC, is in RAM
        LA      p2, 0x100000 - (s4e - s4)
        LA      p3, 0x100000 - (s4e - s4)
        LA      p4, fin
        MOVI    r10, (s4e - s4) / 2
        BRA     copy

        .org    0x1500                 ; B/32 to 0x100000: fault
        LA      p1, s5
        LA      p2, 0x100000 - (s5e - s5)
        LA      p3, 0x100000 - (s5e - s5)
        MOVI    r10, (s5e - s5) / 2
        BRA     copy

        .org    0x1600                 ; LDP across the end: fault
        LA      p2, 0x0FFFFE
        LDP     p1, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1700                 ; LDW at 0xFFFFE and LDBU at 0xFFFFF are fine
        LA      p2, 0x0FFFFE
        LDW     r3, [p2]
        LA      p2, 0x0FFFFF
        LDBU    r3, [p2]
        STB     r3, [p2]
        LA      p2, 0x0FFFFC
        LDP     p1, [p2]
        STP     p1, [p2]
        LDP     p1, [p2]+       ; ends with the pointer at 0x100000
        MOVI    r2, 0
        CALL    exit

        .org    0x1800                 ; STP across the end: fault
        LA      p2, 0x0FFFFE
        STP     p1, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1900                 ; the pointer moves to 0x100000, then is used: fault
        LA      p2, 0x0FFFFE
        LDW     r3, [p2]+
        LDBU    r4, [p2]
        MOVI    r2, 0
        CALL    exit

        .org    0x1A00                 ; indexed access ending past the end: fault
        LA      p2, 0x0FFFFF
        MOVI    r4, 1
        LDBU    r3, [p2 + r4*1]
        MOVI    r2, 0
        CALL    exit

        .org    0x1B00                 ; a 24-bit wrap is not a fault: 0xFFFFFF + 1 = 0
        LA      p2, 0xFFFFFF
        MOVI    r4, 1
        LDBU    r3, [p2 + r4*1]
        LDBU    r3, [p2 + 1]
        MOVI    r2, 0
        CALL    exit

        .org    0x1C00                 ; JR to 0x100000: fault
        LA      p3, 0x100000
        JR      p3

        .org    0x1D00                 ; JR to 0xFFFFE and back, JALR at the end
        LA      p1, s6
        LA      p2, 0x100000 - (s6e - s6)
        LA      p3, 0x100000 - (s6e - s6)
        LA      p4, fin
        MOVI    r10, (s6e - s6) / 2
        BRA     copy

        .org    0x1E00                 ; a 32-bit instruction in the last parcel: fault
        LA      p1, s7
        LA      p2, 0x100000 - (s7e - s7)
        LA      p3, 0x100000 - (s7e - s7)
        MOVI    r10, (s7e - s7) / 2
        BRA     copy

        .org    0x1F00                 ; LEAPC, ADDPI etc. relative to a pc at the end
        LA      p1, s8
        LA      p2, 0x100000 - (s8e - s8)
        LA      p3, 0x100000 - (s8e - s8)
        LA      p4, fin
        MOVI    r10, (s8e - s8) / 2
        BRA     copy

        .org    0x2000
copy:   LDW     r11, [p1]+
        STW     r11, [p2]+
        DEC     r10
        BNE     copy
        JR      p3

fin:    MOVP    p0, p6
        MOVI    r2, 0
        CALL    exit

        .org    0x2100
s0:     MOVI    r2, 1
        ADDI    r2, r2, 1
        ADDI    r2, r2, 2
s0e:
s1:     ADDI    r2, r2, 1
        JR      p4
s1e:
s2:     JR      p4
        ADDI    r2, r2, 1
        BRA     s2
s2e:
s3:     JR      p4
        ADDI    r2, r2, 1
        .word   0xF7FF, 0xFFFE          ; JAL back 4 bytes, to the JR
s3e:
s4:     .word   0xF700, 0x0006          ; B/32 AL, +12 bytes from here
        NOP
        NOP
        NOP
        NOP
        JR      p4
        NOP
s4e:
s5:     .word   0xF700, 0x0008          ; B/32 AL, +16 bytes: 0x100000
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
s5e:
s6:     ADDI    r2, r2, 1
        NOP
        JALR    p4                      ; lr = 0x100000
s6e:
s7:     NOP
        .word   0xE808, 0x0001          ; ADDI r0, r8, 1: needs both parcels
s7e:
s8:     LEAPC   p5, s8
        ADDPI   p5, p5, 2
        NOP
        JR      p4
s8e:

        .include "common.s"
