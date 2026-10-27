	.text
	.globl	__negsf2                        ; -- Begin function __negsf2
	.p2align	1
__negsf2:                               ; @__negsf2
; %bb.0:                                ; %entry
	XORI	r5, r3, -32768
	XORI	r4, r2, 0
	MOVP	p1, p2
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
