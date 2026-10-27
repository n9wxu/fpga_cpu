	.text
	.globl	fabsf                           ; -- Begin function fabsf
	.p2align	1
fabsf:                                  ; @fabsf
; %bb.0:                                ; %entry
	ANDI	r5, r3, 32767
	ANDI	r4, r2, -1
	MOVP	p1, p2
	RET
                                        ; -- End function
	.globl	truncf                          ; -- Begin function truncf
	.p2align	1
truncf:                                 ; @truncf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	SHRI	r4, r3, 7
	MOVI	r5, 0
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	MOVI	r8, 149
	MOV	r9, r5
	CMP	r6, r8
	CPC	r7, r9
	BHI	.LBB1_4
; %bb.1:                                ; %if.end
	MOVI	r4, 126
	CMP	r6, r4
	CPC	r7, r5
	BHI	.LBB1_3
; %bb.2:                                ; %if.then2
	ANDI	r3, r3, -32768
	ANDI	r2, r2, 0
	BRA	.LBB1_4
.LBB1_3:                                ; %if.end5
	ADDPI	p2, p3, -127
	MOVI	r7, -128
	MOVI	r6, 0
	MOVP	p4, p1
	MOVP	p1, p3
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p5, p6
	JAL	__ashrsi3
	MOVP	p6, p5
	MOVP	p2, p1
	MOVP	p1, p4
	AND	r3, r5, r3
	AND	r2, r4, r2
.LBB1_4:                                ; %cleanup
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	floorf                          ; -- Begin function floorf
	.p2align	1
floorf:                                 ; @floorf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p3, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	SHRI	r4, r7, 7
	MOVI	r10, 0
	MOV	r5, r10
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	MOVI	r4, 149
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	MOVP	p4, p3
	BHI	.LBB2_4
; %bb.1:                                ; %if.end.i
	MOVI	r4, 126
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB2_3
; %bb.2:                                ; %if.then2.i
	ANDI	r9, r7, -32768
	ANDI	r8, r6, 0
	BRA	.LBB2_4
.LBB2_3:                                ; %if.end5.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p4, p3
	JAL	__ashrsi3
	MOVP	p3, p4
	AND	r9, r3, r7
	AND	r8, r2, r6
.LBB2_4:                                ; %truncf.exit
	MOVP	p1, p4
	MOVP	p2, p3
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, -16512
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__addsf3
	MOV	r11, r10
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	BGT	.LBB2_6
; %bb.5:                                ; %truncf.exit
	MOVP	p1, p4
.LBB2_6:                                ; %truncf.exit
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	ceilf                           ; -- Begin function ceilf
	.p2align	1
ceilf:                                  ; @ceilf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p3, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	SHRI	r4, r7, 7
	MOVI	r10, 0
	MOV	r5, r10
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	MOVI	r4, 149
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	MOVP	p4, p3
	BHI	.LBB3_4
; %bb.1:                                ; %if.end.i
	MOVI	r4, 126
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB3_3
; %bb.2:                                ; %if.then2.i
	ANDI	r9, r7, -32768
	ANDI	r8, r6, 0
	BRA	.LBB3_4
.LBB3_3:                                ; %if.end5.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p4, p3
	JAL	__ashrsi3
	MOVP	p3, p4
	AND	r9, r3, r7
	AND	r8, r2, r6
.LBB3_4:                                ; %truncf.exit
	MOVP	p1, p4
	MOVP	p2, p3
	JAL	__ltsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, 16256
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__addsf3
	MOV	r11, r10
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	BLT	.LBB3_6
; %bb.5:                                ; %truncf.exit
	MOVP	p1, p4
.LBB3_6:                                ; %truncf.exit
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	roundf                          ; -- Begin function roundf
	.p2align	1
roundf:                                 ; @roundf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	MOVP	p2, p4
	JAL	__ltsf2
	MOVI	r4, -1
	MOV	r5, r4
	MOVI	r9, 16128
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB4_8
; %bb.1:                                ; %cond.true
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__subsf3
	MOVP	p5, p1
	SHRI	r4, r11, 7
	MOV	r5, r8
	MOVI	r6, 149
	MOV	r7, r8
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p4, p5
	BHI	.LBB4_5
; %bb.2:                                ; %if.end.i.i
	MOVI	r7, 0
	MOVI	r6, 126
	CMP	r2, r6
	CPC	r3, r7
	BHI	.LBB4_4
; %bb.3:                                ; %if.then2.i.i
	ANDI	r9, r11, -32768
	ANDI	r8, r10, 0
	BRA	.LBB4_5
.LBB4_8:                                ; %cond.false
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p5, p1
	SHRI	r4, r11, 7
	MOV	r5, r8
	MOVI	r6, 149
	MOV	r7, r8
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p4, p5
	BHI	.LBB4_12
; %bb.9:                                ; %if.end.i.i8
	MOVI	r7, 0
	MOVI	r6, 126
	CMP	r2, r6
	CPC	r3, r7
	BHI	.LBB4_11
; %bb.10:                               ; %if.then2.i.i17
	ANDI	r9, r11, -32768
	ANDI	r8, r10, 0
	BRA	.LBB4_12
.LBB4_4:                                ; %if.end5.i.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOV	r2, r7
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__ashrsi3
	AND	r9, r3, r11
	AND	r8, r2, r10
.LBB4_5:                                ; %floorf.exit
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, -16512
	MOVI	r10, 0
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOV	r11, r10
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB4_7
; %bb.6:                                ; %floorf.exit
	MOVP	p2, p4
.LBB4_7:                                ; %floorf.exit
	XORI	r3, r5, -32768
	XORI	r2, r4, 0
	BRA	.LBB4_14
.LBB4_11:                               ; %if.end5.i.i10
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOV	r2, r7
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__ashrsi3
	AND	r9, r3, r11
	AND	r8, r2, r10
.LBB4_12:                               ; %floorf.exit19
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, -16512
	MOVI	r10, 0
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__addsf3
	MOV	r11, r10
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	BGT	.LBB4_14
; %bb.13:                               ; %floorf.exit19
	MOVP	p1, p4
