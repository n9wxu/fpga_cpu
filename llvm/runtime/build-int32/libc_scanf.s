	.text
	.globl	vsscanf                         ; -- Begin function vsscanf
	.p2align	1
vsscanf:                                ; @vsscanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 24]                   ; 4-byte Folded Spill
	MOVP	p4, p6
	STP	p3, [p7 + 20]
	MOVI	r12, 0
	MOV	r13, r12
	STP	p6, [p7 + 8]
	STP	p1, [p7 + 4]
	LDBU	r2, [p1]
	MOVI	r10, -1
	MOV	r11, r10
	CMPI	r2, 0
	BEQ	.LBB0_2
; %bb.1:                                ; %entry
	MOV	r3, r12
	MOVP	p5, p1
.LBB0_2:                                ; %entry
	STP	p6, [p7 + 16]
	STP	p5, [p7 + 12]
	STP	p3, [p7]
	LEA	p1, [p7 + 4]
	LEA	p3, [p7]
	JAL	scan
	MOVP	p6, p4
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 24]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.p2align	1                               ; -- Begin function scan
scan:                                   ; @scan
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -430
	STP	p4, [p7 + 426]                  ; 4-byte Folded Spill
	STP	p5, [p7 + 422]                  ; 4-byte Folded Spill
	STP	p3, [p7 + 98]                   ; 4-byte Folded Spill
	MOVP	p4, p1
	MOVI	r11, 0
	MOV	r10, r11
	LDBU	r2, [p2]
	CMPI	r2, 0
	BEQ	.LBB1_213
; %bb.1:                                ; %while.body.lr.ph.lr.ph
	STP	p5, [p7 + 118]                  ; 4-byte Folded Spill
	MOVI	r6, -1
	STP	p2, [p7 + 146]                  ; 4-byte Folded Spill
	MOVI	r4, -5
	STP	p2, [p7 + 138]                  ; 4-byte Folded Spill
	MOVI	r4, 1
	STP	p2, [p7 + 134]                  ; 4-byte Folded Spill
	MOVI	r4, 2
	STP	p2, [p7 + 102]                  ; 4-byte Folded Spill
	MOVI	r4, 9
	STP	p2, [p7 + 90]                   ; 4-byte Folded Spill
	MOVI	r4, 108
	STP	p2, [p7 + 50]                   ; 4-byte Folded Spill
	MOVI	r4, 76
	STP	p2, [p7 + 38]                   ; 4-byte Folded Spill
	MOVI	r4, 32767
	STP	p2, [p7 + 70]                   ; 4-byte Folded Spill
	MOVI	r4, 8
	STP	p2, [p7 + 82]                   ; 4-byte Folded Spill
	MOVI	r4, 10
	STP	p2, [p7 + 130]                  ; 4-byte Folded Spill
	MOVI	r4, 45
	STP	p2, [p7 + 46]                   ; 4-byte Folded Spill
	MOVI	r4, 16
	STP	p2, [p7 + 58]                   ; 4-byte Folded Spill
	MOVI	r4, 48
	STP	p2, [p7 + 30]                   ; 4-byte Folded Spill
	MOVI	r4, 88
	STP	p2, [p7 + 26]                   ; 4-byte Folded Spill
	MOVI	r4, -26
	STP	p2, [p7 + 86]                   ; 4-byte Folded Spill
	MOVI	r4, 43
	STP	p2, [p7 + 78]                   ; 4-byte Folded Spill
	MOVI	r4, 47
	STP	p2, [p7 + 54]                   ; 4-byte Folded Spill
	MOVI	r4, 68
	STP	p2, [p7 + 106]                  ; 4-byte Folded Spill
	MOVI	r4, 101
	STP	p2, [p7 + 94]                   ; 4-byte Folded Spill
	MOVI	r4, 69
	STP	p2, [p7 + 74]                   ; 4-byte Folded Spill
	MOVI	r4, 3
	STP	p2, [p7 + 66]                   ; 4-byte Folded Spill
	MOVI	r4, 256
	STP	p2, [p7 + 42]                   ; 4-byte Folded Spill
	MOVI	r10, 5
	STP	p5, [p7 + 158]                  ; 4-byte Folded Spill
	MOVI	r4, 32
	STP	p2, [p7 + 150]                  ; 4-byte Folded Spill
	MOVI	r4, 104
	STP	p2, [p7 + 34]                   ; 4-byte Folded Spill
	MOVI	r4, 72
	STP	p2, [p7 + 62]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
	STP	p6, [p7 + 110]                  ; 4-byte Folded Spill
	STP	p4, [p7 + 154]                  ; 4-byte Folded Spill
	BRA	.LBB1_4
.LBB1_2:                                ; %if.then1.i626
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p4 + 8]
.LBB1_3:                                ; %while.cond.backedge
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p2, p6, 1
	LDBU	r2, [p6 + 1]
	CMPI	r2, 0
	BEQ	.LBB1_214
.LBB1_4:                                ; %while.body
                                        ; =>This Loop Header: Depth=1
                                        ;     Child Loop BB1_16 Depth 2
                                        ;     Child Loop BB1_29 Depth 2
                                        ;     Child Loop BB1_52 Depth 2
                                        ;     Child Loop BB1_88 Depth 2
                                        ;       Child Loop BB1_94 Depth 3
                                        ;     Child Loop BB1_128 Depth 2
                                        ;     Child Loop BB1_67 Depth 2
                                        ;     Child Loop BB1_187 Depth 2
	STP	p2, [p7 + 146]                  ; 4-byte Folded Spill
	ANDI	r12, r2, 255
	CMPI	r12, 32
	BEQ	.LBB1_14
; %bb.5:                                ; %while.body
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p3, [p7 + 142]                  ; 4-byte Folded Spill
	ANDI	r7, r3, 0
	ANDI	r6, r2, 255
	ADDPI	p1, p3, -14
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 138]                  ; 4-byte Folded Reload
	MOV	r5, r6
	STP	p2, [p7 + 138]                  ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BHS	.LBB1_14
; %bb.6:                                ; %if.end
                                        ;   in Loop: Header=BB1_4 Depth=1
	CMPI	r12, 37
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
	BNE	.LBB1_8
; %bb.7:                                ; %lor.lhs.false
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDBU	r2, [p2 + 1]
	CMPI	r2, 37
	BNE	.LBB1_24
.LBB1_8:                                ; %if.then12
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r11
	CMPI	r12, 37
	STP	p1, [p7 + 134]                  ; 4-byte Folded Spill
	BEQ	.LBB1_10
; %bb.9:                                ; %if.then12
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r13
	MOV	r3, r13
.LBB1_10:                               ; %if.then12
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPP	p6, p2, p1
	LDBU	r2, [p6]
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p4 + 8]
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB1_214
; %bb.11:                               ; %if.end.i622
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_23
; %bb.12:                               ; %if.then1.i626
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r2, [p1 + 1]
	MOV	r4, r6
	MOV	r5, r6
	CMPI	r2, 0
	BEQ	.LBB1_2
