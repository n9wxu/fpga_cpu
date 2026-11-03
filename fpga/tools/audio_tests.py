#!/usr/bin/env python3
"""Audio engine regression: generates register scripts and sample memory for
a few tests, renders them with the reference model (audio_ref.py) and with
the RTL (fpga/sim/audio_tb.v, iverilog), compares bit for bit and writes
WAVs of both to the output directory.

usage: audio_tests.py OUTDIR [test ...]      tests: mod chip stream legacy stress
"""
import sys, os, math, struct, subprocess, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_ref as ref

HERE = os.path.dirname(os.path.abspath(__file__))
RTL = os.path.join(HERE, '..', 'rtl')
SIM = os.path.join(HERE, '..', 'sim')

V = 0x1800
def vreg(v, w): return V + v * 64 + w * 2
G_MASTER, G_FMODE, G_FCUT, G_FRES, G_ENVDIV = 0x1000, 0x1002, 0x1004, 0x1006, 0x1008

def ctrl(gate=0, enven=0, src=0, ring=0, sync=0, interp=0, loop=0, b16=0, uns=0, stereo=0, filt=0, irqen=0):
    return (gate | enven << 1 | src << 2 | ring << 5 | sync << 6 | interp << 7 | loop << 8 |
            b16 << 10 | uns << 11 | stereo << 12 | filt << 13 | irqen << 14)

class Script:
    def __init__(self): self.lines = []; self.frames = 0; self.mem = bytearray(1 << 20)
    def w(self, f, a, v): self.lines.append('W %d %x %x' % (f, a, v & 0xFFFF))
    def m(self, f, waddr, v): self.lines.append('M %d %x %x' % (f, waddr, v & 0xFFFFFFFF))
    def put(self, addr, data): self.mem[addr:addr + len(data)] = data
    def voice(self, f, v, addr, ls, end, step, vl, vr, c, pos=0):
        """configure voice v at frame f; the position write arms it (gate one frame later)"""
        self.w(f, vreg(v, 16), addr & 0xFFFF); self.w(f, vreg(v, 17), addr >> 16)
        self.w(f, vreg(v, 18), ls & 0xFFFF);   self.w(f, vreg(v, 19), ls >> 16)
        self.w(f, vreg(v, 20), end & 0xFFFF);  self.w(f, vreg(v, 21), end >> 16)
        self.w(f, vreg(v, 1), step & 0xFFFF);  self.w(f, vreg(v, 2), step >> 16)
        self.w(f, vreg(v, 3), vl); self.w(f, vreg(v, 4), vr)
        self.w(f, vreg(v, 0), c)
        self.setpos(f, v, pos)
    def setpos(self, f, v, pos):
        self.w(f, vreg(v, 11), pos & 0xFFFF); self.w(f, vreg(v, 12), (pos >> 16) & 0xFFFF)
        self.w(f, vreg(v, 13), pos >> 32)
    def write(self, name, outdir):
        sp = os.path.join(outdir, name + '.script'); mp = os.path.join(outdir, name + '.hex')
        with open(sp, 'w') as fh:
            lines = sorted(self.lines, key=lambda l: int(l.split()[1]))     # stable: the order within a frame is kept
            fh.write('\n'.join(lines) + '\nE %d\n' % self.frames)
        with open(mp, 'w') as fh:
            for i in range(0, len(self.mem), 4): fh.write('%08x\n' % struct.unpack_from('<I', self.mem, i)[0])
        return sp, mp

def pstep(period):                       # Amiga PAL period -> 8.16 step
    return round(3546895 / period / 48000 * 65536)
def fstep(hz):                           # chip frequency -> 24-bit phase step
    return round(hz * (1 << 24) / 48000)
def i8(x): return struct.pack('b', max(-128, min(127, int(x))))
def u8(x): return struct.pack('B', max(0, min(255, int(x))))
def i16(x): return struct.pack('<h', max(-32768, min(32767, int(x))))

