"""Turn a video testbench's frame dump (one '0'-'7' rgb digit per pixel, one
line per row) into a PNG. With --doubled, also check that every line
repeats the one above it (the 2x vertical doubling) and exit 1 if not.

usage: frame2png.py [--doubled] vga_frame.txt out.png
"""
import struct
import sys
import zlib


def main(src, dst, doubled=False):
    rows = [line.strip() for line in open(src) if line.strip()]
    if doubled:
        bad = [y for y in range(1, len(rows), 2) if rows[y] != rows[y - 1]]
        if bad:
            sys.exit(f"frame2png: {len(bad)} lines differ from the line above, first {bad[0]}")
    w, h = len(rows[0]), len(rows)
    raw = bytearray()
    for row in rows:
        raw.append(0)                     # filter: none
        for c in row:
            v = int(c)
            raw += bytes((255 * (v >> 2 & 1), 255 * (v >> 1 & 1), 255 * (v & 1)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    with open(dst, "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        out.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        out.write(chunk(b"IEND", b""))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--doubled"]
    main(args[0], args[1], "--doubled" in sys.argv)
