; long __mulsi3(long a, long b): 32 x 32 -> low 32 bits.
; a = p1 (r3:r2), b = p2 (r5:r4), result p1.
        .text
        .globl  __mulsi3
__mulsi3:
        MUL     r6, r2, r5              ; alo * bhi (low half)
        MUL     r7, r3, r4              ; ahi * blo (low half)
        ADD     r6, r6, r7
        MULHU   r7, r2, r4              ; high half of alo * blo
        MUL     r2, r2, r4              ; low half of alo * blo
        ADD     r3, r7, r6
        RET
