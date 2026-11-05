#!/usr/bin/env python3
"""pack.py: place MOD files in SDRAM as modplay finds them: from 0x040000, each at the next 256-byte boundary
after the last, then 2 KB of zeros (no signature: the end of the list).

    pack.py args a.mod b.mod ...            prints tang/tools/run.py arguments (--data ADDR:FILE ... --zero ...)
    pack.py image prog.bin out.bin a.mod .. a flat image for fsim (the program at 0x1000)
"""
import sys

BASE, LIMIT = 0x040000, 0x0C3C00


def mod_size(d):
    sig = d[1080:1084]
    nch = 4 if sig in (b"M.K.", b"M!K!", b"FLT4", b"4CHN") else 6 if sig == b"6CHN" else 8 if sig == b"8CHN" else 0
    if not nch:
        raise SystemExit("not a 4, 6 or 8-channel MOD (signature %r)" % sig)
    npat = max(d[952:1080]) + 1
    return 1084 + npat * 256 * nch + sum(int.from_bytes(d[20 + 30 * i + 22:20 + 30 * i + 24], "big") * 2 for i in range(31))


def place(files):
    a, out = BASE, []
    for fn in files:
        d = open(fn, "rb").read()
        n = mod_size(d)
        if a + n > LIMIT:
            raise SystemExit("%s does not fit (0x%06X + %d > 0x%06X)" % (fn, a, n, LIMIT))
        out.append((a, fn, d[:n]))
        a = (a + n + 255) & ~255
    return out, a


if sys.argv[1] == "args":
    mods, end = place(sys.argv[2:])
    print(" ".join("--data 0x%06X:%s" % (a, fn) for a, fn, _ in mods) + " --zero 0x%06X:2048" % end)
else:
    mods, end = place(sys.argv[4:])
    prog = open(sys.argv[2], "rb").read()
    img = bytearray(end + 2048 - 0x1000)
    img[:len(prog)] = prog
    for a, _, d in mods:
        img[a - 0x1000:a - 0x1000 + len(d)] = d
    open(sys.argv[3], "wb").write(img)
