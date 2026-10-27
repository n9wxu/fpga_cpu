	.text
	.globl	fopen                           ; -- Begin function fopen
	.p2align	1
fopen:                                  ; @fopen
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -38
	STP	p4, [p7 + 34]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 30]                   ; 4-byte Folded Spill
	MOVP	p3, p1
	LDBU	r2, [files+2]
	CMPI	r2, 0
	BEQ	.LBB0_1
; %bb.2:                                ; %for.cond
	LDBU	r2, [files+18]
	CMPI	r2, 0
	BEQ	.LBB0_3
; %bb.4:                                ; %for.cond.1
	LDBU	r2, [files+34]
	CMPI	r2, 0
	BEQ	.LBB0_5
; %bb.6:                                ; %for.cond.2
	LDBU	r2, [files+50]
	CMPI	r2, 0
	BEQ	.LBB0_7
; %bb.8:                                ; %for.cond.3
	LDBU	r2, [files+66]
	CMPI	r2, 0
	BEQ	.LBB0_9
; %bb.10:                               ; %for.cond.4
	LDBU	r2, [files+82]
	CMPI	r2, 0
	BEQ	.LBB0_11
; %bb.12:                               ; %for.cond.5
	LDBU	r2, [files+98]
	CMPI	r2, 0
	BEQ	.LBB0_13
; %bb.14:                               ; %for.cond.6
	LDBU	r2, [files+114]
	CMPI	r2, 0
	BEQ	.LBB0_15
; %bb.36:                               ; %for.cond.7
	MOVI	r3, 0
	MOVI	r2, 24
	STP	p1, [errno]
	MOV	r2, r3
	BRA	.LBB0_35
.LBB0_1:
	LEA	p1, [files]
	BRA	.LBB0_16
.LBB0_3:
	LEA	p1, [files+16]
	BRA	.LBB0_16
.LBB0_5:
	LEA	p1, [files+32]
	BRA	.LBB0_16
.LBB0_7:
	LEA	p1, [files+48]
	BRA	.LBB0_16
.LBB0_9:
	LEA	p1, [files+64]
	BRA	.LBB0_16
.LBB0_11:
	LEA	p1, [files+80]
	BRA	.LBB0_16
.LBB0_13:
	LEA	p1, [files+96]
	BRA	.LBB0_16
.LBB0_15:
	LEA	p1, [files+112]
.LBB0_16:                               ; %if.end4
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7 + 10]                   ; 4-byte Folded Spill
	MOVI	r9, 0
	MOVI	r8, 47
	LDBU	r2, [p2]
	STW	r2, [p7 + 8]                    ; 2-byte Folded Spill
	MOVP	p1, p3
	MOVP	p2, p4
	MOVP	p5, p3
	JAL	strrchr
	MOVP	p2, p5
	MOV	r6, r9
	MOV	r7, r9
	STP	p3, [p7]                        ; 4-byte Folded Spill
	CMP	r2, r6
	CPC	r3, r7
	BEQ	.LBB0_18
; %bb.17:                               ; %if.end4
	ADDPI	p2, p1, 1
.LBB0_18:                               ; %if.end4
	MOVI	r8, 15
	LEA	p5, [p7 + 14]
	MOVP	p1, p5
	MOVP	p3, p4
	JAL	strncpy
	STB	r9, [p7 + 29]
	STP	p5, [15729928]
	MOVI	r8, 1
	LDW	r5, [p7 + 8]                    ; 2-byte Folded Reload
	CMPI	r5, 97
	MOVP	p1, p4
	BEQ	.LBB0_20
; %bb.19:                               ; %if.end4
	LDP	p1, [p7]                        ; 4-byte Folded Reload
.LBB0_20:                               ; %if.end4
	CMPI	r5, 119
	MOV	r4, r8
	BEQ	.LBB0_22
; %bb.21:                               ; %if.end4
	SHLI	r4, r2, 1
.LBB0_22:                               ; %if.end4
	STW	r4, [15729926]
	STW	r8, [15729920]
	LDP	p6, [p7 + 10]                   ; 4-byte Folded Reload
.LBB0_23:                               ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15729922]
	CMPI	r2, 0
	BLT	.LBB0_23