; %bb.13:                               ; %if.then1.i626
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	MOVP	p2, p1
	BRA	.LBB1_2
.LBB1_14:                               ; %while.cond2.preheader
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 8]
	BRA	.LBB1_16
.LBB1_15:                               ; %if.then1.i
                                        ;   in Loop: Header=BB1_16 Depth=2
	STP	p1, [p4 + 8]
.LBB1_16:                               ; %while.cond2
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ADDPI	p2, p1, -9
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	CMP	r4, r12
	CPC	r5, r13
	BHS	.LBB1_21
.LBB1_17:                               ; %if.end.i
                                        ;   in Loop: Header=BB1_16 Depth=2
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_20
; %bb.18:                               ; %if.then1.i
                                        ;   in Loop: Header=BB1_16 Depth=2
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r7, r6
	CMPI	r4, 0
	MOVP	p1, p3
	BEQ	.LBB1_15
; %bb.19:                               ; %if.then1.i
                                        ;   in Loop: Header=BB1_16 Depth=2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_15
.LBB1_20:                               ; %if.else.i
                                        ;   in Loop: Header=BB1_16 Depth=2
	LDP	p1, [p4 + 4]
	MOVP	p5, p3
	JAL	fgetc
	MOVP	p3, p5
	LDP	p4, [p7 + 154]                  ; 4-byte Folded Reload
	STP	p1, [p4 + 8]
	BRA	.LBB1_16
.LBB1_21:                               ; %while.cond2
                                        ;   in Loop: Header=BB1_16 Depth=2
	LDP	p2, [p7 + 150]                  ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r13
	STP	p2, [p7 + 150]                  ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_17
; %bb.22:                               ; %while.cond.backedge.loopexit
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 146]                  ; 4-byte Folded Reload
	BRA	.LBB1_3
.LBB1_23:                               ; %if.else.i633
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 4]
	STP	p6, [p7 + 146]                  ; 4-byte Folded Spill
	JAL	fgetc
	LDP	p6, [p7 + 146]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	STP	p1, [p4 + 8]
	BRA	.LBB1_3
.LBB1_24:                               ; %if.end26
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOV	r6, r2
	LDP	p1, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	STW	r6, [p7 + 8]                    ; 2-byte Folded Spill
	CMPI	r6, 42
	BEQ	.LBB1_26
; %bb.25:                               ; %if.end26
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r7
.LBB1_26:                               ; %if.end26
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPP	p1, p2, p1
	MOVP	p6, p1
	LDBU	r4, [p1]
	ANDI	r3, r5, 0
	STP	p2, [p7 + 114]                  ; 4-byte Folded Spill
	ANDI	r2, r4, 255
	LDP	p2, [p7 + 90]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r11
	MOV	r6, r11
	MOV	r7, r11
	ADDPI	p1, p1, -48
	STP	p2, [p7 + 90]                   ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	LDP	p2, [p7 + 130]                  ; 4-byte Folded Reload
	STP	p3, [p7 + 10]                   ; 4-byte Folded Spill
	BLS	.LBB1_28
; %bb.27:                               ;   in Loop: Header=BB1_4 Depth=1
	MOVP	p1, p3
	BRA	.LBB1_30
.LBB1_28:                               ; %while.body38.preheader
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 146]                  ; 4-byte Folded Spill
	MOVP	p1, p3
.LBB1_29:                               ; %while.body38
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r7
	STP	p2, [p7 + 130]                  ; 4-byte Folded Spill
	LDBU	r6, [p6 + 1]
	ANDI	r13, r7, 0
	STP	p3, [p7 + 114]                  ; 4-byte Folded Spill
	ANDI	r12, r6, 255
	STP	p6, [p7 + 122]                  ; 4-byte Folded Spill
	JAL	__mulsi3
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 130]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 146]                  ; 4-byte Folded Reload
	ADDPP	p1, p3, p1
	ADDPI	p6, p6, 1
	LDP	p3, [p7 + 122]                  ; 4-byte Folded Reload
	ADDPI	p3, p3, -48
	STP	p3, [p7 + 146]                  ; 4-byte Folded Spill
	CMP	r6, r4
	CPC	r7, r5
	BLO	.LBB1_29
.LBB1_30:                               ; %while.end41
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p7 + 130]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	ANDI	r4, r4, 255
	CMPI	r4, 108
	BEQ	.LBB1_34
; %bb.31:                               ; %while.end41
                                        ;   in Loop: Header=BB1_4 Depth=1
	CMPI	r4, 104
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	BNE	.LBB1_40
; %bb.32:                               ; %if.then45
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDBU	r4, [p6 + 1]
	CMPI	r4, 104
	BEQ	.LBB1_36
; %bb.33:                               ; %if.then45
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p6, p6, 1
	BRA	.LBB1_37
.LBB1_34:                               ; %if.then56
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDBU	r4, [p6 + 1]
	CMPI	r4, 108
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	BNE	.LBB1_96
; %bb.35:                               ; %if.then61
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 38]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r11
	ADDPI	p6, p6, 2
	STP	p2, [p7 + 38]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 10]                   ; 4-byte Folded Spill
	BRA	.LBB1_40
.LBB1_36:                               ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p6, p6, 2
.LBB1_37:                               ; %if.then45
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p6, [p7 + 62]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r11
	CMPI	r4, 104
	STP	p6, [p7 + 62]                   ; 4-byte Folded Spill
	STP	p6, [p7 + 10]                   ; 4-byte Folded Spill
	BEQ	.LBB1_39
; %bb.38:                               ; %if.then45
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 34]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r13
	STP	p2, [p7 + 34]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 10]                   ; 4-byte Folded Spill
.LBB1_39:                               ; %if.then45
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
.LBB1_40:                               ; %if.end65thread-pre-split
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	LDBU	r4, [p6]
.LBB1_41:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r12 killed $r12 killed $p6 def $p6
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r11
	STP	p6, [p7 + 114]                  ; 4-byte Folded Spill
	ANDI	r4, r4, 255
	STW	r4, [p7 + 22]                   ; 2-byte Folded Spill
	CMPI	r4, 99
	BEQ	.LBB1_43
; %bb.42:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 70]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r13
	STP	p2, [p7 + 70]                   ; 4-byte Folded Spill
	STP	p2, [p7 + 114]                  ; 4-byte Folded Spill
.LBB1_43:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_45
; %bb.44:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 114]                  ; 4-byte Folded Spill
.LBB1_45:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPI	p1, p1, 1
	STP	p1, [p7 + 146]                  ; 4-byte Folded Spill
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 91
	BEQ	.LBB1_58
; %bb.46:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 99
	BEQ	.LBB1_58
; %bb.47:                               ; %if.end65
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 110
	BNE	.LBB1_50
; %bb.48:                               ; %if.then77
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r2, [p7 + 8]                    ; 2-byte Folded Reload
	CMPI	r2, 42
	BEQ	.LBB1_203
