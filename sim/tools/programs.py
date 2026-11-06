#!/usr/bin/env python3
"""Build the demos, benchmarks and programs the simulators offer, and list them.

  programs.py [--out DIR] [--list] [--json] [NAME ...]

Each program is built (its own Makefile, fcc, or the assembler) and copied, with its symbols where
there are any, into DIR (default sim/build/programs) as <name>.bin / <name>.sym. Then DIR/catalog.tsv
lists what was built, one program a line, tab-separated:

  name  kind  hw  variant  load  entry  xram  disk  keys  title  about

  kind     demo, bench or app
  hw       the hardware it was made for (fsim --hw: sim, spartan3, tang, big)
  load     load and start address (hex); entry "-" = the load address
  xram     MB of expansion RAM it needs above the first ("-" = what the hardware has)
  disk     the file service's folder, relative to DIR ("-" = none)
  keys     typed after boot ("\\n" = Enter; "-" = none)

fsim-gui and the web page (web/Makefile) read the catalogue; --json writes DIR/catalog.json too.
A program whose build fails (a missing toolchain or input) is left out with a note."""
import argparse
import importlib.util
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FCC = os.environ.get("FCC", os.path.join(ROOT, "llvm", "fcc"))   # FCC=... for another checkout's toolchain
ASM = os.path.join(HERE, "asm.py")
PY = sys.executable


def sh(cmd, cwd=ROOT):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError((r.stdout + r.stderr).strip().splitlines()[-1:] or ["failed"])
    return r


def fcc(out, srcs, *flags):
    sh([PY, FCC, "-q", "-v", "A", "-O2", "-o", out, *flags, *srcs])
    return out


def asm(out, src):
    sh([PY, ASM, "-q", "-v", "A", src, "-o", out])
    return out


def make(d, target):
    sh(["make", "-C", d, target, f"FCC={FCC}"])
    return os.path.join(ROOT, d, target)


# ---- the programs ----------------------------------------------------------------------------------
# name: (kind, hw, title, about, build(tmp) -> image path, extra fields)
P = {}


def prog(name, kind, hw, title, about, build, **kw):
    P[name] = dict(name=name, kind=kind, hw=hw, title=title, about=about, build=build, **kw)


prog("overdrive", "demo", "tang", "Overdrive", "a high-energy rave demo: plasma, tunnel, dancers, vector balls (apps/overdrive)",
     lambda t: make("apps/overdrive", "build/overdrive.bin"))
prog("future", "demo", "tang", "The Future", "ten minutes from a one-bit screen to the stars (apps/future)",
     lambda t: make("apps/future", "build/future.bin"))
prog("demo80s", "demo", "tang", "demo80s", "the 80s home computers, then the Tang pulling ahead (apps/demo80s)",
     lambda t: make("apps/demo80s", "build/demo80s.bin"))
prog("demo-tang", "demo", "tang", "FPGA demo (Tang)", "the six-section demo of fpga/demo, the Tang build",
     lambda t: fcc(os.path.join(t, "demo-tang.bin"), ["fpga/demo/demo.c"], "-DTANG", "--stack", "0x1000"))
prog("demo-s3", "demo", "spartan3", "FPGA demo (Spartan-3)", "the six-section demo of fpga/demo, for the low tier",
     lambda t: fcc(os.path.join(t, "demo-s3.bin"), ["fpga/demo/demo.c", "fpga/lib/sid.c"], "--stack", "0x20000"))
prog("paint", "app", "spartan3", "Paint", "an 8-colour paint program with windows, for the PS/2 mouse (apps/paint)",
     lambda t: make("apps/paint", "build/paint.bin"))
def with_data(name, d, target, tool, *args):
    """a program's image with its data in RAM after it (the app's own packer: a flat image for fsim)"""
    def build(t):
        img = make(d, target)
        out = os.path.join(t, name + ".bin")
        sh([PY, os.path.join(ROOT, d, tool), *[a.replace("{img}", img).replace("{out}", out) for a in args]])
        if os.path.exists(img[:-4] + ".sym"):
            shutil.copyfile(img[:-4] + ".sym", out[:-4] + ".sym")
        return out
    return build


SIDS = ["apps/sidplay/tunes/" + t + ".sid" for t in ("neondrift", "glassbells", "syncrider")]
prog("sidplay", "app", "tang", "SID tracker", "C64 tunes on the audio engine through a 6502 JIT, as a tracker (apps/sidplay)",
     with_data("sidplay", "apps/sidplay", "build/sidplay.bin", "tools/mkimage.py", "{img}", "{out}",
               *[os.path.join(ROOT, t) for t in SIDS]))


def modplay(t):
    make("apps/demo80s", "src/music_mods.h")          # its two MODs, made by tools/mkmusic.py
    mods = [os.path.join(ROOT, "apps/demo80s/build", m) for m in ("amiga.mod", "finale.mod")]
    return with_data("modplay", "apps/modplay", "build/modplay.bin", "tools/pack.py", "image", "{img}", "{out}",
                     *mods)(t)


prog("modplay", "app", "tang", "MOD tracker", "Amiga MODs on the audio engine, as a tracker: demo80s's two tunes "
     "(apps/modplay)", modplay)
prog("sym40", "app", "tang-mc", "Symphony No. 40", "Mozart for orchestra on the microcoded audio engine, with a "
     "falling-notes visualizer (apps/sym40)", lambda t: make("apps/sym40", "build/sym40.bin"))
