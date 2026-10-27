	.text
	.globl	__popcountsi2                   ; -- Begin function __popcountsi2
	.p2align	1
__popcountsi2:                          ; @__popcountsi2
; %bb.0:                                ; %entry
	SHRI	r4, r2, 1
	SHLI	r5, r3, 15
	OR	r4, r4, r5
	SHRI	r5, r3, 1
	ANDI	r7, r5, 21845
	ANDI	r6, r4, 21845
	SUB	r2, r2, r6
	SBC	r3, r3, r7
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r4, r5, r4
	SHRI	r5, r3, 2
	ANDI	r7, r5, 13107
	ANDI	r6, r4, 13107
	ANDI	r5, r3, 13107
	ANDI	r4, r2, 13107
	ADDPP	p1, p3, p2
	SHRI	r4, r2, 4
	SHLI	r5, r3, 12
	OR	r4, r4, r5
	SHRI	r5, r3, 4
	ADDPP	p1, p2, p1
	ANDI	r5, r3, 3855
	ANDI	r4, r2, 3855
	MOVI	r3, 0
	MOV	r2, r5
	ADDPP	p1, p1, p2
	SHRI	r4, r2, 8
	SHLI	r5, r3, 8
	OR	r4, r4, r5
	SHRI	r5, r3, 8
	ADDPP	p2, p2, p1
	ANDI	r3, r5, 0
	ANDI	r2, r4, 63
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
