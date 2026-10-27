	.text
	.section	.bss,"aw",@nobits
	.globl	errno                           ; @errno
	.p2align	1, 0x0
errno:
	.long	0                               ; 0x0

	.section	".note.GNU-stack","",@progbits
	.addrsig
