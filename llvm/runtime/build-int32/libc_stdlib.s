	.text
	.globl	abs                             ; -- Begin function abs
	.p2align	1
abs:                                    ; @abs
; %bb.0:                                ; %entry
	SARI	r4, r3, 15
	MOV	r5, r4
	XOR	r7, r3, r5
	XOR	r6, r2, r4
	SUB	r2, r6, r4
	SBC	r3, r7, r5
	RET
                                        ; -- End function
	.globl	labs                            ; -- Begin function labs
	.p2align	1
labs:                                   ; @labs
; %bb.0:                                ; %entry
	SARI	r4, r3, 15
	MOV	r5, r4
	XOR	r7, r3, r5
	XOR	r6, r2, r4
	SUB	r2, r6, r4
	SBC	r3, r7, r5
	RET
                                        ; -- End function
	.globl	strtoul                         ; -- Begin function strtoul
	.p2align	1
strtoul:                                ; @strtoul
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p2, [p7]                        ; 4-byte Folded Spill
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r13, 0
	MOVI	r9, -1
	MOVI	r8, -6
.LBB2_1:                                ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p1
	ADDPI	p1, p2, 1
	LDBU	r12, [p2]
	CMPI	r12, 32
	BEQ	.LBB2_1
; %bb.2:                                ; %while.cond
                                        ;   in Loop: Header=BB2_1 Depth=1
	ADDPI	p5, p6, -14
	CMP	r10, r8
	CPC	r11, r9
	BHI	.LBB2_1
; %bb.3:                                ; %while.end
	MOVI	r3, 0
	CMPI	r12, 43
	BEQ	.LBB2_5
; %bb.4:                                ; %while.end
	MOV	r2, r3
	BRA	.LBB2_6
.LBB2_5:
	MOVI	r2, 1
.LBB2_6:                                ; %while.end
	ADDPP	p5, p2, p1
	MOVI	r2, 16
	ORI	r5, r7, 0
	ORI	r4, r6, 16
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB2_14
; %bb.7:                                ; %land.lhs.true
	MOVI	r2, 0
	LDBU	r4, [p5]
	CMPI	r4, 48
	BNE	.LBB2_11
; %bb.8:                                ; %land.lhs.true11
	LDBU	r4, [p5 + 1]
	ANDI	r4, r4, 223
	CMPI	r4, 88
	BNE	.LBB2_12
; %bb.9:                                ; %if.then16
	MOVI	r3, 0
	MOVI	r2, 16
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	ADDPI	p5, p5, 2
	BRA	.LBB2_15
.LBB2_11:
	MOVI	r4, 10
	BRA	.LBB2_13
.LBB2_12:
	MOVI	r4, 8
.LBB2_13:                               ; %if.else
	MOV	r5, r2
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB2_15
.LBB2_14:                               ; %if.else
	STP	p3, [p7 + 16]                   ; 4-byte Folded Spill
.LBB2_15:                               ; %if.end25
	MOVI	r2, 0
	MOV	r3, r2
	MOVI	r4, 10
	MOV	r5, r2
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r5, -1
	MOVI	r4, -26
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
.LBB2_16:                               ; %for.cond
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r4, [p5]
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -48
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r8, r12
	CPC	r9, r13
	BLO	.LBB2_19
; %bb.17:                               ; %if.else31
                                        ;   in Loop: Header=BB2_16 Depth=1
	ORI	r13, r7, 0
	ORI	r12, r6, 32
	ADDPI	p3, p6, -123
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r6, r12
	CPC	r7, r13
	BLO	.LBB2_21
; %bb.18:                               ; %if.then35
                                        ;   in Loop: Header=BB2_16 Depth=1
	ORI	r4, r4, 32
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -87
.LBB2_19:                               ; %if.end41
                                        ;   in Loop: Header=BB2_16 Depth=1
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r8, r4
	CPC	r9, r5
	BGE	.LBB2_21
; %bb.20:                               ; %for.inc
                                        ;   in Loop: Header=BB2_16 Depth=1
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	ADDPP	p1, p4, p1
	ADDPI	p5, p5, 1
	BRA	.LBB2_16
.LBB2_21:                               ; %for.end
	MOVI	r4, 0
	MOV	r5, r4
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	MOVP	p2, p3
	BEQ	.LBB2_23
; %bb.22:                               ; %if.then49
	STP	p5, [p2]
.LBB2_23:                               ; %if.end50
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	strtol                          ; -- Begin function strtol
	.p2align	1
