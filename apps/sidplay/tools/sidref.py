#!/usr/bin/env python3
"""A straightforward SID renderer (floating point, 48 kHz output) from the SID write log written by the
host build: sidref.py siddump.txt out.wav seconds [voice mask 1..7] [filter 0/1]
Not reSID: single oscillators as the SID does them (24-bit accumulator, combined waves ANDed), ADSR with the
SID's rate counter and exponential-decay dividers (reSID's tables), ring mod and sync between voices
(1<-3, 2<-1, 3<-2), a Chamberlin filter with its own cutoff curve. It exists to cross-check sidmap.c."""
import sys, struct, wave, math

RATE = [9, 32, 63, 95, 149, 220, 267, 313, 392, 977, 1954, 3126, 3907, 11720, 19532, 31251]
EXPT = [(0xFF, 1), (0x5D, 2), (0x36, 4), (0x1A, 8), (0x0E, 16), (0x06, 30)]

def exp_period(level):
    p = 1
    for lv, per in EXPT:
        if level <= lv: p = per
    return p

class Env:
    def __init__(s): s.level = 0; s.state = 'R'; s.acc = 0.0; s.ecnt = 0; s.gate = 0
    def set_gate(s, g):
        if g and not s.gate: s.state = 'A'; s.ecnt = 0
        elif not g and s.gate: s.state = 'R'; s.ecnt = 0
        s.gate = g
    def run(s, cycles, ad, sr):
        s.acc += cycles
        while True:
            if s.state == 'A': p = RATE[ad >> 4]
            elif s.state == 'D': p = RATE[ad & 15]
            else: p = RATE[sr & 15]
            if s.acc < p: return
            s.acc -= p
            if s.state == 'A':
                s.level += 1
                if s.level >= 255: s.level = 255; s.state = 'D'; s.ecnt = 0
            else:
                if s.state == 'D' and s.level == (sr >> 4) * 17: continue
                s.ecnt += 1
                if s.ecnt >= exp_period(s.level):
                    s.ecnt = 0
                    if s.level > 0: s.level -= 1

class Osc:
    def __init__(s): s.acc = 0.0; s.lfsr = 0x7FFFFF; s.noise = 0; s.prev19 = 0; s.wrapped = False
    def advance(s, fn, cycles, test, sync_from):
        if test: s.acc = 0.0; s.wrapped = False; return
        old = s.acc
        s.acc += fn * cycles
        s.wrapped = False
        if s.acc >= 16777216.0: s.acc -= 16777216.0 * int(s.acc / 16777216.0); s.wrapped = True
        b19 = (int(s.acc) >> 19) & 1
        if b19 and not s.prev19:
            bit = ((s.lfsr >> 22) ^ (s.lfsr >> 17)) & 1
            s.lfsr = ((s.lfsr << 1) & 0x7FFFFF) | bit
            s.noise = ((s.lfsr >> 22 & 1) << 7 | (s.lfsr >> 20 & 1) << 6 | (s.lfsr >> 16 & 1) << 5 | (s.lfsr >> 13 & 1) << 4 |
                       (s.lfsr >> 11 & 1) << 3 | (s.lfsr >> 7 & 1) << 2 | (s.lfsr >> 4 & 1) << 1 | (s.lfsr >> 2 & 1)) << 4
        s.prev19 = b19

def load(path):
    base = cpc = None; ev = []
    for l in open(path):
        t = l.split()
        if t[0] == 'B': base, cpc = int(t[1]), int(t[2])
        else: ev.append((int(t[1]), int(t[2]), int(t[3])))
    ev = [(c - (base + cpc), r, v) for (c, r, v) in ev]       # frame 0's play call starts at base + cpc
    return ev, cpc

def render(path, seconds, mask=7, use_filter=True, clock=985248.0):
    ev, cpc = load(path)
    # the player's first call starts at base + cpc; writes before that are init's (cycle < base)
    ev = [(c, r, v) for (c, r, v) in ev]
    regs = [0] * 32
    # init writes happened before frame 0: apply the ones with negative time at t=0
    osc = [Osc() for _ in range(3)]; env = [Env() for _ in range(3)]
    n = int(seconds * 48000); cps = clock / 48000.0
    out = []; i = 0; low = band = 0.0
    for k in range(n):
        t_end = (k + 1) * cps
        while i < len(ev) and ev[i][0] < t_end:
            c, r, v = ev[i]; i += 1
            regs[r] = v
            if r in (4, 11, 18): env[(r - 4) // 7].set_gate(v & 1)
        mix = 0.0; filt_in = 0.0
        for vi in range(3):
            b = vi * 7
            fn = regs[b] | regs[b + 1] << 8; ctrl = regs[b + 4]; pw = regs[b + 2] | (regs[b + 3] & 15) << 8
            src = osc[(vi + 2) % 3]
            osc[vi].advance(fn, cps, ctrl & 8, None)
            if (ctrl & 2) and src.wrapped: osc[vi].acc = 0.0
            env[vi].run(cps, regs[b + 5], regs[b + 6])
            if not (mask >> vi & 1): continue
            a = int(osc[vi].acc) >> 12
            w = None
            flip = (a >> 11 & 1) ^ ((int(src.acc) >> 23 & 1) if (ctrl & 4) else 0)
            tri = ((a & 0x7FF) ^ (0x7FF if flip else 0)) << 1
            for bit, f in ((0x10, lambda: tri), (0x20, lambda: a), (0x40, lambda: 4095 if a >= pw else 0), (0x80, lambda: osc[vi].noise)):
                if ctrl & bit: w = f() if w is None else (w & f())
            if w is None or (ctrl & 8): continue
            s = (w - 2048) / 2048.0 * env[vi].level / 255.0
            if regs[23] >> vi & 1 and use_filter: filt_in += s
            elif not (vi == 2 and regs[24] & 0x80): mix += s
        if use_filter:
            fc = regs[21] & 7 | regs[22] << 3
            f = 30 + 12000.0 * (fc / 2047.0) ** 1.25
            F = 2 * math.sin(math.pi * min(f, 7000) / 48000.0)
            q = 1.0 / (0.707 + 0.25 * (regs[23] >> 4)) if True else 1
            low += F * band; high = filt_in - low - q * band; band += F * high
            m = regs[24] >> 4
            mix += (low if m & 1 else 0) + (band if m & 2 else 0) + (high if m & 4 else 0)
        out.append(mix * (regs[24] & 15) / 15.0)
    return out

if __name__ == '__main__':
    seconds = float(sys.argv[3]); mask = int(sys.argv[4]) if len(sys.argv) > 4 else 7
    y = render(sys.argv[1], seconds, mask, (int(sys.argv[5]) if len(sys.argv) > 5 else 1))
    w = wave.open(sys.argv[2], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes(b''.join(struct.pack('<h', max(-32768, min(32767, int(v * 9000)))) for v in y)); w.close()
    print(sys.argv[2], len(y) / 48000.0, 's')