prog("toccata", "app", "tang-mc", "Toccata and Fugue", "Bach on a 64-node pipe organ, the microcoded audio engine "
     "(apps/toccata)", lambda t: make("apps/toccata", "build/toccata.bin"))
prog("mcdemo", "demo", "tang-mc", "Microcoded audio", "the microcoded audio engine feature by feature, with its "
     "dataflow on screen (apps/mcdemo)", lambda t: make("apps/mcdemo", "build/mcdemo.bin"))

for n, about in (("video", "every display layer and a double-buffered sprite"),
                 ("hicolor", "the 256-colour palette and the 3-bit dither"),
                 ("spr8", "the 8 bpp sprite buffer")):
    prog(n, "demo", "sim", f"sim/demo/{n}.s", about,
         (lambda n: lambda t: asm(os.path.join(t, n + ".bin"), f"sim/demo/{n}.s"))(n))
for n, about in (("hello", "hello, world"), ("fib", "Fibonacci, recursive"), ("sieve", "the sieve of Eratosthenes"),
                 ("lines", "Bresenham lines"), ("memops", "memory copies and fills"),
                 ("arith32", "32-bit arithmetic"), ("tokenize", "a tokenizer")):
    prog("bench-" + n, "bench", "sim", f"sim/bench/{n}.s", about,
         (lambda n: lambda t: asm(os.path.join(t, "bench-" + n + ".bin"), f"sim/bench/{n}.s"))(n))


def c_benchmarks():
    """The C benchmarks of benchmarks/run.py, built as it builds them (16-bit int where it can)."""
    spec = importlib.util.spec_from_file_location("bench_run", os.path.join(ROOT, "benchmarks", "run.py"))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    m.FCC = FCC
    for b in m.BENCHMARKS:
        if b["kind"] != "c":
            continue
        bits = min(b.get("ints", [16]))
        prog("c-" + b["name"], "bench", "sim", b["name"], b["about"],
             (lambda b, bits: lambda t: (m.fcc(b, "A", bits, os.path.join(t, f"c-{b['name']}.bin")),
                                         os.path.join(t, f"c-{b['name']}.bin"))[1])(b, bits))
    prog("basic", "app", "sim", "BASIC", "the fpga_cpu BASIC interpreter, with its examples on the disk",
         lambda t: make("basic", "build/basic.A.bin"), disk="basic-examples",
         disk_src=os.path.join(ROOT, "basic", "examples"))
    for b in m.BENCHMARKS:
        if b["kind"] == "basic":
            prog("basic-" + b["name"][6:], "bench", "sim", b["name"], b["about"],
                 lambda t: make("basic", "build/basic.A.bin"), disk="basic-bench",
                 disk_src=os.path.join(ROOT, "benchmarks", "basic"), keys=f'RUN "{b["file"]}.BAS"\\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=os.path.join(ROOT, "sim", "build", "programs"))
    ap.add_argument("--list", action="store_true", help="list the programs, build nothing")
    ap.add_argument("--json", action="store_true", help="also write catalog.json (the web page's)")
    ap.add_argument("names", nargs="*")
    a = ap.parse_args()
    try:
        c_benchmarks()
    except Exception as e:  # noqa: BLE001 - the C benchmarks are optional
        print(f"(C and BASIC benchmarks left out: {e})", file=sys.stderr)
    if a.list:
        for p in P.values():
            print(f"{p['name']:24} {p['kind']:6} {p['hw']:9} {p['about']}")
        return
    a.out = os.path.abspath(a.out)                  # the builds run in the repository's root
    os.makedirs(a.out, exist_ok=True)
    tmp = os.path.join(a.out, "tmp")
    os.makedirs(tmp, exist_ok=True)
    rows = []
    for p in P.values():
        if a.names and p["name"] not in a.names:
            continue
        try:
            img = p["build"](tmp)
        except Exception as e:  # noqa: BLE001
            print(f"{p['name']}: not built ({e})", file=sys.stderr)
            continue
        dst = os.path.join(a.out, p["name"] + ".bin")
        shutil.copyfile(img, dst)
        sym = img[:-4] + ".sym"
        if os.path.exists(sym):
            shutil.copyfile(sym, dst[:-4] + ".sym")
        if p.get("disk"):
            dd = os.path.join(a.out, p["disk"])
            shutil.rmtree(dd, ignore_errors=True)
            shutil.copytree(p["disk_src"], dd)
        rows.append(dict(name=p["name"], kind=p["kind"], hw=p["hw"], variant="A", load="1000", entry="-",
                         xram="-", disk=p.get("disk", "-"), keys=p.get("keys", "-"),
                         title=p["title"], about=p["about"]))
        print(f"{p['name']}: {os.path.relpath(dst, ROOT)}")
    shutil.rmtree(tmp, ignore_errors=True)
    cols = ["name", "kind", "hw", "variant", "load", "entry", "xram", "disk", "keys", "title", "about"]
    with open(os.path.join(a.out, "catalog.tsv"), "w") as f:
        f.write("# " + "\t".join(cols) + "  (sim/tools/programs.py)\n")
        for r in rows:
            f.write("\t".join(r[c] for c in cols) + "\n")
    if a.json:
        json.dump(rows, open(os.path.join(a.out, "catalog.json"), "w"), indent=1)
    print(f"{len(rows)} programs in {os.path.relpath(os.path.join(a.out, 'catalog.tsv'), ROOT)}")


if __name__ == "__main__":
    main()
