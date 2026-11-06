#!/usr/bin/env python3
"""Writes fpga/rtl/mcaudio/mc_sine.mem: the microcoded audio engine's 256-entry 16-bit sine table."""
import math, os
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'rtl', 'mcaudio', 'mc_sine.mem')
with open(p, 'w') as f:
    for i in range(256):
        f.write('%04x\n' % (int(round(32767 * math.sin(2 * math.pi * i / 256))) & 0xFFFF))
