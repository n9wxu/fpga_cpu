	.text
	.globl	__floatdisf                     ; -- Begin function __floatdisf
	.p2align	1
__floatdisf:                            ; @__floatdisf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -36
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p5, p1
	OR	r7, r11, r5
	OR	r6, r10, r4
	MOVI	r3, 0
	MOV	r2, r3
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_29
; %bb.1:                                ; %if.end.i
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_2
; %bb.3:                                ; %if.end.i
	MOVP	p3, p1
	BRA	.LBB0_4
.LBB0_2:
	MOVI	r6, 1
	MOV	r7, r3
.LBB0_4:                                ; %if.end.i
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB0_5
; %bb.6:                                ; %if.end.i
	MOVP	p4, p2
	BRA	.LBB0_7
.LBB0_5:
	ADDPP	p3, p2, p3
	SUB	r8, r2, r6
	SBC	r9, r3, r7
.LBB0_7:                                ; %if.end.i
	CMP	r4, r2
	CPC	r5, r3
	BGE	.LBB0_9
; %bb.8:
	SUB	r10, r2, r10
	SBC	r11, r3, r11
.LBB0_9:                                ; %if.end.i
	CMP	r8, r2
	CPC	r9, r3
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	BNE	.LBB0_10
; %bb.11:                               ; %if.end.i
	SHLI	r6, r11, 15
	SHRI	r7, r10, 1
	OR	r8, r7, r6
	SHRI	r9, r11, 1
	OR	r7, r11, r9
	OR	r6, r10, r8
	SHLI	r8, r7, 14
	SHRI	r9, r6, 2
	OR	r8, r9, r8
	SHRI	r9, r7, 2
	OR	r11, r7, r9
	OR	r10, r6, r8
	SHLI	r6, r11, 12
	SHRI	r7, r10, 4
	OR	r8, r7, r6
	SHRI	r9, r11, 4
	OR	r7, r11, r9
	OR	r6, r10, r8
	SHLI	r8, r7, 8
	SHRI	r9, r6, 8
	OR	r8, r9, r8
	SHRI	r9, r7, 8
	OR	r11, r7, r9
	OR	r10, r6, r8
	MOV	r6, r11
	MOV	r7, r3
	OR	r9, r11, r7
	OR	r8, r10, r6
	NOT	r7, r9
	NOT	r6, r8
	SHRI	r8, r6, 1
	SHLI	r9, r7, 15
	OR	r8, r8, r9
	SHRI	r9, r7, 1
	ANDI	r11, r9, 21845
	ANDI	r10, r8, 21845
	SUB	r6, r6, r10
	SBC	r7, r7, r11
	SHLI	r8, r7, 14
	SHRI	r9, r6, 2
	OR	r8, r9, r8
	SHRI	r9, r7, 2
	ANDI	r11, r9, 13107
	ANDI	r10, r8, 13107
	ANDI	r9, r7, 13107
	ANDI	r8, r6, 13107
	ADDPP	p3, p4, p5
	SHRI	r8, r6, 4
	SHLI	r9, r7, 12
	OR	r8, r8, r9
	SHRI	r9, r7, 4
	ADDPP	p4, p3, p4
	ANDI	r7, r9, 3855
	ANDI	r6, r8, 3855
	SHLI	r8, r7, 8
	SHRI	r9, r6, 8
	OR	r9, r8, r9
	SHLI	r8, r6, 8
	ADDPP	p3, p3, p4
	MOV	r8, r3
	MOV	r9, r6
	ADDPP	p3, p3, p4
	SHRI	r6, r7, 8
	MOV	r7, r3
	ADDPI	p5, p3, 32
	BRA	.LBB0_12
.LBB0_10:
	SHLI	r6, r9, 15
	SHRI	r7, r8, 1
	OR	r10, r7, r6
	SHRI	r11, r9, 1
	OR	r7, r9, r11
	OR	r6, r8, r10
	SHLI	r10, r7, 14
	SHRI	r11, r6, 2
	OR	r8, r11, r10
	SHRI	r9, r7, 2
	OR	r11, r7, r9
	OR	r10, r6, r8
	SHLI	r6, r11, 12
	SHRI	r7, r10, 4
	OR	r8, r7, r6
	SHRI	r9, r11, 4
	OR	r7, r11, r9
	OR	r6, r10, r8
	SHLI	r8, r7, 8
	SHRI	r9, r6, 8
	OR	r8, r9, r8
	SHRI	r9, r7, 8
	OR	r11, r7, r9
	OR	r10, r6, r8
	MOV	r6, r11
	MOV	r7, r3
	OR	r9, r11, r7
	OR	r8, r10, r6
	NOT	r7, r9
	NOT	r6, r8
	SHRI	r8, r6, 1
	SHLI	r9, r7, 15
	OR	r8, r8, r9
	SHRI	r9, r7, 1
	ANDI	r11, r9, 21845
	ANDI	r10, r8, 21845
	SUB	r6, r6, r10
	SBC	r7, r7, r11
	SHLI	r8, r7, 14
	SHRI	r9, r6, 2
	OR	r8, r9, r8
	SHRI	r9, r7, 2
	ANDI	r11, r9, 13107
	ANDI	r10, r8, 13107
	ANDI	r9, r7, 13107
	ANDI	r8, r6, 13107
	ADDPP	p3, p4, p5
	SHRI	r8, r6, 4
	SHLI	r9, r7, 12
	OR	r8, r8, r9
	SHRI	r9, r7, 4
	ADDPP	p4, p3, p4
	ANDI	r7, r9, 3855
	ANDI	r6, r8, 3855
	SHLI	r8, r7, 8
	SHRI	r9, r6, 8
	OR	r9, r8, r9
	SHLI	r8, r6, 8
	ADDPP	p3, p3, p4
	MOV	r8, r3
	MOV	r9, r6
	ADDPP	p3, p3, p4
	SHRI	r10, r7, 8
	MOV	r11, r3
