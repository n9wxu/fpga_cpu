#!/usr/bin/env python3
"""Bit-exactness of sim/src/mcaud.c against the RTL (fpga/rtl/mcaudio).

Every test of fpga/tools/mctest.py (and some of our own: saturation, the output shift, a 4-node
group, a skipped node, tempo, and random microcode) is run twice on the same RAM image and control
store: through iverilog (mcaud_tb.v: fpga/sim/mc_tb.v with the real rammux, plus a RAM, monitor and
register dump at the end) and through the C model (mcaud_check.c, which drives it with the CPU's
register writes). PASS needs every output sample, the node state written back to RAM, the monitor
(256 output samples and the node meters), the ring position, IRQ, BLOCKS, BURSTS, WORDS and NODES
to match. CLOCKS is an estimate in the model: reported, not compared.

usage: mcaud_check.py [test ...]      exit status: the number of tests that differ
"""
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "fpga", "tools"))
import mcasm  # noqa: E402
import mctest  # noqa: E402

RTL = os.path.join(ROOT, "fpga", "rtl")
OUT = tempfile.mkdtemp(prefix="mcaud_check_")
CFLAGS = ["-O2", "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter"]
results = []           # (test, ok, detail) one per run()


def build():
    vvp = os.path.join(OUT, "tb.vvp")
    subprocess.check_call(["iverilog", "-g2012", "-o", vvp, os.path.join(HERE, "mcaud_tb.v"),
                           os.path.join(RTL, "mcaudio", "mc_engine.v"), os.path.join(RTL, "mcaudio", "mc_core.v"),
                           os.path.join(RTL, "mcaudio", "mc_regs.v"), os.path.join(RTL, "rammux.v")])
    exe = os.path.join(OUT, "mcaud_check")
    subprocess.check_call(["cc"] + CFLAGS + ["-I", os.path.join(ROOT, "sim", "src"), "-o", exe,
                           os.path.join(HERE, "mcaud_check.c"), os.path.join(ROOT, "sim", "src", "mcaud.c")])
    return vvp, exe


def parse(text):
    info = {"MON": {}}
    for line in text.splitlines():
        p = line.split()
        if not p:
            continue
        if p[0] == "MON":
            info["MON"][int(p[1])] = int(p[2].replace("x", "0"), 16)     # (never-written monitor words are x)
        elif p[0] in ("ID", "IRQ"):
            info[p[0]] = int(p[1], 16)
        elif p[0] in ("BLOCKS", "UNDERRUNS", "BURSTS", "WORDS", "NODES", "RING", "CLOCKS"):
            info[p[0]] = int(p[1])
            if p[0] == "CLOCKS" and len(p) > 3:
                info["MAXCLK"] = int(p[3])
    info["METER"] = [info["MON"][256 + i] for i in range(min(32, info.get("NODES", 0)))]
    return info


def lines(path):
    with open(path) as f:
        return [x.strip() for x in f if x.strip() and not x.startswith("//")]


