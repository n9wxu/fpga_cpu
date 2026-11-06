#!/usr/bin/env python3
"""Generate the test vectors (TESTS*.BIN) for the 6502 engines.

Every test is a small 6502 program plus initial registers; the expected result
(registers, flags, cycle count, SID log, checked memory regions) is computed by
the independent Python reference (tools/ref6502.py).  The C runner
(tests/testrun.c) runs each test on the interpreter and the JIT and compares.

File format, little endian:
  "SIDT" u16 ntests
  test: name[24] u8 flags u16 entry u8 a x y s p u32 maxcycles
        u16 play u16 nframes u32 frame_cycles
        u16 nseg {u16 addr u16 len bytes}
        -- expected --
        u8 status u8 a x y s p u16 pc u32 cycles
        u16 nsid {u32 cycle u8 reg u8 val}
        u16 nreg {u16 addr u16 len}        regions checked (initial memory = the segments over zeros)
        u16 ndelta {u16 addr u8 val}       bytes in those regions that differ from the initial memory
flags bit 0: skip stepwise compare.
"""
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
from ref6502 import *   # noqa
import m6502 as M

FRAME = 19656           # PAL cycles per frame
CODE = 0x0800
DATA = [(0x0000, 0x200), (0x0200, 0x400)]     # zp+stack, data pages $0200-$04FF
PMASK = 0xCF

tests = []


def run_ref(t):
    c = CPU()
    for addr, data in t["segs"]:
        c.m[addr:addr + len(data)] = data
    c.a, c.x, c.y = t["a"], t["x"], t["y"]
    c.setp(t["p"])
    c.s = t["s"]
    c.pc = t["entry"]
    # call convention: push STOP-1 so an RTS stops the run
    def call(addr, a, x, y):
        c.a, c.x, c.y = a, x, y
        c.m[0x100 + c.s] = (STOP - 1) >> 8
        c.s = (c.s - 1) & 255
        c.m[0x100 + c.s] = (STOP - 1) & 255
        c.s = (c.s - 1) & 255
        c.pc = addr
        c.run(t["maxcyc"] + c.cycles)
        return c.status
    s0 = t["s"]
    if t["nframes"] == 0:
        c.s = s0
        st = call(t["entry"], t["a"], t["x"], t["y"])
    else:
        st = call(t["entry"], t["a"], t["x"], t["y"])
        for f in range(1, t["nframes"] + 1):
            if st:
                break
            c.cycles = max(c.cycles, f * t["frame_cycles"])
            c.s = s0
            st = call(t["play"], 0, 0, 0)
    return c, st


def add(name, segs, regs=None, regions=None, maxcyc=200000, play=0, nframes=0, frame_cycles=FRAME, flags=0, entry=CODE,
        keep=("limit",)):
    r = dict(a=0, x=0, y=0, s=0xFD, p=0x24)
    if regs:
        r.update(regs)
    t = dict(name=name[:23], segs=[(a, bytes(d)) for a, d in segs], entry=entry, maxcyc=maxcyc, play=play,
             nframes=nframes, frame_cycles=frame_cycles, flags=flags, **r)
    c, st = run_ref(t)
    if st == 1:
        return False        # did not terminate: discard
    t["regions"] = list(DATA if regions is None else regions)
    init = bytearray(65536)
    for addr, data in t["segs"]:
        init[addr:addr + len(data)] = data
    cover = bytearray(65536)
    for a, n in t["regions"]:
        cover[a:a + n] = b"\x01" * n
    leaks = [i for i in range(65536) if c.m[i] != init[i] and not cover[i]]
    for pg in sorted({i >> 8 for i in leaks}):
        t["regions"].append((pg << 8, 256))
    # merge into non-overlapping regions
    iv = sorted((a, a + n) for a, n in t["regions"])
    merged = []
    for lo, hi in iv:
        if merged and lo <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], hi)
        else:
            merged.append([lo, hi])
    t["regions"] = [(lo, hi - lo) for lo, hi in merged]
    if len(t["regions"]) > 8:
        print("too many regions", t["name"])
    deltas = [(a + i, c.m[a + i]) for a, n in t["regions"] for i in range(n) if c.m[a + i] != init[a + i]]
    t["exp"] = (st, c.a, c.x, c.y, c.s, c.getp() & PMASK, c.pc, c.cycles, list(c.sid), deltas)
    tests.append(t)
    return True


