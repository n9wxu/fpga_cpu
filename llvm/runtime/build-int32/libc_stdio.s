	.text
	.globl	putchar                         ; -- Begin function putchar
	.p2align	1
putchar:                                ; @putchar
; %bb.0:                                ; %entry
	ANDI	r4, r2, 255
	STW	r4, [15728640]
	ANDI	r5, r3, 0
	MOVP	p1, p2
	RET
                                        ; -- End function
	.globl	puts                            ; -- Begin function puts
	.p2align	1
puts:                                   ; @puts
; %bb.0:                                ; %entry
	LDBU	r4, [p1]
	CMPI	r4, 0
	BEQ	.LBB1_2
.LBB1_1:                                ; %while.body
                                        ; =>This Inner Loop Header: Depth=1
	ANDI	r4, r4, 255
	STW	r4, [15728640]
	ADDPI	p3, p1, 1
	LDBU	r4, [p1 + 1]
	CMPI	r4, 0
	MOVP	p1, p3
	BNE	.LBB1_1
.LBB1_2:                                ; %while.end
	MOVI	r2, 10
	STW	r2, [15728640]
	MOVI	r2, 0
	MOV	r3, r2
	RET
                                        ; -- End function
	.globl	getchar                         ; -- Begin function getchar
	.p2align	1
getchar:                                ; @getchar
; %bb.0:                                ; %entry
.LBB2_1:                                ; %for.cond
                                        ; =>This Inner Loop Header: Depth=1
	LDW	r2, [15728648]
	CMPI	r2, -1
	BEQ	.LBB2_1
; %bb.2:                                ; %for.cond
	CMPI	r2, -2
	BNE	.LBB2_3
; %bb.4:                                ; %return.loopexit
	MOVI	r2, -1
	MOV	r3, r2
	RET
.LBB2_3:                                ; %if.then5
	MOVI	r3, 0
	RET
                                        ; -- End function
	.globl	vsnprintf                       ; -- Begin function vsnprintf
	.p2align	1
vsnprintf:                              ; @vsnprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -204
	STP	p4, [p7 + 200]                  ; 4-byte Folded Spill
	STP	p5, [p7 + 196]                  ; 4-byte Folded Spill
	STP	p2, [p7 + 128]                  ; 4-byte Folded Spill
	STP	p6, [p7 + 12]                   ; 4-byte Folded Spill
	LEA	p2, [p7 + 145]
	STP	p2, [p7 + 20]                   ; 4-byte Folded Spill
	MOVI	r11, 0
	MOV	r10, r11
	STP	p5, [p7 + 132]                  ; 4-byte Folded Spill
	MOVI	r10, 1
	MOVI	r4, 10
	STP	p2, [p7 + 124]                  ; 4-byte Folded Spill
	MOVI	r4, -1
	STP	p2, [p7 + 76]                   ; 4-byte Folded Spill
	MOVI	r4, 6
	STP	p2, [p7 + 48]                   ; 4-byte Folded Spill
	MOVI	r4, 3
	STP	p2, [p7 + 4]                    ; 4-byte Folded Spill
	MOVI	r5, 32639
	MOVI	r4, -13922
	STP	p2, [p7 + 36]                   ; 4-byte Folded Spill
	MOVI	r5, 16672
	STP	p2, [p7 + 100]                  ; 4-byte Folded Spill
	MOVI	r5, 16256
	STP	p2, [p7 + 72]                   ; 4-byte Folded Spill
	MOVI	r5, 16128
	STP	p2, [p7 + 28]                   ; 4-byte Folded Spill
	MOVI	r4, 7
	STP	p2, [p7 + 32]                   ; 4-byte Folded Spill
	MOVI	r8, 9
	MOVI	r5, 19224
	MOVI	r4, -27008
	STP	p2, [p7 + 16]                   ; 4-byte Folded Spill
	MOVI	r5, 14545
	MOVI	r4, -18665
	STP	p2, [p7 + 8]                    ; 4-byte Folded Spill
	MOVI	r4, 16
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
	MOVI	r4, 8
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	MOVI	r4, 2
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 136]                  ; 4-byte Folded Spill
	BRA	.LBB3_5
.LBB3_1:                                ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 132]                  ; 4-byte Folded Spill
.LBB3_2:                                ; %cleanup
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_3:                                ; %cleanup
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p4, [p7 + 112]                  ; 4-byte Folded Reload
.LBB3_4:                                ; %for.inc266
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 116]                  ; 4-byte Folded Reload
	ADDPI	p3, p3, 1
.LBB3_5:                                ; %for.cond
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB3_11 Depth 2
                                        ;       Child Loop BB3_14 Depth 3
                                        ;     Child Loop BB3_28 Depth 2
                                        ;     Child Loop BB3_50 Depth 2
                                        ;     Child Loop BB3_38 Depth 2
                                        ;     Child Loop BB3_95 Depth 2
                                        ;     Child Loop BB3_181 Depth 2
                                        ;     Child Loop BB3_77 Depth 2
                                        ;     Child Loop BB3_118 Depth 2
                                        ;     Child Loop BB3_142 Depth 2
                                        ;     Child Loop BB3_159 Depth 2
                                        ;     Child Loop BB3_166 Depth 2
                                        ;     Child Loop BB3_175 Depth 2
                                        ;     Child Loop BB3_106 Depth 2
                                        ;     Child Loop BB3_109 Depth 2
                                        ;     Child Loop BB3_193 Depth 2
                                        ;     Child Loop BB3_201 Depth 2
                                        ;     Child Loop BB3_203 Depth 2
                                        ;     Child Loop BB3_207 Depth 2
                                        ;     Child Loop BB3_219 Depth 2
                                        ;     Child Loop BB3_227 Depth 2
                                        ;     Child Loop BB3_235 Depth 2
	STP	p3, [p7 + 116]                  ; 4-byte Folded Spill
	LDBU	r12, [p3]
	CMPI	r12, 37
	BEQ	.LBB3_10
; %bb.6:                                ; %for.cond
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMPI	r12, 0
	BEQ	.LBB3_239
; %bb.7:                                ; %if.then
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p3, p5
	MOV	r4, r7
	MOV	r5, r7
	CMP	r2, r4
	CPC	r3, r5
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPI	p2, p2, 1
	BEQ	.LBB3_30
; %bb.8:                                ; %if.else.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BHS	.LBB3_31
; %bb.9:                                ; %if.then2.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPP	p3, p1, p3
	STB	r12, [p3]
	STP	p2, [p7 + 132]                  ; 4-byte Folded Spill
	BRA	.LBB3_4
.LBB3_10:                               ; %for.cond4.outer.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r2, 0
	MOV	r3, r2
	STP	p1, [p7 + 56]                   ; 4-byte Folded Spill
	MOVP	p3, p1
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 80]                   ; 4-byte Folded Spill
	STP	p4, [p7 + 112]                  ; 4-byte Folded Spill
.LBB3_11:                               ; %for.cond4.outer743
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB3_14 Depth 3
	LDP	p2, [p7 + 116]                  ; 4-byte Folded Reload
	STP	p3, [p7 + 92]                   ; 4-byte Folded Spill
	ADDPI	p1, p2, 1
	STP	p1, [p7 + 116]                  ; 4-byte Folded Spill
	LDBU	r2, [p2 + 1]
	ADDI	r6, r2, -32
	CMPI	r6, 16
	BHI	.LBB3_90
; %bb.12:                               ; %for.cond4.outer743
                                        ;   in Loop: Header=BB3_11 Depth=2
	MOVP	p4, p5
	MOV	r7, r9
	SHLI	r12, r7, 2
	SHRI	r13, r6, 14
	OR	r13, r12, r13
	SHLI	r12, r6, 2
	LEA	p3, [.LJTI3_0]
	ADDPP	p3, p6, p3
	LDP	p6, [p3]
	MOVP	p3, p4
	JR	p6
.LBB3_13:                               ; %for.cond4.preheader
                                        ;   in Loop: Header=BB3_11 Depth=2
	LDP	p3, [p7 + 116]                  ; 4-byte Folded Reload