def t_mod():
    s = Script(); s.frames = 6000
    S0, S1, S2, S3, S4 = 0x1000, 0x1100, 0x1400, 0x2000, 0x3000
    s.put(S0, b''.join(i8(100 * math.sin(2 * math.pi * i / 64)) for i in range(64)))
    s.put(S1, b''.join(i8(-110 + 220 * ((i * 7) % 200) / 200) for i in range(200)))
    rnd = random.Random(5)
    s.put(S2, b''.join(i8(rnd.randint(-120, 120) * (1 - i / 700)) for i in range(600)))
    s.put(S3, b''.join(i16(12000 * math.sin(2 * math.pi * i / 64)) + i16(9000 * math.sin(2 * math.pi * i / 41)) for i in range(256)))
    s.put(S4, b''.join(u8(128 + 120 * (1 - abs(i - 50) / 50)) for i in range(100)))
    s.w(0 + 1, G_MASTER, 0x0A00)
    # row 0: four channels, Amiga pan L R R L
    s.voice(1, 0, S0, 0, 64, pstep(428), 0x100, 0x000, ctrl(loop=1))
    s.voice(1, 1, S1, 40, 200, pstep(380), 0x000, 0xC0, ctrl(loop=1))
    s.voice(1, 2, S2, 0, 600, pstep(250), 0x000, 0x100, ctrl())
    s.voice(1, 3, S4, 10, 100, pstep(320), 0x100, 0x000, ctrl(loop=2, uns=1))
    for v in range(4): s.w(2, vreg(v, 0), ctrl(gate=1, loop=[1, 1, 0, 2][v], uns=(v == 3)))
    # row 1 (frame 1500): new notes, volume slides, sample offset, interpolation
    s.w(1500, vreg(0, 1), pstep(360) & 0xFFFF); s.w(1500, vreg(0, 3), 0x80)
    s.w(1500, vreg(2, 0), ctrl()); s.setpos(1500, 2, 100 << 16)
    s.w(1501, vreg(2, 0), ctrl(gate=1))
    s.w(1500, vreg(1, 0), ctrl(gate=1, loop=1, interp=1))
    for i in range(10): s.w(1600 + 40 * i, vreg(3, 4), 0x100 * i // 40)
    # row 2 (frame 3000): a 16-bit stereo voice with interpolation, centre, loop
    s.voice(3000, 4, S3, 0, 256, pstep(300), 0x100, 0x100, ctrl(loop=1, b16=1, stereo=1, interp=1))
    s.w(3001, vreg(4, 0), ctrl(gate=1, loop=1, b16=1, stereo=1, interp=1))
    s.w(3000, vreg(1, 0), ctrl())
    # row 3 (4500): voice 0 off, voice 2 again from the start (one-shot done flag cleared)
    s.w(4500, vreg(0, 0), ctrl(loop=1))
    s.w(4500, vreg(2, 14), 2)
    s.setpos(4500, 2, 0); s.w(4501, vreg(2, 0), ctrl()); s.w(4502, vreg(2, 0), ctrl(gate=1))
    return s

def t_chip():
    s = Script(); s.frames = 6000
    s.w(1, G_MASTER, 0x0600); s.w(1, G_ENVDIV, 1)
    s.w(1, G_FMODE, 1); s.w(1, G_FCUT, 0x1000); s.w(1, G_FRES, 0x5000)
    def chip(f, v, src, hz, vl=0x100, vr=0x100, **kw):
        s.w(f, vreg(v, 1), fstep(hz) & 0xFFFF); s.w(f, vreg(v, 2), fstep(hz) >> 16)
        s.w(f, vreg(v, 3), vl); s.w(f, vreg(v, 4), vr)
        s.w(f, vreg(v, 0), ctrl(src=src, **kw))
    # voice 0: pulse with ADSR into the filter
    for w, val in ((5, 0x400), (6, 0x2000), (7, 0x0800), (8, 0x6000), (9, 0x0300)): s.w(1, vreg(0, w), val)
    chip(1, 0, 1, 220, 0x180, 0x80, enven=1, filt=1)
    s.w(2, vreg(0, 0), ctrl(gate=1, enven=1, src=1, filt=1))
    # voice 1: triangle, ring-modulated by voice 0, filtered
    chip(1, 1, 2, 331, 0x80, 0x180, ring=1, filt=1)
    s.w(2, vreg(1, 0), ctrl(gate=1, src=2, ring=1, filt=1))
    # voice 2: saw hard-synced to voice 1 (not filtered)
    chip(1, 2, 3, 97, 0x60, 0x60, sync=1)
    s.w(2, vreg(2, 0), ctrl(gate=1, src=3, sync=1))
    # voice 3: short noise (NES periodic) with an envelope; voice 4: SID-style multi-level noise
    for w, val in ((6, 0x8000), (7, 0x1000), (8, 0x2000), (9, 0x1000), (10, 1)): s.w(1, vreg(3, w), val)
    chip(1, 3, 4, 3000, 0x60, 0x60, enven=1)
    s.w(2, vreg(3, 0), ctrl(gate=1, enven=1, src=4))
    s.w(1, vreg(4, 10), 3 | 4)
    chip(1, 4, 4, 1500, 0x50, 0x50)
    s.w(2500, vreg(4, 0), ctrl(gate=1, src=4))
    # changes: duty sweep, cutoff sweep, gates
    for i in range(1, 12):
        s.w(500 * i, vreg(0, 5), 0x100 + 0x120 * i)
        s.w(500 * i, G_FCUT, 0x0800 + 0x0600 * i)
    s.w(3000, vreg(0, 0), ctrl(gate=0, enven=1, src=1, filt=1))     # release
    s.w(3000, vreg(3, 0), ctrl(gate=0, enven=1, src=4))
    s.w(3400, vreg(3, 0), ctrl(gate=1, enven=1, src=4))             # retrigger
    s.w(3600, G_FMODE, 4); s.w(4200, G_FMODE, 2); s.w(4800, G_FMODE, 5)
    s.w(5000, vreg(1, 1), fstep(440) & 0xFFFF); s.w(5000, vreg(1, 2), fstep(440) >> 16)
    return s

def t_legacy():
    s = Script(); s.frames = 3000
    L = 0x400
    s.w(1, L + 0, 440); s.w(1, L + 2, 15); s.w(1, L + 4, 0)
    s.w(500, L + 8, 660); s.w(500, L + 10, 10); s.w(500, L + 12, 1)
    s.w(1000, L + 16, 3000); s.w(1000, L + 18, 8); s.w(1000, L + 20, 3)
    s.w(1500, L + 0, 880); s.w(1500, L + 12, 2)
    s.w(2000, L + 2, 0); s.w(2200, L + 8, 0)
    return s

def t_stream():
    s = Script(); s.frames = 3000
    N = 512; BUF = 0x8000                 # 512 stereo 16-bit frames = 2 KB
    def block(seed): return [(int(9000 * math.sin(2 * math.pi * i * (3 + seed) / 128)) & 0xFFFF) |
                             (int(7000 * math.sin(2 * math.pi * i * (5 + seed) / 128)) & 0xFFFF) << 16 for i in range(N // 2)]
    gen = 0
    first = block(0) + block(1)
    for i, wv in enumerate(first): s.mem[BUF + 4 * i:BUF + 4 * i + 4] = struct.pack('<I', wv)
    s.voice(1, 0, BUF, 0, N, 0x10000, 0x100, 0x100, ctrl(loop=1, b16=1, stereo=1, irqen=1))
    s.w(2, vreg(0, 0), ctrl(gate=1, loop=1, b16=1, stereo=1, irqen=1))
    # playback frame f (>=2) is at position f-2. The half flips at position 256 and at the wrap.
    # CPU: after a flip, refill the half just left (writes, then clear the flag)
    f0 = 2
    k = 1
    for flip in range(f0 + N // 2, s.frames - 20, N // 2):
        k += 1
        half = 0 if ((flip - f0) // (N // 2)) % 2 == 0 else 1       # the half just entered
        refill = 1 - half
        base = (BUF >> 2) + refill * (N // 2)
        for i, wv in enumerate(block(k)): s.m(flip + 3, base + i, wv)
        s.w(flip + 4, vreg(0, 14), 4)                                   # clear the voice's irq flag
        s.w(flip + 4, 0x100C, 1)                                        # (and via the global register)
    # a second, 8-bit mono voice, one-shot with a done flag
    s.put(0x4000, b''.join(i8(90 * math.sin(2 * math.pi * i / 33)) for i in range(300)))
    s.voice(1000, 1, 0x4000, 0, 300, 0x8000, 0x80, 0x80, ctrl(interp=1, irqen=1))
    s.w(1001, vreg(1, 0), ctrl(gate=1, interp=1, irqen=1))
    return s

def t_stress():
    s = Script(); s.frames = 700
    rnd = random.Random(9)
    base = 0x10000
    for i in range(32):
        a = base + i * 0x800
        s.put(a, b''.join(i16(rnd.randint(-9000, 9000)) for _ in range(512)))
    s.w(1, G_MASTER, 0x0300); s.w(1, G_FMODE, 3); s.w(1, G_FCUT, 0x2000); s.w(1, G_FRES, 0x6000)
    for v in range(32):
        a = base + v * 0x800
        if v % 4 == 3:
            src = 1 + (v // 4) % 4
            s.w(1, vreg(v, 1), fstep(100 + 37 * v) & 0xFFFF); s.w(1, vreg(v, 2), fstep(100 + 37 * v) >> 16)
            s.w(1, vreg(v, 3), 0x20); s.w(1, vreg(v, 4), 0x20); s.w(1, vreg(v, 5), 0x600)
            s.w(2, vreg(v, 0), ctrl(gate=1, src=src, filt=v % 8 == 3, ring=v % 8 == 7, sync=v % 8 == 3))
        else:
            c = ctrl(loop=1, b16=1, stereo=1, interp=1, filt=v % 3 == 0)
            s.voice(1, v, a, 0, 256, 0x9000 + 0x300 * v, 0x20 + v, 0x40 - v, c, pos=0)
            s.w(40, vreg(v, 0), c | 1)               # 24 voices start in one tick: the fetches take a few frames
    return s

TESTS = {'mod': t_mod, 'chip': t_chip, 'legacy': t_legacy, 'stream': t_stream, 'stress': t_stress}

def run_rtl(outdir, name, sp, mp, extra=()):
    vvp = os.path.join(outdir, 'tb.vvp')
    if not os.path.exists(vvp):
        subprocess.check_call(['iverilog', '-g2012', '-o', vvp, os.path.join(SIM, 'audio_tb.v'),
                               os.path.join(RTL, 'audio_engine.v'), os.path.join(RTL, 'audio_regs.v'),
                               os.path.join(RTL, 'rammux.v')])
    out = os.path.join(outdir, name + '.rtl.txt')
    r = subprocess.run(['vvp', vvp, '+script=' + sp, '+mem=' + mp, '+out=' + out] + list(extra),
                       capture_output=True, text=True)
    return out, r.stdout

def main():
    outdir = sys.argv[1]; os.makedirs(outdir, exist_ok=True)
    names = sys.argv[2:] or list(TESTS)
    ok = True
    for name in names:
        s = TESTS[name](); sp, mp = s.write(name, outdir)
        model = ref.Engine(ref.load_mem(mp))
        ev, nfr = ref.load_script(sp)
        frames = []
        for f in range(nfr):
            for (fr, a, v) in ev:
                if fr == f:
                    if a < 0: model.mem[(-1 - a) * 4:(-1 - a) * 4 + 4] = struct.pack('<I', v)
                    else: model.write(a, v)
            frames.append(tuple(model.frame()))
        ref.write_wav(os.path.join(outdir, name + '.model.wav'), frames)
        extra = ['+slow'] if name in ('stress', 'stream') else []
        out, log = run_rtl(outdir, name, sp, mp, extra)
        got = [tuple(int(x) for x in l.split()) for l in open(out)] if os.path.exists(out) else []
        bad = [i for i in range(min(len(got), len(frames))) if got[i] != frames[i]]
        ref.write_wav(os.path.join(outdir, name + '.rtl.wav'), got)
        peak = max((max(abs(l), abs(r)) for l, r in frames), default=0)
        status = 'PASS' if len(got) == len(frames) and not bad else 'FAIL'
        if status == 'FAIL': ok = False
        print('%-7s %s  frames %d/%d  mismatches %d  peak %d' % (name, status, len(got), len(frames), len(bad), peak))
        if bad:
            i = bad[0]; print('   first at frame %d: rtl %s model %s' % (i, got[i], frames[i]))
        for l in log.strip().splitlines():
            if l.startswith(('RD', 'done', 'timeout')): print('   ' + l)
        # state readback check against the model
        v0 = model.v[0]
        if any(l.startswith('RD 11') for l in log.splitlines()):
            exp = {11: v0.pos & 0xFFFF, 12: (v0.pos >> 16) & 0xFFFF, 13: (v0.pos >> 32) & 0xFF}
            print('   model voice 0: pos %x env %x run %d done %d irq %d half %d' % (v0.pos, v0.env, v0.run, v0.done, v0.irq, v0.half))
    if 'stress' in names:
        s = t_stress(); sp, mp = s.write('stress_t', outdir)
        out, log = run_rtl(outdir, 'stress_t', sp, mp, ['+turbo'])
        print(next((l for l in log.splitlines() if l.startswith('turbo')), 'turbo: no output'))
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
