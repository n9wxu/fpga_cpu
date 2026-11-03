#!/usr/bin/env python3
"""Reference model of the audio engine (fpga/rtl/audio_engine.v, docs/audio.md).

It renders a register script, frame (48 kHz sample) by frame, with the
engine's exact integer arithmetic, so the testbench can compare bit for bit.

Script file, one event a line:
    M <frame> <word> <value>     the CPU writes a RAM word (hex word address) before <frame>
    W <frame> <addr> <value>     a 16-bit register write (hex addr and value,
                                 addr relative to the I/O space F00000: 1800 =
                                 F01800) taking effect from <frame> (>= 1)
    E <frames>                   number of frames to render
Memory image: one 32-bit hex word a line (little endian bytes), as $readmemh.

The model assumes the sample data is always in the engine's buffers when a
voice needs it (a voice is prepared by writing its position one frame before
its gate), so the underrun / hold behaviour is not modelled.

usage: audio_ref.py script mem.hex out.txt [out.wav]
"""
import sys, struct, wave

BASE = 0x1000
LEG = 0x0400
ATK, DEC, SUS, REL, IDLE = 1, 2, 3, 4, 0

def s16(x):
    x &= 0xFFFF
    return x - 65536 if x & 0x8000 else x

def sat(x, bits):
    lo, hi = -(1 << (bits - 1)), (1 << (bits - 1)) - 1
    return max(lo, min(hi, x))

NZ = {0: (15, 1), 1: (15, 6), 2: (17, 3), 3: (23, 5)}

class Voice:
    def __init__(self):
        self.cfg = [0] * 32
        self.pos = 0; self.env = 0; self.stage = IDLE; self.lfsr = 0x7FFFFF
        self.run = self.arm = self.lg = self.dir = self.half = 0
        self.done = self.irq = self.under = self.pw = 0
        self.wrap = self.msb = 0
        self.last = (0, 0)

