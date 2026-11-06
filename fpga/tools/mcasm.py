#!/usr/bin/env python3
"""The microcoded audio engine's assembler and its routines (docs/mcaudio.md, fpga/rtl/mcaudio).

usage: mcasm.py --hex cs.hex            the control store, one 72-bit word a line (testbenches)
       mcasm.py --c mc_cs.h             a C header: the words (5 x 16 bits each) and the entries
       mcasm.py --list                  a listing

A routine is a node's per-block code (envelope, LFOs, sample DMA: the "script", as microcode on the
same datapath) followed by its per-sample loop (bc LOOP, 32 times) and the write-back of its state.
Timing rules the code below keeps (mc_core.v): MDR is the read issued two words before; a product
reaches acc two words after its MAC word; flags set by a word steer the next word's branch or
predicate; a bus add reads acc at its own word.
"""
import argparse
import sys

FIELDS = dict(nxt=(63, 9), bc=(60, 3), aop=(56, 4), rd=(52, 4), ra=(48, 4), rb=(44, 4), bsel=(42, 2),
              setf=(41, 1), mop=(38, 3), masel=(36, 2), mbsel=(34, 2), memop=(31, 3), sh=(26, 5),
              sfmt=(24, 2), pred=(22, 2), imm=(0, 16))
AOP = dict(add=1, sub=2, and_=3, or_=4, xor=5, shr=6, sar=7, mov=8, min=10, max=11, acc=12, sys=13,
           lfsr=14, wave=15)
BC = dict(j=1, jz=2, jnz=3, jn=4, jnn=5, loop=6, end=7)
PRED = dict(c=1, nc=2, n=3)


class Asm:
    def __init__(self):
        self.words, self.labels, self.fix, self.entries = [], {}, [], {}

    def label(self, name):
        assert name not in self.labels, name
        self.labels[name] = len(self.words)

    def entry(self, name):
        self.label(name)
        self.entries[name] = len(self.words)

    def w(self, *parts, go=None, end=False):
        f = {}
        for p in parts:
            for k, v in p.items():
                if k in f and f[k] != v:
                    raise ValueError("field %s twice in word %d: %r" % (k, len(self.words), parts))
                f[k] = v
        if go:
            cond, lab = go
            assert 'bc' not in f
            f['bc'] = BC[cond]
            self.fix.append((len(self.words), lab))
        if end:
            f['bc'] = BC['end']
        self.words.append(f)

    def check_mdr(self):
        """MDR is the read issued two words before (mc_core.v): on every path into a word that uses
        it, the word two back must have issued a read"""
        n = len(self.words)
        pred = [set() for _ in range(n)]
        for i, f in enumerate(self.words):
            if i + 1 < n and f.get('bc', 0) not in (BC['j'], BC['end']):
                pred[i + 1].add(i)
        for src, lab in self.fix:
            pred[self.labels[lab]].add(src)
        reads = [f.get('memop', 0) in (1, 3, 4, 6) for f in self.words]
        bad = []
        for j, f in enumerate(self.words):
            uses = (f.get('aop', 0) and f.get('bsel', 0) == 3) or (f.get('mop', 0) and f.get('masel', 0) >= 2)
            if not uses:
                continue
            for p1 in pred[j]:
                for p2 in pred[p1]:
                    if not reads[p2]:
                        bad.append((j, p1, p2))
        lab = {v: k for k, v in self.labels.items()}
        for j, p1, p2 in bad:
            print("mcasm: word %d (%s) uses MDR, but on the path %d <- %d, word %d issued no read" %
                  (j, lab.get(j, ''), p2, p1, p2), file=sys.stderr)
        return not bad

    def assemble(self):
        assert self.check_mdr(), "MDR timing"
        out = []
        for i, f in enumerate(self.words):
            f = dict(f)
            for wi, lab in self.fix:
                if wi == i:
                    f['nxt'] = self.labels[lab]
            v = 0
            for k, x in f.items():
                lo, n = FIELDS[k]
                assert 0 <= x < (1 << n), (i, k, x)
                v |= x << lo
            out.append(v)
        assert len(out) <= 512, len(out)
        return out