; %bb.49:                               ; %if.then79
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p4 + 12]
	SARI	r2, r7, 15
	MOV	r3, r2
	STP	p1, [p7]
	LDP	p1, [p7 + 98]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 10]                   ; 4-byte Folded Reload
	JAL	store_int
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	LDP	p6, [p7 + 110]                  ; 4-byte Folded Reload
	BRA	.LBB1_211
.LBB1_50:                               ; %while.cond90.preheader
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 8]
	BRA	.LBB1_52
.LBB1_51:                               ; %if.then1.i651
                                        ;   in Loop: Header=BB1_52 Depth=2
	STP	p1, [p4 + 8]
.LBB1_52:                               ; %while.cond90
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	ADDPI	p2, p1, -9
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	CMP	r4, r12
	CPC	r5, r13
	BHS	.LBB1_57
.LBB1_53:                               ; %if.end.i647
                                        ;   in Loop: Header=BB1_52 Depth=2
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_56
; %bb.54:                               ; %if.then1.i651
                                        ;   in Loop: Header=BB1_52 Depth=2
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_51
; %bb.55:                               ; %if.then1.i651
                                        ;   in Loop: Header=BB1_52 Depth=2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_51
.LBB1_56:                               ; %if.else.i658
                                        ;   in Loop: Header=BB1_52 Depth=2
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	STP	p1, [p4 + 8]
	BRA	.LBB1_52
.LBB1_57:                               ; %while.cond90
                                        ;   in Loop: Header=BB1_52 Depth=2
	LDP	p2, [p7 + 150]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_53
.LBB1_58:                               ; %if.end96
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p4 + 8]
	MOV	r2, r6
	MOV	r3, r6
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_216
; %bb.59:                               ; %if.end103
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p7 + 122]                  ; 4-byte Folded Spill
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	ADDI	r2, r2, -69
	CMPI	r2, 51
	BHI	.LBB1_148
; %bb.60:                               ; %if.end103
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 82]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r5
	MOV	r3, r5
	SHLI	r4, r3, 2
	SHRI	r5, r2, 14
	OR	r5, r4, r5
	SHLI	r4, r2, 2
	LEA	p1, [.LJTI1_0]
	ADDPP	p1, p2, p1
	LDP	p1, [p1]
	STP	p6, [p7 + 82]                   ; 4-byte Folded Spill
	JR	p1
.LBB1_61:                               ; %if.then259
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 54]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	STP	p2, [p7 + 22]                   ; 4-byte Folded Spill
	BLT	.LBB1_63
; %bb.62:                               ; %if.then259
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 22]                   ; 4-byte Folded Spill
.LBB1_63:                               ; %if.then259
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 54]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB1_109
; %bb.64:                               ; %while.body267.preheader
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r12, r3
	MOV	r13, r3
	LDP	p1, [p7 + 122]                  ; 4-byte Folded Reload
	BRA	.LBB1_67
.LBB1_65:                               ; %if.else.i703
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_66:                               ; %cleanup292
                                        ;   in Loop: Header=BB1_67 Depth=2
	ADDPI	p6, p6, 1
	STP	p1, [p4 + 8]
	LDP	p2, [p7 + 22]                   ; 4-byte Folded Reload
	CMP	r12, r4
	CPC	r13, r5
	BGE	.LBB1_110
.LBB1_67:                               ; %while.body267
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 130]                  ; 4-byte Folded Reload
	MOV	r10, r4
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r11, r5
	MOVP	p6, p1
	ADDPI	p2, p6, -48
	CMP	r4, r10
	CPC	r5, r11
	MOVP	p2, p6
	BLO	.LBB1_75
; %bb.68:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p7 + 106]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r11
	STP	p1, [p7 + 106]                  ; 4-byte Folded Spill
	CMP	r4, r2
	CPC	r5, r3
	BGT	.LBB1_72
; %bb.69:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p2, [p7 + 102]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	ADDPI	p1, p6, -45
	STP	p2, [p7 + 102]                  ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	MOVP	p2, p6
	BLO	.LBB1_75
; %bb.70:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p7 + 78]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	STP	p1, [p7 + 78]                   ; 4-byte Folded Spill
	BRA	.LBB1_74
.LBB1_72:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p7 + 94]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	STP	p1, [p7 + 94]                   ; 4-byte Folded Spill
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_75
; %bb.73:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p7 + 74]                   ; 4-byte Folded Reload
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	STP	p1, [p7 + 74]                   ; 4-byte Folded Spill
.LBB1_74:                               ; %switch.early.test
                                        ;   in Loop: Header=BB1_67 Depth=2
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB1_71
.LBB1_75:                               ; %if.end.i692
                                        ;   in Loop: Header=BB1_67 Depth=2
	LEA	p1, [p7 + 166]
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPP	p1, p1, p6
	STB	r4, [p1]
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_65
; %bb.76:                               ; %if.then1.i696
                                        ;   in Loop: Header=BB1_67 Depth=2
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_66
; %bb.77:                               ; %if.then1.i696
                                        ;   in Loop: Header=BB1_67 Depth=2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_66
.LBB1_78:                               ; %if.then115
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 42]                   ; 4-byte Folded Reload
	MOVI	r7, 0
	LEA	p1, [p7 + 166]
	MOV	r4, r7
	MOVP	p5, p3
	JAL	memset
	MOVP	p2, p5
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 91
	STP	p2, [p7 + 42]                   ; 4-byte Folded Spill
	BNE	.LBB1_117
; %bb.79:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 126]                  ; 4-byte Folded Reload
	LDBU	r2, [p2 + 1]
	CMPI	r2, 94
	BNE	.LBB1_81
; %bb.80:                               ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p2, p2, 2
	STP	p2, [p7 + 146]                  ; 4-byte Folded Spill
.LBB1_81:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r13
	LDP	p3, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	MOV	r7, r13
	CMPI	r2, 94
	BEQ	.LBB1_83
; %bb.82:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOVP	p3, p2
.LBB1_83:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	CMPI	r2, 94
	LDP	p1, [p7 + 126]                  ; 4-byte Folded Reload
	BEQ	.LBB1_85
; %bb.84:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
.LBB1_85:                               ; %if.then119
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p7 + 18]                   ; 4-byte Folded Spill
	ADDPP	p1, p1, p3
	LDBU	r2, [p1]
	CMPI	r2, 93
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
	BNE	.LBB1_88
; %bb.86:                               ; %if.then129
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
	STB	r2, [p7 + 259]
	ADDPI	p2, p2, 1
	BRA	.LBB1_88
.LBB1_87:                               ; %if.else161
                                        ;   in Loop: Header=BB1_88 Depth=2
	ADDPI	p2, p2, 1
	LEA	p3, [p7 + 166]
	ADDPP	p1, p3, p1
	MOVP	p3, p2
	LDP	p2, [p7 + 134]                  ; 4-byte Folded Reload
	STB	r4, [p1]
	MOVP	p2, p3