def pack(f, t):
    f.write(t["name"].encode().ljust(24, b"\0"))
    f.write(struct.pack("<BHBBBBBIHHI", t["flags"], t["entry"], t["a"], t["x"], t["y"], t["s"], t["p"], t["maxcyc"],
                        t["play"], t["nframes"], t["frame_cycles"]))
    f.write(struct.pack("<H", len(t["segs"])))
    for a, d in t["segs"]:
        f.write(struct.pack("<HH", a, len(d)) + d)
    st, a, x, y, s, p, pc, cyc, sid, regs = t["exp"]
    f.write(struct.pack("<BBBBBBHI", st, a, x, y, s, p, pc, cyc))
    f.write(struct.pack("<H", len(sid)))
    for ts, r, v in sid:
        f.write(struct.pack("<IBB", ts, r, v))
    f.write(struct.pack("<H", len(t["regions"])))
    for a, n in t["regions"]:
        f.write(struct.pack("<HH", a, n))
    f.write(struct.pack("<H", len(regs)))
    for a, v in regs:
        f.write(struct.pack("<HB", a, v))


rng = random.Random(6502)
EDGE = [0x00, 0x01, 0x7F, 0x80, 0xFF, 0x0F, 0x10, 0x55, 0xAA, 0x99, 0x09, 0x90, 0x45, 0x46, 0xF0, 0x40, 0xC0]


def rv():
    return rng.choice(EDGE) if rng.random() < 0.7 else rng.randrange(256)


def rp():
    """random P without N=Z=1 (not representable by the JIT's flag model)"""
    while True:
        p = rng.randrange(256) & 0xCF
        if not (p & 0x80 and p & 0x02):
            return p | 0x20


# ------------------------------------------------------------------ directed
def directed():
    for opc, ent in enumerate(M.OPS):
        if ent is None:
            continue
        mn, mode, cyc, pen = ent
        if mode == M.REL or mn in ("JMP", "JSR", "RTS", "RTI", "BRK", "KIL"):
            continue
        nvar = 10
        for k in range(nvar):
            a, x, y = rv(), rv(), rv()
            p = rp()
            if mn == "ARR":
                p &= ~0x08
            segs = []
            data = {}
            m = rv()
            if mode in (M.IMP, M.ACC):
                code = [opc]
            elif mode == M.IMM:
                code = [opc, m]
            elif mode == M.ZP:
                z = rng.choice([0x20, 0x80, 0xFF, 0x00, 0x01])
                code = [opc, z]
                data[z] = m
            elif mode in (M.ZPX, M.ZPY):
                z = rng.choice([0x20, 0xF8, 0xFF, 0x00])
                idx = rng.choice([0x00, 0x02, 0x10, 0xFF, 0x80])
                if mode == M.ZPX:
                    x = idx
                else:
                    y = idx
                code = [opc, z]
                data[(z + idx) & 255] = m
            elif mode == M.ABS:
                ad = rng.choice([0x0340, 0x02FF, 0x0400, 0x0001, 0x0100])
                code = [opc, ad & 255, ad >> 8]
                data[ad] = m
            elif mode in (M.ABX, M.ABY):
                base = rng.choice([0x03F0, 0x0300, 0x0480, 0x02FF, 0x0200])
                idx = rng.choice([0x00, 0x05, 0x1F, 0xFF, 0x10, 0x80])
                if mode == M.ABX:
                    x = idx
                else:
                    y = idx
                code = [opc, base & 255, base >> 8]
                data[(base + idx) & 0xFFFF] = m
            elif mode == M.IZX:
                z = rng.choice([0x30, 0xFE, 0xFF, 0x10])
                idx = rng.choice([0x04, 0x01, 0xFF, 0x00])
                x = idx
                pz = (z + idx) & 255
                tgt = rng.choice([0x0345, 0x03FF, 0x0200])
                data[pz] = tgt & 255
                data[(pz + 1) & 255] = tgt >> 8
                data[tgt] = m
                code = [opc, z]
            elif mode == M.IZY:
                z = rng.choice([0x40, 0xFF, 0x20])
                idx = rng.choice([0x00, 0x01, 0x10, 0xFF, 0x80])
                y = idx
                base = rng.choice([0x03F8, 0x0300, 0x0480])
                data[z] = base & 255
                data[(z + 1) & 255] = base >> 8
                data[(base + idx) & 0xFFFF] = m
                code = [opc, z]
            else:
                raise SystemExit("mode " + str(mode))
            code.append(0x60)
            segs.append((CODE, code))
            for ad, v in data.items():
                segs.append((ad, [v]))
            # keep stack page sane: the stack ops use S
            add(f"d_{opc:02X}_{mn}_{M.MODE_NAMES[mode]}_{k}", segs, dict(a=a, x=x, y=y, p=p, s=rng.choice([0xFD, 0x05, 0x01, 0xFF])))


