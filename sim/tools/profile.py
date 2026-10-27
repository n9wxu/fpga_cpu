#!/usr/bin/env python3
"""Flat profile by function from fsim --profile output and a --sym file.

usage: profile.py prog.sym prog.prof [-n 25] [--by insts|cycles]

Each address is charged to the nearest symbol at or below it (assembler
locals such as name$L3 and .L labels are skipped)."""
import argparse
import bisect
import collections


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("sym")
    ap.add_argument("prof")
    ap.add_argument("-n", type=int, default=25)
    ap.add_argument("--by", choices=("cycles", "insts"), default="cycles")
    a = ap.parse_args()
    syms = []
    for line in open(a.sym):
        f = line.split()
        if len(f) == 2 and "$L" not in f[1] and not f[1].startswith("."):
            syms.append((int(f[0], 16), f[1]))
    syms.sort()
    addrs = [s[0] for s in syms]
    ins, cyc = collections.Counter(), collections.Counter()
    for line in open(a.prof):
        if line.startswith("#"):
            continue
        pc, n, c = line.split()
        i = bisect.bisect_right(addrs, int(pc, 16)) - 1
        name = syms[i][1] if i >= 0 else "?"
        ins[name] += int(n)
        cyc[name] += int(c)
    ti, tc = sum(ins.values()), sum(cyc.values())
    key = cyc if a.by == "cycles" else ins
    print(f"{'function':28} {'cycles':>14} {'%':>6} {'insts':>14} {'CPI':>6}")
    for name, _ in key.most_common(a.n):
        print(f"{name:28} {cyc[name]:14,} {100 * cyc[name] / tc:5.1f}% {ins[name]:14,} "
              f"{cyc[name] / max(ins[name], 1):6.2f}")
    print(f"{'total':28} {tc:14,} {'':6} {ti:14,} {tc / max(ti, 1):6.2f}")


if __name__ == "__main__":
    main()
