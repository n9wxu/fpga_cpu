	.text
	.globl	__fixunssfdi                    ; -- Begin function __fixunssfdi
	.p2align	1
__fixunssfdi:                           ; @__fixunssfdi
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r10, 0
	MOV	r11, r10
	MOVP	p2, p5
	JAL	__lesf2
	MOVI	r4, 1
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB0_1
; %bb.2:                                ; %if.end
	MOVI	r11, 12160
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__mulsf3
	JAL	__fixunssfsi
	STP	p1, [p7]                        ; 4-byte Folded Spill
	JAL	__floatunsisf
	MOVI	r11, -12416
	MOVP	p2, p5
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__addsf3
	JAL	__fixunssfsi
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	MOVP	p5, p1
	BRA	.LBB0_3
.LBB0_1:
	MOVP	p2, p5
.LBB0_3:                                ; %return
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
