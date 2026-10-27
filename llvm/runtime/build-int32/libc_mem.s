	.text
	.globl	memcpy                          ; -- Begin function memcpy
	.p2align	1
memcpy:                                 ; @memcpy
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	OR	r13, r5, r3
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	OR	r12, r4, r2
	ANDI	r11, r13, 0
	ANDI	r10, r12, 1
	MOVI	r9, 0
	MOV	r2, r9
	MOV	r3, r9
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_5
; %bb.1:                                ; %entry
	MOVI	r8, 2
	CMP	r6, r8
	CPC	r7, r9
	BLO	.LBB0_5
; %bb.2:                                ; %for.body.preheader
	MOVI	r11, 0
	MOVI	r10, 1
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
.LBB0_3:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r12, [p2]
	STW	r12, [p4]
	ADDPI	p4, p4, 2
	ADDPI	p2, p2, 2
	ADDPI	p3, p3, -2
	CMP	r6, r10
	CPC	r7, r11
	BHI	.LBB0_3
	BRA	.LBB0_6
.LBB0_5:
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
.LBB0_6:                                ; %if.end
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_9
; %bb.7:                                ; %while.body.preheader
	MOVI	r12, 0
	MOV	r13, r12
.LBB0_8:                                ; %while.body
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r2, [p2]
	STB	r2, [p4]
	ADDPI	p4, p4, 1
	ADDPI	p2, p2, 1
	ADDPI	p3, p3, -1
	CMP	r6, r12
	CPC	r7, r13
	BNE	.LBB0_8
.LBB0_9:                                ; %while.end
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	memmove                         ; -- Begin function memmove
	.p2align	1
memmove:                                ; @memmove
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVP	p6, p1
	CMP	r12, r4
	CPC	r13, r5
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	BLS	.LBB1_5
; %bb.1:                                ; %lor.lhs.false
	ADDPP	p4, p2, p3
	CMP	r8, r12
	CPC	r9, r13
	BLS	.LBB1_5
; %bb.2:                                ; %if.end
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB1_14
; %bb.3:                                ; %while.body.preheader
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p6, p1, p3
.LBB1_4:                                ; %while.body
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r2, [p4 - 1]
	STB	r2, [p6 - 1]
	ADDPI	p6, p6, -1
	ADDPI	p4, p4, -1
	ADDPI	p3, p3, -1
	CMP	r6, r4
	CPC	r7, r5
	BNE	.LBB1_4
	BRA	.LBB1_14
.LBB1_5:                                ; %if.then
	MOVI	r2, 0
	MOVI	r8, 2
	MOV	r9, r2
	CMP	r6, r8
	CPC	r7, r9
	BLO	.LBB1_10
; %bb.6:                                ; %if.then
	OR	r9, r5, r13
	OR	r8, r4, r12
	ANDI	r11, r9, 0
	ANDI	r10, r8, 1
	MOV	r3, r2
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB1_10
; %bb.7:                                ; %for.body.i.preheader
	MOVI	r9, 0
	MOVI	r8, 1
	MOVP	p5, p6
.LBB1_8:                                ; %for.body.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r12, [p2]
	STW	r12, [p5]
	ADDPI	p5, p5, 2
	ADDPI	p2, p2, 2
	ADDPI	p3, p3, -2
	CMP	r6, r8
	CPC	r7, r9
	BHI	.LBB1_8
	BRA	.LBB1_11
.LBB1_10:
	MOVP	p5, p6
.LBB1_11:                               ; %if.end.i
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB1_14
; %bb.12:                               ; %while.body.i.preheader
	MOVI	r12, 0
	MOV	r13, r12
.LBB1_13:                               ; %while.body.i
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r2, [p2]
	STB	r2, [p5]
	ADDPI	p5, p5, 1
	ADDPI	p2, p2, 1
	ADDPI	p3, p3, -1
	CMP	r6, r12
	CPC	r7, r13
	BNE	.LBB1_13
.LBB1_14:                               ; %cleanup
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	memset                          ; -- Begin function memset
	.p2align	1
memset:                                 ; @memset
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	ANDI	r13, r3, 0
	ANDI	r12, r2, 1
	MOVI	r11, 0
	MOV	r10, r11
	CMP	r12, r10
	CPC	r13, r11
	BEQ	.LBB2_4
; %bb.1:                                ; %entry
	CMP	r6, r10
	CPC	r7, r11
	BEQ	.LBB2_4
; %bb.2:                                ; %if.then
	STB	r4, [p1]
	ADDPI	p3, p3, -1
	ADDPI	p4, p1, 1
	BRA	.LBB2_5
.LBB2_4:
	MOVP	p4, p1
.LBB2_5:                                ; %if.end
	MOVI	r10, 2
	CMP	r6, r10
	CPC	r7, r11
	BLO	.LBB2_8
; %bb.6:                                ; %for.body.preheader
	ANDI	r12, r4, 255
	MOVI	r13, 257
	MUL	r12, r12, r13
	MOVI	r11, 0
	MOVI	r10, 1
.LBB2_7:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	STW	r12, [p4]
	ADDPI	p4, p4, 2
	ADDPI	p3, p3, -2
	CMP	r6, r10
	CPC	r7, r11
	BHI	.LBB2_7
.LBB2_8:                                ; %for.end
	MOVI	r12, 0
	MOV	r13, r12
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB2_10
; %bb.9:                                ; %if.then8
	STB	r4, [p4]
.LBB2_10:                               ; %if.end9
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	memcmp                          ; -- Begin function memcmp
	.p2align	1
memcmp:                                 ; @memcmp
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p4, p1
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB3_6
; %bb.1:                                ; %for.body.preheader
	STP	p6, [p7]                        ; 4-byte Folded Spill
.LBB3_2:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r12, [p2]
	LDBU	r10, [p4]
	CMP	r10, r12
	BNE	.LBB3_3
; %bb.4:                                ; %for.inc
                                        ;   in Loop: Header=BB3_2 Depth=1
	ADDPI	p2, p2, 1
	ADDPI	p4, p4, 1
	MOV	r3, r2
	ADDPI	p3, p3, -1
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB3_2
	BRA	.LBB3_5
.LBB3_3:                                ; %if.then
	MOVI	r13, 0
	MOV	r11, r13
	SUB	r2, r10, r12
	SBC	r3, r11, r13
.LBB3_5:
	LDP	p6, [p7]                        ; 4-byte Folded Reload
.LBB3_6:                                ; %cleanup
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	memchr                          ; -- Begin function memchr
	.p2align	1
memchr:                                 ; @memchr
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p1
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB4_5
; %bb.1:                                ; %for.body.lr.ph
	ANDI	r4, r4, 255
.LBB4_2:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r5, [p4]
	CMP	r5, r4
	BEQ	.LBB4_3
; %bb.4:                                ; %for.inc
                                        ;   in Loop: Header=BB4_2 Depth=1
	ADDPI	p4, p4, 1
	MOV	r3, r2
	ADDPI	p3, p3, -1
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB4_2
	BRA	.LBB4_5
.LBB4_3:
	MOVP	p1, p4
.LBB4_5:                                ; %cleanup
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