.LBB4_14:                               ; %cond.end
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	fmodf                           ; -- Begin function fmodf
	.p2align	1
fmodf:                                  ; @fmodf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p1, p2
	MOVP	p2, p5
	JAL	__unordsf2
	MOVI	r5, 32704
	MOVI	r4, 0
	MOV	r8, r4
	MOV	r9, r4
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB5_11
; %bb.1:                                ; %entry
	STP	p5, [p7]                        ; 4-byte Folded Spill
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p5, p2
	MOVP	p2, p4
	JAL	__eqsf2
	MOVP	p2, p5
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB5_11
; %bb.2:                                ; %if.end
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ANDI	r3, r11, 32767
	ANDI	r2, r10, -1
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	ANDI	r5, r7, 32767
	ANDI	r4, r6, -1
	STP	p1, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	JAL	__ltsf2
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BGE	.LBB5_4
; %bb.3:
	MOVP	p2, p5
	BRA	.LBB5_11
.LBB5_4:                                ; %while.cond.preheader
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p5, p1
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__gesf2
	CMP	r2, r8
	CPC	r3, r9
	BLT	.LBB5_9
; %bb.5:                                ; %while.cond9.preheader.preheader
	MOVI	r3, 0
	MOVI	r2, 1
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r4, -1
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
.LBB5_6:                                ; %while.cond9.preheader
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB5_7 Depth 2
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
.LBB5_7:                                ; %while.cond9
                                        ;   Parent Loop BB5_6 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p5, p4
	MOVP	p1, p5
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p4, p1
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	JAL	__lesf2
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB5_7
; %bb.8:                                ; %while.end
                                        ;   in Loop: Header=BB5_6 Depth=1
	LDP	p1, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__subsf3
	MOVP	p5, p1
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__gesf2
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB5_6
.LBB5_9:                                ; %while.end13
	MOVI	r8, 0
	MOV	r9, r8
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__ltsf2
	MOVP	p2, p5
	CMP	r2, r8
	CPC	r3, r9
	BGE	.LBB5_11
; %bb.10:
	XORI	r5, r5, -32768
	XORI	r4, r4, 0
.LBB5_11:                               ; %return
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p2
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.globl	ldexpf                          ; -- Begin function ldexpf
	.p2align	1
ldexpf:                                 ; @ldexpf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	SHRI	r6, r3, 7
	MOVI	r7, 0
	ANDI	r8, r6, 255
	CMPI	r8, 0
	BEQ	.LBB6_7
; %bb.1:                                ; %entry
	CMPI	r8, 255
	BEQ	.LBB6_7
; %bb.2:                                ; %if.end
	ANDI	r9, r7, 0
	ANDI	r8, r6, 255
	ADDPP	p2, p4, p2
	MOVI	r7, 0
	MOVI	r6, 255
	CMP	r4, r6
	CPC	r5, r7
	BLT	.LBB6_4
; %bb.3:                                ; %if.then3
	ANDI	r5, r3, -32768
	ANDI	r4, r2, 0
	ORI	r3, r5, 32640
	ORI	r2, r4, 0
	BRA	.LBB6_7
.LBB6_4:                                ; %if.end5
	MOV	r6, r7
	CMP	r4, r6
	CPC	r5, r7
	BLE	.LBB6_5
; %bb.6:                                ; %if.end10
	SHLI	r5, r4, 7
	MOVI	r4, 0
	ANDI	r7, r3, -32641
	ANDI	r6, r2, -1
	OR	r3, r5, r7
	OR	r2, r4, r6
	BRA	.LBB6_7
.LBB6_5:                                ; %if.then7
	ANDI	r3, r3, -32768
	ANDI	r2, r2, 0
.LBB6_7:                                ; %cleanup
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	frexpf                          ; -- Begin function frexpf
	.p2align	1
frexpf:                                 ; @frexpf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	SHRI	r6, r3, 7
	MOVI	r8, 0
	MOV	r7, r8
	ANDI	r10, r6, 255
	CMPI	r10, 255
	BEQ	.LBB7_2
; %bb.1:                                ; %entry
	CMPI	r10, 0
	BNE	.LBB7_3
.LBB7_2:                                ; %if.then
	MOV	r9, r8
	STP	p4, [p2]
	BRA	.LBB7_4
.LBB7_3:                                ; %if.end
	ANDI	r9, r7, 0
	ANDI	r8, r6, 255
	ADDPI	p3, p4, -126
	STP	p3, [p2]
	ANDI	r5, r3, -32641
	ANDI	r4, r2, -1
	ORI	r3, r5, 16128
	ORI	r2, r4, 0
.LBB7_4:                                ; %cleanup
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	sqrtf                           ; -- Begin function sqrtf
	.p2align	1
sqrtf:                                  ; @sqrtf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r8, 0
	MOV	r9, r8
	MOVP	p2, p4
	JAL	__lesf2
	CMP	r2, r8
	CPC	r3, r9
	BLE	.LBB8_1
; %bb.3:                                ; %if.end
	MOVP	p1, p5
	MOVP	p2, p5
	JAL	__unordsf2
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB8_6
; %bb.4:                                ; %if.end
	MOVI	r5, 32639
	MOVI	r4, -13922
	MOVP	p1, p5
	JAL	__gtsf2
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB8_6
; %bb.5:                                ; %if.end5
	MOVI	r3, 8125
	MOVI	r2, 7669
	SHRI	r4, r10, 1
	SHLI	r5, r11, 15
	OR	r4, r4, r5
	SHRI	r5, r11, 1
	ADDPP	p2, p2, p1
	STP	p2, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p5
	JAL	__divsf3
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	JAL	__addsf3
	MOVI	r9, 16128
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p2, p1
	STP	p2, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p5
	JAL	__divsf3
	MOVP	p2, p1
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p5
	MOVP	p5, p2
	JAL	__divsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__addsf3
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p5, p1
	BRA	.LBB8_6
.LBB8_1:                                ; %if.then
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__eqsf2
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB8_6
; %bb.2:                                ; %if.then
	MOVI	r9, 32704
	MOVP	p5, p4
