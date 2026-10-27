	.text
	.globl	strdup                          ; -- Begin function strdup
	.p2align	1
strdup:                                 ; @strdup
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
	JAL	strlen
	ADDPI	p4, p1, 1
	MOVP	p1, p4
	JAL	malloc
	MOVP	p2, p1
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB0_2
; %bb.1:                                ; %cond.true
	MOVP	p1, p2
	MOVP	p2, p5
	MOVP	p3, p4
	JAL	memcpy
.LBB0_2:                                ; %cond.end
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	strndup                         ; -- Begin function strndup
	.p2align	1
strndup:                                ; @strndup
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r4, r8
	CPC	r5, r9
	STP	p4, [p7]                        ; 4-byte Folded Spill
	BEQ	.LBB1_4
.LBB1_1:                                ; %land.rhs
                                        ; =>This Inner Loop Header: Depth=1
	ADDPP	p1, p5, p4
	LDBU	r2, [p1]
	CMPI	r2, 0
	BEQ	.LBB1_4
; %bb.2:                                ; %while.body
                                        ;   in Loop: Header=BB1_1 Depth=1
	ADDPI	p4, p4, 1
	CMP	r8, r4
	CPC	r9, r5
	BLO	.LBB1_1
; %bb.3:
	MOVP	p4, p2
.LBB1_4:                                ; %while.end
	ADDPI	p1, p4, 1
	JAL	malloc
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_6
; %bb.5:                                ; %if.then
	MOVP	p2, p5
	MOVP	p5, p1
	MOVP	p3, p4
	JAL	memcpy
	MOVP	p1, p5
	ADDPP	p3, p1, p4
	MOVI	r4, 0
	STB	r4, [p3]
.LBB1_6:                                ; %if.end
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	strcasecmp                      ; -- Begin function strcasecmp
	.p2align	1
strcasecmp:                             ; @strcasecmp
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -24
	STP	p4, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r9, -1
	MOVI	r8, -26
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	LDBU	r6, [p2]
	MOVI	r4, 0
	MOV	r7, r4
	ADDPI	p6, p3, -91
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	CMP	r12, r8
	CPC	r13, r9
	BLO	.LBB2_2
; %bb.1:                                ; %entry
	ADDPI	p3, p3, 32
.LBB2_2:                                ; %entry
	LDBU	r8, [p1]
	ANDI	r13, r9, 0
	ANDI	r12, r8, 255
	ADDPI	p5, p6, -91
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r10, r4
	CPC	r11, r5
	BLO	.LBB2_4
; %bb.3:                                ; %entry
	ADDPI	p6, p6, 32
.LBB2_4:                                ; %entry
	SUB	r6, r12, r6
	SBC	r7, r13, r7
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r6, r4
	CPC	r7, r5
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	BNE	.LBB2_13
; %bb.5:                                ; %cleanup.preheader
	MOVI	r4, 0
	MOVI	r7, -1
	MOVI	r6, -26
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB2_6
.LBB2_12:                               ; %for.inc
                                        ;   in Loop: Header=BB2_6 Depth=1
	SUB	r6, r12, r6
	SBC	r7, r13, r7
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p6, p6, 1
	ADDPI	p1, p1, 1
	MOV	r5, r4
	CMP	r6, r4
	CPC	r7, r5
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	BNE	.LBB2_13
.LBB2_6:                                ; %cleanup
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r6, r8, 255
	CMPI	r6, 0
	BEQ	.LBB2_7
; %bb.8:                                ; %for.inc
                                        ;   in Loop: Header=BB2_6 Depth=1
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	LDBU	r6, [p6 + 1]
	MOV	r7, r4
	ADDPI	p6, p3, -91
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r12, r8
	CPC	r13, r9
	BLO	.LBB2_10
; %bb.9:                                ; %for.inc
                                        ;   in Loop: Header=BB2_6 Depth=1
	ADDPI	p3, p3, 32
.LBB2_10:                               ; %for.inc
                                        ;   in Loop: Header=BB2_6 Depth=1
	LDBU	r8, [p1 + 1]
	ANDI	r13, r9, 0
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	ANDI	r12, r8, 255
	ADDPI	p4, p6, -91
	CMP	r8, r10
	CPC	r9, r11
	BLO	.LBB2_12
; %bb.11:                               ; %for.inc
                                        ;   in Loop: Header=BB2_6 Depth=1
	ADDPI	p6, p6, 32
	BRA	.LBB2_12
.LBB2_7:
	MOV	r5, r4
	MOVP	p3, p2
.LBB2_13:                               ; %return
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p3
	LDP	p4, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 24
	RET
                                        ; -- End function
	.globl	strncasecmp                     ; -- Begin function strncasecmp
	.p2align	1
strncasecmp:                            ; @strncasecmp
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, 0
	MOV	r10, r11
	CMP	r6, r10
	CPC	r7, r11
	BEQ	.LBB3_1