.LBB1_88:                               ; %while.cond133
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Loop Header: Depth=2
                                        ;       Child Loop BB1_94 Depth 3
	LDBU	r2, [p2]
	CMPI	r2, 0
	BEQ	.LBB1_118
; %bb.89:                               ; %while.cond133
                                        ;   in Loop: Header=BB1_88 Depth=2
	CMPI	r2, 93
	BEQ	.LBB1_118
; %bb.90:                               ; %while.body139
                                        ;   in Loop: Header=BB1_88 Depth=2
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r7
	LDBU	r6, [p2 + 1]
	CMPI	r6, 45
	BNE	.LBB1_87
; %bb.91:                               ; %land.lhs.true144
                                        ;   in Loop: Header=BB1_88 Depth=2
	LDBU	r6, [p2 + 2]
	CMPI	r6, 0
	BEQ	.LBB1_87
; %bb.92:                               ; %land.lhs.true144
                                        ;   in Loop: Header=BB1_88 Depth=2
	CMPI	r6, 93
	BEQ	.LBB1_87
; %bb.93:                               ; %for.cond.preheader
                                        ;   in Loop: Header=BB1_88 Depth=2
	CMP	r2, r6
	BHI	.LBB1_95
.LBB1_94:                               ; %for.body
                                        ;   Parent Loop BB1_4 Depth=1
                                        ;     Parent Loop BB1_88 Depth=2
                                        ; =>    This Inner Loop Header: Depth=3
	LEA	p3, [p7 + 166]
	ADDPP	p3, p3, p1
	LDP	p6, [p7 + 134]                  ; 4-byte Folded Reload
	STB	r12, [p3]
	LDBU	r6, [p2 + 2]
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r13
	ADDPI	p6, p1, 1
	CMP	r2, r6
	CPC	r3, r7
	MOVP	p1, p6
	BLO	.LBB1_94
.LBB1_95:                               ; %for.cond.cleanup
                                        ;   in Loop: Header=BB1_88 Depth=2
	ADDPI	p2, p2, 3
	BRA	.LBB1_88
.LBB1_96:                               ;   in Loop: Header=BB1_4 Depth=1
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	STW	r4, [p7 + 146]                  ; 2-byte Folded Spill
	LDP	p2, [p7 + 50]                   ; 4-byte Folded Reload
	MOV	r5, r11
	STP	p2, [p7 + 50]                   ; 4-byte Folded Spill
	ADDPI	p6, p6, 1
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 50]                   ; 4-byte Folded Reload
	STP	p2, [p7 + 10]                   ; 4-byte Folded Spill
	LDW	r4, [p7 + 146]                  ; 2-byte Folded Reload
	BRA	.LBB1_41
.LBB1_97:                               ; %cond.end339.fold.split610
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 58]                   ; 4-byte Folded Reload
.LBB1_98:                               ; %cond.end339
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOV	r12, r2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r3
.LBB1_99:                               ; %cond.end339
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p6, [p7 + 22]                   ; 4-byte Folded Spill
	LDP	p6, [p7 + 46]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r3
	LDP	p2, [p7 + 122]                  ; 4-byte Folded Reload
	CMP	r4, r12
	CPC	r5, r13
	STP	p6, [p7 + 46]                   ; 4-byte Folded Spill
	BNE	.LBB1_102
; %bb.100:                              ; %land.lhs.true350
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BGE	.LBB1_104
; %bb.101:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r5
	MOV	r3, r5
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p6, [p7 + 122]                  ; 4-byte Folded Spill
	BRA	.LBB1_152
.LBB1_102:                              ; %lor.lhs.false346
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 78]                   ; 4-byte Folded Reload
	MOV	r10, r2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r11, r3
	STP	p5, [p7 + 126]                  ; 4-byte Folded Spill
	MOV	r10, r3
	MOV	r11, r3
	LDP	p1, [p7 + 126]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB1_147
; %bb.103:                              ; %lor.lhs.false346
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	BLE	.LBB1_147
.LBB1_104:                              ; %if.end.i712
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOV	r2, r12
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r11
	STP	p1, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	MOV	r3, r11
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
	MOV	r4, r11
	MOV	r5, r11
	LDP	p1, [p7 + 122]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 126]                  ; 4-byte Folded Reload
	CMP	r2, r10
	CPC	r3, r11
	MOVP	p1, p2
	BEQ	.LBB1_106
; %bb.105:                              ; %if.end.i712
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 4]                    ; 4-byte Folded Spill
.LBB1_106:                              ; %if.end.i712
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p4 + 12]
	ADDPI	p2, p2, 1
	STP	p2, [p4 + 12]
	LDP	p2, [p4]
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_150
; %bb.107:                              ; %if.then1.i716
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p1, p2, 1
	STP	p1, [p4]
	LDBU	r4, [p2 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_151
; %bb.108:                              ; %if.then1.i716
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_151
.LBB1_109:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r12, r3
	MOV	r13, r3
.LBB1_110:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	LEA	p1, [p7 + 166]
	STP	p1, [p7 + 126]                  ; 4-byte Folded Spill
	ADDPP	p2, p1, p6
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	STB	r11, [p2]
	LEA	p2, [p7 + 162]
	JAL	strtof
	STP	p1, [p7 + 122]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 66]                   ; 4-byte Folded Reload
	MOV	r5, r11
	LDP	p6, [p7 + 162]
	LDP	p3, [p7 + 126]                  ; 4-byte Folded Reload
	MOVP	p1, p3
	CMP	r12, r6
	CPC	r13, r7
	STP	p2, [p7 + 66]                   ; 4-byte Folded Spill
	BEQ	.LBB1_112
; %bb.111:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r7
.LBB1_112:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r11
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB1_114
; %bb.113:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r6, r3
	MOV	r7, r3
.LBB1_114:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r12, [p7 + 8]                   ; 2-byte Folded Reload
	CMPI	r12, 42
	BEQ	.LBB1_177
; %bb.115:                              ; %while.end295
                                        ;   in Loop: Header=BB1_4 Depth=1
	ANDI	r13, r7, 0
	ANDI	r12, r6, 1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r6, r3
	MOV	r7, r3
	CMP	r12, r6
	CPC	r13, r7
	BNE	.LBB1_177
; %bb.116:                              ; %if.then304
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 98]                   ; 4-byte Folded Reload
	LDP	p2, [p6]
	ADDPI	p3, p2, 4
	STP	p3, [p6]
	LDP	p2, [p2]
	LDP	p1, [p7 + 122]                  ; 4-byte Folded Reload
	STP	p1, [p2]
	LDP	p2, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	LDP	p1, [p7 + 118]                  ; 4-byte Folded Reload
	ADDPI	p1, p1, 1
	STP	p1, [p7 + 118]                  ; 4-byte Folded Spill
	BRA	.LBB1_177
.LBB1_117:                              ;   in Loop: Header=BB1_4 Depth=1
	MOV	r2, r5
	STP	p1, [p7 + 18]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
	BRA	.LBB1_121
