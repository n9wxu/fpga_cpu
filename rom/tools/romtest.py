#!/usr/bin/env python3
"""The ROM in fsim (make test): boots rom.img with --rom, types into the console and checks what
comes back. Each run ends where a program exits (fsim stops at EXIT) or the input ends.
usage: romtest.py FSIM ROM.img
"""
import subprocess
import sys

RUNS = [
    ("boot and shell", "\ninfo\ndir\nhelp\npoke $2000 1 2 3\ndump $2000 3\nnope\n",
     ["ROM copy CRC ... ok", "RAM ... ok", "flash ... ok  0B4017, 2 file(s)", "Shell:",
      "ROM 1.0, board 1", "BASIC ", "ROMTEST ", "DUMP addr", "002000: 01 02 03", "what?"]),
    ("romtest through the slots", "\nromtest\n", ["romtest: 0 failed"]),
    ("BASIC from the ROM disk", "\nbasic\nPRINT 6*7\n", ["FPGA BASIC", " 42 "]),
]


def main():
    fsim, img = sys.argv[1], sys.argv[2]
    bad = 0
    for name, keys, want in RUNS:
        p = subprocess.run([fsim, "-q", "--xram", "7", "--rom", img], input=keys.encode(),
                           capture_output=True, timeout=300)
        out = p.stdout.decode(errors="replace")
        miss = [w for w in want if w not in out]
        print("%-28s %s" % (name, "ok" if not miss else "FAILED: missing %r" % miss))
        if miss:
            bad += 1
            sys.stdout.write(out[-2000:])
    sys.exit(bad)


if __name__ == "__main__":
    main()