strtol:                                 ; @strtol
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p3, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r11, 0
	MOVI	r13, -1
	MOVI	r12, -6
.LBB3_1:                                ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p3, p1
	ADDPI	p1, p3, 1
	LDBU	r10, [p3]
	CMPI	r10, 32
	BEQ	.LBB3_1
; %bb.2:                                ; %while.cond
                                        ;   in Loop: Header=BB3_1 Depth=1
	ADDPI	p4, p5, -14
	CMP	r8, r12
	CPC	r9, r13
	BHI	.LBB3_1
; %bb.3:                                ; %while.end
	MOVI	r3, 0
	CMPI	r10, 45
	STP	p2, [p7 + 4]                    ; 4-byte Folded Spill
	BEQ	.LBB3_5
; %bb.4:                                ; %while.end
	MOV	r2, r3
	BRA	.LBB3_6
.LBB3_5:
	MOVI	r2, 1
.LBB3_6:                                ; %while.end
	ADDPP	p4, p3, p1
	MOVI	r7, -1
	MOVI	r6, -6
.LBB3_7:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p4
	ADDPI	p4, p2, 1
	LDBU	r2, [p2]
	CMPI	r2, 32
	BEQ	.LBB3_7
; %bb.8:                                ; %while.cond.i
                                        ;   in Loop: Header=BB3_7 Depth=1
	ADDPI	p6, p1, -14
	CMP	r12, r6
	CPC	r13, r7
	BHI	.LBB3_7
; %bb.9:                                ; %while.end.i
	MOVI	r13, 0
	CMPI	r2, 43
	BEQ	.LBB3_11
; %bb.10:                               ; %while.end.i
	MOV	r12, r13
	BRA	.LBB3_12
.LBB3_11:
	MOVI	r12, 1
.LBB3_12:                               ; %while.end.i
	LDP	p4, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPP	p3, p2, p6
	MOVI	r12, 16
	ORI	r3, r9, 0
	ORI	r2, r8, 16
	CMP	r2, r12
	CPC	r3, r13
	STP	p5, [p7]                        ; 4-byte Folded Spill
	BNE	.LBB3_20
; %bb.13:                               ; %land.lhs.true.i
	MOVI	r2, 0
	LDBU	r4, [p3]
	CMPI	r4, 48
	BNE	.LBB3_17
; %bb.14:                               ; %land.lhs.true11.i
	LDBU	r4, [p3 + 1]
	ANDI	r4, r4, 223
	CMPI	r4, 88
	BNE	.LBB3_18
; %bb.15:                               ; %if.then16.i
	MOVI	r3, 0
	MOVI	r2, 16
	STP	p1, [p7 + 20]                   ; 4-byte Folded Spill
	ADDPI	p3, p3, 2
	BRA	.LBB3_21
.LBB3_17:
	MOVI	r4, 10
	BRA	.LBB3_19
.LBB3_18:
	MOVI	r4, 8
.LBB3_19:                               ; %if.else.i
	MOV	r5, r2
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOV	r3, r2
	CMP	r8, r2
	CPC	r9, r3
	BEQ	.LBB3_21
.LBB3_20:                               ; %if.else.i
	STP	p4, [p7 + 20]                   ; 4-byte Folded Spill
.LBB3_21:                               ; %if.end25.i
	MOVI	r2, 0
	MOV	r3, r2
	MOVI	r4, 10
	MOV	r5, r2
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r5, -1
	MOVI	r4, -26
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
.LBB3_22:                               ; %for.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p5, p3
	LDBU	r4, [p3]
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -48
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r8, r12
	CPC	r9, r13
	BLO	.LBB3_25
; %bb.23:                               ; %if.else31.i
                                        ;   in Loop: Header=BB3_22 Depth=1
	ORI	r13, r7, 0
	ORI	r12, r6, 32
	ADDPI	p3, p6, -123
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r6, r12
	CPC	r7, r13
	BLO	.LBB3_27
; %bb.24:                               ; %if.then35.i
                                        ;   in Loop: Header=BB3_22 Depth=1
	ORI	r4, r4, 32
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -87
.LBB3_25:                               ; %if.end41.i
                                        ;   in Loop: Header=BB3_22 Depth=1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	CMP	r8, r4
	CPC	r9, r5
	BGE	.LBB3_27