.LBB1_118:                              ; %while.end166
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r13
	CMPI	r2, 93
	BEQ	.LBB1_120
; %bb.119:                              ; %while.end166
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r6, r3
	MOV	r7, r3
.LBB1_120:                              ; %while.end166
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPP	p2, p2, p3
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
.LBB1_121:                              ; %if.end173
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r12, r7
	MOV	r13, r7
	LDW	r6, [p7 + 8]                    ; 2-byte Folded Reload
	CMPI	r6, 42
	BEQ	.LBB1_123
; %bb.122:                              ; %cond.false176
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 98]                   ; 4-byte Folded Reload
	LDP	p1, [p6]
	ADDPI	p3, p1, 4
	STP	p3, [p6]
	LDP	p6, [p1]
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
.LBB1_123:                              ; %cond.end177
                                        ;   in Loop: Header=BB1_4 Depth=1
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r7
	LDP	p3, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r6, r2
	CPC	r7, r3
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	STP	p2, [p7 + 146]                  ; 4-byte Folded Spill
	BLT	.LBB1_133
; %bb.124:                              ; %land.rhs182.lr.ph
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r5
	MOV	r3, r5
	STP	p1, [p7 + 126]                  ; 4-byte Folded Spill
	LDP	p1, [p4 + 8]
	LDP	p2, [p7 + 18]                   ; 4-byte Folded Reload
	ANDI	r4, r4, 255
	STW	r4, [p7 + 10]                   ; 2-byte Folded Spill
	STP	p6, [p7 + 122]                  ; 4-byte Folded Spill
	BRA	.LBB1_128
.LBB1_125:                              ; %if.else.i682
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_126:                              ; %if.end9.sink.split.i680
                                        ;   in Loop: Header=BB1_128 Depth=2
	STP	p1, [p4 + 8]
.LBB1_127:                              ; %cleanup
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPI	p6, p6, 1
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	CMP	r12, r4
	CPC	r13, r5
	LDP	p6, [p7 + 122]                  ; 4-byte Folded Reload
	BGE	.LBB1_170
.LBB1_128:                              ; %land.rhs182
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	MOV	r4, r6
	MOV	r5, r6
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_132
; %bb.129:                              ; %while.body187
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDW	r4, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r4, 115
	BNE	.LBB1_134
; %bb.130:                              ; %land.lhs.true193
                                        ;   in Loop: Header=BB1_128 Depth=2
	ADDPI	p2, p1, -9
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	CMP	r4, r10
	CPC	r5, r11
	BLO	.LBB1_132
; %bb.131:                              ; %land.lhs.true193
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p2, [p7 + 150]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB1_140
	BRA	.LBB1_132
.LBB1_134:                              ; %if.end197
                                        ;   in Loop: Header=BB1_128 Depth=2
	CMPI	r4, 91
	BNE	.LBB1_140
; %bb.135:                              ; %land.lhs.true201
                                        ;   in Loop: Header=BB1_128 Depth=2
	LEA	p2, [p7 + 166]
	ADDPP	p2, p2, p1
	LDP	p3, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r11
	LDBU	r12, [p2]
	MOV	r4, r11
	MOV	r5, r11
	CMPI	r12, 0
	STP	p3, [p7 + 18]                   ; 4-byte Folded Spill
	BNE	.LBB1_137
; %bb.136:                              ; %land.lhs.true201
                                        ;   in Loop: Header=BB1_128 Depth=2
	STP	p2, [p7 + 18]                   ; 4-byte Folded Spill
.LBB1_137:                              ; %land.lhs.true201
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDW	r12, [p7 + 10]                  ; 2-byte Folded Reload
	CMPI	r12, 0
	BEQ	.LBB1_139
; %bb.138:                              ; %land.lhs.true201
                                        ;   in Loop: Header=BB1_128 Depth=2
	MOVP	p3, p2
.LBB1_139:                              ; %land.lhs.true201
                                        ;   in Loop: Header=BB1_128 Depth=2
	STP	p2, [p7 + 14]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 18]                   ; 4-byte Folded Reload
	XOR	r13, r7, r5
	XOR	r12, r6, r4
	ANDI	r7, r13, 0
	ANDI	r6, r12, 1
	LDP	p2, [p7 + 14]                   ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	LDP	p6, [p7 + 122]                  ; 4-byte Folded Reload
	BNE	.LBB1_132
.LBB1_140:                              ; %if.end212
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	CMP	r12, r4
	CPC	r13, r5
	BEQ	.LBB1_142
; %bb.141:                              ; %if.then214
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p2, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPP	p2, p6, p2
	STB	r2, [p2]
	LDP	p2, [p4 + 8]
	BRA	.LBB1_143
.LBB1_142:                              ;   in Loop: Header=BB1_128 Depth=2
	MOVP	p2, p1
.LBB1_143:                              ; %if.end217
                                        ;   in Loop: Header=BB1_128 Depth=2
	MOV	r2, r6
	MOV	r3, r6
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_127
; %bb.144:                              ; %if.end.i671
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_125
; %bb.145:                              ; %if.then1.i675
                                        ;   in Loop: Header=BB1_128 Depth=2
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_126
; %bb.146:                              ; %if.then1.i675
                                        ;   in Loop: Header=BB1_128 Depth=2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_126
.LBB1_132:                              ; %while.end219
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r5
	MOV	r3, r5
	LDP	p2, [p7 + 126]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB1_171
.LBB1_133:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 66]                   ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	BRA	.LBB1_178
.LBB1_147:                              ;   in Loop: Header=BB1_4 Depth=1
	STP	p5, [p7 + 4]                    ; 4-byte Folded Spill
	BRA	.LBB1_152
.LBB1_148:                              ; %cond.false332
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r12, r3
	MOV	r13, r3
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 105
	BEQ	.LBB1_99
; %bb.149:                              ; %cond.false332
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 130]                  ; 4-byte Folded Reload
	BRA	.LBB1_98
.LBB1_150:                              ; %if.else.i723
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_151:                              ; %next.exit726
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 122]                  ; 4-byte Folded Spill
	STP	p1, [p4 + 8]
	LDP	p1, [p7 + 114]                  ; 4-byte Folded Reload
	ADDPI	p1, p1, -1
	STP	p1, [p7 + 114]                  ; 4-byte Folded Spill
.LBB1_152:                              ; %if.end357
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 22]                   ; 4-byte Folded Reload
	ORI	r3, r5, 0
	ORI	r2, r4, 16
	LDP	p2, [p7 + 58]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r11
	MOV	r12, r11
	MOV	r13, r11
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	STP	p2, [p7 + 58]                   ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB1_158
; %bb.153:                              ; %land.lhs.true363
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 30]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	STP	p1, [p7 + 30]                   ; 4-byte Folded Spill
	LDP	p2, [p7 + 122]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BNE	.LBB1_159