# ---- the parts of a word
def imm16(v):
    return v & 0xFFFF

def alu(op, rd, ra=0, rb=None, i=None, hi=None, mdr=False, setf=False, pred=None):
    """rd = ra OP b; b: rb, or #i (sign-extended), or #hi << 16, or MDR"""
    f = dict(aop=AOP[op], rd=rd)
    if op != 'mov': f['ra'] = ra                          # (a move takes only B)
    if rb is not None: f.update(rb=rb, bsel=0)
    elif i is not None: f.update(bsel=1, imm=imm16(i))
    elif hi is not None: f.update(bsel=2, imm=imm16(hi))
    elif mdr: f.update(bsel=3)
    if setf: f['setf'] = 1
    if pred: f['pred'] = PRED[pred]
    return f

def mov(rd, rb=None, i=None, hi=None, mdr=False, pred=None):
    return alu('mov', rd, 0, rb=rb, i=i, hi=hi, mdr=mdr, pred=pred)

def mac(a, b, add=False, sub=False):
    """acc (=, +=, -=) a * b; a: ('r', n, half) or ('mdr', half); b: ('r', n, half|'u') or ('i', v)"""
    f = dict(mop=3 if sub else 2 if add else 1)
    if a[0] == 'r': f.update(ra=a[1], masel=a[2])
    else: f.update(masel=2 + a[1])
    if b[0] == 'i': f.update(mbsel=2, imm=imm16(b[1]))
    elif b[2] == 'u': f.update(rb=b[1], mbsel=3)
    else: f.update(rb=b[1], mbsel=b[2])
    return f

def accr(rd, point):
    """rd = acc at binary point 0, 8, 15 or 16"""
    return dict(aop=AOP['acc'], rd=rd, sh={0: 0, 8: 1, 15: 2, 16: 3}[point])

def shift(op, rd, ra, n, setf=False):
    f = dict(aop=AOP[op], rd=rd, ra=ra, sh=n)
    if setf: f['setf'] = 1
    return f

def wave(rd, ra, kind, rb=0, ring=False):
    return dict(aop=AOP['wave'], rd=rd, ra=ra, rb=rb, sh={'saw': 0, 'tri': 1, 'pulse': 2, 'noise': 3}[kind] | (4 if ring else 0))

def lfsr(rd, ra, pred=None):
    f = dict(aop=AOP['lfsr'], rd=rd, ra=ra)
    if pred: f['pred'] = PRED[pred]
    return f

def nr(w): return dict(memop=1, imm=w)
def nw(w, ra): return dict(memop=2, imm=w, ra=ra)
def samp(ra, base=0, fmt=0): return dict(memop=3, ra=ra, imm=base, sfmt=fmt)
def tbl(ra): return dict(memop=4, ra=ra)
def busadd(bus, point=15): return dict(memop=5, imm=bus, sh={0: 0, 8: 1, 15: 2, 16: 3}[point])
def busrd(bus): return dict(memop=6, imm=bus)
def dma(ra, rb, n): return dict(aop=AOP['sys'], sh=0, ra=ra, rb=rb, imm=n)
def route(ra, i=0): return dict(aop=AOP['sys'], sh=3, ra=ra, imm=i)

