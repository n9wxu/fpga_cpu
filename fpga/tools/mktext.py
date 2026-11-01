"""Build the demo screen for rtl/video720.v: 80x25 text cells in the
docs/io.md format, as $readmemh data (2048 16-bit words).

usage: mktext.py out.hex

The seconds count is written over cells COUNT_ROW, COUNT_COL.. by the
hardware (board_test.v's COUNT_ADDR); keep the two in step.
"""
import sys

COLS, ROWS = 80, 25
COUNT_ROW, COUNT_COL = 17, 52

BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE = range(8)
NAMES = ["black", "blue", "green", "cyan", "red", "magenta", "yellow", "white"]


def cell(ch, fg=WHITE, bg=BLACK, transparent=False, stipple=False):
    c = ch if isinstance(ch, int) else ord(ch.encode("cp437"))
    return c | fg << 8 | bg << 11 | transparent << 14 | stipple << 15


screen = [[cell(" ") for _ in range(COLS)] for _ in range(ROWS)]


def put(row, col, s, **attr):
    for i, ch in enumerate(s):
        screen[row][col + i] = cell(ch, **attr)


def box(row, col, w, h, **attr):
    put(row, col, "┌" + "─" * (w - 2) + "┐", **attr)
    for r in range(row + 1, row + h - 1):
        put(r, col, "│", **attr)
        put(r, col + w - 1, "│", **attr)
    put(row + h - 1, col, "└" + "─" * (w - 2) + "┘", **attr)


def main(out):
    title = " fpga_cpu video: 1280x720 60 Hz, 640x360 shown 2x, 80x25 text, 8x14 font"
    put(0, 0, title.ljust(COLS), fg=WHITE, bg=BLUE)

    put(2, 1, "Colours 0-7, the same in every mode:", fg=WHITE)
    for i, name in enumerate(NAMES):
        fg = BLACK if i in (GREEN, CYAN, YELLOW, WHITE) else WHITE
        put(3, i * 10, f" {i} {name}".ljust(10), fg=fg, bg=i)
        put(4, i * 10, f" {i} {name}".ljust(10), fg=i, bg=WHITE if i == BLACK else BLACK)

    put(6, 1, "Character set, CP437:", fg=WHITE)
    for code in range(256):
        screen[7 + code // 32][1 + (code % 32) * 2] = cell(code, fg=CYAN)

    box(16, 1, 38, 6, fg=WHITE)
    put(17, 3, "Text background options:", fg=WHITE)
    put(18, 3, " opaque ", fg=WHITE, bg=RED)
    put(18, 12, " transparent ", fg=WHITE, bg=RED, transparent=True)
    put(18, 26, " stipple ", fg=WHITE, bg=RED, stipple=True)
    put(19, 3, "The backdrop (blue) shows through", fg=WHITE)
    put(20, 3, "transparent cells, and below text.", fg=WHITE)

    box(16, 40, 26, 4, fg=YELLOW)
    put(COUNT_ROW, 42, "Seconds:", fg=WHITE)
    put(COUNT_ROW, COUNT_COL, "0000", fg=YELLOW)
    put(18, 42, "BTN0 resets the count", fg=WHITE)

    put(22, 1, "Every logical pixel is 2x2 physical pixels; the last 10 logical", fg=GREEN)
    put(23, 1, "lines (20 physical) below the text are always backdrop.", fg=GREEN)

    words = [c for row in screen for c in row]
    words += [0] * (2048 - len(words))
    with open(out, "w") as f:
        f.write("".join(f"{w:04x}\n" for w in words))


if __name__ == "__main__":
    main(sys.argv[1])
