	.text
	.globl	strtof                          ; -- Begin function strtof
	.p2align	1
strtof:                                 ; @strtof
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -48
	STP	p4, [p7 + 44]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 40]                   ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r3, 0
	MOVI	r7, -1
	MOVI	r6, -6
	STP	p5, [p7]                        ; 4-byte Folded Spill
.LBB0_1:                                ; %while.cond
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p4, p5
	ADDPI	p5, p4, 1
	LDBU	r2, [p4]
	CMPI	r2, 32
	BEQ	.LBB0_1
; %bb.2:                                ; %while.cond
                                        ;   in Loop: Header=BB0_1 Depth=1
	ADDPI	p6, p1, -14
	CMP	r12, r6
	CPC	r13, r7
	BHI	.LBB0_1
; %bb.3:                                ; %while.end
	CMPI	r2, 45
	BEQ	.LBB0_6
; %bb.4:                                ; %while.end
	CMPI	r2, 43
	BNE	.LBB0_5
.LBB0_6:                                ; %if.then
	MOVI	r7, 0
	CMPI	r2, 45
	BNE	.LBB0_7
; %bb.8:                                ; %if.then
	MOV	r6, r7
	BRA	.LBB0_9
.LBB0_7:
	MOVI	r6, 1
.LBB0_9:                                ; %if.then
	LDBU	r12, [p5]
	ANDI	r3, r13, 0
	ANDI	r2, r12, 255
	BRA	.LBB0_10
.LBB0_5:
	MOVI	r6, 1
	MOV	r12, r2
	MOVP	p5, p4
.LBB0_10:                               ; %if.end
	STP	p3, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r5, 0
	MOVI	r4, 9
	MOV	r8, r5
	MOV	r9, r5
	ADDPI	p1, p1, -48
	MOVP	p3, p2
	CMP	r2, r4
	CPC	r3, r5
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	BLS	.LBB0_12
; %bb.11:
	MOVP	p3, p4
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_14
.LBB0_12:                               ; %while.body14.preheader
	MOVI	r5, 16672
	MOV	r4, r7
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVI	r6, 10
	STP	p3, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p3, p4
.LBB0_13:                               ; %while.body14
                                        ; =>This Inner Loop Header: Depth=1
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	LDBU	r4, [p5 + 1]
	ANDI	r9, r5, 0
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	ANDI	r8, r4, 255
	JAL	__floatunsisf
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p3, p1
	ADDPI	p5, p5, 1
	ADDPI	p1, p4, -48
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BLO	.LBB0_13
.LBB0_14:                               ; %while.end18
	MOVP	p2, p5
	LDP	p6, [p7 + 28]                   ; 4-byte Folded Reload
	ANDI	r2, r12, 255
	CMPI	r2, 46
	BNE	.LBB0_20
; %bb.15:                               ; %while.cond24.preheader
	LDBU	r12, [p2 + 1]
	ANDI	r3, r13, 0
	ANDI	r2, r12, 255
	MOVI	r8, 0
	MOVI	r10, 9
	MOV	r11, r8
	ADDPI	p2, p2, 1
	ADDPI	p1, p1, -48
	CMP	r2, r10
	CPC	r3, r11
	BLS	.LBB0_16
.LBB0_20:                               ; %if.end35
	STP	p6, [p7 + 28]                   ; 4-byte Folded Spill
	MOVI	r3, 0
	MOVI	r2, 10
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r8, r2
	CPC	r9, r3
	MOVP	p4, p2
	BLO	.LBB0_18
; %bb.21:                               ; %if.then37
	MOV	r2, r3
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB0_47
; %bb.22:                               ; %if.then39
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	STP	p1, [p2]
	MOVI	r2, 0
	MOV	r3, r2
	BRA	.LBB0_47
.LBB0_16:                               ; %while.body28.preheader
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	MOVI	r2, 0
	MOVI	r3, 16256
	MOVP	p6, p1
	MOVI	r3, 16672
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	MOVP	p4, p2
.LBB0_17:                               ; %while.body28
                                        ; =>This Inner Loop Header: Depth=1
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p3, [p7 + 36]                   ; 4-byte Folded Spill
	LDBU	r2, [p4 + 1]
	ANDI	r5, r3, 0
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	ANDI	r4, r2, 255
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p1, p6
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	JAL	__divsf3
	MOVP	p5, p1
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__floatunsisf
	MOVP	p2, p5
	JAL	__mulsf3
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p6, p5
	LDP	p5, [p7 + 16]                   ; 4-byte Folded Reload
	MOVP	p3, p1
	LDP	p1, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r11, r2
	ADDPI	p4, p4, 1
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p1, p1, -48
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	CMP	r2, r10
	CPC	r3, r11
	BLS	.LBB0_17
.LBB0_18:                               ; %if.end41
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	ANDI	r2, r2, 223
	CMPI	r2, 69
	BNE	.LBB0_19
; %bb.23:                               ; %if.then45
	LDBU	r2, [p4 + 1]
	CMPI	r2, 45
	BEQ	.LBB0_26
; %bb.24:                               ; %if.then45
	CMPI	r2, 43
	BNE	.LBB0_25
.LBB0_26:                               ; %if.then53
	MOVP	p5, p3
	MOVI	r5, 0
	CMPI	r2, 45
	BNE	.LBB0_27
; %bb.28:                               ; %if.then53
	MOV	r4, r5
	BRA	.LBB0_29
