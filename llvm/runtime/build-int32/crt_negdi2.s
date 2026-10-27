	.text
	.globl	__negdi2                        ; -- Begin function __negdi2
	.p2align	1
__negdi2:                               ; @__negdi2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVI	r6, 0
	MOV	r7, r6
	CMP	r2, r6
	CPC	r3, r7
	BNE	.LBB0_1
; %bb.2:                                ; %entry
	MOVP	p4, p3
	BRA	.LBB0_3
.LBB0_1:
	MOVI	r8, 1
	MOV	r9, r6
.LBB0_3:                                ; %entry
	ADDPP	p2, p2, p4
	SUB	r2, r6, r2
	SBC	r3, r7, r3
	SUB	r4, r6, r4
	SBC	r5, r7, r5
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
