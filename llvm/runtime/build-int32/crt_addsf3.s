	.text
	.globl	__addsf3                        ; -- Begin function __addsf3
	.p2align	1
__addsf3:                               ; @__addsf3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -40
	STP	p4, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	ANDI	r9, r3, 32767
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	ANDI	r8, r2, -1
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r7, -32640
	MOVI	r6, 1
	MOVI	r12, 0
	MOV	r13, r7
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	ADDPP	p5, p4, p6
	CMP	r10, r6
	CPC	r11, r7
	BLO	.LBB0_8
; %bb.1:                                ; %entry
	ADDPP	p5, p1, p6
	CMP	r10, r12
	CPC	r11, r13
	BLS	.LBB0_8
.LBB0_2:                                ; %if.end38.i
	CMP	r2, r8
	CPC	r3, r9
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p3, p2
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	BHI	.LBB0_4
; %bb.3:                                ; %if.end38.i
	MOVP	p3, p6
.LBB0_4:                                ; %if.end38.i
	CMP	r2, r8
	CPC	r3, r9
	MOVP	p1, p6
	BHI	.LBB0_6
; %bb.5:                                ; %if.end38.i
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
.LBB0_6:                                ; %if.end38.i
	ANDI	r4, r2, -1
	ANDI	r5, r3, 127
	ANDI	r10, r6, -1
	ANDI	r11, r7, 127
	SHRI	r8, r3, 7
	MOVI	r9, 0
	ANDI	r3, r9, 0
	ANDI	r2, r8, 255
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	SHRI	r2, r7, 7
	MOV	r3, r9
	ANDI	r7, r3, 0
	ANDI	r6, r2, 255
	MOV	r2, r9
	MOV	r3, r9
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_12
; %bb.7:
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB0_13
.LBB0_8:                                ; %if.then.i
	MOVI	r7, 32640
	CMP	r8, r6
	CPC	r9, r7
	BLO	.LBB0_10
; %bb.9:                                ; %if.then6.i
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	ORI	r5, r3, 64
	ORI	r4, r2, 0
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
.LBB0_10:                               ; %if.end.i
	MOVI	r6, 1
	CMP	r2, r6
	CPC	r3, r7
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	BLO	.LBB0_28
; %bb.11:                               ; %if.then10.i
	ORI	r3, r5, 64
	ORI	r2, r4, 0
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
.LBB0_12:                               ; %if.then48.i
	MOVP	p6, p5
	MOVP	p5, p1
	SHLI	r2, r13, 15
	SHRI	r3, r12, 1
	STP	p2, [p7]                        ; 4-byte Folded Spill
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
	MOV	r8, r7
	OR	r5, r7, r9
	OR	r4, r6, r8
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
	MOV	r4, r9
	MOV	r5, r2
	ADDPP	p1, p1, p2
	SHRI	r8, r3, 8
	ADDI	r4, r8, -8
	MOVP	p1, p6
	JAL	__ashlsi3
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r2, 9
	MOV	r3, r9
	SUB	r2, r2, r8
	SBC	r3, r3, r9
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p1, p5
.LBB0_13:                               ; %if.end50.i
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB0_15
; %bb.14:                               ; %if.then52.i
	SHLI	r2, r5, 15
	SHRI	r3, r4, 1
	MOVP	p6, p2
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
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p2, p1
	MOVI	r2, 9
	MOV	r3, r9
	SUB	r6, r2, r8
	SBC	r7, r3, r9
.LBB0_15:                               ; %if.end54.i
	SHLI	r2, r5, 3
	SHRI	r3, r4, 13
	MOVP	p4, p2
	OR	r5, r2, r3
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	SHLI	r2, r11, 3
	SHRI	r3, r10, 13
	OR	r3, r2, r3
	SHLI	r2, r10, 3
	SHLI	r4, r8, 3
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	XOR	r11, r9, r13
	XOR	r10, r8, r12
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	ORI	r9, r5, 1024
	ORI	r8, r4, 0
	ORI	r5, r3, 1024
	ORI	r4, r2, 0
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	ANDI	r13, r3, -32768
	ANDI	r12, r2, 0
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BNE	.LBB0_17
; %bb.16:
	MOVP	p5, p4
	BRA	.LBB0_21
.LBB0_17:                               ; %if.then64.i
	SUB	r6, r4, r6
	SBC	r7, r5, r7
	MOVI	r11, 0
	MOVI	r10, 1
	MOVI	r2, 31
	MOV	r3, r11
	CMP	r6, r2
	CPC	r7, r3
	BHI	.LBB0_21
; %bb.18:                               ; %if.then66.i
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r2, 32
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	SUB	r4, r2, r6
	MOVP	p1, p4
	JAL	__ashlsi3
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB0_20
; %bb.19:                               ; %if.then66.i
	MOVP	p5, p2