.LBB0_19:
	MOVP	p5, p3
	BRA	.LBB0_41
.LBB0_27:
	MOVI	r4, 1
.LBB0_29:                               ; %if.then53
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	LDBU	r2, [p2 + 2]
	ADDPI	p2, p2, 2
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	BRA	.LBB0_30
.LBB0_25:
	MOVP	p5, p3
	MOVI	r4, 1
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	ADDPI	p2, p4, 1
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
.LBB0_30:                               ; %if.end58
	ANDI	r5, r3, 0
	ANDI	r4, r2, 255
	MOVI	r3, -1
	MOVI	r2, -10
	ADDPI	p3, p2, -58
	CMP	r6, r2
	CPC	r7, r3
	BLO	.LBB0_42
; %bb.31:                               ; %while.body67.preheader
	STP	p5, [p7 + 36]                   ; 4-byte Folded Spill
	MOVI	r3, 0
	MOV	r2, r3
	MOVP	p3, p1
	ADDPI	p2, p2, -48
	MOVI	r2, 10
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	MOVI	r8, 9
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
.LBB0_32:                               ; %while.body67
                                        ; =>This Inner Loop Header: Depth=1
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p1, p3
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	JAL	__mulsi3
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPP	p3, p2, p1
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r9, r5
	LDBU	r2, [p5 + 1]
	MOV	r3, r5
	ADDPI	p5, p5, 1
	ADDPI	p2, p1, -48
	CMP	r4, r8
	CPC	r5, r9
	BLS	.LBB0_32
; %bb.33:                               ; %while.cond72.preheader
	MOVI	r8, 0
	MOVI	r2, 0
	MOV	r3, r8
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	CMP	r6, r2
	CPC	r7, r3
	MOVP	p2, p5
	BEQ	.LBB0_34
; %bb.35:
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	ANDI	r13, r3, 0
	ANDI	r12, r2, 1
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	MOVI	r3, 16672
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 36]                   ; 4-byte Folded Reload
	BRA	.LBB0_36
.LBB0_38:                               ; %cond.false
                                        ;   in Loop: Header=BB0_36 Depth=1
	MOV	r4, r8
	LDP	p3, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r5, r7
	JAL	__mulsf3
.LBB0_39:                               ; %cond.end
                                        ;   in Loop: Header=BB0_36 Depth=1
	MOVP	p3, p5
	ADDPI	p3, p3, -1
	MOV	r9, r8
	CMP	r6, r8
	CPC	r7, r9
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	BEQ	.LBB0_40
.LBB0_36:                               ; %while.body74
                                        ; =>This Inner Loop Header: Depth=1
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p5, p3
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BNE	.LBB0_38
; %bb.37:                               ; %cond.true
                                        ;   in Loop: Header=BB0_36 Depth=1
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r4, r8
	STP	p2, [p7 + 32]                   ; 4-byte Folded Spill
	JAL	__divsf3
	BRA	.LBB0_39
.LBB0_40:
	MOVP	p5, p1
	MOVP	p4, p2
.LBB0_41:                               ; %if.end80
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
.LBB0_42:                               ; %if.end80
	MOVI	r2, 0
	MOV	r3, r2
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB0_44
; %bb.43:                               ; %if.then82
	STP	p4, [p6]
.LBB0_44:                               ; %if.end83
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	ANDI	r5, r7, 0
	ANDI	r4, r6, 1
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB0_45
; %bb.46:                               ; %if.end83
	XORI	r3, r11, -32768
	XORI	r2, r10, 0
	BRA	.LBB0_47
.LBB0_45:
	MOVP	p1, p5
.LBB0_47:                               ; %cleanup
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 44]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 40]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 48
	RET
.LBB0_34:
	MOVP	p4, p2
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 36]                   ; 4-byte Folded Reload
	BRA	.LBB0_42
                                        ; -- End function
	.globl	strtod                          ; -- Begin function strtod
	.p2align	1
strtod:                                 ; @strtod
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p6
	JAL	strtof
	MOVP	p6, p4
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	atof                            ; -- Begin function atof
	.p2align	1
atof:                                   ; @atof
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p6
	MOVI	r4, 0
	MOV	r5, r4
	JAL	strtof
	MOVP	p6, p4
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	getenv                          ; -- Begin function getenv
	.p2align	1
getenv:                                 ; @getenv
; %bb.0:                                ; %entry
	MOVI	r2, 0
	MOV	r3, r2
	RET
                                        ; -- End function
	.globl	system                          ; -- Begin function system
	.p2align	1
system:                                 ; @system
; %bb.0:                                ; %entry
	MOVP	p2, p1
	MOVI	r6, 0
	MOV	r7, r6
	MOVI	r2, -1
	MOV	r3, r2
	CMP	r4, r6
	CPC	r5, r7
	BNE	.LBB4_2
; %bb.1:                                ; %entry
	MOVP	p1, p3
.LBB4_2:                                ; %entry
	RET
                                        ; -- End function
	.globl	__assert_fail                   ; -- Begin function __assert_fail
	.p2align	1
__assert_fail:                          ; @__assert_fail
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p3, [p7 + 12]
	STP	p2, [p7 + 8]
	STP	p1, [p7 + 4]
	LEA	p1, [.L.str]
	STP	p1, [p7]
	JAL	printf
	JAL	abort
                                        ; -- End function
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:                                 ; @.str
	.asciz	"assertion failed: %s, %s line %d\n"

	.section	".note.GNU-stack","",@progbits
	.addrsig