; %bb.24:                               ; %fs.exit
	LDW	r8, [15729938]
	CMPI	r8, -1
	BLE	.LBB0_25
; %bb.29:                               ; %if.end22
	MOVI	r7, 0
	MOVI	r6, 16
	MOV	r9, r5
	MOV	r4, r7
	MOV	r5, r7
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p5
	JAL	memset
	MOVP	p1, p5
	MOVI	r4, 1
	STB	r4, [p1 + 2]
	STW	r8, [p1]
	CMPI	r9, 97
	BNE	.LBB0_30
; %bb.31:                               ; %if.then28
	STW	r8, [15729924]
	MOVI	r4, 9
	STW	r4, [15729920]
	LDP	p6, [p7 + 10]                   ; 4-byte Folded Reload
.LBB0_32:                               ; %while.cond.i55
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r4, [15729922]
	CMPI	r4, 0
	BLT	.LBB0_32
; %bb.33:                               ; %fs.exit57
	LDW	r4, [15729938]
	CMPI	r4, 0
	BLT	.LBB0_35
; %bb.34:                               ; %if.then33
	LDP	p2, [15729940]
	STP	p2, [p1 + 8]
	STP	p2, [p1 + 12]
	BRA	.LBB0_35
.LBB0_25:                               ; %if.then17
	MOVI	r2, 0
	CMPI	r8, -1
	BEQ	.LBB0_26
; %bb.27:                               ; %if.then17
	MOVI	r4, 5
	BRA	.LBB0_28
.LBB0_30:
	LDP	p6, [p7 + 10]                   ; 4-byte Folded Reload
	BRA	.LBB0_35
.LBB0_26:
	MOVI	r4, 2
.LBB0_28:                               ; %if.then17
	MOV	r5, r2
	STP	p2, [errno]
	MOV	r3, r2
.LBB0_35:                               ; %cleanup36
	LDP	p4, [p7 + 34]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 30]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 38
	RET
                                        ; -- End function
	.globl	fclose                          ; -- Begin function fclose
	.p2align	1
fclose:                                 ; @fclose
; %bb.0:                                ; %entry
	MOVP	p2, p1
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_5
; %bb.1:                                ; %lor.lhs.false
	LDW	r6, [p2]
	CMPI	r6, 0
	BLT	.LBB1_5
; %bb.2:                                ; %if.end
	STW	r6, [15729924]
	MOVI	r6, 2
	STW	r6, [15729920]
.LBB1_3:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r6, [15729922]
	CMPI	r6, 0
	BLT	.LBB1_3
; %bb.4:                                ; %fs.exit
	LDW	r6, [15729938]
	MOVI	r6, 0
	STB	r6, [p2 + 2]
.LBB1_5:                                ; %return
	RET
                                        ; -- End function
	.globl	fread                           ; -- Begin function fread
	.p2align	1
fread:                                  ; @fread
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p4, p3
	MOVP	p5, p2
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p4
	JAL	__mulsi3
	STP	p1, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r2, 0
	MOV	r3, r2
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB2_30
; %bb.1:                                ; %entry
	CMP	r8, r2
	CPC	r9, r3
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	BEQ	.LBB2_31
; %bb.2:                                ; %if.end
	LDP	p5, [p7 + 32]
	LDW	r2, [p5]
	CMPI	r2, -1
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	BLE	.LBB2_3
; %bb.8:                                ; %if.end12
	MOVI	r6, 0
	LDBU	r2, [p5 + 5]
	CMPI	r2, 0
	BEQ	.LBB2_9
; %bb.10:                               ; %if.then14
	LDBU	r2, [p5 + 6]
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	STB	r2, [p2]
	STB	r6, [p5 + 5]
	MOVI	r2, 1
	MOV	r3, r6
	MOVP	p3, p1
	BRA	.LBB2_11
.LBB2_3:                                ; %while.cond.preheader
	MOVI	r8, 0
	MOV	r9, r8
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB2_29
; %bb.4:                                ; %while.body.preheader
	MOVI	r10, -1
	MOV	r11, r10
.LBB2_5:                                ; %while.body
                                        ; =>This Inner Loop Header: Depth=1
	JAL	getchar
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB2_6
; %bb.7:                                ; %cleanup
                                        ;   in Loop: Header=BB2_5 Depth=1
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPP	p2, p2, p4
	STB	r2, [p2]
	ADDPI	p4, p4, 1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r8, r2
	CPC	r9, r3
	BLO	.LBB2_5
	BRA	.LBB2_29