def run_both(name, ram, n, tempo=0, ctrl=1, words=None, entry_note=""):
    """what mctest.run does (writes cs.hex, ram.hex, head.txt), then both simulators and the comparison"""
    d = os.path.join(OUT, name)
    os.makedirs(d, exist_ok=True)
    head = ram.link()
    with open(os.path.join(d, "cs.hex"), "w") as f:
        w = words if words is not None else mctest.WORDS
        for x in w + [0] * (512 - len(w)):
            f.write("%018x\n" % x)
    top = max(ram.m) + 1
    with open(os.path.join(d, "ram.hex"), "w") as f:
        for a in range(top):
            f.write("%08x\n" % ram.m.get(a, 0))
    with open(os.path.join(d, "head.txt"), "w") as f:
        f.write("%08x\n%08x\n" % (head, tempo))
    r = subprocess.run(["vvp", "-n", VVP, "+dir=" + d, "+n=%d" % n, "+top=%d" % top, "+ctrl=%d" % ctrl],
                       cwd=os.path.join(RTL, "mcaudio"), capture_output=True, text=True)
    rtl = parse(r.stdout)
    L, R = [], []
    for line in lines(os.path.join(d, "out.txt")):
        a, b = line.split()
        L.append(int(a)); R.append(int(b))
    m = subprocess.run([EXE, d, str(rtl["BLOCKS"]), str(ctrl)], capture_output=True, text=True)
    mod = parse(m.stdout)
    ML, MR = [], []
    for line in lines(os.path.join(d, "out_m.txt")):
        a, b = line.split()
        ML.append(int(a)); MR.append(int(b))
    bad = []
    if len(L) != n or "BLOCKS" not in rtl or "BLOCKS" not in mod:
        bad.append("run failed (rtl %d samples, model: %s)" % (len(L), m.stderr.strip()[:80]))
    else:
        if ML[:n] != L or MR[:n] != R:
            k = next(i for i in range(n) if (ML[i], MR[i]) != (L[i], R[i]))
            bad.append("samples differ from %d: rtl %d,%d model %d,%d" % (k, L[k], R[k], ML[k], MR[k]))
        if len(ML) != 32 * rtl["BLOCKS"]:
            bad.append("model frames %d != 32 x %d blocks" % (len(ML), rtl["BLOCKS"]))
        ra, rb = lines(os.path.join(d, "ram_out.hex")), lines(os.path.join(d, "ram_m.hex"))
        if ra != rb:
            k = next(i for i in range(min(len(ra), len(rb))) if ra[i] != rb[i])
            bad.append("RAM differs at word %#x: rtl %s model %s (%d words differ)" % (
                k, ra[k], rb[k], sum(1 for x, y in zip(ra, rb) if x != y)))
        if rtl["MON"] != mod["MON"]:
            k = next(i for i in sorted(rtl["MON"]) if rtl["MON"][i] != mod["MON"].get(i))
            bad.append("monitor differs at %d: rtl %08x model %08x" % (k, rtl["MON"][k], mod["MON"].get(k)))
        for k in ("ID", "IRQ", "BLOCKS", "UNDERRUNS", "BURSTS", "WORDS", "NODES", "RING"):
            if rtl.get(k) != mod.get(k):
                bad.append("%s: rtl %s model %s" % (k, rtl.get(k), mod.get(k)))
    detail = "%d samples, %d blocks, %d nodes, rtl clocks %s max %s, model %s" % (
        n, rtl.get("BLOCKS", -1), rtl.get("NODES", -1), rtl.get("CLOCKS"), rtl.get("MAXCLK"), mod.get("CLOCKS"))
    results.append((name, not bad, "; ".join(bad) if bad else detail))
    rtl["UNDERRUNS"] = rtl.get("UNDERRUNS", 0)
    return L, R, rtl


def patched_run(name, ram, n, out, tempo=0):
    return run_both(name, ram, n, tempo)


# ---- our own tests
def t_sat():
    ram = mctest.Ram()
    for k in range(12):
        mctest.osc(ram, 0x100 + 16 * k, ('saw', 'pulse', 'tri')[k % 3], 100 + 37 * k, v=(1.0, 0.9), s=0x7FFF, d=0)
    run_both("sat", ram, 2400)                       # shift 0: the sum clips
    ram = mctest.Ram()
    for k in range(12):
        mctest.osc(ram, 0x100 + 16 * k, ('saw', 'pulse', 'tri')[k % 3], 100 + 37 * k, v=(1.0, 0.9), s=0x7FFF, d=0)
    run_both("shift", ram, 2400, ctrl=0x31)          # >> 3


def t_group4():
    ram = mctest.Ram()
    w = {2: mctest.step32(120), 4: mctest.vol(0.3, 0.3), 5: 1}
    w.update(mctest.env(s=0x7FFF, d=0))
    ram.node(0x100, mcasm_entry('sync'), w, linked=True)
    ram.node(0x110, 0, {2: mctest.step32(120 * 2.7), 4: mctest.vol(0.4, 0.4), 20: 0x7FFF0000})
    ram.node(0x120, 0, {2: 5, 12: 0xDEAD, 13: 0x1234, 14: 0x5678, 15: 0x9ABC}, linked=True)
    ram.node(0x130, 0, {13: 0x4321})
    mctest.osc(ram, 0x140, 'saw', 330)
    mctest.osc(ram, 0x150, 'noise', 3000)
    run_both("group4", ram, 1920)


