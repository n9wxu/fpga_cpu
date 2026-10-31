"""Regression tests for simulator/assembler defects found in review.

Each test assembles a tiny program, runs build/fsim and checks memory
(--dump), the exit code or the stats. Run by `make test`.
"""
import json
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
sys.path.insert(0, HERE)
FSIM = os.path.join(SIM, "build", "fsim")
ASM = os.path.join(HERE, "asm.py")
results = []


def check(name, ok, detail=""):
    results.append(ok)
    print(f"{name:<38}{'ok' if ok else 'FAIL ' + str(detail)}")


def assemble(tmp, src, v, base=0x1000):
    s = os.path.join(tmp, "t.s")
    b = os.path.join(tmp, f"t.{v}.bin")
    open(s, "w").write(src)
    r = subprocess.run([sys.executable, ASM, "-v", v, "--base", hex(base), s, "-o", b], capture_output=True, text=True)
    return (b if r.returncode == 0 else None), r.stdout + r.stderr


def run(tmp, binp, v, *args, dump=None):
    out = os.path.join(tmp, "dump")
    if os.path.exists(out):
        os.remove(out)
    cmd = [FSIM, "-q", "-v", v, *args]
    if dump:
        cmd += ["--dump", f"{dump[0]:#x}:{dump[1]}:{out}"]
    r = subprocess.run(cmd + [binp], capture_output=True, text=True)
    mem = open(out, "rb").read() if dump and os.path.exists(out) else b""
    return r, mem


def prog(tmp, src, v, *args, dump=None):
    b, msg = assemble(tmp, src, v)
    if b is None:
        return None, b"", msg
    r, mem = run(tmp, b, v, *args, dump=dump)
    return r, mem, ""


STORE_ORDER = """
        .org 0x1000
        LA      p2, 0x3000
        STW     r4, [p2]+           ; r4 is half of p2: stores the OLD r4
        LA      p2, 0x3010
        STB     r4, [p2]+
        LA      p3, 0x3020
        STP     p3, [p3]+           ; old p3
        LA      p3, 0x3030
        STP     p3, -[p3]           ; old p3, at 0x302c
        MOVI    r2, 0x55AA
        STW     r2, [buf]           ; direct address (expanded through p0 in variant A)
        STB     r2, [buf + 4]
        SYS     0
        .org 0x3100
buf:    .word 0, 0, 0
"""


def test_store_order(tmp, v):
    r, mem, msg = prog(tmp, STORE_ORDER, v, dump=(0x3000, 0x110))
    if r is None:
        return check(f"store data before write-back {v}", False, msg)
    w = lambda a: struct.unpack_from("<H", mem, a - 0x3000)[0]
    got = (w(0x3000), mem[0x10], w(0x3020), w(0x3022), w(0x302C), w(0x302E), w(0x3100), mem[0x104])
    want = (0x3000, 0x10, 0x3020, 0, 0x3030, 0, 0x55AA, 0xAA)
    check(f"store data before write-back {v}", got == want, f"{got} != {want}")


def test_asm_scratch(tmp, v):
    # (source, assembles in variant B because it has a direct form?)
    for src, direct_b in (("stw r1, [buf]", True), ("stb r0, [buf + 4]", False), ("stp p0, [buf]", False)):
        b, msg = assemble(tmp, f"        .org 0x1000\n        {src}\n        SYS 0\nbuf:    .word 0, 0\n", v)
        ok = (b is not None) if (v == "B" and direct_b) else (b is None and "scratch" in msg)
        check(f"asm {src.split(',')[0]},[abs] {v}", ok, msg.strip().splitlines()[-1:] if msg else "assembled")


SHIFT_V = """
        .org 0x1000
        MOVI    r2, 0x7FFF
        ADDI    r2, r2, 1           ; signed overflow: V=1
        MFSR    r4, flags
        SHLI    r3, r2, 1
        MFSR    r5, flags
        MOVI    r2, 0x7FFF
        ADDI    r2, r2, 1
        SARI    r3, r2, 1
        MFSR    r6, flags
        LA      p4, 0x3000
        STW     r4, [p4]+
        STW     r5, [p4]+
        STW     r6, [p4]+
        SYS     0
"""