; %bb.26:                               ; %for.inc.i
                                        ;   in Loop: Header=BB3_22 Depth=1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	ADDPP	p1, p4, p1
	MOVP	p3, p5
	ADDPI	p3, p3, 1
	BRA	.LBB3_22
.LBB3_27:                               ; %for.end.i
	MOVI	r4, 0
	MOV	r5, r4
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB3_29
; %bb.28:                               ; %if.then49.i
	STP	p5, [p3]
.LBB3_29:                               ; %strtoul.exit
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMPI	r6, 45
	BNE	.LBB3_31
; %bb.30:
	SUB	r2, r4, r2
	SBC	r3, r5, r3
.LBB3_31:                               ; %strtoul.exit
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.globl	atol                            ; -- Begin function atol
	.p2align	1
atol:                                   ; @atol
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r9, 0
	MOVI	r7, -1
	MOVI	r6, -6
.LBB4_1:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p1
	ADDPI	p1, p2, 1
	LDBU	r8, [p2]
	CMPI	r8, 32
	BEQ	.LBB4_1
; %bb.2:                                ; %while.cond.i
                                        ;   in Loop: Header=BB4_1 Depth=1
	ADDPI	p6, p4, -14
	CMP	r12, r6
	CPC	r13, r7
	BHI	.LBB4_1
; %bb.3:                                ; %while.end.i
	MOVI	r3, 0
	CMPI	r8, 45
	STP	p4, [p7]                        ; 4-byte Folded Spill
	BEQ	.LBB4_4
; %bb.5:                                ; %while.end.i
	MOV	r2, r3
	BRA	.LBB4_6
.LBB4_4:
	MOVI	r2, 1
.LBB4_6:                                ; %while.end.i
	ADDPP	p6, p2, p1
	MOVI	r7, -1
	MOVI	r6, -6
.LBB4_7:                                ; %while.cond.i.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p5, p6
	ADDPI	p6, p5, 1
	LDBU	r2, [p5]
	CMPI	r2, 32
	BEQ	.LBB4_7
; %bb.8:                                ; %while.cond.i.i
                                        ;   in Loop: Header=BB4_7 Depth=1
	ADDPI	p4, p1, -14
	CMP	r8, r6
	CPC	r9, r7
	BHI	.LBB4_7
; %bb.9:                                ; %while.end.i.i
	MOVI	r5, 0
	MOV	r4, r5
	CMPI	r2, 43
	BEQ	.LBB4_10
; %bb.11:                               ; %while.end.i.i
	MOVP	p1, p2
	BRA	.LBB4_12
.LBB4_10:
	MOVI	r2, 1
	MOV	r3, r5
.LBB4_12:                               ; %while.end.i.i
	ADDPP	p5, p5, p1
	MOVP	p6, p2
	MOVI	r4, 10
	MOVI	r3, -1
	MOVI	r2, -26
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB4_13
.LBB4_17:                               ; %for.inc.i.i
                                        ;   in Loop: Header=BB4_13 Depth=1
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p1, p6
	JAL	__mulsi3
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPP	p6, p4, p1
	ADDPI	p5, p5, 1
.LBB4_13:                               ; %for.cond.i.i
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r2, [p5]
	ANDI	r7, r3, 0
	ANDI	r6, r2, 255
	ADDPI	p4, p3, -48
	CMP	r8, r4
	CPC	r9, r5
	BLO	.LBB4_17
; %bb.14:                               ; %if.else31.i.i
                                        ;   in Loop: Header=BB4_13 Depth=1
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p5, p2
	ORI	r13, r7, 0
	ORI	r12, r6, 32
	ADDPI	p3, p6, -123
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BLO	.LBB4_18
; %bb.15:                               ; %if.end41.i.i
                                        ;   in Loop: Header=BB4_13 Depth=1
	ORI	r4, r2, 32
	ANDI	r6, r4, 255
	CMPI	r6, 96
	BHI	.LBB4_18
; %bb.16:                               ;   in Loop: Header=BB4_13 Depth=1
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -87
	MOVP	p2, p5
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	BRA	.LBB4_17
.LBB4_18:                               ; %strtol.exit
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMPI	r4, 45
	BEQ	.LBB4_19
; %bb.20:                               ; %strtol.exit
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	BRA	.LBB4_21
.LBB4_19:
	MOVI	r4, 0
	MOV	r5, r4
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
.LBB4_21:                               ; %strtol.exit
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.globl	atoi                            ; -- Begin function atoi
	.p2align	1
