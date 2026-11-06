#!/usr/bin/env python3
"""Writes three original PSID v2 tunes (tunes/*.sid): a small tracker-style 6502 music driver
assembled with apps/sid6502/tools/m6502.py, plus per-tune instrument and pattern data.
usage: mktunes.py OUTDIR

Driver (all tunes): 16-row patterns of (note, inst|fx<<5) byte pairs, one order list per voice
(shared position), `speed` frames per row.  Notes 1..96 = C-0.., 0 = hold, $FE = gate off.
Instrument = 8 bytes: ctrl (wave+ring+sync, no gate), AD, SR, PW hi, PW sweep speed (signed, x4
of the 16-bit PW per frame, bounces), auto slide (signed, << octave, per frame), 2 spare.
Effects: 1 slide up, 2 slide down, 3 vibrato, 4/5/6/7 arpeggio major / minor / octave / sus4.
Each new note toggles the gate (0 then 1 inside one play call = a retrigger).
A tune's filter: routing byte ($D417), mode+volume ($D418), cutoff start/step/limits per song."""
import os, re, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "sid6502", "tools"))
import m6502

NOTES = {"C-": 0, "C#": 1, "D-": 2, "D#": 3, "E-": 4, "F-": 5, "F#": 6, "G-": 7, "G#": 8, "A-": 9, "A#": 10, "B-": 11}
def note_num(s):
    return NOTES[s[:2]] + 12 * int(s[2])
def fn(n):                                   # SID frequency register for note n (PAL)
    hz = 440.0 * 2 ** ((n - 57) / 12.0)
    return min(65535, round(hz * 16777216 / 985248))

