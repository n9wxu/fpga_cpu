	.text
	.globl	__umoddi3                       ; -- Begin function __umoddi3
	.p2align	1
__umoddi3:                              ; @__umoddi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -64
	STP	p4, [p7 + 60]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 56]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 52]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	LDP	p1, [p7 + 64]
	OR	r13, r7, r3
	OR	r12, r6, r2
	MOVI	r9, 0
	MOVI	r8, 64
	MOV	r4, r9
	MOV	r5, r9
	CMP	r12, r4
	CPC	r13, r5
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p3, [p7 + 40]                   ; 4-byte Folded Spill
	BEQ	.LBB0_3
; %bb.1:                                ; %cond.true.i
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB0_4
; %bb.2:                                ; %cond.true.i
	SHLI	r2, r7, 15
	SHRI	r3, r6, 1
	OR	r2, r3, r2
	SHRI	r3, r7, 1
	OR	r13, r7, r3
	OR	r12, r6, r2
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	OR	r2, r3, r2
	SHRI	r3, r13, 2
	OR	r11, r13, r3
	OR	r10, r12, r2
	SHLI	r2, r11, 12
	SHRI	r3, r10, 4
	OR	r2, r3, r2
	SHRI	r3, r11, 4
	OR	r13, r11, r3
	OR	r12, r10, r2
	SHLI	r2, r13, 8
	SHRI	r3, r12, 8
	OR	r2, r3, r2
	SHRI	r3, r13, 8
	OR	r7, r13, r3
	OR	r6, r12, r2
	MOV	r2, r7
	MOV	r3, r9
	OR	r11, r7, r3
	OR	r10, r6, r2
	NOT	r13, r11
	NOT	r12, r10
	SHRI	r2, r12, 1
	SHLI	r3, r13, 15
	OR	r2, r2, r3
	SHRI	r3, r13, 1
	ANDI	r7, r3, 21845
	ANDI	r6, r2, 21845
	SUB	r12, r12, r6
	SBC	r13, r13, r7
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	OR	r2, r3, r2
	SHRI	r3, r13, 2
	ANDI	r7, r3, 13107
	ANDI	r6, r2, 13107
	ANDI	r3, r13, 13107
	ANDI	r2, r12, 13107
	ADDPP	p1, p1, p3
	SHRI	r6, r2, 4
	SHLI	r7, r3, 12
	OR	r6, r6, r7
	SHRI	r7, r3, 4
	ADDPP	p1, p1, p3
	ANDI	r13, r3, 3855
	ANDI	r12, r2, 3855
	SHLI	r2, r13, 8
	SHRI	r3, r12, 8
	OR	r3, r2, r3
	SHLI	r2, r12, 8
	ADDPP	p1, p6, p1
	MOV	r6, r9
	MOV	r7, r2
	ADDPP	p1, p1, p3
	SHRI	r2, r3, 8
	MOV	r3, r9
	ADDPI	p6, p1, 32
	BRA	.LBB0_5
.LBB0_3:
	MOVP	p6, p4
	BRA	.LBB0_5
.LBB0_4:
	SHLI	r12, r3, 15
	SHRI	r13, r2, 1
	OR	r10, r13, r12
	SHRI	r11, r3, 1
	OR	r13, r3, r11
	OR	r12, r2, r10
	SHLI	r10, r13, 14
	SHRI	r11, r12, 2
	OR	r6, r11, r10
	SHRI	r7, r13, 2
	OR	r11, r13, r7
	OR	r10, r12, r6
	SHLI	r6, r11, 12
	SHRI	r7, r10, 4
	OR	r6, r7, r6
	SHRI	r7, r11, 4
	OR	r13, r11, r7
	OR	r12, r10, r6
	SHLI	r6, r13, 8
	SHRI	r7, r12, 8
	OR	r6, r7, r6
	SHRI	r7, r13, 8
	OR	r11, r13, r7
	OR	r10, r12, r6
	MOV	r6, r11
	MOV	r7, r9
	OR	r3, r11, r7
	OR	r2, r10, r6
	NOT	r13, r3
	NOT	r12, r2
	SHRI	r2, r12, 1
	SHLI	r3, r13, 15
	OR	r2, r2, r3
	SHRI	r3, r13, 1
	ANDI	r7, r3, 21845
	ANDI	r6, r2, 21845
	SUB	r12, r12, r6
	SBC	r13, r13, r7
	SHLI	r2, r13, 14
	SHRI	r3, r12, 2
	OR	r2, r3, r2
	SHRI	r3, r13, 2
	ANDI	r7, r3, 13107
	ANDI	r6, r2, 13107
	ANDI	r3, r13, 13107
	ANDI	r2, r12, 13107
	ADDPP	p1, p1, p3
	SHRI	r6, r2, 4
	SHLI	r7, r3, 12
	OR	r6, r6, r7
	SHRI	r7, r3, 4
	ADDPP	p1, p1, p3
	ANDI	r13, r3, 3855
	ANDI	r12, r2, 3855
	SHLI	r2, r13, 8
	SHRI	r3, r12, 8
	OR	r3, r2, r3
	SHLI	r2, r12, 8
	ADDPP	p1, p6, p1
	MOV	r6, r9
	MOV	r7, r2
	ADDPP	p1, p1, p3
	SHRI	r12, r3, 8
	MOV	r13, r9
