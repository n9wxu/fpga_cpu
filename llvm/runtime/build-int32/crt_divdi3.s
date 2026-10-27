	.text
	.globl	__divdi3                        ; -- Begin function __divdi3
	.p2align	1
__divdi3:                               ; @__divdi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -44
	STP	p4, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 44]
	SARI	r8, r13, 15
	MOV	r9, r8
	XOR	r2, r8, r12
	XOR	r3, r9, r13
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	XOR	r11, r9, r7
	XOR	r10, r8, r6
	SHRI	r12, r13, 15
	MOVI	r13, 0
	ADDPP	p1, p5, p6
	MOVI	r6, 1
	MOV	r7, r13
	MOV	r4, r13
	MOV	r5, r13
	STP	p1, [p7 + 12]                   ; 4-byte Folded Spill
	CMP	r2, r10
	CPC	r3, r11
	STP	p3, [p7 + 32]                   ; 4-byte Folded Spill
	BLO	.LBB0_2
; %bb.1:                                ; %entry
	MOVP	p3, p2
.LBB0_2:                                ; %entry
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p3
	STP	p1, [p7]
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 4]
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	SARI	r10, r3, 15
	MOV	r11, r10
	XOR	r4, r10, r2
	XOR	r5, r11, r3
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	XOR	r7, r11, r5
	XOR	r6, r10, r4
	SHRI	r12, r3, 15
	ADDPP	p1, p3, p6
	CMP	r2, r6
	CPC	r3, r7
	LDP	p3, [p7 + 32]                   ; 4-byte Folded Reload
	BLO	.LBB0_4
; %bb.3:                                ; %entry
	LDP	p3, [p7 + 24]                   ; 4-byte Folded Reload
.LBB0_4:                                ; %entry
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPP	p2, p2, p3
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	JAL	__udivmoddi4
	XOR	r7, r9, r11
	XOR	r6, r8, r10
	XOR	r13, r3, r7
	XOR	r12, r2, r6
	CMP	r12, r6
	CPC	r13, r7
	BLO	.LBB0_6
; %bb.5:                                ; %entry
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
.LBB0_6:                                ; %entry
	XOR	r3, r5, r7
	XOR	r2, r4, r6
	SUB	r2, r2, r6
	SBC	r3, r3, r7
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	SUB	r4, r2, r4
	SBC	r5, r3, r5
	SUB	r2, r12, r6
	SBC	r3, r13, r7
	LDP	p6, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 40]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 36]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 44
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
