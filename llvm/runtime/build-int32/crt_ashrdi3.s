	.text
	.globl	__ashrdi3                       ; -- Begin function __ashrdi3
	.p2align	1
__ashrdi3:                              ; @__ashrdi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p5, p6
	ANDI	r9, r7, 0
	ANDI	r8, r6, 32
	MOVI	r12, 0
	MOV	r13, r12
	CMP	r8, r12
	CPC	r9, r13
	BNE	.LBB0_1
; %bb.2:                                ; %if.else
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB0_4
; %bb.3:                                ; %if.end
	MOVI	r12, 32
	SUB	r12, r12, r6
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p2
	STP	p2, [p7 + 4]                    ; 4-byte Folded Spill
	MOV	r4, r12
	MOVP	p4, p3
	JAL	__ashlsi3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	OR	r11, r5, r3
	OR	r10, r4, r2
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__ashrsi3
	MOVP	p2, p1
	MOVP	p1, p5
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB0_4
.LBB0_1:                                ; %if.then
	ADDI	r6, r6, -32
	MOVP	p1, p2
	MOVP	p4, p2
	MOV	r4, r6
	JAL	__ashrsi3
	MOVP	p2, p4
	SARI	r4, r5, 15
	MOV	r5, r4
.LBB0_4:                                ; %cleanup
	MOVP	p6, p5
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