.LBB2_9:
	MOV	r7, r6
.LBB2_11:                               ; %if.end16
	LDP	p1, [p5 + 8]
	LDP	p2, [p5 + 12]
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB2_17
; %bb.12:                               ; %if.end.i.i
	LDW	r4, [p5]
	STW	r4, [15729924]
	STP	p1, [15729944]
	MOVI	r2, 8
	STW	r2, [15729920]
.LBB2_13:                               ; %while.cond.i.i.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15729922]
	CMPI	r2, 0
	BLT	.LBB2_13
; %bb.14:                               ; %fs.exit.i.i
	LDW	r2, [15729938]
	CMPI	r2, -1
	BLE	.LBB2_15
; %bb.16:                               ; %if.end5.i.i
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p1, [p5 + 8]
	STP	p1, [p5 + 12]
.LBB2_17:                               ; %while.cond.i.preheader
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	SUB	r8, r2, r6
	SBC	r9, r3, r7
	STP	p3, [p7]                        ; 4-byte Folded Spill
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p3
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r13, 0
	MOV	r12, r13
	MOVP	p2, p6
	MOVI	r12, 16384
	MOVP	p3, p5
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
.LBB2_18:                               ; %while.cond.i
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB2_22 Depth 2
	CMP	r8, r4
	CPC	r9, r5
	BLS	.LBB2_27
; %bb.19:                               ; %while.body.i
                                        ;   in Loop: Header=BB2_18 Depth=1
	LDW	r2, [p3]
	STW	r2, [15729924]
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p2
	STP	p1, [15729932]
	SUB	r8, r8, r4
	SBC	r9, r9, r5
	CMP	r8, r12
	CPC	r9, r13
	BLO	.LBB2_21
; %bb.20:                               ; %while.body.i
                                        ;   in Loop: Header=BB2_18 Depth=1
	MOVP	p4, p6
.LBB2_21:                               ; %while.body.i
                                        ;   in Loop: Header=BB2_18 Depth=1
	STW	r8, [15729936]
	MOVI	r2, 3
	STW	r2, [15729920]
.LBB2_22:                               ; %while.cond.i.i
                                        ;   Parent Loop BB2_18 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDW	r2, [15729922]
	CMPI	r2, 0
	BLT	.LBB2_22
; %bb.23:                               ; %fs.exit.i
                                        ;   in Loop: Header=BB2_18 Depth=1
	LDW	r2, [15729938]
	CMPI	r2, -1
	BLE	.LBB2_24
; %bb.25:                               ; %if.end8.i
                                        ;   in Loop: Header=BB2_18 Depth=1
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p3, [p3 + 8]
	ADDPU	p3, p3, r2
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	STP	p3, [p5 + 12]
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	STP	p3, [p5 + 8]
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPU	p2, p2, r2
	MOV	r3, r13
	CMP	r8, r2
	CPC	r9, r3
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	BLS	.LBB2_18
; %bb.26:                               ; %if.then16.i
	MOVI	r2, 1
	STB	r2, [p3 + 3]
	BRA	.LBB2_27
.LBB2_24:                               ; %if.then7.i
	MOVI	r2, 1
	STB	r2, [p3 + 4]
.LBB2_27:                               ; %xfer.exit
	LDP	p3, [p7]                        ; 4-byte Folded Reload
.LBB2_28:                               ; %xfer.exit
	ADDPP	p4, p2, p3
.LBB2_29:                               ; %cleanup19.sink.split
	MOVP	p1, p4
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	JAL	__udivsi3
.LBB2_30:                               ; %cleanup19
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
.LBB2_31:                               ; %cleanup19
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
.LBB2_6:                                ; %cleanup.thread
	MOVI	r2, 1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	STB	r2, [p2 + 3]
	BRA	.LBB2_29
.LBB2_15:                               ; %sync_pos.exit.thread.i
	MOVI	r2, 1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	STB	r2, [p2 + 4]
	MOVI	r4, 0
	MOV	r5, r4
	BRA	.LBB2_28
                                        ; -- End function
	.globl	fwrite                          ; -- Begin function fwrite
	.p2align	1
