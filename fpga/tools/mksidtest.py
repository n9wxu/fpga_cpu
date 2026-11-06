#!/usr/bin/env python3
"""Write a small PSID test tune (our own, for fpga/demo/sidplay.c's tests).

Three voices step through a C major scale together, a note every 12
frames (50 Hz), each held for 9: voice 1 a pulse wave (its width sweeping),
voice 2 a sawtooth a fifth below, voice 3 a triangle two octaves below.
The 6502 code is assembled here (a few instructions, by hand).

  mksidtest.py out.sid"""

import struct
import sys

LOAD = 0x1000
ZP_CNT, ZP_STEP, ZP_PW = 0xFB, 0xFC, 0xFD


class Asm:
    def __init__(self, org):
        self.org = org
        self.b = bytearray()
        self.labels = {}
        self.fix = []                         # (offset, label, kind)

    def pc(self):
        return self.org + len(self.b)

    def label(self, n):
        self.labels[n] = self.pc()

    def op(self, *bs):
        self.b += bytes(bs)

    def abs_(self, code, a):
        self.op(code, a & 0xFF, a >> 8)

    def abs_label(self, code, n):
        self.op(code, 0, 0)
        self.fix.append((len(self.b) - 2, n, 'abs'))

    def br(self, code, n):
        self.op(code, 0)
        self.fix.append((len(self.b) - 1, n, 'rel'))

    def done(self):
        for off, n, kind in self.fix:
            t = self.labels[n]
            if kind == 'abs':
                self.b[off:off + 2] = struct.pack('<H', t)
            else:
                d = t - (self.org + off + 1)
                assert -128 <= d < 128
                self.b[off] = d & 0xFF
        return bytes(self.b)


def note(hz):
    return round(hz * 16777216 / 985248)


def main():
    scale = [261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25]
    v1 = [note(f * 2) for f in scale]           # C5 up
    v2 = [note(f * 2 / 1.5) for f in scale]     # a fifth below
    v3 = [note(f / 2) for f in scale]           # C3 up

    a = Asm(LOAD)
    # init: volume, envelopes, counters
    a.op(0xA9, 0x0F); a.abs_(0x8D, 0xD418)                     # LDA #$0F; STA $D418
    a.op(0xA9, 0x09)
    for r in (0xD405, 0xD40C, 0xD413):
        a.abs_(0x8D, r)                                          # attack 0, decay 9
    a.op(0xA9, 0xA8)
    for r in (0xD406, 0xD40D, 0xD414):
        a.abs_(0x8D, r)                                          # sustain 10, release 8
    a.op(0xA9, 0x00); a.op(0x85, ZP_CNT); a.op(0x85, ZP_STEP)
    a.op(0xA9, 0x02); a.op(0x85, ZP_PW)
    a.op(0x60)                                                   # RTS
    play = a.pc()
    # play: a new note when the counter is 0
    a.op(0xA6, ZP_CNT)                                           # LDX cnt
    a.br(0xD0, 'held')                                           # BNE held
    a.op(0xA4, ZP_STEP)                                          # LDY step
    for lo, hi, ctrl, base in (('l1', 'h1', 0x41, 0xD400), ('l2', 'h2', 0x21, 0xD407), ('l3', 'h3', 0x11, 0xD40E)):
        a.abs_label(0xB9, lo); a.abs_(0x8D, base)                # LDA lo,Y; STA freq lo
        a.abs_label(0xB9, hi); a.abs_(0x8D, base + 1)            # LDA hi,Y; STA freq hi
        a.op(0xA9, ctrl); a.abs_(0x8D, base + 4)                 # gate on
    a.op(0xC8, 0x98, 0x29, 0x07, 0x85, ZP_STEP)                  # INY; TYA; AND #7; STA step
    a.op(0xA2, 12)                                               # LDX #12
    a.label('held')
    a.op(0xCA, 0x86, ZP_CNT)                                     # DEX; STX cnt
    a.op(0xE0, 3)                                                # CPX #3
    a.br(0xD0, 'sweep')
    for ctrl, base in ((0x40, 0xD400), (0x20, 0xD407), (0x10, 0xD40E)):
        a.op(0xA9, ctrl); a.abs_(0x8D, base + 4)                 # gate off
    a.label('sweep')
    a.op(0xE6, ZP_PW); a.op(0xA5, ZP_PW); a.op(0x29, 0x0F); a.abs_(0x8D, 0xD403)   # pulse width hi
    a.op(0x60)
    for n, tab in (('l1', [x & 0xFF for x in v1]), ('h1', [x >> 8 for x in v1]),
                   ('l2', [x & 0xFF for x in v2]), ('h2', [x >> 8 for x in v2]),
                   ('l3', [x & 0xFF for x in v3]), ('h3', [x >> 8 for x in v3])):
        a.label(n)
        a.b += bytes(tab)
    code = a.done()

    hdr = struct.pack('>4sHHHHHHHI', b'PSID', 2, 0x7C, LOAD, LOAD, play, 1, 1, 0)
    hdr += b'Scale test'.ljust(32, b'\0') + b'fpga_cpu'.ljust(32, b'\0') + b'2026'.ljust(32, b'\0')
    hdr += struct.pack('>HBBH', 0, 0, 0, 0)
    assert len(hdr) == 0x7C
    open(sys.argv[1], 'wb').write(hdr + code)


if __name__ == '__main__':
    main()
