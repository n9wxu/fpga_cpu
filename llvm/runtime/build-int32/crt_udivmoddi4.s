	.text
	.section	.rodata.cst32,"aM",@progbits,32
.LCPI0_0:                               ; -- Begin function __udivmoddi4
	.ascii	"\000\001\034\002\035\016\030\003\036\026\024\017\031\021\004\b\037\033\r\027\025\023\020\007\032\f\022\006\013\005\n\t"
	.text
	.globl	__udivmoddi4
	.p2align	1
__udivmoddi4:                           ; @__udivmoddi4
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -64
	STP	p4, [p7 + 60]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 56]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p5, [p7 + 64]
	LDP	p4, [p7 + 68]
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB0_1
; %bb.8:                                ; %if.end16
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	CMP	r6, r2
	CPC	r7, r3
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	BEQ	.LBB0_9
; %bb.22:                               ; %if.else
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVI	r12, 0
	MOV	r13, r12
	CMP	r10, r12
	CPC	r11, r13
	STP	p2, [p7 + 48]                   ; 4-byte Folded Spill
	BEQ	.LBB0_23
; %bb.34:                               ; %if.else183
	SHLI	r2, r5, 15
	SHRI	r3, r4, 1
	OR	r2, r3, r2
	SHRI	r3, r5, 1
	STP	p6, [p7 + 44]                   ; 4-byte Folded Spill
	OR	r13, r5, r3
	OR	r12, r4, r2
	SHLI	r2, r11, 15
	SHRI	r3, r10, 1
	OR	r6, r3, r2
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	OR	r8, r3, r2
	SHRI	r7, r11, 1
	SHRI	r9, r13, 2
	OR	r3, r11, r7
	OR	r2, r10, r6
	OR	r7, r13, r9
	OR	r6, r12, r8
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r8, r5, r4
	SHLI	r4, r7, 12
	SHRI	r5, r6, 4
	OR	r4, r5, r4
	SHRI	r9, r3, 2
	SHRI	r5, r7, 4
	OR	r13, r3, r9
	OR	r12, r2, r8
	OR	r11, r7, r5
	OR	r10, r6, r4
	SHLI	r2, r13, 12
	SHRI	r3, r12, 4
	OR	r6, r3, r2
	SHLI	r2, r11, 8
	SHRI	r3, r10, 8
	OR	r4, r3, r2
	SHRI	r7, r13, 4
	SHRI	r5, r11, 8
	OR	r3, r13, r7
	OR	r2, r12, r6
	OR	r7, r11, r5
	OR	r6, r10, r4
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r8, r5, r4
	SHRI	r9, r3, 8
	MOV	r4, r7
	LDP	p6, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r5, r12
	OR	r11, r7, r5
	OR	r10, r6, r4
	OR	r13, r3, r9
	OR	r12, r2, r8
	NOT	r7, r11
	NOT	r6, r10
	MOV	r2, r13
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r3, r4
	OR	r9, r13, r3
	OR	r8, r12, r2
	SHRI	r2, r6, 1
	SHLI	r3, r7, 15
	OR	r12, r2, r3
	SHRI	r13, r7, 1
	NOT	r3, r9
	NOT	r2, r8
	ANDI	r5, r13, 21845
	ANDI	r4, r12, 21845
	SUB	r10, r6, r4
	SBC	r11, r7, r5
	SHRI	r4, r2, 1
	SHLI	r5, r3, 15
	OR	r12, r4, r5
	SHRI	r13, r3, 1
	SHLI	r4, r11, 14
	SHRI	r5, r10, 2
	OR	r8, r5, r4
	ANDI	r6, r10, 13107
	ANDI	r7, r11, 13107
	SHRI	r9, r11, 2
	ANDI	r5, r13, 21845
	ANDI	r4, r12, 21845
	SUB	r10, r2, r4
	SBC	r11, r3, r5
	ANDI	r3, r9, 13107
	ANDI	r2, r8, 13107
	ADDPP	p3, p3, p1
	SHLI	r2, r11, 14
	SHRI	r3, r10, 2
	OR	r12, r3, r2
	ANDI	r2, r10, 13107
	ANDI	r3, r11, 13107
	SHRI	r13, r11, 2
	SHRI	r4, r6, 4
	SHLI	r5, r7, 12
	OR	r4, r4, r5
	SHRI	r5, r7, 4
	ADDPP	p2, p3, p2
	ANDI	r7, r13, 13107
	ANDI	r6, r12, 13107
	ADDPP	p3, p1, p3
	ANDI	r3, r5, 3855
	ANDI	r2, r4, 3855
	SHRI	r4, r6, 4
	SHLI	r5, r7, 12
	OR	r4, r4, r5
	SHRI	r5, r7, 4
	SHLI	r12, r3, 8
	SHRI	r13, r2, 8
	OR	r13, r12, r13
	ADDPP	p3, p3, p2
	SHLI	r12, r2, 8
	ANDI	r5, r7, 3855
	ANDI	r4, r6, 3855
	ADDPP	p1, p1, p6
	SHLI	r6, r5, 8
	SHRI	r7, r4, 8
	OR	r7, r6, r7
	SHLI	r6, r4, 8
	ADDPP	p2, p2, p3
	LDP	p3, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r7, r4
	ADDPP	p2, p2, p3
	MOV	r7, r2
	ADDPP	p1, p1, p3
	MOVI	r12, 32
	MOV	r13, r6
	SHRI	r2, r3, 8
	MOV	r3, r6
	SHRI	r4, r5, 8
	MOV	r5, r6
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	CMP	r2, r12
	CPC	r3, r13
	BLO	.LBB0_35
