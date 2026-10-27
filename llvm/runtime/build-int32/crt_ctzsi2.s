	.text
	.globl	__ctzsi2                        ; -- Begin function __ctzsi2
	.p2align	1
__ctzsi2:                               ; @__ctzsi2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r8, 0
	MOVI	r10, 1
	MOV	r11, r8
	MOV	r9, r8
	CMPI	r2, 0
	MOVP	p6, p5
	BEQ	.LBB0_2
; %bb.1:                                ; %entry
	MOVP	p6, p4
.LBB0_2:                                ; %entry
	SHLI	r6, r13, 4
	SHRI	r7, r12, 12
	OR	r5, r6, r7
	SHLI	r4, r12, 4
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	CMP	r4, r8
	CPC	r5, r9
	MOVP	p6, p5
	BEQ	.LBB0_4
; %bb.3:                                ; %entry
	MOVP	p6, p4
.LBB0_4:                                ; %entry
	SHLI	r6, r13, 3
	SHRI	r7, r12, 13
	OR	r5, r6, r7
	SHLI	r4, r12, 3
	STP	p2, [p7 + 4]                    ; 4-byte Folded Spill
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, 15
	CMP	r4, r8
	CPC	r5, r9
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	BEQ	.LBB0_6
; %bb.5:                                ; %entry
	MOVP	p2, p4
.LBB0_6:                                ; %entry
	SHLI	r6, r5, 2
	SHRI	r7, r4, 14
	OR	r11, r6, r7
	SHLI	r10, r4, 2
	MOV	r4, r10
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, 3
	CMP	r4, r8
	CPC	r5, r9
	BEQ	.LBB0_8
; %bb.7:                                ; %entry
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
.LBB0_8:                                ; %entry
	LDP	p3, [p7 + 16]                   ; 4-byte Folded Reload
	SHLI	r4, r7, 1
	SHRI	r5, r6, 15
	STP	p4, [p7]                        ; 4-byte Folded Spill
	OR	r9, r4, r5
	SHLI	r8, r6, 1
	MOV	r4, r8
	JAL	__lshrsi3
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	OR	r7, r13, r5
	OR	r6, r12, r4
	OR	r5, r7, r11
	OR	r4, r6, r10
	ORI	r7, r3, -1
	ORI	r6, r2, -2
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	SHLI	r13, r2, 14
	OR	r3, r5, r9
	OR	r2, r4, r8
	ADDPI	p2, p3, 1
	SARI	r6, r13, 15
	MOV	r7, r6
	ADDPI	p3, p3, 2
	AND	r13, r7, r5
	AND	r12, r6, r4
	ADDPP	p1, p1, p6
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
