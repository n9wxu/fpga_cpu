	.text
	.globl	clock                           ; -- Begin function clock
	.p2align	1
clock:                                  ; @clock
; %bb.0:                                ; %entry
.LBB0_1:                                ; %do.body.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r4, [15728646]
	LDW	r2, [15728644]
	LDW	r6, [15728646]
	CMP	r6, r4
	BNE	.LBB0_1
; %bb.2:                                ; %cycles.exit
	MOVI	r6, 0
	MOV	r7, r4
	MOV	r3, r6
	OR	r5, r7, r3
	OR	r4, r6, r2
	SHRI	r2, r4, 4
	SHLI	r3, r5, 12
	OR	r2, r2, r3
	SHRI	r3, r5, 4
	RET
                                        ; -- End function
	.globl	time                            ; -- Begin function time
	.p2align	1
time:                                   ; @time
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7]                        ; 4-byte Folded Spill
.LBB1_1:                                ; %do.body.i
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15728646]
	LDW	r4, [15728644]
	LDW	r6, [15728646]
	CMP	r6, r2
	BNE	.LBB1_1
; %bb.2:                                ; %cycles.exit
	MOVI	r8, 0
	MOV	r9, r2
	MOV	r5, r8
	OR	r3, r9, r5
	OR	r2, r8, r4
	MOVI	r5, 762
	MOVI	r4, -3968
	JAL	__udivsi3
	MOV	r9, r8
	CMP	r10, r8
	CPC	r11, r9
	BEQ	.LBB1_4
; %bb.3:                                ; %if.then
	STP	p1, [p5]
.LBB1_4:                                ; %if.end
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