.LBB0_20:                               ; %if.then66.i
	MOVP	p1, p4
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__lshrsi3
	OR	r11, r3, r11
	OR	r10, r2, r10
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
.LBB0_21:                               ; %if.end75.i
	MOVI	r2, -1
	MOV	r3, r2
	LDP	p3, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	BLE	.LBB0_24
; %bb.22:                               ; %if.else93.i
	MOVP	p3, p2
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPP	p4, p5, p1
	ANDI	r3, r9, 2048
	ANDI	r2, r8, 0
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB0_27
; %bb.23:
	MOVP	p5, p4
	BRA	.LBB0_34
.LBB0_24:                               ; %if.then77.i
	MOVI	r2, 0
	MOV	r3, r2
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB0_60
; %bb.25:                               ; %if.end83.i
	MOVP	p3, p2
	SUB	r10, r2, r10
	SBC	r11, r3, r11
	SHRI	r2, r11, 10
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	MOV	r3, r4
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB0_34
; %bb.26:                               ; %if.end107.thread.i
	SHLI	r2, r11, 15
	SHRI	r3, r10, 1
	OR	r4, r3, r2
	SHRI	r5, r11, 1
	OR	r3, r11, r5
	OR	r2, r10, r4
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
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
	NOT	r5, r7
	NOT	r4, r6
	SHRI	r6, r4, 1
	SHLI	r7, r5, 15
	OR	r6, r6, r7
	SHRI	r7, r5, 1
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	ANDI	r13, r7, 21845
	ANDI	r12, r6, 21845
	SUB	r4, r4, r12
	SBC	r5, r5, r13
	SHLI	r6, r5, 14
	SHRI	r7, r4, 2
	OR	r6, r7, r6
	SHRI	r7, r5, 2
	ANDI	r13, r7, 13107
	ANDI	r12, r6, 13107
	ANDI	r7, r5, 13107
	ANDI	r6, r4, 13107
	ADDPP	p2, p3, p6
	SHRI	r6, r4, 4
	SHLI	r7, r5, 12
	OR	r6, r6, r7
	SHRI	r7, r5, 4
	ADDPP	p3, p2, p3
	ANDI	r5, r7, 3855
	ANDI	r4, r6, 3855
	SHLI	r6, r5, 8
	SHRI	r7, r4, 8
	OR	r7, r6, r7
	SHLI	r6, r4, 8
	ADDPP	p2, p2, p3
	MOV	r6, r3
	MOV	r7, r4
	ADDPP	p2, p2, p3
	SHRI	r2, r5, 8
	ADDPI	p4, p1, -5
	MOVP	p1, p5
	MOV	r4, r8
	JAL	__ashlsi3
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	MOVP	p5, p1
	SUB	r4, r4, r8
	SBC	r5, r5, r9
	BRA	.LBB0_37
.LBB0_34:
	MOVP	p2, p3
	BRA	.LBB0_35
.LBB0_27:                               ; %if.then96.i
	SHRI	r2, r8, 1
	SHLI	r3, r9, 15
	OR	r2, r2, r3
	SHRI	r3, r9, 1
	ANDI	r5, r11, 0
	ANDI	r4, r10, 1
	OR	r11, r3, r5
	OR	r10, r2, r4
	MOVP	p2, p3
	ADDPI	p2, p2, 1
.LBB0_35:                               ; %if.end107.i
	MOVI	r3, 0
	MOVI	r2, 255
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB0_37
; %bb.36:                               ; %if.then110.i
	ORI	r3, r13, 32640
	ORI	r2, r12, 0
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
.LBB0_37:                               ; %if.end113.i
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r4, r8
	CPC	r5, r9
	BGT	.LBB0_41
; %bb.38:                               ; %if.then116.i
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	ADDI	r4, r4, 31
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	JAL	__ashlsi3
	MOVI	r4, 1
	MOV	r5, r8
	CMP	r2, r8
	CPC	r3, r9
	MOVP	p5, p2
	BNE	.LBB0_40
; %bb.39:                               ; %if.then116.i
	MOVP	p5, p4
