#!/usr/bin/env python3
"""Render an engine register script (W frame addr val / E frames) with the bit-exact model
fpga/tools/audio_ref.py: audiorender.py script.txt out.wav [seconds]"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "fpga", "tools"))
import audio_ref as ref

def render(script, maxsec=None):
    ev, n = ref.load_script(script)
    if maxsec: n = min(n, int(maxsec * 48000))
    ev.sort(key=lambda e: e[0])
    eng = ref.Engine([0] * 16)
    out, i = [], 0
    for f in range(n):
        while i < len(ev) and ev[i][0] <= f:
            eng.write(ev[i][1], ev[i][2]); i += 1
        out.append(tuple(eng.frame()))
    return out

if __name__ == "__main__":
    out = render(sys.argv[1], float(sys.argv[3]) if len(sys.argv) > 3 else None)
    ref.write_wav(sys.argv[2], out)
    pk = max(max(abs(l), abs(r)) for l, r in out)
    print(sys.argv[2], len(out) / 48000.0, "s, peak", pk)