.LBB8_6:                                ; %return
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	expf                            ; -- Begin function expf
	.p2align	1
expf:                                   ; @expf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -24
	STP	p4, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__unordsf2
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB9_1
; %bb.2:                                ; %if.end
	MOVI	r5, 17073
	MOVI	r4, 28836
	MOVP	p1, p4
	JAL	__gtsf2
	CMP	r2, r10
	CPC	r3, r11
	BLE	.LBB9_4
; %bb.3:
	MOVI	r11, 32640
	BRA	.LBB9_25
.LBB9_4:                                ; %if.end3
	MOVI	r5, -15665
	MOVI	r4, -13107
	MOVP	p1, p4
	JAL	__ltsf2
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r2, r10
	CPC	r3, r11
	BLT	.LBB9_25
; %bb.5:                                ; %if.end6
	MOVI	r5, 16312
	MOVI	r4, -21957
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, 16128
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p6, p1
	SHRI	r4, r13, 7
	MOV	r5, r10
	MOVI	r6, 149
	MOV	r7, r10
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p2, p6
	BHI	.LBB9_9
; %bb.6:                                ; %if.end.i.i
	MOVI	r7, 0
	MOVI	r6, 126
	CMP	r2, r6
	CPC	r3, r7
	BHI	.LBB9_8
; %bb.7:                                ; %if.then2.i.i
	ANDI	r5, r13, -32768
	ANDI	r4, r12, 0
	BRA	.LBB9_9
.LBB9_8:                                ; %if.end5.i.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOV	r2, r7
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p5, p6
	JAL	__ashrsi3
	MOVP	p6, p5
	AND	r5, r3, r13
	AND	r4, r2, r12
.LBB9_9:                                ; %floorf.exit
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p2
	MOVP	p2, p6
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r11, -16512
	MOVI	r10, 0
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__addsf3
	MOV	r4, r10
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOV	r5, r10
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BGT	.LBB9_11
; %bb.10:                               ; %floorf.exit
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
.LBB9_11:                               ; %floorf.exit
	MOVI	r5, -16591
	MOVI	r4, 29184
	MOVP	p5, p1
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__addsf3
	MOVP	p4, p1
	MOVI	r5, -19009
	MOVI	r4, -16754
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__addsf3
	MOVP	p4, p1
	MOVI	r5, 14672
	MOVI	r4, 3329
	JAL	__mulsf3
	MOVI	r5, 15030
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, 15658
	MOVI	r10, -21845
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, 15914
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	MOVI	r11, 16128
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, 16256
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p5, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__fixsfsi
	MOVI	r5, -1
	MOVI	r4, -126
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB9_18
; %bb.12:                               ; %if.then10
	SHRI	r4, r11, 7
	MOVI	r5, 0
	ANDI	r6, r4, 255
	CMPI	r6, 0
	BEQ	.LBB9_17
; %bb.13:                               ; %if.then10
	CMPI	r6, 255
	BEQ	.LBB9_17
; %bb.14:                               ; %if.end.i
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	MOVI	r5, 0
	MOVI	r4, 60
	CMP	r6, r4
	CPC	r7, r5
	BHI	.LBB9_16
; %bb.15:                               ; %if.then7.i
	ANDI	r11, r11, -32768
	ANDI	r10, r10, 0
	BRA	.LBB9_17
.LBB9_16:                               ; %if.end10.i
	ANDI	r13, r11, -32641
	ANDI	r12, r10, -1
	SHLI	r7, r6, 7
	MOV	r6, r5
	OR	r9, r7, r13
	OR	r8, r6, r12
	MOVI	r7, -7680
	MOV	r6, r5
	ADDPP	p5, p4, p3
.LBB9_17:                               ; %ldexpf.exit
	ADDPI	p1, p1, 60
.LBB9_18:                               ; %if.end12
	SHRI	r4, r11, 7
	MOVI	r5, 0
	ANDI	r6, r4, 255
	CMPI	r6, 0
	BEQ	.LBB9_25
; %bb.19:                               ; %if.end12
	CMPI	r6, 255
	BEQ	.LBB9_25
; %bb.20:                               ; %if.end.i35
	ANDI	r7, r5, 0
	ANDI	r6, r4, 255
	ADDPP	p1, p3, p1
	MOVI	r5, 0
	MOVI	r4, 255
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB9_22
; %bb.21:                               ; %if.then3.i47
	ANDI	r3, r11, -32768
	ANDI	r2, r10, 0
	ORI	r11, r3, 32640
	ORI	r10, r2, 0
	BRA	.LBB9_25
.LBB9_22:                               ; %if.end5.i39
	MOV	r4, r5
	CMP	r2, r4
	CPC	r3, r5
	BLE	.LBB9_23
; %bb.24:                               ; %if.end10.i41
	SHLI	r3, r2, 7
	MOVI	r2, 0
	ANDI	r5, r11, -32641
	ANDI	r4, r10, -1
	OR	r11, r3, r5
	OR	r10, r2, r4
	BRA	.LBB9_25
.LBB9_23:                               ; %if.then7.i45
	ANDI	r11, r11, -32768
	ANDI	r10, r10, 0
.LBB9_25:                               ; %return
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 24
	RET
.LBB9_1:
	MOVP	p5, p4
	BRA	.LBB9_25
                                        ; -- End function
	.globl	logf                            ; -- Begin function logf
	.p2align	1
logf:                                   ; @logf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r10, 0
	MOV	r11, r10
	MOVP	p2, p5
	JAL	__gesf2
	CMP	r2, r10
	CPC	r3, r11
	BGE	.LBB10_2
; %bb.1:
	MOVI	r11, 32704
	BRA	.LBB10_17
.LBB10_2:                               ; %if.end
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__eqsf2
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB10_4
; %bb.3:
	MOVI	r11, -128
	BRA	.LBB10_17
.LBB10_4:                               ; %if.end4
	MOVI	r5, 32639
	MOVI	r4, -13922
	MOVP	p1, p4
	JAL	__gtsf2
	MOVI	r6, 0
	MOV	r7, r6
	CMP	r2, r6
	CPC	r3, r7
	BLE	.LBB10_6