.LBB0_19:                               ; %if.then78
	MOVI	r6, 0
	MOV	r7, r6
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r6
	CPC	r3, r7
	BEQ	.LBB0_20
; %bb.48:                               ; %if.then80
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	STP	p2, [p1]
	LDP	p2, [p7 + 48]                   ; 4-byte Folded Reload
	STP	p2, [p1 + 4]
.LBB0_20:
	MOVP	p2, p3
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	BRA	.LBB0_47
.LBB0_1:                                ; %if.then
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB0_2
; %bb.5:                                ; %if.end10
	MOVI	r6, 0
	MOV	r7, r6
	CMP	r8, r6
	CPC	r9, r7
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	BEQ	.LBB0_6
; %bb.7:                                ; %if.then12
	STP	p1, [p4]
	STP	p3, [p4 + 4]
	MOVP	p2, p3
	BRA	.LBB0_47
.LBB0_9:                                ; %if.then20
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB0_10
; %bb.11:                               ; %if.end36
	STP	p2, [p7 + 48]                   ; 4-byte Folded Spill
	STP	p4, [p7]                        ; 4-byte Folded Spill
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	BEQ	.LBB0_12
; %bb.14:                               ; %if.end53
	ADDPI	p1, p5, -1
	AND	r5, r11, r3
	AND	r4, r10, r2
	MOVI	r12, 0
	MOV	r13, r12
	CMP	r4, r12
	CPC	r5, r13
	BNE	.LBB0_18
; %bb.15:                               ; %if.then58
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMP	r6, r12
	CPC	r7, r13
	LDP	p4, [p7 + 48]                   ; 4-byte Folded Reload
	BEQ	.LBB0_17
; %bb.16:                               ; %if.then60
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	STP	p2, [p3]
	AND	r5, r3, r9
	AND	r4, r2, r8
	STP	p2, [p3 + 4]
.LBB0_17:                               ; %if.end68
	MOVI	r10, 0
	MOV	r11, r10
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r4, r10, r6
	SBC	r5, r11, r7
	AND	r3, r7, r5
	AND	r2, r6, r4
	MOVI	r7, 1916
	MOVI	r6, -19151
	MOVP	p2, p3
	JAL	__mulsi3
	SHRI	r2, r3, 11
	MOV	r3, r10
	LEA	p2, [.LCPI0_0]
	ADDPP	p1, p2, p1
	LDBU	r4, [p1]
	MOVP	p1, p4
	JAL	__lshrsi3
	MOVP	p2, p5
	MOVP	p3, p1
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	BRA	.LBB0_47
.LBB0_23:                               ; %if.then99
	LDP	p3, [p7 + 36]                   ; 4-byte Folded Reload
	ADDPI	p1, p3, -1
	AND	r5, r7, r3
	AND	r4, r6, r2
	CMP	r4, r12
	CPC	r5, r13
	BNE	.LBB0_29