# ---- node layout (words; a group's second node is at +16)
#   all voices: w3 [7:0] trigger count (a change: note on), [8] gate, [12] to the filter bus;
#               w4 volume {R, L} Q15; w9 attack << 16 (Q15 a block); w10 {release, decay} (Q15 rates
#               a block, toward 0 and toward sustain); w11 sustain << 16; state w13 meter (the
#               envelope, Q15 << 16), w14 envelope {level << 16, release 0x200, attack 0x100, trigger}
#   osc:  w2 step (0.32 cycles a sample), w5 pulse width (0-4095), w6 LFO rate << 16 (a block),
#         w7 PWM depth (width units), w8 vibrato depth (Q15); state w12 phase, w15 LFO phase (noise:
#         its LFSR, start it at 1)
#   smp:  w2 step 16.16, w5 the sample's RAM word address, w6 end << 16 (frames, a multiple of 4),
#         w7 loop length << 16 (a multiple of 4; 0 = one-shot), w8 loop start's RAM word address;
#         state w12 position 16.16 (frames; under 32768)
#   sync: node 0 the master: w2 step, w5 mode (bit 0 hard sync, bit 1 ring: the slave a triangle
#         ring-modulated by the master), volume and envelope as above (one envelope for both);
#         node 1 the slave: w18 step, w20 volume {R, L}; state w12 master phase, w28 slave phase
#   flt:  w2 cutoff F (2 sin(pi fc / 48000) Q15), w3 damping (1/Q, Q15), w4 output volume {R, L},
#         w5 LFO rate << 16, w6 sweep depth (F units), w7 mode (0 low-pass, 1 band-pass); state w12
#         low, w13 meter (F now), w14 band, w15 LFO phase. Mono: the filter buses' L + R.

def envelope(a, b, tag):
    """the voice's envelope for this block, and its routing. Leaves r2 = w3, r8 = the level at the
    block's start, r9 = its step a sample, r10 = the level at its end (all << 16), r11 = 0 unless a
    note-on came. Uses r2-r11. (MDR: the read two words before.)"""
    a.w(nr(b + 3))
    a.w(nr(b + 14))
    a.w(mov(2, mdr=True))                                 # w3
    a.w(mov(3, mdr=True))                                 # w14
    a.w(alu('and_', 8, 3, hi=0x7FFF))                     # the level now
    a.w(alu('and_', 4, 2, i=0xFF))                        # trigger count
    a.w(alu('and_', 5, 3, i=0xFF))
    a.w(alu('sub', 11, 4, rb=5, setf=True))               # note on?
    a.w(alu('and_', 6, 3, i=0x300), go=('jz', tag + 'nt'))
    a.w(mov(6, i=0x100))                                  # note on: attack (not released)
    a.label(tag + 'nt')
    a.w(alu('and_', 7, 2, i=0x100, setf=True))            # gate
    a.w(mov(10, rb=8), go=('jnz', tag + 'g'))
    a.w(mov(6, i=0x200))                                  # gate off: release
    a.label(tag + 'g')
    a.w(alu('and_', 7, 6, i=0x100, setf=True))            # attacking?
    a.w(nr(b + 9), go=('jz', tag + 'dec'))
    a.w()
    a.w(alu('add', 10, 10, mdr=True))                     # attack: up by w9
    a.w(alu('sub', 7, 10, hi=0x7FFF, setf=True))          # C: at the top
    a.w(mov(10, hi=0x7FFF, pred='c'))
    a.w(mov(6, i=0, pred='c'), go=('j', tag + 'out'))     # then decay
    a.label(tag + 'dec')
    a.w(nr(b + 10))
    a.w(nr(b + 11))
    a.w(mov(5, mdr=True))                                 # {release, decay}
    a.w(mov(4, mdr=True))                                 # sustain << 16: the target
    a.w(alu('and_', 7, 6, i=0x200, setf=True))            # released?
    a.w(alu('sub', 7, 4, rb=10), go=('jz', tag + 'dk'))   # toward the sustain level at rate D
    a.w(mov(4, i=0))
    a.w(alu('sub', 7, 4, rb=10))                          # toward 0 at rate R
    a.w(mac(('r', 7, 1), ('r', 5, 1)), go=('j', tag + 'dk2'))
    a.label(tag + 'dk')
    a.w(mac(('r', 7, 1), ('r', 5, 0)))
    a.label(tag + 'dk2')
    a.w()
    a.w(accr(7, 0))                                       # (T - L) * K: Q30, so << 1 for << 16 Q15
    a.w(alu('add', 7, 7, rb=7))
    a.w(alu('add', 10, 10, rb=7))
    a.label(tag + 'out')
    a.w(alu('max', 10, 10, i=0))
    a.w(alu('and_', 10, 10, hi=0x7FFF))
    a.w(alu('and_', 4, 2, i=0xFF))
    a.w(alu('or_', 4, 4, rb=6))
    a.w(alu('or_', 4, 4, rb=10))
    a.w(nw(b + 14, 4))
    a.w(nw(b + 13, 10))
    a.w(alu('sub', 9, 10, rb=8))
    a.w(shift('sar', 9, 9, 5))                            # a 32nd of the change a sample
    a.w(shift('shr', 7, 2, 11))                           # w3 bit 12: route to the filter buses
    a.w(route(7))