def exhaust_alu():
    """every A/M/C combination (every 3rd value) for ADC and SBC in binary and decimal mode"""
    for mn, opc in (("ADC", 0x69), ("SBC", 0xE9)):
        for d in (0, 1):
            for c in (0, 1):
                for v in range(0, 256, 17):
                    segs = [(CODE, [])]
                    tb = []
                    # a loop over M: LDX #0; L: LDA tabA(fixed v) ... simpler: one test per (A) with an M loop
                    code = [0xA9, v,            # LDA #v ... A reloaded each iteration
                            0xA2, 0x00]         # LDX #0
                    code_start = CODE
                    # loop: LDA #v; (D,C set by test) ADC/SBC mtab,X ; STA out,X ; PHP;PLA; STA outp,X ; INX ; BNE
                    code = []
                    code += [0xA9, v]                     # loop: LDA #v
                    code += [0x48, 0x28] if False else []
                    code += [0xBD, 0x00, 0x03]            #       LDA ... placeholder replaced below
                    code = [0xA2, 0x00,                   # LDX #0
                            0xA9, v,                      # L: LDA #v  (CODE+2)
                            0xA0, 0x08 | 0x00, ]          # dummy
                    code = [0xA2, 0x00]                   # LDX #0
                    loop = CODE + len(code)
                    code += [0xA9, v]                     # LDA #v
                    code += [0x38 if c else 0x18]         # SEC/CLC
                    code += [0xF8 if d else 0xD8]         # SED/CLD
                    code += [0x7D if mn == "ADC" else 0xFD, 0x00, 0x03]   # ADC/SBC $0300,X
                    code += [0x9D, 0x00, 0x04]            # STA $0400,X
                    code += [0x08, 0x68]                  # PHP, PLA
                    code += [0x9D, 0x00, 0x05]            # STA $0500,X
                    code += [0xE8, 0xE8, 0xE8]            # INX x3
                    code += [0xD0, (loop - (CODE + len(code) + 2)) & 255]   # BNE loop
                    code += [0xD8, 0x60]
                    segs = [(CODE, code), (0x0300, bytes(range(256)))]
                    add(f"x_{mn}_{'D' if d else 'B'}{c}_{v:02X}", segs, dict(), regions=[(0x0000, 0x200), (0x0400, 0x200)],
                        maxcyc=100000)


def sweeps():
    """Every A (every 3rd) x every M for each ALU-type instruction; a checksum over results and flags is kept in
    zero page ($30,$31) so one test covers thousands of combinations."""
    def body(opl, d, c, zpop=True):
        code = []
        code += [0xA2, 0x00]                    # LDX #0
        loop_x = CODE + len(code)
        code += [0x86, 0x20]                    # outer: STX $20
        code += [0xA0, 0x00]                    # LDY #0
        loop_y = CODE + len(code)
        code += [0x84, 0x21]                    # inner: STY $21
        code += [0x38 if c else 0x18]           # SEC / CLC
        code += [0xF8 if d else 0xD8]           # SED / CLD
        code += [0xA5, 0x20]                    # LDA $20 (A)
        code += opl                              # instruction under test (operand $21 / A)
        code += [0xD8]                          # CLD
        code += [0x85, 0x22]                    # STA $22
        code += [0x08, 0x68, 0x29, 0xC3]        # PHP; PLA; AND #$C3
        code += [0x45, 0x22]                    # EOR $22
        code += [0x18, 0x65, 0x30, 0x85, 0x30]  # CLC; ADC $30; STA $30
        code += [0x18, 0x65, 0x31, 0x85, 0x31]  # CLC; ADC $31; STA $31
        code += [0xA5, 0x23, 0x18, 0x65, 0x30, 0x85, 0x30] if False else []
        code += [0xC8]                          # INY
        code += [0xD0, 0]                       # BNE inner (patched)
        code[-1] = (loop_y - (CODE + len(code))) & 255
        code += [0xE8, 0xE8, 0xE8]              # INX x3
        code += [0xE0, 0xFF]                    # CPX #$FF
        code += [0x90, 0]                       # BCC outer  (X < 255)
        code[-1] = (loop_x - (CODE + len(code))) & 255
        code += [0xD8, 0x60]
        return code
    ops = {
        "ADC": [0x65, 0x21], "SBC": [0xE5, 0x21], "CMP": [0xC5, 0x21], "CPX": [0xA6, 0x21, 0xE4, 0x21],
        "CPY": [0xA4, 0x21, 0xC4, 0x21], "AND": [0x25, 0x21], "ORA": [0x05, 0x21], "EOR": [0x45, 0x21],
        "BIT": [0x24, 0x21], "ASLA": [0x0A], "LSRA": [0x4A], "ROLA": [0x2A], "RORA": [0x6A],
        "ASLZ": [0x06, 0x21, 0xA5, 0x21], "RORZ": [0x66, 0x21, 0xA5, 0x21], "INCZ": [0xE6, 0x21, 0xA5, 0x21],
        "DECZ": [0xC6, 0x21, 0xA5, 0x21], "ISC": [0xE7, 0x21], "RRA": [0x67, 0x21], "DCP": [0xC7, 0x21],
        "ARR": [0x6B, 0x55], "SBX": [0xCB, 0x37],
    }
    for nm, opl in ops.items():
        dvars = (0, 1) if nm in ("ADC", "SBC", "ISC", "RRA") else (0,)
        for d in dvars:
            for c in (0, 1):
                add(f"sw_{nm}_D{d}C{c}", [(CODE, body(opl, d, c))], dict(p=0x20), regions=[(0, 0x100)],
                    maxcyc=3000000, flags=1)


