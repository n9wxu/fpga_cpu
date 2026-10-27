	.text
	.globl	malloc                          ; -- Begin function malloc
	.p2align	1
malloc:                                 ; @malloc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	ADDPI	p1, p1, 11
	ANDI	r5, r3, -1
	ANDI	r4, r2, -4
	MOVI	r12, 0
	MOV	r13, r12
	LDP	p4, [free_list]
	CMP	r8, r12
	CPC	r9, r13
	BEQ	.LBB0_10
; %bb.1:                                ; %for.body.preheader
	LEA	p1, [free_list]
	LDP	p5, [p4]
	CMP	r10, r4
	CPC	r11, r5
	BHS	.LBB0_6
; %bb.2:
	MOVI	r2, 0
	MOV	r3, r2
.LBB0_3:                                ; %for.inc
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p3, p4
	LDP	p4, [p3 + 4]
	CMP	r8, r2
	CPC	r9, r3
	BEQ	.LBB0_10
; %bb.4:                                ; %for.body
                                        ;   in Loop: Header=BB0_3 Depth=1
	LDP	p5, [p4]
	CMP	r10, r4
	CPC	r11, r5
	BLO	.LBB0_3
; %bb.5:                                ; %if.then.loopexit
	ADDPI	p1, p3, 4
.LBB0_6:                                ; %if.then
	ADDPI	p3, p2, 16
	CMP	r10, r6
	CPC	r11, r7
	BHS	.LBB0_7
; %bb.8:                                ; %if.else
	LDP	p3, [p4 + 4]
	BRA	.LBB0_9
.LBB0_10:                               ; %for.end
	LDP	p4, [brk_ptr]
	CMP	r8, r12
	CPC	r9, r13
	BNE	.LBB0_12
; %bb.11:                               ; %if.then15
	LEA	p1, [_end+3]
	ANDI	r9, r3, -1
	ANDI	r8, r2, -4
	STP	p4, [brk_ptr]
.LBB0_12:                               ; %if.end16
	ADDPP	p3, p4, p2
	MOVI	r2, 0
	MOV	r3, r2
	LEA	p6, [__stack_top-16384]
	CMP	r6, r12
	CPC	r7, r13
	BHI	.LBB0_15
; %bb.13:                               ; %if.end20
	STP	p3, [brk_ptr]
	BRA	.LBB0_14
.LBB0_7:                                ; %if.then3
	SUB	r12, r10, r4
	SBC	r13, r11, r5
	ADDPP	p3, p4, p2
	STP	p6, [p3]
	LDP	p6, [p4 + 4]
	STP	p6, [p3 + 4]
	STP	p2, [p4]
.LBB0_9:                                ; %cleanup
	STP	p3, [p1]
	LDP	p2, [p4]
.LBB0_14:                               ; %cleanup27.sink.split
	ORI	r3, r5, 0
	ORI	r2, r4, 1
	STP	p1, [p4]
	ADDPI	p1, p4, 8
.LBB0_15:                               ; %cleanup27
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	free                            ; -- Begin function free
	.p2align	1
free:                                   ; @free
; %bb.0:                                ; %entry
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_2
; %bb.1:                                ; %if.end
	LDP	p2, [p1 - 8]
	ANDI	r7, r5, -1
	ANDI	r6, r4, -2
	STP	p3, [p1 - 8]
	LDP	p2, [free_list]
	STP	p2, [p1 - 4]
	ADDPI	p1, p1, -8
	STP	p1, [free_list]
.LBB1_2:                                ; %return
	RET
                                        ; -- End function
	.globl	calloc                          ; -- Begin function calloc
	.p2align	1
calloc:                                 ; @calloc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -16
	STP	p4, [p7 + 12]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p3, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p1, p2
	MOVP	p2, p3
	JAL	__mulsi3
	STP	p1, [p7]                        ; 4-byte Folded Spill
	ADDPI	p1, p1, 11
	ANDI	r5, r3, -1
	ANDI	r4, r2, -4
	MOVI	r10, 0
	MOV	r11, r10
	LDP	p3, [free_list]
	CMP	r6, r10
	CPC	r7, r11
	BEQ	.LBB2_10
; %bb.1:                                ; %for.body.i.preheader
	LEA	p6, [free_list]
	LDP	p4, [p3]
	CMP	r8, r4
	CPC	r9, r5
	BHS	.LBB2_6
; %bb.2:
	MOVI	r12, 0
	MOV	r13, r12
.LBB2_3:                                ; %for.inc.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p1, p3
	LDP	p3, [p1 + 4]
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB2_10
; %bb.4:                                ; %for.body.i
                                        ;   in Loop: Header=BB2_3 Depth=1
	LDP	p4, [p3]
	CMP	r8, r4
	CPC	r9, r5
	BLO	.LBB2_3
; %bb.5:                                ; %if.then.i.loopexit
	ADDPI	p6, p1, 4
