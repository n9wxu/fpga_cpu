; Benchmark support library.
;
; Calling convention:
;   arguments r2-r5 (p1-p2), result r2 (or p1)
;   caller-saved r0-r7 (p0-p3) and lr; callee-saved r8-r11 (p4-p5)
;   lr = p6 (r12:r13), sp = p7 (r14:r15), stack grows down

IO_BASE = 0xF00000              ; console at +0, exit at +2

; putc: write the character in r2. Clobbers p0.
putc:   LA      p0, IO_BASE
        STB     r2, [p0]
        RET

; puts: write the NUL-terminated string at p1. Clobbers r4, p0, p1.
puts:   LA      p0, IO_BASE
puts1:  LDBU    r4, [p1]+
        TST     r4
        BEQ     puts2
        STB     r4, [p0]
        BRA     puts1
puts2:  RET

; newline: write "\n". Clobbers r2, p0.
newline:
        MOVI    r2, 10
        LA      p0, IO_BASE
        STB     r2, [p0]
        RET

; print_dec: write r2 as an unsigned decimal number.
; Divides by 10 with a multiply: n/10 = (n * 0xCCCD) >> 19 for any 16-bit n.
; Clobbers r2-r7, p0.
print_dec:
        LA      p0, IO_BASE
        LA      p2, decbuf_end
pd_loop:
        LI      r7, 0xCCCD
        MULHU   r6, r2, r7
        SHRI    r6, r6, 3               ; r6 = n / 10
        MOVI    r7, 10
        MUL     r7, r6, r7
        SUB     r7, r2, r7              ; r7 = n % 10
        ADDI    r7, r7, '0'
        STB     r7, -[p2]
        MOV     r2, r6
        TST     r2
        BNE     pd_loop
pd_out: LDBU    r6, [p2]+
        TST     r6
        BEQ     pd_done
        STB     r6, [p0]
        BRA     pd_out
pd_done:
        RET

; print_hex32: write p1 (r3:r2) as 8 hex digits. Clobbers r2-r7, p0.
print_hex32:
        LA      p0, IO_BASE
        MOV     r4, r2                  ; low half for later
        MOV     r2, r3                  ; high half first
        MOVI    r7, 2
ph_half:
        MOVI    r6, 4
ph_nib: SHRI    r5, r2, 12
        ADDI    r5, r5, '0'
        CMPI    r5, '9' + 1
        BLT     ph_dig
        ADDI    r5, r5, 7               ; 'A' - '0' - 10
ph_dig: STB     r5, [p0]
        SHLI    r2, r2, 4
        DEC     r6
        BNE     ph_nib
        MOV     r2, r4
        DEC     r7
        BNE     ph_half
        RET

; exit: stop with exit code r2.
exit:   LA      p0, IO_BASE
        STW     r2, [p0 + 2]
halt:   BRA     halt

decbuf: .space  6
decbuf_end:
        .byte   0
        .align  2