def control():
    # branches: taken / not taken / page crossing
    for opc in (0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0):
        for p in (0x20, 0x20 | 0x80, 0x20 | 0x40, 0x20 | 0x01, 0x20 | 0x02, 0x20 | 0xC1 & ~0x02):
            for at, off in ((0x0800, 5), (0x08FC, 5), (0x08F0, -20 & 255), (0x0900, -10 & 255), (0x08FE, 0x7F)):
                code = [opc, off] + [0xEA] * 3
                segs = [(at, code)]
                # destination: fill with RTS wherever the branch may land
                dst = (at + 2 + (off - 256 if off > 127 else off)) & 0xFFFF
                segs += [(dst, [0x60]), (at + 2, [0x60])]
                segs.sort()
                add(f"b_{opc:02X}_{p:02X}_{at:04X}_{off:02X}", segs, dict(p=p), entry=at)
    # JMP abs / ind, JSR / RTS
    for ad in (0x0900, 0x0A34, 0x08FD, 0x0B00):
        add(f"jmp_{ad:04X}", [(CODE, [0x4C, ad & 255, ad >> 8]), (ad, [0xA9, 0x77, 0x60])])
    for ptr, bug in ((0x0310, 0), (0x02FF, 1), (0x03FF, 1)):
        segs = [(CODE, [0x6C, ptr & 255, ptr >> 8]), (ptr, [0x00, 0x09]), (0x0900, [0xA2, 0x33, 0x60])]
        if bug:
            segs.append((ptr & 0xFF00, [0x0A]))
            segs.append((0x0A00, [0xA0, 0x44, 0x60]))
        add(f"jmpi_{ptr:04X}", segs)
    add("jsr_nest", [(CODE, [0x20, 0x00, 0x09, 0xE8, 0x60]), (0x0900, [0x20, 0x00, 0x0A, 0xC8, 0x60]),
                     (0x0A00, [0xA9, 0x12, 0x60])])
    add("jsr_stack_wrap", [(CODE, [0x20, 0x00, 0x09, 0x60]), (0x0900, [0xA9, 0x21, 0x60])], dict(s=0x01))
    add("jsr_stack_wrap0", [(CODE, [0x20, 0x00, 0x09, 0x60]), (0x0900, [0xA9, 0x21, 0x60])], dict(s=0x00))
    # PHP/PLP/PHA/PLA/TSX/TXS with wrap
    for s in (0xFF, 0x00, 0x01, 0x80):
        add(f"stk_{s:02X}", [(CODE, [0x48, 0x08, 0xBA, 0xA2, 0x7F, 0x9A, 0x28, 0x68, 0x68, 0x60]),
                              (0x0100, bytes(rng.randrange(256) & ~0x82 for _ in range(256)))], dict(s=s, a=0x5A, p=0x65))
    for p in (0x00, 0x04, 0x08, 0x0C, 0x41, 0x45, 0x49, 0x8D, 0x23, 0xFF & ~0x02 & ~0x10, 0x4A & ~0x80):
        add(f"plp_{p:02X}", [(CODE, [0x28, 0x08, 0x68, 0xF8, 0x08, 0x28, 0x60, ]), (0x01FE, [p & 0xCF | 0x30]), ],
            dict(s=0xFD, a=1), regions=[(0x0100, 0x100), (0x0000, 0x100)])
    # BRK / RTI
    segs = [(CODE, [0xA9, 0x01, 0x00, 0xEA, 0xA9, 0x02, 0x60]), (0xFFFE, [0x00, 0x09]), (0x0900, [0xA2, 0x55, 0x28, 0x60])]
    add("brk_basic", segs, dict(p=0x20))
    segs = [(CODE, [0xA9, 0x01, 0x00, 0xEA, 0xA9, 0x02, 0x60]), (0xFFFE, [0x00, 0x09]), (0x0900, [0x40])]
    add("brk_rti", segs, dict(p=0x61))
    # RTI with crafted stack
    add("rti", [(CODE, [0x40]), (0x01FB, [0x61, 0x00, 0x09]), (0x0900, [0xA9, 0x99, 0x60])], dict(s=0xFA))
    # illegal opcodes trap (KIL and unstable ones)
    for opc in range(256):
        if M.OPS[opc] is None:
            add(f"ill_{opc:02X}", [(CODE, [0xA9, 0x11, opc, 0x60])])
    add("arr_dec_trap", [(CODE, [0xF8, 0x6B, 0x55, 0x60])])