; %bb.154:                              ; %land.lhs.true363
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB1_159
; %bb.155:                              ; %if.end.i730
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_161
; %bb.156:                              ; %if.then1.i734
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_162
; %bb.157:                              ; %if.then1.i734
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_162
.LBB1_158:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	LDP	p1, [p7 + 22]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 14]                   ; 4-byte Folded Spill
	BRA	.LBB1_181
.LBB1_159:                              ; %if.end386
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 130]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	STP	p1, [p7 + 14]                   ; 4-byte Folded Spill
	MOV	r12, r5
	MOV	r13, r5
	LDP	p2, [p7 + 22]                   ; 4-byte Folded Reload
	CMP	r4, r12
	CPC	r5, r13
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
	BEQ	.LBB1_182
; %bb.160:                              ; %if.end386
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p7 + 14]                   ; 4-byte Folded Spill
	BRA	.LBB1_182
.LBB1_161:                              ; %if.else.i741
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_162:                              ; %next.exit744
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p4 + 8]
	MOVP	p6, p1
	LDP	p2, [p7 + 26]                   ; 4-byte Folded Reload
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	ANDI	r3, r13, -1
	STP	p6, [p7 + 122]                  ; 4-byte Folded Spill
	ANDI	r2, r12, -33
	STP	p2, [p7 + 26]                   ; 4-byte Folded Spill
	CMP	r2, r4
	CPC	r3, r5
	BNE	.LBB1_167
; %bb.163:                              ; %next.exit744
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BLO	.LBB1_167
; %bb.164:                              ; %if.end.i748
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r13
	MOV	r5, r13
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_179
; %bb.165:                              ; %if.then1.i752
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_180
; %bb.166:                              ; %if.then1.i752
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_180
.LBB1_167:                              ; %if.else380
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 82]                   ; 4-byte Folded Reload
	MOV	r6, r2
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r5
	MOV	r2, r5
	MOV	r3, r5
	LDP	p2, [p7 + 22]                   ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_169
; %bb.168:                              ; %if.else380
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOVP	p3, p2
.LBB1_169:                              ; %if.else380
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p3, [p7 + 14]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
	MOV	r12, r2
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r13, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	ADDPI	p2, p2, -1
	STP	p2, [p7 + 114]                  ; 4-byte Folded Spill
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	BRA	.LBB1_182
.LBB1_170:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 114]                  ; 4-byte Folded Reload
	STP	p1, [p7 + 126]                  ; 4-byte Folded Spill
.LBB1_171:                              ; %if.end222
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r2, [p7 + 22]                   ; 2-byte Folded Reload
	CMPI	r2, 99
	BEQ	.LBB1_174
; %bb.172:                              ; %if.end222
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r5
	MOV	r3, r5
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB1_174
; %bb.173:                              ; %if.then228
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPP	p1, p6, p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	STB	r5, [p1]
.LBB1_174:                              ; %if.end230
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDW	r4, [p7 + 8]                    ; 2-byte Folded Reload
	CMPI	r4, 42
	BNE	.LBB1_176
; %bb.175:                              ; %if.end230
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r5
	MOV	r3, r5
.LBB1_176:                              ; %if.end230
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r4 killed $r4 killed $p2 def $p2
	LDP	p3, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r7
	LDP	p3, [p7 + 118]                  ; 4-byte Folded Reload
	ADDPP	p3, p3, p1
	STP	p3, [p7 + 118]                  ; 4-byte Folded Spill
.LBB1_177:                              ; %cleanup314
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_178:                              ; %cleanup443
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 102]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	CMP	r4, r2
	CPC	r5, r3
	LDP	p6, [p7 + 110]                  ; 4-byte Folded Reload
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
	BEQ	.LBB1_212
	BRA	.LBB1_215
.LBB1_71:                               ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	BRA	.LBB1_110
.LBB1_179:                              ; %if.else.i759
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p4 + 4]
	JAL	fgetc
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_180:                              ; %next.exit762
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p1, [p7 + 122]                  ; 4-byte Folded Spill
	STP	p1, [p4 + 8]
	LDP	p1, [p7 + 58]                   ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	STP	p1, [p7 + 14]                   ; 4-byte Folded Spill
	MOV	r12, r5
	MOV	r13, r5
	LDP	p1, [p7 + 114]                  ; 4-byte Folded Reload
	ADDPI	p1, p1, -2
	STP	p1, [p7 + 114]                  ; 4-byte Folded Spill
.LBB1_181:                              ; %if.end386.thread
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p1, [p7 + 134]                  ; 4-byte Folded Reload
.LBB1_182:                              ; %if.end386.thread
                                        ;   in Loop: Header=BB1_4 Depth=1
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p2, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r5
	LDP	p2, [p7 + 114]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BLT	.LBB1_198
; %bb.183:                              ; %while.body394.lr.ph
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p6, [p7 + 22]                   ; 4-byte Folded Spill
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r3
	MOV	r5, r3
	STP	p2, [p7 + 126]                  ; 4-byte Folded Spill
	BRA	.LBB1_187
.LBB1_184:                              ; %if.else.i783
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p1, [p4 + 4]
	STP	p6, [p7 + 122]                  ; 4-byte Folded Spill
	JAL	fgetc
	LDP	p6, [p7 + 122]                  ; 4-byte Folded Reload
.LBB1_185:                              ; %if.end9.sink.split.i781
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p2, [p7 + 18]                   ; 4-byte Folded Reload
	STP	p1, [p7 + 122]                  ; 4-byte Folded Spill
	STP	p1, [p4 + 8]
.LBB1_186:                              ; %cleanup421
                                        ;   in Loop: Header=BB1_187 Depth=2
	ADDPP	p2, p6, p2
	LDP	p6, [p7 + 114]                  ; 4-byte Folded Reload
	ADDPI	p1, p6, -1
	LDP	p3, [p7 + 22]                   ; 4-byte Folded Reload
	ADDPI	p3, p3, 1
	STP	p3, [p7 + 22]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r11
	CMP	r12, r6
	CPC	r13, r7
	STP	p1, [p7 + 114]                  ; 4-byte Folded Spill
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	BLS	.LBB1_202
.LBB1_187:                              ; %while.body394
                                        ;   Parent Loop BB1_4 Depth=1
                                        ; =>  This Inner Loop Header: Depth=2
	LDP	p1, [p7 + 130]                  ; 4-byte Folded Reload
                                        ; kill: def $r2 killed $r2 killed $p1 def $p1
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r3, r13
	LDP	p6, [p7 + 122]                  ; 4-byte Folded Reload
	ADDPI	p6, p6, -48
	CMP	r12, r2
	CPC	r13, r3
	BLO	.LBB1_190
; %bb.188:                              ; %if.else401
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p6, [p7 + 86]                   ; 4-byte Folded Reload
	MOV	r13, r6
	LDP	p3, [p7 + 122]                  ; 4-byte Folded Reload
	ORI	r3, r7, 0
	ORI	r2, r6, 32
	ADDPI	p3, p1, -123
	STP	p6, [p7 + 86]                   ; 4-byte Folded Spill
	CMP	r6, r12
	CPC	r7, r13
	BLO	.LBB1_200
