"""Measure every benchmark on every encoding variant and timing configuration.

  measure.py           full sweep -> sim/build/report.md and docs/measurements.md
  measure.py --quick   correctness only (base configuration)

Every run's output is checked against the benchmark's '; expect:' line,
which comes from sim/tools/bench_ref.py, so each measurement is also a
test of the assembler, decoder and simulator.
"""
import ast
import glob
import json
import math
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SIM = os.path.dirname(HERE)
ROOT = os.path.dirname(SIM)
sys.path.insert(0, HERE)
from isa import Isa  # noqa: E402
from gen import opcode_space  # noqa: E402

FSIM = os.path.join(SIM, "build", "fsim")
OUT = os.path.join(SIM, "build", "bench")
PY = sys.executable

CONFIGS = {
    "base":       ([], "2 KB I-cache, 16 B lines, prefetch, video 7%, no fusion"),
    "fuse":       (["--fuse"], "base + macro-op fusion"),
    "noprefetch": (["--no-prefetch"], "base without the stream buffer"),
    "nocache":    (["--icache", "0"], "no I-cache: every fetch from SRAM"),
    "ic512":      (["--icache", "512"], "512 B I-cache"),
    "ic1k":       (["--icache", "1024"], "1 KB I-cache"),
    "ic4k":       (["--icache", "4096"], "4 KB I-cache"),
    "line8":      (["--line", "8"], "8 B lines"),
    "line32":     (["--line", "32"], "32 B lines"),
    "novideo":    (["--video", "0:0"], "no video traffic"),
    "bp1":        (["--branch-pen", "1"], "1-cycle taken-branch penalty"),
    # options for the small-LUT, fastest-core study
    "fuse-cmpbr": (["--fuse=cmpbr"], "fuse CMP+Bcc only"),
    "fuse-br":    (["--fuse=cmpbr,alubr"], "fuse CMP+Bcc and ALU+Bcc (DEC+BNE = DBNZ)"),
    "dual":       (["--dual"], "dual issue, even/odd write banks"),
    "dual-ports": (["--dual=ports"], "dual issue, full second write port"),
    "dual+br":    (["--dual", "--fuse=cmpbr,alubr"], "dual issue (banked) + branch fusion"),
    "bp1+br":     (["--branch-pen", "1", "--fuse=cmpbr,alubr"], "1-cycle branch + branch fusion"),
    "bp1+dual+br": (["--branch-pen", "1", "--dual", "--fuse=cmpbr,alubr"], "1-cycle branch + dual (banked) + branch fusion"),
    "nocache+br": (["--icache", "0", "--fuse=cmpbr,alubr"], "no I-cache + branch fusion"),
    "nocache+dual": (["--icache", "0", "--dual"], "no I-cache + dual issue"),
    "ic64":       (["--icache", "64"], "64 B I-cache (miss-path stress)"),
    "ic64+br":    (["--icache", "64", "--fuse=cmpbr,alubr"], "64 B I-cache + branch fusion"),
    "ic64+dual+br": (["--icache", "64", "--dual", "--fuse=cmpbr,alubr"], "64 B I-cache + dual + branch fusion"),
}

# Rough extra-LUT estimates for each option on the XC3S200 (LUT4).  These are
# design guesses, not synthesis results: replace them once the RTL exists.
# (low, high, what the LUTs buy)
HW_COST = {
    "bp1":        (30, 80, "resolve taken branches one stage earlier: target adder and redirect in decode"),
    "fuse-cmpbr": (30, 60, "pair detect, flags forwarded straight into the branch decision"),
    "fuse-br":    (40, 80, "as fuse-cmpbr, plus the register write of the ALU half"),
    "fuse":       (150, 250, "all kinds: adds a 32-bit carry across ADD+ADC, LEA into the AGU, paired LDW/STW"),
    "dual":       (300, 450, "slot-2 decoder and 16-bit ALU, two more register-file read copies, "
                             "forwarding, pairing check, flag merge"),
    "dual-ports": (360, 570, "as dual, plus a second full write port (replicated file or LVT)"),
}