DRIVER = r"""
ptr = $FB
tl = $FD
th = $FE
sl = $F9
sh = $FA
tick = $2000
row = $2001
order = $2002
speed = $2003
rowofs = $2004
cutv = $2005
cutd = $2006
arpc = $2007
note = $2010
inst = $2013
fx = $2016
flo = $2019
fhi = $201C
pwlo = $201F
pwhi = $2022
pwsp = $2025
ctl = $2028
ad = $202B
sr = $202E
vph = $2031
oct = $2034
aslide = $2037
        .org $1000
        jmp init_
        jmp play_
init_:  pha
        lda #0
        tax
ic:     sta $2000,x
        inx
        cpx #$40
        bne ic
        tax
ic2:    sta $D400,x
        inx
        cpx #$19
        bne ic2
        pla
        tax
        lda speedtab,x
        sta speed
        lda startord,x
        sta order
        lda cutstep,x
        sta cutd
        lda cutinit,x
        sta cutv
        lda routing,x
        sta $D417
        lda modevol,x
        sta $D418
        rts
play_:  lda tick
        beq newrow
nr:     ldx #2
fl:     jsr voiceupd
        dex
        bpl fl
        jsr filt
        inc arpc
        lda arpc
        cmp #3
        bcc arpok
        lda #0
        sta arpc
arpok:  inc tick
        lda tick
        cmp speed
        bcc pdone
        lda #0
        sta tick
pdone:  rts
newrow: ldx #2
rl:     jsr dorow
        dex
        bpl rl
        lda rowofs
        clc
        adc #2
        sta rowofs
        cmp #32
        bcc nr
        lda #0
        sta rowofs
        inc order
        lda order
        cmp #ORDLEN
        bcc nr
        lda loopord
        sta order
        jmp nr
dorow:  lda ordoff,x
        clc
        adc order
        tay
        lda ordtab,y
        tay
        lda patlo,y
        clc
        adc rowofs
        sta ptr
        lda pathi,y
        adc #0
        sta ptr+1
        ldy #0
        lda (ptr),y
        beq nonote
        cmp #$FE
        beq noteoff
        sec
        sbc #1
        sta note,x
        ldy #1
        lda (ptr),y
        pha
        and #$1F
        beq keepi
        sta inst,x
keepi:  pla
        lsr
        lsr
        lsr
        lsr
        lsr
        sta fx,x
        jmp trigger
noteoff: lda ctl,x
        and #$FE
        sta ctl,x
        rts
nonote: ldy #1
        lda (ptr),y
        beq dret
        lsr
        lsr
        lsr
        lsr
        lsr
        sta fx,x
dret:   rts
trigger: ldy note,x
        lda freqlo,y
        sta flo,x
        lda freqhi,y
        sta fhi,x
        lda octtab,y
        sta oct,x
        lda inst,x
        asl
        asl
        asl
        tay
        lda instab,y
        ora #1
        sta ctl,x
        lda instab+1,y
        sta ad,x
        lda instab+2,y
        sta sr,x
        lda instab+3,y
        sta pwhi,x
        lda #0
        sta pwlo,x
        sta vph,x
        lda instab+4,y
        sta pwsp,x
        lda instab+5,y
        sta aslide,x
        ldy voff,x
        lda #0
        sta $D404,y
        rts
shifted: sta sl
        lda #0
        sta sh
        lda sl
        bpl sh1
        lda #$FF
        sta sh
sh1:    ldy oct,x
sh2:    beq shd
        asl sl
        rol sh
        dey
        jmp sh2
shd:    rts
addbase: jsr shifted
        clc
        lda flo,x
        adc sl
        sta flo,x
        lda fhi,x
        adc sh
        sta fhi,x
        bpl abok
        lda #0
        sta flo,x
        sta fhi,x
abok:   rts
addtemp: jsr shifted
        clc
        lda tl
        adc sl
        sta tl
        lda th
        adc sh
        sta th
        rts
voiceupd: lda aslide,x
        beq noaut
        jsr addbase
noaut:  lda flo,x
        sta tl
        lda fhi,x
        sta th
        lda fx,x
        beq fxdone
        cmp #1
        bne f2
        lda #6
        jsr addbase
        jmp fxold
f2:     cmp #2
        bne f3
        lda #$FA
        jsr addbase
        jmp fxold
f3:     cmp #3
        bne f4
        lda vph,x
        clc
        adc #3
        and #$1F
        sta vph,x
        tay
        lda vibtab,y
        jsr addtemp
        jmp fxdone
f4:     sec
        sbc #4
        sta sl
        asl
        clc
        adc sl
        clc
        adc arpc
        tay
        lda arptab,y
        clc
        adc note,x
        tay
        lda freqlo,y
        sta tl
        lda freqhi,y
        sta th
        jmp fxdone
fxold:  lda flo,x
        sta tl
        lda fhi,x
        sta th
fxdone: ldy voff,x
        lda tl
        sta $D400,y
        lda th
        sta $D401,y
        lda pwsp,x
        beq pwwrite
        sta sl
        lda #0
        sta sh
        lda sl
        bpl pw1
        lda #$FF
        sta sh
pw1:    asl sl
        rol sh
        asl sl
        rol sh
        clc
        lda pwlo,x
        adc sl
        sta pwlo,x
        lda pwhi,x
        adc sh
        sta pwhi,x
        cmp #$18
        bcc bounce
        cmp #$E8
        bcc pwwrite
bounce: lda #0
        sec
        sbc pwsp,x
        sta pwsp,x
pwwrite: ldy voff,x
        lda pwhi,x
        lsr
        lsr
        lsr
        lsr
        sta $D403,y
        lda pwlo,x
        lsr
        lsr
        lsr
        lsr
        sta sh
        lda pwhi,x
        asl
        asl
        asl
        asl
        ora sh
        sta $D402,y
        lda ad,x
        sta $D405,y
        lda sr,x
        sta $D406,y
        lda ctl,x
        sta $D404,y
        rts
filt:   lda cutd
        beq fwr
        lda cutv
        clc
        adc cutd
        sta cutv
        cmp #CUTHI
        bcs fb
        cmp #CUTLO
        bcs fwr
fb:     lda #0
        sec
        sbc cutd
        sta cutd
fwr:    lda cutv
        sta $D416
        rts
"""

def parse_pat(s):
    toks = s.split()
    assert len(toks) == 16, (len(toks), s)
    out = []
    for t in toks:
        if t == ".":
            out += [0, 0]
        elif t == "---":
            out += [0xFE, 0]
        else:
            p = t.split("/")
            n = note_num(p[0]) + 1
            ins = int(p[1]) if len(p) > 1 else 0
            f = int(p[2]) if len(p) > 2 else 0
            assert n <= 96
            out += [n, ins | f << 5]
    return out

