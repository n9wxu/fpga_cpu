	.text
	.globl	__moddi3                        ; -- Begin function __moddi3
	.p2align	1
__moddi3:                               ; @__moddi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -48
	STP	p4, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	LDP	p5, [p7 + 48]
	SARI	r12, r11, 15
	MOV	r13, r12
	XOR	r8, r12, r6
	XOR	r4, r12, r10
	XOR	r9, r13, r7
	XOR	r5, r13, r11
	SHRI	r10, r11, 15
	MOVI	r11, 0
	ADDPP	p1, p4, p5
	MOVI	r6, 1
	MOV	r7, r11
	MOV	r12, r11
	MOV	r13, r11
	STP	p6, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 12]                   ; 4-byte Folded Spill
	CMP	r2, r8
	CPC	r3, r9
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p4, p3
	BLO	.LBB0_2
; %bb.1:                                ; %entry
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
.LBB0_2:                                ; %entry
	ADDPP	p1, p2, p4
	LEA	p6, [p7 + 32]
	STP	p6, [p7 + 4]
	STP	p1, [p7]
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	SARI	r8, r5, 15
	MOV	r9, r8
	XOR	r6, r8, r4
	XOR	r7, r9, r5
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	XOR	r13, r9, r3
	XOR	r12, r8, r2
	SHRI	r10, r5, 15
	ADDPP	p1, p6, p5
	CMP	r2, r12
	CPC	r3, r13
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	BLO	.LBB0_4
; %bb.3:                                ; %entry
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
.LBB0_4:                                ; %entry
	ADDPP	p2, p3, p2
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	JAL	__udivmoddi4
	LDP	p1, [p7 + 32]
	XOR	r5, r3, r9
	XOR	r4, r2, r8
	ADDPP	p1, p2, p5
	CMP	r2, r4
	CPC	r3, r5
	BLO	.LBB0_6
; %bb.5:                                ; %entry
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
.LBB0_6:                                ; %entry
	LDP	p2, [p7 + 36]
	XOR	r7, r5, r9
	XOR	r6, r4, r8
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPP	p2, p3, p2
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 40]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 48
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