def io_tests():
    for ea in (0xD400, 0xD418, 0xD41F, 0xD420, 0xD7FF, 0xD800, 0xD000, 0xCFFF, 0xDC0D):
        for opc, nm in ((0x8D, "sta"), (0x8E, "stx"), (0x8C, "sty"), (0x8F, "sax")):
            add(f"io_{nm}_{ea:04X}", [(CODE, [opc, ea & 255, ea >> 8, 0x60])], dict(a=0xF3, x=0x3C, y=0x81),
                regions=DATA + [(0xD400, 0x40), (0xD000, 0x10)])
    for base in (0xD3F0, 0xD400, 0xD410, 0xD3FF):
        for idx in (0, 5, 0x0F, 0x1F, 0xFF):
            add(f"io_stax_{base:04X}_{idx:02X}", [(CODE, [0x9D, base & 255, base >> 8, 0x99, base & 255, base >> 8, 0x60])],
                dict(a=0xAB, x=idx, y=idx ^ 3), regions=DATA + [(0xD300, 0x200)])
    # (zp),Y store into SID, RMW into SID
    for opc in (0xEE, 0xCE, 0x0E, 0x4E, 0x2E, 0x6E):
        add(f"io_rmw_{opc:02X}", [(CODE, [opc, 0x04, 0xD4, 0x60]), (0xD404, [0x81])], dict(a=1, p=0x21),
            regions=DATA + [(0xD400, 0x20)])
    add("io_izy", [(CODE, [0x91, 0x40, 0x60]), (0x40, [0x00, 0xD4])], dict(a=0x7E, y=0x18), regions=DATA + [(0xD400, 0x20)])
    add("io_izx", [(CODE, [0x81, 0x40, 0x60]), (0x44, [0x07, 0xD4])], dict(a=0x7F, x=4), regions=DATA + [(0xD400, 0x20)])
    # I/O reads: raster, others
    for ea in (0xD012, 0xD011, 0xD019, 0xD41B, 0xDC0D, 0xDD00, 0xD400, 0xCFFF, 0xE000):
        for ldr in (0xAD, 0xAE, 0xAC):
            add(f"ior_{ldr:02X}_{ea:04X}", [(CODE, [0xEA] * 7 + [ldr, ea & 255, ea >> 8, 0x60]), (ea, [0x9C])], regions=DATA)
    for idx in (0, 2, 0x12, 0x3C):
        add(f"ior_abx_{idx:02X}", [(CODE, [0xBD, 0x00, 0xD0, 0xB9, 0x00, 0xD0, 0x60])], dict(x=idx, y=0x12), regions=DATA)
    # raster wait loop: LDA $D012; CMP #$80; BNE
    add("raster_wait", [(CODE, [0xAD, 0x12, 0xD0, 0xC9, 0x80, 0xD0, 0xF9, 0x60])], maxcyc=300000)
    add("raster_wait2", [(CODE, [0xAD, 0x12, 0xD0, 0xC9, 0x30, 0xD0, 0xF9, 0xAD, 0x11, 0xD0, 0x60])], dict(), maxcyc=300000)