; %bb.5:
	MOVP	p5, p4
	BRA	.LBB10_17
.LBB10_6:                               ; %if.end7
	SHRI	r2, r9, 7
	MOV	r3, r6
	ANDI	r4, r2, 255
	CMPI	r4, 0
	BEQ	.LBB10_9
; %bb.7:                                ; %if.end7
	CMPI	r4, 255
	BNE	.LBB10_10
.LBB10_9:
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	BRA	.LBB10_11
.LBB10_10:                              ; %if.end.i
	ANDI	r5, r9, 127
	ANDI	r4, r8, -1
	ORI	r9, r5, 16128
	ORI	r8, r4, 0
	ADDPI	p1, p1, -126
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
.LBB10_11:                              ; %frexpf.exit
	MOVI	r5, 16181
	MOVI	r4, 1267
	MOVP	p1, p4
	JAL	__ltsf2
	MOVP	p5, p1
	MOVP	p1, p4
	MOVP	p2, p4
	JAL	__addsf3
	MOVI	r6, 0
	MOV	r7, r6
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	CMP	r10, r6
	CPC	r11, r7
	BLT	.LBB10_13
; %bb.12:                               ; %frexpf.exit
	MOVP	p1, p4
.LBB10_13:                              ; %frexpf.exit
	MOVI	r5, -16512
	MOV	r4, r6
	MOVP	p4, p1
	STP	p3, [p7 + 12]                   ; 4-byte Folded Spill
	JAL	__addsf3
	MOVP	p5, p1
	MOVI	r5, 16256
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	MOV	r4, r2
	MOVP	p1, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__divsf3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__mulsf3
	MOVP	p5, p1
	MOVI	r5, 15843
	MOVI	r4, -29127
	JAL	__mulsf3
	MOVI	r5, 15890
	MOVI	r4, 18725
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, 15948
	MOVI	r4, -13107
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, 16042
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p4, p1
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	MOVP	p2, p1
	JAL	__addsf3
	MOVP	p5, p1
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__addsf3
	LDP	p2, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p4, p1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB10_15
; %bb.14:                               ; %frexpf.exit
	MOVP	p1, p2
	BRA	.LBB10_16
.LBB10_15:
	MOVI	r2, 1
	MOV	r3, r4
.LBB10_16:                              ; %frexpf.exit
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	JAL	__floatsisf
	MOVP	p5, p1
	MOVI	r5, 13759
	MOVI	r4, -16754
	JAL	__mulsf3
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p4, p1
	MOVI	r5, 16177
	MOVI	r4, 29184
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p5, p1
.LBB10_17:                              ; %return
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	log10f                          ; -- Begin function log10f
	.p2align	1
log10f:                                 ; @log10f
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -4
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p4, p6
	JAL	logf
	MOVI	r5, 16094
	MOVI	r4, 23513
	JAL	__mulsf3
	MOVP	p6, p4
	LDP	p4, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 4
	RET
                                        ; -- End function
	.globl	powf                            ; -- Begin function powf
	.p2align	1
powf:                                   ; @powf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -44
	STP	p4, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	MOVI	r9, 16256
	MOVI	r8, 0
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__eqsf2
	MOV	r10, r8
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	MOV	r11, r8
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB12_6
; %bb.1:                                ; %entry
	LDP	p1, [p7 + 32]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__eqsf2
	CMP	r2, r10
	CPC	r3, r11
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	BEQ	.LBB12_25
; %bb.2:                                ; %if.end3
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 32]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__unordsf2
	MOVI	r8, 0
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB12_33
; %bb.3:                                ; %if.end7
	SHRI	r4, r11, 7
	MOV	r5, r8
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	MOVI	r4, 149
	MOV	r5, r8
	CMP	r2, r4
	CPC	r3, r5
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	BHI	.LBB12_8
; %bb.4:                                ; %if.end.i
	MOVI	r7, 0
	MOVI	r6, 126
	CMP	r2, r6
	CPC	r3, r7
	BHI	.LBB12_7
; %bb.5:                                ; %if.then2.i
	ANDI	r3, r11, -32768
	ANDI	r2, r10, 0
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	BRA	.LBB12_8
.LBB12_7:                               ; %if.end5.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOV	r2, r7
                                        ; kill: def $r4 killed $r4 killed $p2
	JAL	__ashrsi3
	AND	r5, r3, r11
	AND	r4, r2, r10
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
.LBB12_8:                               ; %truncf.exit
	ANDI	r3, r11, 32767
	ANDI	r2, r10, -1
	MOVI	r9, 17024
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__lesf2
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB12_15
; %bb.9:                                ; %truncf.exit
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__nesf2
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB12_15
; %bb.10:                               ; %if.then12
	MOVI	r9, 16256
	MOVI	r8, 0
	MOVP	p1, p5
	JAL	__fixsfsi
	MOVI	r4, 0
	MOVI	r6, 0
	MOV	r5, r6
	STP	p2, [p7]                        ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	BEQ	.LBB12_21
; %bb.11:                               ; %while.body.preheader
	SARI	r6, r3, 15
	MOV	r7, r6
	XOR	r5, r3, r7
	XOR	r4, r2, r6
	SUB	r12, r4, r6
	SBC	r13, r5, r7
	MOVI	r10, 1
	MOVP	p1, p4
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	MOVI	r8, 0
	BRA	.LBB12_13
.LBB12_12:                              ; %while.body
                                        ;   in Loop: Header=BB12_13 Depth=1
	LDP	p4, [p7 + 20]                   ; 4-byte Folded Reload
	SHRI	r2, r8, 1
	SHLI	r3, r9, 15
	OR	r2, r2, r3
	SHRI	r3, r9, 1
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	MOV	r11, r4
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__mulsf3
	MOVP	p2, p1
	CMP	r8, r10
	CPC	r9, r11
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 32]                   ; 4-byte Folded Reload
	BLS	.LBB12_22
