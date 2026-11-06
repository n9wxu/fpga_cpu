#!/usr/bin/env python3
"""Run a generated test in the Python reference, printing each instruction with cycle count and registers.
usage: trace_ref.py NAME [from_pc] [to_pc]"""
import os
import struct
import sys
sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tests"))
import m6502 as M
from ref6502 import CPU, STOP
d = os.path.join(os.path.dirname(__file__), "..", "tests", "disk")
name = sys.argv[1]
lo = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0
hi = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x10000
for fn in sorted(os.listdir(d)):
    if not fn.startswith("TESTS"):
        continue
    f = open(os.path.join(d, fn), "rb")
    f.read(4)
    n, = struct.unpack("<H", f.read(2))
    for _ in range(n):
        nm = f.read(24).rstrip(b"\0").decode()
        flags, entry, a, x, y, s, p, maxc, play, nfr, fc = struct.unpack("<BHBBBBBIHHI", f.read(20))
        nseg, = struct.unpack("<H", f.read(2))
        segs = []
        for _ in range(nseg):
            ad, ln = struct.unpack("<HH", f.read(4))
            segs.append((ad, f.read(ln)))
        f.read(12)
        nsid, = struct.unpack("<H", f.read(2)); f.read(6 * nsid)
        nreg, = struct.unpack("<H", f.read(2)); f.read(4 * nreg)
        nd, = struct.unpack("<H", f.read(2)); f.read(3 * nd)
        if nm != name:
            continue
        c = CPU()
        for ad, data in segs:
            c.m[ad:ad + len(data)] = data
        c.a, c.x, c.y, c.s = a, x, y, s
        c.setp(p)
        c.m[0x100 + c.s] = (STOP - 1) >> 8
        c.s -= 1
        c.m[0x100 + c.s] = (STOP - 1) & 255
        c.s -= 1
        c.pc = entry
        while c.pc != STOP:
            pc = c.pc
            cyc = c.cycles
            ent = M.OPS[c.m[pc]]
            if not c.step():
                print("illegal"); break
            if lo <= pc < hi:
                print(f"{pc:04X} {ent[0]} {M.MODE_NAMES[ent[1]]:4s} cyc {cyc:5d} (+{c.cycles-cyc}) A={c.a:02X} X={c.x:02X} Y={c.y:02X} P={c.getp():02X} S={c.s:02X}")
        sys.exit(0)