fwrite:                                 ; @fwrite
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p4, p2
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p3
	JAL	__mulsi3
	MOVP	p3, p1
	MOVI	r2, 0
	MOV	r3, r2
	STP	p4, [p7]                        ; 4-byte Folded Spill
	CMP	r8, r2
	CPC	r9, r3
	BEQ	.LBB3_25
; %bb.1:                                ; %entry
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB3_25
; %bb.2:                                ; %if.end
	LDP	p4, [p7 + 28]
	LDW	r2, [p4]
	CMPI	r2, -1
	STP	p3, [p7 + 16]                   ; 4-byte Folded Spill
	BLE	.LBB3_3
; %bb.8:                                ; %if.end7
	MOVI	r13, 0
	STB	r13, [p4 + 5]
	LDP	p2, [p4 + 8]
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p4 + 12]
	CMP	r4, r8
	CPC	r5, r9
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	BEQ	.LBB3_14
; %bb.9:                                ; %if.end.i.i
	STW	r2, [15729924]
	STP	p2, [15729944]
	MOVI	r2, 8
	STW	r2, [15729920]
.LBB3_10:                               ; %while.cond.i.i.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15729922]
	CMPI	r2, 0
	BLT	.LBB3_10
; %bb.11:                               ; %fs.exit.i.i
	LDW	r2, [15729938]
	CMPI	r2, 0
	BLT	.LBB3_12
; %bb.13:                               ; %if.end5.i.i
	LDP	p1, [p4 + 8]
	STP	p1, [p4 + 12]
	MOVI	r13, 0
.LBB3_14:                               ; %while.cond.i.preheader
	MOV	r12, r13
	MOVP	p1, p6
	MOVI	r12, 16384
	MOVP	p2, p6
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
.LBB3_15:                               ; %while.cond.i
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB3_19 Depth 2
	CMP	r6, r2
	CPC	r7, r3
	BLS	.LBB3_24
; %bb.16:                               ; %while.body.i
                                        ;   in Loop: Header=BB3_15 Depth=1
	LDW	r13, [p4]
	STW	r13, [15729924]
	ADDPP	p4, p5, p1
	STP	p4, [15729932]
	SUB	r10, r6, r2
	SBC	r11, r7, r3
	MOVP	p6, p2
	CMP	r10, r12
	CPC	r11, r13
	BLO	.LBB3_18
; %bb.17:                               ; %while.body.i
                                        ;   in Loop: Header=BB3_15 Depth=1
	MOVP	p5, p6
.LBB3_18:                               ; %while.body.i
                                        ;   in Loop: Header=BB3_15 Depth=1
	STW	r10, [15729936]
	MOVI	r6, 4
	STW	r6, [15729920]
.LBB3_19:                               ; %while.cond.i.i
                                        ;   Parent Loop BB3_15 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDW	r13, [15729922]
	CMPI	r13, 0
	BLT	.LBB3_19
; %bb.20:                               ; %fs.exit.i
                                        ;   in Loop: Header=BB3_15 Depth=1
	LDW	r8, [15729938]
	CMPI	r8, 0
	BLT	.LBB3_21
; %bb.22:                               ; %if.end8.i
                                        ;   in Loop: Header=BB3_15 Depth=1
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p6, [p3 + 8]
	ADDPU	p6, p6, r8
	STP	p6, [p3 + 12]
	STP	p6, [p3 + 8]
	ADDPU	p1, p1, r8
	MOV	r9, r5
	CMP	r10, r8
	CPC	r11, r9
	MOVP	p4, p3
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 16]                   ; 4-byte Folded Reload
	BLS	.LBB3_15
	BRA	.LBB3_23
.LBB3_3:                                ; %for.cond.preheader
	MOVI	r9, 0
	MOV	r8, r9
	CMP	r6, r8
	CPC	r7, r9
	BEQ	.LBB3_4
; %bb.5:                                ; %for.body.preheader
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p5, p4
.LBB3_6:                                ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p5
	LDBU	r8, [p1]
	MOVP	p1, p4
	JAL	putchar
	ADDPI	p5, p5, 1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r10, r2
	CPC	r11, r3
	BLO	.LBB3_6