def osc(a):
    for kind, code in (('saw', 0), ('tri', 1), ('pulse', 2)):
        a.entry('osc_' + kind)
        a.w(mov(14, i=code), go=('j', 'osc'))
    a.entry('osc_noise')
    a.w(mov(14, i=3))
    a.label('osc')
    envelope(a, 0, 'o')
    a.w(nr(2))
    a.w(nr(12))
    a.w(mov(1, mdr=True), nr(15))                         # step
    a.w(mov(0, mdr=True), nr(6))                          # phase
    a.w(mov(12, mdr=True), nr(4))                         # LFO phase (noise: LFSR)
    a.w(alu('add', 13, 12, mdr=True))                     # the LFO, a block on (w6)
    a.w(mov(15, mdr=True))                                # volume (w4)
    a.w(alu('sub', 7, 14, i=3, setf=True))
    a.w(go=('jz', 'osc_n'))
    a.w(mov(12, rb=13))
    a.w(nw(15, 12))
    a.w(tbl(12))                                          # sin(LFO)
    a.w(nr(8))
    a.w(mov(13, mdr=True), nr(7))                         # sin
    a.w(mac(('mdr', 0), ('r', 13, 0)))                    # vibrato depth (w8) * sin
    a.w(mov(6, mdr=True))                                 # PWM depth (w7)
    a.w(mac(('r', 6, 0), ('r', 13, 0)))                   # PWM depth * sin
    a.w(accr(4, 15))                                      # m, Q15
    a.w(accr(5, 15))                                      # the width's offset
    a.w(shift('shr', 7, 1, 16), nr(5))
    a.w(mac(('r', 7, 0), ('r', 4, 0)))                    # step_hi * m
    a.w(alu('add', 11, 5, mdr=True))                      # the width (w5)
    a.w(accr(7, 0))
    a.w(alu('add', 7, 7, rb=7))
    a.w(alu('add', 1, 1, rb=7))                           # step * (1 + m)
    a.w(alu('sub', 7, 14, i=1, setf=True))                # 0 saw, 1 triangle, 2 pulse
    a.w(go=('jn', 'osc_saw_l'))
    a.w(go=('jz', 'osc_tri_l'))
    a.w(go=('j', 'osc_pulse_l'))
    for kind in ('saw', 'tri', 'pulse'):
        a.label('osc_%s_l' % kind)
        top = 'osc_%s_l' % kind
        a.w(wave(4, 0, kind, rb=11))
        a.w(mac(('r', 4, 0), ('r', 8, 1)))                # * envelope
        a.w(alu('add', 0, 0, rb=1))                       # phase
        a.w(alu('add', 8, 8, rb=9))                       # envelope ramp
        a.w(accr(5, 15))
        a.w(mac(('r', 5, 0), ('r', 15, 0)))
        a.w(mac(('r', 5, 0), ('r', 15, 1)))
        a.w(busadd(0))
        a.w(busadd(1), go=('loop', top))
        a.w(nw(12, 0), go=('j', 'osc_end'))
    a.label('osc_n')
    a.label('osc_noise_l')
    a.w(alu('add', 0, 0, rb=1, setf=True))                # the phase; C: a new noise value
    a.w(lfsr(12, 12, pred='c'))
    a.w(wave(4, 12, 'noise'))
    a.w(mac(('r', 4, 0), ('r', 8, 1)))
    a.w(alu('add', 8, 8, rb=9))
    a.w()
    a.w(accr(5, 15))
    a.w(mac(('r', 5, 0), ('r', 15, 0)))
    a.w(mac(('r', 5, 0), ('r', 15, 1)))
    a.w(busadd(0))
    a.w(busadd(1), go=('loop', 'osc_noise_l'))
    a.w(nw(15, 12))
    a.w(nw(12, 0))
    a.label('osc_end')
    a.w(end=True)