.LBB3_14:                               ; %for.cond4
                                        ;   Parent Loop BB3_5 Depth=1
                                        ;     Parent Loop BB3_11 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	MOVP	p2, p3
	ADDPI	p3, p2, 1
	LDBU	r2, [p2 + 1]
	CMPI	r2, 45
	BEQ	.LBB3_14
; %bb.15:                               ; %for.cond4
                                        ;   in Loop: Header=BB3_11 Depth=2
	STP	p3, [p7 + 116]                  ; 4-byte Folded Spill
	ADDI	r6, r2, -32
	CMPI	r6, 16
	MOVP	p4, p5
	BHI	.LBB3_25
; %bb.16:                               ; %for.cond4
                                        ;   in Loop: Header=BB3_11 Depth=2
	MOV	r7, r9
	SHLI	r12, r7, 2
	SHRI	r13, r6, 14
	OR	r13, r12, r13
	SHLI	r12, r6, 2
	LEA	p3, [.LJTI3_1]
	ADDPP	p3, p6, p3
	LDP	p3, [p3]
	JR	p3
.LBB3_17:                               ; %if.then22.loopexit
                                        ;   in Loop: Header=BB3_11 Depth=2
	MOVI	r2, 1
	MOV	r3, r9
	LDP	p3, [p7 + 92]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	STP	p1, [p7 + 80]                   ; 4-byte Folded Spill
	BRA	.LBB3_11
.LBB3_18:                               ; %if.then17.loopexit
                                        ;   in Loop: Header=BB3_11 Depth=2
	STP	p4, [p7 + 56]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 92]                   ; 4-byte Folded Reload
	BRA	.LBB3_20
.LBB3_19:                               ; %if.then12.loopexit
                                        ;   in Loop: Header=BB3_11 Depth=2
	MOVP	p3, p4
.LBB3_20:                               ; %for.cond4.outer743
                                        ;   in Loop: Header=BB3_11 Depth=2
	STP	p4, [p7 + 80]                   ; 4-byte Folded Spill
	BRA	.LBB3_11
.LBB3_21:                               ; %if.then17.loopexit879
                                        ;   in Loop: Header=BB3_11 Depth=2
	STP	p5, [p7 + 56]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 92]                   ; 4-byte Folded Reload
	BRA	.LBB3_11
.LBB3_22:                               ; %if.then22.loopexit880
                                        ;   in Loop: Header=BB3_11 Depth=2
	MOVI	r2, 1
	STP	p1, [p7 + 52]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 92]                   ; 4-byte Folded Reload
	BRA	.LBB3_11
.LBB3_23:                               ; %if.then32.loopexit
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 80]                   ; 4-byte Folded Spill
.LBB3_24:                               ; %if.then32
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 204]
	ADDPI	p1, p3, 4
	STP	p1, [p7 + 204]
	ADDPI	p6, p2, 2
	LDBU	r2, [p2 + 2]
	LDP	p2, [p3]
	STP	p2, [p7 + 64]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 124]                  ; 4-byte Folded Reload
	BRA	.LBB3_33
.LBB3_25:                               ; %while.cond.preheader.loopexit
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 80]                   ; 4-byte Folded Spill
.LBB3_26:                               ; %while.cond.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r4, r9
	MOV	r5, r9
	STP	p2, [p7 + 64]                   ; 4-byte Folded Spill
	ADDI	r4, r2, -48
	ANDI	r4, r4, 255
	CMPI	r4, 9
	LDP	p2, [p7 + 124]                  ; 4-byte Folded Reload
	BHI	.LBB3_32
; %bb.27:                               ; %while.body.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 140]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 116]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 64]                   ; 4-byte Folded Reload
.LBB3_28:                               ; %while.body
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ANDI	r9, r3, 0
	ANDI	r8, r2, 255
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r5, r3
	STP	p2, [p7 + 124]                  ; 4-byte Folded Spill
	MOVP	p1, p3
	JAL	__mulsi3
	LDP	p2, [p7 + 124]                  ; 4-byte Folded Reload
	ADDPP	p1, p1, p4
	ADDPI	p6, p5, 1
	ADDPI	p4, p1, -48
	LDBU	r2, [p5 + 1]
	ADDI	r6, r2, -48
	ANDI	r6, r6, 255
	CMPI	r6, 10
	MOVP	p3, p4
	MOVP	p5, p6
	BLO	.LBB3_28
; %bb.29:                               ;   in Loop: Header=BB3_5 Depth=1
	STP	p3, [p7 + 64]                   ; 4-byte Folded Spill
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	BRA	.LBB3_33
.LBB3_30:                               ; %if.then.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STW	r12, [15728640]
.LBB3_31:                               ;   in Loop: Header=BB3_5 Depth=1
	STP	p2, [p7 + 132]                  ; 4-byte Folded Spill
	BRA	.LBB3_4
.LBB3_32:                               ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p5, p4
	LDP	p6, [p7 + 116]                  ; 4-byte Folded Reload
.LBB3_33:                               ; %if.end43
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r8, r6
	MOV	r9, r6
	ANDI	r2, r2, 255
	CMPI	r2, 46
	STP	p5, [p7 + 140]                  ; 4-byte Folded Spill
	STP	p2, [p7 + 124]                  ; 4-byte Folded Spill
	BNE	.LBB3_36
; %bb.34:                               ; %if.then47
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDBU	r2, [p6 + 1]
	CMPI	r2, 42
	BNE	.LBB3_49
; %bb.35:                               ; %if.then52
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 204]
	ADDPI	p3, p1, 4
	STP	p3, [p7 + 204]
	ADDPI	p6, p6, 2
	LDP	p5, [p1]
	BRA	.LBB3_37
.LBB3_36:                               ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p5, p4
.LBB3_37:                               ; %if.end73
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r2, r7
	MOV	r3, r7
.LBB3_38:                               ; %while.cond74
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDBU	r8, [p6]
	CMPI	r8, 108
	BNE	.LBB3_40
; %bb.39:                               ; %while.body78
                                        ;   in Loop: Header=BB3_38 Depth=2
	ADDPI	p6, p6, 1
	ADDPI	p1, p1, 1
	BRA	.LBB3_38
.LBB3_40:                               ; %while.cond74
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMPI	r8, 104
	BNE	.LBB3_42
; %bb.41:                               ; %if.then84
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDBU	r8, [p6 + 1]
	ADDPI	p6, p6, 1
.LBB3_42:                               ; %if.end86
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p6, [p7 + 116]                  ; 4-byte Folded Spill
	ANDI	r9, r8, 255
	ADDI	r6, r9, -37
	CMPI	r6, 83
	BHI	.LBB3_2
; %bb.43:                               ; %if.end86
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p5, [p7 + 60]                   ; 4-byte Folded Spill
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r5, r11
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	MOV	r7, r11
	SHLI	r4, r7, 2
	SHRI	r12, r6, 14
	OR	r13, r4, r12
	SHLI	r12, r6, 2
	LEA	p2, [.LJTI3_2]
	ADDPP	p3, p6, p2
	LDP	p3, [p3]
	JR	p3
.LBB3_44:                               ; %sw.bb207
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 204]
	ADDPI	p2, p1, 4
	STP	p2, [p7 + 204]
	LDP	p3, [p7 + 48]                   ; 4-byte Folded Reload
	MOV	r7, r11
	MOVP	p2, p5
	MOV	r10, r5
	MOV	r11, r5
	LDP	p2, [p7 + 60]                   ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	STP	p3, [p7 + 48]                   ; 4-byte Folded Spill
	BLT	.LBB3_46
; %bb.45:                               ; %sw.bb207
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p3, p2
.LBB3_46:                               ; %sw.bb207
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p3, [p7 + 92]                   ; 4-byte Folded Spill
	LDP	p1, [p1]
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p2, p1
	JAL	__unordsf2
	CMP	r2, r10
	CPC	r3, r11
	BNE	.LBB3_238
; %bb.47:                               ; %if.end.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	STP	p2, [p7 + 120]                  ; 4-byte Folded Spill
	JAL	__ltsf2
	LEA	p6, [p7 + 144]
	LDP	p3, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r4, r6
	MOV	r5, r6
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB3_60
; %bb.48:                               ; %if.then4.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r2, 45
	STB	r2, [p7 + 144]
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	XORI	r3, r3, -32768
	XORI	r2, r2, 0
	LDP	p2, [p7 + 20]                   ; 4-byte Folded Reload
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	STP	p5, [p7 + 120]                  ; 4-byte Folded Spill
	BRA	.LBB3_61