atoi:                                   ; @atoi
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r9, 0
	MOVI	r7, -1
	MOVI	r6, -6
.LBB5_1:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p1
	ADDPI	p1, p2, 1
	LDBU	r8, [p2]
	CMPI	r8, 32
	BEQ	.LBB5_1
; %bb.2:                                ; %while.cond.i
                                        ;   in Loop: Header=BB5_1 Depth=1
	ADDPI	p6, p4, -14
	CMP	r12, r6
	CPC	r13, r7
	BHI	.LBB5_1
; %bb.3:                                ; %while.end.i
	MOVI	r3, 0
	CMPI	r8, 45
	STP	p4, [p7]                        ; 4-byte Folded Spill
	BEQ	.LBB5_4
; %bb.5:                                ; %while.end.i
	MOV	r2, r3
	BRA	.LBB5_6
.LBB5_4:
	MOVI	r2, 1
.LBB5_6:                                ; %while.end.i
	ADDPP	p6, p2, p1
	MOVI	r7, -1
	MOVI	r6, -6
.LBB5_7:                                ; %while.cond.i.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p5, p6
	ADDPI	p6, p5, 1
	LDBU	r2, [p5]
	CMPI	r2, 32
	BEQ	.LBB5_7
; %bb.8:                                ; %while.cond.i.i
                                        ;   in Loop: Header=BB5_7 Depth=1
	ADDPI	p4, p1, -14
	CMP	r8, r6
	CPC	r9, r7
	BHI	.LBB5_7
; %bb.9:                                ; %while.end.i.i
	MOVI	r5, 0
	MOV	r4, r5
	CMPI	r2, 43
	BEQ	.LBB5_10
; %bb.11:                               ; %while.end.i.i
	MOVP	p1, p2
	BRA	.LBB5_12
.LBB5_10:
	MOVI	r2, 1
	MOV	r3, r5
.LBB5_12:                               ; %while.end.i.i
	ADDPP	p5, p5, p1
	MOVP	p6, p2
	MOVI	r4, 10
	MOVI	r3, -1
	MOVI	r2, -26
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB5_13
.LBB5_17:                               ; %for.inc.i.i
                                        ;   in Loop: Header=BB5_13 Depth=1
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p1, p6
	JAL	__mulsi3
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPP	p6, p4, p1
	ADDPI	p5, p5, 1
.LBB5_13:                               ; %for.cond.i.i
                                        ; =>This Inner Loop Header: Depth=1
	LDBU	r2, [p5]
	ANDI	r7, r3, 0
	ANDI	r6, r2, 255
	ADDPI	p4, p3, -48
	CMP	r8, r4
	CPC	r9, r5
	BLO	.LBB5_17
; %bb.14:                               ; %if.else31.i.i
                                        ;   in Loop: Header=BB5_13 Depth=1
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p5, p2
	ORI	r13, r7, 0
	ORI	r12, r6, 32
	ADDPI	p3, p6, -123
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BLO	.LBB5_18
; %bb.15:                               ; %if.end41.i.i
                                        ;   in Loop: Header=BB5_13 Depth=1
	ORI	r4, r2, 32
	ANDI	r6, r4, 255
	CMPI	r6, 96
	BHI	.LBB5_18
; %bb.16:                               ;   in Loop: Header=BB5_13 Depth=1
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPI	p4, p3, -87
	MOVP	p2, p5
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	BRA	.LBB5_17
.LBB5_18:                               ; %strtol.exit
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMPI	r4, 45
	BEQ	.LBB5_19
; %bb.20:                               ; %strtol.exit
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	BRA	.LBB5_21
.LBB5_19:
	MOVI	r4, 0
	MOV	r5, r4
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
.LBB5_21:                               ; %strtol.exit
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.globl	srand                           ; -- Begin function srand
	.p2align	1
srand:                                  ; @srand
; %bb.0:                                ; %entry
	STP	p1, [seed]
	RET
                                        ; -- End function
	.globl	rand                            ; -- Begin function rand
	.p2align	1
rand:                                   ; @rand
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p6
	MOVI	r5, 16838
	MOVI	r4, 20077
	LDP	p1, [seed]
	JAL	__mulsi3
	ADDPI	p1, p1, 12345
	STP	p1, [seed]
	MOVI	r5, 0
	MOV	r4, r3
	ANDI	r3, r5, 0
	ANDI	r2, r4, 32767
	MOVP	p6, p4
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	abort                           ; -- Begin function abort
	.p2align	1
