"""Compare system_tb's frame dump with an fsim --screenshot PPM, pixel for
pixel, and write a PNG of the dump.

usage: cmpshot.py frame.txt fsim.ppm [out.png]
"""
import sys

RGB8 = [(0, 0, 0), (0, 0, 255), (0, 255, 0), (0, 255, 255),
        (255, 0, 0), (255, 0, 255), (255, 255, 0), (255, 255, 255)]


def read_ppm(path):
    data = open(path, "rb").read()
    parts, i = [], 0
    while len(parts) < 4:
        while data[i:i + 1].isspace():
            i += 1
        j = i
        while not data[j:j + 1].isspace():
            j += 1
        parts.append(data[i:j])
        i = j
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[i + 1:]


def main(frame, ppm, png=None):
    rows = [l.strip() for l in open(frame) if l.strip()]
    w, h, px = read_ppm(ppm)
    bad, first = 0, None
    for y in range(h):
        for x in range(w):
            want = tuple(px[3 * (y * w + x):3 * (y * w + x) + 3])
            got = RGB8[int(rows[y][x])]
            if want != got:
                bad += 1
                first = first or (x, y, got, want)
    if png:
        sys.path.insert(0, __import__("os").path.join(__import__("os").path.dirname(__file__), "..", "sim"))
        import frame2png
        frame2png.main(frame, png)
    if bad:
        print(f"cmpshot: {bad} pixels differ; first at x={first[0]} y={first[1]}: got {first[2]}, fsim {first[3]}")
        sys.exit(1)
    print(f"cmpshot: {w}x{h} identical")


if __name__ == "__main__":
    main(*sys.argv[1:4])