class Engine:
    def __init__(self, mem_words):
        self.mem = bytearray()
        for w in mem_words:
            self.mem += struct.pack('<I', w)
        self.v = [Voice() for _ in range(32)]
        self.master = 0x1000; self.fmode = 0; self.fcut = 0; self.fres = 0; self.envdiv = 0
        self.low = [0, 0]; self.band = [0, 0]
        self.frame_no = 0
        self.leg = [[0, 0, 0] for _ in range(3)]       # freq, vol, wave

    # ------------------------------------------------------------ registers
    def write(self, addr, val):
        val &= 0xFFFF
        if LEG <= addr < LEG + 0x18:
            vo, w = (addr - LEG) >> 3, ((addr - LEG) >> 1) & 3
            if vo < 3 and w < 3:
                self.leg[vo][w] = val & (0xFFFF, 0xF, 3)[w]
                self.leg_apply(vo)
        elif (addr >> 8) == (BASE >> 8):
            w = (addr >> 1) & 0x7F
            if w == 0: self.master = val
            elif w == 1: self.fmode = val & 7
            elif w == 2: self.fcut = val
            elif w == 3: self.fres = val
            elif w == 4: self.envdiv = val & 7
            elif w in (6, 7):
                m = val << (16 if w == 7 else 0)
                for i in range(32):
                    if m >> i & 1: self.v[i].irq = 0
        elif (addr >> 11) == ((BASE >> 11) + 1):
            vo, w = (addr >> 6) & 31, (addr >> 1) & 31
            v = self.v[vo]
            v.cfg[w] = val
            if w == 13: v.pw = 1
            if w == 14:
                if val & 2: v.done = 0
                if val & 4: v.irq = 0
                if val & 16: v.under = 0

    def leg_apply(self, vo):
        f, vol, wv = self.leg[vo]
        gate = 1 if (f and vol) else 0
        step = f * 349 + ((f * 134) >> 8)
        c = self.v[vo].cfg
        c[0] = ((wv + 1) << 2) | gate
        c[1] = step & 0xFFFF
        c[2] = (step >> 16) & 0xFF
        c[3] = c[4] = vol * 5
        c[5] = 0x800

    # ------------------------------------------------------------ memory
    def rd(self, a, n):
        a &= 0xFFFFFF
        return int.from_bytes(self.mem[a:a + n].ljust(n, b'\0'), 'little')

    @staticmethod
    def conv8(b, uns): return s16(((b ^ 0x80) if uns else b) << 8)
    @staticmethod
    def conv16(h, uns): return s16(h ^ 0x8000 if uns else h)

    def getframe(self, ba, b16, st, uns):
        if not st and not b16: l = self.conv8(self.rd(ba, 1), uns); return l, l
        if not st and b16: l = self.conv16(self.rd(ba & ~1, 2), uns); return l, l
        if st and not b16:
            h = self.rd(ba & ~1, 2); return self.conv8(h & 255, uns), self.conv8(h >> 8, uns)
        w = self.rd(ba & ~3, 4); return self.conv16(w & 0xFFFF, uns), self.conv16(w >> 16, uns)

    # ------------------------------------------------------------ one voice
    def voice(self, vi, n, acc):
        v = self.v[vi]; c = v.cfg
        ctrl = c[0]
        gate = ctrl & 1; enven = ctrl >> 1 & 1; src = ctrl >> 2 & 7
        ring = ctrl >> 5 & 1; sync = ctrl >> 6 & 1; interp = ctrl >> 7 & 1
        loopm = ctrl >> 8 & 3; b16 = ctrl >> 10 & 1; uns = ctrl >> 11 & 1
        stereo = ctrl >> 12 & 1; filt = ctrl >> 13 & 1; irqen = ctrl >> 14 & 1
        fshift = b16 + stereo
        step = ((c[2] & 0xFF) << 16) | c[1]
        vl, vr = c[3] & 0x1FF, c[4] & 0x1FF
        duty = c[5] & 0xFFF
        atk, dec, sus, rel = c[6], c[7], c[8], c[9]
        noise = c[10]
        addr = ((c[17] & 0xFF) << 16) | c[16]
        ls = ((c[19] & 0xFF) << 16) | c[18]
        end = ((c[21] & 0xFF) << 16) | c[20]
        pw = v.pw
        if not gate and not v.lg and v.env == 0 and not pw and not v.arm and not v.run:
            v.wrap = 0
            return
        v.pw = 0
        src_v = self.v[(vi - 1) & 31]
        # position, gate edge
        if pw:
            pos = (c[11] & 0xFFFF) | (c[12] << 16) | ((c[13] & 0xFF) << 32) if src == 0 else \
                  (c[11] & 0xFFFF) | ((c[12] & 0xFF) << 16)
        else:
            pos = v.pos if src == 0 else v.pos & 0xFFFFFF
        rise, fall = gate and not v.lg, (not gate) and v.lg
        stage = ATK if rise else (REL if fall and enven else v.stage)
        run = 1 if rise else v.run
        d = (0 if pw else v.dir) if loopm == 2 else 0
        env = v.env
        # envelope
        if not enven:
            env = 0xFFFFFF if gate else 0
        elif (n & ((1 << self.envdiv) - 1)) == 0:
            delta = max(1, (((v.env >> 8) * (rel if stage == REL else dec)) >> 8) & 0xFFFFFF)
            if stage == ATK:
                e = env + (atk << 4)
                if e >= 0xFFFFFF: env = 0xFFFFFF; stage = DEC
                else: env = e
            elif stage == DEC:
                e = env - delta
                if e <= sus << 8: env = sus << 8; stage = SUS
                else: env = e
            elif stage == REL:
                e = env - delta
                if e <= 0: env = 0; stage = IDLE
                else: env = e
        audible = (env != 0) if enven else gate
        if src == 0:
            act = run and audible
            run_n = run and (gate or audible)
            npos, nd, done = pos, d, 0
            if act:
                ba = (addr + ((pos >> 16) << fshift)) & 0xFFFFFF
                aL, aR = self.getframe(ba, b16, stereo, uns)
                bL, bR = self.getframe((ba + (1 << fshift)) & 0xFFFFFF, b16, stereo, uns)
                fr = (pos >> 8) & 0xFF if interp else 0
                xL = s16(aL + (((bL - aL) * fr) >> 8)); xR = s16(aR + (((bR - aR) * fr) >> 8))
                v.last = (xL, xR)
                ends, lsf = end << 16, ls << 16
                pn = pos - step if d else pos + step
                dE, dS, ln = pn - ends, pn - lsf, ends - lsf
                rf, rb, pn2 = 2 * ends - pn - 1, 2 * lsf - pn, (pn - ends) + lsf
                if not d:
                    if dE >= 0:
                        if loopm == 1 and ln > 0: npos = lsf if pn2 >= ends else pn2
                        elif loopm == 2 and ln > 0: npos = lsf if rf < 0 else rf; nd = 1
                        else: npos = ends; done = 1
                    else: npos = pn
                else:
                    if dS < 0: npos = rb; nd = 0
                    else: npos = pn
                npos &= (1 << 40) - 1
            else:
                xL = xR = 0
            run_n = run_n and not done
            mid = (ls + end) >> 1
            ih = 1 if (npos >> 16) >= mid else 0
            if irqen and (done or (loopm != 0 and ih != v.half and not pw)): v.irq = 1
            if done: v.done = 1
            v.half = ih
            v.arm = 0
            v.wrap = 0; v.msb = 0
            v.pos, v.dir, v.run = npos, nd, 1 if run_n else 0
        else:
            ph_old = pos & 0xFFFFFF
            s = ph_old + step
            syn = sync and src_v.wrap
            ph = 0 if syn else s & 0xFFFFFF
            lf = v.lfsr
            if src == 4:
                nb, tp = NZ[noise & 3]
                mask = (1 << nb) - 1
                l = lf & mask
                if l == 0: l = mask
                if not (ph_old >> 19 & 1) and (s >> 19 & 1) and not syn:
                    fb = (l ^ (l >> tp)) & 1
                    l = (l >> 1) | (fb << (nb - 1))
                lf = l
            ringb = ring and src_v.msb
            wm = (ph >> 23 & 1) ^ ringb
            if src == 1: w = 0x7FFF if (ph >> 12) < duty else -0x7FFF
            elif src == 2: w = s16((((ph >> 8) & 0x7FFF) ^ (0x7FFF if wm else 0)) << 1 ^ 0x8000)
            elif src == 3: w = s16((ph >> 8) ^ 0x8000)
            elif src == 4:
                if noise >> 2 & 1:
                    u = 0
                    for i, b in enumerate((22, 20, 16, 13, 11, 7, 4, 2)): u |= (lf >> b & 1) << (7 - i)
                    w = s16((u << 8) ^ 0x8000)
                else: w = 0x7FFF if lf & 1 else -0x7FFF
            else: w = 0
            xL = xR = w
            v.lfsr = lf
            v.pos = ph
            v.wrap = s >> 24 & 1
            v.msb = ph >> 23 & 1
            v.half = 0; v.run = 0; v.arm = 0; v.dir = d
        e16 = env >> 8
        mL = s16((xL * e16) >> 16); mR = s16((xR * e16) >> 16)
        k = 1 if filt else 0
        acc[k][0] += mL * vl
        acc[k][1] += mR * vr
        v.lg = gate; v.env = env; v.stage = stage

    def frame(self):
        n = self.frame_no
        acc = [[0, 0], [0, 0]]               # direct, filtered; L, R
        for vi in range(32): self.voice(vi, n, acc)
        out = []
        for ch in (0, 1):
            fin = sat(acc[1][ch] >> 8, 24)
            dl = acc[0][ch] >> 8
            low = sat(self.low[ch] + ((self.band[ch] * self.fcut) >> 15), 24)
            high = sat(fin - low - ((self.band[ch] * self.fres) >> 15), 24)
            band = sat(self.band[ch] + ((high * self.fcut) >> 15), 24)
            self.low[ch], self.band[ch] = low, band
            tot = dl + (low if self.fmode & 1 else 0) + (band if self.fmode & 2 else 0) + (high if self.fmode & 4 else 0)
            out.append(sat((tot * self.master) >> 12, 16))
        self.frame_no += 1
        return out

