; sid6502 JIT run-time glue: entering and leaving translated code.
;
; Register map inside translated code (see ../README.md):
;   r0,r1  scratch T0,T1      p1 (r2,r3) = mem6502 + 0x8000     p2 (r4,r5) = attr6502 + 0x8000
;   r6 A   r7 X   r8 Y   r9 S   r10 RES (N/Z)   r11 C   r12 V   r13 CYC (cycle budget, counts down)
;   sp = p7 is the C stack and is never touched by translated code.
;
; struct jst (jit.c): 0 entry(32) 4 memb(32) 8 attrb(32) 12 a 14 x 16 y 18 s 20 res 22 c 24 v 26 cyc 28 t0 30 t1
        .text
        .globl  jit_enter
jit_enter:
        STP     p4, -[sp]
        STP     p5, -[sp]
        STP     p6, -[sp]
        LA      p0, jst
        LDP     p1, [p0 + 4]
        LDP     p2, [p0 + 8]
        LDW     r6, [p0 + 12]
        LDW     r7, [p0 + 14]
        LDW     r8, [p0 + 16]
        LDW     r9, [p0 + 18]
        LDW     r10, [p0 + 20]
        LDW     r11, [p0 + 22]
        LDW     r12, [p0 + 24]
        LDW     r13, [p0 + 26]
        LDP     p0, [p0]
        JR      p0

; translated code leaves with r1 = event number, r0 = T0 (kept for the handler)
        .globl  jit_exit
jit_exit:
        STP     p0, -[sp]
        LA      p0, jst
        STW     r6, [p0 + 12]
        STW     r7, [p0 + 14]
        STW     r8, [p0 + 16]
        STW     r9, [p0 + 18]
        STW     r10, [p0 + 20]
        STW     r11, [p0 + 22]
        STW     r12, [p0 + 24]
        STW     r13, [p0 + 26]
        LDP     p1, [sp]+
        STW     r2, [p0 + 28]
        STW     r3, [p0 + 30]
        LDP     p6, [sp]+
        LDP     p5, [sp]+
        LDP     p4, [sp]+
        RET

        .globl  jit_sync
jit_sync:
        SYNC
        RET

; Where translated code can branch to without a register: the address of jit_exit as a word
; (the branch encoding is pc-relative and reaches the whole address space)
