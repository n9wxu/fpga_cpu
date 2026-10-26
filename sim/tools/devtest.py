"""Run the headless device test (sim/demo/devtest.s) on both variants.

Checks the keyboard queue, the file service and board I/O against the
'; expect:' line in the source.
"""
import ast
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
SRC = os.path.join(SIM, "demo", "devtest.s")
BUILD = os.path.join(SIM, "build")


def main():
    want = ast.literal_eval(re.search(r"^; expect: (.*)$", open(SRC).read(), re.M).group(1))
    ok = True
    for v in ("A", "B"):
        binp = os.path.join(BUILD, f"devtest.{v}.bin")
        disk = os.path.join(BUILD, f"disktest.{v}")
        subprocess.run([sys.executable, os.path.join(HERE, "asm.py"), "-v", v, SRC, "-o", binp],
                       check=True, capture_output=True)
        shutil.rmtree(disk, ignore_errors=True)
        r = subprocess.run([os.path.join(BUILD, "fsim"), "-q", "-v", v, "--keys", "Hi", "--disk", disk, binp],
                           capture_output=True, text=True)
        good = r.returncode == 0 and r.stdout == want
        ok &= good
        print(f"devtest    {v}  {'ok' if good else 'FAIL: ' + repr(r.stdout) + ' ' + r.stderr.strip()}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
