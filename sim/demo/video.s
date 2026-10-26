; Video demo: every layer of docs/io.md at once.
;   text (transparent, opaque and stippled cells), an 8-color bitmap,
;   the backdrop, and a sprite that moves using the double buffer.
; Runs 90 frames, then exits, so it also works headless:
;   fsim sim/demo/video.bin --screenshot shot.ppm

        .org    0x1000

IO      = 0xF00000
FRAME   = 0x0A                  ; offsets from IO
VID     = 0x200                 ; video registers
TEXT    = 0x0FC000
BMP     = 0x0C0000
SPR     = 0x080000
SPRBUF  = 115200
SPR_Y   = 200

start:  LA      sp, 0x080000
        LA      p5, IO

        ; video registers: layout from docs/io.md
        LA      p1, TEXT
        STP     p1, [p5 + VID + 4]
        LA      p1, BMP
        STP     p1, [p5 + VID + 8]
        LA      p1, SPR
        STP     p1, [p5 + VID + 12]
        MOVI    r2, 1                   ; blue backdrop
        STW     r2, [p5 + VID + 2]
        LI      r2, 7 * 80 + 2          ; cursor at row 7, column 2
        STW     r2, [p5 + VID + 0x18]

        ; clear the text buffer: transparent background, white space
        LA      p1, TEXT
        LI      r4, 0x4720
        MOV     r5, r4
        LI      r6, 1000
        CALL    fill32

        ; title, yellow on black
        LA      p1, title
        LA      p2, TEXT + (1 * 80 + 2) * 2
        LI      r6, 0x0600
        CALL    text

        ; row 3: the 8 colors as opaque backgrounds, white "#"
        LA      p2, TEXT + (3 * 80 + 2) * 2
        MOVI    r6, 0
colors: SHLI    r7, r6, 11
        ORI     r7, r7, 0x0723
        STW     r7, [p2]+
        STW     r7, [p2]+
        ADDI    r6, r6, 1
        CMPI    r6, 8
        BNE     colors

        ; row 5: stippled red background behind cyan text
        LA      p1, stip
        LA      p2, TEXT + (5 * 80 + 2) * 2
        LI      r6, 0xA300
        CALL    text

        ; bitmap: top half 8 color bars, bottom half transparent (backdrop)
        LA      p1, BMP
        MOVI    r8, 180                 ; rows
bars:   MOVI    r6, 0                   ; color
bar:    MOVI    r4, 0x11
        MUL     r4, r4, r6
        MOVI    r7, 40
barb:   STB     r4, [p1]+
        DEC     r7
        BNE     barb
        ADDI    r6, r6, 1
        CMPI    r6, 8
        BNE     bar
        DEC     r8
        BNE     bars
        LI      r4, 0x8888
        MOV     r5, r4
        LI      r6, 180 * 320 / 4
        CALL    fill32

        ; both sprite buffers fully transparent
        LA      p1, SPR
        LI      r4, 0x8888
        MOV     r5, r4
        LI      r6, SPRBUF / 2
        CALL    fill32

        MOVI    r2, 0x2B                ; text, bitmap, sprites, cursor; buffer 0 shown
        STW     r2, [p5 + VID]

        ; animation: draw into the hidden buffer, flip at the next frame
        MOVI    r9, 0                   ; frame number
        LDW     r8, [p5 + FRAME]        ; last frame counter value
frame:  LDW     r12, [p5 + VID]
        ANDI    r12, r12, 0x10          ; which buffer is shown?
        LA      p1, SPR + SPR_Y * 320
        BNE     hid1                    ; buffer 1 shown: draw into 0
        LA      p1, SPR + SPRBUF + SPR_Y * 320
hid1:   PUSHP   p1
        LI      r4, 0x8888              ; clear the sprite's 16-row band
        MOV     r5, r4
        LI      r6, 16 * 320 / 4
        CALL    fill32
        POPP    p1
        ; sprite at x = 100 + 4*frame: 16 rows of 8 bytes (16 pixels)
        SHLI    r12, r9, 1
        ADDI    r12, r12, 50            ; byte column
        ADDP    p1, p1, r12
        LA      p2, sprite
        MOVI    r7, 16
srow:   MOVI    r6, 8
sbyte:  LDBU    r12, [p2]+              ; lr is free: no calls in this loop
        STB     r12, [p1]+
        DEC     r6
        BNE     sbyte
        ADDPI   p1, p1, 320 - 8
        DEC     r7
        BNE     srow
        ; wait for the next frame, then show the buffer just drawn
wait:   LDW     r2, [p5 + FRAME]
        CMP     r2, r8
        BEQ     wait
        MOV     r8, r2
        LDW     r2, [p5 + VID]
        XORI    r2, r2, 0x10            ; show the buffer just drawn
        STW     r2, [p5 + VID]
        ADDI    r9, r9, 1
        CMPI    r9, 90
        BNE     frame

        MOVI    r2, 0
        STW     r2, [p5 + 2]            ; exit 0

; fill32: store the pair p2 (r5:r4) r6 times from p1 upward.
fill32: STP     p2, [p1]+
        DEC     r6
        BNE     fill32
        RET

; text: copy the string at p1 to cells at p2, attribute r6 (high byte). Clobbers r7.
text:   LDBU    r7, [p1]+
        TST     r7
        BEQ     text1
        OR      r7, r7, r6
        STW     r7, [p2]+
        BRA     text
text1:  RET

title:  .asciz  "fpga_cpu video demo: text over bitmap over backdrop, sprite on top"
stip:   .asciz  " stippled background "
        .align  2
; 16x16 sprite, 4 bpp: a red ring with a yellow center, transparent corners
sprite: .byte   0x88,0x88,0x84,0x44,0x44,0x48,0x88,0x88
        .byte   0x88,0x84,0x44,0x44,0x44,0x44,0x48,0x88
        .byte   0x88,0x44,0x44,0x44,0x44,0x44,0x44,0x88
        .byte   0x84,0x44,0x44,0x66,0x66,0x44,0x44,0x48
        .byte   0x84,0x44,0x46,0x66,0x66,0x64,0x44,0x48
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x44,0x44,0x66,0x66,0x66,0x66,0x44,0x44
        .byte   0x84,0x44,0x46,0x66,0x66,0x64,0x44,0x48
        .byte   0x84,0x44,0x44,0x66,0x66,0x44,0x44,0x48
        .byte   0x88,0x44,0x44,0x44,0x44,0x44,0x44,0x88
        .byte   0x88,0x84,0x44,0x44,0x44,0x44,0x48,0x88
        .byte   0x88,0x88,0x84,0x44,0x44,0x48,0x88,0x88