; %bb.189:                              ; %if.then404
                                        ;   in Loop: Header=BB1_187 Depth=2
	ADDPI	p6, p1, -87
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_190:                              ; %if.end410
                                        ;   in Loop: Header=BB1_187 Depth=2
	MOVP	p1, p3
	LDP	p3, [p7 + 14]                   ; 4-byte Folded Reload
	CMP	r12, r6
	CPC	r13, r7
	BHS	.LBB1_199
; %bb.191:                              ; %if.end414
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r11
	MOV	r3, r11
	STP	p1, [p7 + 18]                   ; 4-byte Folded Spill
	STP	p1, [p7]
	LDP	p1, [p7 + 126]                  ; 4-byte Folded Reload
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
	JAL	__muldi3
	MOVP	p6, p2
	LDP	p2, [p7 + 134]                  ; 4-byte Folded Reload
	MOV	r6, r4
	MOV	r7, r11
	LDP	p2, [p7 + 126]                  ; 4-byte Folded Reload
	ADDPP	p2, p1, p2
	STP	p2, [p7 + 126]                  ; 4-byte Folded Spill
	CMP	r4, r2
	CPC	r5, r3
	BLO	.LBB1_193
; %bb.192:                              ; %if.end414
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p3, [p7 + 18]                   ; 4-byte Folded Reload
.LBB1_193:                              ; %if.end414
                                        ;   in Loop: Header=BB1_187 Depth=2
	STP	p3, [p7 + 18]                   ; 4-byte Folded Spill
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	MOV	r2, r6
	MOV	r3, r6
	LDP	p2, [p7 + 122]                  ; 4-byte Folded Reload
	CMP	r4, r2
	CPC	r5, r3
	BEQ	.LBB1_197
; %bb.194:                              ; %if.end.i772
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p1, [p4 + 12]
	ADDPI	p1, p1, 1
	STP	p1, [p4 + 12]
	LDP	p1, [p4]
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r4, r11
	MOV	r5, r11
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB1_184
; %bb.195:                              ; %if.then1.i776
                                        ;   in Loop: Header=BB1_187 Depth=2
	ADDPI	p2, p1, 1
	STP	p2, [p4]
	LDBU	r4, [p1 + 1]
	MOV	r2, r6
	MOV	r3, r6
	CMPI	r4, 0
	BEQ	.LBB1_185
; %bb.196:                              ; %if.then1.i776
                                        ;   in Loop: Header=BB1_187 Depth=2
	LDP	p1, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r5, r3
	MOVP	p1, p2
	BRA	.LBB1_185
.LBB1_197:                              ;   in Loop: Header=BB1_187 Depth=2
	STP	p1, [p7 + 122]                  ; 4-byte Folded Spill
	LDP	p2, [p7 + 18]                   ; 4-byte Folded Reload
	BRA	.LBB1_186
.LBB1_198:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 126]                  ; 4-byte Folded Reload
	BRA	.LBB1_201
.LBB1_199:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 22]                   ; 4-byte Folded Reload
	MOVP	p3, p1
	BRA	.LBB1_201
.LBB1_200:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
	LDP	p6, [p7 + 22]                   ; 4-byte Folded Reload
.LBB1_201:                              ; %while.end425
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p5, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r2, r11
	MOV	r3, r11
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB1_214
.LBB1_202:                              ; %if.end428
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDW	r2, [p7 + 8]                    ; 2-byte Folded Reload
	CMPI	r2, 42
	BNE	.LBB1_204
.LBB1_203:                              ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 110]                  ; 4-byte Folded Reload
	BRA	.LBB1_211
.LBB1_204:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p3, [p7 + 134]                  ; 4-byte Folded Reload
                                        ; kill: def $r6 killed $r6 killed $p3 def $p3
	LDP	p6, [p7 + 158]                  ; 4-byte Folded Reload
	MOV	r7, r13
	MOV	r2, r13
	MOV	r3, r13
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	CMP	r12, r2
	CPC	r13, r3
	BNE	.LBB1_206
; %bb.205:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	MOVP	p3, p1
.LBB1_206:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB1_208
; %bb.207:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 126]                  ; 4-byte Folded Reload
	SUB	r12, r2, r12
	SBC	r13, r3, r13
	STP	p6, [p7 + 126]                  ; 4-byte Folded Spill
.LBB1_208:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	CMP	r12, r2
	CPC	r13, r3
	BEQ	.LBB1_210
; %bb.209:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	ADDPP	p2, p2, p3
	SUB	r4, r2, r4
	SBC	r5, r3, r5
.LBB1_210:                              ; %if.then430
                                        ;   in Loop: Header=BB1_4 Depth=1
	STP	p2, [p7]
	LDP	p1, [p7 + 98]                   ; 4-byte Folded Reload
	LDP	p2, [p7 + 10]                   ; 4-byte Folded Reload
	LDP	p3, [p7 + 126]                  ; 4-byte Folded Reload
	JAL	store_int
	LDP	p1, [p7 + 118]                  ; 4-byte Folded Reload
	ADDPI	p1, p1, 1
	STP	p1, [p7 + 118]                  ; 4-byte Folded Spill
	LDP	p6, [p7 + 110]                  ; 4-byte Folded Reload
	LDP	p3, [p7 + 142]                  ; 4-byte Folded Reload
.LBB1_211:                              ; %while.cond.outer.backedge
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDP	p2, [p7 + 146]                  ; 4-byte Folded Reload
.LBB1_212:                              ; %while.cond.outer.backedge
                                        ;   in Loop: Header=BB1_4 Depth=1
	LDBU	r2, [p2]
	CMPI	r2, 0
	BNE	.LBB1_4
	BRA	.LBB1_215
.LBB1_213:
	STP	p5, [p7 + 118]                  ; 4-byte Folded Spill
	BRA	.LBB1_215
.LBB1_216:                              ; %cleanup443.thread
	MOVI	r4, 0
	MOV	r5, r4
	MOVI	r2, -1
	MOV	r3, r2
	LDP	p3, [p7 + 118]                  ; 4-byte Folded Reload
	CMP	r6, r4
	CPC	r7, r5
	MOVP	p2, p3
	BEQ	.LBB1_218
; %bb.217:                              ; %cleanup443.thread
	MOVP	p1, p2
.LBB1_218:                              ; %cleanup443.thread
	STP	p1, [p7 + 118]                  ; 4-byte Folded Spill
.LBB1_214:
	LDP	p6, [p7 + 110]                  ; 4-byte Folded Reload
.LBB1_215:                              ; %cleanup450
	LDP	p1, [p7 + 118]                  ; 4-byte Folded Reload
	LDP	p4, [p7 + 426]                  ; 4-byte Folded Reload
	LDP	p5, [p7 + 422]                  ; 4-byte Folded Reload
	ADDPI	p7, p7, 430
	RET
	.section	.rodata,"a",@progbits
	.p2align	1, 0x0
