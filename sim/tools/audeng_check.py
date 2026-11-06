#!/usr/bin/env python3
"""fsim's audio engine (sim/src/audeng.c) against the reference model (fpga/tools/audio_ref.py),
frame for frame, on the register scripts of fpga/tools/audio_tests.py (which checks the model
against the RTL).

usage: audeng_check.py [OUTDIR] [test ...]      tests: mod chip stream legacy stress
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
ROOT = os.path.dirname(SIM)
sys.path.insert(0, os.path.join(ROOT, "fpga", "tools"))
import audio_ref as ref  # noqa: E402
import audio_tests  # noqa: E402


def main():
    args = sys.argv[1:]
    outdir = args.pop(0) if args and args[0] not in audio_tests.TESTS else tempfile.mkdtemp(prefix="audeng_")
    os.makedirs(outdir, exist_ok=True)
    exe = os.path.join(outdir, "audeng_check")
    subprocess.check_call(["cc", "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter",
                           "-I", os.path.join(SIM, "src"), "-o", exe,
                           os.path.join(HERE, "audeng_check.c"), os.path.join(SIM, "src", "audeng.c")])
    ok = True
    for name in args or list(audio_tests.TESTS):
        sp, mp = audio_tests.TESTS[name]().write(name, outdir)
        want = ref.render(sp, mp)
        out = os.path.join(outdir, name + ".fsim.txt")
        subprocess.check_call([exe, sp, mp, out])
        got = [tuple(int(x) for x in l.split()) for l in open(out)]
        bad = [i for i in range(min(len(got), len(want))) if got[i] != want[i]]
        good = len(got) == len(want) and not bad
        ok &= good
        print("%-7s %s  frames %d/%d  mismatches %d" % (name, "PASS" if good else "FAIL", len(got), len(want), len(bad)))
        if bad:
            print("   first at frame %d: fsim %s model %s" % (bad[0], got[bad[0]], want[bad[0]]))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