def smp(a, bits=8):
    """a sample voice: 8-bit (smp) or 16-bit (smp16) frames. Per block it DMAs a window from its
    position (8-bit: 32 words, 128 frames; 16-bit: 48 words, 96 frames) and, looping and near the
    end, the loop's start right after the end, so the loop reads on through the loop point; it
    interpolates linearly. (A loop shorter than the window can glitch: keep loops >= 256 frames.)"""
    t = 's' if bits == 8 else 'h'
    sh, nwin, fmt = (18, 2, 0) if bits == 8 else (17, 3, 1)   # frames to words: >> 2 or >> 1
    a.entry('smp' if bits == 8 else 'smp16')
    envelope(a, 0, t)
    a.w(nr(12))
    a.w(nr(7))
    a.w(mov(0, mdr=True), nr(6))                          # position
    a.w(mov(2, mdr=True), nr(4))                          # loop length
    a.w(mov(3, mdr=True), nr(2))                          # end
    a.w(mov(15, mdr=True), nr(5))                         # volume
    a.w(mov(1, mdr=True))                                 # step
    a.w(mov(4, mdr=True))                                 # the sample's address
    a.w(alu('or_', 7, 11, rb=11, setf=True))
    a.w(go=('jz', t + '_nt'))
    a.w(mov(0, i=0))                                      # note on: from the start
    a.label(t + '_nt')
    a.w(alu('or_', 7, 2, rb=2, setf=True))
    a.w(mov(11, i=0), go=('jz', t + '_os'))               # r11: -1 looping, 0 one-shot
    a.w(mov(11, i=-1))
    a.label(t + '_os')
    a.w(alu('sub', 7, 0, rb=3, setf=True))                # a one-shot already ended: quiet
    a.w(alu('and_', 8, 8, rb=11, pred='c'))
    a.w(alu('and_', 9, 9, rb=11, pred='c'))
    # window A from the position's word
    a.w(shift('shr', 5, 0, sh))
    a.w(alu('add', 4, 4, rb=5))
    a.w(mov(7, hi=1 << (sh - 16)))
    a.w(alu('sub', 7, 7, i=1))
    a.w(alu('and_', 13, 0, rb=7))                         # q: the position in the window
    a.w(mov(12, hi=0x0001))
    a.w(alu('add', 12, 12, rb=13))                        # q + 1 frame
    a.w(mov(6, i=0))
    for k in range(nwin):
        a.w(dma(4, 6, 16))
        if k + 1 < nwin:
            a.w(alu('add', 4, 4, i=16))
            a.w(alu('add', 6, 6, i=16))
    # window B, looping and near the end
    a.w(alu('or_', 7, 11, rb=11, setf=True))
    a.w(shift('shr', 6, 3, sh), go=('jz', t + '_l'))
    a.w(alu('sub', 6, 6, rb=5))                           # where the end falls in the buffer
    a.w(alu('sub', 7, 6, i=16 * nwin, setf=True))
    a.w(nr(8))
    a.w(go=('jnn', t + '_l'))
    a.w(mov(4, mdr=True))                                 # the loop start's address (w8)
    for k in range(nwin):
        a.w(dma(4, 6, 16))
        if k + 1 < nwin:
            a.w(alu('add', 4, 4, i=16))
            a.w(alu('add', 6, 6, i=16))
    a.label(t + '_l')
    a.w(samp(13, fmt=fmt))                                # s0
    a.w(samp(12, fmt=fmt))                                # s1
    a.w(mov(4, mdr=True))                                 # s0 (MDR holds it in both halves: use the low)
    a.w(mac(('mdr', 0), ('i', 1)))                        # acc = s1
    a.w(mac(('r', 4, 0), ('i', 1), sub=True))             # acc -= s0: the difference, signed, 17 bits
    a.w()
    a.w(accr(5, 0))
    a.w(shift('sar', 5, 5, 1))                            # (s1 - s0) / 2
    a.w(mac(('r', 5, 0), ('r', 13, 'u')))                # * fraction
    a.w(alu('add', 8, 8, rb=9))                           # envelope ramp
    a.w(accr(6, 15))
    a.w(alu('add', 6, 6, rb=4))                           # interpolated
    a.w(mac(('r', 6, 0), ('r', 8, 1)))                    # * envelope
    a.w(alu('add', 13, 13, rb=1))                         # q, q + 1 and the position, a step on
    a.w(alu('add', 12, 12, rb=1))
    a.w(accr(6, 15))
    a.w(mac(('r', 6, 0), ('r', 15, 0)))                   # L
    a.w(mac(('r', 6, 0), ('r', 15, 1)))                   # R
    a.w(busadd(0), alu('add', 0, 0, rb=1))                # (acc: L here, R the next word)
    a.w(busadd(1), alu('sub', 7, 0, rb=3, setf=True))     # C: at the end
    a.w(alu('sub', 0, 0, rb=2, pred='c'))                 # loop back (one-shot: by 0)
    a.w(alu('and_', 8, 8, rb=11, pred='c'))               # one-shot: quiet from here
    a.w(alu('and_', 9, 9, rb=11, pred='c'))
    a.w(alu('min', 0, 0, rb=3), go=('loop', t + '_l'))
    a.w(nw(12, 0), end=True)