.LJTI1_0:
	.long	.LBB1_61
	.long	.LBB1_148
	.long	.LBB1_61
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_97
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_78
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_78
	.long	.LBB1_148
	.long	.LBB1_61
	.long	.LBB1_61
	.long	.LBB1_61
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_99
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_78
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_148
	.long	.LBB1_97
                                        ; -- End function
	.text
	.globl	sscanf                          ; -- Begin function sscanf
	.p2align	1
sscanf:                                 ; @sscanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -32
	STP	p4, [p7 + 28]                   ; 4-byte Folded Spill
	MOVP	p4, p6
	LEA	p1, [p7 + 40]
	STP	p1, [p7]
	STP	p1, [p7 + 24]
	MOVI	r4, 0
	MOV	r5, r4
	STP	p2, [p7 + 12]
	LDP	p3, [p7 + 32]
	STP	p3, [p7 + 8]
	LDBU	r6, [p3]
	MOVI	r12, -1
	MOV	r13, r12
	CMPI	r6, 0
	BEQ	.LBB2_2
; %bb.1:                                ; %entry
	MOV	r7, r4
	MOVP	p6, p3
.LBB2_2:                                ; %entry
	STP	p2, [p7 + 20]
	STP	p6, [p7 + 16]
	STP	p1, [p7 + 4]
	LEA	p1, [p7 + 8]
	LDP	p2, [p7 + 36]
	LEA	p3, [p7 + 4]
	JAL	scan
	MOVP	p6, p4
	LDP	p4, [p7 + 28]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 32
	RET
                                        ; -- End function
	.globl	vfscanf                         ; -- Begin function vfscanf
	.p2align	1
vfscanf:                                ; @vfscanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -40
	STP	p4, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p2, [p7]                        ; 4-byte Folded Spill
	MOVP	p5, p1
	STP	p6, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p3, [p7 + 28]
	STP	p5, [p7 + 16]
	MOVI	r8, 0
	MOV	r9, r8
	STP	p4, [p7 + 12]
	JAL	fgetc
	STP	p4, [p7 + 24]
	STP	p1, [p7 + 20]
	LDP	p1, [p7 + 28]
	STP	p1, [p7 + 8]
	LEA	p1, [p7 + 12]
	LEA	p3, [p7 + 8]
	LDP	p2, [p7]                        ; 4-byte Folded Reload
	JAL	scan
	MOVI	r6, -1
	MOV	r7, r6
	LDP	p2, [p7 + 20]
	CMP	r4, r6
	CPC	r5, r7
	BEQ	.LBB3_2
; %bb.1:                                ; %if.then
	MOVP	p4, p1
	MOVP	p1, p2
	MOVP	p2, p5
	JAL	ungetc
	MOVP	p1, p4
.LBB3_2:                                ; %if.end
	LDP	p6, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p4, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 32]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 40
	RET
                                        ; -- End function
	.globl	fscanf                          ; -- Begin function fscanf
	.p2align	1
fscanf:                                 ; @fscanf
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -40
	STP	p4, [p7 + 36]                   ; 4-byte Folded Spill
	STP	p5, [p7 + 32]                   ; 4-byte Folded Spill
	STP	p6, [p7]                        ; 4-byte Folded Spill
	LEA	p1, [p7 + 48]
	STP	p1, [p7 + 4]
	STP	p1, [p7 + 28]
	LDP	p5, [p7 + 40]
	STP	p5, [p7 + 16]
	MOVI	r8, 0
	MOV	r9, r8
	STP	p4, [p7 + 12]
	MOVP	p1, p5
	JAL	fgetc
	STP	p4, [p7 + 24]
	STP	p1, [p7 + 20]
	LDP	p1, [p7 + 28]
	STP	p1, [p7 + 8]
	LEA	p1, [p7 + 12]
	LDP	p2, [p7 + 44]
	LEA	p3, [p7 + 8]
	JAL	scan
	MOVP	p4, p1
	MOVI	r4, -1
	MOV	r5, r4
	LDP	p1, [p7 + 20]
	CMP	r2, r4
	CPC	r3, r5
	BEQ	.LBB4_2
; %bb.1:                                ; %if.then.i
	MOVP	p2, p5
	JAL	ungetc
.LBB4_2:                                ; %vfscanf.exit
	LDP	p6, [p7]                        ; 4-byte Folded Reload
	MOVP	p1, p4
	LDP	p4, [p7 + 36]                   ; 4-byte Folded Reload
	LDP	p5, [p7 + 32]                   ; 4-byte Folded Reload
	ADDPI	p7, p7, 40
	RET
                                        ; -- End function
	.p2align	1                               ; -- Begin function store_int
store_int:                              ; @store_int
; %bb.0:                                ; %entry
	ADDPI	p7, p7, -8
	STP	p4, [p7 + 4]                    ; 4-byte Folded Spill
	STP	p5, [p7]                        ; 4-byte Folded Spill
	MOVI	r9, 0
	MOVI	r8, 36
	ADDPI	p2, p2, -72
	CMP	r4, r8
	CPC	r5, r9
	BHI	.LBB5_5
; %bb.1:                                ; %entry
	SHLI	r8, r5, 2
	SHRI	r9, r4, 14
	OR	r9, r8, r9
	SHLI	r8, r4, 2
	LEA	p2, [.LJTI5_0]
	ADDPP	p2, p4, p2
	LDP	p2, [p2]
	JR	p2
.LBB5_5:                                ; %sw.default
	LDP	p2, [p1]
	ADDPI	p4, p2, 4
	STP	p4, [p1]
	LDP	p1, [p2]
	BRA	.LBB5_6
.LBB5_4:                                ; %sw.bb7
	LDP	p2, [p7 + 8]
	LDP	p4, [p1]
	ADDPI	p5, p4, 4
	STP	p5, [p1]
	LDP	p1, [p4]
	STP	p2, [p1 + 4]
.LBB5_6:                                ; %sw.epilog
	STP	p3, [p1]
	BRA	.LBB5_7
.LBB5_2:                                ; %sw.bb
	LDP	p2, [p1]
	ADDPI	p4, p2, 4
	STP	p4, [p1]
	LDP	p1, [p2]
	STB	r6, [p1]
	BRA	.LBB5_7
.LBB5_3:                                ; %sw.bb1
	LDP	p2, [p1]
	ADDPI	p4, p2, 4
	STP	p4, [p1]
	LDP	p1, [p2]
	STW	r6, [p1]
.LBB5_7:                                ; %sw.epilog
	LDP	p4, [p7 + 4]                    ; 4-byte Folded Reload
	LDP	p5, [p7]                        ; 4-byte Folded Reload
	ADDPI	p7, p7, 8
	RET
	.section	.rodata,"a",@progbits
	.p2align	1, 0x0
.LJTI5_0:
	.long	.LBB5_2
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_4
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_3
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
	.long	.LBB5_5
                                        ; -- End function
	.section	".note.GNU-stack","",@progbits
	.addrsig
