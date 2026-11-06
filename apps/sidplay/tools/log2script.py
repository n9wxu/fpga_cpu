#!/usr/bin/env python3
"""Engine register log (fsim --dump of the AUDLOG buffer) -> script for audio_ref.py:
log2script.py dump.bin out.script [cycles_per_call=19656] [clock=985248]
dump.bin is the fsim dump of 0x035000.. (statistics at +0, the engine log at +0x3000)."""
import struct, sys
d = open(sys.argv[1], "rb").read()[0x3000:]
cpc = int(sys.argv[3]) if len(sys.argv) > 3 else 19656
clk = int(sys.argv[4]) if len(sys.argv) > 4 else 985248
n = struct.unpack_from("<I", d, 0)[0]
last = 0
with open(sys.argv[2], "w") as f:
    for i in range(n):
        lo, hi, addr, val = struct.unpack_from("<HHHH", d, 16 + 8 * i)
        tick = lo | hi << 16
        last = max(last, tick)
        f.write("W %d %x %x\n" % (tick * 48000 * cpc // clk, addr, val))
    f.write("E %d\n" % ((last + 1) * 48000 * cpc // clk))
print(n, "writes,", last + 1, "ticks")
