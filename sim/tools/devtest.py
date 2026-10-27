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
    ok &= spr8_test()
    sys.exit(0 if ok else 1)


def read_ppm(path):
    data = open(path, "rb").read()
    magic, w, h, mx, body = data.split(maxsplit=4)
    return int(w), body


def spr8_test():
    """demo/spr8.s: an orange pixel in buffer 1 at (10, 10), one in buffer 0 at (20, 20)."""
    src = os.path.join(SIM, "demo", "spr8.s")
    good = True
    for v in ("A", "B"):
        binp = os.path.join(BUILD, f"spr8.{v}.bin")
        ppm = os.path.join(BUILD, f"spr8.{v}.ppm")
        subprocess.run([sys.executable, os.path.join(HERE, "asm.py"), "-v", v, src, "-o", binp],
                       check=True, capture_output=True)
        subprocess.run([os.path.join(BUILD, "fsim"), "-q", "-v", v, "--true-color", "--screenshot", ppm, binp],
                       check=True, capture_output=True)
        w, px = read_ppm(ppm)
        at = lambda x, y: tuple(px[3 * (y * w + x):3 * (y * w + x) + 3])
        # output pixels are doubled; buffer 1 is shown, so buffer 0's pixel is not
        res = (at(20, 20), at(21, 21), at(40, 40))
        g = res == ((255, 166, 0), (255, 166, 0), (0, 0, 0))
        good &= g
        print(f"spr8       {v}  {'ok' if g else 'FAIL: ' + repr(res)}")
    return good


if __name__ == "__main__":
    main()
