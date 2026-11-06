"""Scripted paint sessions in fsim (the Spartan-3's low video tier, PS/2
mouse with a wheel): each session moves, clicks and drags the mouse to
absolute screen positions, then saves the screen and the picture as PNG
in out/. Uses build/paint_test.bin (no pointer acceleration: one count is
one pixel).

usage: fsimtest.py [session ...]      (default: all)
"""
import os
import subprocess
import sys
import zlib
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(APP))
FSIM = os.path.join(ROOT, "sim", "build", "fsim")
BIN = os.path.join(APP, "build", "paint_test.bin")
OUT = os.path.join(APP, "out")
DOC_A = 0x021200

L, R, M = 1, 2, 4
# canvas origin on the screen and the view's offset at 1x (src/view.c view_init)
VX, VY = 64, 32
OX, OY = 40, 62


def doc(x, y):
    """the screen point of picture point x, y (40..620, 30..350) squeezed into the
    initial view of the picture window (it shows 40..597, 62..297)"""
    px = 42 + (x - 40) * 550 // 580
    py = 64 + (y - 30) * 230 // 320
    return VX + px - OX, VY + py - OY


class Session:
    def __init__(self):
        self.x, self.y, self.b = 320, 180, 0
        self.p = []

    def pkt(self, dx, dy, z=0):
        self.p.append(f"{dx},{-dy},{self.b},{z}")

    def wait(self, n=6):
        for _ in range(n):
            self.pkt(0, 0)

    def move(self, x, y, step=6):
        """in a straight line, step pixels at a time"""
        x0, y0 = self.x, self.y
        n = max(1, (max(abs(x - x0), abs(y - y0)) + step - 1) // step)
        for i in range(1, n + 1):
            nx, ny = x0 + round((x - x0) * i / n), y0 + round((y - y0) * i / n)
            if (nx, ny) != (self.x, self.y):
                self.pkt(nx - self.x, ny - self.y)
                self.x, self.y = nx, ny
        return self

    def down(self, b=L):
        self.wait(6)
        self.b |= b
        self.pkt(0, 0)
        self.wait(6)

    def up(self, b=L):
        self.wait(6)
        self.b &= ~b
        self.pkt(0, 0)
        self.wait(6)

    def click(self, x, y, b=L):
        self.move(x, y)
        self.down(b)
        self.up(b)

    def drag(self, pts, b=L, step=4):
        self.move(*pts[0])
        self.down(b)
        for p in pts[1:]:
            self.move(*p, step=step)
        self.up(b)

    def keys(self):
        """wait while the keyboard types (its script's next \\w part)"""
        self.wait(6)
        self.p.append("k")
        self.wait(6)

    def wheel(self, z):
        self.wait(6)
        self.pkt(0, 0, z)
        self.wait(6)

    def script(self):
        return ";".join(self.p)


# screen positions of the user interface (src/ui.c)
TX, TY = 4, 32                            # the Tools window's content (src/win.c layout0)
KX, KY = 64, 299                          # the Colours window's


def tool(i):
    return TX + 2 + (i & 1) * 24 + 11, TY + 2 + (i >> 1) * 22 + 10


def colour(i):
    return KX + 44 + i * 20 + 8, KY + 2 + 8


def pattern(i):
    return KX + 44 + (i & 7) * 20 + 8, KY + 21 + (i >> 3) * 19 + 8


def opt_row(i):
    return TX + 26, TY + 180 + 22 + i * 19 + 8


def opt_top(i, n):
    w = 44 // n
    return TX + 4 + i * w + w // 2, TY + 180 + 3 + 8


T_SELECT, T_PICKER, T_PENCIL, T_BRUSH, T_SPRAY, T_ERASER, T_BUCKET, T_TEXT, T_LINE, T_CURVE, \
    T_RECT, T_RRECT, T_ELLIPSE, T_POLYGON, T_ZOOM, T_HAND = range(16)


def s_shapes():
    s = Session()
    s.wait(30)
    s.click(*tool(T_PENCIL))
    s.drag([doc(60, 60), doc(120, 80), doc(90, 130), doc(60, 60)])
    s.click(*colour(4))                       # red
    s.click(*tool(T_BRUSH))
    s.drag([doc(150, 60), doc(250, 120), doc(300, 60)])
    s.click(*colour(1))                       # blue
    s.click(*tool(T_RECT))
    s.click(*opt_row(2))                      # 3 wide
    s.drag([doc(340, 50), doc(450, 130)])
    s.click(*tool(T_ELLIPSE))
    s.click(*opt_top(1, 3))                   # filled
    s.click(*pattern(1))
    s.drag([doc(470, 50), doc(600, 140)])
    s.click(*pattern(0))
    s.click(*colour(2))
    s.click(*tool(T_RRECT))
    s.click(*opt_top(2, 3))                   # framed
    s.drag([doc(60, 170), doc(200, 260)], b=R)   # right: green inside, white... (swapped)
    s.click(*tool(T_LINE))
    s.click(*opt_row(4))                      # 8 wide
    s.drag([doc(230, 170), doc(330, 260)])
    s.click(*opt_row(0))
    s.drag([doc(230, 260), doc(330, 170)])
    s.click(*colour(5))
    s.click(*tool(T_POLYGON))
    s.click(*opt_top(1, 3))
    s.drag([doc(360, 180), doc(420, 170)])
    s.click(*doc(440, 250))
    s.click(*doc(380, 260))
    s.click(*doc(361, 181))                   # the first corner: closes
    s.click(*colour(6))
    s.click(*tool(T_BUCKET))
    s.click(*doc(80, 70))                     # inside the pencil's triangle
    s.click(*colour(0))
    s.click(*tool(T_CURVE))
    s.click(*opt_row(2))
    s.drag([doc(460, 200), doc(600, 200)])
    s.drag([doc(500, 160), doc(500, 150)])
    s.drag([doc(560, 260), doc(560, 270)])
    s.click(*tool(T_SPRAY))
    s.click(*colour(3))
    s.drag([doc(60, 300), doc(200, 320)], step=2)
    s.click(*tool(T_ERASER))
    s.drag([doc(150, 50), doc(200, 140)])
    s.wait(30)
    return s


def s_select():
    s = Session()
    s.wait(30)
    s.click(*tool(T_RECT))
    s.click(*opt_top(1, 3))
    s.click(*colour(4))
    s.drag([doc(60, 60), doc(140, 120)])
    s.click(*colour(1))
    s.click(*tool(T_ELLIPSE))
    s.drag([doc(100, 90), doc(180, 160)])
    s.click(*tool(T_SELECT))
    s.drag([doc(50, 50), doc(190, 170)])      # a box
    s.drag([doc(100, 100), doc(300, 150)])    # move it
    s.drag([doc(300, 150), doc(300, 250)], b=R)   # leave a copy, move on
    s.click(*doc(500, 40))                    # put it down
    s.click(*tool(T_TEXT))
    s.wait(10)
    s.click(*doc(420, 100))
    s.keys()
    s.click(*tool(T_ZOOM))
    s.click(*doc(440, 102))                   # 2x
    s.wheel(1)                                # 4x
    s.wait(10)
    s.click(*tool(T_PENCIL))
    s.click(*colour(2))
    s.drag([(300, 150), (360, 200)])
    s.wait(30)
    return s


def s_undo():
    s = Session()
    s.wait(30)
    s.click(*tool(T_BRUSH))
    s.drag([doc(60, 60), doc(300, 60)])
    s.drag([doc(60, 100), doc(300, 100)])
    s.drag([doc(60, 140), doc(300, 140)])
    s.click(*tool(T_LINE))
    s.drag([doc(60, 180), doc(300, 180)])
    # Edit > Undo twice, then Redo once
    for _ in range(2):
        s.click(56, 6)
        s.click(70, 22)
    s.click(56, 6)
    s.click(70, 36)
    s.wait(20)
    return s


def menu(m, item):
    """a menu's title and an item (counting separators as half)"""
    xs = [8, 56, 104, 160]
    return (xs[m] + 8, 6), (xs[m] + 20, 17 + item + 6)


def s_menus():
    s = Session()
    s.wait(30)
    s.click(*tool(T_ELLIPSE))
    s.click(*opt_top(1, 3))
    s.click(*colour(4))
    s.drag([doc(60, 60), doc(160, 120)])
    s.click(*colour(1))
    s.click(*tool(T_RECT))
    s.click(*opt_top(1, 3))
    s.drag([doc(60, 60), doc(90, 90)])
    s.click(*tool(T_SELECT))
    s.drag([doc(50, 50), doc(170, 130)])
    t, i = menu(2, 3)                         # Image > Flip horizontal (the first item)
    s.click(*t); s.click(i[0], 17 + 6)
    t, i = menu(1, 0)                         # Edit > Copy (5th row: Undo Redo - Cut Copy)
    s.click(*t); s.click(i[0], 17 + 14 * 3 + 7 + 6)
    s.click(*t); s.click(i[0], 17 + 14 * 4 + 7 + 6)   # Paste
    s.drag([(VX + 10, VY + 10), doc(300, 200)])     # the pasted copy, at the view's corner
    s.click(*doc(500, 300))                   # down
    s.move(*colour(3)); s.wheel(-1); s.wheel(-1)     # the wheel over the colours: 3 -> 5
    s.click(*tool(T_BUCKET))
    s.click(*doc(400, 100))
    s.move(*doc(320, 200))
    s.down(M); s.up(M)                        # the middle button: the whole picture
    s.wait(30)
    return s


def s_pan():
    s = Session()
    s.wait(30)
    s.click(*tool(T_ELLIPSE))
    s.click(*opt_top(1, 3))
    s.click(*pattern(11))
    s.drag([doc(100, 80), doc(400, 300)])
    s.click(*tool(T_SELECT))
    s.drag([doc(150, 150), doc(250, 200)])
    s.move(*doc(250, 180))
    s.wheel(1)                                # 2x
    s.move(300, 200)
    s.down(M)
    s.move(200, 120, step=3)                  # drag the view
    s.move(260, 160, step=3)
    s.up(M)
    s.wait(30)
    return s


def s_windows():
    s = Session()
    s.wait(30)
    s.click(*tool(T_ELLIPSE))
    s.click(*opt_top(1, 3))
    s.click(*colour(4))
    s.drag([doc(200, 100), doc(400, 250)])
    s.click(224, 6)                           # Window
    s.click(240, 17 + 14 * 2 + 7 + 6)         # New view (4x the zoom, on the middle)
    s.drag([(343 + 100, 47), (380, 150)])     # its title bar: move it down and left
    s.drag([(380 + 288 - 70 - 6, 150 + 216 - 23 - 6), (600, 340)])   # its grow box: bigger
    s.click(*tool(T_PENCIL))
    s.click(*colour(1))
    s.drag([(420, 220), (520, 260)])          # draw in the zoomed view: shows in both
    s.click(224, 6)                           # Window
    s.click(240, 17 + 14 * 1 + 6)             # Colours: hidden
    s.move(560, 100)
    s.wheel(1)                                # zoom the view under the pointer
    s.wait(30)
    return s


SESSIONS = {"windows": (s_windows, ""), "pan": (s_pan, ""), "menus": (s_menus, ""), "shapes": (s_shapes, ""), "select": (s_select, "\\wHello\\nfpga"), "undo": (s_undo, "")}


def write_png(path, w, h, rgb):
    raw = b"".join(b"\0" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def ppm_to_png(ppm, png):
    d = open(ppm, "rb").read()
    parts = d.split(maxsplit=4)
    w, h = int(parts[1]), int(parts[2])
    write_png(png, w, h, parts[4][:w * h * 3])


COL = [(0, 0, 0), (0, 0, 255), (0, 255, 0), (0, 255, 255), (255, 0, 0), (255, 0, 255), (255, 255, 0), (255, 255, 255)]


def pic_to_png(raw, png):
    rgb = bytearray()
    for b in raw:
        rgb += bytes(COL[(b >> 4) & 7]) + bytes(COL[b & 7])
    write_png(png, 640, 360, bytes(rgb))


def run(name):
    make, keys = SESSIONS[name]
    s = make()
    os.makedirs(OUT, exist_ok=True)
    ppm = os.path.join(OUT, name + ".ppm")
    dump = os.path.join(OUT, name + ".pic")
    cycles = 50_000_000 * (2 + len(s.p) // 200)
    cmd = [FSIM, "-q", "--no-hicolor", "--ps2mouse", s.script(), "--max-cycles", str(cycles),
           "--screenshot", ppm, "--dump", f"0x{DOC_A:x}:115200:{dump}", BIN]
    if keys:
        cmd[1:1] = ["--ps2keys", keys]
    subprocess.run(cmd, check=False)
    ppm_to_png(ppm, os.path.join(OUT, name + ".png"))
    pic_to_png(open(dump, "rb").read(), os.path.join(OUT, name + "_pic.png"))
    os.remove(ppm)
    os.remove(dump)
    print(f"{name}: {len(s.p)} packets, out/{name}.png, out/{name}_pic.png")


if __name__ == "__main__":
    for n in sys.argv[1:] or SESSIONS:
        run(n)