; %bb.24:                               ; %if.then106
	MOVI	r7, 0
	MOV	r6, r7
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	CMP	r12, r6
	CPC	r13, r7
	BEQ	.LBB0_26
; %bb.25:                               ; %if.then108
	STP	p3, [p6 + 4]
	LDP	p4, [p7 + 52]                   ; 4-byte Folded Reload
	AND	r5, r3, r9
	AND	r4, r2, r8
	STP	p2, [p6]
.LBB0_26:                               ; %if.end114
	MOVI	r6, 1
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BNE	.LBB0_28
; %bb.27:
	LDP	p3, [p7 + 52]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 48]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	BRA	.LBB0_47
.LBB0_2:                                ; %if.then3
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	MOVP	p2, p3
	MOVP	p5, p2
	JAL	__udivsi3
	MOVP	p3, p4
	MOVP	p4, p1
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB0_4
; %bb.3:                                ; %if.then4
	STP	p2, [p3 + 4]
	MOVP	p1, p4
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	MOVP	p5, p3
	JAL	__mulsi3
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	STP	p1, [p5]
	BRA	.LBB0_4
.LBB0_35:                               ; %if.end193
	MOVI	r13, 0
	MOV	r4, r13
	MOV	r5, r13
	ADDPI	p4, p1, 1
	CMP	r8, r12
	CPC	r9, r13
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	BNE	.LBB0_37
; %bb.36:
	STP	p6, [p7 + 32]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	BRA	.LBB0_38
.LBB0_6:
	MOVP	p2, p3
	BRA	.LBB0_47
.LBB0_10:
                                        ; implicit-def: $p3
                                        ; implicit-def: $p2
	BRA	.LBB0_47
.LBB0_29:                               ; %if.end131
	LDP	p2, [p7 + 48]                   ; 4-byte Folded Reload
	SHLI	r2, r5, 15
	SHRI	r3, r4, 1
	OR	r2, r3, r2
	SHRI	r3, r5, 1
	OR	r13, r5, r3
	OR	r12, r4, r2
	SHLI	r2, r7, 15
	SHRI	r3, r6, 1
	OR	r4, r3, r2
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	OR	r8, r3, r2
	SHRI	r5, r7, 1
	SHRI	r9, r13, 2
	OR	r3, r7, r5
	OR	r2, r6, r4
	OR	r7, r13, r9
	OR	r6, r12, r8
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r4, r5, r4
	SHLI	r12, r7, 12
	SHRI	r13, r6, 4
	OR	r10, r13, r12
	SHRI	r5, r3, 2
	SHRI	r11, r7, 4
	OR	r13, r3, r5
	OR	r12, r2, r4
	OR	r9, r7, r11
	OR	r8, r6, r10
	SHLI	r2, r13, 12
	SHRI	r3, r12, 4
	OR	r4, r3, r2
	SHLI	r2, r9, 8
	SHRI	r3, r8, 8
	OR	r10, r3, r2
	SHRI	r5, r13, 4
	SHRI	r11, r9, 8
	OR	r3, r13, r5
	OR	r2, r12, r4
	OR	r7, r9, r11
	OR	r6, r8, r10
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r4, r5, r4
	SHRI	r5, r3, 8
	MOVI	r11, 0
	MOV	r10, r7
	OR	r9, r7, r11
	OR	r8, r6, r10
	OR	r13, r3, r5
	OR	r12, r2, r4
	NOT	r7, r9
	NOT	r6, r8
	MOV	r2, r13
	MOV	r3, r11
	OR	r5, r13, r3
	OR	r4, r12, r2
	SHRI	r2, r6, 1
	SHLI	r3, r7, 15
	OR	r12, r2, r3
	SHRI	r13, r7, 1
	NOT	r3, r5
	NOT	r2, r4
	ANDI	r5, r13, 21845
	ANDI	r4, r12, 21845
	SUB	r4, r6, r4
	SBC	r5, r7, r5
	SHRI	r6, r2, 1
	SHLI	r7, r3, 15
	OR	r12, r6, r7
	SHRI	r13, r3, 1
	SHLI	r6, r5, 14
	SHRI	r7, r4, 2
	OR	r8, r7, r6
	ANDI	r6, r4, 13107
	ANDI	r7, r5, 13107
	SHRI	r9, r5, 2
	ANDI	r5, r13, 21845
	ANDI	r4, r12, 21845
	SUB	r4, r2, r4
	SBC	r5, r3, r5
	ANDI	r3, r9, 13107
	ANDI	r2, r8, 13107
	ADDPP	p3, p3, p1
	SHLI	r2, r5, 14
	SHRI	r3, r4, 2
	OR	r12, r3, r2
	ANDI	r2, r4, 13107
	ANDI	r3, r5, 13107
	SHRI	r13, r5, 2
	SHRI	r4, r6, 4
	SHLI	r5, r7, 12
	OR	r4, r4, r5
	SHRI	r5, r7, 4
	ADDPP	p2, p3, p2
	ANDI	r7, r13, 13107
	ANDI	r6, r12, 13107
	ADDPP	p6, p1, p3
	ANDI	r3, r5, 3855
	ANDI	r2, r4, 3855
	SHRI	r4, r12, 4
	SHLI	r5, r13, 12
	OR	r4, r4, r5
	SHRI	r5, r13, 4
	SHLI	r6, r3, 8
	SHRI	r7, r2, 8
	OR	r7, r6, r7
	ADDPP	p6, p6, p2
	ANDI	r5, r13, 3855
	ANDI	r4, r12, 3855
	SHLI	r6, r2, 8
	SHLI	r12, r5, 8
	SHRI	r13, r4, 8
	OR	r13, r12, r13
	SHLI	r12, r4, 8
	ADDPP	p2, p2, p6
	MOV	r12, r11
	MOV	r13, r4
	ADDPP	p2, p2, p6
	ADDPP	p1, p1, p3
	MOV	r6, r11
	MOV	r7, r2
	ADDPP	p1, p1, p3
	SHRI	r2, r3, 8
	MOV	r3, r11
	SHRI	r4, r5, 8
	MOV	r5, r11
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	MOVI	r4, 32
	MOV	r5, r11
	ADDPI	p4, p1, 33
	CMP	r8, r4
	CPC	r9, r5
	BNE	.LBB0_31
