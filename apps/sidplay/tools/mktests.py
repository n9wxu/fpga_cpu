#!/usr/bin/env python3
"""Variants of tune 1 that exercise the loader: PSID v1 header, RSID, play address 0, a CIA-timed tune (60 Hz via
timer A programmed by init), an NTSC-flagged tune.  usage: mktests.py OUTDIR  (writes tests/*.sid)"""
import os, struct, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mktunes
sys.path.insert(0, os.path.join(HERE, "..", "..", "sid6502", "tools"))
import m6502

def hdr(magic=b"PSID", ver=2, off=0x7C, load=0, init=0x1000, play=0x1003, songs=1, speed=0, flags=0x14, name="test"):
    def s32(x): return x.encode()[:31].ljust(32, b"\0")
    h = magic + struct.pack(">HHHHHHHI", ver, off, load, init, play, songs, 1, speed) + s32(name) + s32("fpga_cpu test") + s32("2026")
    if ver >= 2: h += struct.pack(">HBBBB", flags, 0, 0, 0, 0)
    assert len(h) == off
    return h

out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "tests")
os.makedirs(out, exist_ok=True)
# the music data of tune 1 without a header: re-use mktunes.build through its output file
tmp = os.path.join(out, "_t")
os.makedirs(tmp, exist_ok=True)
mktunes.tune1(tmp)
raw = open(os.path.join(tmp, "neondrift.sid"), "rb").read()[0x7C:]       # load address + data
os.remove(os.path.join(tmp, "neondrift.sid")); os.rmdir(tmp)
w = lambda n, d: open(os.path.join(out, n), "wb").write(d)
w("v1.sid", hdr(ver=1, off=0x76, name="v1 header") + raw)
w("rsid.sid", hdr(magic=b"RSID", name="rsid tune") + raw)
w("noplay.sid", hdr(play=0, name="play = 0") + raw)
w("ntsc.sid", hdr(flags=0x08, name="ntsc flag") + raw)
# CIA timed: an init stub at $0F00 programs timer A for 60 Hz (PAL clock: 16421 cycles) and jumps to the driver's init
stub, _ = m6502.assemble("""
        .org $0F00
        pha
        lda #$25
        sta $DC04
        lda #$40
        sta $DC05
        pla
        jmp $1000
""", 0x0F00)
sd = bytes(stub[a] for a in sorted(stub))
segs = struct.pack("<H", 0x0F00) + sd + bytes(0x1000 - 0x0F00 - len(sd)) + raw[2:]
w("cia60.sid", hdr(init=0x0F00, speed=1, name="cia timer 60 Hz") + segs)
print("wrote", sorted(os.listdir(out)))
