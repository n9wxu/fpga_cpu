"""Compare the low tier's sound from the RTL (system_tb +i2s=FILE: a line
"left right" an output frame) with fsim's (--no-hicolor --wav FILE).

usage: i2scmp.py rtl.txt fsim.wav

The two start at different frames (the CPU runs at different speeds), so
the RTL's output is aligned on fsim's at the offset where the most frames
agree after both have become non-silent; then every frame they have in
common from there on must be equal.
"""
import struct
import sys


def read_rtl(path):
    out = []
    for line in open(path):
        l, r = line.split()
        out.append((int(l), int(r)))
    return out


def read_wav(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<I", d, 40)[0] // 4
    return [struct.unpack_from("<hh", d, 44 + 4 * i) for i in range(n)]


def first_sound(s):
    for i, v in enumerate(s):
        if v != (0, 0):
            return i
    return len(s)


def main(rtl_path, wav_path):
    a, b = read_rtl(rtl_path), read_wav(wav_path)
    fa, fb = first_sound(a), first_sound(b)
    if fa == len(a) or fb == len(b):
        sys.exit(f"i2scmp: silence (rtl {len(a)} frames, fsim {len(b)})")
    best = None
    for k in range(-64, 65):                     # around the first sounds
        off = fb - fa + k
        n = same = 0
        for i in range(fa, min(len(a), fa + 2000)):
            j = i + off
            if 0 <= j < len(b):
                n += 1
                same += a[i] == b[j]
        if n and (best is None or same > best[1]):
            best = (off, same, n)
    off = best[0]
    n = bad = 0
    first_bad = None
    for i in range(fa, len(a)):
        j = i + off
        if 0 <= j < len(b):
            n += 1
            if a[i] != b[j]:
                bad += 1
                if first_bad is None:
                    first_bad = (i, a[i], b[j])
    print(f"i2scmp: rtl {len(a)} frames, fsim {len(b)}; offset {off}; {n - bad} of {n} frames equal")
    if bad:
        i, x, y = first_bad
        print(f"  first difference at rtl frame {i}: rtl {x}, fsim {y}")
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