.LBB0_12:                               ; %if.end.i
	MOVI	r2, 39
	XORI	r7, r11, 0
	XORI	r6, r10, 63
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	SARI	r4, r5, 15
	MOV	r5, r4
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	CMP	r10, r2
	CPC	r11, r3
	BHI	.LBB0_26
; %bb.13:                               ; %if.then4.i
	MOVI	r4, 0
	MOVI	r6, 38
	MOV	r7, r4
	CMP	r10, r6
	CPC	r11, r7
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	BEQ	.LBB0_14
; %bb.15:                               ; %if.then4.i
	MOVI	r6, 39
	MOV	r7, r4
	CMP	r10, r6
	CPC	r11, r7
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	BNE	.LBB0_17
; %bb.16:                               ; %if.then6.i
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	SHLI	r6, r9, 1
	SHRI	r7, r8, 15
	OR	r13, r6, r7
	SHLI	r12, r8, 1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	SHRI	r8, r5, 15
	MOVI	r9, 0
	OR	r7, r13, r9
	OR	r6, r12, r8
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	SHLI	r6, r5, 1
	SHRI	r7, r4, 15
	OR	r5, r6, r7
	SHLI	r4, r4, 1
	BRA	.LBB0_20
.LBB0_26:                               ; %if.else30.i
	ADDI	r6, r10, -40
	MOVI	r7, 0
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p4, p6
	JAL	__ashldi3
	MOVP	p6, p4
	BRA	.LBB0_27
.LBB0_14:
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	BRA	.LBB0_20
.LBB0_17:                               ; %if.else9.i
	STP	p1, [p7]                        ; 4-byte Folded Spill
	ADDI	r8, r10, 26
	MOVI	r9, 0
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p3, p4
	JAL	__ashldi3
	MOVP	p3, p4
	OR	r13, r3, r5
	OR	r12, r2, r4
	MOV	r8, r7
	MOV	r9, r7
	CMP	r12, r8
	CPC	r13, r9
	BEQ	.LBB0_19
; %bb.18:
	MOVI	r8, 1
	MOV	r9, r7
.LBB0_19:                               ; %if.else9.i
	MOVI	r2, 38
	SUB	r6, r2, r10
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__lshrdi3
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	OR	r5, r3, r9
	OR	r4, r2, r8
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p1, [p7]                        ; 4-byte Folded Reload
.LBB0_20:                               ; %if.end18.i
	SHLI	r6, r5, 14
	SHRI	r7, r4, 2
	OR	r6, r7, r6
	SHRI	r7, r5, 2
	ANDI	r9, r7, 0
	ANDI	r8, r6, 1
	OR	r7, r5, r9
	OR	r6, r4, r8
	ADDPI	p2, p3, 1
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r4, r10
	CPC	r5, r11
	BEQ	.LBB0_21
; %bb.22:                               ; %if.end18.i
	MOVP	p3, p5
	BRA	.LBB0_23
.LBB0_21:
	MOVI	r6, 1
	MOV	r7, r10
.LBB0_23:                               ; %if.end18.i
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPP	p3, p4, p3
	ANDI	r9, r5, 1024
	ANDI	r8, r4, 0
	CMP	r8, r10
	CPC	r9, r11
	BNE	.LBB0_25
; %bb.24:
	SHLI	r2, r5, 14
	SHRI	r3, r4, 2
	OR	r8, r3, r2
	SHRI	r9, r5, 2
	SHLI	r11, r6, 14
	OR	r3, r9, r11
	OR	r2, r8, r10
.LBB0_27:                               ; %if.end34.i
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	BRA	.LBB0_28
.LBB0_25:                               ; %if.then26.i
	MOVI	r2, 64
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r10, r2, r8
	SBC	r11, r3, r9
	SHRI	r2, r4, 3
	SHLI	r3, r5, 13
	OR	r8, r2, r3
	SHRI	r9, r5, 3
	SHLI	r5, r6, 13
	MOVI	r4, 0
	OR	r3, r9, r5
	OR	r2, r8, r4
.LBB0_28:                               ; %if.end34.i
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	ANDI	r9, r5, -32768
	ANDI	r8, r4, 0
	SHLI	r5, r10, 7
	MOVI	r4, 0
	OR	r7, r5, r9
	OR	r6, r4, r8
	MOVI	r5, 16256
	ANDI	r9, r3, 127
	ANDI	r8, r2, -1
	OR	r3, r7, r9
	OR	r2, r6, r8
	ADDPP	p1, p1, p2
.LBB0_29:                               ; %__floatXiYf__.exit
	LDP	p4, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 36
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
