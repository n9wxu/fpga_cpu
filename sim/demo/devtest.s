; Device test (headless): keyboard queue, file service, board I/O.
; Run: fsim --keys "Hi" --disk DIR devtest.bin  (DIR must start empty)
; expect: "Hi 65535\n0 9\n9 hello fs\n\nHELLO.TXT 9\n0 65535 65531\n165\n"

        .org    0x1000
IO      = 0xF00000
KBD     = 0x100
FS      = 0x500                 ; + 0 cmd, 4 handle, 6 mode, 8 name, C buf, 10 len, 12 result, 14 size
LEDS    = 0x600

start:  LA      sp, 0x080000
        LA      p5, IO
        ; keyboard: two keys, then the empty marker
        LDW     r2, [p5 + KBD]
        CALL    putc
        LDW     r2, [p5 + KBD]
        CALL    putc
        MOVI    r2, ' '
        CALL    putc
        LDW     r2, [p5 + KBD]
        CALL    print_dec
        CALL    newline

        ; create HELLO.TXT (lower-case name on purpose) and write 9 bytes
        LA      p1, fname
        STP     p1, [p5 + FS + 8]
        MOVI    r2, 1
        STW     r2, [p5 + FS + 6]       ; mode: write
        STW     r2, [p5 + FS]           ; OPEN
        LDW     r2, [p5 + FS + 0x12]
        STW     r2, [p5 + FS + 4]
        CALL    print_dec               ; handle: 0, the first free one
        MOVI    r2, ' '
        CALL    putc
        LA      p1, msg
        STP     p1, [p5 + FS + 0x0C]
        MOVI    r2, 9
        STW     r2, [p5 + FS + 0x10]
        MOVI    r2, 4
        STW     r2, [p5 + FS]           ; WRITE
        LDW     r2, [p5 + FS + 0x12]
        CALL    print_dec
        CALL    newline
        MOVI    r2, 2
        STW     r2, [p5 + FS]           ; CLOSE

        ; read it back
        MOVI    r2, 0
        STW     r2, [p5 + FS + 6]       ; mode: read
        MOVI    r2, 1
        STW     r2, [p5 + FS]           ; OPEN
        LDW     r2, [p5 + FS + 0x12]
        STW     r2, [p5 + FS + 4]
        LA      p1, buf
        STP     p1, [p5 + FS + 0x0C]
        MOVI    r2, 64
        STW     r2, [p5 + FS + 0x10]
        MOVI    r2, 3
        STW     r2, [p5 + FS]           ; READ
        LDW     r2, [p5 + FS + 0x12]
        CALL    print_dec
        MOVI    r2, ' '
        CALL    putc
        LA      p1, buf
        CALL    puts
        MOVI    r2, 2
        STW     r2, [p5 + FS]           ; CLOSE
        CALL    newline

        ; directory listing
        MOVI    r2, 6
        STW     r2, [p5 + FS]           ; DIR_OPEN
dir:    LA      p1, buf
        STP     p1, [p5 + FS + 0x0C]
        MOVI    r2, 7
        STW     r2, [p5 + FS]           ; DIR_NEXT
        LDW     r2, [p5 + FS + 0x12]
        TST     r2
        BEQ     dirend
        LA      p1, buf
        CALL    puts
        MOVI    r2, ' '
        CALL    putc
        LDW     r2, [p5 + FS + 0x14]
        CALL    print_dec
        CALL    newline
        BRA     dir

        ; delete, then open again (not found = -1), then a bad name (-5)
dirend: LA      p1, fname
        STP     p1, [p5 + FS + 8]
        MOVI    r2, 5
        STW     r2, [p5 + FS]           ; DELETE
        LDW     r2, [p5 + FS + 0x12]
        CALL    print_dec
        MOVI    r2, ' '
        CALL    putc
        MOVI    r2, 0
        STW     r2, [p5 + FS + 6]
        MOVI    r2, 1
        STW     r2, [p5 + FS]           ; OPEN for reading: gone
        LDW     r2, [p5 + FS + 0x12]
        CALL    print_dec
        MOVI    r2, ' '
        CALL    putc
        LA      p1, badname
        STP     p1, [p5 + FS + 8]
        MOVI    r2, 1
        STW     r2, [p5 + FS]
        LDW     r2, [p5 + FS + 0x12]
        CALL    print_dec
        CALL    newline

        ; board: LEDs read back
        LI      r2, 0xA5
        STW     r2, [p5 + LEDS]
        LDW     r2, [p5 + LEDS]
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

fname:  .asciz  "hello.txt"
badname: .asciz "toolongname.txt"
msg:    .ascii  "hello fs\n"
        .align  2
buf:    .space  64
        .include "../bench/lib.s"