.LBB0_40:                               ; %if.then116.i
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	SUB	r4, r4, r2
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__lshrsi3
	OR	r5, r3, r11
	OR	r4, r2, r10
	MOVP	p5, p2
	MOVP	p2, p4
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
.LBB0_41:                               ; %if.end129.i
	SHLI	r2, r11, 13
	SHRI	r3, r10, 3
	OR	r6, r3, r2
	SHRI	r7, r11, 3
	ANDI	r3, r7, 127
	STP	p3, [p7 + 16]                   ; 4-byte Folded Spill
	ANDI	r2, r6, -1
	ANDI	r7, r11, 0
	ANDI	r6, r10, 7
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	SHLI	r9, r4, 7
	MOVI	r10, 2
	MOV	r11, r8
	OR	r5, r3, r9
	OR	r4, r2, r8
	OR	r3, r5, r13
	MOVP	p4, p6
	OR	r2, r4, r12
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	JAL	__fe_getround
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB0_51
; %bb.42:                               ; %if.end129.i
	MOVI	r5, 0
	MOVI	r4, 1
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB0_46
; %bb.43:                               ; %if.end129.i
	MOV	r4, r5
	CMP	r2, r4
	CPC	r3, r5
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	BNE	.LBB0_58
; %bb.44:                               ; %sw.bb.i
	MOVI	r3, 0
	MOVI	r2, 4
	CMP	r12, r2
	CPC	r13, r3
	BHI	.LBB0_62
; %bb.45:                               ; %sw.bb.i
	MOV	r4, r3
	BRA	.LBB0_63
.LBB0_28:                               ; %if.end14.i
	MOVI	r7, 32640
	MOVI	r6, 0
	CMP	r8, r6
	CPC	r9, r7
	BNE	.LBB0_31
; %bb.29:                               ; %if.then16.i
	XOR	r3, r5, r13
	XOR	r2, r4, r12
	MOVI	r5, -32768
	MOV	r4, r6
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB0_61
; %bb.30:                               ; %if.then16.i
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
.LBB0_31:                               ; %if.end22.i
	CMP	r2, r6
	CPC	r3, r7
	BEQ	.LBB0_60
; %bb.32:                               ; %if.end25.i
	MOVI	r6, 0
	MOV	r7, r6
	CMP	r8, r6
	CPC	r9, r7
	BEQ	.LBB0_66
; %bb.33:                               ; %if.end34.i
	CMP	r2, r6
	CPC	r3, r7
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	BNE	.LBB0_2
	BRA	.LBB0_60
.LBB0_46:                               ; %sw.bb147.i
	MOV	r6, r5
	MOV	r7, r5
	CMP	r8, r6
	CPC	r9, r7
	MOVP	p1, p2
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	BNE	.LBB0_48
; %bb.47:                               ; %sw.bb147.i
	MOVP	p1, p3
.LBB0_48:                               ; %sw.bb147.i
	CMP	r12, r6
	CPC	r13, r7
	BNE	.LBB0_50
; %bb.49:                               ; %sw.bb147.i
	MOVP	p2, p3
.LBB0_50:                               ; %sw.bb147.i
	AND	r7, r3, r5
	AND	r6, r2, r4
	BRA	.LBB0_56
.LBB0_51:                               ; %sw.bb153.i
	MOVI	r3, 0
	MOVI	r2, 1
	MOV	r6, r3
	MOV	r7, r3
	CMP	r8, r6
	CPC	r9, r7
	MOVP	p2, p1
	BEQ	.LBB0_53
; %bb.52:                               ; %sw.bb153.i
	MOVP	p2, p3
.LBB0_53:                               ; %sw.bb153.i
	LDP	p6, [p7 + 24]                   ; 4-byte Folded Reload
	CMP	r12, r6
	CPC	r13, r7
	BNE	.LBB0_55
; %bb.54:                               ; %sw.bb153.i
	MOVP	p1, p3
.LBB0_55:                               ; %sw.bb153.i
	AND	r7, r5, r3
	AND	r6, r4, r2
.LBB0_56:                               ; %sw.epilog.i
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p3
.LBB0_57:                               ; %sw.epilog.i
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
.LBB0_58:                               ; %sw.epilog.i
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB0_60
.LBB0_59:                               ; %if.then161.i
	JAL	__fe_raise_inexact
.LBB0_60:                               ; %__addXf3__.exit
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 32]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 40
	RET
.LBB0_61:
	MOVI	r7, 32704
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
.LBB0_62:
	MOVI	r4, 1
.LBB0_63:                               ; %sw.bb.i
	MOV	r5, r3
	CMP	r12, r2
	CPC	r13, r3
	BNE	.LBB0_65
; %bb.64:                               ; %sw.epilog.thread.i
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	ANDI	r3, r5, 0
	ANDI	r2, r4, 1
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPP	p2, p2, p1
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_59
.LBB0_65:
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPP	p1, p1, p2
	BRA	.LBB0_57
.LBB0_66:                               ; %if.then26.i
	CMP	r2, r6
	CPC	r3, r7
	BNE	.LBB0_60
; %bb.67:                               ; %if.then28.i
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	AND	r7, r5, r3
	AND	r6, r4, r2
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_60
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