def geomean(xs):
    xs = [x for x in xs if x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else 0.0


def expected(src):
    m = re.search(r"^; expect: (.*)$", open(src).read(), re.M)
    return ast.literal_eval(m.group(1)) if m else None


def assemble(src, v):
    name = os.path.splitext(os.path.basename(src))[0]
    binp = os.path.join(OUT, f"{name}.{v}.bin")
    stp = os.path.join(OUT, f"{name}.{v}.json")
    r = subprocess.run([PY, os.path.join(HERE, "asm.py"), "-v", v, src, "-o", binp, "--stats", stp,
                        "--lst", os.path.join(OUT, f"{name}.{v}.lst")], capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"assembly failed: {src} [{v}]\n{r.stdout}{r.stderr}")
    return binp, json.load(open(stp))


def run(binp, v, st, cfg):
    js = binp.replace(".bin", f".{cfg}.run.json")
    args = [FSIM, "-q", "-v", v, "--load", str(st["base"]), "--entry", str(st["entry"]), "--json", js,
            *CONFIGS[cfg][0], binp]
    r = subprocess.run(args, capture_output=True, text=True)
    return r.stdout, r.returncode, json.load(open(js))


def main():
    quick = "--quick" in sys.argv
    os.makedirs(OUT, exist_ok=True)
    isa = Isa()
    variants = list(isa.variants)
    benches = sorted(b for b in glob.glob(os.path.join(SIM, "bench", "*.s")) if not b.endswith("lib.s"))
    configs = ["base"] if quick else list(CONFIGS)
    res = {}      # (bench, variant, cfg) -> run stats
    static = {}   # (bench, variant) -> asm stats
    fails = []
    for src in benches:
        name = os.path.splitext(os.path.basename(src))[0]
        exp = expected(src)
        for v in variants:
            binp, st = assemble(src, v)
            static[name, v] = st
            for cfg in configs:
                out, rc, rs = run(binp, v, st, cfg)
                res[name, v, cfg] = rs
                ok = rc == 0 and out == exp
                if not ok:
                    fails.append(f"{name} [{v}/{cfg}]: rc={rc} {rs['halt']!r} out={out!r} expected={exp!r}")
        status = "ok" if not any(f.startswith(name + " ") for f in fails) else "FAIL"
        print(f"{name:10} {status}")
    if fails:
        print("\n".join(fails))
        sys.exit(1)
    print(f"all {len(benches)} benchmarks correct on variants {', '.join(variants)} "
          f"({len(configs)} configuration{'s' if len(configs) > 1 else ''})")
    if quick:
        return
    names = [os.path.splitext(os.path.basename(b))[0] for b in benches]
    report = build_report(isa, variants, names, static, res)
    for path in (os.path.join(SIM, "build", "report.md"), os.path.join(ROOT, "docs", "measurements.md")):
        with open(path, "w") as fh:
            fh.write(report)
    print("report: sim/build/report.md, docs/measurements.md")


def pct(a, b):
    return 100.0 * a / b if b else 0.0


def build_report(isa, variants, names, static, res):
    A, B = variants[0], variants[1]
    L = ["# Measurements", "",
         "Generated by `sim/tools/measure.py`. Do not edit; rerun `make -C sim measure`.", "",
         "The benchmarks are hand-written assembly (`sim/bench`). They show how the encodings and the",
         "timing model behave, but they are small and fit in any I-cache. Code-size and I-cache",
         "conclusions need compiled code: the BASIC interpreter via the LLVM backend.", "",
         "CPU and SRAM share one 50 MHz clock. Every run was checked against the Python reference results.", ""]
    # opcode space
    L += ["## Opcode space", "", "| variant | 16-bit share | 16-bit used | 32-bit share | 32-bit used | encodings |",
          "|---|---|---|---|---|---|"]
    for v in variants:
        nl, nlu, ns, nsu = opcode_space(isa.variants[v])
        L.append(f"| {v} | {pct(ns, 65536):.1f}% | {pct(nsu, ns):.1f}% | {pct(nl, 65536):.1f}% | "
                 f"{pct(nlu, nl):.1f}% | {len(isa.variants[v].forms)} |")
    L.append("")
    # static size
    L += ["## Static code size", "",
          f"| benchmark | {A} bytes | {A} 16-bit | {B} bytes | {B} 16-bit | {B}/{A} | {B} expanded |",
          "|---|---|---|---|---|---|---|"]
    ta = tb = 0
    for n in names:
        sa, sb = static[n, A], static[n, B]
        ta += sa["code_bytes"]
        tb += sb["code_bytes"]
        L.append(f"| {n} | {sa['code_bytes']} | {pct(sa['len16'], sa['insts']):.0f}% | {sb['code_bytes']} | "
                 f"{pct(sb['len16'], sb['insts']):.0f}% | {sb['code_bytes'] / sa['code_bytes']:.3f} | {sb['expanded']} |")
    L += [f"| **total** | **{ta}** | | **{tb}** | | **{tb / ta:.3f}** | |", ""]
    # dynamic
    L += ["## Dynamic performance (base configuration)", "",
          f"| benchmark | insts | {A} cycles | {A} CPI | {A} 16-bit exec | {B} cycles | {B} CPI | {B}/{A} |",
          "|---|---|---|---|---|---|---|---|"]
    ratios = []
    for n in names:
        ra, rb = res[n, A, "base"], res[n, B, "base"]
        ratios.append(rb["cycles"] / ra["cycles"])
        L.append(f"| {n} | {ra['insts']} | {ra['cycles']} | {ra['cycles'] / ra['insts']:.3f} | "
                 f"{pct(ra['len16'], ra['insts']):.0f}% | {rb['cycles']} | {rb['cycles'] / rb['insts']:.3f} | "
                 f"{ratios[-1]:.4f} |")
    L += [f"| **geomean** | | | | | | | **{geomean(ratios):.4f}** |", ""]
    # stalls
    L += [f"## Where the cycles go (variant {A}, base)", "",
          "| benchmark | issue | ifetch | taken branch | load-use | video | I/O | pair split |",
          "|---|---|---|---|---|---|---|---|"]
    for n in names:
        r = res[n, A, "base"]
        s, c = r["stalls"], r["cycles"]
        issue = c - sum(s.values())
        L.append(f"| {n} | {pct(issue, c):.0f}% | {pct(s['ifetch'], c):.1f}% | {pct(s['branch'], c):.1f}% | "
                 f"{pct(s['loaduse'], c):.1f}% | {pct(s['video'], c):.1f}% | {pct(s['io'], c):.1f}% | "
                 f"{pct(s['pair_split'], c):.1f}% |")
    L.append("")
    # fusion
    fnames = [k for k in res[names[0], A, "base"]["fusion"] if k != "applied"]
    for v in variants:
        L += [f"## Fusion candidates (variant {v}), per 100 instructions", "",
              "| benchmark | " + " | ".join(fnames) + " | speedup with fusion |",
              "|---|" + "---|" * len(fnames) + "---|"]
        sp = []
        for n in names:
            r, rf = res[n, v, "base"], res[n, v, "fuse"]
            sp.append(r["cycles"] / rf["cycles"])
            L.append(f"| {n} | " + " | ".join(f"{pct(r['fusion'][k], r['insts']):.1f}" for k in fnames) +
                     f" | {sp[-1]:.3f}x |")
        L += [f"| **geomean** | " + " | " * (len(fnames) - 1) + f" | **{geomean(sp):.3f}x** |", ""]
    # dual issue
    L += [f"## Dual issue (variant {A})", "",
          "Slot 2 takes simple ALU ops, MOVI or a branch; slot 1 takes anything but a control transfer. "
          "Paired instructions must be independent (no register or flag dependence). Banked: the two "
          "must write opposite even/odd register banks.", "",
          "| benchmark | paired, banked | paired, ports | slot 2 alu / movi / branch | top blockers (banked) | "
          "speedup banked | speedup ports | dual + branch fusion |",
          "|---|---|---|---|---|---|---|---|"]
    spb, spp, spd = [], [], []
    for n in names:
        rb, rp, rd, r0 = res[n, A, "dual"], res[n, A, "dual-ports"], res[n, A, "dual+br"], res[n, A, "base"]
        du = rb["dual"]
        sl = du["slot2"]
        tot = max(1, du["paired"])
        blk = sorted(du["blocked"].items(), key=lambda kv: -kv[1])[:2]
        bl = ", ".join(f"{k} {pct(x, r0['insts']):.0f}%" for k, x in blk if x)
        spb.append(r0["cycles"] / rb["cycles"])
        spp.append(r0["cycles"] / rp["cycles"])
        spd.append(r0["cycles"] / rd["cycles"])
        L.append(f"| {n} | {pct(du['paired'], rb['insts']):.1f}% | {pct(rp['dual']['paired'], rp['insts']):.1f}% | "
                 f"{pct(sl['alu'], tot):.0f} / {pct(sl['movi'], tot):.0f} / {pct(sl['branch'], tot):.0f}% | {bl} | "
                 f"{spb[-1]:.3f}x | {spp[-1]:.3f}x | {spd[-1]:.3f}x |")
    L += [f"| **geomean** | | | | | **{geomean(spb):.3f}x** | **{geomean(spp):.3f}x** | **{geomean(spd):.3f}x** |",
          "", "Blockers are counted per instruction: window = the pair does not fit the fetch window, "
          "slot1/slot2 = an instruction class the slot cannot take, raw/waw = register dependence, "
          "bank = both write the same bank, flags = slot 2 reads flags that slot 1 writes.", ""]
    # SRAM port
    vfrac = 4 / 56
    pcfgs = ["base", "fuse-br", "dual+br", "ic64", "ic64+dual+br", "nocache", "nocache+br", "nocache+dual"]
    L += [f"## SRAM port use (variant {A}, all benchmarks)", "",
          "Share of all cycles. Video is the fixed scan-out reservation (4 of every 56 cycles). "
          "Demand is the instruction bytes the core consumes per cycle; the port can supply 4.", "",
          "| configuration | instruction fetch | data | video | idle | demand B/cycle | cycles vs base |",
          "|---|---|---|---|---|---|---|"]
    for cfg in pcfgs:
        cyc = sum(res[n, A, cfg]["cycles"] for n in names)
        fw = sum(res[n, A, cfg]["port"]["ifetch_words"] for n in names)
        dw = sum(res[n, A, cfg]["data_words"] for n in names)
        fb = sum(res[n, A, cfg]["port"]["fetch_bytes"] for n in names)
        rel = geomean([res[n, A, cfg]["cycles"] / res[n, A, "base"]["cycles"] for n in names])
        idle = max(0.0, 100 - pct(fw, cyc) - pct(dw, cyc) - 100 * vfrac)
        L.append(f"| {cfg} | {pct(fw, cyc):.1f}% | {pct(dw, cyc):.1f}% | {100 * vfrac:.1f}% | {idle:.1f}% | "
                 f"{fb / cyc:.2f} | {rel:.3f} |")
    L += ["", "The benchmarks fit in a 64 B cache except `tokenize` and `hello`, so the miss path is only "
          "really exercised by the no-cache rows until compiled BASIC is measured.", ""]
    # cost-effectiveness
    L += [f"## Speed per LUT (variant {A}, estimates)", "",
          "LUT figures are design estimates in `sim/tools/measure.py`, not synthesis results. "
          "Speedup is the geomean over benchmarks against base.", "",
          "| option | speedup | extra LUTs (est.) | speedup % per 100 LUTs | what the LUTs buy |",
          "|---|---|---|---|---|"]
    for cfg, (lo, hi, what) in HW_COST.items():
        sp = geomean([res[n, A, "base"]["cycles"] / res[n, A, cfg]["cycles"] for n in names])
        mid = (lo + hi) / 2
        L.append(f"| {cfg} | {sp:.3f}x | {lo}–{hi} | {100 * (sp - 1) / mid * 100:.1f} | {what} |")
    L.append("")
    # config sweep
    sweep = [c for c in CONFIGS if c not in ("base",)]
    L += ["## Configuration sweep: cycles relative to base (geomean over benchmarks)", "",
          "| configuration | description | " + " | ".join(variants) + " |",
          "|---|---|" + "---|" * len(variants)]
    for cfg in sweep:
        cells = [f"{geomean([res[n, v, cfg]['cycles'] / res[n, v, 'base']['cycles'] for n in names]):.3f}"
                 for v in variants]
        L.append(f"| {cfg} | {CONFIGS[cfg][1]} | " + " | ".join(cells) + " |")
    L.append("")
    # I-cache miss rate
    L += ["## I-cache misses per 1000 instructions", "",
          "| benchmark | " + " | ".join(f"{v} {c}" for v in variants for c in ("ic512", "ic1k", "base")) + " |",
          "|---|" + "---|" * (3 * len(variants))]
    for n in names:
        cells = []
        for v in variants:
            for c in ("ic512", "ic1k", "base"):
                r = res[n, v, c]
                cells.append(f"{1000 * r['icache']['miss'] / r['insts']:.2f}")
        L.append(f"| {n} | " + " | ".join(cells) + " |")
    L.append("")
    # top encodings
    for v in variants:
        tot = {}
        all_insts = 0
        for n in names:
            r = res[n, v, "base"]
            all_insts += r["insts"]
            for k, x in r["forms"].items():
                tot[k] = tot.get(k, 0) + x
        L += [f"## Most executed encodings (variant {v}, all benchmarks)", "",
              "| encoding | share |", "|---|---|"]
        for k, x in sorted(tot.items(), key=lambda kv: -kv[1])[:15]:
            L.append(f"| `{k}` | {pct(x, all_insts):.1f}% |")
        L.append("")
    return "\n".join(L)


if __name__ == "__main__":
    main()