def test_shift_v(tmp, v):
    r, mem, msg = prog(tmp, SHIFT_V, v, dump=(0x3000, 6))
    f = struct.unpack("<3H", mem) if r else None
    check(f"shifts clear V {v}", f is not None and f[0] & 1 and not f[1] & 1 and not f[2] & 1, f)


LOADS = """
        .org 0x1000
        LA      p2, {addr}
        {body}
        SYS     0
"""


def test_nocache_data_port(tmp, v):
    """Loads from SRAM and from the boot ROM are the same instructions with
    the same fetches; without an I-cache the SRAM ones also need the port."""
    n = 64
    cyc = {}
    for kind, addr in (("sram", "0x3000"), ("rom", "0xFF1000")):
        b, msg = assemble(tmp, LOADS.format(addr=addr, body="LDW r3, [p2 + 0]\n        " * n), v)
        for ic in (0, 2048):
            j = os.path.join(tmp, "j.json")
            subprocess.run([FSIM, "-q", "-v", v, "--icache", str(ic), "--video", "0", "--json", j, b], capture_output=True)
            cyc[kind, ic] = json.load(open(j))["cycles"]
    check(f"no-I-cache loads share port {v}",
          cyc["sram", 0] - cyc["rom", 0] == n, cyc)


FS_CLAMP = """
        .org 0x1000
IO      = 0xF00000
FS      = 0x500
        LA      p5, IO
        LA      p1, fname
        STP     p1, [p5 + FS + 8]
        MOVI    r2, 1
        STW     r2, [p5 + FS + 6]
        STW     r2, [p5 + FS]           ; OPEN for write
        LDW     r2, [p5 + FS + 0x12]
        STW     r2, [p5 + FS + 4]
        LA      p1, 0x10000
        STP     p1, [p5 + FS + 0x0C]
        MOVI    r2, 0xFFFF
        STW     r2, [p5 + FS + 0x10]
        MOVI    r2, 4
        STW     r2, [p5 + FS]           ; WRITE 65535
        LDW     r2, [p5 + FS + 0x12]
        LA      p4, 0x3000
        STW     r2, [p4]
        SYS     0
fname:  .asciz "BIG.BIN"
"""


def test_fs_clamp(tmp, v):
    disk = os.path.join(tmp, "disk")
    b, msg = assemble(tmp, FS_CLAMP, v)
    if b is None:
        return check(f"FS transfer clamped to 32767 {v}", False, msg)
    r, mem = run(tmp, b, v, "--disk", disk, dump=(0x3000, 2))
    n = struct.unpack("<h", mem)[0] if mem else None
    f = os.path.join(disk, "BIG.BIN")
    size = os.path.getsize(f) if os.path.exists(f) else None
    check(f"FS transfer clamped to 32767 {v}", n == 32767 and size == 32767, (n, size, r.stderr[-100:]))


def test_options(tmp):
    b, _ = assemble(tmp, ".org 0x1000\n SYS 0\n", "A")
    cases = ((["--video", "56:4"], True), (["--video", "0"], True), (["--video", "4:56"], False),
             (["--video", "56:56"], False), (["--video", "10:x"], False), (["--branch-pen", "-1"], False),
             (["--branch-pen", "2"], True), (["--icache", "abc"], False), (["--load", "0x1000zz"], False))
    for args, good in cases:
        try:
            r = subprocess.run([FSIM, "-q", *args, b], capture_output=True, text=True, timeout=10)
            ok = (r.returncode == 128) if good else (r.returncode not in (0, 128))
            d = r.returncode
        except subprocess.TimeoutExpired:
            ok, d = False, "hang"
        check(f"option {' '.join(args)}", ok, d)


def test_last_parcel(tmp, v):
    top = 0x100000 - 2
    b, msg = assemble(tmp, f"        .org {top}\n        SYS 7\n", v, base=top)
    if b is None:
        return check(f"16-bit inst at end of RAM {v}", False, msg)
    r = subprocess.run([FSIM, "-q", "-v", v, "--load", hex(top), b], capture_output=True, text=True)
    check(f"16-bit inst at end of RAM {v}", r.returncode == 128 + 7, (r.returncode, r.stderr))