.LBB3_49:                               ; %while.cond56.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	ADDPI	p4, p6, 1
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r10, r7
	MOV	r11, r7
	ADDI	r6, r2, -48
	ANDI	r6, r6, 255
	CMPI	r6, 9
	BHI	.LBB3_70
.LBB3_50:                               ; %while.body65
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p3, p5
	ANDI	r11, r3, 0
	ANDI	r10, r2, 255
	MOVP	p1, p3
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r5, r7
	JAL	__mulsi3
	LDP	p2, [p7 + 124]                  ; 4-byte Folded Reload
	ADDPP	p1, p1, p5
	ADDPI	p6, p4, 1
	ADDPI	p5, p1, -48
	LDBU	r2, [p4 + 1]
	ADDI	r6, r2, -48
	ANDI	r6, r6, 255
	CMPI	r6, 10
	MOVP	p4, p6
	BLO	.LBB3_50
	BRA	.LBB3_37
.LBB3_51:                               ; %cond.end129
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 44]                   ; 4-byte Folded Reload
	MOV	r5, r11
	CMPI	r9, 111
	STP	p2, [p7 + 44]                   ; 4-byte Folded Spill
	BEQ	.LBB3_53
; %bb.52:                               ; %cond.end129
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 24]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r5, r7
	STP	p2, [p7 + 24]                   ; 4-byte Folded Spill
.LBB3_53:                               ; %cond.end129
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	CMPI	r9, 112
	BNE	.LBB3_71
; %bb.54:                               ; %if.then134
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r10, r9
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r4, r3
	MOV	r5, r3
	LDP	p6, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	BEQ	.LBB3_113
; %bb.55:                               ; %if.else.i389
                                        ;   in Loop: Header=BB3_5 Depth=1
	ADDPI	p2, p4, 1
	LDP	p3, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BHS	.LBB3_57
; %bb.56:                               ; %if.then2.i396
                                        ;   in Loop: Header=BB3_5 Depth=1
	ADDPP	p3, p6, p4
	MOVI	r2, 48
	STB	r2, [p3]
.LBB3_57:                               ; %if.else.i401
                                        ;   in Loop: Header=BB3_5 Depth=1
	ADDPI	p3, p4, 2
	LDP	p4, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r6, r8
	CPC	r7, r9
	BHS	.LBB3_59
; %bb.58:                               ; %if.then2.i408
                                        ;   in Loop: Header=BB3_5 Depth=1
	ADDPP	p3, p6, p2
	MOVI	r2, 120
	STB	r2, [p3]
.LBB3_59:                               ; %if.else145.thread
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	BRA	.LBB3_114
.LBB3_60:                               ;   in Loop: Header=BB3_5 Depth=1
	STP	p6, [p7 + 104]                  ; 4-byte Folded Spill
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
.LBB3_61:                               ; %if.end5.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 36]                   ; 4-byte Folded Reload
	JAL	__gtsf2
	CMP	r2, r10
	CPC	r3, r11
	BLT	.LBB3_63
; %bb.62:                               ; %if.then8.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	LEA	p2, [.L.str.4]
	JAL	strcpy
	LEA	p1, [p7 + 144]
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	ADDPI	p6, p6, 3
	BRA	.LBB3_103
.LBB3_63:                               ; %if.end10.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	ORI	r3, r8, 32
	MOV	r4, r11
	MOV	r5, r11
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	ANDI	r2, r3, 255
	CMPI	r2, 101
	BEQ	.LBB3_68
; %bb.64:                               ; %if.end10.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMPI	r2, 103
	BNE	.LBB3_104
; %bb.65:                               ; %land.lhs.true.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 16]                   ; 4-byte Folded Reload
	JAL	__gesf2
	LDP	p2, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r8, r4
	MOV	r9, r4
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB3_68
; %bb.66:                               ; %lor.lhs.false19.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 8]                    ; 4-byte Folded Reload
	JAL	__ltsf2
	MOVI	r4, 103
	STW	r4, [p7 + 88]                   ; 2-byte Folded Spill
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r10, r5
	MOV	r11, r5
	CMP	r2, r8
	CPC	r3, r9
	BGT	.LBB3_187
; %bb.67:                               ; %lor.lhs.false19.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__eqsf2
	CMP	r2, r10
	CPC	r3, r11
	BEQ	.LBB3_187
.LBB3_68:                               ; %if.then25.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r4, r11
	LDP	p1, [p7 + 100]                  ; 4-byte Folded Reload
	MOV	r5, r3
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	JAL	__gesf2
	LDP	p2, [p7 + 104]                  ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BGE	.LBB3_105
; %bb.69:                               ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p5, p2
	LDP	p4, [p7 + 100]                  ; 4-byte Folded Reload
	BRA	.LBB3_107
.LBB3_70:                               ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p4
	BRA	.LBB3_37
.LBB3_71:                               ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_72:                               ; %if.end135
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 40]                   ; 4-byte Folded Reload
	MOV	r5, r11
	STP	p2, [p7 + 40]                   ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BLO	.LBB3_88
; %bb.73:                               ; %if.then138
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p1, [.L.str.1]
	STP	p1, [p7 + 68]                   ; 4-byte Folded Spill
	CMPI	r9, 88
	BEQ	.LBB3_75
; %bb.74:                               ; %if.then138
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p1, [.L.str.2]
	STP	p1, [p7 + 68]                   ; 4-byte Folded Spill
.LBB3_75:                               ; %if.then138
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p6, [p7 + 204]
	ADDPI	p2, p6, 4
	STP	p2, [p7 + 204]
	LDP	p1, [p6]
	ADDPI	p2, p6, 8
	STP	p2, [p7 + 204]
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r8, r5
	MOV	r9, r5
	LDP	p2, [p6 + 4]
	MOVP	p6, p4
	BRA	.LBB3_77
.LBB3_76:                               ; %do.body.i413
                                        ;   in Loop: Header=BB3_77 Depth=2
	ANDI	r3, r5, 0
	ANDI	r2, r4, 1
	ADDPI	p6, p6, 1
	CMP	r2, r8
	CPC	r3, r9
	LDP	p1, [p7 + 96]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 88]                   ; 4-byte Folded Reload
	BNE	.LBB3_89
.LBB3_77:                               ; %do.body.i413
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p1, [p7 + 84]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	STP	p4, [p7]
	STP	p2, [p7 + 108]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 104]                  ; 4-byte Folded Reload
	MOVP	p3, p5
	JAL	__udivdi3
	STP	p4, [p7]
	STP	p1, [p7 + 96]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 88]                   ; 4-byte Folded Spill
	MOVP	p3, p5
	JAL	__muldi3
	LDP	p2, [p7 + 84]                   ; 4-byte Folded Reload
	MOVP	p3, p2
	CMP	r4, r10
	CPC	r5, r11
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	BLO	.LBB3_79
; %bb.78:                               ; %do.body.i413
                                        ;   in Loop: Header=BB3_77 Depth=2
	MOVP	p2, p4
.LBB3_79:                               ; %do.body.i413
                                        ;   in Loop: Header=BB3_77 Depth=2
	SUB	r2, r6, r2
	SBC	r3, r7, r3
	LDP	p3, [p7 + 68]                   ; 4-byte Folded Reload
	ADDPP	p1, p3, p1
	LDBU	r2, [p1]
	LEA	p3, [p7 + 144]
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	ADDPP	p3, p3, p6
	STB	r2, [p3]
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB3_76
; %bb.80:                               ; %do.body.i413
                                        ;   in Loop: Header=BB3_77 Depth=2
	MOVP	p2, p4
	BRA	.LBB3_76
.LBB3_81:                               ; %sw.bb
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r2, r10
	CPC	r3, r11
	BLS	.LBB3_94
; %bb.82:                               ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 204]
	ADDPI	p2, p3, 4
	STP	p2, [p7 + 204]
	LDP	p1, [p3]
	ADDPI	p2, p3, 8
	STP	p2, [p7 + 204]
	MOV	r12, r11
	MOV	r13, r11
	CMP	r2, r12
	CPC	r3, r13
	MOVP	p2, p5
	LDP	p5, [p7 + 124]                  ; 4-byte Folded Reload
	BNE	.LBB3_84
