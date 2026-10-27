"""Generate the images used by the BASIC tests and examples.

    ../.venv/bin/python tools/mkassets.py

Everything is drawn here from code, so the files can be rebuilt and carry
no outside artwork:

  tests/images.disk/HERO.PNG     32x16 RGBA sheet: two 16x16 frames, 5 colors
  tests/images.disk/SKY.JPG      64x36 gradient photo (4:2:0)
  tests/images.disk/LOGO.PNG     12x8 indexed PNG with a transparent entry
  tests/images.disk/PROG.JPG     a progressive JPEG (must be refused)
  tests/images.disk/GAME.GPL   GIMP palette, 4 colors
  tests/images.disk/GAME.PAL   the same as JASC-PAL
  tests/images.disk/STRIP.PNG  the same as a 4x1 true-color palette strip
  examples/WALKER.PNG     192x32 indexed sheet: 8 frames of a 24x32 walker,
                          palette entry 0 transparent, 1-6 its colors
  examples/WALKER.GPL     that palette as a GIMP palette file
  examples/STAR.PNG       32x32 indexed star with its hot spot in the middle
  examples/VALLEY.JPG     640x360 landscape backdrop
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.join(HERE, "..", "tests", "images.disk")
EX = os.path.join(HERE, "..", "examples")
os.makedirs(TESTS, exist_ok=True)


def hero():
    im = Image.new("RGBA", (32, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for f in range(2):
        x = f * 16
        d.rectangle((x + 4, 2, x + 11, 9), fill=(255, 200, 150, 255))   # head
        d.rectangle((x + 3, 10, x + 12, 13), fill=(40, 90, 220, 255))   # body
        d.point((x + 6, 5), fill=(0, 0, 0, 255))                          # eyes
        d.point((x + 9, 5), fill=(0, 0, 0, 255))
        legs = (x + 4, x + 10) if f == 0 else (x + 5, x + 9)
        for lx in legs:
            d.rectangle((lx, 14, lx + 1, 15), fill=(120, 60, 20, 255))
    im.save(os.path.join(TESTS, "HERO.PNG"))


def sky(path, w, h, progressive=False):
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (40 + 150 * y // h, 90 + 120 * y // h, 230 - 60 * x // w)
    im.save(path, quality=85, subsampling=2, progressive=progressive)


def logo():
    im = Image.new("P", (12, 8), 0)
    im.putpalette([0, 0, 0, 255, 0, 0, 255, 255, 255] + [0] * (253 * 3))
    d = ImageDraw.Draw(im)
    d.rectangle((1, 1, 10, 6), fill=1)
    d.rectangle((4, 3, 7, 4), fill=2)
    im.save(os.path.join(TESTS, "LOGO.PNG"), transparency=0)


WALKER_PAL = [(0, 0, 0), (240, 190, 150), (90, 50, 20), (200, 40, 40), (40, 60, 140), (50, 30, 20), (20, 20, 20)]


def indexed(im, pal):
    """RGBA drawing -> P image on exactly this palette, index 0 transparent."""
    out = Image.new("P", im.size, 0)
    out.putpalette([c for rgb in pal for c in rgb] + [0] * (3 * (256 - len(pal))))
    lut = {rgb: i for i, rgb in enumerate(pal) if i}
    src, dst = im.load(), out.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b, a = src[x, y]
            dst[x, y] = lut[(r, g, b)] if a else 0
    return out


def gpl(path, name, pal):
    with open(path, "w") as f:
        f.write(f"GIMP Palette\nName: {name}\nColumns: 4\n#\n")
        for r, g, b in pal:
            f.write(f"{r:3} {g:3} {b:3}\tcolor\n")


def walker():
    """8-frame walk cycle, 24x32 per frame, drawn on WALKER_PAL."""
    skin, hair, shirt, pants, boot, eye = WALKER_PAL[1:]
    im = Image.new("RGBA", (24 * 8, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for f in range(8):
        ox = f * 24
        ph = 2 * math.pi * f / 8
        bob = 1 if f % 4 in (1, 3) else 0
        top = 2 + bob
        d.rectangle((ox + 8, top, ox + 15, top + 7), fill=skin)          # head
        d.rectangle((ox + 8, top, ox + 15, top + 2), fill=hair)
        d.point((ox + 14, top + 4), fill=eye)
        d.rectangle((ox + 7, top + 8, ox + 16, top + 17), fill=shirt)    # body
        arm = round(3 * math.sin(ph))
        d.rectangle((ox + 11 + arm, top + 9, ox + 12 + arm, top + 16), fill=skin)
        for side in (1, -1):                                              # legs
            step = round(4 * math.sin(ph) * side)
            lx = ox + 10 + step
            d.rectangle((lx, top + 18, lx + 2, 28), fill=pants)
            d.rectangle((lx, 29, lx + 3, 30), fill=boot)
    indexed(im, WALKER_PAL).save(os.path.join(EX, "WALKER.PNG"), transparency=0)
    gpl(os.path.join(EX, "WALKER.GPL"), "Walker", WALKER_PAL)


def star():
    pal = [(0, 0, 0), (255, 220, 60), (255, 150, 20), (255, 255, 200)]
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    pts = []
    for k in range(10):
        r = 15 if k % 2 == 0 else 6
        a = math.pi * k / 5 - math.pi / 2
        pts.append((15.5 + r * math.cos(a), 15.5 + r * math.sin(a)))
    d.polygon(pts, fill=pal[2] + (255,))
    d.polygon([(x * 0.6 + 6.2, y * 0.6 + 6.2) for x, y in pts], fill=pal[1] + (255,))
    d.ellipse((13, 13, 18, 18), fill=pal[3] + (255,))
    indexed(im, pal).save(os.path.join(EX, "STAR.PNG"), transparency=0)


def game_palettes():
    pal = [(10, 20, 30), (200, 100, 50), (255, 255, 255), (0, 128, 255)]
    gpl(os.path.join(TESTS, "GAME.GPL"), "Game", pal)
    with open(os.path.join(TESTS, "GAME.PAL"), "w", newline="\r\n") as f:
        f.write("JASC-PAL\n0100\n4\n" + "".join(f"{r} {g} {b}\n" for r, g, b in pal))
    strip = Image.new("RGB", (4, 1))
    strip.putdata(pal)
    strip.save(os.path.join(TESTS, "STRIP.PNG"))


def valley():
    w, h = 640, 360
    random.seed(7)
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        t = y / h
        for x in range(w):
            px[x, y] = (int(255 * (0.35 + 0.6 * t)), int(255 * (0.55 + 0.35 * t)), int(255 * (0.95 - 0.2 * t)))
    d = ImageDraw.Draw(im)
    d.ellipse((470, 50, 540, 120), fill=(255, 235, 150))
    for layer, (base, amp, col) in enumerate(((210, 50, (90, 120, 150)), (250, 35, (60, 120, 70)),
                                               (300, 20, (40, 95, 45)))):
        pts = [(0, h)]
        for x in range(0, w + 8, 8):
            pts.append((x, base + amp * math.sin(x / (90 + 40 * layer) + layer) + random.uniform(-4, 4)))
        pts.append((w, h))
        d.polygon(pts, fill=col)
    d.rectangle((0, 330, w, h), fill=(120, 100, 70))
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    im.save(os.path.join(EX, "VALLEY.JPG"), quality=80, subsampling=2, optimize=True)


hero()
sky(os.path.join(TESTS, "SKY.JPG"), 64, 36)
sky(os.path.join(TESTS, "PROG.JPG"), 32, 16, progressive=True)
logo()
walker()
star()
game_palettes()
valley()
for p in sorted(os.listdir(TESTS)) + ["../../examples/" + n for n in ("WALKER.PNG", "WALKER.GPL", "STAR.PNG", "VALLEY.JPG")]:
    print(f"{p:28} {os.path.getsize(os.path.join(TESTS, p))} bytes")
