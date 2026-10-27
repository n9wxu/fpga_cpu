	.text
	.globl	strlen                          ; -- Begin function strlen
	.p2align	1
strlen:                                 ; @strlen
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p3, p1
.LBB0_1:                                ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p3
	ADDPI	p3, p2, 1
	LDBU	r8, [p2]
	CMPI	r8, 0
	BNE	.LBB0_1
; %bb.2:                                ; %while.end
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	strcpy                          ; -- Begin function strcpy
	.p2align	1
strcpy:                                 ; @strcpy
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p3, p1
.LBB1_1:                                ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r8, [p2]
	STB	r8, [p3]
	ADDPI	p3, p3, 1
	ADDPI	p2, p2, 1
	CMPI	r8, 0
	BNE	.LBB1_1
; %bb.2:                                ; %while.end
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	strncpy                         ; -- Begin function strncpy
	.p2align	1
strncpy:                                ; @strncpy
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r12, 0
	MOV	r13, r12
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB2_5
; %bb.1:                                ; %land.rhs.preheader
	MOVP	p4, p1
.LBB2_2:                                ; %land.rhs
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r10, [p2]
	CMPI	r10, 0
	BEQ	.LBB2_3
; %bb.6:                                ; %for.body
                                        ;   in Loop: Header=BB2_2 Depth=1
	STB	r10, [p4]
	ADDPI	p4, p4, 1
	ADDPI	p2, p2, 1
	ADDPI	p3, p3, -1
	MOV	r13, r12
	CMP	r6, r12
	CPC	r7, r13
	BNE	.LBB2_2
	BRA	.LBB2_5
.LBB2_3:
	MOVI	r4, 0
	MOV	r5, r4
.LBB2_4:                                ; %for.body5
                                        ; =>This Inner Loop Header: Depth=1
	STB	r4, [p4]
	ADDPI	p4, p4, 1
	ADDPI	p3, p3, -1
	CMP	r6, r4
	CPC	r7, r5
	BNE	.LBB2_4
.LBB2_5:                                ; %for.end9
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	strcat                          ; -- Begin function strcat
	.p2align	1
strcat:                                 ; @strcat
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p1
.LBB3_1:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p3, p4
	ADDPI	p4, p3, 1
	LDBU	r10, [p3]
	CMPI	r10, 0
	BNE	.LBB3_1
.LBB3_2:                                ; %while.cond.i4
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r8, [p2]
	STB	r8, [p3]
	ADDPI	p3, p3, 1
	ADDPI	p2, p2, 1
	CMPI	r8, 0
	BNE	.LBB3_2
; %bb.3:                                ; %strcpy.exit
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	strncat                         ; -- Begin function strncat
	.p2align	1
strncat:                                ; @strncat
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVP	p5, p1
.LBB4_1:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p4, p5
	ADDPI	p5, p4, 1
	LDBU	r12, [p4]
	CMPI	r12, 0
	BNE	.LBB4_1
; %bb.2:                                ; %strlen.exit
	MOVI	r12, 0
.LBB4_3:                                ; %strlen.exit
                                        ; =>This Inner Loop Header: Depth=1
	MOV	r13, r12
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB4_6
; %bb.4:                                ; %land.rhs
                                        ;   in Loop: Header=BB4_3 Depth=1
	LDBU	r10, [p2]
	CMPI	r10, 0
	BEQ	.LBB4_6
; %bb.5:                                ; %for.body
                                        ;   in Loop: Header=BB4_3 Depth=1
	STB	r10, [p4]
	ADDPI	p4, p4, 1
	ADDPI	p2, p2, 1
	ADDPI	p3, p3, -1
	BRA	.LBB4_3
.LBB4_6:                                ; %for.end
	STB	r12, [p4]
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	strcmp                          ; -- Begin function strcmp
	.p2align	1
strcmp:                                 ; @strcmp
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVI	r7, 0
	LDBU	r8, [p1]
	CMPI	r8, 0
	BEQ	.LBB5_3
.LBB5_1:                                ; %land.rhs
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r10, [p2]
	ANDI	r11, r8, 255
	CMP	r11, r10
	BNE	.LBB5_4
; %bb.2:                                ; %while.body
                                        ;   in Loop: Header=BB5_1 Depth=1
	ADDPI	p2, p2, 1
	ADDPI	p5, p1, 1
	LDBU	r8, [p1 + 1]
	CMPI	r8, 0
	MOVP	p1, p5
	BNE	.LBB5_1
.LBB5_3:
	MOV	r8, r7
.LBB5_4:                                ; %while.end
	ANDI	r3, r9, 0
	ANDI	r2, r8, 255
	LDBU	r6, [p2]
	SUB	r2, r2, r6
	SBC	r3, r3, r7
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	strncmp                         ; -- Begin function strncmp
	.p2align	1
strncmp:                                ; @strncmp
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
.LBB6_1:                                ; %entry
                                        ; =>This Inner Loop Header: Depth=1
	CMP	r6, r8
	CPC	r7, r9
	BEQ	.LBB6_4
; %bb.2:                                ; %for.body
                                        ;   in Loop: Header=BB6_1 Depth=1
	MOVP	p6, p4
	LDBU	r8, [p2]
	LDBU	r10, [p1]
	CMP	r10, r8
	BNE	.LBB6_3