; %bb.83:                               ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p2, p6
.LBB3_84:                               ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p3 + 4]
	CMP	r6, r12
	CPC	r7, r13
	BGE	.LBB3_86
; %bb.85:                               ;   in Loop: Header=BB3_5 Depth=1
	SUB	r2, r12, r2
	SBC	r3, r13, r3
.LBB3_86:                               ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r6, r12
	CPC	r7, r13
	BLT	.LBB3_178
; %bb.87:                               ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p4, p3
	BRA	.LBB3_179
.LBB3_88:                               ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 132]                  ; 4-byte Folded Reload
	BRA	.LBB3_115
.LBB3_89:                               ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 84]                   ; 4-byte Folded Spill
	BRA	.LBB3_119
.LBB3_90:                               ; %while.cond.preheader.loopexit878
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p4, p5
	BRA	.LBB3_26
.LBB3_91:                               ; %sw.bb219
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 204]
	ADDPI	p2, p1, 4
	STP	p2, [p7 + 204]
	LDW	r2, [p1]
	BRA	.LBB3_93
.LBB3_92:                               ; %sw.bb236
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r2, 37
.LBB3_93:                               ; %sw.epilog
                                        ;   in Loop: Header=BB3_5 Depth=1
	STB	r2, [p7 + 144]
	LEA	p1, [p7 + 144]
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p6, p5
	LDP	p1, [p7 + 64]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	BRA	.LBB3_214
.LBB3_94:                               ; %cond.false
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 204]
	ADDPI	p2, p1, 4
	STP	p2, [p7 + 204]
	LDP	p1, [p1]
	SARI	r4, r3, 15
	MOV	r5, r4
	XOR	r7, r3, r5
	XOR	r6, r2, r4
	SUB	r8, r6, r4
	SBC	r9, r7, r5
	SHRI	r2, r3, 15
	MOV	r3, r11
	STP	p1, [p7 + 84]                   ; 4-byte Folded Spill
	MOV	r12, r11
	MOV	r13, r11
	LDP	p3, [p7 + 124]                  ; 4-byte Folded Reload
.LBB3_95:                               ; %do.body.i380
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	MOV	r10, r6
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r11, r3
	MOVP	p1, p4
	MOVP	p2, p5
	JAL	__udivsi3
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__mulsi3
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 124]                  ; 4-byte Folded Reload
	SUB	r2, r8, r2
	SBC	r3, r9, r3
	LEA	p2, [.L.str.2]
	ADDPP	p1, p1, p2
	LDBU	r2, [p1]
	LEA	p2, [p7 + 144]
	ADDPP	p2, p2, p6
	STB	r2, [p2]
	ADDPI	p6, p6, 1
	CMP	r8, r10
	CPC	r9, r11
	LDP	p4, [p7 + 108]                  ; 4-byte Folded Reload
	BHS	.LBB3_95
	BRA	.LBB3_119
.LBB3_96:                               ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 204]
	ADDPI	p2, p1, 4
	STP	p2, [p7 + 204]
	MOV	r8, r11
	MOV	r9, r11
	LDP	p2, [p1]
	LEA	p1, [.L.str]
	CMP	r4, r8
	CPC	r5, r9
	BEQ	.LBB3_98
; %bb.97:                               ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p2
.LBB3_98:                               ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	JAL	strlen
	LDP	p3, [p7 + 60]                   ; 4-byte Folded Reload
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p2, p1
	BLT	.LBB3_100
; %bb.99:                               ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p2, p3
.LBB3_100:                              ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r6, r8
	CPC	r7, r9
	BLT	.LBB3_102
; %bb.101:                              ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p2
.LBB3_102:                              ; %sw.bb223
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_103:                              ; %sw.epilog
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 64]                   ; 4-byte Folded Reload
	BRA	.LBB3_214
.LBB3_104:                              ;   in Loop: Header=BB3_5 Depth=1
	STW	r3, [p7 + 88]                   ; 2-byte Folded Spill
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	STP	p1, [p7 + 84]                   ; 4-byte Folded Spill
	BRA	.LBB3_188
.LBB3_105:                              ; %while.body.i500.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r2, r11
	MOV	r3, r11
	STP	p1, [p7 + 104]                  ; 4-byte Folded Spill
	MOVP	p5, p1
	LDP	p4, [p7 + 100]                  ; 4-byte Folded Reload
.LBB3_106:                              ; %while.body.i500
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r8, r3
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__divsf3
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__gesf2
	ADDPI	p5, p5, 1
	LDP	p2, [p7 + 104]                  ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BGE	.LBB3_106
.LBB3_107:                              ; %while.cond29.preheader.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p5, [p7 + 104]                  ; 4-byte Folded Spill
	STP	p4, [p7 + 100]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r4, r11
	LDP	p1, [p7 + 72]                   ; 4-byte Folded Reload
	MOV	r5, r3
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	JAL	__ltsf2
	MOVI	r4, 101
	STW	r4, [p7 + 88]                   ; 2-byte Folded Spill
	LDP	p3, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r4, r6
	MOV	r5, r6
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB3_112
; %bb.108:                              ; %while.cond29.preheader.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r8, r11
	MOV	r9, r11
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__eqsf2
.LBB3_109:                              ; %while.cond29.preheader.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB3_112
; %bb.110:                              ; %while.body34.i
                                        ;   in Loop: Header=BB3_109 Depth=2
	MOV	r4, r11
	LDP	p1, [p7 + 100]                  ; 4-byte Folded Reload
	MOV	r5, r3
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	JAL	__mulsf3
	LDP	p2, [p7 + 72]                   ; 4-byte Folded Reload
	MOV	r4, r11
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	STP	p2, [p7 + 72]                   ; 4-byte Folded Spill
	JAL	__ltsf2
	LDP	p3, [p7 + 104]                  ; 4-byte Folded Reload
	ADDPI	p3, p3, -1
	LDP	p2, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r5, r4
	STP	p2, [p7 + 76]                   ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BGT	.LBB3_186
; %bb.111:                              ; %while.body34.i
                                        ;   in Loop: Header=BB3_109 Depth=2
	STP	p3, [p7 + 104]                  ; 4-byte Folded Spill
	MOV	r8, r11
	MOV	r9, r11
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__nesf2
	BRA	.LBB3_109
.LBB3_112:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	STP	p1, [p7 + 84]                   ; 4-byte Folded Spill
	BRA	.LBB3_189
.LBB3_113:                              ; %if.then.i410
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r2, 48
	STW	r2, [15728640]
	MOVI	r2, 120
	STW	r2, [15728640]
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	ADDPI	p2, p4, 1
.LBB3_114:                              ; %if.else145.thread
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r9, r10
	ADDPI	p1, p2, 1
.LBB3_115:                              ; %cond.end152
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 132]                  ; 4-byte Folded Spill
	LEA	p1, [.L.str.1]
	CMPI	r9, 88
	BEQ	.LBB3_117
; %bb.116:                              ; %cond.end152
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p1, [.L.str.2]
.LBB3_117:                              ; %cond.end152
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 96]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 204]
	ADDPI	p2, p1, 4
	STP	p2, [p7 + 204]
	MOV	r12, r7
	MOV	r13, r7
	LDP	p4, [p1]
	STP	p6, [p7 + 84]                   ; 4-byte Folded Spill
.LBB3_118:                              ; %do.body.i426
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	MOVP	p1, p4
	LDP	p5, [p7 + 104]                  ; 4-byte Folded Reload
	MOVP	p2, p5
	JAL	__udivsi3
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__mulsi3
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	SUB	r2, r8, r2
	SBC	r3, r9, r3
	LDP	p2, [p7 + 96]                   ; 4-byte Folded Reload
	ADDPP	p1, p2, p1
	LEA	p2, [p7 + 144]
	ADDPP	p2, p2, p6
	LDBU	r2, [p1]
	STB	r2, [p2]
	ADDPI	p6, p6, 1
	CMP	r8, r10
	CPC	r9, r11
	LDP	p4, [p7 + 108]                  ; 4-byte Folded Reload
	BHS	.LBB3_118
