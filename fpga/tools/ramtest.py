#!/usr/bin/env python3
"""Drive rtl/ramtest.v (the RAM machine under mock clients) over JTAG.

  ramtest.py status                  the counters since the last clear
  ramtest.py run SECONDS             clear, wait, report
  ramtest.py cap STEPS / we STEPS    set the capture / write-strobe phase (DCM steps)
  ramtest.py sweep cap|we FROM TO STEP [SECONDS]
                                     at each phase: clear, run, report errors
  ramtest.py ctrl HEX                write the control register (see ramtest.v)
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jtagload  # noqa: E402

BASE = 0x0FF000                         # register 0 (word 3FC00)
NAMES = ["video", "media", "cpu write", "cpu read", "prefetch"]


def regs():
    v = jtagload.read_words(BASE, 21)
    return v


def setreg(i, value, v=None):
    """Write register i (0-2). Registers 0-2 are written together: the
    loader writes a dummy word just below what it writes, which here must
    fall outside the registers."""
    v = v or regs()
    w = [v[0] & ~0x20, v[1] & 0xFFFF, v[2] & 0xFFFF]   # (clear is a one-shot: only when asked)
    w[i] = value & 0xFFFFFFFF
    jtagload.run_tcl(["init"] + jtagload.load(BASE, w) + ["shutdown"])


def signed16(x):
    return x - 0x10000 if x & 0x8000 else x


def report(v):
    clocks = v[4] or 1
    cap, we = v[1], v[2]
    print(f"ctrl {v[0]:08x}  capture phase {signed16(cap >> 16)} (target {signed16(cap & 0xFFFF)})"
          f"  strobe phase {signed16(we >> 16)} (target {signed16(we & 0xFFFF)})  status {v[3]:x}")
    total = 0
    for p in range(5):
        total += v[5 + p]
        print(f"  {NAMES[p]:10s} {v[5 + p]:10d} words  {v[10 + p]:6d} errors")
    print(f"  {clocks} clocks, RAM busy {100.0 * total / clocks:.1f}%"
          f" ({total * 4 / (clocks / 50e6) / 1e6:.0f} MB/s), video misses {v[15]}")
    print(f"  lost: {100.0 * v[19] / clocks:.1f}% to bus turnarounds, {100.0 * v[20] / clocks:.1f}% with nothing asked")
    if v[16]:
        print(f"  first error at word {v[16] & 0x3FFFF:05x}: got {v[17]:08x}, expected {v[18]:08x}")
    return sum(v[10:15])


def clear(ctrl):
    setreg(0, ctrl | 0x20)


def main():
    a = sys.argv[1:]
    if not a or a[0] == "status":
        report(regs())
    elif a[0] == "run":
        v = regs()
        clear(v[0])
        time.sleep(float(a[1]) if len(a) > 1 else 2.0)
        report(regs())
    elif a[0] in ("cap", "we"):
        setreg(1 if a[0] == "cap" else 2, int(a[1], 0))
        time.sleep(0.2)
        report(regs())
    elif a[0] == "ctrl":
        setreg(0, int(a[1], 16))
        report(regs())
    elif a[0] == "sweep":
        which, lo, hi, step = a[1], int(a[2], 0), int(a[3], 0), int(a[4], 0)
        secs = float(a[5]) if len(a) > 5 else 1.0
        reg = 1 if which == "cap" else 2
        ctrl = regs()[0]
        print(f"{which:>5s} {'errors':>10s} {'words':>12s} {'misses':>7s}")
        for ph in range(lo, hi + (1 if step > 0 else -1), step):
            setreg(reg, ph)
            time.sleep(0.1)
            clear(ctrl)
            time.sleep(secs)
            v = regs()
            print(f"{ph:5d} {sum(v[10:15]):10d} {sum(v[5:10]):12d} {v[15]:7d}"
                  f"   (dcm overflow {v[3] >> 2 & 3})", flush=True)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
