#!/usr/bin/env python3
"""The microcoded audio engine's tests (docs/mcaudio.md): node lists in RAM for each routine, run
through fpga/sim/mc_tb.v (iverilog, the real rammux), the output checked for what the routine is
for: pitch (zero crossings), envelope, looping, sync, ring, the filter. WAVs of each in OUTDIR.

usage: mctest.py OUTDIR [test ...]
"""
import math
import os
import subprocess
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import mcasm  # noqa: E402

RTL = os.path.join(ROOT, "fpga", "rtl")
WORDS, ENTRY, _ = mcasm.build()
FS = 48000


def step32(hz): return int(hz / FS * 2**32) & 0xFFFFFFFF
def vol(l, r): return (int(r * 32767) << 16) | int(l * 32767)


class Ram:
    def __init__(self):
        self.m = {}
        self.nodes = []

    def node(self, addr, entry, w, linked=False):
        """a node at word address addr: w = {word: value} for words 2-15"""
        self.m[addr + 1] = entry
        for k, v in w.items():
            self.m[addr + k] = v & 0xFFFFFFFF
        self.nodes.append((addr, linked))

    def link(self):
        for i, (a, lk) in enumerate(self.nodes):
            nxt = self.nodes[i + 1][0] if i + 1 < len(self.nodes) else 0
            self.m[a] = nxt | (0x80000000 if lk else 0)
        return self.nodes[0][0]

    def bytes8(self, addr, data):
        for i in range(0, len(data), 4):
            b = list(data[i:i + 4]) + [0] * (4 - len(data[i:i + 4]))
            self.m[addr + i // 4] = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24


def env(gate=1, trig=1, a=0x2000, d=0x0400, r=0x0800, s=0x6000, route=0):
    return {3: trig | gate << 8 | route << 12, 9: a << 16, 10: r << 16 | d, 11: s << 16}


def osc(ram, addr, kind, hz, v=(0.5, 0.5), width=2048, lfo=0, pwm=0, vib=0, **e):
    w = {2: step32(hz), 4: vol(*v), 5: width, 6: lfo << 16, 7: pwm, 8: vib, 15: 1 if kind == 'noise' else 0}
    w.update(env(**e))
    ram.node(addr, ENTRY['osc_' + kind], w)


def run(name, ram, n, out, tempo=0):
    d = os.path.join(out, name)
    os.makedirs(d, exist_ok=True)
    head = ram.link()
    with open(os.path.join(d, "cs.hex"), "w") as f:
        for w in WORDS + [0] * (512 - len(WORDS)):
            f.write("%018x\n" % w)
    top = max(ram.m) + 1
    with open(os.path.join(d, "ram.hex"), "w") as f:
        for a in range(top):
            f.write("%08x\n" % ram.m.get(a, 0))
    with open(os.path.join(d, "head.txt"), "w") as f:
        f.write("%08x\n%08x\n" % (head, tempo))
    vvp = os.path.join(out, "mc_tb.vvp")
    if not os.path.exists(vvp):
        subprocess.check_call(["iverilog", "-g2012", "-o", vvp, os.path.join(ROOT, "fpga", "sim", "mc_tb.v"),
                               os.path.join(RTL, "mcaudio", "mc_engine.v"), os.path.join(RTL, "mcaudio", "mc_core.v"),
                               os.path.join(RTL, "mcaudio", "mc_regs.v"), os.path.join(RTL, "rammux.v")])
    r = subprocess.run(["vvp", "-n", vvp, "+dir=" + d, "+n=%d" % n], cwd=os.path.join(RTL, "mcaudio"),
                       capture_output=True, text=True)
    info = {}
    for line in r.stdout.splitlines():
        p = line.split()
        if p and p[0] in ("ID", "BLOCKS", "UNDERRUNS", "CLOCKS", "BURSTS", "WORDS", "NODES", "RING"):
            info[p[0]] = p[1] if p[0] == "ID" else int(p[1])
            if p[0] == "CLOCKS": info["MAXCLK"] = int(p[3])
        if p and p[0] == "METER":
            info.setdefault("METER", []).append(int(p[2], 16))
    L, R = [], []
    for line in open(os.path.join(d, "out.txt")):
        a, b = line.split()
        L.append(int(a)); R.append(int(b))
    with wave.open(os.path.join(out, name + ".wav"), "wb") as wv:
        wv.setnchannels(2); wv.setsampwidth(2); wv.setframerate(FS)
        wv.writeframes(b"".join((l & 0xFFFF).to_bytes(2, "little") + (r_ & 0xFFFF).to_bytes(2, "little")
                                for l, r_ in zip(L, R)))
    return L, R, info


def freq(x, a=0, b=None):
    """the fundamental from upward zero crossings (with hysteresis)"""
    x = x[a:b]
    c, st, first, last = 0, 1, None, None       # (st 1: a crossing needs a negative excursion first)
    pk = max(1, max(abs(v) for v in x) // 8)
    for i, v in enumerate(x):
        if st <= 0 and v > pk:
            if first is None: first = i
            else: c += 1; last = i
            st = 1
        elif st >= 0 and v < -pk:
            st = -1
    return (c * FS / (last - first)) if c else 0


def rms(x): return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


def t_saw(out):
    ram = Ram(); osc(ram, 0x100, 'saw', 440, v=(0.5, 0.25))
    L, R, i = run("saw", ram, 4800, out)
    f = freq(L, 2400)
    ok = abs(f - 440) < 4 and rms(L[3000:]) > 3000 and rms(R[3000:]) < rms(L[3000:]) * 0.7 and rms(L[:64]) < rms(L[3000:])
    return ok, "440 Hz saw: %.1f Hz, rms early %d late %d, R/L %.2f, meter %s" % (
        f, rms(L[:64]), rms(L[3000:]), rms(R[3000:]) / max(1, rms(L[3000:])), i.get("METER")), i


def t_pulse(out):
    ram = Ram(); osc(ram, 0x100, 'pulse', 220, width=1024, lfo=600, pwm=900)
    L, R, i = run("pulse", ram, 9600, out)
    # the duty cycle moves: the fraction of positive samples over two windows
    def duty(x): return sum(1 for v in x if v > 0) / len(x)
    d1, d2 = duty(L[2400:4800]), duty(L[7200:9600])
    # the pitch from the rising edges (at each phase wrap); a width change at a block boundary can
    # add an edge inside a period (as a SID's does), so the median spacing
    r = [i for i in range(2400, 9600) if L[i] > 0 and L[i - 1] <= 0]
    d = sorted(b - a for a, b in zip(r, r[1:]))
    f = FS / d[len(d) // 2]
    return abs(f - 220) < 3 and abs(d1 - d2) > 0.03, "pulse 220 Hz, PWM: %.1f Hz, duty %.2f then %.2f" % (f, d1, d2), i


def t_tri_vib(out):
    ram = Ram(); osc(ram, 0x100, 'tri', 330, lfo=200, vib=1500)     # a 4.6 Hz LFO, +-4.6%
    L, R, i = run("tri", ram, 24000, out)
    fs = [freq(L, a, a + 1200) for a in range(2400, 24000, 1200)]
    return max(fs) - min(fs) > 10 and abs(sum(fs) / len(fs) - 330) < 15, \
        "triangle 330 Hz with vibrato: %s" % ", ".join("%.0f" % f for f in fs), i


def t_noise(out):
    ram = Ram(); osc(ram, 0x100, 'noise', 12000)
    L, R, i = run("noise", ram, 4800, out)
    zc = sum(1 for a, b in zip(L[2400:], L[2401:]) if (a < 0) != (b < 0))
    return zc > 200 and rms(L[2400:]) > 2000, "noise: %d sign changes in 2400 samples, rms %d" % (zc, rms(L[2400:])), i


def t_release(out):
    ram = Ram(); osc(ram, 0x100, 'saw', 440, gate=0, a=0x7FFF)
    L, R, i = run("release", ram, 4800, out)
    return rms(L[4000:]) < 200, "gate off: released to rms %d" % rms(L[4000:]), i


def smp_node(ram, addr, base, frames, step, loop_start=None, v=(0.6, 0.6), **e):
    end = frames
    w = {2: step, 4: vol(*v), 5: base, 6: end << 16, 7: ((end - loop_start) << 16) if loop_start is not None else 0,
         8: base + (loop_start or 0) // 4}
    w.update(env(**e))
    ram.node(addr, ENTRY['smp'], w)


def t_sample_loop(out):
    ram = Ram()
    # a 64-frame sine cycle, looped from frame 32 (a 64-frame sample: frames 0-31 once, then 32-63)
    cyc = [int(100 * math.sin(2 * math.pi * k / 64)) & 0xFF for k in range(64)]
    ram.bytes8(0x2000, bytes(cyc))
    smp_node(ram, 0x100, 0x2000, 64, 0x8000, loop_start=0, s=0x7FFF, d=0)        # step 0.5: 375 Hz
    L, R, i = run("sample_loop", ram, 4800, out)
    f = freq(L, 1200)
    return abs(f - 375) < 3 and rms(L[1200:]) > 3000, "64-frame sine looped, step 0.5: %.1f Hz, rms %d" % (f, rms(L[1200:])), i


def t_sample_long(out):
    ram = Ram()
    # 6000 frames: a 1 kHz tone at 48 kHz (48 frames a cycle), looped over its last 2976 (62 cycles), step 1.5
    n = 6000
    d = [int(110 * math.sin(2 * math.pi * k / 48)) & 0xFF for k in range(n)]
    ram.bytes8(0x4000, bytes(d))
    smp_node(ram, 0x100, 0x4000, n, 0x18000, loop_start=n - 2976, s=0x7FFF, d=0)
    L, R, i = run("sample_long", ram, 14400, out)
    fs = [freq(L, a, a + 2400) for a in range(2400, 14400, 2400)]
    return all(abs(f - 1500) < 8 for f in fs), "6000-frame 1 kHz tone at step 1.5, through its loop: %s Hz" % \
        ", ".join("%.0f" % f for f in fs), i


def t_sample16(out):
    ram = Ram()
    n = 6000
    d = []
    for k in range(n):
        v = int(28000 * math.sin(2 * math.pi * k / 48)) & 0xFFFF
        d += [v & 0xFF, v >> 8]
    ram.bytes8(0x4000, bytes(d))
    w = {2: 0x18000, 4: vol(0.6, 0.6), 5: 0x4000, 6: n << 16, 7: 2976 << 16, 8: 0x4000 + (n - 2976) // 2}
    w.update(env(s=0x7FFF, d=0))
    ram.node(0x100, ENTRY['smp16'], w)
    L, R, i = run("sample16", ram, 14400, out)
    fs = [freq(L, a, a + 2400) for a in range(2400, 14400, 2400)]
    dd = [abs(L[k + 1] - 2 * L[k] + L[k - 1]) for k in range(2400, 14399)]
    return all(abs(f - 1500) < 8 for f in fs) and max(dd) < 3 * sorted(dd)[len(dd) // 2] + 200, \
        "16-bit 1 kHz tone at step 1.5 through its loop: %s Hz, largest 2nd difference %d (median %d)" % (
            ", ".join("%.0f" % f for f in fs), max(dd), sorted(dd)[len(dd) // 2]), i


def t_oneshot(out):
    ram = Ram()
    d = [int(100 * math.sin(2 * math.pi * k / 32)) & 0xFF for k in range(1600)]
    ram.bytes8(0x2000, bytes(d))
    smp_node(ram, 0x100, 0x2000, 1600, 0x10000, s=0x7FFF, d=0)      # 1600 frames: 33 ms
    L, R, i = run("oneshot", ram, 4800, out)
    return rms(L[400:1400]) > 3000 and rms(L[2000:]) < 50, "one-shot of 1600 frames: rms %d while playing, %d after" % (
        rms(L[400:1400]), rms(L[2000:])), i


def t_sync(out, ring=False):
    ram = Ram()
    w = {2: step32(120), 4: vol(0.0, 0.0), 5: 2 if ring else 1}      # 120 Hz: 400 samples a period
    w.update(env(s=0x7FFF, d=0))
    ram.node(0x100, ENTRY['sync'], w, linked=True)
    ram.node(0x110, 0, {2: step32(120 * 2.7), 4: vol(0.6, 0.6)})
    L, R, i = run("ring" if ring else "sync", ram, 9600 if ring else 4800, out)
    def corr(a0, lag, n=2000):
        x, y = L[a0:a0 + n], L[a0 + lag:a0 + lag + n]
        return sum(p * q for p, q in zip(x, y)) / math.sqrt(sum(p * p for p in x) * sum(q * q for q in y))
    if not ring:
        # synced, the slave (2.7 times the master) restarts each master period: the output repeats there
        c = corr(2400, 400)
        return c > 0.95 and rms(L[2400:4400]) > 2000, "hard sync: correlation over the master's period %.3f, rms %d" % (
            c, rms(L[2400:4400])), i
    # ring: the slave runs free, flipped by the master's top bit: the output repeats every 10 master
    # periods (27 slave cycles, 4000 samples), but not every slave cycle as a plain triangle would
    c10, cs = corr(2400, 4000), corr(2400, round(400 / 2.7 * 3))
    return c10 > 0.95 and cs < 0.9 and rms(L[2400:4400]) > 2000, \
        "ring: correlation over 10 master periods %.3f, over 3 slave cycles %.3f, rms %d" % (c10, cs, rms(L[2400:4400])), i


def t_filter(out):
    def make(route):
        ram = Ram()
        osc(ram, 0x100, 'saw', 220, s=0x7FFF, d=0, route=route)
        if route:
            ram.node(0x110, ENTRY['flt'], {2: 0x0600, 3: 0x7000, 4: vol(0.9, 0.9), 5: 0, 6: 0, 7: 0})
        return ram
    L0, _, _ = run("filter_off", make(0), 4800, out)
    L1, _, i = run("filter_on", make(1), 4800, out)
    def hf(x): return rms([b - a for a, b in zip(x, x[1:])])
    r0, r1 = hf(L0[2400:]) / max(1, rms(L0[2400:])), hf(L1[2400:]) / max(1, rms(L1[2400:]))
    return r1 < r0 * 0.5 and rms(L1[2400:]) > 1000, "low-pass at ~1 kHz on a 220 Hz saw: HF ratio %.3f -> %.3f, rms %d" % (
        r0, r1, rms(L1[2400:])), i


def t_load(out):
    ram = Ram()
    for k in range(24):
        osc(ram, 0x100 + 16 * k, ('saw', 'tri', 'pulse')[k % 3], 110 * (1 + k % 8), v=(0.03, 0.03))
    d = [int(100 * math.sin(2 * math.pi * k / 48)) & 0xFF for k in range(4000)]
    ram.bytes8(0x4000, bytes(d))
    for k in range(6):
        smp_node(ram, 0x400 + 16 * k, 0x4000, 4000, 0x10000 + 0x2000 * k, loop_start=2000, v=(0.05, 0.05), s=0x7FFF, d=0)
    L, R, i = run("load", ram, 1920, out)
    return i["UNDERRUNS"] == 0 and i["NODES"] == 30, "30 voices (24 oscillators, 6 samples): %d nodes, %d of 44,992 clocks (max %d), %d bursts, underruns %d" % (
        i["NODES"], i["CLOCKS"], i["MAXCLK"], i["BURSTS"], i["UNDERRUNS"]), i


TESTS = dict(saw=t_saw, pulse=t_pulse, tri=t_tri_vib, noise=t_noise, release=t_release,
             sample_loop=t_sample_loop, sample_long=t_sample_long, sample16=t_sample16, oneshot=t_oneshot,
             sync=t_sync, ring=lambda o: t_sync(o, True), filter=t_filter, load=t_load)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    vvp = os.path.join(out, "mc_tb.vvp")
    if os.path.exists(vvp): os.remove(vvp)
    names = sys.argv[2:] or list(TESTS)
    bad = 0
    for n in names:
        ok, msg, i = TESTS[n](out)
        print("%-12s %s  %s" % (n, "PASS" if ok else "FAIL", msg))
        if not ok: bad += 1
    sys.exit(bad)


if __name__ == "__main__":
    main()