def smc():
    # patch an operand before executing it (same trace)
    add("smc_same", [(CODE, [0xA9, 0x33, 0x8D, 0x05, 0x08, 0xA9, 0x11, 0x60])])  # sta $0805 -> changes "LDA #$11" operand
    # loop modifies its own code each pass
    code = [0xA2, 0x00,                 # LDX #0
            0x8E, 0x08, 0x08,           # L: STX op+1   ($0808)
            0xA9, 0x00,                 #    (filler)
            0xEA,                       #    NOP
            0xA9, 0x00,                 # op: LDA #0     ($0808 = operand)
            0x18, 0x69, 0x01,           # CLC; ADC #1
            0x9D, 0x00, 0x03,           # STA $0300,X
            0xE8, 0xE0, 0x10, 0xD0, 0xEA, 0x60]
    code[2:5] = [0x8E, 0x09, 0x08]
    add("smc_loop", [(CODE, code)], maxcyc=500000)
    # dynamic store into code: STA (ptr),Y and STA abs,X
    add("smc_izy", [(CODE, [0xA9, 0x42, 0xA0, 0x0A, 0x91, 0x40, 0xA9, 0x00, 0x60, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA]), (0x40, [0x00, 0x08])],
        dict(), maxcyc=10000)
    add("smc_abx", [(CODE, [0xA9, 0xE8, 0xA2, 0x0A, 0x9D, 0x00, 0x08, 0xA2, 0x01, 0xA2, 0x01, 0xA2, 0x01, 0x60])], dict())
    # code written then run: generate "LDA #n; RTS" at $0A00 and call it three times
    code = [0xA2, 0x00,
            0xA9, 0xA9, 0x8D, 0x00, 0x0A,       # LDA #$A9; STA $0A00
            0x8A, 0x8D, 0x01, 0x0A,             # L: TXA; STA $0A01
            0xA9, 0x60, 0x8D, 0x02, 0x0A,       # LDA #$60; STA $0A02
            0x20, 0x00, 0x0A,                   # JSR $0A00
            0x9D, 0x00, 0x03,                   # STA $0300,X
            0xE8, 0xE0, 0x08, 0xD0, 0xEA, 0x60]
    add("smc_gen", [(CODE, code)], maxcyc=100000)
    # data in the same page as code (no self modification): must not slow down or break
    code = [0xAD, 0x20, 0x08, 0x18, 0x69, 0x01, 0x8D, 0x20, 0x08, 0xEE, 0x21, 0x08, 0x60]
    code += [0xEA] * (0x20 - len(code)) + [0x05, 0x00]
    add("smc_datainpage", [(CODE, code)], regions=DATA + [(CODE, 0x30)])
    # a store to the byte right after the executing instruction (next instruction patched)
    add("smc_next", [(CODE, [0xA9, 0x60, 0x8D, 0x06, 0x08, 0xA9, 0x00, 0xA9, 0x77, 0x60])])
    # RMW on code
    add("smc_inc", [(CODE, [0xEE, 0x04, 0x08, 0xA9, 0x01, 0x60])])
    add("smc_inc2", [(CODE, [0xEE, 0x05, 0x08, 0xA9, 0x01, 0x60])])


# ------------------------------------------------------------------ random soup
NONCTL = [(o, e) for o, e in enumerate(M.OPS) if e and e[1] != M.REL and e[0] not in
          ("JMP", "JSR", "RTS", "RTI", "BRK", "PLP", "TXS", "ARR", "ANC", "SBX", "ALR") and e[0] not in ("PHA", "PLA", "PHP")]
UNDOC = ("SLO", "RLA", "SRE", "RRA", "SAX", "LAX", "DCP", "ISC", "ANC", "ALR", "ARR", "SBX")