; %bb.2:                                ; %for.body.preheader
	MOVI	r13, -1
	MOVI	r12, -26
	STP	p6, [p7 + 16]                   ; 4-byte Folded Spill
.LBB3_3:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r10, [p2]
	ADDPI	p6, p5, -91
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r12, r8
	CPC	r13, r9
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	BLO	.LBB3_4
; %bb.5:                                ; %for.body
                                        ;   in Loop: Header=BB3_3 Depth=1
	ADDPI	p3, p5, 32
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	BRA	.LBB3_6
.LBB3_4:                                ;   in Loop: Header=BB3_3 Depth=1
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
.LBB3_6:                                ; %for.body
                                        ;   in Loop: Header=BB3_3 Depth=1
	MOVP	p6, p5
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	LDBU	r10, [p1]
	MOVP	p3, p6
	MOV	r11, r13
	ADDPI	p6, p5, -91
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r12, r2
	CPC	r13, r3
	MOVP	p4, p5
	BLO	.LBB3_8
; %bb.7:                                ; %for.body
                                        ;   in Loop: Header=BB3_3 Depth=1
	ADDPI	p4, p5, 32
.LBB3_8:                                ; %for.body
                                        ;   in Loop: Header=BB3_3 Depth=1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	SUB	r8, r8, r2
	SBC	r9, r9, r3
	MOVP	p1, p3
	MOV	r12, r3
	MOV	r13, r3
	CMP	r10, r12
	CPC	r11, r13
	BEQ	.LBB3_11
; %bb.9:                                ; %for.body
                                        ;   in Loop: Header=BB3_3 Depth=1
	CMP	r8, r12
	CPC	r9, r13
	BNE	.LBB3_11
; %bb.10:                               ; %for.inc
                                        ;   in Loop: Header=BB3_3 Depth=1
	ADDPI	p2, p2, 1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p1, p1, 1
	MOVP	p5, p3
	MOV	r8, r11
	MOV	r9, r11
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p3, p3, -1
	CMP	r6, r8
	CPC	r7, r9
	BNE	.LBB3_3
	BRA	.LBB3_11
.LBB3_1:
	MOVP	p4, p5
.LBB3_11:                               ; %return
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	strspn                          ; -- Begin function strspn
	.p2align	1
strspn:                                 ; @strspn
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	LDBU	r2, [p1]
	CMPI	r2, 0
	BEQ	.LBB4_1
; %bb.2:                                ; %land.rhs.preheader
	MOVP	p5, p4
.LBB4_3:                                ; %land.rhs
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	strchr
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB4_5
; %bb.4:                                ; %while.body
                                        ;   in Loop: Header=BB4_3 Depth=1
	ADDPI	p5, p5, 1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p5
	LDBU	r2, [p1]
	CMPI	r2, 0
	BNE	.LBB4_3
	BRA	.LBB4_5
.LBB4_1:
	MOVP	p5, p4
.LBB4_5:                                ; %while.end
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.globl	strcspn                         ; -- Begin function strcspn
	.p2align	1
strcspn:                                ; @strcspn
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	LDBU	r2, [p1]
	CMPI	r2, 0
	BEQ	.LBB5_1
; %bb.2:                                ; %land.rhs.preheader
	MOVP	p5, p4
.LBB5_3:                                ; %land.rhs
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	strchr
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB5_5
; %bb.4:                                ; %while.body
                                        ;   in Loop: Header=BB5_3 Depth=1
	ADDPI	p5, p5, 1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p5
	LDBU	r2, [p1]
	CMPI	r2, 0
	BNE	.LBB5_3
	BRA	.LBB5_5
.LBB5_1:
	MOVP	p5, p4
.LBB5_5:                                ; %while.end
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.globl	strpbrk                         ; -- Begin function strpbrk
	.p2align	1
strpbrk:                                ; @strpbrk
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	LDBU	r2, [p1]
	CMPI	r2, 0
	BEQ	.LBB6_5
; %bb.1:                                ; %land.rhs.i.preheader
	MOVI	r10, 0
	MOV	r11, r10
	MOVP	p4, p5
.LBB6_2:                                ; %land.rhs.i
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	strchr
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB6_4
; %bb.3:                                ; %while.body.i
                                        ;   in Loop: Header=BB6_2 Depth=1
	ADDPI	p4, p4, 1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p4
	LDBU	r2, [p1]
	CMPI	r2, 0
	BNE	.LBB6_2
	BRA	.LBB6_5
.LBB6_4:                                ; %strcspn.exit
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p4
	LDBU	r4, [p1]
	CMPI	r4, 0
	BNE	.LBB6_6
.LBB6_5:                                ; %strcspn.exit.thread
	MOVI	r2, 0
	MOV	r3, r2
.LBB6_6:
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.globl	strtok                          ; -- Begin function strtok
	.p2align	1
strtok:                                 ; @strtok
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB7_2
; %bb.1:
	LDP	p5, [strtok.last]
