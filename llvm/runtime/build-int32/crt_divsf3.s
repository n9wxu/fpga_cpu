	.text
	.globl	__divsf3                        ; -- Begin function __divsf3
	.p2align	1
__divsf3:                               ; @__divsf3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -48
	STP	p4, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 32]                   ; 4-byte Folded Spill
	XOR	r7, r5, r3
	XOR	r6, r4, r2
	ANDI	r9, r7, -32768
	ANDI	r8, r6, 0
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	SHRI	r6, r3, 7
	MOVI	r12, 0
	MOV	r7, r12
	ANDI	r9, r7, 0
	ANDI	r8, r6, 255
	SHRI	r6, r5, 7
	MOV	r7, r12
	ANDI	r11, r7, 0
	ANDI	r10, r6, 255
	ANDI	r7, r5, 127
	ANDI	r6, r4, -1
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	ANDI	r7, r3, 127
	ANDI	r6, r2, -1
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	MOVI	r7, -1
	MOVI	r6, -254
	STP	p4, [p7 + 20]                   ; 4-byte Folded Spill
	ADDPI	p4, p4, -255
	CMP	r8, r6
	CPC	r9, r7
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	BLO	.LBB0_5
; %bb.1:                                ; %entry
	MOV	r13, r12
	ADDPI	p4, p5, -255
	MOVI	r6, -255
	CMP	r8, r6
	CPC	r9, r7
	BLS	.LBB0_5
; %bb.2:
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
.LBB0_3:                                ; %if.end61.i
	ORI	r5, r13, 128
	ORI	r4, r12, 0
	MOVI	r10, 0
	MOV	r11, r10
	STP	p5, [p7]
	SHLI	r2, r5, 8
	SHRI	r3, r4, 8
	OR	r7, r2, r3
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	SHLI	r6, r4, 8
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	MOVI	r3, 29956
	MOVI	r2, -3277
	SUB	r8, r2, r6
	SBC	r9, r3, r7
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__muldi3
	STP	p5, [p7]
	SUB	r2, r10, r4
	SBC	r3, r11, r5
	MOVP	p2, p5
	MOVP	p3, p4
	JAL	__muldi3
	STP	p5, [p7]
	SHLI	r6, r5, 1
	SHRI	r7, r4, 15
	OR	r7, r6, r7
	SHLI	r6, r4, 1
	SHRI	r2, r3, 15
	MOV	r3, r10
	OR	r9, r3, r7
	OR	r8, r2, r6
	MOVP	p1, p4
	MOVP	p2, p5
	LDP	p3, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__muldi3
	STP	p5, [p7]
	SUB	r6, r10, r4
	SBC	r7, r11, r5
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__muldi3
	STP	p5, [p7]
	SHLI	r6, r5, 1
	SHRI	r7, r4, 15
	OR	r7, r6, r7
	SHLI	r6, r4, 1
	SHRI	r2, r3, 15
	MOV	r3, r10
	OR	r9, r3, r7
	OR	r8, r2, r6
	MOVP	p1, p4
	MOVP	p2, p5
	LDP	p3, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__muldi3
	STP	p5, [p7]
	SUB	r6, r10, r4
	SBC	r7, r11, r5
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__muldi3
	STP	p5, [p7]
	LDP	p3, [p7 + 36]                   ; 4-byte Folded Reload
	ORI	r9, r7, 128
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	ORI	r8, r6, 0
	SHLI	r6, r9, 1
	SHRI	r7, r8, 15
	OR	r7, r6, r7
	SHLI	r6, r8, 1
	SHLI	r12, r5, 1
	SHRI	r13, r4, 15
	OR	r13, r12, r13
	SHLI	r12, r4, 1
	SHRI	r2, r3, 15
	MOV	r3, r10
	OR	r5, r3, r13
	OR	r4, r2, r12
	ADDPI	p1, p2, -12
	MOVP	p2, p5
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	JAL	__muldi3
	MOVP	p3, p2
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	SUB	r2, r2, r4
	SBC	r3, r3, r5
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPP	p1, p2, p1
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	SHRI	r2, r7, 8
	MOV	r3, r10
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB0_7
; %bb.4:                                ; %if.then106.i
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	SHLI	r11, r2, 8
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	MOVP	p2, p3
	JAL	__mulsi3
	SUB	r2, r10, r2
	SBC	r3, r11, r3
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p5, p2, 126
	BRA	.LBB0_8
.LBB0_5:                                ; %if.then.i
	ANDI	r7, r3, 32767
	ANDI	r6, r2, -1
	MOVI	r9, 32640
	MOVI	r8, 1
	CMP	r6, r8
	CPC	r7, r9
	BLO	.LBB0_12
