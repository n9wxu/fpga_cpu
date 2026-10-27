	.text
	.globl	__lshrdi3                       ; -- Begin function __lshrdi3
	.p2align	1
__lshrdi3:                              ; @__lshrdi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p4, p3
	MOVP	p3, p2
	ANDI	r5, r9, 0
	ANDI	r4, r8, 32
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r4, r10
	CPC	r5, r11
	BNE	.LBB0_1
; %bb.2:                                ; %if.else
	CMP	r8, r10
	CPC	r9, r11
	BEQ	.LBB0_3
; %bb.4:                                ; %if.end
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r4, 32
	SUB	r4, r4, r8
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p1, p3
	STP	p3, [p7]                        ; 4-byte Folded Spill
	JAL	__ashlsi3
	MOVP	p5, p1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	OR	r5, r11, r3
	OR	r4, r10, r2
	STP	p2, [p7 + 4]                    ; 4-byte Folded Spill
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	MOVP	p5, p1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	BRA	.LBB0_5
.LBB0_1:                                ; %if.then
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	ADDI	r4, r8, -32
	MOVP	p1, p3
	JAL	__lshrsi3
.LBB0_5:                                ; %if.end11
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
.LBB0_6:                                ; %cleanup
	MOVP	p2, p5
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
.LBB0_3:
	MOVP	p5, p3
	BRA	.LBB0_6
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
