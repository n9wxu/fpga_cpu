#!/usr/bin/env python3
"""mkimage.py sidplay.bin out.bin [tune.sid ...]: one flat image for fsim (--load 0x1000): the program at 0x1000
and the tunes in the SDRAM slots (0x080000 + n x 0x10000, file length in the slot's last 4 bytes), as
tang/tools/run.py --tune puts them on the board."""
import struct, sys
prog = open(sys.argv[1], "rb").read()
img = bytearray(0xC0000 - 0x1000)
img[:len(prog)] = prog
for i, fn in enumerate(sys.argv[3:]):
    d = open(fn, "rb").read()
    base = 0x80000 + i * 0x10000 - 0x1000
    img[base:base + len(d)] = d
    img[base + 0xFFFC:base + 0x10000] = struct.pack("<I", len(d))
open(sys.argv[2], "wb").write(img)
