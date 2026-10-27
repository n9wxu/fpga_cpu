"""Check BASIC's image decoders (src/image.c) against reference results.

    make -C basic imgcheck        (builds tools/imgdump with ASan and UBSan)

PNG: files are written by a small encoder here, covering every color type
and bit depth, all five filters, stored/fixed/dynamic deflate blocks,
IDAT split across chunks and tRNS. The decoded RGBA must match exactly.

JPEG: files are written by Pillow (libjpeg) in gray, 4:4:4, 4:2:2 and
4:2:0, at several qualities and odd sizes, with optimized Huffman tables
and restart markers. They are compared with Pillow's own decode. The IDCT,
color conversion and chroma upsampling follow libjpeg's defaults, so every
channel must agree within 3 levels, and on average within 0.5.

Unsupported files (progressive, interlaced, CMYK) and damaged files must
fail with ?IMAGE FORMAT, never crash.
"""
import io
import os
import random
import struct
import subprocess
import sys
import tempfile
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
DUMP = os.path.join(HERE, "..", "build", "imgdump")
E_IF = 21  # index of IMAGE FORMAT in basic.h's error enum

random.seed(1)
fails = 0
work = tempfile.mkdtemp(prefix="imgcheck")


def run(data, ext):
    src = os.path.join(work, "in." + ext)
    out = os.path.join(work, "out.pam")
    with open(src, "wb") as f:
        f.write(data)
    r = subprocess.run([DUMP, src, out], capture_output=True, text=True)
    if r.returncode not in (0, 1) or "runtime error" in r.stderr or "AddressSanitizer" in r.stderr:
        return "crash", r.stderr[-800:]
    if r.returncode == 1:
        return "error", int(r.stdout.split()[-1])
    raw = open(out, "rb").read()
    hdr, body = raw.split(b"ENDHDR\n", 1)
    kv = dict(l.split(" ", 1) for l in hdr.decode().splitlines()[1:])
    return "ok", (int(kv["WIDTH"]), int(kv["HEIGHT"]), body)


def check(name, cond, detail=""):
    global fails
    if not cond:
        fails += 1
        print(f"FAIL {name} {detail}")


# ---- PNG ----

def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)


def pack_row(samples, depth):
    if depth == 8:
        return bytes(samples)
    if depth == 16:
        return b"".join(struct.pack(">H", s) for s in samples)
    out, acc, n = bytearray(), 0, 0
    for s in samples:
        acc = acc << depth | s
        n += depth
        if n == 8:
            out.append(acc)
            acc = n = 0
    if n:
        out.append(acc << (8 - n))
    return bytes(out)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else b if pb <= pc else c