; %bb.7:
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB3_25
.LBB3_21:
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB3_23
.LBB3_4:
	MOVP	p1, p2
	BRA	.LBB3_25
.LBB3_12:
	MOVI	r2, 0
	MOV	r3, r2
.LBB3_23:                               ; %xfer.exit.sink.split
	MOVI	r6, 1
	STB	r6, [p4 + 4]
.LBB3_24:                               ; %xfer.exit
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	JAL	__udivsi3
.LBB3_25:                               ; %cleanup
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	fseek                           ; -- Begin function fseek
	.p2align	1
fseek:                                  ; @fseek
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	LDW	r10, [p1]
	CMPI	r10, 0
	BLT	.LBB4_1
; %bb.2:                                ; %if.end
	MOVI	r9, 0
	MOVI	r8, 2
	CMP	r6, r8
	CPC	r7, r9
	BEQ	.LBB4_7
; %bb.3:                                ; %if.end
	MOV	r8, r9
	MOVI	r10, 1
	MOV	r11, r9
	CMP	r6, r10
	CPC	r7, r11
	BNE	.LBB4_11
; %bb.4:                                ; %if.then4
	MOVI	r11, 0
	LDBU	r6, [p1 + 5]
	CMPI	r6, 0
	BNE	.LBB4_6
; %bb.5:                                ; %if.then4
	MOV	r10, r11
.LBB4_6:                                ; %if.then4
	LDP	p3, [p1 + 8]
	SUB	r8, r6, r10
	SBC	r9, r7, r11
	BRA	.LBB4_11
.LBB4_7:                                ; %if.then8
	STW	r10, [15729924]
	MOVI	r6, 9
	STW	r6, [15729920]
.LBB4_8:                                ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r6, [15729922]
	CMPI	r6, 0
	BLT	.LBB4_8
; %bb.9:                                ; %fs.exit
	LDW	r6, [15729938]
	CMPI	r6, 0
	BLT	.LBB4_1
; %bb.10:                               ; %if.end14
	LDP	p4, [15729940]
.LBB4_11:                               ; %if.end16
	ADDPP	p3, p4, p2
	MOVI	r4, -1
	MOV	r5, r4
	CMP	r6, r4
	CPC	r7, r5
	BLE	.LBB4_12
; %bb.13:                               ; %if.end20
	STP	p3, [p1 + 8]
	MOVI	r4, 0
	STB	r4, [p1 + 5]
	STB	r4, [p1 + 3]
	BRA	.LBB4_14
.LBB4_1:
	MOVI	r4, -1
.LBB4_14:                               ; %cleanup
	MOV	r5, r4
	BRA	.LBB4_15
.LBB4_12:                               ; %if.then19
	MOVI	r3, 0
	MOVI	r2, 22
	STP	p1, [errno]
.LBB4_15:                               ; %cleanup
	MOVP	p1, p2
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	ftell                           ; -- Begin function ftell
	.p2align	1
ftell:                                  ; @ftell
; %bb.0:                                ; %entry
	LDW	r4, [p1]
	CMPI	r4, 0
	BLT	.LBB5_1
; %bb.2:                                ; %cond.false
	MOVI	r5, 0
	LDBU	r6, [p1 + 5]
	CMPI	r6, 0
	BNE	.LBB5_3
; %bb.4:                                ; %cond.false
	MOV	r4, r5
	BRA	.LBB5_5
.LBB5_1:
	MOVI	r2, -1
	MOV	r3, r2
	RET
.LBB5_3:
	MOVI	r4, 1
.LBB5_5:                                ; %cond.false
	LDP	p1, [p1 + 8]
	SUB	r2, r2, r4
	SBC	r3, r3, r5
	RET
                                        ; -- End function
	.globl	rewind                          ; -- Begin function rewind
	.p2align	1
rewind:                                 ; @rewind
; %bb.0:                                ; %entry
	LDW	r4, [p1]
	CMPI	r4, 0
	BLT	.LBB6_2
; %bb.1:                                ; %if.end.i
	MOVI	r4, 0
	STB	r4, [p1 + 5]
	STB	r4, [p1 + 3]
	MOV	r5, r4
	STP	p2, [p1 + 8]
