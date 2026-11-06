#!/usr/bin/env python3
"""stats.py out/stats.bin [t1_mips=6.5]: the STATS build's per-frame numbers and the estimated T1F load."""
import struct, sys
d = struct.unpack("<18I", open(sys.argv[1], "rb").read()[:72])
mips = float(sys.argv[2]) if len(sys.argv) > 2 else 6.5
n = d[0] or 1
print("frames measured: %d (after the look-ahead); 6502 cycles per call (budget): %d" % (d[0], d[5]))
print("native instructions per frame: avg %d, max %d (each part is a 16-bit counter: a part over 65535 wraps, so the max can be low)" % (d[3], d[4]))
print("  of which: generate (play call + model) avg %d max %d; sm_apply max %d; ui avg %d max %d" % (d[15] // n, d[12], d[13], d[14] // n, d[11]))
print("JIT: %d traces, %d 6502 instructions translated, %d SID events, %d assists, %d cache flushes" % (d[6], d[7], d[8], d[9], d[10]))
ms = d[3] / (mips * 1000.0)
print("estimated T1F load at %.1f MIPS: avg %.1f ms of a 20 ms frame = %.0f%%; worst frame %.1f ms = %.0f%%" % (mips, ms, ms / 0.2, d[4] / (mips * 1000.0), d[4] / (mips * 1000.0) / 0.2))
print("stack: %d bytes used at most (of %d between the BSS and the stack top)" % (d[16], d[17]))