.LBB3_119:                              ; %number.loopexit669
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_120:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r10, r7
	MOV	r11, r7
	LDP	p2, [p7 + 92]                   ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	MOVP	p4, p3
	BEQ	.LBB3_122
; %bb.121:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p3, p5
.LBB3_122:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p3, [p7 + 96]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r2, r4
	MOV	r3, r4
	LDP	p2, [p7 + 60]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	MOVP	p3, p4
	BGT	.LBB3_124
; %bb.123:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p3, p5
.LBB3_124:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 80]                   ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	MOVP	p1, p4
	BNE	.LBB3_126
; %bb.125:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p5
.LBB3_126:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 92]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 56]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 84]                   ; 4-byte Folded Reload
	OR	r3, r13, r5
	OR	r2, r12, r4
	CMP	r2, r10
	CPC	r3, r11
	MOVP	p1, p4
	BNE	.LBB3_128
; %bb.127:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p5
.LBB3_128:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 60]                   ; 4-byte Folded Reload
	LDP	p4, [p7 + 120]                  ; 4-byte Folded Reload
	CMP	r4, r8
	CPC	r5, r9
	BGT	.LBB3_130
; %bb.129:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p5
	BRA	.LBB3_131
.LBB3_130:                              ;   in Loop: Header=BB3_5 Depth=1
	SUB	r12, r4, r8
	SBC	r13, r5, r9
.LBB3_131:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 52]                   ; 4-byte Folded Reload
	OR	r4, r2, r4
	ANDI	r3, r5, 0
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	ANDI	r2, r4, 1
	STP	p6, [p7 + 108]                  ; 4-byte Folded Spill
	ADDPP	p2, p4, p6
	LDP	p4, [p7 + 64]                   ; 4-byte Folded Reload
	SUB	r4, r8, r4
	SBC	r5, r9, r5
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	CMP	r2, r10
	CPC	r3, r11
	MOVP	p2, p1
	BGT	.LBB3_133
; %bb.132:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p2, p5
.LBB3_133:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p6, [p7 + 96]                   ; 4-byte Folded Reload
	OR	r9, r13, r7
	OR	r8, r12, r6
	LDP	p6, [p7 + 92]                   ; 4-byte Folded Reload
	OR	r7, r13, r9
	OR	r6, r12, r8
	CMP	r6, r10
	CPC	r7, r11
	MOVP	p6, p5
	BNE	.LBB3_135
; %bb.134:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p2
.LBB3_135:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r6, r10
	CPC	r7, r11
	MOVP	p4, p1
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	BNE	.LBB3_137
; %bb.136:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p4, p5
.LBB3_137:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 80]                   ; 4-byte Folded Reload
	CMP	r6, r10
	CPC	r7, r11
	STP	p4, [p7 + 96]                   ; 4-byte Folded Spill
	BNE	.LBB3_145
; %bb.138:                              ; %number
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r8, r4
	CPC	r9, r5
	BLT	.LBB3_145
; %bb.139:                              ; %while.body.i.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p5, p1
	BRA	.LBB3_142
.LBB3_140:                              ; %if.then.i.i
                                        ;   in Loop: Header=BB3_142 Depth=2
	MOVI	r4, 32
	STW	r4, [15728640]
.LBB3_141:                              ; %emit.exit.i
                                        ;   in Loop: Header=BB3_142 Depth=2
	STP	p5, [p7 + 132]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	ADDPI	p5, p3, -1
	CMP	r6, r4
	CPC	r7, r5
	BLS	.LBB3_145
.LBB3_142:                              ; %while.body.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p3, p5
	MOV	r8, r5
	MOV	r9, r5
	LDP	p5, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r10, r8
	CPC	r11, r9
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPI	p5, p2, 1
	BEQ	.LBB3_140
; %bb.143:                              ; %if.else.i.i
                                        ;   in Loop: Header=BB3_142 Depth=2
	LDP	p4, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r10, r8
	CPC	r11, r9
	BHS	.LBB3_141
; %bb.144:                              ; %if.then2.i.i
                                        ;   in Loop: Header=BB3_142 Depth=2
	LDP	p4, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPP	p4, p4, p2
	MOVI	r4, 32
	STB	r4, [p4]
	BRA	.LBB3_141
.LBB3_145:                              ; %if.end188
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 104]                  ; 4-byte Folded Reload
	ANDI	r9, r7, 0
	ANDI	r8, r6, 1
	MOV	r6, r5
	MOV	r7, r5
	CMP	r8, r6
	CPC	r9, r7
	STP	p1, [p7 + 104]                  ; 4-byte Folded Spill
	BEQ	.LBB3_153
; %bb.146:                              ; %if.then190
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r8, 43
	LDP	p2, [p7 + 56]                   ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BNE	.LBB3_148
; %bb.147:                              ; %if.then190
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r8, 32
.LBB3_148:                              ; %if.then190
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r10, 45
	LDP	p1, [p7 + 84]                   ; 4-byte Folded Reload
	CMP	r2, r6
	CPC	r3, r7
	BNE	.LBB3_150
; %bb.149:                              ; %if.then190
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r10, r8
.LBB3_150:                              ; %if.then190
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p4, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r8, r6
	CPC	r9, r7
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPI	p4, p2, 1
	BEQ	.LBB3_154
; %bb.151:                              ; %if.else.i438
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r8, r6
	CPC	r9, r7
	BHS	.LBB3_155
; %bb.152:                              ; %if.then2.i445
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p3, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPP	p3, p3, p2
	STB	r10, [p3]
	BRA	.LBB3_155
.LBB3_153:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	BRA	.LBB3_155
.LBB3_154:                              ; %if.then.i447
                                        ;   in Loop: Header=BB3_5 Depth=1
	ANDI	r6, r10, 255
	STW	r6, [15728640]
.LBB3_155:                              ; %if.end199
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	ADDPP	p6, p6, p1
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BLT	.LBB3_162
; %bb.156:                              ; %while.body.i453.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	BRA	.LBB3_159
.LBB3_157:                              ; %if.then.i.i465
                                        ;   in Loop: Header=BB3_159 Depth=2
	MOVI	r4, 48
	STW	r4, [15728640]
.LBB3_158:                              ; %emit.exit.i460
                                        ;   in Loop: Header=BB3_159 Depth=2
	MOVP	p4, p6
	ADDPI	p6, p3, -1
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	BLS	.LBB3_163
.LBB3_159:                              ; %while.body.i453
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p3, p6
	MOV	r12, r5
	MOV	r13, r5
	CMP	r10, r12
	CPC	r11, r13
	ADDPI	p6, p4, 1
	BEQ	.LBB3_157
; %bb.160:                              ; %if.else.i.i457
                                        ;   in Loop: Header=BB3_159 Depth=2
	LDP	p2, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BHS	.LBB3_158
; %bb.161:                              ; %if.then2.i.i463
                                        ;   in Loop: Header=BB3_159 Depth=2
	ADDPP	p4, p5, p4
	MOVI	r4, 48
	STB	r4, [p4]
	BRA	.LBB3_158
.LBB3_162:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_163:                              ; %while.body202.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	BRA	.LBB3_166
.LBB3_164:                              ; %if.then.i477
                                        ;   in Loop: Header=BB3_166 Depth=2
	STW	r6, [15728640]
	ADDPI	p4, p4, 1
.LBB3_165:                              ; %emit.exit478
                                        ;   in Loop: Header=BB3_166 Depth=2
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r6, r5
	MOV	r7, r5
	MOVP	p6, p1
	CMP	r12, r6
	CPC	r13, r7
	BEQ	.LBB3_170
.LBB3_166:                              ; %while.body202
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ADDPI	p6, p6, -1
	LEA	p3, [p7 + 144]
	MOVP	p1, p6
	ADDPP	p3, p3, p6
	LDBU	r6, [p3]
	MOV	r12, r5
	MOV	r13, r5
	CMP	r10, r12
	CPC	r11, r13
	BEQ	.LBB3_164
; %bb.167:                              ; %if.else.i468
                                        ;   in Loop: Header=BB3_166 Depth=2
	ADDPI	p6, p4, 1
	LDP	p2, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BHS	.LBB3_169
