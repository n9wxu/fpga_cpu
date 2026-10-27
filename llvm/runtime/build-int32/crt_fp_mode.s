	.text
	.globl	__fe_getround                   ; -- Begin function __fe_getround
	.p2align	1
__fe_getround:                          ; @__fe_getround
; %bb.0:                                ; %entry
	MOVI	r2, 0
	MOV	r3, r2
	RET
                                        ; -- End function
	.globl	__fe_raise_inexact              ; -- Begin function __fe_raise_inexact
	.p2align	1
__fe_raise_inexact:                     ; @__fe_raise_inexact
; %bb.0:                                ; %entry
	MOVI	r2, 0
	MOV	r3, r2
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