.LBB12_13:                              ; %while.body
                                        ; =>This Inner Loop Header: Depth=1
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	ANDI	r11, r13, 0
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	ANDI	r10, r12, 1
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	JAL	__mulsf3
	MOV	r9, r8
	CMP	r10, r8
	CPC	r11, r9
	MOVP	p2, p4
	BEQ	.LBB12_12
; %bb.14:                               ; %while.body
                                        ;   in Loop: Header=BB12_13 Depth=1
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	BRA	.LBB12_12
.LBB12_15:                              ; %if.end25
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__nesf2
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB12_18
; %bb.16:                               ; %if.then28
	MOVI	r8, 0
	MOV	r9, r8
	LDP	p1, [p7 + 32]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__gtsf2
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB12_25
; %bb.17:                               ; %if.then28
	MOVI	r9, 32640
	BRA	.LBB12_25
.LBB12_18:                              ; %if.end32
	MOVI	r8, 0
	MOV	r9, r8
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__ltsf2
	MOVI	r4, -1
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	BGT	.LBB12_24
; %bb.19:                               ; %if.then35
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p5, p2
	JAL	__nesf2
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB12_26
.LBB12_33:
	MOVI	r9, 32704
	BRA	.LBB12_25
.LBB12_21:
	MOVP	p1, p4
.LBB12_22:                              ; %while.end
	MOVP	p2, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p5, p2
	JAL	__divsf3
	MOVP	p4, p1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB12_25
; %bb.23:                               ; %while.end
	MOVP	p4, p5
	BRA	.LBB12_25
.LBB12_24:                              ; %if.end52
	MOVP	p1, p5
	MOVP	p4, p2
	JAL	logf
	MOVP	p2, p4
	JAL	__mulsf3
	JAL	expf
	MOVP	p4, p1
	BRA	.LBB12_25
.LBB12_26:                              ; %if.end38
	MOVI	r9, 16128
	MOVI	r8, 0
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p3, p1
	STP	p3, [p7 + 20]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 28]                   ; 4-byte Folded Reload
	XORI	r3, r5, -32768
	XORI	r2, r4, 0
	SHRI	r4, r7, 7
	MOV	r5, r8
	MOVI	r6, 149
	MOV	r7, r8
	STP	p3, [p7 + 28]                   ; 4-byte Folded Spill
	ANDI	r11, r5, 0
	ANDI	r10, r4, 255
	JAL	logf
	LDP	p2, [p7 + 32]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	JAL	expf
	LDP	p3, [p7 + 20]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	CMP	r10, r2
	CPC	r11, r3
	MOVP	p1, p3
	BHI	.LBB12_30
; %bb.27:                               ; %if.end.i92
	MOVI	r2, 126
	MOV	r3, r8
	CMP	r10, r2
	CPC	r11, r3
	BHI	.LBB12_29
; %bb.28:                               ; %if.then2.i98
	ANDI	r3, r7, -32768
	ANDI	r2, r6, 0
	BRA	.LBB12_30
.LBB12_29:                              ; %if.end5.i94
	ADDPI	p2, p5, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p5, p3
	JAL	__ashrsi3
	MOVP	p3, p5
	MOVP	p2, p1
	AND	r3, r5, r7
	AND	r2, r4, r6
.LBB12_30:                              ; %truncf.exit100
	MOVP	p2, p3
	JAL	__nesf2
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB12_32
.LBB12_6:
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
.LBB12_25:                              ; %return
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 40]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 36]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 44
	RET
.LBB12_32:
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	XORI	r9, r9, -32768
	XORI	r8, r8, 0
	BRA	.LBB12_25
                                        ; -- End function
	.globl	sinf                            ; -- Begin function sinf
	.p2align	1
sinf:                                   ; @sinf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	MOVI	r9, 32704
	MOVI	r8, 0
	MOV	r10, r8
	MOV	r11, r8
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB13_17
; %bb.1:                                ; %entry
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r5, 32639
	MOVI	r4, -13922
	JAL	__gtsf2
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB13_17
; %bb.2:                                ; %if.end
	MOVI	r5, 16162
	MOVI	r4, -1661
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	MOVI	r9, 16128
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p6, p1
	SHRI	r4, r13, 7
	MOV	r5, r8
	MOVI	r6, 149
	MOV	r7, r8
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p5, p6
	BHI	.LBB13_6
; %bb.3:                                ; %if.end.i.i.i
	MOVI	r4, 126
	MOV	r5, r8
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB13_5
; %bb.4:                                ; %if.then2.i.i.i
	ANDI	r11, r13, -32768
	ANDI	r10, r12, 0
	BRA	.LBB13_6
.LBB13_5:                               ; %if.end5.i.i.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p5, p6
	JAL	__ashrsi3
	MOVP	p6, p5
	AND	r11, r3, r13
	AND	r10, r2, r12
.LBB13_6:                               ; %reduce.exit
	MOVP	p1, p5
	MOVP	p2, p6
	JAL	__gtsf2
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r9, -16512
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p5, p1
	MOV	r2, r8
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOV	r3, r8
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BGT	.LBB13_8
; %bb.7:                                ; %reduce.exit
	LDP	p5, [p7]                        ; 4-byte Folded Reload
.LBB13_8:                               ; %reduce.exit
	MOVP	p1, p5
	JAL	__fixsfsi
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r5, -16439
	MOVI	r4, 4059
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p4, p1
	MOVI	r5, 13115
	MOVI	r4, -17106
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__addsf3
	MOVP	p5, p1
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	ANDI	r9, r3, 0
	ANDI	r8, r2, 3
	MOVP	p1, p5
	MOVP	p2, p5
	JAL	__mulsf3
	MOVI	r4, 2
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r5, r6
	CMP	r8, r4
	CPC	r9, r5
	BEQ	.LBB13_14
; %bb.9:                                ; %reduce.exit
	MOVP	p2, p5
	MOVI	r11, 0
	MOVI	r10, 1
	CMP	r8, r10
	CPC	r9, r11
	BEQ	.LBB13_12
; %bb.10:                               ; %reduce.exit
	MOV	r10, r11
	CMP	r8, r10
	CPC	r9, r11
	BNE	.LBB13_15