; %bb.168:                              ; %if.then2.i475
                                        ;   in Loop: Header=BB3_166 Depth=2
	ADDPP	p4, p5, p4
	STB	r6, [p4]
.LBB3_169:                              ; %emit.exit478
                                        ;   in Loop: Header=BB3_166 Depth=2
	MOVP	p4, p6
	BRA	.LBB3_165
.LBB3_170:                              ; %while.end203
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 80]                   ; 4-byte Folded Reload
	CMP	r4, r6
	CPC	r5, r7
	BEQ	.LBB3_1
; %bb.171:                              ; %while.end203
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 96]                   ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BLT	.LBB3_1
; %bb.172:                              ; %while.body.i483.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 132]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	BRA	.LBB3_175
.LBB3_173:                              ; %if.then.i.i495
                                        ;   in Loop: Header=BB3_175 Depth=2
	MOVI	r6, 32
	STW	r6, [15728640]
.LBB3_174:                              ; %emit.exit.i490
                                        ;   in Loop: Header=BB3_175 Depth=2
	STP	p1, [p7 + 132]                  ; 4-byte Folded Spill
	ADDPI	p1, p2, -1
	MOVP	p5, p6
	CMP	r4, r10
	CPC	r5, r11
	BLS	.LBB3_3
.LBB3_175:                              ; %while.body.i483
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p2, p1
	MOV	r2, r11
	MOVP	p6, p5
	MOV	r3, r11
	LDP	p3, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPI	p1, p4, 1
	BEQ	.LBB3_173
; %bb.176:                              ; %if.else.i.i487
                                        ;   in Loop: Header=BB3_175 Depth=2
	LDP	p5, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BHS	.LBB3_174
; %bb.177:                              ; %if.then2.i.i493
                                        ;   in Loop: Header=BB3_175 Depth=2
	ADDPP	p3, p3, p4
	MOVI	r8, 32
	STB	r8, [p3]
	BRA	.LBB3_174
.LBB3_178:                              ;   in Loop: Header=BB3_5 Depth=1
	ADDPP	p2, p3, p2
	SUB	r8, r12, r4
	SBC	r9, r13, r5
.LBB3_179:                              ; %cond.true111
                                        ;   in Loop: Header=BB3_5 Depth=1
	SHRI	r4, r7, 15
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r5, r7
	STP	p2, [p7 + 84]                   ; 4-byte Folded Spill
	MOVP	p2, p4
	BRA	.LBB3_181
.LBB3_180:                              ; %do.body.i
                                        ;   in Loop: Header=BB3_181 Depth=2
	ANDI	r3, r5, 0
	ANDI	r2, r4, 1
	ADDPI	p6, p6, 1
	CMP	r2, r8
	CPC	r3, r9
	LDP	p1, [p7 + 96]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 88]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 124]                  ; 4-byte Folded Reload
	BNE	.LBB3_120
.LBB3_181:                              ; %do.body.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	MOV	r8, r7
	MOV	r9, r7
	STP	p4, [p7]
                                        ; kill: def $r10 killed $r10 killed $p5 def $p5
	MOV	r11, r7
	STP	p2, [p7 + 104]                  ; 4-byte Folded Spill
	MOVP	p3, p5
	JAL	__udivdi3
	STP	p4, [p7]
	STP	p1, [p7 + 96]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 88]                   ; 4-byte Folded Spill
	MOVP	p3, p5
	JAL	__muldi3
	LDP	p3, [p7 + 108]                  ; 4-byte Folded Reload
	CMP	r6, r10
	CPC	r7, r11
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	BLO	.LBB3_183
; %bb.182:                              ; %do.body.i
                                        ;   in Loop: Header=BB3_181 Depth=2
	MOVP	p2, p4
.LBB3_183:                              ; %do.body.i
                                        ;   in Loop: Header=BB3_181 Depth=2
	SUB	r2, r6, r2
	SBC	r3, r7, r3
	LEA	p3, [.L.str.2]
	ADDPP	p1, p1, p3
	LDBU	r2, [p1]
	LEA	p3, [p7 + 144]
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	ADDPP	p3, p3, p6
	STB	r2, [p3]
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	CMP	r2, r8
	CPC	r3, r9
	BEQ	.LBB3_180
; %bb.184:                              ; %do.body.i
                                        ;   in Loop: Header=BB3_181 Depth=2
	MOVP	p2, p4
	BRA	.LBB3_180
.LBB3_186:                              ;   in Loop: Header=BB3_5 Depth=1
	STP	p3, [p7 + 84]                   ; 4-byte Folded Spill
	BRA	.LBB3_189
.LBB3_187:                              ;   in Loop: Header=BB3_5 Depth=1
	STP	p5, [p7 + 84]                   ; 4-byte Folded Spill
.LBB3_188:                              ; %if.end36.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_189:                              ; %if.end36.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 100]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 32]                   ; 4-byte Folded Reload
	MOV	r3, r11
	LDP	p3, [p7 + 92]                   ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	MOVP	p6, p3
	BLT	.LBB3_191
; %bb.190:                              ; %if.end36.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p1
.LBB3_191:                              ; %if.end36.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p1, [p7 + 32]                   ; 4-byte Folded Spill
	CMP	r6, r10
	CPC	r7, r11
	STP	p2, [p7 + 100]                  ; 4-byte Folded Spill
	BLT	.LBB3_194
; %bb.192:                              ; %for.body.i.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r2, r11
	LDP	p3, [p7 + 28]                   ; 4-byte Folded Reload
	MOV	r3, r7
	MOV	r8, r11
	MOV	r9, r11
	MOVP	p5, p6
.LBB3_193:                              ; %for.body.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r4, r5
	LDP	p3, [p7 + 100]                  ; 4-byte Folded Reload
	MOV	r5, r7
	JAL	__divsf3
	ADDPI	p4, p4, 1
	CMP	r8, r10
	CPC	r9, r11
	BLT	.LBB3_193
	BRA	.LBB3_195
.LBB3_194:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 28]                   ; 4-byte Folded Reload
	MOV	r2, r11
	STP	p1, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p5, p6
.LBB3_195:                              ; %for.cond.cleanup.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p5, [p7 + 96]                   ; 4-byte Folded Spill
	MOVP	p2, p1
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	JAL	__addsf3
	MOVP	p2, p1
	LDW	r2, [p7 + 88]                   ; 2-byte Folded Reload
	ANDI	r10, r2, 255
	CMPI	r10, 101
	STW	r10, [p7 + 68]                  ; 2-byte Folded Spill
	BNE	.LBB3_198
; %bb.196:                              ; %for.cond.cleanup.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	MOV	r8, r3
	LDP	p1, [p7 + 100]                  ; 4-byte Folded Reload
	MOV	r9, r3
	MOVP	p1, p2
	STP	p2, [p7 + 88]                   ; 4-byte Folded Spill
	MOVP	p2, p4
	JAL	__gesf2
	LDP	p3, [p7 + 76]                   ; 4-byte Folded Reload
	MOV	r4, r6
	MOV	r5, r6
	CMP	r2, r4
	CPC	r3, r5
	BLE	.LBB3_199
; %bb.197:                              ; %if.then52.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 88]                   ; 4-byte Folded Reload
	MOVP	p2, p4
	JAL	__divsf3
	MOVP	p2, p1
	LDP	p1, [p7 + 84]                   ; 4-byte Folded Reload
	ADDPI	p1, p1, 1
	STP	p1, [p7 + 84]                   ; 4-byte Folded Spill
.LBB3_198:                              ; %if.end55.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p4, [p7 + 140]                  ; 4-byte Folded Reload
	BRA	.LBB3_200
.LBB3_199:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p4, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 88]                   ; 4-byte Folded Reload
.LBB3_200:                              ; %if.end55.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p2, [p7 + 88]                   ; 4-byte Folded Spill
	MOVP	p1, p2
	JAL	__fixunssfsi
	MOVP	p6, p4
	MOV	r8, r13
	MOV	r9, r13
	STP	p1, [p7 + 60]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 112]                  ; 4-byte Folded Reload
