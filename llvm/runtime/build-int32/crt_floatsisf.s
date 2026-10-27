	.text
	.globl	__floatsisf                     ; -- Begin function __floatsisf
	.p2align	1
__floatsisf:                            ; @__floatsisf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	MOVI	r3, 0
	MOV	r2, r3
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB0_10
; %bb.1:                                ; %if.end
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	SARI	r6, r5, 15
	MOV	r7, r6
	XOR	r13, r5, r7
	XOR	r12, r4, r6
	SUB	r8, r12, r6
	SBC	r9, r13, r7
	ANDI	r7, r5, -32768
	ANDI	r6, r4, 0
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	SHLI	r4, r9, 15
	SHRI	r5, r8, 1
	OR	r6, r5, r4
	SHRI	r7, r9, 1
	OR	r5, r9, r7
	OR	r4, r8, r6
	SHLI	r6, r5, 14
	SHRI	r7, r4, 2
	OR	r12, r7, r6
	SHRI	r13, r5, 2
	OR	r7, r5, r13
	OR	r6, r4, r12
	SHLI	r4, r7, 12
	SHRI	r5, r6, 4
	OR	r12, r5, r4
	SHRI	r13, r7, 4
	OR	r5, r7, r13
	OR	r4, r6, r12
	SHLI	r6, r5, 8
	SHRI	r7, r4, 8
	OR	r6, r7, r6
	SHRI	r7, r5, 8
	OR	r13, r5, r7
	OR	r12, r4, r6
	MOV	r2, r13
	OR	r7, r13, r3
	OR	r6, r12, r2
	NOT	r5, r7
	NOT	r4, r6
	SHRI	r6, r4, 1
	SHLI	r7, r5, 15
	OR	r6, r6, r7
	SHRI	r7, r5, 1
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
	MOVI	r2, 23
	SHRI	r10, r5, 8
	MOV	r11, r3
	XORI	r5, r11, 0
	XORI	r4, r10, 31
	CMP	r4, r2
	CPC	r5, r3
	BHI	.LBB0_3
; %bb.2:                                ; %if.then6
	SUB	r4, r2, r4
	MOVP	p1, p4
	JAL	__ashlsi3
	XORI	r5, r3, 128
	XORI	r4, r2, 0
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	BRA	.LBB0_9
.LBB0_3:                                ; %if.else
	MOVI	r2, 8
	SUB	r4, r2, r10
	MOVP	p1, p4
	JAL	__lshrsi3
	XORI	r5, r3, 128
	XORI	r4, r2, 0
	STP	p2, [p7]                        ; 4-byte Folded Spill
	ADDI	r4, r10, 24
	MOVP	p1, p4
	JAL	__ashlsi3
	MOVI	r7, -32768
	MOVI	r6, 0
	MOV	r4, r6
	MOV	r5, r6
	CMP	r2, r6
	CPC	r3, r7
	BHI	.LBB0_4
; %bb.5:                                ; %if.else
	MOVP	p6, p2
	BRA	.LBB0_6
.LBB0_4:
	MOVI	r12, 1
	MOV	r13, r6
.LBB0_6:                                ; %if.else
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPP	p4, p4, p6
	CMP	r2, r6
	CPC	r3, r7
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	BNE	.LBB0_8
; %bb.7:
	ANDI	r5, r9, 0
	ANDI	r4, r8, 1
.LBB0_8:                                ; %if.else
	ADDPP	p2, p2, p4
.LBB0_9:                                ; %if.end19
	MOVI	r3, 20224
	MOVI	r2, 0
	SHLI	r7, r10, 7
	MOV	r6, r2
	SUB	r4, r4, r6
	SBC	r5, r5, r7
	ADDPP	p2, p2, p1
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	OR	r3, r5, r7
	OR	r2, r4, r6
.LBB0_10:                               ; %cleanup
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
