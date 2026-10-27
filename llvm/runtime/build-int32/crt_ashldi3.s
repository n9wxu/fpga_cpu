	.text
	.globl	__ashldi3                       ; -- Begin function __ashldi3
	.p2align	1
__ashldi3:                              ; @__ashldi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p4, p6
	ANDI	r13, r7, 0
	ANDI	r12, r6, 32
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r12, r10
	CPC	r13, r11
	BNE	.LBB0_1
; %bb.2:                                ; %if.else
	CMP	r6, r10
	CPC	r7, r11
	BEQ	.LBB0_4
; %bb.3:                                ; %if.end
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p2
	MOV	r4, r6
	MOVP	p5, p3
	JAL	__ashlsi3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r2, 32
	SUB	r4, r2, r10
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__lshrsi3
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	OR	r9, r5, r3
	OR	r8, r4, r2
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r4, r10
	JAL	__ashlsi3
	MOVP	p2, p4
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
.LBB0_4:                                ; %cleanup
	MOVP	p5, p1
	BRA	.LBB0_5
.LBB0_1:                                ; %if.then
	ADDI	r4, r6, -32
	JAL	__ashlsi3
	MOVP	p2, p1
.LBB0_5:                                ; %cleanup
	MOVP	p6, p4
	MOVP	p1, p5
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