def load_script(path):
    ev, nframes = [], 0
    for line in open(path):
        t = line.split()
        if not t: continue
        if t[0] == 'W': ev.append((int(t[1]), int(t[2], 16), int(t[3], 16)))
        elif t[0] == 'M': ev.append((int(t[1]), -1 - int(t[2], 16), int(t[3], 16)))
        elif t[0] == 'E': nframes = int(t[1])
    return ev, nframes

def load_mem(path):
    return [int(l.strip(), 16) for l in open(path) if l.strip()]

def render(script, memhex):
    ev, nframes = load_script(script)
    eng = Engine(load_mem(memhex))
    out = []
    for f in range(nframes):
        for (fr, a, v) in ev:
            if fr == f:
                if a < 0:
                    eng.mem[(-1 - a) * 4:(-1 - a) * 4 + 4] = struct.pack('<I', v)
                else: eng.write(a, v)
        out.append(tuple(eng.frame()))
    return out

def write_wav(path, frames, rate=48000):
    w = wave.open(path, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(b''.join(struct.pack('<hh', l, r) for l, r in frames)); w.close()

if __name__ == '__main__':
    out = render(sys.argv[1], sys.argv[2])
    with open(sys.argv[3], 'w') as f:
        for l, r in out: f.write('%d %d\n' % (l, r))
    if len(sys.argv) > 4: write_wav(sys.argv[4], out)