; %bb.30:
	STP	p2, [p7 + 32]                   ; 4-byte Folded Spill
	MOV	r10, r11
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	BRA	.LBB0_38
.LBB0_37:                               ; %if.else204
	MOVI	r4, 31
	SUB	r4, r4, r2
	STW	r4, [p7 + 32]                   ; 2-byte Folded Spill
	LDP	p5, [p7 + 52]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 40]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	MOV	r4, r8
	JAL	__lshrsi3
	LDP	p3, [p7 + 40]                   ; 4-byte Folded Reload
	OR	r5, r7, r3
	OR	r4, r6, r2
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	LDW	r4, [p7 + 32]                   ; 2-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
	BRA	.LBB0_38
.LBB0_12:                               ; %if.then40
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__udivsi3
	MOVP	p4, p1
	MOVI	r4, 0
	MOV	r5, r4
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB0_4
; %bb.13:                               ; %if.then42
	STP	p2, [p3]
	MOVP	p1, p4
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	MOVP	p5, p3
	JAL	__mulsi3
	LDP	p2, [p7 + 48]                   ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	STP	p1, [p5 + 4]
.LBB0_4:                                ; %if.end
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p3, p4
	BRA	.LBB0_47
.LBB0_18:                               ; %if.end72
	LDP	p2, [p7 + 48]                   ; 4-byte Folded Reload
	SHLI	r2, r5, 15
	SHRI	r3, r4, 1
	OR	r2, r3, r2
	SHRI	r3, r5, 1
	STP	p6, [p7 + 44]                   ; 4-byte Folded Spill
	OR	r13, r5, r3
	OR	r12, r4, r2
	SHLI	r2, r11, 15
	SHRI	r3, r10, 1
	OR	r4, r3, r2
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	MOVP	p3, p5
	OR	r10, r3, r2
	SHRI	r5, r7, 1
	SHRI	r11, r13, 2
	OR	r3, r7, r5
	OR	r2, r6, r4
	OR	r7, r13, r11
	OR	r6, r12, r10
	SHLI	r4, r3, 14
	SHRI	r5, r2, 2
	OR	r4, r5, r4
	SHLI	r12, r7, 12
	SHRI	r13, r6, 4
	OR	r8, r13, r12
	SHRI	r5, r3, 2
	SHRI	r9, r7, 4
	OR	r13, r3, r5
	OR	r12, r2, r4
	OR	r11, r7, r9
	OR	r10, r6, r8
	SHLI	r2, r13, 12
	SHRI	r3, r12, 4
	OR	r4, r3, r2
	SHLI	r2, r11, 8
	SHRI	r3, r10, 8
	OR	r8, r3, r2
	SHRI	r5, r13, 4
	SHRI	r9, r11, 8
	OR	r3, r13, r5
	OR	r2, r12, r4
	OR	r7, r11, r9
	OR	r6, r10, r8
	SHLI	r4, r3, 8
	SHRI	r5, r2, 8
	OR	r12, r5, r4
	SHRI	r13, r3, 8
	MOV	r4, r7
	LDP	p4, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r5, r8
	OR	r11, r7, r5
	OR	r10, r6, r4
	OR	r5, r3, r13
	OR	r4, r2, r12
	NOT	r7, r11
	NOT	r6, r10
	MOV	r2, r5
	MOV	r3, r8
	OR	r13, r5, r3
	OR	r12, r4, r2
	SHRI	r2, r6, 1
	SHLI	r3, r7, 15
	OR	r4, r2, r3
	SHRI	r5, r7, 1
	NOT	r3, r13
	NOT	r2, r12
	ANDI	r13, r5, 21845
	ANDI	r12, r4, 21845
	SUB	r4, r6, r12
	SBC	r5, r7, r13
	SHRI	r6, r2, 1
	SHLI	r7, r3, 15
	OR	r12, r6, r7
	SHRI	r13, r3, 1
	SHLI	r6, r5, 14
	SHRI	r7, r4, 2
	OR	r10, r7, r6
	ANDI	r6, r4, 13107
	ANDI	r7, r5, 13107
	SHRI	r11, r5, 2
	ANDI	r5, r13, 21845
	ANDI	r4, r12, 21845
	SUB	r4, r2, r4
	SBC	r5, r3, r5
	ANDI	r3, r11, 13107
	ANDI	r2, r10, 13107
	ADDPP	p3, p3, p1
	SHLI	r2, r5, 14
	SHRI	r3, r4, 2
	OR	r12, r3, r2
	ANDI	r2, r4, 13107
	ANDI	r3, r5, 13107
	SHRI	r13, r5, 2
	SHRI	r4, r6, 4
	SHLI	r5, r7, 12
	OR	r4, r4, r5
	SHRI	r5, r7, 4
	ADDPP	p2, p3, p2
	ANDI	r7, r13, 13107
	ANDI	r6, r12, 13107
	ADDPP	p3, p1, p3
	ANDI	r3, r5, 3855
	ANDI	r2, r4, 3855
	SHRI	r4, r6, 4
	SHLI	r5, r7, 12
	OR	r4, r4, r5
	SHRI	r5, r7, 4
	SHLI	r12, r3, 8
	SHRI	r13, r2, 8
	OR	r13, r12, r13
	ADDPP	p3, p3, p2
	SHLI	r12, r2, 8
	ANDI	r5, r7, 3855
	ANDI	r4, r6, 3855
	ADDPP	p1, p1, p6
	SHLI	r6, r5, 8
	SHRI	r7, r4, 8
	OR	r7, r6, r7
	SHLI	r6, r4, 8
	ADDPP	p2, p2, p3
	MOVP	p6, p4
	MOV	r13, r4
	ADDPP	p2, p2, p6
	MOV	r13, r2
	ADDPP	p3, p1, p6
	MOVI	r2, 31
	MOV	r3, r12
	SHRI	r6, r7, 8
	MOV	r7, r12
	SHRI	r4, r5, 8
	MOV	r5, r12
	SUB	r8, r4, r6
	SBC	r9, r5, r7
	CMP	r8, r2
	CPC	r9, r3
	BHS	.LBB0_19