.LBB0_5:                                ; %cond.end.i
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 52]                   ; 4-byte Folded Reload
	OR	r11, r3, r7
	OR	r10, r2, r6
	CMP	r10, r4
	CPC	r11, r5
	BEQ	.LBB0_9
; %bb.6:                                ; %cond.true2.i
	MOVI	r10, 0
	MOV	r11, r10
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB0_8
; %bb.7:                                ; %cond.true2.i
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	SHLI	r4, r3, 15
	SHRI	r5, r2, 1
	OR	r4, r5, r4
	SHRI	r5, r3, 1
	OR	r7, r3, r5
	OR	r6, r2, r4
	SHLI	r2, r7, 14
	SHRI	r3, r6, 2
	OR	r2, r3, r2
	SHRI	r3, r7, 2
	OR	r9, r7, r3
	OR	r8, r6, r2
	SHLI	r2, r9, 12
	SHRI	r3, r8, 4
	OR	r2, r3, r2
	SHRI	r3, r9, 4
	OR	r7, r9, r3
	OR	r6, r8, r2
	SHLI	r2, r7, 8
	SHRI	r3, r6, 8
	OR	r2, r3, r2
	SHRI	r3, r7, 8
	OR	r5, r7, r3
	OR	r4, r6, r2
	MOV	r2, r5
	MOV	r3, r10
	OR	r7, r5, r3
	OR	r6, r4, r2
	NOT	r9, r7
	NOT	r8, r6
	SHRI	r2, r8, 1
	SHLI	r3, r9, 15
	OR	r2, r2, r3
	SHRI	r3, r9, 1
	ANDI	r5, r3, 21845
	ANDI	r4, r2, 21845
	SUB	r6, r8, r4
	SBC	r7, r9, r5
	SHLI	r2, r7, 14
	SHRI	r3, r6, 2
	OR	r2, r3, r2
	SHRI	r3, r7, 2
	ANDI	r5, r3, 13107
	ANDI	r4, r2, 13107
	ANDI	r3, r7, 13107
	ANDI	r2, r6, 13107
	ADDPP	p1, p1, p2
	SHRI	r4, r2, 4
	SHLI	r5, r3, 12
	OR	r4, r4, r5
	SHRI	r5, r3, 4
	ADDPP	p1, p1, p2
	ANDI	r7, r3, 3855
	ANDI	r6, r2, 3855
	SHLI	r2, r7, 8
	SHRI	r3, r6, 8
	OR	r3, r2, r3
	SHLI	r2, r6, 8
	ADDPP	p1, p3, p1
	MOV	r11, r2
	ADDPP	p1, p1, p5
	SHRI	r2, r3, 8
	MOV	r3, r10
	ADDPI	p4, p1, 32
	BRA	.LBB0_9
.LBB0_8:
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	SHLI	r4, r3, 15
	SHRI	r5, r2, 1
	OR	r4, r5, r4
	SHRI	r5, r3, 1
	OR	r7, r3, r5
	OR	r6, r2, r4
	SHLI	r2, r7, 14
	SHRI	r3, r6, 2
	OR	r2, r3, r2
	SHRI	r3, r7, 2
	OR	r9, r7, r3
	OR	r8, r6, r2
	SHLI	r2, r9, 12
	SHRI	r3, r8, 4
	OR	r2, r3, r2
	SHRI	r3, r9, 4
	OR	r7, r9, r3
	OR	r6, r8, r2
	SHLI	r2, r7, 8
	SHRI	r3, r6, 8
	OR	r2, r3, r2
	SHRI	r3, r7, 8
	OR	r5, r7, r3
	OR	r4, r6, r2
	MOV	r2, r5
	MOV	r3, r10
	OR	r7, r5, r3
	OR	r6, r4, r2
	NOT	r9, r7
	NOT	r8, r6
	SHRI	r2, r8, 1
	SHLI	r3, r9, 15
	OR	r2, r2, r3
	SHRI	r3, r9, 1
	ANDI	r5, r3, 21845
	ANDI	r4, r2, 21845
	SUB	r6, r8, r4
	SBC	r7, r9, r5
	SHLI	r2, r7, 14
	SHRI	r3, r6, 2
	OR	r2, r3, r2
	SHRI	r3, r7, 2
	ANDI	r5, r3, 13107
	ANDI	r4, r2, 13107
	ANDI	r3, r7, 13107
	ANDI	r2, r6, 13107
	ADDPP	p1, p1, p2
	SHRI	r4, r2, 4
	SHLI	r5, r3, 12
	OR	r4, r4, r5
	SHRI	r5, r3, 4
	ADDPP	p1, p1, p2
	ANDI	r7, r3, 3855
	ANDI	r6, r2, 3855
	SHLI	r2, r7, 8
	SHRI	r3, r6, 8
	OR	r3, r2, r3
	SHLI	r2, r6, 8
	ADDPP	p1, p3, p1
	MOV	r11, r2
	ADDPP	p1, p1, p5
	SHRI	r8, r3, 8
	MOV	r9, r10
