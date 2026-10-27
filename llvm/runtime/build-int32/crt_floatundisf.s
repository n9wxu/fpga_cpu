	.text
	.globl	__floatundisf                   ; -- Begin function __floatundisf
	.p2align	1
__floatundisf:                          ; @__floatundisf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p5, p1
	OR	r7, r11, r5
	OR	r6, r10, r4
	MOVI	r3, 0
	MOV	r2, r3
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_19
; %bb.1:                                ; %if.end.i
	CMP	r4, r2
	CPC	r5, r3
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	BNE	.LBB0_2
; %bb.3:                                ; %if.end.i
	SHLI	r6, r11, 15
	SHRI	r7, r10, 1
	OR	r8, r7, r6
	SHRI	r9, r11, 1
	OR	r7, r11, r9
	OR	r6, r10, r8
	SHLI	r8, r7, 14
	SHRI	r9, r6, 2
	OR	r10, r9, r8
	SHRI	r11, r7, 2
	OR	r9, r7, r11
	OR	r8, r6, r10
	SHLI	r6, r9, 12
	SHRI	r7, r8, 4
	OR	r10, r7, r6
	SHRI	r11, r9, 4
	OR	r7, r9, r11
	OR	r6, r8, r10
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
	ADDPI	p4, p3, 32
	BRA	.LBB0_4
.LBB0_2:
	SHLI	r6, r5, 15
	SHRI	r7, r4, 1
	OR	r8, r7, r6
	SHRI	r9, r5, 1
	OR	r7, r5, r9
	OR	r6, r4, r8
	SHLI	r8, r7, 14
	SHRI	r9, r6, 2
	OR	r10, r9, r8
	SHRI	r11, r7, 2
	OR	r9, r7, r11
	OR	r8, r6, r10
	SHLI	r6, r9, 12
	SHRI	r7, r8, 4
	OR	r10, r7, r6
	SHRI	r11, r9, 4
	OR	r7, r9, r11
	OR	r6, r8, r10
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
	SHRI	r8, r7, 8
	MOV	r9, r3
.LBB0_4:                                ; %if.end.i
	MOVI	r2, 39
	XORI	r11, r9, 0
	XORI	r10, r8, 63
	CMP	r8, r2
	CPC	r9, r3
	BHI	.LBB0_17
; %bb.5:                                ; %if.then4.i
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r4, 0
	MOVI	r6, 38
	MOV	r7, r4
	CMP	r8, r6
	CPC	r9, r7
	LDP	p5, [p7 + 16]                   ; 4-byte Folded Reload
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	BEQ	.LBB0_11
; %bb.6:                                ; %if.then4.i
	MOVI	r6, 39
	MOV	r7, r4
	CMP	r8, r6
	CPC	r9, r7
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	BNE	.LBB0_8
; %bb.7:                                ; %if.then6.i
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	SHLI	r6, r5, 1
	SHRI	r7, r4, 15
	OR	r7, r6, r7
	SHLI	r6, r4, 1
	MOVP	p6, p5
	SHRI	r10, r13, 15
	MOVI	r11, 0
	OR	r5, r7, r11
	OR	r4, r6, r10
	MOVP	p5, p6
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	SHLI	r6, r11, 1
	SHRI	r7, r10, 15
	OR	r11, r6, r7
	SHLI	r10, r10, 1
	BRA	.LBB0_11
.LBB0_17:                               ; %if.else29.i
	ADDI	r6, r8, -40
	MOVI	r7, 0
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	MOVP	p4, p6
	JAL	__ashldi3
	MOVP	p6, p4
	BRA	.LBB0_18
.LBB0_8:                                ; %if.else9.i
	STP	p1, [p7]                        ; 4-byte Folded Spill
	ADDI	r6, r8, 26
	MOVI	r7, 0
	MOVP	p1, p5
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p5, p3
	JAL	__ashldi3
	MOVP	p3, p5
	OR	r13, r3, r5
	OR	r12, r2, r4
	MOV	r10, r7
	MOV	r11, r7
	CMP	r12, r10
	CPC	r13, r11
	BEQ	.LBB0_10
; %bb.9:
	MOVI	r10, 1
	MOV	r11, r7
.LBB0_10:                               ; %if.else9.i
	MOVI	r2, 38
	SUB	r6, r2, r8
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	JAL	__lshrdi3
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	OR	r11, r3, r5
	OR	r10, r2, r4
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p1, [p7]                        ; 4-byte Folded Reload
.LBB0_11:                               ; %if.end17.i
	SHLI	r6, r11, 14
	SHRI	r7, r10, 2
	OR	r6, r7, r6
	SHRI	r7, r11, 2
	MOVP	p2, p5
	ANDI	r11, r7, 0
	ANDI	r10, r6, 1
	OR	r7, r5, r11
	OR	r6, r4, r10
	ADDPI	p3, p3, 1
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r6, r8
	CPC	r7, r9
	BEQ	.LBB0_12
; %bb.13:                               ; %if.end17.i
	MOVP	p5, p4
	BRA	.LBB0_14
.LBB0_12:
	MOVI	r10, 1
	MOV	r11, r8
.LBB0_14:                               ; %if.end17.i
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPP	p2, p2, p5
	ANDI	r11, r7, 1024
	ANDI	r10, r6, 0
	CMP	r10, r8
	CPC	r11, r9
	BNE	.LBB0_16
; %bb.15:
	SHLI	r2, r7, 14
	SHRI	r3, r6, 2
	OR	r10, r3, r2
	SHRI	r11, r7, 2
	SHLI	r9, r4, 14
	OR	r3, r11, r9
	OR	r2, r10, r8
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	BRA	.LBB0_18
.LBB0_16:                               ; %if.then25.i
	MOVI	r2, 64
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r10, r2, r8
	SBC	r11, r3, r9
	SHRI	r2, r6, 3
	SHLI	r3, r7, 13
	OR	r8, r2, r3
	SHRI	r9, r7, 3
	SHLI	r5, r4, 13
	MOVI	r4, 0
	OR	r3, r9, r5
	OR	r2, r8, r4
.LBB0_18:                               ; %if.end33.i
	ANDI	r5, r3, 127
	ANDI	r4, r2, -1
	SHLI	r3, r10, 7
	MOVI	r2, 0
	OR	r7, r3, r5
	OR	r6, r2, r4
	MOVI	r3, 16256
	ADDPP	p1, p3, p1
.LBB0_19:                               ; %__floatXiYf__.exit
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
