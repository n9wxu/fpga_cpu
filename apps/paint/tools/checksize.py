"""Check that paint's program, data and BSS leave its stack room below
the scratch area (src/paint.h): usage: checksize.py paint.sym STACK"""
import sys

syms = {}
for line in open(sys.argv[1]):
    parts = line.split()
    if len(parts) == 2:
        syms[parts[1]] = int(parts[0], 16)
end = syms.get("__bss_end") or max(syms.values())
stack = int(sys.argv[2], 0)
room = stack - end
print(f"paint: program and data end at 0x{end:06x}, {room} bytes for the stack")
if room < 4096:
    sys.exit("paint: less than 4 KB for the stack")
