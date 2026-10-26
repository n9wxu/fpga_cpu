; 32-bit shifts by a variable amount: value p1 (r3:r2), count r4.
        .text
        .globl  __ashlsi3
__ashlsi3:
        ANDI    r4, r4, 31
        CMPI    r4, 16
        BHS     .Lshl_big
        CMPI    r4, 0
        BEQ     .Lshl_ret
        MOVI    r5, 16
        SUB     r5, r5, r4
        SHR     r6, r2, r5              ; bits that move from lo into hi
        SHL     r3, r3, r4
        OR      r3, r3, r6
        SHL     r2, r2, r4
.Lshl_ret:
        RET
.Lshl_big:
        ADDI    r4, r4, -16
        SHL     r3, r2, r4
        MOVI    r2, 0
        RET

        .globl  __lshrsi3
__lshrsi3:
        ANDI    r4, r4, 31
        CMPI    r4, 16
        BHS     .Lshr_big
        CMPI    r4, 0
        BEQ     .Lshr_ret
        MOVI    r5, 16
        SUB     r5, r5, r4
        SHL     r6, r3, r5              ; bits that move from hi into lo
        SHR     r2, r2, r4
        OR      r2, r2, r6
        SHR     r3, r3, r4
.Lshr_ret:
        RET
.Lshr_big:
        ADDI    r4, r4, -16
        SHR     r2, r3, r4
        MOVI    r3, 0
        RET

        .globl  __ashrsi3
__ashrsi3:
        ANDI    r4, r4, 31
        CMPI    r4, 16
        BHS     .Lsar_big
        CMPI    r4, 0
        BEQ     .Lsar_ret
        MOVI    r5, 16
        SUB     r5, r5, r4
        SHL     r6, r3, r5
        SHR     r2, r2, r4
        OR      r2, r2, r6
        SAR     r3, r3, r4
.Lsar_ret:
        RET
.Lsar_big:
        ADDI    r4, r4, -16
        SAR     r2, r3, r4
        SARI    r3, r3, 15
        RET