.LBB3_201:                              ; %do.body.i.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p2, [p7 + 112]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 124]                  ; 4-byte Folded Reload
                                        ; kill: def $r10 killed $r10 killed $p5 def $p5
	MOV	r11, r13
	STP	p1, [p7 + 104]                  ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__udivsi3
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	MOVP	p2, p5
	JAL	__mulsi3
	LDP	p6, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 104]                  ; 4-byte Folded Reload
	SUB	r2, r6, r2
	SBC	r3, r7, r3
	LEA	p2, [.L.str.2]
	ADDPP	p1, p1, p2
	LDBU	r2, [p1]
	LEA	p2, [p7 + 184]
	ADDPP	p2, p2, p4
	STB	r2, [p2]
	LDP	p2, [p7 + 112]                  ; 4-byte Folded Reload
	MOV	r5, r13
	ADDPI	p4, p4, 1
	CMP	r6, r4
	CPC	r7, r5
	LDP	p1, [p7 + 108]                  ; 4-byte Folded Reload
	BHI	.LBB3_201
; %bb.202:                              ; %while.body60.i.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p2, [p7 + 112]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 120]                  ; 4-byte Folded Reload
.LBB3_203:                              ; %while.body60.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p1, p5
	LEA	p2, [p7 + 144]
	ADDPP	p2, p2, p1
	ADDPI	p4, p4, -1
	LEA	p3, [p7 + 184]
	ADDPP	p3, p3, p4
	LDBU	r6, [p3]
	STB	r6, [p2]
	ADDPI	p5, p1, 1
	MOV	r4, r13
	MOV	r5, r13
	CMP	r8, r4
	CPC	r9, r5
	BNE	.LBB3_203
; %bb.204:                              ; %while.end65.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p6, [p7 + 92]                   ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BEQ	.LBB3_208
; %bb.205:                              ; %if.then67.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p2, [p7 + 144]
	ADDPP	p2, p2, p5
	MOVI	r6, 46
	STB	r6, [p2]
	ADDPI	p5, p1, 2
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BLT	.LBB3_209
; %bb.206:                              ; %for.body75.i.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 60]                   ; 4-byte Folded Reload
	MOVP	p4, p2
	JAL	__floatunsisf
	MOVP	p2, p1
	LDP	p1, [p7 + 88]                   ; 4-byte Folded Reload
	JAL	__subsf3
	MOVP	p2, p4
	MOV	r8, r5
	MOV	r9, r5
.LBB3_207:                              ; %for.body75.i
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOV	r4, r5
	LDP	p3, [p7 + 100]                  ; 4-byte Folded Reload
	MOV	r5, r7
	JAL	__mulsf3
	STP	p1, [p7 + 120]                  ; 4-byte Folded Spill
	JAL	__fixsfsi
	LEA	p2, [p7 + 144]
	ADDPP	p2, p2, p5
	ADDI	r6, r2, 48
	STB	r6, [p2]
	JAL	__floatsisf
	MOVP	p2, p1
	LDP	p1, [p7 + 120]                  ; 4-byte Folded Reload
	JAL	__subsf3
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
	ADDPI	p5, p5, 1
	ADDPI	p4, p4, 1
	LDP	p3, [p7 + 96]                   ; 4-byte Folded Reload
	CMP	r8, r6
	CPC	r9, r7
	BLT	.LBB3_207
	BRA	.LBB3_209
.LBB3_208:                              ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_209:                              ; %if.end87.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p6, p5
	MOVP	p4, p2
	LDW	r2, [p7 + 68]                   ; 2-byte Folded Reload
	CMPI	r2, 101
	BNE	.LBB3_213
; %bb.210:                              ; %if.then91.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p4
	MOV	r4, r3
	MOV	r5, r3
	MOVI	r2, 45
	LDP	p5, [p7 + 84]                   ; 4-byte Folded Reload
	CMP	r10, r4
	CPC	r11, r5
	BLT	.LBB3_212
; %bb.211:                              ; %if.then91.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOVI	r2, 43
.LBB3_212:                              ; %if.then91.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	SARI	r4, r11, 15
	MOV	r5, r4
	XOR	r7, r11, r5
	XOR	r6, r10, r4
	SUB	r6, r6, r4
	SBC	r7, r7, r5
	STP	p3, [p7 + 108]                  ; 4-byte Folded Spill
	LDP	p5, [p7 + 124]                  ; 4-byte Folded Reload
                                        ; kill: def $r10 killed $r10 killed $p5 def $p5
	MOV	r11, r9
	LEA	p2, [p7 + 144]
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	ADDPP	p4, p2, p6
	MOVI	r3, 101
	STB	r3, [p4]
	STB	r2, [p4 + 1]
	MOVP	p1, p3
	MOVP	p2, p5
	JAL	__udivsi3
	ADDI	r4, r2, 48
	STB	r4, [p4 + 2]
	MOVP	p2, p5
	JAL	__mulsi3
	LDP	p2, [p7 + 108]                  ; 4-byte Folded Reload
	SUB	r2, r4, r2
	SBC	r3, r5, r3
	ORI	r2, r2, 48
	STB	r2, [p4 + 3]
	LDP	p4, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	ADDPI	p6, p6, 4
.LBB3_213:                              ; %if.end113.i
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p1, [p7 + 144]
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 132]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 64]                   ; 4-byte Folded Reload
	MOVP	p5, p4
	MOVP	p4, p2
.LBB3_214:                              ; %sw.epilog
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p6, [p7 + 120]                  ; 4-byte Folded Spill
	SUB	r6, r2, r12
	SBC	r7, r3, r13
	MOV	r4, r11
	MOV	r5, r11
	LDP	p1, [p7 + 80]                   ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	STP	p3, [p7 + 104]                  ; 4-byte Folded Spill
	BNE	.LBB3_222
; %bb.215:                              ; %sw.epilog
                                        ;   in Loop: Header=BB3_5 Depth=1
	CMP	r6, r10
	CPC	r7, r11
	MOVP	p1, p5
	BLT	.LBB3_223
; %bb.216:                              ; %while.body.i513.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p5, [p7 + 128]                  ; 4-byte Folded Reload
	BRA	.LBB3_219
.LBB3_217:                              ; %if.then.i.i525
                                        ;   in Loop: Header=BB3_219 Depth=2
	MOVI	r12, 32
	STW	r12, [15728640]
.LBB3_218:                              ; %emit.exit.i520
                                        ;   in Loop: Header=BB3_219 Depth=2
	MOVP	p4, p3
	ADDPI	p3, p2, -1
	CMP	r4, r2
	CPC	r5, r3
	BLS	.LBB3_223
.LBB3_219:                              ; %while.body.i513
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p2, p3
	MOV	r6, r3
	MOV	r7, r3
	LDP	p6, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r12, r6
	CPC	r13, r7
	ADDPI	p3, p4, 1
	BEQ	.LBB3_217
; %bb.220:                              ; %if.else.i.i517
                                        ;   in Loop: Header=BB3_219 Depth=2
	CMP	r6, r10
	CPC	r7, r11
	BHS	.LBB3_218
; %bb.221:                              ; %if.then2.i.i523
                                        ;   in Loop: Header=BB3_219 Depth=2
	LDP	p6, [p7 + 136]                  ; 4-byte Folded Reload
	ADDPP	p6, p6, p4
	MOVI	r8, 32
	STB	r8, [p6]
	LDP	p5, [p7 + 128]                  ; 4-byte Folded Reload
	BRA	.LBB3_218
.LBB3_222:                              ;   in Loop: Header=BB3_5 Depth=1
	MOVP	p1, p5
.LBB3_223:                              ; %if.end241
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p2, [p7 + 120]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB3_231
; %bb.224:                              ; %for.body245.preheader
                                        ;   in Loop: Header=BB3_5 Depth=1
	MOV	r6, r3
	MOV	r7, r3
	STP	p3, [p7 + 132]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 108]                  ; 4-byte Folded Reload
	BRA	.LBB3_227
.LBB3_225:                              ; %if.then.i538
                                        ;   in Loop: Header=BB3_227 Depth=2
	STW	r12, [15728640]
	ADDPI	p4, p4, 1
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
.LBB3_226:                              ; %emit.exit539
                                        ;   in Loop: Header=BB3_227 Depth=2
	ADDPI	p3, p3, 1
	LDP	p6, [p7 + 120]                  ; 4-byte Folded Reload
	CMP	r6, r12
	CPC	r7, r13
	BGE	.LBB3_231