def filt(row, prev, bpp, f):
    out = bytearray()
    for i, x in enumerate(row):
        a = row[i - bpp] if i >= bpp else 0
        b = prev[i]
        c = prev[i - bpp] if i >= bpp else 0
        pred = [0, a, b, (a + b) // 2, paeth(a, b, c)][f]
        out.append((x - pred) & 255)
    return bytes([f]) + bytes(out)


def make_png(w, h, ctype, depth, level=6, strategy=None, idat_split=0, trns=None, pal=None, interlace=0):
    ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    maxv = (1 << depth) - 1
    rows, expect = [], bytearray()
    if ctype == 3 and pal is None:
        pal = [(random.randrange(256), random.randrange(256), random.randrange(256)) for _ in range(maxv + 1)]
    for y in range(h):
        samples = []
        for x in range(w):
            if ctype == 3:
                px = [random.randrange(len(pal)) if random.random() < 0.3 else (x + y) % len(pal)]
            else:
                base = [(x * 37 + y * 11 + k * 50) % (maxv + 1) for k in range(ch)]
                px = [random.randrange(maxv + 1) if random.random() < 0.2 else v for v in base]
            samples += px
            to8 = (lambda v: v >> 8) if depth == 16 else (lambda v: v * 255 // maxv)
            if ctype == 0:
                g = to8(px[0])
                a = 0 if trns is not None and px[0] == trns[0] else 255
                expect += bytes([g, g, g, a])
            elif ctype == 2:
                a = 0 if trns is not None and tuple(px) == tuple(trns) else 255
                expect += bytes([to8(px[0]), to8(px[1]), to8(px[2]), a])
            elif ctype == 3:
                r, g, b = pal[px[0]]
                a = trns[px[0]] if trns is not None and px[0] < len(trns) else 255
                expect += bytes([r, g, b, a])
            elif ctype == 4:
                g = to8(px[0])
                expect += bytes([g, g, g, to8(px[1])])
            else:
                expect += bytes(to8(v) for v in px)
        rows.append(pack_row(samples, depth))
    bpp = max(1, ch * depth // 8)
    raw, prev = bytearray(), bytes(len(rows[0]))
    for y, r in enumerate(rows):
        raw += filt(r, prev, bpp, y % 5)
        prev = r
    co = zlib.compressobj(level, zlib.DEFLATED, 15, 9, strategy if strategy is not None else zlib.Z_DEFAULT_STRATEGY)
    z = co.compress(bytes(raw)) + co.flush()
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, interlace))
    out += chunk(b"tEXt", b"Comment\x00imgcheck")
    if ctype == 3:
        out += chunk(b"PLTE", b"".join(bytes(c) for c in pal))
    if trns is not None:
        if ctype == 3:
            out += chunk(b"tRNS", bytes(trns))
        else:
            out += chunk(b"tRNS", b"".join(struct.pack(">H", v) for v in trns))
    if idat_split:
        for i in range(0, len(z), idat_split):
            out += chunk(b"IDAT", z[i:i + idat_split])
    else:
        out += chunk(b"IDAT", z)
    return out + chunk(b"IEND", b""), bytes(expect)


png_cases = []
for ctype, depths in ((0, (1, 2, 4, 8, 16)), (2, (8, 16)), (3, (1, 2, 4, 8)), (4, (8, 16)), (6, (8, 16))):
    for d in depths:
        for (w, h) in ((1, 1), (7, 5), (33, 17)):
            png_cases.append((f"type{ctype} depth{d} {w}x{h}", dict(w=w, h=h, ctype=ctype, depth=d)))
png_cases += [
    ("stored blocks", dict(w=40, h=30, ctype=2, depth=8, level=0)),
    ("fixed Huffman", dict(w=40, h=30, ctype=6, depth=8, strategy=zlib.Z_FIXED)),
    ("level 9", dict(w=200, h=120, ctype=6, depth=8, level=9)),
    ("IDAT split in 7-byte chunks", dict(w=64, h=48, ctype=2, depth=8, idat_split=7)),
    ("gray key tRNS", dict(w=20, h=10, ctype=0, depth=8, trns=(37,))),
    ("RGB key tRNS", dict(w=20, h=10, ctype=2, depth=8, trns=(0, 50, 100))),
    ("16-bit RGB key tRNS", dict(w=9, h=9, ctype=2, depth=16, trns=(0, 50, 100))),
    ("palette tRNS", dict(w=20, h=10, ctype=3, depth=4, trns=[0, 128, 255])),
    ("sprite sheet 256x64", dict(w=256, h=64, ctype=6, depth=8)),
    ("wide 2000x3", dict(w=2000, h=3, ctype=2, depth=8)),
]
for name, kw in png_cases:
    data, expect = make_png(**kw)
    st, res = run(data, "png")
    if st != "ok":
        check(f"png {name}", False, f"{st} {res}")
        continue
    w, h, body = res
    check(f"png {name}", (w, h) == (kw["w"], kw["h"]) and body == expect,
          f"size {w}x{h}, {sum(a != b for a, b in zip(body, expect))} bytes differ")

st, res = run(make_png(16, 16, 2, 8, interlace=1)[0], "png")
check("png interlaced is refused", st == "error" and res == E_IF, f"{st} {res}")

# Pillow-written PNGs too (its own filter choices and zlib settings)
for mode in ("1", "L", "LA", "RGB", "RGBA", "P"):
    im = Image.radial_gradient("L").resize((61, 47)).convert("RGB")
    im = Image.merge("RGB", (im.getchannel(0), im.getchannel(0).rotate(90), im.getchannel(0).transpose(0)))
    im = im.convert(mode) if mode != "LA" else Image.merge("LA", (im.convert("L"), im.convert("L").rotate(45)))
    buf = io.BytesIO()
    im.save(buf, "PNG", optimize=True)
    st, res = run(buf.getvalue(), "png")
    ref = Image.open(io.BytesIO(buf.getvalue())).convert("RGBA").tobytes()
    check(f"png Pillow {mode}", st == "ok" and res[2] == ref, f"{st}")

# ---- JPEG ----

def test_image(w, h):
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 255 // max(1, w - 1)), (y * 255 // max(1, h - 1)), ((x ^ y) * 3) & 255)
    noise = Image.effect_noise((w, h), 40).convert("RGB")
    im = Image.blend(im, noise, 0.25)
    from PIL import ImageDraw
    d = ImageDraw.Draw(im)
    d.ellipse((w // 4, h // 4, 3 * w // 4, 3 * h // 4), outline=(255, 255, 0), width=max(1, w // 40))
    d.rectangle((0, 0, w // 3, h // 5), fill=(200, 30, 60))
    return im


def stats(a, b, step):
    diffs = [abs(x - y) for i, (x, y) in enumerate(zip(a, b)) if i % 4 != 3]
    return max(diffs), sum(diffs) / len(diffs)


jpeg_cases = []
for (w, h) in ((1, 1), (8, 8), (17, 9), (333, 201), (640, 360)):
    for sub in (0, 1, 2):
        for q in (15, 75, 95):
            jpeg_cases.append((w, h, "RGB", sub, q, {}))
jpeg_cases += [(100, 60, "L", 0, 80, {}), (37, 23, "L", 0, 30, {}),
               (200, 120, "RGB", 2, 80, {"optimize": True}),
               (200, 120, "RGB", 2, 80, {"restart_marker_blocks": 3}),
               (200, 120, "RGB", 0, 80, {"restart_marker_rows": 1}),
               (64, 48, "RGB", 0, 90, {"keep_rgb": True})]
for (w, h, mode, sub, q, extra) in jpeg_cases:
    name = f"jpeg {w}x{h} {mode} sub{sub} q{q} {extra or ''}"
    im = test_image(w, h).convert(mode)
    buf = io.BytesIO()
    try:
        im.save(buf, "JPEG", quality=q, subsampling=sub, **extra)
    except TypeError as e:
        print(f"skip {name}: {e}")
        continue
    data = buf.getvalue()
    st, res = run(data, "jpg")
    if st != "ok":
        check(name, False, f"{st} {res}")
        continue
    ref = Image.open(io.BytesIO(data)).convert("RGBA").tobytes()
    rw, rh, body = res
    if (rw, rh) != (w, h):
        check(name, False, f"size {rw}x{rh}")
        continue
    mx, mean = stats(body, ref, 4)
    check(name, mx <= 3 and mean <= 0.5, f"max {mx} mean {mean:.2f}")
    print(f"ok   {name}: max {mx}, mean {mean:.2f}")

for desc, kw in (("progressive", {"progressive": True}),):
    buf = io.BytesIO()
    test_image(40, 30).save(buf, "JPEG", **kw)
    st, res = run(buf.getvalue(), "jpg")
    check(f"jpeg {desc} is refused", st == "error" and res == E_IF, f"{st} {res}")
buf = io.BytesIO()
test_image(40, 30).convert("CMYK").save(buf, "JPEG")
st, res = run(buf.getvalue(), "jpg")
check("jpeg CMYK is refused", st == "error" and res == E_IF, f"{st} {res}")

# ---- damaged files: must fail cleanly, never crash ----
seeds = []
for kw in (dict(w=50, h=40, ctype=6, depth=8), dict(w=30, h=20, ctype=3, depth=4)):
    seeds.append(("png", make_png(**kw)[0]))
for sub in (0, 2):
    buf = io.BytesIO()
    test_image(64, 40).save(buf, "JPEG", quality=70, subsampling=sub)
    seeds.append(("jpg", buf.getvalue()))
crashes = 0
for i in range(600):
    ext, data = random.choice(seeds)
    d = bytearray(data)
    kind = i % 3
    if kind == 0:
        d = d[:random.randrange(len(d))]
    else:
        for _ in range(random.randrange(1, 8)):
            d[random.randrange(len(d))] = random.randrange(256)
    st, res = run(bytes(d), ext)
    if st == "crash":
        crashes += 1
        if crashes <= 3:
            print(f"FAIL damaged {ext} #{i} crashed:\n{res}")
check("damaged files", crashes == 0, f"{crashes} crashes in 600")
for junk in (b"", b"\x00" * 10, b"GIF89a....", b"\xff\xd8\xff\xd9"):
    st, res = run(junk, "bin")
    check(f"junk {junk[:6]!r}", st == "error", f"{st} {res}")

print(f"\n{len(png_cases) + len(jpeg_cases) + 600} cases, {fails} failure{'s' if fails != 1 else ''}")
sys.exit(1 if fails else 0)
