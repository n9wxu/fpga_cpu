	.text
	.globl	__fixunssfsi                    ; -- Begin function __fixunssfsi
	.p2align	1
__fixunssfsi:                           ; @__fixunssfsi
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVI	r9, 0
	MOV	r8, r9
	MOVP	p2, p4
	CMP	r2, r8
	CPC	r3, r9
	BLT	.LBB0_7
; %bb.1:                                ; %entry
	SHRI	r8, r3, 7
	ANDI	r7, r9, 0
	ANDI	r6, r8, 255
	MOVI	r8, 127
	CMP	r6, r8
	CPC	r7, r9
	BLO	.LBB0_7
; %bb.2:                                ; %if.end.i
	MOVI	r9, -1
	MOVI	r8, -32
	MOV	r4, r9
	MOV	r5, r9
	ADDPI	p5, p3, -159
	CMP	r10, r8
	CPC	r11, r9
	BLO	.LBB0_7
; %bb.3:                                ; %if.end6.i
	MOVP	p4, p6
	ANDI	r4, r2, -1
	ANDI	r5, r3, 127
	ORI	r3, r5, 128
	ORI	r2, r4, 0
	MOVI	r5, 0
	MOVI	r4, 149
	CMP	r6, r4
	CPC	r7, r5
	BHI	.LBB0_5
; %bb.4:                                ; %if.then8.i
	MOVI	r4, 150
	SUB	r4, r4, r6
	JAL	__lshrsi3
	BRA	.LBB0_6
.LBB0_5:                                ; %if.else.i
	ADDI	r4, r6, -150
	JAL	__ashlsi3
.LBB0_6:                                ; %__fixuint.exit
	MOVP	p2, p1
	MOVP	p6, p4
.LBB0_7:                                ; %__fixuint.exit
	MOVP	p1, p2
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
