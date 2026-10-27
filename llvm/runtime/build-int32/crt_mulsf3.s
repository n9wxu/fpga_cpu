	.text
	.globl	__mulsf3                        ; -- Begin function __mulsf3
	.p2align	1
__mulsf3:                               ; @__mulsf3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -44
	STP	p4, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 32]                   ; 4-byte Folded Spill
	XOR	r7, r5, r3
	XOR	r6, r4, r2
	ANDI	r9, r7, -32768
	ANDI	r8, r6, 0
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	SHRI	r6, r3, 7
	MOVI	r12, 0
	MOV	r7, r12
	ANDI	r11, r7, 0
	ANDI	r10, r6, 255
	SHRI	r6, r5, 7
	MOV	r7, r12
	ANDI	r9, r7, 0
	ANDI	r8, r6, 255
	ANDI	r7, r5, 127
	ANDI	r6, r4, -1
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	ANDI	r7, r3, 127
	ANDI	r6, r2, -1
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	MOVI	r7, -1
	MOVI	r6, -254
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	ADDPI	p5, p5, -255
	CMP	r10, r6
	CPC	r11, r7
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	BLO	.LBB0_5
; %bb.1:                                ; %entry
	MOV	r13, r12
	ADDPI	p5, p4, -255
	MOVI	r6, -255
	CMP	r10, r6
	CPC	r11, r7
	BLS	.LBB0_5
; %bb.2:
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p4, p6
.LBB0_3:                                ; %if.end61.i
	SHLI	r2, r11, 8
	SHRI	r3, r10, 8
	OR	r5, r2, r3
	SHLI	r4, r10, 8
	MOVI	r10, 0
	MOV	r11, r10
	STP	p5, [p7]
	ORI	r3, r5, -32768
	ORI	r2, r4, 0
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	ORI	r7, r5, 128
	ORI	r6, r4, 0
	MOVP	p2, p5
	JAL	__muldi3
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPP	p3, p3, p6
	ADDPP	p3, p3, p4
	ANDI	r13, r5, 128
	ANDI	r12, r4, 0
	ADDPI	p3, p3, -127
	CMP	r12, r10
	CPC	r13, r11
	BNE	.LBB0_7
; %bb.4:                                ; %if.else70.i
	SHLI	r12, r5, 1
	SHRI	r13, r4, 15
	OR	r13, r12, r13
	SHLI	r12, r4, 1
	SHRI	r8, r3, 15
	MOV	r9, r10
	OR	r5, r13, r9
	OR	r4, r12, r8
	SHLI	r12, r3, 1
	SHRI	r13, r2, 15
	OR	r3, r12, r13
	SHLI	r2, r2, 1
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
.LBB0_29:                               ; %__mulXf3__.exit
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BRA	.LBB0_30
.LBB0_7:                                ; %if.then69.i
	ADDPI	p3, p3, 1
.LBB0_8:                                ; %if.end71.i
	MOVI	r9, 0
	MOVI	r8, 255
	CMP	r6, r8
	CPC	r7, r9
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BLT	.LBB0_10
.LBB0_9:                                ; %if.then29.i
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	ORI	r3, r5, 32640
	ORI	r2, r4, 0
	BRA	.LBB0_30
.LBB0_10:                               ; %if.end76.i
	MOV	r8, r9
	CMP	r6, r8
	CPC	r7, r9
	BLE	.LBB0_17
; %bb.11:                               ; %if.else87.i
	SHLI	r7, r6, 7
	MOVI	r6, 0
	ANDI	r9, r5, 127
	ANDI	r8, r4, -1
	OR	r5, r7, r9
	OR	r4, r6, r8
	BRA	.LBB0_21
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
	BRA	.LBB0_30
.LBB0_14:                               ; %if.end26.i
	MOVI	r3, 32640
	MOVI	r2, 0
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB0_25
; %bb.15:                               ; %if.then28.i
	MOV	r4, r2
	MOV	r5, r2
	CMP	r10, r4
	CPC	r11, r5
	BNE	.LBB0_9
	BRA	.LBB0_16
.LBB0_17:                               ; %if.then78.i
	MOVI	r13, 0
	MOVI	r12, 1
	SUB	r8, r12, r6
	SBC	r9, r13, r7
	MOVI	r10, 31
	STP	p6, [p7 + 16]                   ; 4-byte Folded Spill
	MOV	r11, r13
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	CMP	r8, r10
	CPC	r9, r11
	BHI	.LBB0_28
