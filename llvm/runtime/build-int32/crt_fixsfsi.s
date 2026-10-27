	.text
	.globl	__fixsfsi                       ; -- Begin function __fixsfsi
	.p2align	1
__fixsfsi:                              ; @__fixsfsi
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	SHRI	r10, r3, 7
	MOVI	r11, 0
	ANDI	r7, r11, 0
	ANDI	r6, r10, 255
	MOVI	r8, -1
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BLE	.LBB0_2
; %bb.1:
	MOVI	r8, 1
	MOV	r9, r11
.LBB0_2:                                ; %entry
	MOVI	r10, 127
	MOV	r4, r11
	MOV	r5, r11
	CMP	r6, r10
	CPC	r7, r11
	BLO	.LBB0_9
; %bb.3:                                ; %if.end.i
	MOVI	r5, -1
	MOVI	r4, -33
	ADDPI	p5, p3, -159
	CMP	r10, r4
	CPC	r11, r5
	BHI	.LBB0_5
; %bb.4:                                ; %if.then4.i
	SARI	r2, r3, 15
	MOV	r3, r2
	XORI	r5, r3, 32767
	XORI	r4, r2, -1
	BRA	.LBB0_9
.LBB0_5:                                ; %if.end7.i
	MOVP	p5, p6
	ANDI	r4, r2, -1
	ANDI	r5, r3, 127
	ORI	r3, r5, 128
	ORI	r2, r4, 0
	MOVI	r5, 0
	MOVI	r4, 149
	CMP	r6, r4
	CPC	r7, r5
	BHI	.LBB0_7
; %bb.6:                                ; %if.then9.i
	MOVI	r4, 150
	SUB	r4, r4, r6
	JAL	__lshrsi3
	BRA	.LBB0_8
.LBB0_7:                                ; %if.else.i
	ADDI	r4, r6, -150
	JAL	__ashlsi3
.LBB0_8:                                ; %__fixint.exit
	MOVP	p2, p4
	JAL	__mulsi3
	MOVP	p2, p1
	MOVP	p6, p5
.LBB0_9:                                ; %__fixint.exit
	MOVP	p1, p2
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