; %bb.11:                               ; %sw.bb
	MOVP	p4, p1
	MOVP	p5, p2
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r5, 13880
	MOVI	r4, -4323
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -18096
	MOVI	r4, 3329
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16854
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	MOVP	p2, p5
	BRA	.LBB13_13
.LBB13_12:                              ; %sw.bb4
	MOVI	r5, 14288
	MOVI	r4, 3329
	MOVP	p4, p1
	JAL	__mulsf3
	MOVI	r5, -17738
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15658
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16640
	MOV	r4, r11
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 16256
	MOV	r4, r11
.LBB13_13:                              ; %return
	JAL	__addsf3
	MOVP	p4, p1
	BRA	.LBB13_17
.LBB13_14:                              ; %sw.bb6
	MOVP	p4, p1
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r5, 13880
	MOVI	r4, -4323
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -18096
	MOVI	r4, 3329
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16854
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	BRA	.LBB13_16
.LBB13_15:                              ; %sw.default
	MOVI	r5, 14288
	MOVI	r4, 3329
	MOVP	p4, p1
	JAL	__mulsf3
	MOVI	r5, -17738
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15658
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, -16640
	MOVI	r10, 0
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, 16256
.LBB13_16:                              ; %return
	MOVP	p2, p5
	JAL	__addsf3
	XORI	r9, r3, -32768
	XORI	r8, r2, 0
.LBB13_17:                              ; %return
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	cosf                            ; -- Begin function cosf
	.p2align	1
cosf:                                   ; @cosf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	MOVI	r11, 32704
	MOVI	r10, 0
	MOV	r8, r10
	MOV	r9, r10
	CMP	r2, r8
	CPC	r3, r9
	BNE	.LBB14_17
; %bb.1:                                ; %entry
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r5, 32639
	MOVI	r4, -13922
	JAL	__gtsf2
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB14_17
; %bb.2:                                ; %if.end
	MOVI	r5, 16162
	MOVI	r4, -1661
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	MOVI	r11, 16128
	MOVI	r10, 0
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p6, p1
	SHRI	r4, r13, 7
	MOV	r5, r10
	MOVI	r6, 149
	MOV	r7, r10
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p4, p6
	BHI	.LBB14_6
; %bb.3:                                ; %if.end.i.i.i
	MOVI	r4, 126
	MOV	r5, r10
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB14_5
; %bb.4:                                ; %if.then2.i.i.i
	ANDI	r9, r13, -32768
	ANDI	r8, r12, 0
	BRA	.LBB14_6
.LBB14_5:                               ; %if.end5.i.i.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p4, p6
	JAL	__ashrsi3
	MOVP	p6, p4
	AND	r9, r3, r13
	AND	r8, r2, r12
.LBB14_6:                               ; %reduce.exit
	MOVP	p1, p4
	MOVP	p2, p6
	JAL	__gtsf2
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r11, -16512
	STP	p4, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p4, p1
	MOV	r2, r10
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOV	r3, r10
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BGT	.LBB14_8
; %bb.7:                                ; %reduce.exit
	LDP	p4, [p7]                        ; 4-byte Folded Reload
.LBB14_8:                               ; %reduce.exit
	MOVP	p1, p4
	JAL	__fixsfsi
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r5, -16439
	MOVI	r4, 4059
	MOVP	p1, p4
	JAL	__mulsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p5, p1
	MOVI	r5, 13115
	MOVI	r4, -17106
	MOVP	p1, p4
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__addsf3
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	ANDI	r9, r5, 0
	ANDI	r8, r4, 3
	MOVP	p5, p1
	MOVP	p2, p1
	JAL	__mulsf3
	MOVP	p3, p1
	MOVI	r2, 2
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	MOV	r3, r4
	CMP	r8, r2
	CPC	r9, r3
	BEQ	.LBB14_13
; %bb.9:                                ; %reduce.exit
	MOVI	r3, 0
	MOVI	r2, 1
	CMP	r8, r2
	CPC	r9, r3
	BEQ	.LBB14_12
; %bb.10:                               ; %reduce.exit
	MOV	r2, r3
	CMP	r8, r2
	CPC	r9, r3
	BNE	.LBB14_15
; %bb.11:                               ; %sw.bb
	MOVI	r5, 14288
	MOVI	r4, 3329
	MOVP	p5, p3
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, -17738
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, 15658
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, -16640
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, 16256
	MOVP	p2, p4
	BRA	.LBB14_16
.LBB14_12:                              ; %sw.bb4
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p4, p3
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p5, p1
	MOVI	r5, 13880
	MOVI	r4, -4323
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -18096
	MOVI	r4, 3329
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16854
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	BRA	.LBB14_14
.LBB14_13:                              ; %sw.bb6
	MOVI	r5, 14288
	MOVI	r4, 3329
	MOVP	p5, p3
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, -17738
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r5, 15658
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, -16640
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, 16256
	MOVP	p2, p4
.LBB14_14:                              ; %return
	JAL	__addsf3
	XORI	r11, r3, -32768
	XORI	r10, r2, 0
	BRA	.LBB14_17
.LBB14_15:                              ; %sw.default
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p4, p3
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p5, p1
	MOVI	r5, 13880
	MOVI	r4, -4323
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -18096
	MOVI	r4, 3329
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16854
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
.LBB14_16:                              ; %return
	JAL	__addsf3
	MOVP	p5, p1
.LBB14_17:                              ; %return
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
                                        ; -- End function
	.globl	tanf                            ; -- Begin function tanf
	.p2align	1
tanf:                                   ; @tanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -36
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	MOVI	r9, 32704
	MOVI	r8, 0
	MOV	r10, r8
	MOV	r11, r8
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB15_12
; %bb.1:                                ; %entry
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r5, 32639
	MOVI	r4, -13922
	JAL	__gtsf2
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB15_12
; %bb.2:                                ; %if.end
	MOVI	r5, 16162
	MOVI	r4, -1661
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__mulsf3
	MOVI	r9, 16128
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p6, p1
	SHRI	r4, r13, 7
	MOV	r5, r8
	MOVI	r6, 149
	MOV	r7, r8
	ANDI	r3, r5, 0
	ANDI	r2, r4, 255
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p5, p6
	BHI	.LBB15_6