def t_skip():
    ram = mctest.Ram()
    mctest.osc(ram, 0x100, 'saw', 440)
    mctest.osc(ram, 0x110, 'tri', 330)
    ram.m[0x111] |= 0x80000000                       # skipped: not run, its state still written back
    ram.m[0x11D] = 0x4444
    mctest.osc(ram, 0x120, 'pulse', 220)
    run_both("skip", ram, 1920)


def t_tempo():
    ram = mctest.Ram()
    mctest.osc(ram, 0x100, 'saw', 440)
    run_both("tempo", ram, 1280, tempo=0x5555)


def t_many():
    """40 nodes: the meters of the monitor wrap at 32 (5-bit node order)"""
    ram = mctest.Ram()
    for k in range(40):
        mctest.osc(ram, 0x100 + 16 * k, ('saw', 'pulse', 'tri', 'noise')[k % 4], 90 + 21 * k, v=(0.02, 0.02), lfo=40 + k, vib=300)
        ram.m[0x100 + 16 * k + 13] = 0
    run_both("many", ram, 192)


def mcasm_entry(n): return mctest.ENTRY[n]


# ---- random microcode: straight-line bodies with forward branches, run 32 times by the sample loop
F = mcasm.FIELDS


def pack(f):
    v = 0
    for k, x in f.items():
        v |= x << F[k][0]
    return v


def rword(rng, i, last):
    """a random word: any ALU op (no DMA), flags, predicates, MACs, reads, bus ops, SYS (not DMA);
    a branch only forward (to a word index in (i, last])"""
    f = dict(aop=rng.choice([0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 13, 14, 15]), rd=rng.randrange(16),
             ra=rng.randrange(16), rb=rng.randrange(16), bsel=rng.randrange(4), setf=rng.random() < 0.4,
             mop=rng.choice([0, 0, 1, 2, 3, 4]), masel=rng.randrange(4), mbsel=rng.randrange(4),
             memop=rng.choice([0, 0, 1, 2, 3, 3, 4, 5, 5, 6, 7]), sh=rng.randrange(32), sfmt=rng.randrange(4),
             pred=rng.choice([0, 0, 0, 1, 2, 3]), imm=rng.choice([rng.randrange(65536), rng.randrange(64), rng.randrange(4)]))
    if f['aop'] == 13:
        f['sh'] = rng.choice([1, 2, 3, 5, 6, 7])
        f['imm'] = rng.randrange(8)
    if rng.random() < 0.2 and i < last:
        f['bc'] = rng.choice([1, 2, 3, 4, 5])
        f['nxt'] = rng.randrange(i + 1, last + 1)
    return {k: int(v) for k, v in f.items()}


def random_cs(rng, nroutines):
    words, entries = [{}], []                        # word 0: end
    words[0] = dict(bc=7)
    # the prefix: fill the sample buffer (32 bursts of 16 words from RAM word 0x4000), then end
    pre = len(words)
    for k in range(32):
        words.append(dict(aop=mcasm.AOP['mov'], rd=1, bsel=1, imm=0x4000 + 16 * k))
        words.append(dict(aop=mcasm.AOP['mov'], rd=2, bsel=1, imm=16 * k))
        words.append(dict(aop=mcasm.AOP['sys'], sh=0, ra=1, rb=2, imm=16))
    words.append(dict(bc=7))
    for _ in range(nroutines):
        base = len(words)
        k = rng.randrange(6, 40)
        for i in range(k):
            words.append(rword(rng, base + i, base + k))
        loop = rword(rng, base + k, base + k)
        loop.pop('bc', None); loop.pop('nxt', None)
        loop.update(bc=6, nxt=base)
        words.append(loop)
        end = rword(rng, 0, 0); end.pop('bc', None); end.pop('nxt', None)
        end['bc'] = 7
        words.append(end)
        entries.append(base)
    return [pack(w) for w in words], pre, entries


def t_random(seed):
    rng = random.Random(seed)
    words, pre, entries = random_cs(rng, 8)
    ram = mctest.Ram()
    for a in range(0x4000, 0x4000 + 512):
        ram.m[a] = rng.getrandbits(32)
    # a first group of 4 (fills the whole node buffer): the sample fill, three fillers
    ram.node(0x100, pre, {k: rng.getrandbits(32) for k in range(2, 16)}, linked=True)
    for j in (1, 2):
        ram.node(0x100 + 16 * j, 0, {k: rng.getrandbits(32) for k in range(2, 16)}, linked=True)
    ram.node(0x130, 0, {k: rng.getrandbits(32) for k in range(2, 16)})
    for j, e in enumerate(entries):
        ram.node(0x200 + 16 * j, e, {k: rng.getrandbits(32) if rng.random() < 0.6 else rng.randrange(4)
                                     for k in range(2, 16)}, linked=rng.random() < 0.25 and j % 4 != 3)
    run_both("random%d" % seed, ram, 192, tempo=rng.getrandbits(16), ctrl=1 | rng.randrange(8) << 4, words=words)