abort:                                  ; @abort
; %bb.0:                                ; %entry
	MOVI	r3, 0
	MOVI	r2, 134
	JAL	exit
                                        ; -- End function
	.globl	qsort                           ; -- Begin function qsort
	.p2align	1
qsort:                                  ; @qsort
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -52
	STP	p4, [p7 + 48]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r12, 0
	MOVI	r2, 2
	MOV	r3, r12
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	CMP	r4, r2
	CPC	r5, r3
	BHS	.LBB9_1
.LBB9_12:                               ; %for.cond.cleanup
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 48]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 44]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 52
	RET
.LBB9_1:
	LDP	p1, [p7 + 52]
	STP	p1, [p7 + 20]                   ; 4-byte Folded Spill
	MOVI	r2, 4
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	BRA	.LBB9_3
.LBB9_2:                                ; %for.cond.loopexit
                                        ;   in Loop: Header=BB9_3 Depth=1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r3, r12
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r8, r2
	CPC	r9, r3
	BLO	.LBB9_12
.LBB9_3:                                ; %for.cond1.preheader
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB9_4 Depth 2
                                        ;       Child Loop BB9_6 Depth 3
                                        ;         Child Loop BB9_10 Depth 4
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p4, p2
	SHRI	r2, r8, 1
	SHLI	r3, r9, 15
	OR	r4, r2, r3
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	SHRI	r5, r9, 1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	MOVP	p4, p2
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	BLO	.LBB9_4
	BRA	.LBB9_2
.LBB9_7:                                ; %for.cond.cleanup10
                                        ;   in Loop: Header=BB9_4 Depth=2
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPI	p4, p4, 1
	MOVP	p2, p1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r8, r2
	CPC	r9, r3
	BHS	.LBB9_2
.LBB9_4:                                ; %for.cond5.preheader
                                        ;   Parent Loop BB9_3 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB9_6 Depth 3
                                        ;         Child Loop BB9_10 Depth 4
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	CMP	r8, r4
	CPC	r9, r5
	MOVP	p1, p2
	BLO	.LBB9_7
; %bb.5:                                ; %land.rhs.preheader
                                        ;   in Loop: Header=BB9_4 Depth=2
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	BRA	.LBB9_6
.LBB9_11:                               ; %swap.exit
                                        ;   in Loop: Header=BB9_6 Depth=3
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	CMP	r8, r2
	CPC	r9, r3
	BLO	.LBB9_7
.LBB9_6:                                ; %land.rhs
                                        ;   Parent Loop BB9_3 Depth=1
                                        ;     Parent Loop BB9_4 Depth=2
                                        ; =>    This Loop Header: Depth=3
                                        ;         Child Loop BB9_10 Depth 4
	STP	p6, [p7 + 32]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	SUB	r2, r8, r2
	SBC	r3, r9, r3
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p2, p3
	JAL	__mulsi3
	MOVP	p5, p1
	MOVP	p1, p4
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPP	p4, p2, p1
	ADDPP	p5, p2, p5
	MOVP	p1, p5
	MOVP	p2, p4
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	JALR	p3
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 36]                   ; 4-byte Folded Reload
	MOV	r4, r12
	MOV	r5, r12
	CMP	r2, r4
	CPC	r3, r5
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	BLE	.LBB9_7
; %bb.8:                                ; %for.body11
                                        ;   in Loop: Header=BB9_6 Depth=3
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB9_11
; %bb.9:                                ; %while.body.i.preheader
                                        ;   in Loop: Header=BB9_6 Depth=3
	MOVP	p1, p3
.LBB9_10:                               ; %while.body.i
                                        ;   Parent Loop BB9_3 Depth=1
                                        ;     Parent Loop BB9_4 Depth=2
                                        ;       Parent Loop BB9_6 Depth=3
                                        ; =>      This Inner Loop Header: Depth=4
	LDBU	r4, [p5]
	LDBU	r5, [p4]
	STB	r5, [p5]
	STB	r4, [p4]
	ADDPI	p4, p4, 1
	ADDPI	p5, p5, 1
	ADDPI	p1, p1, -1
	MOV	r13, r12
	CMP	r2, r12
	CPC	r3, r13
	BNE	.LBB9_10
	BRA	.LBB9_11
                                        ; -- End function
	.data
	.p2align	1, 0x0                          ; @seed
seed:
	.long	1                               ; 0x1

	.section	".note.GNU-stack","",@progbits
	.addrsig
