"""Reference (Python) results for the benchmarks in sim/bench.

Each function mirrors its benchmark's algorithm, so the simulator's output
can be checked independently of the assembler and simulator.
  bench_ref.py           print expected outputs
  bench_ref.py --update  rewrite the '; expect:' line of each benchmark
"""
import os
import re
import sys

BENCH = os.path.join(os.path.dirname(__file__), "..", "bench")


def hello():
    return "Hello, world!\n"


def sieve():
    n = 8192
    flags = [1] * n
    flags[0] = flags[1] = 0
    count = 0
    for i in range(2, n):
        if flags[i]:
            count += 1
            for j in range(2 * i, n, i):
                flags[j] = 0
    return f"{count}\n"


def fib():
    def f(n):
        return n if n < 2 else f(n - 1) + f(n - 2)
    return f"{f(20)}\n"


KEYWORDS = ["PRINT", "GOTO", "GOSUB", "RETURN", "IF", "THEN", "FOR", "TO", "NEXT", "LET",
            "END", "INPUT", "REM", "STEP", "DIM", "DATA", "READ", "AND", "OR", "NOT"]
LINES = ["10 PRINT 'HELLO' ; X", "20 FOR I = 1 TO 100 STEP 2", "30 LET A = A + I * 3", "40 NEXT I",
         "50 IF A > 1000 THEN GOTO 80", "60 GOSUB 200", "70 GOTO 20", "80 PRINT A AND NOT B OR C",
         "200 REM SUBROUTINE", "210 RETURN", "220 END"]


def tokenize():
    s = 0
    for _ in range(50):
        for line in LINES:
            i = 0
            while i < len(line):
                ch = line[i]
                if "A" <= ch <= "Z":
                    j = i
                    while j < len(line) and "A" <= line[j] <= "Z":
                        j += 1
                    word = line[i:j]
                    tok = 0x81 + KEYWORDS.index(word) if word in KEYWORDS else ord(word[0])
                    s += tok
                    i = j
                elif "0" <= ch <= "9":
                    v = 0
                    while i < len(line) and "0" <= line[i] <= "9":
                        v = (v * 10 + ord(line[i]) - 48) & 0xFFFF
                        i += 1
                    s += v
                else:
                    s += ord(ch)
                    i += 1
                s &= 0xFFFF
    return f"{s}\n"


def memops():
    n = 8192
    src = bytes(((i * 7 + 3) & 0xFF) for i in range(n))
    a = b = 0
    for k in range(0, n, 2):
        a = (a + (src[k] | src[k + 1] << 8)) & 0xFFFF
        b = (b + a) & 0xFFFF
    return f"{b} {b} {b}\n"


def lines():
    w, h = 640, 360
    fb = bytearray(w * h)
    pts = [(x, 0) for x in range(0, 640, 16)] + [(x, 359) for x in range(0, 640, 16)] + \
          [(0, y) for y in range(0, 360, 16)] + [(639, y) for y in range(0, 360, 16)]
    for color, (x1, y1) in enumerate(pts):
        p = 180 * w + 320
        dx, sx = x1 - 320, 1
        if dx < 0:
            dx, sx = -dx, -1
        dy, sy = y1 - 180, w
        if dy < 0:
            dy, sy = -dy, -w
        steps = max(dx, dy) + 1
        dy = -dy
        err = dx + dy
        for _ in range(steps):
            fb[p] = color & 0xFF
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                p += sx
            if e2 <= dx:
                err += dx
                p += sy
    s = sum(fb[k] | fb[k + 1] << 8 for k in range(0, len(fb), 2)) & 0xFFFF
    return f"{s}\n"


def arith32():
    total = sum(i * i for i in range(1000)) & 0xFFFFFFFF
    q, r = divmod(total, 7)
    return f"{total:08X} {q:08X} {r:08X}\n"


REFS = {"hello": hello, "sieve": sieve, "fib": fib, "tokenize": tokenize, "memops": memops,
        "lines": lines, "arith32": arith32}


def main():
    for name, fn in REFS.items():
        out = fn()
        print(f"{name:10} {out!r}")
        if "--update" in sys.argv:
            path = os.path.join(BENCH, name + ".s")
            src = open(path).read()
            line = "; expect: " + repr(out).replace("'", '"')
            src = re.sub(r"^; expect: .*$", lambda m: line, src, count=1, flags=re.M)
            open(path, "w").write(src)


if __name__ == "__main__":
    main()