.LBB3_227:                              ; %for.body245
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ADDPP	p6, p2, p3
	LDBU	r12, [p6]
	LDP	p5, [p7 + 136]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 132]                  ; 4-byte Folded Reload
	CMP	r10, r2
	CPC	r11, r3
	BEQ	.LBB3_225
; %bb.228:                              ; %if.else.i528
                                        ;   in Loop: Header=BB3_227 Depth=2
	MOVP	p1, p4
	ADDPI	p4, p1, 1
	LDP	p2, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r8, r4
	CPC	r9, r5
	BHS	.LBB3_230
; %bb.229:                              ; %if.then2.i536
                                        ;   in Loop: Header=BB3_227 Depth=2
	LDP	p5, [p7 + 136]                  ; 4-byte Folded Reload
	ADDPP	p5, p5, p1
	STB	r12, [p5]
.LBB3_230:                              ; %emit.exit539
                                        ;   in Loop: Header=BB3_227 Depth=2
	LDP	p1, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 108]                  ; 4-byte Folded Reload
	BRA	.LBB3_226
.LBB3_231:                              ; %for.cond.cleanup
                                        ;   in Loop: Header=BB3_5 Depth=1
	STP	p4, [p7 + 132]                  ; 4-byte Folded Spill
	MOV	r4, r3
	MOV	r5, r3
	MOVP	p5, p1
	LDP	p1, [p7 + 80]                   ; 4-byte Folded Reload
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB3_3
; %bb.232:                              ; %for.cond.cleanup
                                        ;   in Loop: Header=BB3_5 Depth=1
	LDP	p1, [p7 + 104]                  ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BGE	.LBB3_235
	BRA	.LBB3_3
.LBB3_233:                              ; %if.then.i.i556
                                        ;   in Loop: Header=BB3_235 Depth=2
	MOVI	r6, 32
	STW	r6, [15728640]
.LBB3_234:                              ; %emit.exit.i551
                                        ;   in Loop: Header=BB3_235 Depth=2
	STP	p1, [p7 + 132]                  ; 4-byte Folded Spill
	ADDPI	p1, p2, -1
	MOVP	p5, p6
	CMP	r4, r10
	CPC	r5, r11
	BLS	.LBB3_3
.LBB3_235:                              ; %while.body.i544
                                        ;   Parent Loop BB3_5 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOVP	p2, p1
	MOV	r2, r11
	MOVP	p6, p5
	MOV	r3, r11
	LDP	p3, [p7 + 136]                  ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	ADDPI	p1, p4, 1
	BEQ	.LBB3_233
; %bb.236:                              ; %if.else.i.i548
                                        ;   in Loop: Header=BB3_235 Depth=2
	LDP	p5, [p7 + 128]                  ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BHS	.LBB3_234
; %bb.237:                              ; %if.then2.i.i554
                                        ;   in Loop: Header=BB3_235 Depth=2
	ADDPP	p3, p3, p4
	MOVI	r8, 32
	STB	r8, [p3]
	BRA	.LBB3_234
.LBB3_238:                              ; %if.then.i508
                                        ;   in Loop: Header=BB3_5 Depth=1
	LEA	p1, [p7 + 144]
	STP	p1, [p7 + 108]                  ; 4-byte Folded Spill
	LEA	p2, [.L.str.3]
	JAL	strcpy
	LDP	p5, [p7 + 140]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 4]                    ; 4-byte Folded Reload
	MOV	r3, r11
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p6, p1
	BRA	.LBB3_103
.LBB3_239:                              ; %for.end268
	MOVI	r4, 0
	MOV	r5, r4
	CMP	r2, r4
	CPC	r3, r5
	LDP	p3, [p7 + 128]                  ; 4-byte Folded Reload
	LDP	p4, [p7 + 132]                  ; 4-byte Folded Reload
	BEQ	.LBB3_244
; %bb.240:                              ; %for.end268
	CMP	r6, r4
	CPC	r7, r5
	BEQ	.LBB3_244
; %bb.241:                              ; %if.then272
	CMP	r8, r6
	CPC	r9, r7
	MOVP	p2, p4
	BLO	.LBB3_243
; %bb.242:                              ; %if.then272
	ADDPI	p2, p3, -1
.LBB3_243:                              ; %if.then272
	ADDPP	p1, p1, p2
	MOVI	r4, 0
	STB	r4, [p1]
.LBB3_244:                              ; %if.end283
	LDP	p6, [p7 + 12]                   ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 200]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 196]                  ; 4-byte Folded Reload
	ADDPI	p7, p7, 204
	RET
	.section	.rodata,"a",@progbits
	.p2align	1, 0x0
.LJTI3_0:
	.long	.LBB3_22
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_24
	.long	.LBB3_21
	.long	.LBB3_90
	.long	.LBB3_13
	.long	.LBB3_90
	.long	.LBB3_90
	.long	.LBB3_11
.LJTI3_1:
	.long	.LBB3_17
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_23
	.long	.LBB3_18
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_25
	.long	.LBB3_19
.LJTI3_2:
	.long	.LBB3_92
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_44
	.long	.LBB3_44
	.long	.LBB3_44
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_51
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_91
	.long	.LBB3_81
	.long	.LBB3_44
	.long	.LBB3_44
	.long	.LBB3_44
	.long	.LBB3_3
	.long	.LBB3_81
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_51
	.long	.LBB3_51
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_96
	.long	.LBB3_3
	.long	.LBB3_72
	.long	.LBB3_3
	.long	.LBB3_3
	.long	.LBB3_51
                                        ; -- End function
	.text
	.globl	vprintf                         ; -- Begin function vprintf
	.p2align	1
vprintf:                                ; @vprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p3, p1
	MOVP	p4, p6
	STP	p2, [p7]
	MOVI	r2, 0
	MOV	r3, r2
	MOVP	p2, p1
	JAL	vsnprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
                                        ; -- End function
	.globl	printf                          ; -- Begin function printf
	.p2align	1
printf:                                 ; @printf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	LEA	p1, [p7 + 16]
	STP	p1, [p7 + 4]
	STP	p1, [p7]
	MOVI	r2, 0
	MOV	r3, r2
	LDP	p3, [p7 + 12]
	MOVP	p2, p1
	JAL	vsnprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	snprintf                        ; -- Begin function snprintf
	.p2align	1
snprintf:                               ; @snprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	LEA	p1, [p7 + 24]
	STP	p1, [p7 + 4]
	STP	p1, [p7]
	LDP	p1, [p7 + 12]
	LDP	p2, [p7 + 16]
	LDP	p3, [p7 + 20]
	JAL	vsnprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	sprintf                         ; -- Begin function sprintf
	.p2align	1
sprintf:                                ; @sprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	MOVP	p4, p6
	LEA	p1, [p7 + 20]
	STP	p1, [p7 + 4]
	STP	p1, [p7]
	MOVI	r5, 0
	MOVI	r4, 32767
	LDP	p1, [p7 + 12]
	LDP	p3, [p7 + 16]
	JAL	vsnprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.globl	vsprintf                        ; -- Begin function vsprintf
	.p2align	1
vsprintf:                               ; @vsprintf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -12
	STP	p4, [p7 + 8]                    ; 4-byte Folded Spill
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	MOVP	p5, p2
	MOVP	p4, p6
	STP	p3, [p7]
	MOVI	r5, 0
	MOVI	r4, 32767
	MOVP	p3, p5
	JAL	vsnprintf
	MOVP	p6, p4
	LDP	p4, [p7 + 8]                    ; 4-byte Folded Reload
	LDP	p5, [p7 + 4]                    ; 4-byte Folded Reload
	ADDPI	p7, p7, 12
	RET
                                        ; -- End function
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:                                 ; @.str
	.asciz	"(null)"

.L.str.1:                               ; @.str.1
	.asciz	"0123456789ABCDEF"

.L.str.2:                               ; @.str.2
	.asciz	"0123456789abcdef"

.L.str.3:                               ; @.str.3
	.asciz	"nan"

.L.str.4:                               ; @.str.4
	.asciz	"inf"

	.section	".note.GNU-stack","",@progbits
	.addrsig
