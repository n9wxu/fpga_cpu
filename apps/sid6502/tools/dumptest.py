#!/usr/bin/env python3
"""Disassemble one test of the generated vectors: dumptest.py NAME [dir]"""
import os
import struct
import sys
sys.path.insert(0, os.path.dirname(__file__))
import m6502 as M


def dis(code, start, base):
    pc = 0
    out = []
    while pc < len(code):
        opc = code[pc]
        e = M.OPS[opc]
        if not e:
            out.append(f"{start+pc:04X}  {opc:02X}        ???")
            pc += 1
            continue
        mn, mode, cyc, pen = e
        ln = M.MODE_LEN[mode]
        b = code[pc:pc + ln]
        v = b[1] if ln == 2 else (b[1] | b[2] << 8 if ln == 3 else None)
        fmt = {M.IMP: "", M.ACC: " A", M.IMM: f" #${v:02X}" if v is not None else "", M.ZP: f" ${v:02X}" if v is not None else "",
               M.ZPX: f" ${v:02X},X" if v is not None else "", M.ZPY: f" ${v:02X},Y" if v is not None else "",
               M.ABS: f" ${v:04X}" if v is not None else "", M.ABX: f" ${v:04X},X" if v is not None else "",
               M.ABY: f" ${v:04X},Y" if v is not None else "", M.IND: f" (${v:04X})" if v is not None else "",
               M.IZX: f" (${v:02X},X)" if v is not None else "", M.IZY: f" (${v:02X}),Y" if v is not None else "",
               M.REL: f" ${(start+pc+2+(b[1]-256 if b[1]>127 else b[1]))&0xFFFF:04X}" if ln == 2 else ""}[mode]
        out.append(f"{start+pc:04X}  {' '.join(f'{x:02X}' for x in b):9s} {mn}{fmt}")
        pc += ln
    return out


name = sys.argv[1]
d = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "tests", "disk")
for fn in sorted(os.listdir(d)):
    if not fn.startswith("TESTS"):
        continue
    f = open(os.path.join(d, fn), "rb")
    assert f.read(4) == b"SIDT"
    n, = struct.unpack("<H", f.read(2))
    for _ in range(n):
        nm = f.read(24).rstrip(b"\0").decode()
        flags, entry, a, x, y, s, p, maxc, play, nfr, fc = struct.unpack("<BHBBBBBIHHI", f.read(20))
        nseg, = struct.unpack("<H", f.read(2))
        segs = []
        for _ in range(nseg):
            ad, ln = struct.unpack("<HH", f.read(4))
            segs.append((ad, f.read(ln)))
        st, ea, ex, ey, es, ep, epc, ecyc = struct.unpack("<BBBBBBHI", f.read(12))
        nsid, = struct.unpack("<H", f.read(2))
        sid = [struct.unpack("<IBB", f.read(6)) for _ in range(nsid)]
        nreg, = struct.unpack("<H", f.read(2))
        regs = [struct.unpack("<HH", f.read(4)) for _ in range(nreg)]
        nd, = struct.unpack("<H", f.read(2))
        deltas = [struct.unpack("<HB", f.read(3)) for _ in range(nd)]
        if nm == name:
            print(f"{nm}: entry {entry:04X} A={a:02X} X={x:02X} Y={y:02X} S={s:02X} P={p:02X} play={play:04X} frames={nfr}")
            for ad, data in segs:
                if ad in (0x800, 0x900, 0x980, 0x9c0) or (ad >= 0x1000 and ad < 0x1100):
                    print(f"-- segment {ad:04X} ({len(data)} bytes)")
                    print("\n".join(dis(data, ad, ad)))
                else:
                    print(f"-- data {ad:04X} ({len(data)} bytes)", data[:16].hex())
            print(f"expect: status {st} A={ea:02X} X={ex:02X} Y={ey:02X} S={es:02X} P={ep:02X} pc={epc:04X} cycles={ecyc}")
            print("sid:", sid[:12])
            print("regions", [(hex(r[0]), r[1]) for r in regs], "deltas", len(deltas))
            sys.exit(0)
print("not found")