def test_odd_pair(tmp, v):
    import isa as isamod
    isa = isamod.Isa()
    b, msg = assemble(tmp, "        .org 0x1000\n        ADDPI p1, p2, 1000\n        SYS 1\n", v)
    data = bytearray(open(b, "rb").read())
    form = [f for f in isa.variants[v].forms if f.op == "ADDPI" and f.len == 32][0]
    fld = [f for f in form.fields if f.spec.div > 1][0]
    bit = fld.positions[-1]                      # LSB of the pair field, in the 32-bit word
    word = (data[1] << 8 | data[0]) << 16 | (data[3] << 8 | data[2])
    r1 = subprocess.run([FSIM, "-q", "-v", v, b], capture_output=True, text=True)
    word ^= 1 << bit
    data[0:4] = bytes([(word >> 16) & 0xFF, (word >> 24) & 0xFF, word & 0xFF, (word >> 8) & 0xFF])
    bad = os.path.join(tmp, "odd.bin")
    open(bad, "wb").write(data)
    r2 = subprocess.run([FSIM, "-q", "-v", v, bad], capture_output=True, text=True)
    check(f"odd pair number is illegal {v}", r1.returncode == 129 and r2.returncode == 128 + 126,
          (r1.returncode, r2.returncode))


def test_fault_not_logged(tmp, v):
    src = ".org 0x1000\n LA p1, 0x800000\n MOVI r2, 0x1234\n STB r2, [p1]\n SYS 0\n"
    b, _ = assemble(tmp, src, v)
    log, js = os.path.join(tmp, "l.log"), os.path.join(tmp, "l.json")
    r = subprocess.run([FSIM, "-q", "-v", v, "--log", log, "--json", js, b], capture_output=True, text=True)
    insts = json.load(open(js))["insts"]
    recs = os.path.getsize(log) // 8
    check(f"faulting store not logged {v}", r.returncode == 3 and recs == insts, (r.returncode, recs, insts))


def test_memlog(tmp):
    def rec(w0, w1):
        return struct.pack("<II", w0, w1)
    inst = lambda cyc: rec(0x1000 | 2 << 24, cyc & 0xFFFFFFFF)
    marker = rec(2 << 28 | 2 << 24 | 0x4000, 1)
    log = os.path.join(tmp, "m.log")
    sym = os.path.join(tmp, "m.sym")
    open(sym, "w").write("004000 frame_done\n")
    # cycles wrap in the log; the marker write belongs to the second instruction
    data = inst(0xFFFFFFF0) + inst(0xFFFFFFFA) + marker + inst(0xFFFFFFFE) + inst(4 + (1 << 32)) + inst(10) + b"\x01\x02\x03"
    open(log, "wb").write(data)
    r = subprocess.run([sys.executable, os.path.join(HERE, "memlog.py"), log, "--sym", sym, "--marker", "frame_done"],
                       capture_output=True, text=True)
    rows = [ln.split() for ln in r.stdout.splitlines()[1:]]
    # frame 0 (first two insts): 0xFFFFFFF0 -> 0xFFFFFFFE = 14 cycles, the last one included;
    # exit: 0xFFFFFFFE -> 4 (6) and 4 -> 10 (6), the final instruction has no successor (0)
    ok = r.returncode == 0 and rows[0][:3] == ["0", "14", "2"] and rows[1][1] == "12" and "partial" in r.stderr
    check("memlog partial record, wrap, last inst", ok, (r.stdout, r.stderr))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        for v in ("A", "B"):
            test_store_order(tmp, v)
            test_asm_scratch(tmp, v)
            test_shift_v(tmp, v)
            test_nocache_data_port(tmp, v)
            test_fs_clamp(tmp, v)
            test_last_parcel(tmp, v)
            test_odd_pair(tmp, v)
            test_fault_not_logged(tmp, v)
        test_options(tmp)
        test_memlog(tmp)
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
