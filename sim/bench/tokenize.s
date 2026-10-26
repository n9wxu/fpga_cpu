; BASIC tokenizer: the inner loop of an interpreter's line entry.
; Scans program lines, matches keywords against a table, parses numbers,
; and folds everything into a 16-bit checksum. Repeated 50 times.
; expect: "48420\n"

        .org    0x1000
start:  LA      sp, 0x0F0000
        MOVI    r8, 50                  ; repetitions
        MOVI    r9, 0                   ; checksum
rep:    LA      p5, lines               ; p5 = current character
line:   LDBU    r2, [p5]
        TST     r2
        BEQ     rep_end                 ; empty line ends the program
scan:   LDBU    r2, [p5]
        TST     r2
        BEQ     line_end
        CMPI    r2, 'A'
        BLO     notletter
        CMPI    r2, 'Z' + 1
        BHS     notletter
        MOVP    p3, p5                  ; word start
wl:     ADDPI   p5, p5, 1
        LDBU    r2, [p5]
        CMPI    r2, 'A'
        BLO     wend
        CMPI    r2, 'Z' + 1
        BLO     wl
wend:   SUB     r4, r10, r6             ; word length
        CALL    lookup
        TST     r2
        BNE     havetok
        LDBU    r2, [p3]                ; identifier: use its first letter
havetok:
        ADD     r9, r9, r2
        BRA     scan
notletter:
        CMPI    r2, '0'
        BLO     other
        CMPI    r2, '9' + 1
        BHS     other
        MOVI    r3, 0                   ; number value
num:    MOVI    r4, 10
        MUL     r3, r3, r4
        ADDI    r2, r2, -'0'
        ADD     r3, r3, r2
        ADDPI   p5, p5, 1
        LDBU    r2, [p5]
        CMPI    r2, '0'
        BLO     numend
        CMPI    r2, '9' + 1
        BLO     num
numend: ADD     r9, r9, r3
        BRA     scan
other:  ADD     r9, r9, r2
        ADDPI   p5, p5, 1
        BRA     scan
line_end:
        ADDPI   p5, p5, 1               ; skip the terminator
        BRA     line
rep_end:
        DEC     r8
        BNE     rep
        MOV     r2, r9
        CALL    print_dec
        CALL    newline
        MOVI    r2, 0
        CALL    exit

; lookup: word at p3, length r4 -> r2 = token (0x81 + index) or 0.
; Preserves p3 and r8-r11.
lookup: PUSH    r8
        LA      p1, keywords
        MOVI    r5, 0x81
lk_next:
        LDBU    r0, [p1]
        TST     r0
        BEQ     lk_fail
        MOVI    r1, 0                   ; i
lk_cmp: CMP     r1, r4
        BEQ     lk_endw
        LDBU    r0, [p1 + r1]
        LDBU    r8, [p3 + r1]
        CMP     r0, r8
        BNE     lk_skip
        INC     r1
        BRA     lk_cmp
lk_endw:
        LDBU    r0, [p1 + r1]           ; keyword must end here too
        TST     r0
        BEQ     lk_found
lk_skip:
        LDBU    r0, [p1]+
        TST     r0
        BNE     lk_skip
        INC     r5
        BRA     lk_next
lk_found:
        MOV     r2, r5
        POP     r8
        RET
lk_fail:
        MOVI    r2, 0
        POP     r8
        RET

keywords:
        .asciz  "PRINT"
        .asciz  "GOTO"
        .asciz  "GOSUB"
        .asciz  "RETURN"
        .asciz  "IF"
        .asciz  "THEN"
        .asciz  "FOR"
        .asciz  "TO"
        .asciz  "NEXT"
        .asciz  "LET"
        .asciz  "END"
        .asciz  "INPUT"
        .asciz  "REM"
        .asciz  "STEP"
        .asciz  "DIM"
        .asciz  "DATA"
        .asciz  "READ"
        .asciz  "AND"
        .asciz  "OR"
        .asciz  "NOT"
        .byte   0

lines:  .asciz  "10 PRINT 'HELLO' ; X"
        .asciz  "20 FOR I = 1 TO 100 STEP 2"
        .asciz  "30 LET A = A + I * 3"
        .asciz  "40 NEXT I"
        .asciz  "50 IF A > 1000 THEN GOTO 80"
        .asciz  "60 GOSUB 200"
        .asciz  "70 GOTO 20"
        .asciz  "80 PRINT A AND NOT B OR C"
        .asciz  "200 REM SUBROUTINE"
        .asciz  "210 RETURN"
        .asciz  "220 END"
        .byte   0
        .align  2

        .include "lib.s"
