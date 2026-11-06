"""Pictures in and out of paint on the Spartan-3, over JTAG (fpga/tools/jtagload.py).

usage: paint.py run                   load build/paint.bin and run it (a white picture)
       paint.py open PICTURE [--dither]   run paint with PICTURE (any image Pillow reads;
                                          scaled to fit 640x360, in the 8 colours)
       paint.py save OUT.png          the picture paint is showing, as a PNG; paint then
                                      starts again with it (the undo history is lost)

The picture is DOC in src/paint.h: 640x360, 4 bits a pixel at 0x021200. A
read holds the CPU in reset, so save restarts paint afterwards with the
word at 0x021100 set to "PNT1", which tells it to keep the picture.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
APP = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(APP))
sys.path.insert(0, os.path.join(ROOT, "fpga", "tools"))
import jtagload  # noqa: E402

DOC_A = 0x021200
HEADER_A = 0x021100
MAGIC = 0x31544E50        # "PNT1": keep the picture
RUNNING = 0x4E555250      # "PRUN": paint has started
W, H = 640, 360
COLOURS = [(0, 0, 0), (0, 0, 255), (0, 255, 0), (0, 255, 255),
           (255, 0, 0), (255, 0, 255), (255, 255, 0), (255, 255, 255)]
BIN = os.path.join(APP, "build", "paint.bin")


def words_of(data):
    data += b"\0" * (-len(data) % 4)
    return [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]


def start(picture=None):
    """load paint (and a picture for it to keep) and run it"""
    image = open(BIN, "rb").read()
    cmds = ["init"] + jtagload.user2(0x40000000 | 0x1000)        # hold
    cmds += jtagload.load(0x1000, words_of(image))
    if picture is not None:
        cmds += jtagload.load(DOC_A, words_of(picture))
        cmds += jtagload.load(HEADER_A, [MAGIC, 0])
    cmds += jtagload.load(0x0FC000, [0] * 1000)                  # (as jtagload.py: the text buffer)
    cmds += jtagload.user2(0xC0000000 | 0x1000)                  # run
    cmds += ["sleep 100"] + jtagload.read_status() + ["shutdown"]
    out = jtagload.run_tcl(cmds)
    status = [line for line in out.splitlines() if line.startswith("status")]
    print(jtagload.decode(status[-1]) if status else out)


def to_picture(path, dither):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    im.thumbnail((W, H), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), (255, 255, 255))
    canvas.paste(im, ((W - im.width) // 2, (H - im.height) // 2))
    pal = Image.new("P", (1, 1))
    pal.putpalette([c for rgb in COLOURS for c in rgb] + [0] * (256 - 8) * 3)
    q = canvas.quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
    px = q.tobytes()
    return bytes((px[i] & 7) << 4 | (px[i + 1] & 7) for i in range(0, W * H, 2))


def save(out):
    head = jtagload.read_words(HEADER_A, 2)
    if head[1] != RUNNING:
        sys.exit(f"paint is not running (0x{HEADER_A:06x}+4 is 0x{head[1]:08x}); run it first: paint.py run")
    print(f"reading the picture ({W * H // 8} words)...")
    words = jtagload.read_words(DOC_A, W * H // 8)
    data = b"".join(w.to_bytes(4, "little") for w in words)
    from PIL import Image
    im = Image.new("P", (W, H))
    im.putpalette([c for rgb in COLOURS for c in rgb])
    im.putdata([(b >> 4) & 7 if k == 0 else b & 7 for b in data for k in (0, 1)])
    im.save(out)
    print(f"saved {out}; starting paint again with the picture")
    start(data)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("run")
    o = sub.add_parser("open")
    o.add_argument("picture")
    o.add_argument("--dither", action="store_true", help="Floyd-Steinberg (for photographs)")
    s = sub.add_parser("save")
    s.add_argument("out")
    a = ap.parse_args()
    if not os.path.exists(BIN):
        sys.exit(f"{BIN} is missing: make -C apps/paint")
    if a.cmd == "run":
        start()
    elif a.cmd == "open":
        start(to_picture(a.picture, a.dither))
    else:
        save(a.out)


if __name__ == "__main__":
    main()