.LBB0_9:                                ; %cond.end5.i
	SUB	r10, r12, r8
	SBC	r11, r13, r9
	MOVI	r3, 0
	MOVI	r2, 63
	CMP	r10, r2
	CPC	r11, r3
	BLS	.LBB0_11
; %bb.10:
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 48]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	BRA	.LBB0_13
.LBB0_11:                               ; %if.end.i
	CMP	r10, r2
	CPC	r11, r3
	BNE	.LBB0_15
; %bb.12:
	MOV	r2, r3
	STP	p1, [p7 + 48]                   ; 4-byte Folded Spill
.LBB0_13:                               ; %__umodXi3.exit
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
.LBB0_14:                               ; %__umodXi3.exit
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 60]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 56]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 64
	RET
.LBB0_15:                               ; %if.end9.i
	ADDPI	p4, p5, 1
	ANDI	r7, r9, 0
	ANDI	r6, r8, -1
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	JAL	__lshrdi3
	STP	p1, [p7 + 48]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	MOVI	r2, 63
	SUB	r10, r2, r10
	MOVI	r11, 0
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	MOVP	p3, p5
	JAL	__ashldi3
	MOV	r12, r11
	MOV	r13, r11
	MOVI	r6, 1
	STP	p3, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 36]                   ; 4-byte Folded Spill
	BRA	.LBB0_17
.LBB0_16:                               ; %for.body.i
                                        ;   in Loop: Header=BB0_17 Depth=1
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p3, [p7 + 12]                   ; 4-byte Folded Reload
	OR	r3, r7, r5
	OR	r2, r6, r4
	LDP	p3, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	OR	r5, r13, r7
	OR	r4, r12, r6
	LDP	p6, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r12, r12, r8
	SBC	r13, r13, r9
	STP	p6, [p7 + 44]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 48]                   ; 4-byte Folded Reload
	SUB	r12, r12, r10
	SBC	r13, r13, r11
	STP	p6, [p7 + 48]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	SHRI	r12, r13, 15
	LDP	p5, [p7 + 52]                   ; 4-byte Folded Reload
	MOV	r13, r11
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p4, p4, -1
	CMP	r8, r6
	CPC	r9, r7
	BEQ	.LBB0_14
.LBB0_17:                               ; %for.body.i
                                        ; =>This Inner Loop Header: Depth=1
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p4, [p7 + 48]                   ; 4-byte Folded Reload
	SHRI	r6, r8, 15
	SHLI	r7, r9, 1
	OR	r7, r7, r6
	SHLI	r6, r8, 1
	SHRI	r10, r5, 15
	OR	r13, r7, r11
	OR	r12, r6, r10
	LDP	p1, [p7 + 44]                   ; 4-byte Folded Reload
	SHLI	r6, r3, 1
	SHRI	r7, r2, 15
	OR	r7, r6, r7
	SHLI	r6, r2, 1
	SHRI	r8, r9, 15
	MOV	r9, r11
	OR	r3, r7, r9
	OR	r2, r6, r8
	STP	p1, [p7 + 44]                   ; 4-byte Folded Spill
	SHLI	r6, r5, 1
	SHRI	r7, r4, 15
	OR	r9, r6, r7
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	SHRI	r6, r2, 15
	SHLI	r7, r3, 1
	OR	r3, r7, r6
	SHLI	r8, r4, 1
	MOVP	p2, p6
	NOT	r13, r5
	STP	p2, [p7 + 48]                   ; 4-byte Folded Spill
	NOT	r12, r4
	MOVP	p2, p1
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	SHRI	r6, r3, 15
	MOV	r7, r11
	OR	r3, r9, r7
	OR	r2, r8, r6
	STP	p1, [p7 + 20]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 52]                   ; 4-byte Folded Reload
	SHLI	r4, r2, 1
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p4, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r9, r11
	LDP	p1, [p7 + 40]                   ; 4-byte Folded Reload
	ADDPP	p1, p6, p1
	CMP	r2, r12
	CPC	r3, r13
	MOVP	p1, p4
	BLO	.LBB0_19
; %bb.18:                               ; %for.body.i
                                        ;   in Loop: Header=BB0_17 Depth=1
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
.LBB0_19:                               ; %for.body.i
                                        ;   in Loop: Header=BB0_17 Depth=1
	LDP	p5, [p7 + 44]                   ; 4-byte Folded Reload
	NOT	r5, r11
	NOT	r4, r10
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p2, p2, p3
	ADDPP	p1, p2, p1
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	SARI	r2, r3, 15
	MOV	r3, r2
	AND	r5, r3, r7
	AND	r4, r2, r6
	SUB	r4, r10, r4
	SBC	r5, r11, r5
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 40]                   ; 4-byte Folded Reload
	AND	r11, r3, r5
	AND	r10, r2, r4
	LDP	p1, [p7 + 48]                   ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	MOVP	p1, p4
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	BLO	.LBB0_16
; %bb.20:                               ; %for.body.i
                                        ;   in Loop: Header=BB0_17 Depth=1
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	BRA	.LBB0_16
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