; %bb.21:                               ; %if.end82
	SUB	r4, r2, r8
	STW	r4, [p7 + 44]                   ; 2-byte Folded Spill
	LDP	p5, [p7 + 52]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 40]                   ; 4-byte Folded Spill
	ADDPI	p4, p4, 1
	MOVP	p1, p5
	MOV	r4, r8
	JAL	__lshrsi3
	LDP	p3, [p7 + 40]                   ; 4-byte Folded Reload
	OR	r5, r7, r3
	OR	r4, r6, r2
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	LDW	r4, [p7 + 44]                   ; 2-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	MOV	r4, r8
	JAL	__lshrsi3
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
	MOVI	r2, 0
	MOV	r3, r2
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	BRA	.LBB0_38
.LBB0_28:                               ; %if.end119
	MOVI	r8, 0
	MOV	r9, r8
	SUB	r2, r8, r4
	SBC	r3, r9, r5
	AND	r11, r5, r3
	AND	r10, r4, r2
	MOVI	r7, 1916
	MOVI	r6, -19151
	MOVP	p1, p5
	LDP	p5, [p7 + 48]                   ; 4-byte Folded Reload
	MOVP	p2, p3
	JAL	__mulsi3
	SHRI	r2, r3, 11
	MOV	r3, r8
	LEA	p2, [.LCPI0_0]
	ADDPP	p1, p2, p1
	LDBU	r2, [p1]
	ANDI	r4, r2, 31
	MOV	r8, r2
	STW	r8, [p7 + 44]                   ; 2-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	JAL	__lshrsi3
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	SHLI	r2, r11, 1
	SHRI	r3, r10, 15
	OR	r3, r2, r3
	SHLI	r2, r10, 1
	NOT	r4, r8
	ANDI	r4, r4, 31
	JAL	__ashlsi3
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	OR	r9, r3, r5
	OR	r8, r2, r4
	MOVP	p1, p5
	LDW	r4, [p7 + 44]                   ; 2-byte Folded Reload
	JAL	__lshrsi3
	MOVP	p3, p4
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p1
	BRA	.LBB0_47