.LBB2_6:                                ; %if.then.i
	ADDPI	p1, p2, 16
	CMP	r8, r2
	CPC	r9, r3
	BHS	.LBB2_7
; %bb.8:                                ; %if.else.i
	LDP	p1, [p3 + 4]
	BRA	.LBB2_9
.LBB2_10:                               ; %for.end.i
	LDP	p3, [brk_ptr]
	CMP	r6, r10
	CPC	r7, r11
	BNE	.LBB2_12
; %bb.11:                               ; %if.then15.i
	LEA	p1, [_end+3]
	ANDI	r7, r3, -1
	ANDI	r6, r2, -4
	STP	p3, [brk_ptr]
.LBB2_12:                               ; %if.end16.i
	ADDPP	p1, p3, p2
	MOVI	r10, 0
	MOV	r11, r10
	LEA	p6, [__stack_top-16384]
	CMP	r2, r12
	CPC	r3, r13
	BHI	.LBB2_15
; %bb.13:                               ; %if.end20.i
	STP	p1, [brk_ptr]
	BRA	.LBB2_14
.LBB2_7:                                ; %if.then3.i
	SUB	r8, r8, r4
	SBC	r9, r9, r5
	ADDPP	p1, p3, p2
	STP	p4, [p1]
	LDP	p4, [p3 + 4]
	STP	p4, [p1 + 4]
	STP	p2, [p3]
.LBB2_9:                                ; %cleanup.i
	STP	p1, [p6]
	LDP	p2, [p3]
.LBB2_14:                               ; %if.then
	ORI	r3, r5, 0
	ORI	r2, r4, 1
	STP	p1, [p3]
	ADDPI	p5, p3, 8
	MOVI	r4, 0
	MOV	r5, r4
	MOVP	p1, p5
	LDP	p3, [p7]                        ; 4-byte Folded Reload
	JAL	memset
.LBB2_15:                               ; %if.end
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	MOVP	p1, p5
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 16
	RET
                                        ; -- End function
	.globl	realloc                         ; -- Begin function realloc
	.p2align	1
realloc:                                ; @realloc
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -24
	STP	p4, [p7 + 20]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 16]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r6, 0
	MOV	r7, r6
	STP	p1, [p7 + 12]                   ; 4-byte Folded Spill
	CMP	r2, r6
	CPC	r3, r7
	BEQ	.LBB3_1
; %bb.16:                               ; %if.end
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	LDP	p1, [p1 - 8]
	ANDI	r7, r3, -1
	ANDI	r6, r2, -2
	ADDPI	p1, p3, -8
	CMP	r2, r4
	CPC	r3, r5
	BHS	.LBB3_17
; %bb.18:                               ; %if.end2
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	ADDPI	p1, p2, 11
	ANDI	r11, r3, -1
	ANDI	r10, r2, -4
	MOVI	r2, 0
	MOV	r3, r2
	LDP	p3, [free_list]
	CMP	r6, r2
	CPC	r7, r3
	BEQ	.LBB3_28
; %bb.19:                               ; %for.body.i22.preheader
	LEA	p6, [free_list]
	LDP	p4, [p3]
	CMP	r8, r10
	CPC	r9, r11
	BHS	.LBB3_24
; %bb.20:
	MOVI	r12, 0
	MOV	r13, r12
.LBB3_21:                               ; %for.inc.i44
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p2, p3
	LDP	p3, [p2 + 4]
	CMP	r6, r12
	CPC	r7, r13
	BEQ	.LBB3_28
; %bb.22:                               ; %for.body.i22
                                        ;   in Loop: Header=BB3_21 Depth=1
	LDP	p4, [p3]
	CMP	r8, r10
	CPC	r9, r11
	BLO	.LBB3_21
; %bb.23:                               ; %if.then.i26.loopexit
	ADDPI	p6, p2, 4
.LBB3_24:                               ; %if.then.i26
	ADDPI	p1, p5, 16
	CMP	r8, r2
	CPC	r9, r3
	BHS	.LBB3_25
; %bb.26:                               ; %if.else.i42
	LDP	p1, [p3 + 4]
	BRA	.LBB3_27
.LBB3_1:                                ; %if.then
	ADDPI	p2, p2, 11
	ANDI	r3, r5, -1
	ANDI	r2, r4, -4
	LDP	p2, [free_list]
	CMP	r4, r6
	CPC	r5, r7
	BEQ	.LBB3_11
; %bb.2:                                ; %for.body.i.preheader
	LEA	p3, [free_list]
	LDP	p4, [p2]
	CMP	r8, r2
	CPC	r9, r3
	BHS	.LBB3_7
; %bb.3:
	MOVI	r6, 0
	MOV	r7, r6
