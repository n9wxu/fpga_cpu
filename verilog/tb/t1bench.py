#!/usr/bin/env python3
"""Cycles and instructions of program images on T1 (no lockstep), with the
console output checked against fsim.

  t1bench.py [--ext-x 0|1] [-j N] IMAGE.bin ...     (prints a markdown table)

Images are variant-A code at 0x1000, e.g. benchmarks/build/NAME/NAME.int16.A.bin
(or the .A.ext.bin build, which uses extension X; run it with --ext-x 1).
"""
import argparse, concurrent.futures, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import t1run  # noqa: E402


def one(a):
    img, ext_x = a
    res = t1run.run(img, False, 4_000_000_000, ext_x)
    want, rc = t1run.fsim_run(img, 20_000_000_000, ext_x)
    ok = res["output"] == want and t1run.expected_rc(res) == rc
    return img, res, ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("images", nargs="+")
    ap.add_argument("--ext-x", type=int, default=1)
    ap.add_argument("-j", type=int, default=4)
    a = ap.parse_args()
    t1run.build(a.ext_x)
    print("| image | cycles | instructions | cycles/inst | output |")
    print("|---|---|---|---|---|")
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for img, res, ok in ex.map(one, [(i, a.ext_x) for i in a.images]):
            c, n = res.get("cycles", 0), res.get("insts", 0)
            print(f"| {os.path.basename(img)} | {c:,} | {n:,} | {c / n if n else 0:.2f} | {'same as fsim' if ok else 'DIFFERENT'} |")
            sys.stdout.flush()


if __name__ == "__main__":
    main()