; %bb.5:                                ; %if.end
                                        ;   in Loop: Header=BB6_1 Depth=1
	MOVP	p4, p6
	MOV	r9, r8
	CMPI	r10, 0
	BEQ	.LBB6_4
; %bb.6:                                ; %if.end
                                        ;   in Loop: Header=BB6_1 Depth=1
	ADDPI	p2, p2, 1
	ADDPI	p1, p1, 1
	ADDPI	p3, p3, -1
	BRA	.LBB6_1
.LBB6_3:                                ; %if.then
	MOVI	r9, 0
	MOV	r11, r9
	SUB	r8, r10, r8
	SBC	r9, r11, r9
.LBB6_4:                                ; %cleanup
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	strchr                          ; -- Begin function strchr
	.p2align	1
strchr:                                 ; @strchr
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	LDBU	r7, [p1]
	ANDI	r6, r4, 255
	CMP	r7, r6
	BNE	.LBB7_2
; %bb.1:
	MOVP	p2, p1
	BRA	.LBB7_6
.LBB7_2:                                ; %if.end.preheader
	MOVI	r8, 0
.LBB7_3:                                ; %if.end
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r4, r7, 255
	CMPI	r4, 0
	BEQ	.LBB7_4
; %bb.5:                                ; %for.inc
                                        ;   in Loop: Header=BB7_3 Depth=1
	ADDPI	p2, p1, 1
	LDBU	r7, [p1 + 1]
	CMP	r7, r6
	MOVP	p1, p2
	BNE	.LBB7_3
	BRA	.LBB7_6
.LBB7_4:
	MOV	r9, r8
	MOVP	p2, p4
.LBB7_6:                                ; %return
	MOVP	p1, p2
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	strrchr                         ; -- Begin function strrchr
	.p2align	1
strrchr:                                ; @strrchr
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	ANDI	r6, r4, 255
	BRA	.LBB8_1
.LBB8_3:                                ; %for.cond
                                        ;   in Loop: Header=BB8_1 Depth=1
	ADDPI	p1, p1, 1
	CMPI	r7, 0
	MOVP	p4, p2
	BEQ	.LBB8_4
.LBB8_1:                                ; %for.cond
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r7, [p1]
	CMP	r7, r6
	MOVP	p2, p1
	BEQ	.LBB8_3
; %bb.2:                                ; %for.cond
                                        ;   in Loop: Header=BB8_1 Depth=1
	MOVP	p2, p4
	BRA	.LBB8_3
.LBB8_4:                                ; %if.then4
	MOVP	p1, p2
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	strstr                          ; -- Begin function strstr
	.p2align	1
strstr:                                 ; @strstr
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -22
	STP	p4, [p7 + 18]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 14]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7 + 10]                   ; 4-byte Folded Spill
	MOVP	p6, p2
.LBB9_1:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p1, p6
	ADDPI	p6, p1, 1
	LDBU	r6, [p1]
	CMPI	r6, 0
	BNE	.LBB9_1
; %bb.2:                                ; %strlen.exit
	SUB	r6, r2, r4
	SBC	r7, r3, r5
	MOVI	r2, 0
	MOV	r3, r2
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB9_13
; %bb.3:                                ; %for.cond.preheader
	LDBU	r12, [p4]
	CMPI	r12, 0
	BEQ	.LBB9_14
; %bb.4:                                ; %for.body.lr.ph
	STP	p2, [p7]                        ; 4-byte Folded Spill
	LDBU	r2, [p2]
	ANDI	r2, r2, 255
	STW	r2, [p7 + 8]                    ; 2-byte Folded Spill
	MOVI	r2, 0
	BRA	.LBB9_6
.LBB9_5:                                ; %for.inc
                                        ;   in Loop: Header=BB9_6 Depth=1
	ADDPI	p3, p4, 1
	LDBU	r12, [p4 + 1]
	CMPI	r12, 0
	MOVP	p4, p3
	BEQ	.LBB9_15
.LBB9_6:                                ; %for.body
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB9_8 Depth 2
	ANDI	r4, r12, 255
	LDW	r5, [p7 + 8]                    ; 2-byte Folded Reload
	CMP	r4, r5
	BNE	.LBB9_5
; %bb.7:                                ; %for.body.i.preheader
                                        ;   in Loop: Header=BB9_6 Depth=1
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	MOVP	p3, p4
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
.LBB9_8:                                ; %for.body.i
                                        ;   Parent Loop BB9_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDBU	r10, [p3]
	LDBU	r11, [p2]
	CMP	r10, r11
	BNE	.LBB9_5
; %bb.9:                                ; %if.end.i
                                        ;   in Loop: Header=BB9_8 Depth=2
	CMPI	r10, 0
	BEQ	.LBB9_13
; %bb.10:                               ; %if.end.i
                                        ;   in Loop: Header=BB9_8 Depth=2
	ADDPI	p2, p2, 1
	ADDPI	p3, p3, 1
	ADDPI	p6, p6, -1
	MOV	r3, r2
	CMP	r12, r2
	CPC	r13, r3
	BNE	.LBB9_8
.LBB9_13:
	MOVP	p1, p4
.LBB9_14:                               ; %cleanup
	LDP	p6, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 18]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 14]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 22
	RET
.LBB9_15:
	MOV	r3, r2
	BRA	.LBB9_14
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