.LBB6_2:                                ; %fseek.exit
	MOVI	r4, 0
	STB	r4, [p1 + 4]
	RET
                                        ; -- End function
	.globl	feof                            ; -- Begin function feof
	.p2align	1
feof:                                   ; @feof
; %bb.0:                                ; %entry
	LDBU	r2, [p1 + 3]
	MOVI	r3, 0
	RET
                                        ; -- End function
	.globl	ferror                          ; -- Begin function ferror
	.p2align	1
ferror:                                 ; @ferror
; %bb.0:                                ; %entry
	LDBU	r2, [p1 + 4]
	MOVI	r3, 0
	RET
                                        ; -- End function
	.globl	clearerr                        ; -- Begin function clearerr
	.p2align	1
clearerr:                               ; @clearerr
; %bb.0:                                ; %entry
	MOVI	r4, 0
	STB	r4, [p1 + 3]
	STB	r4, [p1 + 4]
	RET
                                        ; -- End function
	.globl	fflush                          ; -- Begin function fflush
	.p2align	1
fflush:                                 ; @fflush
; %bb.0:                                ; %entry
	MOVI	r2, 0
	MOV	r3, r2
	RET
                                        ; -- End function
	.globl	fgetc                           ; -- Begin function fgetc
	.p2align	1
fgetc:                                  ; @fgetc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -14
	STP	p4, [p7 + 10]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 6]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	STP	p1, [p7]
	MOVI	r11, 0
	MOVI	r10, 1
	LEA	p1, [p7 + 5]
	MOVP	p2, p5
	MOVP	p3, p5
	JAL	fread
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB11_1
; %bb.2:                                ; %entry
	MOVI	r10, -1
	MOV	r11, r10
	BRA	.LBB11_3
.LBB11_1:
	LDBU	r10, [p7 + 5]
.LBB11_3:                               ; %entry
	MOVP	p6, p4
	MOVP	p1, p5
	LDP	p4, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 6]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 14
	RET
                                        ; -- End function
	.globl	getc                            ; -- Begin function getc
	.p2align	1
getc:                                   ; @getc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -14
	STP	p4, [p7 + 10]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 6]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	STP	p1, [p7]
	MOVI	r11, 0
	MOVI	r10, 1
	LEA	p1, [p7 + 5]
	MOVP	p2, p5
	MOVP	p3, p5
	JAL	fread
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB12_1
; %bb.2:                                ; %entry
	MOVI	r10, -1
	MOV	r11, r10
	BRA	.LBB12_3
.LBB12_1:
	LDBU	r10, [p7 + 5]
.LBB12_3:                               ; %entry
	MOVP	p6, p4
	MOVP	p1, p5
	LDP	p4, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 6]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 14
	RET
                                        ; -- End function
	.globl	ungetc                          ; -- Begin function ungetc
	.p2align	1
ungetc:                                 ; @ungetc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p3, p1
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB13_4
; %bb.1:                                ; %lor.lhs.false
	LDW	r8, [p2]
	CMPI	r8, 0
	BLT	.LBB13_4
; %bb.2:                                ; %lor.lhs.false3
	MOVI	r2, -1
	MOV	r3, r2
	LDBU	r8, [p2 + 5]
	CMPI	r8, 0
	BNE	.LBB13_4
; %bb.3:                                ; %if.end
	MOVI	r2, 1
	STB	r2, [p2 + 5]
	MOVI	r2, 0
	STB	r2, [p2 + 3]
	STB	r6, [p2 + 6]
	MOVP	p1, p3
.LBB13_4:                               ; %return
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	fgets                           ; -- Begin function fgets
	.p2align	1
fgets:                                  ; @fgets
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -34
	STP	p4, [p7 + 30]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 26]                   ; 4-byte Folded Spill
	STP	p3, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p3, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r3, 0
	MOV	r2, r3
	MOVP	p5, p1
	ADDPI	p2, p2, -1
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVI	r2, 1
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
.LBB14_1:                               ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	CMP	r10, r2
	CPC	r11, r3
	BGE	.LBB14_5
