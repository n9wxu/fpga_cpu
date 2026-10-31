#!/usr/bin/env python3
"""Compare T1's instruction decoder with the ISA tables.

Generates first/second parcel pairs (every 16-bit first parcel, and for the
32-bit ones the legal second parcels of every form plus one-bit-flipped and
random ones), asks the ISA tables (sim/tools/isa.py, which reads
cpu/isa.yaml) whether each is a legal variant-A instruction, asks the RTL
(t1_dec_tb.v) whether it traps as illegal, and lists every disagreement.
EXT_X selects whether extension X counts as legal.

  t1dec.py [--ext-x 0|1] [--random N] [--show N]
"""
import argparse, os, random, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "sim", "tools"))
from isa import Isa  # noqa: E402

BUILD = os.path.join(ROOT, "verilog", "build")
EXT_OPS = ("MULPU", "FSHR", "FSHL", "CLZ", "DSTEP")


def legal_ref(isa, ext, ir, p2):
    f, vals = isa.decode("A", (ir << 16) | p2)
    if f is None:
        return False
    if f.op in EXT_OPS:
        return ext
    if f.op == "LEA" and f.mode in ("pi", "pd"):
        return False
    if f.op == "B" and vals.get("cc", 0) == 15:
        return False
    return True


def vectors(isa, n_random, rnd):
    v = set()
    for ir in range(0x10000):
        v.add((ir, 0))
    for f in [f for f in isa.variants["A"].forms if f.len == 32]:
        for _ in range(40):
            w = (f.value | (rnd.getrandbits(32) & ~f.mask & 0xFFFFFFFF)) & 0xFFFFFFFF
            v.add((w >> 16, w & 0xFFFF))
            for b in range(32):                      # one bit away from legal
                x = w ^ (1 << b)
                v.add((x >> 16, x & 0xFFFF))
    for _ in range(n_random):
        w = rnd.getrandbits(32)
        v.add((w >> 16, w & 0xFFFF))
    return sorted(v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ext-x", type=int, default=1)
    ap.add_argument("--random", type=int, default=100000)
    ap.add_argument("--show", type=int, default=40)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    os.makedirs(BUILD, exist_ok=True)
    vvp = os.path.join(BUILD, f"t1_dec_tb{a.ext_x}.vvp")
    srcs = [os.path.join(HERE, "t1_dec_tb.v"), os.path.join(ROOT, "verilog", "rtl", "t1_core.v"),
            os.path.join(ROOT, "verilog", "rtl", "t1_rf.v")]
    r = subprocess.run(["iverilog", "-g2001", f"-Pt1_dec_tb.EXT_X={a.ext_x}", "-o", vvp, *srcs],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    isa = Isa()
    rnd = random.Random(a.seed)
    vec = vectors(isa, a.random, rnd)
    with tempfile.NamedTemporaryFile("w", suffix=".vec", delete=False) as t:
        for ir, p2 in vec:
            t.write(f"{ir:04x} {p2:04x}\n")
    out = subprocess.run(["vvp", "-n", vvp, f"+vec={t.name}"], capture_output=True, text=True).stdout
    os.unlink(t.name)
    bad = {}
    seen = 0
    for line in out.splitlines():
        p = line.split()
        if len(p) != 3:
            continue
        ir, p2, ill = int(p[0], 16), int(p[1], 16), p[2] == "1"
        if ir == 0xA2FE:
            continue            # SYS 0xFE halts with the same code as an illegal instruction
        if not isa.variants["A"].is_long(ir) and p2 != 0:
            continue            # a 16-bit instruction does not look at the second parcel
        seen += 1
        if legal_ref(isa, bool(a.ext_x), ir, p2) == ill:
            f, _ = isa.decode("A", (ir << 16) | p2)
            key = (f.name() if f else "none", "T1 traps a legal one" if ill else "T1 accepts an illegal one")
            bad.setdefault(key, []).append((ir, p2))
    print(f"{seen} encodings checked, {sum(len(x) for x in bad.values())} disagreements")
    for (name, what), lst in sorted(bad.items())[:a.show]:
        ex = ", ".join(f"{i:04x} {p:04x}" for i, p in lst[:3])
        print(f"  {name:28s} {what}: {len(lst)}  e.g. {ex}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