EXTRA = dict(many=t_many, sat=t_sat, group4=t_group4, skip=t_skip, tempo=t_tempo,
             **{"random%d" % s: (lambda s=s: t_random(s)) for s in range(1, 9)})


def one(n):
    del results[:]
    if n in mctest.TESTS:
        mctest.TESTS[n](OUT)                         # (their own functional checks are mctest.py's job)
    else:
        EXTRA[n]()
    return list(results)


def bench():
    """host cost of a block on heavy lists: toccata's (64 looped 16-bit sample nodes sounding), 64
    oscillators, and a mix with the filter"""
    exe = os.path.join(OUT, "mcaud_bench")
    subprocess.check_call(["cc"] + CFLAGS + ["-I", os.path.join(ROOT, "sim", "src"), "-o", exe,
                           os.path.join(HERE, "mcaud_check.c"), os.path.join(ROOT, "sim", "src", "mcaud.c")])
    n = 6000
    data = []
    for k in range(n):
        import math
        v = int(9000 * math.sin(2 * math.pi * k / 48) + 6000 * math.sin(2 * math.pi * k / 31)) & 0xFFFF
        data += [v & 0xFF, v >> 8]
    for name in ("toccata64", "osc64", "mix"):
        ram = mctest.Ram()
        ram.bytes8(0x40000, bytes(data))
        for k in range(64):
            a = 0x100 + 16 * k
            if name == "toccata64" or (name == "mix" and k % 2):
                w = {2: 0x10000 + 0x400 * k, 4: mctest.vol(0.02, 0.02), 5: 0x40000, 6: n << 16, 7: 2976 << 16,
                     8: 0x40000 + (n - 2976) // 2}
                w.update(mctest.env(s=0x7FFF, d=0))
                ram.node(a, mctest.ENTRY['smp16'], w)
            else:
                mctest.osc(ram, a, ('saw', 'pulse', 'tri')[k % 3], 100 + 13 * k, v=(0.02, 0.02), lfo=100, vib=500)
        if name == "mix":
            ram.node(0x100 + 16 * 64, mctest.ENTRY['flt'], {2: 0x0600, 3: 0x7000, 4: mctest.vol(0.9, 0.9), 5: 0, 6: 0, 7: 0})
        d = os.path.join(OUT, "bench_" + name)
        os.makedirs(d, exist_ok=True)
        head = ram.link()
        with open(os.path.join(d, "cs.hex"), "w") as f:
            for x in mctest.WORDS + [0] * (512 - len(mctest.WORDS)):
                f.write("%018x\n" % x)
        with open(os.path.join(d, "ram.hex"), "w") as f:
            for a in range(max(ram.m) + 1):
                f.write("%08x\n" % ram.m.get(a, 0))
        with open(os.path.join(d, "head.txt"), "w") as f:
            f.write("%08x\n%08x\n" % (head, 0))
        r = subprocess.run([exe, d, "1500", "1", "time"], capture_output=True, text=True)
        print("%-10s %s" % (name, r.stderr.strip()))


def main():
    global VVP, EXE
    if sys.argv[1:] == ["--bench"]:
        return bench()
    VVP, EXE = build()
    mctest.run = patched_run
    names = sys.argv[1:] or list(mctest.TESTS) + list(EXTRA)
    import multiprocessing as mp
    with mp.get_context("fork").Pool(min(len(names), os.cpu_count() or 1)) as pool:
        got = pool.map(one, names, chunksize=1)
    bad = tot = 0
    for rs in got:
        for name, ok, detail in rs:
            print("%-14s %s  %s" % (name, "PASS" if ok else "FAIL", detail))
            bad += not ok
            tot += 1
    print("%d of %d runs bit-exact" % (tot - bad, tot))
    sys.exit(bad)


if __name__ == "__main__":
    main()