; %bb.18:                               ; %cleanup84.thread.i
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	ADDI	r10, r6, 31
	STP	p1, [p7 + 12]                   ; 4-byte Folded Spill
	MOV	r4, r10
	JAL	__ashlsi3
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	MOV	r4, r9
	MOV	r5, r9
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB0_20
; %bb.19:                               ; %cleanup84.thread.i
	MOVP	p4, p2
.LBB0_20:                               ; %cleanup84.thread.i
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	MOV	r4, r10
	JAL	__ashlsi3
	MOVP	p5, p1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__lshrsi3
	OR	r5, r11, r3
	OR	r4, r10, r2
	OR	r11, r5, r9
	OR	r10, r4, r8
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__lshrsi3
	MOVP	p2, p1
	MOVP	p1, p5
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
.LBB0_21:                               ; %if.end91.i
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	OR	r7, r5, r9
	OR	r6, r4, r8
	MOVI	r9, -32768
	MOVI	r8, 1
	CMP	r2, r8
	CPC	r3, r9
	BLO	.LBB0_23
; %bb.22:                               ; %if.end96.thread.i
	MOVP	p1, p3
	ADDPI	p1, p1, 1
	BRA	.LBB0_30
.LBB0_23:                               ; %if.end96.i
	MOVI	r8, 0
	CMP	r2, r8
	CPC	r3, r9
	MOVP	p1, p3
	BNE	.LBB0_30
; %bb.24:                               ; %if.then98.i
	ANDI	r7, r5, 0
	ANDI	r6, r4, 1
	ADDPP	p1, p1, p3
	BRA	.LBB0_30
.LBB0_25:                               ; %if.end33.i
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_31
; %bb.26:                               ; %if.then35.i
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB0_9
.LBB0_16:
	MOVI	r3, 32704
	BRA	.LBB0_30
.LBB0_28:                               ; %cleanup84.i
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	BRA	.LBB0_29
.LBB0_31:                               ; %if.end42.i
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_36
; %bb.32:                               ; %if.end46.i
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB0_36
; %bb.33:                               ; %if.end50.i
	SHRI	r4, r7, 7
	MOVI	r5, 0
	MOV	r2, r5
	MOV	r3, r5
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB0_37
; %bb.34:                               ; %if.then52.i
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
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
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r3, r5
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
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r4, r5
	MOV	r5, r2
	ADDPP	p1, p1, p2
	SHRI	r8, r3, 8
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r9, r3
	ADDI	r4, r8, -8
	MOVP	p1, p6
	JAL	__ashlsi3
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	MOVI	r2, 9
	MOV	r3, r5
	SUB	r8, r2, r8
	SBC	r9, r3, r9
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	BRA	.LBB0_38
.LBB0_36:                               ; %if.then48.i
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
.LBB0_30:                               ; %__mulXf3__.exit
	LDP	p4, [p7 + 40]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 36]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 44
	RET
.LBB0_37:
	MOVP	p4, p1
.LBB0_38:                               ; %if.end54.i
	SHRI	r4, r11, 7
	CMP	r4, r2
	CPC	r5, r3
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	BNE	.LBB0_3
; %bb.39:                               ; %if.then56.i
	SHLI	r2, r11, 15
	SHRI	r3, r10, 1
	OR	r4, r3, r2
	SHRI	r5, r11, 1
	OR	r3, r11, r5
	OR	r2, r10, r4
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
	OR	r6, r5, r4
	SHRI	r7, r3, 8
	OR	r5, r3, r7
	OR	r4, r2, r6
	MOVI	r3, 0
	MOV	r2, r5
	OR	r7, r5, r3
	OR	r6, r4, r2
	MOVP	p6, p1
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
	MOV	r4, r13
	MOV	r5, r2
	ADDPP	p1, p1, p2
	SHRI	r12, r3, 8
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	ADDI	r4, r12, -8
	MOVP	p1, p5
	JAL	__ashlsi3
	MOVP	p5, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r2, r8, r2
	SBC	r3, r9, r3
	ADDPI	p4, p1, 9
	BRA	.LBB0_3
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
