#!/usr/bin/env python3
"""Write the synthetic music routines for the benchmark: MUSIC0.BIN (base), MUSIC2K.BIN, MUSIC5K.BIN.
File: u16 init, u16 play, u16 nseg, {u16 addr, u16 len, bytes}.  Prints the reference cycles per play call."""
import os
import struct
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "tests"))
import gen_tests as g
import m6502
from ref6502 import CPU, STOP

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "tests", "disk")
for fn, extra, reps in (("MUSIC0.BIN", 0, 1), ("MUSIC2K.BIN", 93, 1), ("MUSIC5K.BIN", 135, 2)):
    segs, syms = g.music_image(extra, reps)
    with open(os.path.join(out, fn), "wb") as f:
        f.write(struct.pack("<HHH", syms["init"], syms["play"], len(segs)))
        for a, d in segs:
            f.write(struct.pack("<HH", a, len(d)) + d)
    c = CPU()
    for a, d in segs:
        c.m[a:a + len(d)] = d

    def call(addr):
        c.m[0x100 + c.s] = (STOP - 1) >> 8
        c.s -= 1
        c.m[0x100 + c.s] = (STOP - 1) & 255
        c.s -= 1
        c.pc = addr
        c.run()
    call(syms["init"])
    cs = []
    for _ in range(50):
        c0 = c.cycles
        c.s = 0xFF
        call(syms["play"])
        cs.append(c.cycles - c0)
    print(fn, "play cycles per frame: avg", sum(cs) // len(cs), "min", min(cs), "max", max(cs), "sid writes/50 frames", len(c.sid))

# a sample-playback ("digi") routine: 160 volume-register writes per frame, about 8 kHz
DIGI = """
        .org $1000
init:   rts
play:   ldy #0
lp:     lda sample,y
        sta $D418
        ldx #21
dl:     dex
        bne dl
        iny
        cpy #160
        bne lp
        rts
        .org $1100
sample: .byte 0
"""
out_, syms = m6502.assemble(DIGI, 0x1000)
segs = [(a, bytes(out_[a + i] for i in range(n))) for a, n in ((0x1000, max(k for k in out_ if k < 0x1100) - 0x1000 + 1),)]
segs.append((0x1100, bytes((i * 37 + (i >> 2)) & 0x0F for i in range(256))))
with open(os.path.join(out, "DIGI.BIN"), "wb") as f:
    f.write(struct.pack("<HHH", syms["init"], syms["play"], len(segs)))
    for a, d in segs:
        f.write(struct.pack("<HH", a, len(d)) + d)
c = CPU()
for a, d in segs:
    c.m[a:a + len(d)] = d
c.m[0x1FF] = (STOP - 1) >> 8
c.m[0x1FE] = (STOP - 1) & 255
c.s = 0xFD
c.pc = syms["play"]
c.run()
print("DIGI.BIN play cycles per frame", c.cycles, "sid writes", len(c.sid))