; %bb.3:                                ; %if.end.i.i.i
	MOVI	r4, 126
	MOV	r5, r8
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB15_5
; %bb.4:                                ; %if.then2.i.i.i
	ANDI	r11, r13, -32768
	ANDI	r10, r12, 0
	BRA	.LBB15_6
.LBB15_5:                               ; %if.end5.i.i.i
	ADDPI	p2, p1, -127
	MOVI	r3, -128
	MOVI	r2, 0
                                        ; kill: def $r4 killed $r4 killed $p2
	MOVP	p5, p6
	JAL	__ashrsi3
	MOVP	p6, p5
	AND	r11, r3, r13
	AND	r10, r2, r12
.LBB15_6:                               ; %reduce.exit
	MOVP	p1, p5
	MOVP	p2, p6
	JAL	__gtsf2
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r9, -16512
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOV	r4, r8
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	MOV	r5, r8
	STP	p2, [p7 + 12]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 8]                    ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BGT	.LBB15_8
; %bb.7:                                ; %reduce.exit
	MOVP	p1, p5
.LBB15_8:                               ; %reduce.exit
	MOVI	r5, -16439
	MOVI	r4, 4059
	MOVP	p5, p1
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	JAL	__mulsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p4, p1
	MOVI	r5, 13115
	MOVI	r4, -17106
	MOVP	p1, p5
	JAL	__mulsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__addsf3
	MOVP	p5, p1
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__mulsf3
	MOVP	p4, p1
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVI	r5, 13880
	MOVI	r4, -4323
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r11, -18096
	MOVI	r10, 3329
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	JAL	__mulsf3
	MOVI	r5, 15368
	MOVI	r4, -30583
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r9, -16854
	MOVI	r8, -21845
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	JAL	__mulsf3
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
	JAL	__addsf3
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r11, 14288
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__mulsf3
	MOVI	r5, -17738
	MOVI	r4, 2913
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, 15658
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	MOVI	r9, -16640
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	MOVI	r9, 16256
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p4, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__fixsfsi
	ANDI	r5, r3, 0
	ANDI	r4, r2, 1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB15_9
; %bb.10:                               ; %cond.false
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p4
	BRA	.LBB15_11
.LBB15_9:                               ; %cond.true
	XORI	r3, r9, -32768
	XORI	r2, r8, 0
	LDP	p2, [p7 + 4]                    ; 4-byte Folded Reload
.LBB15_11:                              ; %return
	JAL	__divsf3
	MOVP	p4, p1
.LBB15_12:                              ; %return
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 36
	RET
                                        ; -- End function
	.globl	atanf                           ; -- Begin function atanf
	.p2align	1
atanf:                                  ; @atanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -28
	STP	p4, [p7 + 24]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__unordsf2
	MOVI	r5, 0
	MOV	r6, r5
	MOV	r7, r5
	STP	p3, [p7 + 8]                    ; 4-byte Folded Spill
	CMP	r2, r6
	CPC	r3, r7
	BNE	.LBB16_1
; %bb.2:                                ; %if.end
	ANDI	r11, r9, 32767
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	ANDI	r10, r8, -1
	MOVI	r9, 16256
	MOV	r8, r5
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p4
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__divsf3
	MOVP	p4, p1
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB16_4
; %bb.3:                                ; %if.end
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
.LBB16_4:                               ; %if.end
	MOVI	r5, 16009
	MOVI	r4, 12451
	MOVP	p1, p4
	JAL	__gtsf2
	MOVI	r5, 0
	MOVI	r4, 1
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB16_6
; %bb.5:                                ; %if.then8
	MOVP	p5, p4
	MOVI	r9, 16349
	MOVI	r8, -19497
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	MOVI	r5, -16512
	MOVI	r4, 0
	JAL	__addsf3
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__divsf3
	MOVP	p4, p1
	MOVI	r11, 16134
	MOVI	r10, 2706
.LBB16_6:                               ; %if.end10
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p1, p4
	MOVP	p2, p4
	JAL	__mulsf3
	MOVP	p4, p1
	MOVI	r5, -16966
	MOVI	r4, 11916
	JAL	__mulsf3
	MOVI	r5, 15843
	MOVI	r4, -29127
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16878
	MOVI	r4, 18725
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15948
	MOVI	r4, -13107
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16726
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r9, 16256
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__mulsf3
	MOVP	p2, p5
	JAL	__addsf3
	MOVP	p2, p1
	MOVI	r3, 16329
	MOVI	r2, 4059
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	JAL	__subsf3
	MOVP	p5, p1
	LDP	p1, [p7 + 16]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__gtsf2
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB16_8
; %bb.7:                                ; %if.end10
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
.LBB16_8:                               ; %if.end10
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__ltsf2
	CMP	r2, r8
	CPC	r3, r9
	BGE	.LBB16_10
; %bb.9:
	XORI	r11, r11, -32768
	XORI	r10, r10, 0
.LBB16_10:                              ; %return
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 20]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 28
	RET
.LBB16_1:
	MOVP	p5, p4
	BRA	.LBB16_10
                                        ; -- End function
	.globl	atan2f                          ; -- Begin function atan2f
	.p2align	1
atan2f:                                 ; @atan2f
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -36
	STP	p4, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p4, p2
	MOVP	p2, p1
	STP	p6, [p7 + 20]                   ; 4-byte Folded Spill
	MOVP	p1, p4
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	JAL	__unordsf2
	MOVI	r10, 0
	MOV	r11, r10
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB17_24
; %bb.1:                                ; %if.end
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__nesf2
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB17_5
; %bb.2:                                ; %if.then3
	MOVI	r10, 0
	MOV	r11, r10
	LDP	p4, [p7 + 24]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__eqsf2
	CMP	r2, r10
	CPC	r3, r11
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
	BEQ	.LBB17_23
; %bb.3:                                ; %if.end6
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__gtsf2
	MOVI	r4, 4059
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB17_20
; %bb.4:                                ; %if.end6
	MOVI	r5, -16439
	BRA	.LBB17_21