def sync(a):
    a.entry('sync')
    envelope(a, 0, 'y')
    a.w(nr(2))
    a.w(nr(12))
    a.w(mov(1, mdr=True), nr(18))                         # master step
    a.w(mov(0, mdr=True), nr(28))                         # master phase
    a.w(mov(3, mdr=True), nr(4))                          # slave step
    a.w(mov(2, mdr=True), nr(20))                         # slave phase
    a.w(mov(15, mdr=True), nr(5))                         # master volume
    a.w(mov(14, mdr=True))                                # slave volume
    a.w(mov(6, mdr=True))                                 # mode
    a.w(alu('and_', 7, 6, i=1, setf=True))
    a.w(mov(12, i=-1), go=('jz', 'y_ns'))                 # r12: 0 with hard sync
    a.w(mov(12, i=0))
    a.label('y_ns')
    a.w(alu('and_', 7, 6, i=2, setf=True))
    a.w(go=('jnz', 'y_ring'))
    for name, mk, sk, ring in (('y_sync', 'saw', 'saw', False), ('y_ring', 'tri', 'tri', True)):
        a.label(name)
        a.w(alu('add', 0, 0, rb=1, setf=True))            # master; C: it wrapped
        a.w(alu('add', 2, 2, rb=3))
        a.w(alu('and_', 2, 2, rb=12, pred='c'))           # hard sync
        a.w(wave(4, 0, mk))
        a.w(wave(5, 2, sk, rb=0, ring=ring))
        a.w(mac(('r', 4, 0), ('r', 8, 1)))
        a.w(mac(('r', 5, 0), ('r', 8, 1)))
        a.w(accr(4, 15))
        a.w(accr(5, 15))
        a.w(mac(('r', 4, 0), ('r', 15, 0)))
        a.w(mac(('r', 5, 0), ('r', 14, 0), add=True))
        a.w(mac(('r', 4, 0), ('r', 15, 1)))
        a.w(mac(('r', 5, 0), ('r', 14, 1), add=True), busadd(0))
        a.w(alu('add', 8, 8, rb=9))
        a.w(busadd(1), go=('loop', name))
        a.w(go=('j', 'y_end'))
    a.label('y_end')
    a.w(nw(12, 0))
    a.w(nw(28, 2), end=True)