def build(name, author, released, songs, instruments, patterns, orders, filt, loopord=0, cuthilo=(0xE0, 0x10)):
    """patterns: dict name -> 16-token string; orders: 3 lists of pattern names; songs: list of dict(speed,start,route,mode,cutinit,cutstep)"""
    pnames = list(patterns)
    N = len(orders[0])
    assert all(len(o) == N for o in orders)
    L = []
    L.append("ORDLEN = %d" % N)
    L.append("CUTHI = %d" % cuthilo[0])
    L.append("CUTLO = %d" % cuthilo[1])
    L.append(DRIVER)
    def tab(label, vals):
        L.append(label + ": .byte " + ",".join(str(v & 0xFF) for v in vals))
    tab("freqlo", [fn(n) & 255 for n in range(96)])
    tab("freqhi", [fn(n) >> 8 for n in range(96)])
    tab("octtab", [n // 12 for n in range(96)])
    tab("voff", [0, 7, 14])
    tab("ordoff", [0, N, 2 * N])
    tab("vibtab", [round(6 * __import__("math").sin(2 * 3.14159265 * i / 32)) for i in range(32)])
    tab("arptab", [0, 4, 7, 0, 3, 7, 0, 12, 0, 0, 5, 7])
    tab("speedtab", [s["speed"] for s in songs])
    tab("startord", [s["start"] for s in songs])
    tab("routing", [s["route"] for s in songs])
    tab("modevol", [s["mode"] for s in songs])
    tab("cutinit", [s["cutinit"] for s in songs])
    tab("cutstep", [s["cutstep"] for s in songs])
    tab("loopord", [loopord])
    ords = []
    for o in orders:
        ords += [pnames.index(p) for p in o]
    tab("ordtab", ords)
    inst_bytes = [0] * 8
    for ins in instruments:
        b = list(ins) + [0] * (8 - len(ins))
        inst_bytes += b
    tab("instab", inst_bytes)
    L.append("patlo: .byte " + ",".join("<pat_%d" % i for i in range(len(pnames))))
    L.append("pathi: .byte " + ",".join(">pat_%d" % i for i in range(len(pnames))))
    for i, p in enumerate(pnames):
        L.append("pat_%d: .byte " % i + ",".join(str(v) for v in parse_pat(patterns[p])))
    out, syms = m6502.assemble("\n".join(L), 0x1000)
    lo = min(out)
    hi = max(out)
    data = bytes(out.get(a, 0) for a in range(lo, hi + 1))
    assert lo == 0x1000
    return data, syms

def psid(path, data, syms, title, author, released, nsongs, speedbits=0, flags=0x04 | 0x10):
    hdr = b"PSID" + struct.pack(">HHHHHHHI", 2, 0x7C, 0, syms["init_"] - 0 if False else 0x1000, 0x1003, nsongs, 1, speedbits)
    def s32(x): return x.encode("latin-1")[:31].ljust(32, b"\0")
    hdr += s32(title) + s32(author) + s32(released) + struct.pack(">HBBBB", flags, 0, 0, 0, 0)
    assert len(hdr) == 0x7C, len(hdr)
    with open(path, "wb") as f:
        f.write(hdr + struct.pack("<H", 0x1000) + data)
    print(path, len(data) + 2, "bytes of music data,", nsongs, "song(s)")

# ----------------------------------------------------------------------------- tune 1: Neon Drift
def tune1(out):
    P = {}
    bass = lambda a, b, c, d: "%s/1 . . %s/1 . . %s/1 . %s/1 . . %s/1 . . %s/1 ." % (a, a, c, a, a, d)
    P["BA"] = bass("A-2", 0, "A-3", "E-3")
    P["BF"] = bass("F-2", 0, "F-3", "C-3")
    P["BC"] = bass("C-2", 0, "C-3", "G-2")
    P["BG"] = bass("G-2", 0, "G-3", "D-3")
    P["AA"] = "A-3/3/5 . . . . . . . A-3/3/5 . . . . . . ."
    P["AF"] = "F-3/3/4 . . . . . . . F-3/3/4 . . . . . . ."
    P["L0"] = "E-5/2 . . . . . C-5/2 . D-5/2 . . . E-5/2 . . ."
    P["L1"] = "F-5/2 . . . . . C-5/2 . A-4/2/3 . . . . . . ."
    P["L2"] = "G-5/2 . . . E-5/2 . . . C-5/2 . D-5/2 . E-5/2 . . ."
    P["L3"] = "D-5/2 . . . . . B-4/2 . D-5/2/3 . . . . . --- ."
    P["L4"] = "A-5/2 . G-5/2 . E-5/2 . . . A-5/2 . C-6/2 . B-5/2 . A-5/2 ."
    P["L5"] = "A-5/2 . F-5/2 . C-5/2 . F-5/2 . A-5/2/3 . . . . . . ."
    P["D0"] = "C-2/4 . F-6/5 . D-4/6 . F-6/5 . C-2/4 . F-6/5 . D-4/6 . F-6/5 ."
    P["D1"] = "C-2/4 . F-6/5 C-2/4 D-4/6 . F-6/5 . C-2/4 . F-6/5 C-2/4 D-4/6 D-4/6 D-4/6 D-4/6"
    P["RS"] = ". . . . . . . . . . . . . . . ."
    orders = [["AA", "AF", "L2", "L3", "L4", "L1", "L2", "L5"],
              ["BA", "BF", "BC", "BG", "BA", "BF", "BC", "BG"],
              ["D0", "D0", "D0", "D1", "D0", "D0", "D0", "D1"]]
    inst = [
        (0x20, 0x08, 0x84, 0, 0, 0),            # 1 saw bass
        (0x40, 0x29, 0x9A, 0x60, 0x08, 0),      # 2 pulse lead, PW sweep
        (0x40, 0x03, 0x30, 0x40, 0, 0),         # 3 pulse arp pluck
        (0x80, 0x05, 0x00, 0, 0, 0xFA),         # 4 kick: noise with falling pitch
        (0x80, 0x01, 0x00, 0, 0, 0),            # 5 hat
        (0x80, 0x07, 0x00, 0, 0, 0xFE),         # 6 snare
    ]
    songs = [dict(speed=6, start=0, route=0xA3, mode=0x1F, cutinit=0x30, cutstep=5),
             dict(speed=4, start=2, route=0xA1, mode=0x2F, cutinit=0x60, cutstep=9)]
    data, syms = build("Neon Drift", "", "", songs, inst, P, orders, None, loopord=0)
    psid(os.path.join(out, "neondrift.sid"), data, syms, "Neon Drift", "fpga_cpu demo", "2026 original", 2)

# ----------------------------------------------------------------------------- tune 2: Glass Bells (ring modulation)
def tune2(out):
    P = {}
    P["D0"] = "D-2/1 . . . . . . . . . . . . . . ."
    P["D1"] = "A#1/1 . . . . . . . . . . . . . . ."
    P["D2"] = "F-2/1 . . . . . . . . . . . . . . ."
    P["D3"] = "C-2/1 . . . . . . . . . . . . . . ."
    # bells: triangle + ring (voice 2 modulated by voice 1)
    P["B0"] = "D-4/2 . . . F-4/2 . . . A-4/2 . . . D-5/2 . . ."
    P["B1"] = "A#3/2 . . . D-4/2 . . . F-4/2 . . . A#4/2 . . ."
    P["B2"] = "F-4/2 . . . A-4/2 . . . C-5/2 . . . F-5/2 . . ."
    P["B3"] = "C-4/2 . . . E-4/2 . . . G-4/2 . . . C-5/2 . E-5/2 ."
    P["B4"] = "A-4/2 . . . . . D-5/2 . . . F-5/2 . . . . ."
    # pad: pulse with slow attack and vibrato, filtered
    P["P0"] = "A-3/3/3 . . . . . . . . . . . D-4/3 . . ."
    P["P1"] = "F-3/3/3 . . . . . . . . . . . A#3/3 . . ."
    P["P2"] = "C-4/3/3 . . . . . . . . . . . F-4/3 . . ."
    P["P3"] = "G-3/3/3 . . . . . . . . . . . C-4/3 . . ."
    orders = [["D0", "D1", "D2", "D3", "D0", "D1", "D2", "D3"],
              ["B0", "B1", "B2", "B3", "B0", "B1", "B2", "B4"],
              ["P0", "P1", "P2", "P3", "P0", "P1", "P2", "P3"]]
    inst = [
        (0x10, 0x0A, 0xF8, 0, 0, 0),             # 1 triangle drone (ring source)
        (0x15, 0x09, 0x00, 0, 0, 0),             # 2 triangle + ring bell
        (0x40, 0x5A, 0xA9, 0x30, 0x04, 0),       # 3 pad
    ]
    songs = [dict(speed=8, start=0, route=0x44, mode=0x2F, cutinit=0x40, cutstep=3)]
    data, syms = build("Glass Bells", "", "", songs, inst, P, orders, None, cuthilo=(0xD0, 0x28))
    psid(os.path.join(out, "glassbells.sid"), data, syms, "Glass Bells", "fpga_cpu demo", "2026 original", 1)

# ----------------------------------------------------------------------------- tune 3: Sync Rider (hard sync)
def tune3(out):
    P = {}
    # voice 3 = sync master and bass (pulse); voice 1 = saw, hard-synced to voice 3, sweeping
    bass = lambda n, m: "%s/2 . . %s/2 . . %s/2 . . . . . %s/2 . . ." % (n, n, n, m)
    P["B0"] = bass("E-2", "G-2")
    P["B1"] = bass("C-2", "D-2")
    P["B2"] = bass("D-2", "A-2")
    P["B3"] = bass("B-1", "E-2")
    P["S0"] = "E-4/1 . . . . . . . G-4/1 . . . B-3/1 . . ."
    P["S1"] = "C-4/1 . . . . . G-3/1 . . . . E-4/1 . . . ."
    P["S2"] = "D-4/1 . . . F#4/1 . . . A-4/1 . . . . . --- ."
    P["S3"] = "B-3/1 . . . . . D-4/1 . E-4/1 . . . G-4/1 . . ."
    P["H0"] = "C-2/4 . F-6/5 . D-4/6 . F-6/5 . C-2/4 . F-6/5 . D-4/6 . F-6/5 F-6/5"
    P["H1"] = "C-2/4 . F-6/5 . D-4/6 . F-6/5 . C-2/4 C-2/4 F-6/5 . D-4/6 . D-4/6 D-4/6"
    P["R0"] = "E-5/3/4 . . . . . . . E-5/3/5 . . . . . . ."
    orders = [["S0", "S1", "S2", "S3", "S0", "S1", "S2", "S3"],
              ["H0", "H0", "H0", "H1", "H0", "H0", "H0", "H1"],
              ["B0", "B1", "B2", "B3", "B0", "B1", "B2", "B3"]]
    inst = [
        (0x23, 0x0A, 0xF9, 0, 0, 0x03),          # 1 sync lead, rising sweep (the slide changes the ratio to voice 3)
        (0x40, 0x08, 0x88, 0x80, 0x05, 0),       # 2 pulse bass / sync master
        (0x40, 0x03, 0x30, 0x20, 0, 0),          # 3 arp
        (0x80, 0x05, 0x00, 0, 0, 0xFA),          # 4 kick
        (0x80, 0x01, 0x00, 0, 0, 0),             # 5 hat
        (0x80, 0x07, 0x00, 0, 0, 0xFE),          # 6 snare
    ]
    songs = [dict(speed=5, start=0, route=0x91, mode=0x4F, cutinit=0x50, cutstep=7)]
    data, syms = build("Sync Rider", "", "", songs, inst, P, orders, None, cuthilo=(0xC8, 0x30))
    psid(os.path.join(out, "syncrider.sid"), data, syms, "Sync Rider", "fpga_cpu demo", "2026 original", 1)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "tunes")
    os.makedirs(out, exist_ok=True)
    tune1(out); tune2(out); tune3(out)