; %bb.2:                                ; %while.body
                                        ;   in Loop: Header=BB14_1 Depth=1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	STP	p1, [p7]
	LEA	p1, [p7 + 25]
	MOVP	p2, p4
	MOVP	p3, p4
	JAL	fread
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB14_5
; %bb.3:                                ; %if.end
                                        ;   in Loop: Header=BB14_1 Depth=1
	LDBU	r2, [p7 + 25]
	ADDPP	p2, p3, p5
	STB	r2, [p2]
	ADDPI	p5, p5, 1
	ANDI	r2, r2, 255
	CMPI	r2, 10
	BNE	.LBB14_1
	BRA	.LBB14_4
.LBB14_5:                               ; %while.end
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB14_6
.LBB14_4:                               ; %if.end7
	ADDPP	p1, p3, p5
	MOVI	r4, 0
	STB	r4, [p1]
	BRA	.LBB14_7
.LBB14_6:
	MOVP	p3, p1
.LBB14_7:                               ; %cleanup9
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p3
	LDP	p4, [p7 + 30]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 26]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 34
	RET
                                        ; -- End function
	.globl	fputc                           ; -- Begin function fputc
	.p2align	1
fputc:                                  ; @fputc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -14
	STP	p4, [p7 + 10]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 6]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	STB	r2, [p7 + 5]
	STP	p2, [p7]
	MOVI	r11, 0
	MOVI	r10, 1
	LEA	p1, [p7 + 5]
	MOVP	p2, p5
	MOVP	p3, p5
	JAL	fwrite
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB15_1
; %bb.2:                                ; %entry
	MOVI	r10, -1
	MOV	r11, r10
	BRA	.LBB15_3
.LBB15_1:
	LDBU	r10, [p7 + 5]
.LBB15_3:                               ; %entry
	MOVP	p6, p4
	MOVP	p1, p5
	LDP	p4, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 6]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 14
	RET
                                        ; -- End function
	.globl	putc                            ; -- Begin function putc
	.p2align	1
putc:                                   ; @putc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -14
	STP	p4, [p7 + 10]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 6]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	STB	r2, [p7 + 5]
	STP	p2, [p7]
	MOVI	r11, 0
	MOVI	r10, 1
	LEA	p1, [p7 + 5]
	MOVP	p2, p5
	MOVP	p3, p5
	JAL	fwrite
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB16_1
; %bb.2:                                ; %entry
	MOVI	r10, -1
	MOV	r11, r10
	BRA	.LBB16_3
.LBB16_1:
	LDBU	r10, [p7 + 5]
.LBB16_3:                               ; %entry
	MOVP	p6, p4
	MOVP	p1, p5
	LDP	p4, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 6]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 14
	RET
                                        ; -- End function
	.globl	fputs                           ; -- Begin function fputs
	.p2align	1
fputs:                                  ; @fputs
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p5, p2
	MOVP	p4, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	JAL	strlen
	MOVP	p3, p1
	STP	p5, [p7]
	MOVI	r11, 0
	MOVI	r10, 1
	MOVP	p1, p4
	MOVP	p2, p5
	MOVP	p4, p3
	JAL	fwrite
	MOVP	p2, p1
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r4, r8
	CPC	r5, r9
	BNE	.LBB17_2
; %bb.1:                                ; %entry
	MOV	r10, r11
	MOVP	p1, p5
.LBB17_2:                               ; %entry
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	vfprintf                        ; -- Begin function vfprintf
	.p2align	1
vfprintf:                               ; @vfprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -296
	STP	p4, [p7 + 292]                  ; 4-byte Folded Spill
	STP	p5, [p7 + 288]                  ; 4-byte Folded Spill
	STP	p3, [p7 + 284]
	LDW	r8, [p1]
	CMPI	r8, -1
	BLE	.LBB18_1
; %bb.2:                                ; %if.end
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 284]
	STP	p1, [p7 + 24]
	STP	p1, [p7]
	MOVP	p3, p2
	MOVI	r9, 0
	MOVI	r8, 256
	LEA	p5, [p7 + 28]
	MOVP	p1, p5
	MOVP	p2, p4
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	JAL	vsnprintf
	MOVP	p3, p1
	CMP	r6, r8
	CPC	r7, r9
	BLT	.LBB18_6
