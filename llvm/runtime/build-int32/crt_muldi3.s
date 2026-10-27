	.text
	.globl	__muldi3                        ; -- Begin function __muldi3
	.p2align	1
__muldi3:                               ; @__muldi3
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -36
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	ANDI	r12, r6, -1
	ANDI	r13, r7, 0
	STP	p6, [p7 + 24]                   ; 4-byte Folded Spill
	ANDI	r4, r2, -1
	ANDI	r5, r3, 0
	MOVP	p4, p1
	STP	p4, [p7]                        ; 4-byte Folded Spill
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p1, p6
	JAL	__mulsi3
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r11, 0
	MOV	r10, r9
	MOV	r8, r3
	MOV	r9, r11
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__mulsi3
	ADDPP	p1, p4, p1
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__mulsi3
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	LDP	p1, [p7 + 36]
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	JAL	__mulsi3
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPP	p1, p1, p2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	ANDI	r4, r2, -1
	ANDI	r5, r3, 0
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	MOV	r8, r9
	MOV	r9, r11
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__mulsi3
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	ADDPP	p1, p2, p1
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p1, p4
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPP	p1, p2, p1
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	MOV	r10, r5
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPP	p2, p2, p5
	MOV	r6, r11
	MOV	r7, r2
	MOV	r10, r3
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	ANDI	r13, r3, 0
	ANDI	r12, r2, -1
	ADDPP	p2, p2, p5
	OR	r3, r7, r13
	OR	r2, r6, r12
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 36
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
