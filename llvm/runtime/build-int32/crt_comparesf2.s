	.text
	.globl	__lesf2                         ; -- Begin function __lesf2
	.p2align	1
__lesf2:                                ; @__lesf2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
	ANDI	r9, r11, 32767
	ANDI	r8, r10, -1
	MOVI	r3, 0
	MOVI	r2, 1
	MOVI	r7, 32640
	MOV	r6, r3
	CMP	r8, r6
	CPC	r9, r7
	BHI	.LBB0_10
; %bb.1:                                ; %entry
	MOVP	p6, p5
	ANDI	r11, r5, 32767
	ANDI	r10, r4, -1
	CMP	r10, r6
	CPC	r11, r7
	BHI	.LBB0_10
; %bb.2:                                ; %if.end.i
	OR	r7, r11, r9
	OR	r6, r10, r8
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_10
; %bb.3:                                ; %if.end6.i
	MOVP	p4, p6
	AND	r7, r5, r9
	AND	r6, r4, r8
	CMP	r6, r2
	CPC	r7, r3
	BLT	.LBB0_7
; %bb.4:                                ; %if.then9.i
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r8, r4
	CPC	r9, r5
	BLT	.LBB0_10
	BRA	.LBB0_5
.LBB0_7:                                ; %if.else15.i
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r8, r4
	CPC	r9, r5
	BGT	.LBB0_10
.LBB0_5:                                ; %if.else.i
	MOVI	r3, 0
	CMP	r12, r4
	CPC	r13, r5
	BNE	.LBB0_6
; %bb.9:                                ; %if.else18.i
	MOV	r2, r3
	BRA	.LBB0_10
.LBB0_6:
	MOVI	r2, 1
.LBB0_10:                               ; %__leXf2__.exit
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	__gesf2                         ; -- Begin function __gesf2
	.p2align	1
__gesf2:                                ; @__gesf2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
	ANDI	r9, r11, 32767
	ANDI	r8, r10, -1
	MOVI	r7, 32640
	MOVI	r6, 0
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r8, r6
	CPC	r9, r7
	BHI	.LBB1_10
; %bb.1:                                ; %entry
	MOVP	p6, p5
	ANDI	r11, r5, 32767
	ANDI	r10, r4, -1
	CMP	r10, r6
	CPC	r11, r7
	BHI	.LBB1_10
; %bb.2:                                ; %if.end.i
	OR	r7, r11, r9
	OR	r6, r10, r8
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB1_10
; %bb.3:                                ; %if.end6.i
	MOVP	p4, p6
	AND	r7, r5, r9
	AND	r6, r4, r8
	CMP	r6, r2
	CPC	r7, r3
	BLT	.LBB1_7
; %bb.4:                                ; %if.then9.i
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r8, r4
	CPC	r9, r5
	BLT	.LBB1_10
	BRA	.LBB1_5
.LBB1_7:                                ; %if.else15.i
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r8, r4
	CPC	r9, r5
	BGT	.LBB1_10
.LBB1_5:                                ; %if.else.i
	MOVI	r3, 0
	CMP	r12, r4
	CPC	r13, r5
	BNE	.LBB1_6
; %bb.9:                                ; %if.else18.i
	MOV	r2, r3
	BRA	.LBB1_10
.LBB1_6:
	MOVI	r2, 1
.LBB1_10:                               ; %__geXf2__.exit
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	__unordsf2                      ; -- Begin function __unordsf2
	.p2align	1
__unordsf2:                             ; @__unordsf2
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	ANDI	r7, r5, 32767
	ANDI	r6, r4, -1
	ANDI	r9, r3, 32767
	ANDI	r8, r2, -1
	MOVI	r3, 32640
	MOVI	r2, 0
	MOVI	r4, 1
	MOV	r5, r2
	MOV	r10, r2
	MOV	r11, r2
	CMP	r8, r2
	CPC	r9, r3
	MOVP	p4, p2
	BHI	.LBB2_2
; %bb.1:                                ; %entry
	MOVP	p4, p5
.LBB2_2:                                ; %entry
	CMP	r6, r2
	CPC	r7, r3
	BHI	.LBB2_4
; %bb.3:                                ; %entry
	MOVP	p2, p5
.LBB2_4:                                ; %entry
	OR	r3, r9, r5
	OR	r2, r8, r4
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	__cmpsf2
.set __cmpsf2, __lesf2
	.globl	__eqsf2
.set __eqsf2, __lesf2
	.globl	__ltsf2
.set __ltsf2, __lesf2
	.globl	__nesf2
.set __nesf2, __lesf2
	.globl	__gtsf2
.set __gtsf2, __gesf2
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym __lesf2
	.addrsig_sym __gesf2