.LBB7_2:                                ; %entry
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB7_17
; %bb.3:                                ; %if.end3
	MOVI	r8, 0
	MOV	r9, r8
	LDBU	r2, [p5]
	CMPI	r2, 0
	BEQ	.LBB7_8
; %bb.4:                                ; %land.rhs.i.preheader
	STP	p6, [p7]                        ; 4-byte Folded Spill
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
.LBB7_5:                                ; %land.rhs.i
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	strchr
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB7_9
; %bb.6:                                ; %while.body.i
                                        ;   in Loop: Header=BB7_5 Depth=1
	ADDPI	p4, p4, 1
	ADDPP	p1, p5, p4
	LDBU	r2, [p1]
	CMPI	r2, 0
	BNE	.LBB7_5
; %bb.7:
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	MOV	r9, r8
	BRA	.LBB7_14
.LBB7_8:
	MOVP	p1, p4
	BRA	.LBB7_16
.LBB7_9:                                ; %strspn.exit
	ADDPP	p1, p5, p4
	MOVI	r8, 0
	MOV	r9, r8
	LDBU	r6, [p1]
	CMPI	r6, 0
	BEQ	.LBB7_14
; %bb.10:                               ; %land.rhs.i23.preheader
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
.LBB7_11:                               ; %land.rhs.i23
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p5, p1
	ANDI	r5, r7, 0
	ANDI	r4, r6, 255
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	strchr
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB7_18
; %bb.12:                               ; %while.body.i29
                                        ;   in Loop: Header=BB7_11 Depth=1
	ADDPI	p4, p4, 1
	MOVP	p1, p5
	ADDPP	p2, p1, p4
	LDBU	r6, [p2]
	CMPI	r6, 0
	BNE	.LBB7_11
; %bb.13:
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	MOV	r9, r8
	BRA	.LBB7_15
.LBB7_14:
	MOVP	p1, p4
.LBB7_15:                               ; %return.sink.split
	LDP	p6, [p7]                        ; 4-byte Folded Reload
.LBB7_16:                               ; %return.sink.split
	STP	p4, [strtok.last]
.LBB7_17:                               ; %return
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
.LBB7_18:                               ; %strcspn.exit
	MOVP	p1, p5
	ADDPP	p3, p1, p4
	MOVI	r8, 0
	LDBU	r4, [p3]
	CMPI	r4, 0
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	BEQ	.LBB7_20
; %bb.19:                               ; %if.then10
	STB	r8, [p3]
	ADDPI	p4, p3, 1
	BRA	.LBB7_16
.LBB7_20:
	MOV	r9, r8
	BRA	.LBB7_16
                                        ; -- End function
	.globl	strerror                        ; -- Begin function strerror
	.p2align	1
strerror:                               ; @strerror
; %bb.0:                                ; %entry
	MOVP	p2, p1
	MOVI	r3, 0
	MOVI	r2, 38
	CMP	r4, r2
	CPC	r5, r3
	BHI	.LBB8_7
; %bb.1:                                ; %entry
	LEA	p1, [.L.str]
	SHLI	r6, r5, 2
	SHRI	r7, r4, 14
	OR	r7, r6, r7
	SHLI	r6, r4, 2
	LEA	p2, [.LJTI8_0]
	ADDPP	p2, p3, p2
	LDP	p2, [p2]
	JR	p2
.LBB8_2:                                ; %sw.bb1
	LEA	p1, [.L.str.1]
	RET
.LBB8_3:                                ; %sw.bb2
	LEA	p1, [.L.str.2]
	RET
.LBB8_6:                                ; %sw.bb5
	LEA	p1, [.L.str.5]
	RET
.LBB8_7:                                ; %sw.epilog
	LEA	p1, [.L.str.6]
.LBB8_8:                                ; %return
	RET
.LBB8_4:                                ; %sw.bb3
	LEA	p1, [.L.str.3]
	RET
.LBB8_5:                                ; %sw.bb4
	LEA	p1, [.L.str.4]
	RET
	.section	.rodata,"a",@progbits
	.p2align	1, 0x0
.LJTI8_0:
	.long	.LBB8_8
	.long	.LBB8_7
	.long	.LBB8_2
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_3
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_4
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_5
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_7
	.long	.LBB8_6
                                        ; -- End function
	.local	strtok.last                     ; @strtok.last
	.comm	strtok.last,4,2
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:                                 ; @.str
	.asciz	"no error"

.L.str.1:                               ; @.str.1
	.asciz	"no such file"

.L.str.2:                               ; @.str.2
	.asciz	"I/O error"

.L.str.3:                               ; @.str.3
	.asciz	"out of memory"

.L.str.4:                               ; @.str.4
	.asciz	"invalid argument"

.L.str.5:                               ; @.str.5
	.asciz	"not supported"

.L.str.6:                               ; @.str.6
	.asciz	"error"

	.section	".note.GNU-stack","",@progbits
	.addrsig
