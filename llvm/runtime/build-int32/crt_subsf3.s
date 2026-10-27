	.text
	.globl	__subsf3                        ; -- Begin function __subsf3
	.p2align	1
__subsf3:                               ; @__subsf3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p6
	XORI	r7, r5, -32768
	XORI	r6, r4, 0
	MOVP	p2, p3
	JAL	__addsf3
	MOVP	p6, p4
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
