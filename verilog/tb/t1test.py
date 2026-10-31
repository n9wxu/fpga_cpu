#!/usr/bin/env python3
"""Run the directed T1 tests (tests/*.s) against the C simulator.

Each test is assembled (sim/tools/asm.py, variant A), then run on the RTL in
lockstep with fsim: registers and flags must agree after every instruction,
the console output must match, and the run must end the same way (EXIT code,
illegal instruction, address fault).  Header comments in a test select:

  ; entries: 0x1000 0x1100   start at each address in turn (default: one run
                              from the load address 0x1000)
  ; ext: yes                 uses extension X; also runs without it
                              (EXT_X=0 core, fsim without --ext), where it
                              must trap as illegal in both

  t1test.py [-k SUBSTRING] [--keep] [--ext-x 0]
"""
import argparse, glob, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import t1run  # noqa: E402

ASM = os.path.join(ROOT, "sim", "tools", "asm.py")
PY = sys.executable
OUT = os.path.join(ROOT, "verilog", "build", "tests")


def assemble(src):
    os.makedirs(OUT, exist_ok=True)
    img = os.path.join(OUT, os.path.basename(src)[:-2] + ".bin")
    r = subprocess.run([PY, ASM, "-q", "-v", "A", src, "-o", img], capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"{src}: assembler failed\n{r.stdout}{r.stderr}")
    return img


def header(src):
    ents, ext = [None], False
    for line in open(src):
        m = re.match(r";\s*entries:\s*(.*)", line)
        if m:
            ents = [int(x, 0) for x in m.group(1).split()]
        if re.match(r";\s*ext:\s*yes", line):
            ext = True
    return ents, ext


def check(img, entry, ext_x, lockstep=True):
    res = t1run.run(img, lockstep, 1_000_000, ext_x, entry)
    want, rc = t1run.fsim_run(img, 20_000_000, ext_x, entry)
    got = t1run.expected_rc(res)
    ok = res["output"] == want and got == rc and "MISMATCH" not in res["output"] and "during reset" not in res["output"]
    what = f"T1 {res['result']} | fsim exit {rc}"
    if "MISMATCH" in res["output"]:
        what += "\n" + res["output"].strip()
    return ok, what


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-k", default="")
    a = ap.parse_args()
    fails = n = 0
    for src in sorted(glob.glob(os.path.join(HERE, "tests", "*.s"))):
        name = os.path.basename(src)
        if name == "common.s" or a.k not in name:
            continue
        img = assemble(src)
        ents, ext = header(src)
        runs = [(e, 1) for e in ents] + ([(e, 0) for e in ents] if ext else [])
        for entry, ext_x in runs:
            ok, what = check(img, entry, ext_x)
            n += 1
            tag = f"{name}" + (f" @{entry:#x}" if entry is not None else "") + ("" if ext_x else " (no EXT_X)")
            print(f"{'ok  ' if ok else 'FAIL'} {tag}: {what.splitlines()[0]}")
            if not ok:
                fails += 1
                print("\n".join(what.splitlines()[1:]))
    print(f"{n - fails}/{n} runs passed")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
