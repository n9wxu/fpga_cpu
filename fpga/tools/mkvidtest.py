"""Add a test picture to a program image for sim/progs/vidtest.c: a bitmap
at 0C3C00 (hi-color bytes, or 8-color nibbles with --nibbles) and an 80x25
text screen at 0FC000 using every attribute.

usage: mkvidtest.py prog.bin out.bin [--nibbles]
"""
import sys

BASE, BMP, TEXT = 0x1000, 0x0C3C00, 0x0FC000


def main(src, dst, nibbles):
    img = bytearray(open(src, "rb").read())
    img += bytes(BMP - BASE - len(img))
    if nibbles:                              # [T][R][G][B], left pixel high
        for y in range(360):
            for x in range(0, 640, 2):
                p = [((x + k) // 8 + y // 6) & 15 for k in (0, 1)]
                img.append(p[0] << 4 | p[1])
        img += bytes(230400 - 115200)
    else:
        for y in range(360):
            for x in range(640):
                v = (x * 3 + y * 5 + (x * y >> 7)) & 255
                img.append(0 if (x // 32 + y // 24) % 7 == 0 else v)
    img += bytes(TEXT - BASE - len(img))
    for r in range(25):
        for c in range(80):
            ch = (r * 80 + c) & 255
            fg, bg = (r + c) & 7, (r * 3 + c // 4) & 7
            transp, stip = (c // 10) % 3 == 1, (c // 10) % 3 == 2 and r % 2 == 0
            if r % 5 == 4:
                ch = 32                      # rows of blanks: background only
            w = ch | fg << 8 | bg << 11 | transp << 14 | stip << 15
            img += w.to_bytes(2, "little")
    open(dst, "wb").write(img)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], "--nibbles" in sys.argv)