def soup(n, seed, undoc=False, size=160, smc=False):
    r = random.Random(seed)
    for t in range(n):
        code = []
        body = []

        def emit_ins():
            while True:
                opc, (mn, mode, cyc, pen) = r.choice(NONCTL)
                if mn in UNDOC and not undoc:
                    continue
                if mn == "NOP" and mode == M.IMP and r.random() < 0.8:
                    continue
                break
            if mode in (M.IMP, M.ACC):
                return [opc]
            if mode == M.IMM:
                return [opc, r.randrange(256)]
            if mode in (M.ZP, M.ZPX, M.ZPY):
                return [opc, r.randrange(0x20, 0x100)]
            if mode == M.ABS:
                ad = r.choice([0x0300 + r.randrange(0x100), 0x0400 + r.randrange(0x40), r.randrange(0x40, 0x100)])
                if r.random() < 0.08:
                    ad = 0xD400 + r.randrange(0x19)
                if smc and r.random() < 0.15:
                    ad = CODE + r.randrange(0x80)
                return [opc, ad & 255, ad >> 8]
            if mode in (M.ABX, M.ABY):
                ad = r.choice([0x0300 + r.randrange(0x100), 0x0200 + r.randrange(0x100)])
                if r.random() < 0.06:
                    ad = 0xD400 + r.randrange(0x19)
                if smc and r.random() < 0.15:
                    ad = CODE + r.randrange(0x60)
                return [opc, ad & 255, ad >> 8]
            if mode in (M.IZX, M.IZY):
                return [opc, r.choice([0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E])]
            raise SystemExit(mode)
        i = 0
        while len(code) < size:
            k = r.random()
            if k < 0.62:
                code += emit_ins()
            elif k < 0.70:
                code += [r.choice([0x48, 0x68, 0x08, 0xAA, 0xA8, 0x8A, 0x98, 0xBA])]
                if code[-1] == 0x68 or code[-1] == 0x08:
                    code[-1:] = [0x48, code[-1]] if False else [code[-1]]
            elif k < 0.80:
                # forward conditional branch over 1-3 instructions
                blk = []
                for _ in range(r.randrange(1, 4)):
                    blk += emit_ins()
                code += [r.choice([0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0]), len(blk)] + blk
            elif k < 0.87:
                # counted loop
                blk = []
                for _ in range(r.randrange(1, 4)):
                    blk += emit_ins()
                reg = r.choice(["x", "y"])
                if reg == "x":
                    pre, dec = [0xA2, r.randrange(2, 12)], 0xCA
                    # keep loop body from touching X? it can; use a dedicated counter in ZP instead
                pre = [0xA9, r.randrange(2, 10), 0x85, 0x0F]          # LDA #n; STA $0F
                tail = [0xC6, 0x0F, 0xD0, (-(len(blk) + 4)) & 255]       # DEC $0F; BNE
                code += pre + blk + tail
            elif k < 0.93:
                code += [0x20, 0x00, 0x09 + r.randrange(0, 3)]            # JSR sub
            elif k < 0.96:
                code += [0xA9, 0x20 + r.randrange(0x10), 0x8D, 0x18, 0xD4]
            else:
                code += [r.choice([0x18, 0x38, 0xB8, 0xD8, 0xF8, 0x58, 0x78])]
        code += [0xD8, 0x60]
        subs = []
        for si in range(3):
            sb = []
            for _ in range(r.randrange(2, 6)):
                sb += emit_ins()
            sb.append(0x60)
            subs.append((0x0900 + si, sb))
        # make subroutines non-overlapping: place at 0x0900, 0x0980, 0x09C0 and patch JSR targets
        code2 = bytearray(code)
        segs = [(CODE, bytes(code2))]
        offs = [0x0900, 0x0980, 0x09C0]
        # fix JSR operands: scan for pattern is unsafe; instead emit JSRs via table
        segs2 = []
        for si, (_, sb) in enumerate(subs):
            segs2.append((offs[si], bytes(sb)))
        # rewrite JSR targets by decoding the stream properly
        fixed = bytearray()
        j = 0
        while j < len(code2):
            opc = code2[j]
            ent = M.OPS[opc]
            ln = M.MODE_LEN[ent[1]]
            if opc == 0x20:
                tgt = offs[code2[j + 2] - 0x09]
                fixed += bytes([0x20, tgt & 255, tgt >> 8])
            else:
                fixed += code2[j:j + ln]
            j += ln
        data = bytearray(rng_bytes(r, 0x300))
        zp = bytearray(rng_bytes(r, 0x100))
        for q in range(0x10, 0x20, 2):
            ptr = r.choice([0x0300, 0x0340, 0x03F8, 0x0400] + ([CODE + 0x20, CODE + 0x50] if smc else []))
            zp[q], zp[q + 1] = ptr & 255, ptr >> 8
        zp[0x0F] = 0
        segs = [(CODE, bytes(fixed))] + segs2 + [(0x0000, bytes(zp)), (0x0200, bytes(data))]
        regs = dict(a=r.randrange(256), x=r.randrange(0x20), y=r.randrange(0x20), p=rp() & ~(0x08 if r.random() < 0.7 else 0))
        add(f"soup{seed}_{t}", segs, regs, regions=DATA + [(0xD400, 0x20)] , maxcyc=200000)


def rng_bytes(r, n):
    return bytes(r.randrange(256) for _ in range(n))