def flt(a):
    a.entry('flt')
    a.w(nr(2))
    a.w(nr(3))
    a.w(mov(1, mdr=True), nr(4))                          # F
    a.w(mov(2, mdr=True), nr(12))                         # damping
    a.w(mov(15, mdr=True), nr(14))                        # output volume
    a.w(mov(10, mdr=True), nr(15))                        # low
    a.w(mov(11, mdr=True), nr(5))                         # band
    a.w(mov(12, mdr=True), nr(6))                         # the LFO
    a.w(alu('add', 12, 12, mdr=True), nr(7))              # its rate (w5)
    a.w(mov(13, mdr=True))                                # sweep depth (w6)
    a.w(mov(14, mdr=True))                                # mode (w7)
    a.w(nw(15, 12))
    a.w(tbl(12))
    a.w()
    a.w(mac(('mdr', 0), ('r', 13, 0)))                    # sin * depth
    a.w()
    a.w(accr(7, 15))
    a.w(alu('add', 1, 1, rb=7))
    a.w(alu('max', 1, 1, i=0x0100))
    a.w(alu('min', 1, 1, i=0x7000))
    a.w(nw(13, 1))                                        # the meter: F now
    a.w(alu('sub', 7, 14, i=1, setf=True))                # C: band-pass
    a.label('f_l')
    a.w(busrd(2))
    a.w(busrd(3))
    a.w(mov(4, mdr=True))
    a.w(alu('add', 4, 4, mdr=True))
    a.w(shift('sar', 4, 4, 1))
    a.w(alu('max', 4, 4, i=-32767))
    a.w(alu('min', 4, 4, i=32767))
    a.w(mac(('r', 11, 0), ('r', 1, 0)))                   # band * F
    a.w(mac(('r', 11, 0), ('r', 2, 0)))                   # band * damping
    a.w(accr(5, 15))
    a.w(accr(6, 15))
    a.w(alu('add', 10, 10, rb=5))                         # low += F band
    a.w(alu('max', 10, 10, i=-32767))
    a.w(alu('min', 10, 10, i=32767))
    a.w(alu('sub', 7, 4, rb=10))                          # high = in - low - damping band
    a.w(alu('sub', 7, 7, rb=6))
    a.w(alu('max', 7, 7, i=-32767))
    a.w(alu('min', 7, 7, i=32767))
    a.w(mac(('r', 7, 0), ('r', 1, 0)))                    # high * F
    a.w(mov(3, rb=10))
    a.w(accr(5, 15))
    a.w(alu('add', 11, 11, rb=5))                         # band += F high
    a.w(alu('max', 11, 11, i=-32767))
    a.w(alu('min', 11, 11, i=32767))
    a.w(mov(3, rb=11, pred='c'))                          # band-pass: the band output
    a.w(mac(('r', 3, 0), ('r', 15, 0)))
    a.w(mac(('r', 3, 0), ('r', 15, 1)))
    a.w(busadd(0))
    a.w(busadd(1), go=('loop', 'f_l'))
    a.w(nw(12, 10))
    a.w(nw(14, 11), end=True)


def build():
    a = Asm()
    a.entry('silent')
    a.w(end=True)
    osc(a)
    smp(a)
    smp(a, 16)
    sync(a)
    flt(a)
    return a.assemble(), a.entries, a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hex")
    ap.add_argument("--c")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    words, entries, asm = build()
    if a.hex:
        with open(a.hex, "w") as f:
            for w in words + [0] * (512 - len(words)):
                f.write("%018x\n" % w)
    if a.c:
        with open(a.c, "w") as f:
            f.write("/* generated by fpga/tools/mcasm.py: the microcoded audio engine's control store */\n")
            f.write("#define MC_CS_WORDS %d\n" % len(words))
            for k, v in sorted(entries.items(), key=lambda x: x[1]):
                f.write("#define MC_%s %d\n" % (k.upper(), v))
            f.write("static const uint16_t mc_cs[MC_CS_WORDS][5] = {\n")
            for w in words:
                f.write("    { 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%02x },\n" %
                        (w & 0xFFFF, (w >> 16) & 0xFFFF, (w >> 32) & 0xFFFF, (w >> 48) & 0xFFFF, w >> 64))
            f.write("};\n")
    if a.list:
        lab = {v: k for k, v in asm.labels.items()}
        for i, f in enumerate(asm.words):
            print("%3d %-12s %s" % (i, lab.get(i, ''), f))
    print("mcasm: %d words; entries %s" % (len(words), entries), file=sys.stderr)


if __name__ == "__main__":
    main()