.LBB17_5:                               ; %if.end8
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__divsf3
	MOVP	p4, p1
	MOVP	p2, p4
	JAL	__unordsf2
	MOVI	r5, 0
	MOV	r6, r5
	MOV	r7, r5
	CMP	r2, r6
	CPC	r3, r7
	STP	p3, [p7 + 16]                   ; 4-byte Folded Spill
	BNE	.LBB17_25
; %bb.6:                                ; %if.end.i
	ANDI	r11, r9, 32767
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	ANDI	r10, r8, -1
	MOVI	r9, 16256
	MOV	r8, r5
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__gtsf2
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p4
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__divsf3
	MOVP	p5, p1
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB17_8
; %bb.7:                                ; %if.end.i
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
.LBB17_8:                               ; %if.end.i
	MOVI	r5, 16009
	MOVI	r4, 12451
	MOVP	p1, p5
	JAL	__gtsf2
	MOVI	r5, 0
	MOVI	r4, 1
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB17_10
; %bb.9:                                ; %if.then8.i
	MOVI	r9, 16349
	MOVI	r8, -19497
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__mulsf3
	MOVI	r5, -16512
	MOVI	r4, 0
	JAL	__addsf3
	MOVP	p2, p5
	MOVP	p5, p1
	MOVP	p1, p2
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__divsf3
	MOVP	p5, p1
	MOVI	r3, 16134
	MOVI	r2, 2706
	STP	p1, [p7]                        ; 4-byte Folded Spill
	BRA	.LBB17_11
.LBB17_10:
	STP	p4, [p7]                        ; 4-byte Folded Spill
.LBB17_11:                              ; %if.end10.i
	MOVP	p1, p5
	MOVP	p2, p5
	JAL	__mulsf3
	MOVP	p4, p1
	MOVI	r5, -16966
	MOVI	r4, 11916
	JAL	__mulsf3
	MOVI	r5, 15843
	MOVI	r4, -29127
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16878
	MOVI	r4, 18725
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, 15948
	MOVI	r4, -13107
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r5, -16726
	MOVI	r4, -21845
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p4
	JAL	__mulsf3
	MOVI	r9, 16256
	MOVI	r8, 0
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	__mulsf3
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p2, p1
	MOVI	r3, 16329
	MOVI	r2, 4059
	STP	p2, [p7]                        ; 4-byte Folded Spill
	JAL	__subsf3
	MOVP	p5, p1
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__gtsf2
	MOV	r9, r8
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB17_13
; %bb.12:                               ; %if.end10.i
	LDP	p5, [p7]                        ; 4-byte Folded Reload
.LBB17_13:                              ; %if.end10.i
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__ltsf2
	CMP	r2, r8
	CPC	r3, r9
	BGE	.LBB17_15
; %bb.14:
	XORI	r11, r11, -32768
	XORI	r10, r10, 0
.LBB17_15:                              ; %if.end10.i
	LDP	p1, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__gesf2
	MOVP	p2, p1
	MOVI	r2, 4059
	MOVI	r6, -1
	MOV	r7, r6
	CMP	r4, r6
	CPC	r5, r7
	BGT	.LBB17_17
; %bb.16:                               ; %atanf.exit
	MOVI	r3, -16311
	BRA	.LBB17_18
.LBB17_17:
	MOVI	r3, 16457
.LBB17_18:                              ; %atanf.exit
	MOVP	p2, p5
	JAL	__addsf3
	STP	p1, [p7 + 24]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__gtsf2
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB17_22
; %bb.19:                               ; %atanf.exit
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	BRA	.LBB17_22
.LBB17_20:
	MOVI	r5, 16329
.LBB17_21:                              ; %atanf.exit
	MOVP	p5, p2
.LBB17_22:                              ; %atanf.exit
	LDP	p6, [p7 + 20]                   ; 4-byte Folded Reload
.LBB17_23:                              ; %return
	MOVP	p1, p5
	LDP	p4, [p7 + 32]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 36
	RET
.LBB17_24:
	MOVI	r11, 32704
	BRA	.LBB17_22
.LBB17_25:
	MOVP	p5, p4
	BRA	.LBB17_15
                                        ; -- End function
	.globl	asinf                           ; -- Begin function asinf
	.p2align	1
asinf:                                  ; @asinf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	MOVI	r5, 32704
	MOVI	r8, 0
	MOVI	r4, 0
	MOVI	r10, 0
	MOV	r11, r8
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB18_3
; %bb.1:                                ; %entry
	STP	p2, [p7]                        ; 4-byte Folded Spill
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r9, 16256
	MOVP	p2, p4
	JAL	__gtsf2
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB18_3
; %bb.2:                                ; %if.end
	MOVI	r8, 0
	MOVP	p1, p4
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__subsf3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	JAL	__mulsf3
	JAL	sqrtf
	MOVP	p2, p1
	MOVP	p1, p5
	JAL	atan2f
	MOVP	p2, p1
.LBB18_3:                               ; %return
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p2
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.globl	acosf                           ; -- Begin function acosf
	.p2align	1
acosf:                                  ; @acosf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -20
	STP	p4, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p1, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	MOVI	r5, 32704
	MOVI	r8, 0
	MOVI	r4, 0
	MOVI	r10, 0
	MOV	r11, r8
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB19_3
; %bb.1:                                ; %entry
	STP	p2, [p7]                        ; 4-byte Folded Spill
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	ANDI	r3, r5, 32767
	ANDI	r2, r4, -1
	MOVI	r9, 16256
	MOVP	p2, p4
	JAL	__gtsf2
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BGT	.LBB19_3
; %bb.2:                                ; %if.end
	MOVI	r8, 0
	MOVP	p1, p4
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__subsf3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	MOVP	p1, p5
	MOVP	p2, p4
	JAL	__addsf3
	MOVP	p2, p1
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	JAL	__mulsf3
	JAL	sqrtf
	MOVP	p2, p5
	JAL	atan2f
	MOVP	p2, p1
.LBB19_3:                               ; %return
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p2
	LDP	p4, [p7 + 16]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 20
	RET
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