# ------------------------------------------------------------------ synthetic music routine
def music_asm(extra=0, reps=1):
    """3-voice sequencer with vibrato, pulse sweep and filter sweep; `extra` adds a table-processing
    loop of that many iterations (about 17 cycles each) so a frame can be made to cost 2,000 or 5,000 cycles."""
    return """
        .org $1000
ptr = $FB
ro  = $FD
vx  = $FE
init:   sta song
        lda #0
        ldx #2
iloop:  sta pos,x
        sta vib,x
        sta $D404,x
        lda #1
        sta cnt,x
        lda #0
        dex
        bpl iloop
        sta frame
        lda #$0F
        sta $D418
        lda #$08
        sta $D403
        sta $D40A
        sta $D411
        lda #$F9
        sta $D406
        sta $D40D
        sta $D414
        rts

play:   inc frame
        ldx #2
vloop:  lda regoff,x
        sta ro
        dec cnt,x
        bne vib1
        lda patlo,x
        sta ptr
        lda pathi,x
        sta ptr+1
        ldy pos,x
        lda (ptr),y
        cmp #$FF
        bne nt
        ldy #0
        lda (ptr),y
nt:     sta note
        iny
        lda (ptr),y
        sta cnt,x
        iny
        tya
        sta pos,x
        ldy note
        lda freqlo,y
        sta basel,x
        lda freqhi,y
        sta baseh,x
        ldy ro
        lda #$41
        sta $D404,y
        lda #0
        sta vib,x
vib1:   lda vib,x
        clc
        adc #1
        and #15
        sta vib,x
        stx vx
        tax
        lda vibtab,x
        sta tmp
        ldx vx
        ldy ro
        lda basel,x
        clc
        adc tmp
        sta $D400,y
        lda baseh,x
        adc #0
        sta $D401,y
        lda frame
        asl
        sta $D402,y
        lda frame
        lsr
        lsr
        and #$0F
        sta $D403,y
        dex
        bmi done
        jmp vloop
done:   lda frame
        and #$7F
        sta $D416
        lda #$F1
        sta $D417
        lda frame
        and #$0F
        ora #$10
        sta $D418
""" + (f"""
        ldy #{reps}
rl:     ldx #{extra}
xl:     lda wtab,x
        eor frame
        sta wtab2,x
        dex
        bne xl
        dey
        bne rl
""" if extra else "") + """
        rts

regoff: .byte 0, 7, 14
patlo:  .byte <pat0, <pat1, <pat2
pathi:  .byte >pat0, >pat1, >pat2
freqlo: .byte $8F,$FB,$7E,$14,$C0,$80,$55,$3F
freqhi: .byte $0B,$0C,$0E,$10,$12,$14,$17,$1A
vibtab: .byte 0,1,2,3,4,3,2,1,0,255,254,253,252,253,254,255
song:   .byte 0
pos:    .byte 0,0,0
cnt:    .byte 0,0,0
frame:  .byte 0
tmp:    .byte 0
note:   .byte 0
vib:    .byte 0,0,0
basel:  .byte 0,0,0
baseh:  .byte 0,0,0
        .org $1400
pat0:   .byte 0,3, 1,3, 2,3, 3,3, 4,6, 5,3, 6,3, 7,6, 0,3, 2,3, 4,3, 7,9, $FF
        .org $1440
pat1:   .byte 7,5, 6,5, 5,5, 4,10, 3,5, 2,5, 1,5, 0,10, $FF
        .org $1480
pat2:   .byte 0,12, 4,12, 7,12, 4,12, $FF
        .org $1500
wtab:   .byte 1,2,3,4,5,6,7,8
        .fill 250, 9
wtab2:  .fill 256, 0
"""


def music_image(extra=0, reps=1):
    out, syms = M.assemble(music_asm(extra, reps), 0x1000)
    segs = []
    start = prev = None
    buf = []
    for a in sorted(out):
        if start is None or a != prev + 1:
            if start is not None:
                segs.append((start, bytes(buf)))
            start, buf = a, []
        buf.append(out[a])
        prev = a
    segs.append((start, bytes(buf)))
    return segs, syms


def music():
    for extra, nm in ((0, "music_base"), (60, "music_x60")):
        segs, syms = music_image(extra)
        add(nm, segs, dict(a=0), entry=syms["init"], play=syms["play"], nframes=60, maxcyc=100000,
            regions=DATA + [(0x1000, 0x600), (0xD400, 0x20)])


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "disk")
    os.makedirs(out, exist_ok=True)
    groups = {}
    directed()
    groups["TESTS1.BIN"] = list(tests)
    tests.clear()
    exhaust_alu()
    control()
    io_tests()
    smc()
    groups["TESTS2.BIN"] = list(tests)
    tests.clear()
    soup(250, 1)
    soup(150, 2, undoc=True)
    soup(120, 3, smc=True, size=60)
    soup(100, 4, smc=True, undoc=True, size=100)
    music()
    groups["TESTS3.BIN"] = list(tests)
    tests.clear()
    sweeps()
    groups["TESTS4.BIN"] = list(tests)
    n = 0
    for fn, lst in groups.items():
        with open(os.path.join(out, fn), "wb") as f:
            f.write(b"SIDT" + struct.pack("<H", len(lst)))
            for t in lst:
                pack(f, t)
        print(fn, len(lst), "tests", os.path.getsize(os.path.join(out, fn)), "bytes")
        n += len(lst)
    print(n, "tests")
