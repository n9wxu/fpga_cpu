	.text
	.globl	__ucmpdi2                       ; -- Begin function __ucmpdi2
	.p2align	1
__ucmpdi2:                              ; @__ucmpdi2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p1
	MOVI	r2, 0
	MOV	r3, r2
	LDP	p5, [p7 + 8]
	CMP	r4, r10
	CPC	r5, r11
	BLO	.LBB0_5
; %bb.1:                                ; %if.end
	CMP	r4, r10
	CPC	r5, r11
	BLS	.LBB0_4
.LBB0_2:
	MOVI	r4, 2
.LBB0_3:                                ; %if.end10
	MOV	r5, r2
	MOVP	p1, p2
	BRA	.LBB0_5
.LBB0_4:                                ; %if.end6
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r8, r6
	CPC	r9, r7
	BHS	.LBB0_6
.LBB0_5:                                ; %cleanup
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
.LBB0_6:                                ; %if.end10
	CMP	r8, r6
	CPC	r9, r7
	BHI	.LBB0_2
; %bb.7:                                ; %if.end10
	MOVI	r4, 1
	BRA	.LBB0_3
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