; %bb.6:                                ; %if.then18.i
	MOVP	p2, p1
	ORI	r3, r5, 64
	ORI	r2, r4, 0
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BRA	.LBB0_26
.LBB0_7:                                ; %if.else112.i
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	SHLI	r11, r2, 7
	SHRI	r2, r6, 1
	SHLI	r3, r7, 15
	OR	r6, r2, r3
	SHRI	r7, r7, 1
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	MOVP	p1, p3
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	SUB	r2, r10, r2
	SBC	r3, r11, r3
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p5, p2, 127
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
.LBB0_8:                                ; %if.end117.i
	MOVI	r5, 0
	MOVI	r4, 255
	CMP	r10, r4
	CPC	r11, r5
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BLT	.LBB0_10
.LBB0_9:                                ; %if.else.i
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	ORI	r3, r5, 32640
	ORI	r2, r4, 0
	BRA	.LBB0_26
.LBB0_10:                               ; %if.end123.i
	MOVI	r4, 1
	CMP	r10, r4
	CPC	r11, r5
	BLT	.LBB0_17
; %bb.11:                               ; %if.then126.i
	SHLI	r5, r10, 7
	MOVI	r4, 0
	LDP	p4, [p7 + 36]                   ; 4-byte Folded Reload
	ANDI	r7, r9, 127
	ANDI	r6, r8, -1
	OR	r9, r5, r7
	OR	r8, r4, r6
	SHLI	r4, r3, 1
	SHRI	r5, r2, 15
	OR	r5, r4, r5
	SHLI	r4, r2, 1
	BRA	.LBB0_22
.LBB0_12:                               ; %if.end.i
	ANDI	r11, r5, 32767
	ANDI	r10, r4, -1
	CMP	r10, r8
	CPC	r11, r9
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BLO	.LBB0_14
; %bb.13:                               ; %if.then22.i
	ORI	r3, r5, 64
	ORI	r2, r4, 0
	BRA	.LBB0_26
.LBB0_14:                               ; %if.end26.i
	MOVI	r3, 32640
	MOVI	r2, 0
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB0_19
; %bb.15:                               ; %if.then28.i
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_9
.LBB0_16:
	MOVI	r3, 32704
	BRA	.LBB0_26
.LBB0_17:                               ; %if.else131.i
	MOVI	r3, -1
	MOVI	r2, -24
	CMP	r10, r2
	CPC	r11, r3
	BLE	.LBB0_20
; %bb.21:                               ; %if.end137.i
	MOVI	r2, 1
	SUB	r4, r2, r10
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	JAL	__lshrsi3
	MOVP	p4, p1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__mulsi3
	SHLI	r4, r3, 1
	SHRI	r5, r2, 15
	OR	r5, r4, r5
	SHLI	r4, r2, 1
	STP	p2, [p7 + 36]                   ; 4-byte Folded Spill
	ADDI	r4, r10, 23
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	JAL	__ashlsi3
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	SUB	r4, r2, r4
	SBC	r5, r3, r5
.LBB0_22:                               ; %if.end146.i
	ANDI	r3, r9, 0
	ANDI	r2, r8, 1
	ADDPP	p2, p1, p2
	MOVI	r3, 0
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BHI	.LBB0_24
; %bb.23:                               ; %if.end146.i
	MOV	r2, r3
	BRA	.LBB0_25
.LBB0_24:
	MOVI	r2, 1
.LBB0_25:                               ; %if.end146.i
	ADDPP	p2, p4, p1
	LDP	p3, [p7 + 28]                   ; 4-byte Folded Reload
	OR	r3, r5, r7
	OR	r2, r4, r6
	BRA	.LBB0_26
.LBB0_19:                               ; %if.end34.i
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_27
.LBB0_20:                               ; %if.then36.i
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
.LBB0_26:                               ; %__divXf3__.exit
	LDP	p4, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 40]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 48
	RET
.LBB0_27:                               ; %if.end38.i
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_31
; %bb.28:                               ; %if.end45.i
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB0_9
; %bb.29:                               ; %if.end50.i
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	SHRI	r10, r7, 7
	MOVI	r11, 0
	MOV	r4, r11
	MOV	r5, r11
	CMP	r10, r4
	CPC	r11, r5
	BNE	.LBB0_33