.LBB0_31:                               ; %if.else144
	MOVI	r10, 31
	CMP	r8, r10
	CPC	r9, r11
	BHI	.LBB0_33
; %bb.32:                               ; %if.then147
	MOVI	r2, 32
	SUB	r4, r2, r8
	STW	r4, [p7 + 44]                   ; 2-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	JAL	__ashlsi3
	MOVP	p5, p1
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	OR	r5, r11, r3
	OR	r4, r10, r2
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	LDW	r4, [p7 + 44]                   ; 2-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	MOV	r4, r8
	JAL	__lshrsi3
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
	MOVI	r2, 0
	MOV	r3, r2
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	BRA	.LBB0_38
.LBB0_33:                               ; %if.else163
	MOVI	r2, 64
	SUB	r4, r2, r8
	STW	r4, [p7 + 44]                   ; 2-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	JAL	__ashlsi3
	MOVP	p2, p4
	STP	p2, [p7 + 32]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	ADDI	r4, r4, -32
	STW	r4, [p7 + 40]                   ; 2-byte Folded Spill
	LDP	p5, [p7 + 52]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	JAL	__lshrsi3
	OR	r5, r9, r3
	OR	r4, r8, r2
	MOVP	p4, p2
	MOVP	p1, p5
	LDW	r4, [p7 + 44]                   ; 2-byte Folded Reload
	JAL	__ashlsi3
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	LDW	r4, [p7 + 40]                   ; 2-byte Folded Reload
	JAL	__lshrsi3
	STP	p1, [p7 + 40]                   ; 4-byte Folded Spill
	MOVI	r2, 0
	MOV	r3, r2
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 52]                   ; 4-byte Folded Spill
.LBB0_38:                               ; %for.body.preheader
	MOVI	r5, 0
	MOV	r2, r5
	MOVI	r7, 0
	MOV	r3, r5
	MOVI	r12, 1
	STP	p6, [p7 + 48]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 40]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	BRA	.LBB0_39
