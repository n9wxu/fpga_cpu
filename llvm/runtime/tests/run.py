"""Compiler tests: build each test for the host and for fpga_cpu (both
encoding variants), run them, and compare the output. Lines starting with
'~' hold approximate values and are compared with a relative tolerance.

usage: run.py [-v] [test.c ...]
"""
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LLVM = os.path.dirname(os.path.dirname(HERE))
ROOT = os.path.dirname(LLVM)
BUILD = os.path.join(HERE, "build")
FCC = os.path.join(LLVM, "fcc")
FSIM = os.path.join(ROOT, "sim", "build", "fsim")
TOL = 2e-5


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def close(a, b):
    ta, tb = a.split(), b.split()
    if len(ta) != len(tb) or ta[:2] != tb[:2]:
        return False
    x, y = float(ta[2]), float(tb[2])
    return abs(x - y) <= TOL * max(abs(x), abs(y), 1e-30) or abs(x - y) < 1e-6


def compare(want, got):
    wl, gl = want.splitlines(), got.splitlines()
    if len(wl) != len(gl):
        return [f"line count {len(gl)} != {len(wl)}"]
    bad = []
    for w, g in zip(wl, gl):
        if w == g:
            continue
        if w.startswith("~") and close(w, g):
            continue
        bad.append(f"want {w!r}\n           got  {g!r}")
    return bad


def main(argv):
    verbose = "-v" in argv
    tests = [a for a in argv if a.endswith(".c")] or sorted(glob.glob(os.path.join(HERE, "*.c")))
    os.makedirs(BUILD, exist_ok=True)
    fails = 0
    for t in tests:
        name = os.path.splitext(os.path.basename(t))[0]
        host = os.path.join(BUILD, name + ".host")
        r = sh(["cc", "-O1", "-funsigned-char", "-w", "-o", host, t, "-lm"])
        if r.returncode:
            print(f"{name:10} host build failed\n{r.stderr}")
            fails += 1
            continue
        want = sh([host]).stdout
        # both encodings, and the 32-bit int build (variant A) with its own runtime
        for v, int32 in (("A", False), ("B", False), ("A", True)):
            tag = v + ("/int32" if int32 else "")
            binp = os.path.join(BUILD, f"{name}.{v}{'.int32' if int32 else ''}.bin")
            r = sh([FCC, "-q", "-v", v, t, "-o", binp, "--stats"] + (["--int32"] if int32 else []))
            if r.returncode:
                print(f"{name:10} {tag} compile failed\n{r.stdout}{r.stderr}")
                fails += 1
                continue
            r = sh([FSIM, "-q", "-v", v, "--max-cycles", "200000000", "--disk", os.path.join(BUILD, "disk"), binp])
            bad = compare(want, r.stdout)
            if r.returncode:
                bad.insert(0, f"exit {r.returncode} {r.stderr.strip()}")
            status = "ok" if not bad else "FAIL"
            fails += bool(bad)
            print(f"{name:10} {tag:8} {status}")
            for b in bad[: (50 if verbose else 6)]:
                print("    " + b)
    print("all tests passed" if not fails else f"{fails} failure(s)")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main(sys.argv[1:])