; %bb.3:                                ; %if.then6
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	ADDPI	p1, p3, 1
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	JAL	malloc
	MOVP	p5, p1
	MOV	r2, r9
	MOV	r3, r9
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB18_4
; %bb.5:                                ; %if.end9
	LDP	p1, [p7 + 24]
	STP	p1, [p7]
	MOVP	p1, p5
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	JAL	vsnprintf
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
.LBB18_6:                               ; %if.end12
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	STP	p1, [p7]
	MOVI	r8, 1
	MOVP	p1, p5
	MOVP	p2, p4
	MOVP	p4, p3
	JAL	fwrite
	LEA	p1, [p7 + 28]
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB18_8
; %bb.7:                                ; %if.then17
	MOVP	p1, p5
	JAL	free
.LBB18_8:                               ; %cleanup
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	BRA	.LBB18_9
.LBB18_1:                               ; %if.then
	MOVP	p1, p2
	MOVP	p2, p3
	MOVP	p4, p6
	JAL	vprintf
	MOVP	p6, p4
	BRA	.LBB18_9
.LBB18_4:                               ; %if.then8
	MOVI	r2, -1
	MOV	r3, r2
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
.LBB18_9:                               ; %cleanup
	LDP	p4, [p7 + 292]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 288]                  ; 4-byte Folded Reload
	ADDPI	p7, p7, 296
	RET
                                        ; -- End function
	.globl	fprintf                         ; -- Begin function fprintf
	.p2align	1
fprintf:                                ; @fprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	LEA	p3, [p7 + 16]
	STP	p3, [p7]
	LDP	p1, [p7 + 8]
	LDP	p2, [p7 + 12]
	JAL	vfprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	remove                          ; -- Begin function remove
	.p2align	1
remove:                                 ; @remove
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, 0
	MOVI	r10, 47
	MOVP	p2, p5
	JAL	strrchr
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	MOVP	p2, p4
	BEQ	.LBB20_2
; %bb.1:                                ; %entry
	ADDPI	p2, p1, 1
.LBB20_2:                               ; %entry
	MOVI	r10, 15
	LEA	p4, [p7 + 4]
	MOVP	p1, p4
	MOVP	p3, p5
	JAL	strncpy
	STB	r11, [p7 + 19]
	STP	p4, [15729928]
	MOVI	r2, 5
	STW	r2, [15729920]
.LBB20_3:                               ; %while.cond.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15729922]
	CMPI	r2, 0
	BLT	.LBB20_3
; %bb.4:                                ; %fs.exit
	MOVI	r2, 0
	MOV	r3, r2
	LDW	r4, [15729938]
	CMPI	r4, -1
	BGT	.LBB20_6
; %bb.5:                                ; %if.then
	MOVI	r4, 2
	MOV	r5, r2
	STP	p2, [errno]
	MOVI	r2, -1
	MOV	r3, r2
.LBB20_6:                               ; %cleanup
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	rename                          ; -- Begin function rename
	.p2align	1
rename:                                 ; @rename
; %bb.0:                                ; %entry
	MOVI	r3, 0
	MOVI	r2, 38
	STP	p1, [errno]
	MOVI	r2, -1
	MOV	r3, r2
	RET
                                        ; -- End function
	.data
	.p2align	1, 0x0                          ; @con_in
con_in:
	.word	65535                           ; 0xffff
	.byte	1                               ; 0x1
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.space	1
	.long	0                               ; 0x0
	.long	0                               ; 0x0

	.globl	stdin                           ; @stdin
	.p2align	1, 0x0
stdin:
	.long	con_in

	.p2align	1, 0x0                          ; @con_out
con_out:
	.word	65535                           ; 0xffff
	.byte	1                               ; 0x1
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.space	1
	.long	0                               ; 0x0
	.long	0                               ; 0x0

	.globl	stdout                          ; @stdout
	.p2align	1, 0x0
stdout:
	.long	con_out

	.p2align	1, 0x0                          ; @con_err
con_err:
	.word	65535                           ; 0xffff
	.byte	1                               ; 0x1
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.byte	0                               ; 0x0
	.space	1
	.long	0                               ; 0x0
	.long	0                               ; 0x0

	.globl	stderr                          ; @stderr
	.p2align	1, 0x0
stderr:
	.long	con_err

	.local	files                           ; @files
	.comm	files,128,2
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym con_in
	.addrsig_sym con_out
	.addrsig_sym con_err
	.addrsig_sym files
