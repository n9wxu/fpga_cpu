; Interrupts (docs/t1.md "Interrupts"), in lockstep with fsim: the
; software source of the controller at F00700, entry (EPC, IE cleared, to
; 0x000010), RETI (back, IE set), IE as MTSR leaves it, no nesting with IE
; clear, and a handler that keeps the flags itself (MFSR/MTSR SR0).
; cores: t1s t1f
        .org    0x1000
start:  LA      sp, 0x0F0000
        ; the vector: a long branch to the handler at 0x000010
        LA      p2, 0x10
        LI      r3, 0xF700 | (((handler - 0x10) >> 17) & 0x7F)
        STW     r3, [p2]
        LI      r3, ((handler - 0x10) >> 1) & 0xFFFF
        STW     r3, [p2 + 2]
        LA      p3, 0xF00700
        LI      r3, 0xFF
        STW     r3, [p3]                ; nothing pending
        LI      r3, 0x80
        STW     r3, [p3 + 2]            ; the software source enabled
        MOVI    r9, 0                   ; interrupts taken
        MOVI    r10, 0                  ; the handler's mode
        ; IE clear: raised, not taken
        STW     r3, [p3 + 4]
        NOP
        NOP
        ; IE set by MTSR: taken before the next instruction
        MOVI    r4, 1
        MTSR    sr6, r4
        ADDI    r5, r5, 1               ; runs after the return
        ; the flags survive: Z set, an interrupt, then a branch on Z
        MOVI    r5, 3
        CMPI    r5, 3
        STW     r3, [p3 + 4]
        BEQ     z_ok
        MOVI    r2, 10
        CALL    exit
z_ok:   ; an interrupt with the flags clear (Z) comes back to them clear
        MOVI    r10, 1
        CMPI    r5, 4
        STW     r3, [p3 + 4]
        BNE     z2_ok
        MOVI    r2, 12
        CALL    exit
z2_ok:  MOVI    r12, 0x66
        ; a pending interrupt with IE cleared again is not taken
        MOVI    r4, 0
        MTSR    sr6, r4
        MOVI    r10, 0
        STW     r3, [p3 + 4]
        NOP
        CMPI    r9, 3
        BEQ     n_ok
        MOVI    r2, 11
        CALL    exit
n_ok:   MOVI    r2, 0
        CALL    exit

handler:
        MFSR    r0, sr0                 ; the flags, before anything sets them
        ADDI    r9, r9, 1
        LI      r1, 0x80                ; (r0-r1 are p0: p3, r6-r7, holds F00700)
        STW     r1, [p3]                ; cleared
        CMPI    r10, 1                  ; (changes the flags: restored below)
        MTSR    sr0, r0
        RETI
        .include "common.s"
