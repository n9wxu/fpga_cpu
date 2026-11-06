#!/usr/bin/env python3
"""bin2c.py in.bin out.h name: a byte array for #include"""
import sys
d = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "w") as f:
    f.write("static const unsigned char %s[%d] = {\n" % (sys.argv[3], len(d)))
    for i in range(0, len(d), 16):
        f.write(",".join(str(b) for b in d[i:i + 16]) + ",\n")
    f.write("};\n")
