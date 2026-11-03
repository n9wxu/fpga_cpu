#!/usr/bin/env python3
"""The RAM machine's status screen (rtl/ramvid.v): its text and its fields.

Writes build/stat_text.hex (80 x 25 cells: attribute << 8 | character;
attribute = background << 3 | foreground, 3-bit colours) and
build/stat_fields.hex (one 32-bit word a field, then a zero word):
  bits 10:0  cell (row * 80 + column) of the field's last character
  15:11      source (a counter, see SRC below)
  18:16      format: 0 the number, 1 per mille of a second's clocks
             (shown xx.x %), 2 0.1 MB/s from words, 3 a bar (width
             cells for 100 %; ramvid.v divides by 50M/30: 30 wide), 4 source*100 / the source before it (x.xx)
  23:19      width (cells)
  26:24      attribute index into COLOURS
"""
import os

W, H = 80, 25
BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE = range(8)


def attr(fg, bg=BLACK):
    return bg << 3 | fg


# the sources (ramvid.v's snapshot, taken each second)
SRC = ["w_video", "w_media", "w_draw", "w_cpuwr", "w_cpurd", "w_fetch",
       "busy", "turn", "idle", "e_video", "e_cpu", "e_fetch",
       "insts", "cycles", "st_fetch", "st_load", "st_store",
       "underruns", "lines", "redraws", "uptime", "e_total", "e_audio"]
FMT_NUM, FMT_PCT, FMT_MBS, FMT_BAR, FMT_RATIO = range(5)
COLOURS = [attr(WHITE), attr(YELLOW), attr(GREEN), attr(CYAN), attr(RED), attr(WHITE, BLUE), attr(MAGENTA)]

text = [[(attr(WHITE), ord(" ")) for _ in range(W)] for _ in range(H)]
fields = []


def put(r, c, s, a=attr(WHITE)):
    for i, ch in enumerate(s):
        text[r][c + i] = (a, ord(ch))


def field(r, c_end, src, fmt, width, colour=1):
    fields.append((r * W + c_end) | SRC.index(src) << 11 | fmt << 16 | width << 19 | colour << 24)


def bar_row(r, label, src, colour):
    put(r, 2, label, attr(WHITE))
    field(r, 14 + 30 - 1, src, FMT_BAR, 30, colour)


title = " FPGA_CPU RAM MACHINE  -  Spartan-3 SRAM, 50 MHz, 32-bit  -  1280x720 from SRAM"
put(0, 0, title.ljust(W), attr(WHITE, BLUE))

put(2, 0, "PORT  CLIENT          WORDS/S      MB/S   SHARE    ERRORS", attr(CYAN))
rows = [("0", "video 720p", "w_video", "e_video"), ("1", "audio 48/ms", "w_media", "e_audio"),
        ("2", "CPU writes", "w_cpuwr", None), ("3", "CPU reads", "w_cpurd", "e_cpu"),
        ("4", "CPU fetch", "w_fetch", "e_fetch"), ("5", "status draw", "w_draw", None)]
for i, (p, name, w, e) in enumerate(rows):
    r = 3 + i
    put(r, 2, p, attr(WHITE))
    put(r, 6, name, attr(WHITE))
    field(r, 30, w, FMT_NUM, 10)
    field(r, 40, w, FMT_MBS, 6)
    field(r, 48, w, FMT_PCT, 5, 2)
    put(r, 49, "%")
    if e:
        field(r, 58, e, FMT_NUM, 8, 4)
    else:
        put(r, 52, "       -")

def lf(r, c, label, src, fmt, width, colour=1, suffix=""):
    """label, then the field, then suffix; returns the column after"""
    put(r, c, label)
    c += len(label) + 1
    field(r, c + width - 1, src, fmt, width, colour)
    c += width
    put(r, c, suffix)
    return c + len(suffix) + 3


put(10, 0, "RAM", attr(CYAN))
c = lf(10, 6, "busy", "busy", FMT_PCT, 5, 1, "%")
c = lf(10, c, "turnarounds", "turn", FMT_PCT, 5, 1, "%")
lf(10, c, "nothing asked", "idle", FMT_PCT, 5, 1, "%")
bar_row(11, "busy", "busy", 2)
bar_row(12, "video", "w_video", 3)
bar_row(13, "CPU fetch", "w_fetch", 6)

put(15, 0, "MOCK CPU", attr(CYAN))
put(15, 10, "the T1 demo's working mix, FC-like at 25 MHz (one instruction a clock)", attr(WHITE))
c = lf(16, 2, "instructions/s", "insts", FMT_NUM, 10, 1, "  of 25000000")
lf(16, c, "clocks per instruction", "cycles", FMT_RATIO, 5)
c = lf(17, 2, "stalled: fetch", "st_fetch", FMT_PCT, 5, 1, "%")
c = lf(17, c, "load", "st_load", FMT_PCT, 5, 1, "%")
lf(17, c, "store", "st_store", FMT_PCT, 5, 1, "%")
bar_row(18, "load stall", "st_load", 4)

put(20, 0, "VIDEO", attr(CYAN))
c = lf(20, 10, "lines/s", "lines", FMT_NUM, 8)
c = lf(20, c, "underruns", "underruns", FMT_NUM, 8, 4)
lf(20, c, "screen redraws/s", "redraws", FMT_NUM, 3)

put(22, 0, "UPTIME", attr(CYAN))
c = lf(22, 7, "", "uptime", FMT_NUM, 10, 1, " s")
lf(22, c, "errors, all ports", "e_total", FMT_NUM, 8, 4)
put(24, 0, " every word read is checked; CPU writes stay out of the framebuffer ".ljust(W), attr(BLACK, CYAN))

here = os.path.dirname(os.path.abspath(__file__))
build = os.path.join(here, "..", "build")
os.makedirs(build, exist_ok=True)
with open(os.path.join(build, "stat_text.hex"), "w") as f:
    for r in range(H):
        for a, ch in text[r]:
            f.write(f"{a << 8 | ch:04x}\n")
    for _ in range(2048 - W * H):
        f.write("0000\n")
with open(os.path.join(build, "stat_fields.hex"), "w") as f:
    for v in fields + [0] * (64 - len(fields)):
        f.write(f"{v:08x}\n")
with open(os.path.join(build, "stat_colours.hex"), "w") as f:
    for c in COLOURS + [0] * (8 - len(COLOURS)):
        f.write(f"{c:02x}\n")
print(f"{len(fields)} fields")
