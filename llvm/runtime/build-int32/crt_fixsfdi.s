	.text
	.globl	__fixsfdi                       ; -- Begin function __fixsfdi
	.p2align	1
__fixsfdi:                              ; @__fixsfdi
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r10, 0
	MOV	r11, r10
	MOVP	p2, p5
	JAL	__ltsf2
	MOVI	r4, -1
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB0_5
; %bb.1:                                ; %if.then
	XORI	r3, r9, -32768
	XORI	r2, r8, 0
	JAL	__fixunssfdi
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB0_2
; %bb.3:                                ; %if.then
	MOVP	p3, p5
	BRA	.LBB0_4
.LBB0_5:                                ; %if.end
	MOVP	p1, p4
	JAL	__fixunssfdi
	BRA	.LBB0_6
.LBB0_2:
	MOVI	r6, 1
	MOV	r7, r10
.LBB0_4:                                ; %if.then
	ADDPP	p2, p2, p3
	SUB	r4, r10, r4
	SBC	r5, r11, r5
	SUB	r2, r10, r2
	SBC	r3, r11, r3
.LBB0_6:                                ; %return
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
