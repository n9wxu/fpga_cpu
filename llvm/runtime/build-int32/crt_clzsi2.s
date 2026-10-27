	.text
	.globl	__clzsi2                        ; -- Begin function __clzsi2
	.p2align	1
__clzsi2:                               ; @__clzsi2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r5, 0
	MOVI	r10, 1
	MOV	r11, r5
	MOV	r4, r3
	MOV	r8, r5
	MOV	r9, r5
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	CMP	r4, r8
	CPC	r5, r9
	MOVP	p2, p5
	BEQ	.LBB0_2
; %bb.1:                                ; %entry
	MOVP	p2, p4
.LBB0_2:                                ; %entry
	SHLI	r6, r5, 4
	SHRI	r7, r4, 12
	OR	r7, r6, r7
	SHLI	r6, r4, 4
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	XORI	r4, r6, 16
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, -256
	CMP	r4, r8
	CPC	r5, r9
	MOVP	p2, p5
	BEQ	.LBB0_4
; %bb.3:                                ; %entry
	MOVP	p2, p4
.LBB0_4:                                ; %entry
	SHLI	r6, r5, 3
	SHRI	r7, r4, 13
	OR	r7, r6, r7
	SHLI	r6, r4, 3
	STP	p3, [p7]                        ; 4-byte Folded Spill
	XORI	r4, r6, 8
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, 240
	CMP	r4, r8
	CPC	r5, r9
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	BEQ	.LBB0_6
; %bb.5:                                ; %entry
	MOVP	p2, p4
.LBB0_6:                                ; %entry
	SHLI	r6, r5, 2
	SHRI	r7, r4, 14
	OR	r11, r6, r7
	SHLI	r10, r4, 2
	XORI	r4, r10, 4
	JAL	__lshrsi3
	ANDI	r5, r3, 0
	ANDI	r4, r2, 12
	CMP	r4, r8
	CPC	r5, r9
	BEQ	.LBB0_8
; %bb.7:                                ; %entry
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
.LBB0_8:                                ; %entry
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	SHLI	r4, r7, 1
	SHRI	r5, r6, 15
	OR	r9, r4, r5
	SHLI	r8, r6, 1
	XORI	r4, r8, 2
	JAL	__lshrsi3
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	MOVI	r4, 2
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	OR	r5, r13, r7
	OR	r4, r12, r6
	OR	r7, r5, r11
	OR	r6, r4, r10
	SHRI	r4, r2, 1
	SHLI	r5, r3, 15
	OR	r12, r4, r5
	SHRI	r13, r3, 1
	OR	r5, r7, r9
	OR	r4, r6, r8
	LDP	p3, [p7 + 16]                   ; 4-byte Folded Reload
	SUB	r2, r6, r2
	SBC	r3, r7, r3
	ORI	r7, r13, -1
	ORI	r6, r12, -2
	ADDPI	p3, p3, 1
	AND	r13, r7, r3
	AND	r12, r6, r2
	ADDPP	p1, p2, p6
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
