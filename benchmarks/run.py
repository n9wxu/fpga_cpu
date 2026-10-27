"""Build the benchmarks for fpga_cpu, run them in the simulator, check
their output and record what they cost.

  run.py                         every benchmark except Doom, standard configs
  run.py dhrystone sieve         only these (names, or prefixes like stanford)
  run.py --doom                  add Doom (minutes per run)
  run.py --configs base,fuse-br  timing configurations (see CONFIGS)
  run.py --variants A            encoding variants
  run.py --quick                 base configuration only
  run.py --list                  list the benchmarks

Every benchmark is also built for the development machine, and the
simulator's output must match it: exactly, or within a tolerance for
floating-point programs (float and double are both single precision on
fpga_cpu). Lines that report timing are left out of the comparison. C
benchmarks are built with a 16-bit int (fpga_cpu's default) and/or a
32-bit int (fcc --int32), as the program allows. BASIC benchmarks run in
the BASIC interpreter built for fpga_cpu.

Writes results.json (everything) and results.md (tables) next to this file.
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")
FCC = os.path.join(ROOT, "llvm", "fcc")
FSIM = os.path.join(ROOT, "sim", "build", "fsim")
BASIC = os.path.join(ROOT, "basic")
PY = os.path.join(ROOT, ".venv", "bin", "python")
MHZ = 50.0

# Timing configurations, named as in sim/tools/measure.py.
CONFIGS = {
    "base":     ([], "2 KB I-cache, 2-cycle taken branch, no fusion"),
    "fuse-br":  (["--fuse=cmpbr,alubr"], "fuse CMP+Bcc and ALU+Bcc"),
    "bp1+br":   (["--branch-pen", "1", "--fuse=cmpbr,alubr"], "1-cycle taken branch + branch fusion"),
    "dual+br":  (["--dual", "--fuse=cmpbr,alubr"], "dual issue (banked) + branch fusion"),
    "fuse":     (["--fuse"], "every fusion kind"),
    "nocache":  (["--icache", "0"], "no I-cache"),
    "ic512":    (["--icache", "512"], "512 B I-cache"),
    "ic4k":     (["--icache", "4096"], "4 KB I-cache"),
}
DEFAULT_CONFIGS = ["base", "fuse-br", "bp1+br", "dual+br", "nocache", "ic512"]

C = os.path.join(HERE, "c")


def stanford(name, iters, floaty=False):
    return dict(name=f"stanford-{name.lower()}", kind="c", srcs=[f"{C}/stanford/{name}.c"],
                defs=[f"ITERS={iters}"], ints=[16, 32], compare="float" if floaty else "exact",
                about=f"Stanford {name} (Hennessy), {iters} iterations")


BENCHMARKS = [
    dict(name="dhrystone", kind="c", srcs=[f"{C}/dhrystone/dhry_1.c", f"{C}/dhrystone/dhry_2.c"],
         defs=["MSC_CLOCK", "FCPU_EXACT_TIMER", "NUMBER_OF_RUNS=10000"], cflags=["-std=gnu89"],
         host_defs=["CLK_TCK=CLOCKS_PER_SEC"], ints=[16, 32], compare="exact",
         ignore=r"Ptr_Comp|Microseconds|Dhrystones per Second|DMIPS|time too small|increase number",
         metric=("DMIPS", r"DMIPS[^:]*:\s*([\d.]+)"),
         about="Dhrystone 2.1 (Weicker 1988), 10,000 runs; DMIPS = Dhrystones/s / 1757"),
    dict(name="whetstone", kind="c", srcs=[f"{C}/whetstone/whetstone.c"],
         defs=["LOOPS=4", "FCPU_EXACT_TIMER"], ints=[16, 32], compare="float",
         ignore=r"Duration|KIPS|MIPS", metric=("KIPS", r"Whetstones:\s*([\d.]+)\s*KIPS"),
         about="Whetstone (Curnow & Wichmann 1972; C by Painter 1998), soft float, LOOP=4"),
    dict(name="richards", kind="c", srcs=[f"{C}/richards/richards.c"], defs=["CLASSIC_SIZE"],
         ints=[16, 32], compare="exact", about="Richards (Martin Richards), the original Count = 10,000"),
    dict(name="sieve", kind="c", srcs=[f"{C}/sieve/sieve.c"], defs=["ITERS=10"], ints=[16, 32],
         compare="exact", about="BYTE Sieve of Eratosthenes (Gilbreath 1981), 10 iterations"),
    stanford("Bubblesort", 3), stanford("IntMM", 2), stanford("Perm", 3), stanford("Puzzle", 1),
    stanford("Queens", 10), stanford("Quicksort", 4), stanford("Towers", 4), stanford("Treesort", 2),
    stanford("FloatMM", 2, True), stanford("RealMM", 1, True), stanford("Oscar", 1, True),
    dict(name="coremark", kind="c",
         srcs=[f"{C}/coremark/{f}.c" for f in ("core_list_join", "core_main", "core_matrix", "core_state",
                                               "core_util")] + [f"{C}/coremark/fcpu/core_portme.c"],
         incs=[f"{C}/coremark", f"{C}/coremark/fcpu"], defs=["ITERATIONS=10", 'FLAGS_STR="-O2"'],
         ints=[16, 32], compare="check", require=r"Correct operation validated",
         ignore=r".", metric=("CoreMark/MHz", None),
         about="CoreMark 1.0 (EEMBC 2009), 10 iterations: an ISA measurement, not a reportable score"),
    dict(name="basic-dhrystone", kind="basic", file="DHRY", compare="exact",
         ignore=r"CYCLES|DHRYSTONES PER|DMIPS|FRAMES", metric=("DMIPS", r"DMIPS:\s*([\d.E+-]+)"),
         about="Dhrystone 2.1 translated to BASIC, 100 runs, in the fpga_cpu BASIC interpreter"),
] + [dict(name=f"basic-bm{n}", kind="basic", file=f"BM{n}", compare="exact", ignore=r"CYCLES",
          metric=("cycles", r"CYCLES:\s*([\d.E+]+)"),
          about=f"Rugg/Feldman BASIC benchmark {n} (Kilobaud 1977)") for n in range(1, 9)]

DOOM = dict(name="doom", kind="doom", ints=[32], compare="doom", tics=100,
            metric=("fps", r"\(([\d.]+) fps\)"),
            about="Doom (doomgeneric), -timedemo demo1 for 100 gametics, 32-bit int, expansion RAM")


def sh(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def fail(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


# ---- building ----

def fcc(b, variant, bits, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cmd = [PY, FCC, "-q", "-v", variant, "-O2", "--stats", "-o", out]
    if bits == 32:
        cmd.append("--int32")
    cmd += [f"-D{d}" for d in b.get("defs", [])] + [f"-I{i}" for i in b.get("incs", [])]
    cmd += [f"--cflag={f}" for f in b.get("cflags", []) + ["-w"]]
    cmd += b.get("fcc_extra", []) + b["srcs"]
    r = sh(cmd)
    if r.returncode:
        raise RuntimeError(f"fcc failed for {b['name']} [{variant}, int{bits}]:\n{r.stdout}{r.stderr}")
    return json.load(open(out[:-4] + ".json"))


def host_build(b, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    cmd = ["cc", "-O1", "-w", "-funsigned-char", *b.get("cflags", []), "-o", out]
    cmd += [f"-D{d}" for d in b.get("defs", []) + b.get("host_defs", [])]
    cmd += [f"-I{i}" for i in b.get("incs", [])] + b["srcs"] + ["-lm"]
    r = sh(cmd)
    if r.returncode:
        raise RuntimeError(f"host build failed for {b['name']}:\n{r.stderr}")


# ---- comparing output ----

NUM = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


def keep(text, ignore):
    lines = [l.rstrip() for l in text.replace("\r", "").splitlines()]
    return [l for l in lines if l and not (ignore and re.search(ignore, l))]


def same_float(a, b):
    na, nb = NUM.findall(a), NUM.findall(b)
    if NUM.sub("#", a) != NUM.sub("#", b) or len(na) != len(nb):
        return False
    for x, y in zip(na, nb):
        x, y = float(x), float(y)
        if abs(x - y) > 2e-2 * max(abs(x), abs(y)) and abs(x - y) > 1e-3:
            return False
    return True


def compare(b, got, want):
    if b["compare"] == "check":
        return bool(re.search(b["require"], got)) and "ERROR!" not in got.replace(
            "ERROR! Must execute for at least 10 secs", "")
    g, w = keep(got, b.get("ignore")), keep(want, b.get("ignore"))
    if len(g) != len(w):
        return False
    if b["compare"] == "float":
        return all(x == y or same_float(x, y) for x, y in zip(g, w))
    return g == w


# ---- running ----

def fsim(binp, variant, cfg, base=None, xram=0, disk=None, stdin=None, max_cycles=20_000_000_000):
    js = binp[:-4] + f".{cfg}.run.json"
    cmd = [FSIM, "-q", "-v", variant, "--json", js, "--max-cycles", str(max_cycles), *CONFIGS[cfg][0]]
    if base is not None:
        cmd += ["--load", hex(base)]
    if xram:
        cmd += ["--xram", str(xram)]
    if disk:
        cmd += ["--disk", disk]
    r = subprocess.run(cmd + [binp], input=stdin, capture_output=True, text=True)
    st = json.load(open(js))
    return r.stdout, r.returncode, st


def metric(b, out, st):
    if not b.get("metric"):
        return None
    name, rx = b["metric"]
    if b["name"] == "coremark":
        m = re.search(r"Iterations\s*:\s*(\d+)", out)
        t = re.search(r"Total ticks\s*:\s*(\d+)", out)
        if m and t and int(t.group(1)):
            return int(m.group(1)) / (int(t.group(1)) / (MHZ * 1e6)) / MHZ
        return None
    m = re.search(rx, out)
    return float(m.group(1)) if m else None


def record(b, variant, bits, cfg, out, rc, st, want, stats):
    ok = rc == 0 and compare(b, out, want) if want is not None else rc == 0
    return dict(bench=b["name"], variant=variant, int=bits, config=cfg, ok=ok, rc=rc,
                cycles=st["cycles"], insts=st["insts"], len16=st["len16"],
                code_bytes=stats.get("code_bytes") if stats else None,
                metric=metric(b, out, st), halt=st["halt"], output=out[-4000:])


def run_c(b, variants, configs):
    rows = []
    want = None
    if b["compare"] != "check":
        hb = os.path.join(BUILD, "host", b["name"])
        host_build(b, hb)
        r = sh([hb], timeout=600)
        want = r.stdout
    else:
        want = ""
    for bits in b["ints"]:
        for v in variants:
            binp = os.path.join(BUILD, b["name"], f"{b['name']}.int{bits}.{v}.bin")
            stats = fcc(b, v, bits, binp)
            for cfg in configs:
                out, rc, st = fsim(binp, v, cfg)
                rows.append(record(b, v, bits, cfg, out, rc, st, want, stats))
    return rows


def basic_bins(variants):
    bins = {}
    for v in variants:
        p = os.path.join(BASIC, "build", f"basic-test.{v}.bin")
        r = sh(["make", "-C", BASIC, f"build/basic-test.{v}.bin"])
        if r.returncode:
            raise RuntimeError(f"BASIC target build failed [{v}]:\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
        bins[v] = p
    host = os.path.join(BASIC, "build", "basic-host")
    if sh(["make", "-C", BASIC, "build/basic-host"]).returncode:
        raise RuntimeError("BASIC host build failed")
    return bins, host


def run_basic(b, variants, configs, bins, host):
    disk = os.path.join(HERE, "basic")
    cmd = f'RUN "{b["file"]}"\n'
    env = dict(os.environ, BASIC_DISK=disk, BASIC_FASTFRAMES="1")
    want = subprocess.run([host, "-q"], input=cmd, capture_output=True, text=True, env=env).stdout
    rows = []
    for v in variants:
        stats = json.load(open(bins[v][:-4] + ".json")) if os.path.exists(bins[v][:-4] + ".json") else {}
        for cfg in configs:
            # a scratch copy of the binary per run so the run JSONs don't collide
            binp = os.path.join(BUILD, b["name"], f"{b['name']}.{v}.bin")
            os.makedirs(os.path.dirname(binp), exist_ok=True)
            shutil.copyfile(bins[v], binp)
            out, rc, st = fsim(binp, v, cfg, disk=disk, stdin=cmd)
            rows.append(record(b, v, 16, cfg, out, rc, st, want, stats))
    return rows


DOOMDIR = os.path.join(HERE, "doom")
DOOM_DEFS = ["CMAP256", "DOOMGENERIC_RESX=640", "DOOMGENERIC_RESY=360", "NORMALUNIX"]
DOOM_BMP = 0x0C3C00


def doom_wad():
    r = sh([os.path.join(DOOMDIR, "get_wad.sh")])
    if r.returncode:
        raise RuntimeError(f"no IWAD for Doom: {r.stdout}{r.stderr}\n"
                           "Copy the shareware DOOM1.WAD to benchmarks/doom/wad/, or let "
                           "get_wad.sh fetch Freedoom.")
    for n in ("DOOM1.WAD", "DOOM.WAD"):
        if os.path.exists(os.path.join(DOOMDIR, "wad", n)):
            return n


def run_doom(b, variants, configs):
    wad = doom_wad()
    srcs = sorted(os.path.join(DOOMDIR, "src", f) for f in os.listdir(os.path.join(DOOMDIR, "src"))
                  if f.endswith(".c"))
    args = f"-timedemo demo1 -timedemotics {b['tics']}"
    # host reference: the same engine and demo; its last frame as palette indices
    hdir = os.path.join(BUILD, "doom", "host")
    os.makedirs(hdir, exist_ok=True)
    hb = os.path.join(hdir, "doom-host")
    r = sh(["cc", "-O2", "-w", *[f"-D{d}" for d in DOOM_DEFS], "-DLINUX", "-D_DEFAULT_SOURCE",
            f"-I{DOOMDIR}/src", *srcs, os.path.join(DOOMDIR, "port", "doomgeneric_host.c"), "-o", hb, "-lm"])
    if r.returncode:
        raise RuntimeError(f"Doom host build failed:\n{r.stderr[-3000:]}")
    link = os.path.join(hdir, wad)
    if not os.path.exists(link):
        os.symlink(os.path.join(DOOMDIR, "wad", wad), link)
    want = sh([hb, "-iwad", wad, *args.split()], cwd=hdir, timeout=900).stdout
    host_frame = open(os.path.join(hdir, "LASTFRAME.IDX"), "rb").read()
    # the simulator's disk: the IWAD and the arguments
    disk = os.path.join(BUILD, "doom", "disk")
    os.makedirs(disk, exist_ok=True)
    if not os.path.exists(os.path.join(disk, wad)):
        os.symlink(os.path.join(DOOMDIR, "wad", wad), os.path.join(disk, wad))
    with open(os.path.join(disk, "DOOMARGS.TXT"), "w") as f:
        f.write(args + "\n")
    b = dict(b, srcs=srcs + [os.path.join(DOOMDIR, "port", "doomgeneric_fcpu.c")],
             incs=[f"{DOOMDIR}/src", f"{DOOMDIR}/port/include"], defs=DOOM_DEFS,
             fcc_extra=["--base", "0x100000", "--stack", "0xF00000"])
    rows = []
    for v in variants:
        binp = os.path.join(BUILD, "doom", f"doom.int32.{v}.bin")
        stats = fcc(b, v, 32, binp)
        for cfg in configs:
            dump = binp[:-4] + f".{cfg}.frame"
            out, rc, st = fsim_doom(binp, v, cfg, disk, dump)
            ok_text = re.search(r"timed (\d+) gametics", out)
            frame = open(dump, "rb").read() if os.path.exists(dump) else b""
            sim_frame = b"".join(frame[y * 640 + 160:y * 640 + 480] for y in range(200)) if frame else b""
            row = record(b, v, 32, cfg, out, rc, st, None, stats)
            row["ok"] = bool(ok_text and ok_text.group(1) == str(b["tics"]) and
                             f"timed {b['tics']} gametics" in want and sim_frame == host_frame)
            row["frame_match"] = sim_frame == host_frame
            row["frames"] = b["tics"]
            rows.append(row)
    return rows


def fsim_doom(binp, variant, cfg, disk, dump):
    js = binp[:-4] + f".{cfg}.run.json"
    addr = DOOM_BMP + 80 * 640
    cmd = [FSIM, "-q", "-v", variant, "--json", js, "--max-cycles", "0", "--xram", "14",
           "--load", "0x100000", "--disk", disk, "--dump", f"{addr:#x}:{200 * 640}:{dump}",
           *CONFIGS[cfg][0], binp]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.stdout + r.stderr, r.returncode, json.load(open(js))


# ---- report ----

def fmt(x, d=0):
    return "–" if x is None else f"{x:,.{d}f}"


def report(rows, variants, configs):
    by = {(r["bench"], r["variant"], r["int"], r["config"]): r for r in rows}
    names = list(dict.fromkeys(r["bench"] for r in rows))
    about = {b["name"]: b["about"] for b in BENCHMARKS + [DOOM]}
    v0 = variants[0]
    L = ["# Benchmark results", "",
         "Generated by `benchmarks/run.py`; do not edit. The CPU runs at 50 MHz in the simulator, ",
         "and every run's output was checked against a build of the same program for the development "
         "machine (✓) unless marked ✗.", "",
         f"## Summary (variant {v0}, base configuration)", "",
         "| benchmark | int | ok | cycles | instructions | CPI | 16-bit instr. | code bytes | result |",
         "|---|---|---|---|---|---|---|---|---|"]
    for n in names:
        for bits in (16, 32):
            r = by.get((n, v0, bits, "base"))
            if not r:
                continue
            res = ""
            if r["metric"] is not None:
                b = next(b for b in BENCHMARKS + [DOOM] if b["name"] == n)
                res = f"{r['metric']:.3f} {b['metric'][0]}" if r["metric"] < 1000 else f"{r['metric']:,.0f} {b['metric'][0]}"
            L.append(f"| {n} | {bits} | {'✓' if r['ok'] else '✗'} | {fmt(r['cycles'])} | {fmt(r['insts'])} | "
                     f"{r['cycles'] / r['insts']:.3f} | {100 * r['len16'] / r['insts']:.0f}% | "
                     f"{fmt(r['code_bytes'])} | {res} |")
    L.append("")
    pairs = [(n, by.get((n, v0, 16, "base")), by.get((n, v0, 32, "base"))) for n in names]
    pairs = [(n, a, b) for n, a, b in pairs if a and b]
    if pairs:
        L += [f"## 16-bit int against 32-bit int (variant {v0}, base)", "",
              "The same C source compiled with each `int` width. Ratios are 32-bit / 16-bit.", "",
              "| benchmark | cycles int16 | cycles int32 | cycle ratio | code ratio | both outputs correct |",
              "|---|---|---|---|---|---|"]
        for n, a, b in pairs:
            cr = b["code_bytes"] / a["code_bytes"] if a["code_bytes"] and b["code_bytes"] else None
            L.append(f"| {n} | {fmt(a['cycles'])} | {fmt(b['cycles'])} | {b['cycles'] / a['cycles']:.3f} | "
                     f"{fmt(cr, 3)} | {'yes' if a['ok'] and b['ok'] else 'int16 ✗' if b['ok'] else '✗'} |")
        L.append("")
    if len(variants) > 1:
        L += ["## Encoding variant B against A (base)", "",
              "| benchmark | int | code bytes A | code bytes B | B/A code | B/A cycles |", "|---|---|---|---|---|---|"]
        for n in names:
            for bits in (16, 32):
                a, b = by.get((n, "A", bits, "base")), by.get((n, "B", bits, "base"))
                if a and b:
                    cr = b["code_bytes"] / a["code_bytes"] if a["code_bytes"] and b["code_bytes"] else None
                    L.append(f"| {n} | {bits} | {fmt(a['code_bytes'])} | {fmt(b['code_bytes'])} | {fmt(cr, 3)} | "
                             f"{b['cycles'] / a['cycles']:.3f} |")
        L.append("")
    others = [c for c in configs if c != "base"]
    if others:
        L += [f"## Speedup over base by configuration (variant {v0})", "",
              "| benchmark | int | " + " | ".join(others) + " |", "|---|---|" + "---|" * len(others)]
        import math
        logs = {c: [] for c in others}
        for n in names:
            for bits in (16, 32):
                base = by.get((n, v0, bits, "base"))
                if not base:
                    continue
                cells = []
                for c in others:
                    r = by.get((n, v0, bits, c))
                    if r:
                        s = base["cycles"] / r["cycles"]
                        logs[c].append(math.log(s))
                        cells.append(f"{s:.3f}×")
                    else:
                        cells.append("–")
                L.append(f"| {n} | {bits} | " + " | ".join(cells) + " |")
        L.append("| **geomean** | | " + " | ".join(
            f"**{math.exp(sum(l) / len(l)):.3f}×**" if l else "–" for l in logs.values()) + " |")
        L += ["", "Configurations: " + "; ".join(f"`{c}` {CONFIGS[c][1]}" for c in configs), ""]
    bad = [r for r in rows if not r["ok"]]
    L += ["## Benchmarks", ""] + [f"- **{n}**: {about[n]}" for n in names] + [""]
    if bad:
        L += ["## Runs that failed their check", ""]
        for r in bad:
            L.append(f"- {r['bench']} [{r['variant']}, int{r['int']}, {r['config']}]: {r['halt']}")
        L.append("")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("names", nargs="*")
    ap.add_argument("--configs", default=",".join(DEFAULT_CONFIGS))
    ap.add_argument("--variants", default="A,B")
    ap.add_argument("--quick", action="store_true")
    ap.add_argument("--doom", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    a = ap.parse_args()
    benches = BENCHMARKS + ([DOOM] if a.doom or "doom" in a.names else [])
    if a.list:
        for b in BENCHMARKS + [DOOM]:
            print(f"{b['name']:20} {b['about']}")
        return
    if a.names:
        benches = [b for b in benches if any(b["name"] == n or b["name"].startswith(n) for n in a.names)]
    configs = ["base"] if a.quick else a.configs.split(",")
    variants = a.variants.split(",")
    for c in configs:
        if c not in CONFIGS:
            fail(f"unknown configuration {c}")
    for tool in (FCC, FSIM):
        if not os.path.exists(tool):
            fail(f"missing {tool}: build the toolchain (llvm/setup.sh) and the simulator (make -C sim)")
    os.makedirs(BUILD, exist_ok=True)
    t0 = time.time()
    bins = host = None
    if any(b["kind"] == "basic" for b in benches):
        bins, host = basic_bins(variants)
    rows = []
    with cf.ThreadPoolExecutor(max_workers=a.j) as ex:
        futs = {}
        for b in benches:
            if b["kind"] == "c":
                futs[ex.submit(run_c, b, variants, configs)] = b
            elif b["kind"] == "basic":
                futs[ex.submit(run_basic, b, variants, configs, bins, host)] = b
            else:
                futs[ex.submit(run_doom, b, variants, configs)] = b
        for f in cf.as_completed(futs):
            b = futs[f]
            try:
                rs = f.result()
            except Exception as e:  # report and carry on with the rest
                print(f"{b['name']:20} ERROR {e}")
                continue
            rows += rs
            good = sum(r["ok"] for r in rs)
            print(f"{b['name']:20} {good}/{len(rs)} runs correct")
    order = {b["name"]: i for i, b in enumerate(BENCHMARKS + [DOOM])}
    rows.sort(key=lambda r: (order[r["bench"]], r["int"], r["variant"], configs.index(r["config"])))
    with open(os.path.join(HERE, "results.json"), "w") as f:
        json.dump(dict(configs=configs, variants=variants, rows=rows), f, indent=1)
    with open(os.path.join(HERE, "results.md"), "w") as f:
        f.write(report(rows, variants, configs))
    bad = sum(not r["ok"] for r in rows)
    print(f"{len(rows)} runs, {bad} failed checks, {time.time() - t0:.0f} s; results.md written")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