; %bb.30:                               ; %if.then52.i
	LDP	p6, [p7 + 36]                   ; 4-byte Folded Reload
	SHLI	r2, r13, 15
	SHRI	r3, r12, 1
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	OR	r4, r3, r2
	SHRI	r5, r13, 1
	OR	r3, r13, r5
	OR	r2, r12, r4
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r6, r5, r4
	SHRI	r7, r3, 2
	OR	r5, r3, r7
	OR	r4, r2, r6
	SHLI	r2, r5, 12
	SHRI	r3, r4, 4
	OR	r6, r3, r2
	SHRI	r7, r5, 4
	OR	r3, r5, r7
	OR	r2, r4, r6
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r4, r5, r4
	SHRI	r5, r3, 8
	OR	r7, r3, r5
	OR	r6, r2, r4
	MOV	r2, r7
	MOV	r3, r11
	OR	r5, r7, r3
	OR	r4, r6, r2
	NOT	r3, r5
	NOT	r2, r4
	SHRI	r4, r2, 1
	SHLI	r5, r3, 15
	OR	r4, r4, r5
	SHRI	r5, r3, 1
	ANDI	r7, r5, 21845
	ANDI	r6, r4, 21845
	SUB	r2, r2, r6
	SBC	r3, r3, r7
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r4, r5, r4
	SHRI	r5, r3, 2
	ANDI	r7, r5, 13107
	ANDI	r6, r4, 13107
	ANDI	r5, r3, 13107
	ANDI	r4, r2, 13107
	ADDPP	p1, p2, p3
	SHRI	r4, r2, 4
	SHLI	r5, r3, 12
	OR	r4, r4, r5
	SHRI	r5, r3, 4
	ADDPP	p2, p1, p2
	ANDI	r3, r5, 3855
	ANDI	r2, r4, 3855
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r5, r4, r5
	SHLI	r4, r2, 8
	ADDPP	p1, p1, p2
	MOV	r4, r11
	MOV	r5, r2
	ADDPP	p1, p1, p2
	SHRI	r8, r3, 8
	MOV	r9, r11
	ADDI	r4, r8, -8
	MOVP	p1, p6
	JAL	__ashlsi3
	MOVI	r4, 9
	MOV	r5, r11
	SUB	r6, r4, r8
	SBC	r7, r5, r9
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB0_34
.LBB0_31:                               ; %if.then39.i
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB0_16
	BRA	.LBB0_20
.LBB0_33:
	MOVP	p3, p2
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
.LBB0_34:                               ; %if.end54.i
	STP	p1, [p7 + 36]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	SHRI	r10, r3, 7
	CMP	r10, r4
	CPC	r11, r5
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	BNE	.LBB0_36
; %bb.35:                               ; %if.then56.i
	SHLI	r2, r13, 15
	SHRI	r3, r12, 1
	OR	r4, r3, r2
	SHRI	r5, r13, 1
	OR	r3, r13, r5
	OR	r2, r12, r4
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	MOVP	p5, p3
	OR	r6, r5, r4
	SHRI	r7, r3, 2
	OR	r5, r3, r7
	OR	r4, r2, r6
	SHLI	r2, r5, 12
	SHRI	r3, r4, 4
	OR	r6, r3, r2
	SHRI	r7, r5, 4
	OR	r3, r5, r7
	OR	r2, r4, r6
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r6, r5, r4
	SHRI	r7, r3, 8
	OR	r5, r3, r7
	OR	r4, r2, r6
	MOVI	r9, 0
	MOV	r8, r5
	OR	r7, r5, r9
	OR	r6, r4, r8
	NOT	r3, r7
	NOT	r2, r6
	SHRI	r4, r2, 1
	SHLI	r5, r3, 15
	OR	r4, r4, r5
	SHRI	r5, r3, 1
	ANDI	r7, r5, 21845
	ANDI	r6, r4, 21845
	SUB	r2, r2, r6
	SBC	r3, r3, r7
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r4, r5, r4
	SHRI	r5, r3, 2
	ANDI	r7, r5, 13107
	ANDI	r6, r4, 13107
	ANDI	r5, r3, 13107
	ANDI	r4, r2, 13107
	ADDPP	p1, p2, p3
	SHRI	r4, r2, 4
	SHLI	r5, r3, 12
	OR	r4, r4, r5
	SHRI	r5, r3, 4
	ADDPP	p2, p1, p2
	ANDI	r3, r5, 3855
	ANDI	r2, r4, 3855
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r5, r4, r5
	SHLI	r4, r2, 8
	ADDPP	p1, p1, p2
	MOV	r4, r9
	MOV	r5, r2
	ADDPP	p1, p1, p2
	SHRI	r8, r3, 8
	ADDI	r4, r8, -8
	MOVP	p1, p6
	JAL	__ashlsi3
	MOVP	p6, p1
	ADDPP	p1, p4, p5
	ADDPI	p1, p1, -9
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB0_3
.LBB0_36:
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB0_3
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
