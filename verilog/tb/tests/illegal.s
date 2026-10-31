; Reserved encodings trap as illegal instructions (fsim exit 254, T1 halt FE).
; Each entry is one raw instruction; first parcel first (little-endian words).
; entries: 0x1000 0x1010 0x1020 0x1030 0x1040 0x1050 0x1060 0x1070 0x1080 0x1090 0x10A0 0x10B0 0x10C0 0x10D0 0x10E0 0x1110 0x1120 0x1130 0x1140
        .org    0x1000
        .word   0xE223, 0x0300          ; LDW [p1 + r0*s]: scale code 3
        .org    0x1010
        .word   0xE223, 0x0001          ; LDW [p1 + r0*1]: reserved low byte of the second parcel
        .org    0x1020
        .word   0xE723, 0x0800          ; LEA p1, [p1]+: LEA has no pi mode
        .org    0x1030
        .word   0xEE32, 0x0001          ; ADDPI pD with an odd pair number
        .org    0x1040
        .word   0xE330, 0x0000          ; LDP into an odd register number
        .org    0x1050
        .word   0x9F00                  ; Bcc with condition 15
        .org    0x1060
        .word   0xA008                  ; JR with bits set in the register field
        .org    0x1070
        .word   0xA580                  ; MOVP with a reserved bit
        .org    0x1080
        .word   0xFB23, 0x4101          ; MULHU with a reserved low byte
        .org    0x1090
        .word   0xF831, 0x0800          ; CMP rs, rt with the d field set
        .org    0x10A0
        .word   0xFC21, 0x3000          ; FSHR from an odd pair number
        .org    0x10B0
        .word   0xFB32, 0x3200          ; MULPU into an odd pair number
        .org    0x10C0
        .word   0xFD21, 0x0001          ; CLZ with a reserved bit
        .org    0x10D0
        .word   0xFD21, 0x0100          ; DSTEP with an odd source pair
        .org    0x10E0
        .word   0xE223, 0x0B00          ; LDW with an unassigned mode (second parcel 0B00)
        .org    0x1110
        .word   0xF820, 0x1700          ; MOV with a nonzero t field
        .org    0x1120
        .word   0xF920, 0x0310          ; SHLI/SHRI/SARI with f = 3
        .org    0x1130
        .word   0xFA21, 0x0000          ; ADDP with an odd source pair
        .org    0x1140
        .word   0xA3FF                  ; MISC 3 with an unassigned sub-opcode