.LBB0_43:                               ; %for.body
                                        ;   in Loop: Header=BB0_39 Depth=1
	LDP	p6, [p7 + 52]                   ; 4-byte Folded Reload
	OR	r5, r13, r7
	OR	r4, r12, r6
	STP	p2, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	OR	r7, r13, r5
	OR	r6, r12, r4
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	SUB	r4, r4, r2
	SBC	r5, r5, r3
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	SUB	r8, r2, r8
	SBC	r9, r3, r9
	SHRI	r12, r11, 15
	LDP	p1, [p7 + 44]                   ; 4-byte Folded Reload
	MOVP	p3, p1
	MOV	r13, r3
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 32]                   ; 4-byte Folded Reload
	ADDPI	p6, p6, -1
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB0_44
.LBB0_39:                               ; %for.body
                                        ; =>This Inner Loop Header: Depth=1
	STP	p6, [p7 + 32]                   ; 4-byte Folded Spill
	LDP	p5, [p7 + 52]                   ; 4-byte Folded Reload
	SHRI	r2, r8, 15
	SHLI	r3, r9, 1
	OR	r3, r3, r2
	SHLI	r2, r8, 1
	MOVP	p6, p3
	SHRI	r12, r11, 15
	OR	r7, r3, r13
	OR	r6, r2, r12
	STP	p6, [p7 + 44]                   ; 4-byte Folded Spill
	SHLI	r2, r5, 1
	SHRI	r3, r4, 15
	OR	r3, r2, r3
	SHLI	r2, r4, 1
	SHRI	r4, r9, 15
	MOV	r5, r13
	OR	r9, r3, r5
	OR	r8, r2, r4
	STP	p5, [p7 + 52]                   ; 4-byte Folded Spill
	SHLI	r2, r11, 1
	SHRI	r3, r10, 15
	OR	r13, r2, r3
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	SHRI	r2, r10, 15
	SHLI	r3, r11, 1
	OR	r3, r3, r2
	NOT	r5, r7
	STP	p3, [p7 + 40]                   ; 4-byte Folded Spill
	NOT	r4, r6
	SHLI	r2, r10, 1
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	SHLI	r12, r2, 1
	STP	p6, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 48]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r13, r7
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	ADDPP	p1, p2, p1
	CMP	r2, r4
	CPC	r3, r5
	STP	p6, [p7 + 48]                   ; 4-byte Folded Spill
	MOVP	p2, p6
	MOVP	p6, p3
	BLO	.LBB0_41
; %bb.40:                               ; %for.body
                                        ;   in Loop: Header=BB0_39 Depth=1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
.LBB0_41:                               ; %for.body
                                        ;   in Loop: Header=BB0_39 Depth=1
	NOT	r3, r9
	NOT	r2, r8
	SHRI	r6, r11, 15
	MOV	r7, r13
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p6
	ADDPP	p5, p1, p2
	SARI	r2, r11, 15
	MOV	r3, r2
	AND	r5, r3, r13
	AND	r4, r2, r12
	SUB	r4, r8, r4
	SBC	r5, r9, r5
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	AND	r9, r3, r5
	AND	r8, r2, r4
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	BLO	.LBB0_43
; %bb.42:                               ; %for.body
                                        ;   in Loop: Header=BB0_39 Depth=1
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	BRA	.LBB0_43
.LBB0_44:                               ; %for.end
	MOVP	p6, p4
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 52]                   ; 4-byte Folded Reload
	SHLI	r2, r7, 1
	SHRI	r3, r6, 15
	OR	r5, r2, r3
	SHLI	r4, r6, 1
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	SHRI	r2, r11, 15
	MOVI	r3, 0
	OR	r9, r3, r5
	OR	r8, r2, r4
	SHLI	r4, r11, 1
	SHRI	r5, r10, 15
	OR	r5, r4, r5
	SHLI	r4, r10, 1
	MOV	r2, r3
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB0_46
; %bb.45:                               ; %if.then257
	STP	p6, [p3]
	LDP	p1, [p7 + 44]                   ; 4-byte Folded Reload
	STP	p1, [p3 + 4]
.LBB0_46:                               ; %if.end258
	ANDI	r3, r5, -1
	ANDI	r2, r4, -2
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	OR	r7, r5, r3
	OR	r6, r4, r2
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p4
.LBB0_47:                               ; %cleanup
	MOVP	p1, p3
	LDP	p4, [p7 + 60]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 56]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 64
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
