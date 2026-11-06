#!/usr/bin/env python3
"""Reads fcc's size line from stdin and checks that image + bss end well below the stack top (src/layout.h, 0x30000)
and the first fixed data region, leaving at least 8 KB for the stack."""
import re, sys
t = sys.stdin.read()
sys.stdout.write(t)
m = re.search(r"image (\d+) bytes @ 0x([0-9a-fA-F]+), bss (\d+) bytes", t)
if not m:
    sys.exit("sizecheck: no size line from fcc")
end = int(m.group(2), 16) + int(m.group(1)) + int(m.group(3))
print("image+bss end at 0x%X, %d bytes below the stack top 0x30000" % (end, 0x30000 - end))
if 0x30000 - end < 8192:
    sys.exit("sizecheck: image+bss too large: less than 8 KB for the stack below 0x30000")