.LBB3_4:                                ; %for.inc.i
                                        ; =>This Inner Loop Header: Depth=1
	MOVP	p6, p2
	LDP	p2, [p6 + 4]
	CMP	r4, r6
	CPC	r5, r7
	BEQ	.LBB3_11
; %bb.5:                                ; %for.body.i
                                        ;   in Loop: Header=BB3_4 Depth=1
	LDP	p4, [p2]
	CMP	r8, r2
	CPC	r9, r3
	BLO	.LBB3_4
; %bb.6:                                ; %if.then.i.loopexit
	ADDPI	p3, p6, 4
.LBB3_7:                                ; %if.then.i
	ADDPI	p6, p1, 16
	CMP	r8, r12
	CPC	r9, r13
	BHS	.LBB3_8
; %bb.9:                                ; %if.else.i
	LDP	p6, [p2 + 4]
	BRA	.LBB3_10
.LBB3_17:
	LDP	p4, [p7 + 12]                   ; 4-byte Folded Reload
	BRA	.LBB3_33
.LBB3_11:                               ; %for.end.i
	MOVI	r8, 0
	MOV	r9, r8
	LDP	p2, [brk_ptr]
	CMP	r4, r8
	CPC	r5, r9
	BNE	.LBB3_13
; %bb.12:                               ; %if.then15.i
	LEA	p3, [_end+3]
	ANDI	r5, r7, -1
	ANDI	r4, r6, -4
	STP	p2, [brk_ptr]
.LBB3_13:                               ; %if.end16.i
	ADDPP	p3, p2, p1
	LEA	p6, [__stack_top-16384]
	CMP	r6, r12
	CPC	r7, r13
	BHI	.LBB3_33
; %bb.14:                               ; %if.end20.i
	STP	p3, [brk_ptr]
	BRA	.LBB3_15
.LBB3_28:                               ; %for.end.i48
	LDP	p3, [brk_ptr]
	CMP	r6, r2
	CPC	r7, r3
	BNE	.LBB3_30
; %bb.29:                               ; %if.then15.i54
	LEA	p1, [_end+3]
	ANDI	r7, r3, -1
	ANDI	r6, r2, -4
	STP	p3, [brk_ptr]
.LBB3_30:                               ; %if.end16.i50
	ADDPP	p1, p3, p5
	MOVI	r8, 0
	MOV	r9, r8
	LEA	p2, [__stack_top-16384]
	CMP	r2, r4
	CPC	r3, r5
	BHI	.LBB3_33
; %bb.31:                               ; %if.end20.i53
	STP	p1, [brk_ptr]
	BRA	.LBB3_32
.LBB3_8:                                ; %if.then3.i
	SUB	r8, r8, r2
	SBC	r9, r9, r3
	ADDPP	p6, p2, p1
	STP	p4, [p6]
	LDP	p4, [p2 + 4]
	STP	p4, [p6 + 4]
	STP	p1, [p2]
.LBB3_10:                               ; %cleanup.i
	STP	p6, [p3]
	LDP	p1, [p2]
.LBB3_15:                               ; %cleanup27.sink.split.i
	ORI	r7, r3, 0
	ORI	r6, r2, 1
	STP	p3, [p2]
	ADDPI	p4, p2, 8
	BRA	.LBB3_33
.LBB3_25:                               ; %if.then3.i29
	SUB	r4, r8, r10
	SBC	r5, r9, r11
	ADDPP	p1, p3, p5
	STP	p2, [p1]
	LDP	p2, [p3 + 4]
	STP	p2, [p1 + 4]
	STP	p5, [p3]
.LBB3_27:                               ; %cleanup.i34
	STP	p1, [p6]
	LDP	p5, [p3]
.LBB3_32:                               ; %free.exit
	LDP	p1, [p7 + 12]                   ; 4-byte Folded Reload
	ADDPI	p2, p1, -8
	STP	p2, [p7]                        ; 4-byte Folded Spill
	ORI	r5, r11, 0
	ORI	r4, r10, 1
	STP	p2, [p3]
	ADDPI	p4, p3, 8
	MOVP	p5, p1
	MOVP	p1, p4
	MOVP	p2, p5
	LDP	p3, [p7 + 4]                    ; 4-byte Folded Reload
	JAL	memcpy
	LDP	p1, [p5 - 8]
	ANDI	r5, r3, -1
	ANDI	r4, r2, -2
	STP	p2, [p5 - 8]
	LDP	p1, [free_list]
	STP	p1, [p5 - 4]
	LDP	p1, [p7]                        ; 4-byte Folded Reload
	STP	p1, [free_list]
.LBB3_33:                               ; %return
	LDP	p6, [p7 + 8]                    ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 20]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 16]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 24
	RET
                                        ; -- End function
	.local	free_list                       ; @free_list
	.comm	free_list,4,2
	.local	brk_ptr                         ; @brk_ptr
	.comm	brk_ptr,4,2
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym _end
	.addrsig_sym __stack_top
