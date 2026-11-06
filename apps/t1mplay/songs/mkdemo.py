#!/usr/bin/env python3
"""Writes demo.t1s, "Neon Circuit": an original song that uses every T1M feature (docs/t1m.md) -
saw, triangle, pulse with PWM, noise, 8- and 16-bit samples, a hard-sync pair and a ring pair,
unison, every kind of sequence, two filters with LFO sweeps, and most of the effects."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
NCH = 11
KICK, SNARE, HAT, BASS, PAD1, PAD2, PAD3, LEAD, BELL, CHOIR, CHIP = range(NCH)
I_KICK, I_SNARE, I_HAT, I_BASS, I_PAD, I_LEAD, I_BELL, I_CHOIR, I_CHIP, I_FLUTE, I_TOM, I_OHAT = range(12)

HEAD = '''# Neon Circuit: the T1M demo song (written by mkdemo.py; docs/t1m.md)
title  "Neon Circuit"
author "fpga_cpu"
comment "every T1M feature: oscillators, samples, pairs, unison, sequences, filters"
channels %d
tick 50
speed 6
mix 0.42
volume 1.0
restart 1

sample kick  "samples/kick.wav"  note C-4
sample snare "samples/snare.wav" note C-4
sample choir "samples/choir.wav" note A-3 loop 2000 1000

# both filters hear every voice routed to them (the bass): a resonant low-pass that breathes, and a
# band-pass wah on top of it
filter 0 lowpass  cutoff 900  q 3   lfo 0.20 sweep 1500 level 0.55 pan 0.45
filter 1 bandpass cutoff 1400 q 5   lfo 0.55 sweep 900  level 0.3 pan 0.65

instrument 00 "Kick" sample kick
  level 0.65 attack 0 decay 0 sustain 1.0 release 30
instrument 01 "Snare" sample snare
  level 0.55 pan 0.55 release 40
instrument 02 "Hat" noise
  level 0.35 pan 0.62 attack 0 decay 25 sustain 0 release 20
  seq volume 255 170 90 40 0
instrument 03 "Bass" saw
  level 0.42 attack 2 decay 400 sustain 0.6 release 60 filter on unison 2 detune 14 spread 0
instrument 04 "Pad" pulse
  level 0.2 attack 220 decay 900 sustain 0.7 release 500
  width 0.5 pwm 0.35 lfo 0.7 unison 2 detune 16 spread 0.6
instrument 05 "Sync lead" sync offset 1200 partner 0.85
  level 0.28 attack 6 decay 600 sustain 0.65 release 220 pan 0.45 vibrato 18 lfo 5.5
  seq pitch 0 0 0 0 0 0 1 1 1 0 0 -1 -1 -1 0 0 |
instrument 06 "Bell" ring offset 1902 partner 0.9
  level 0.42 attack 1 decay 350 sustain 0.15 release 600 pan 0.7
instrument 07 "Choir" sample choir
  level 0.5 attack 180 decay 2000 sustain 0.8 release 700 pan 0.38 vibrato 10 lfo 4.8
instrument 08 "Chip" pulse
  level 0.3 attack 0 decay 200 sustain 0.5 release 80 pan 0.3
  seq arp 0 0 3 3 7 7 12 12 |
  seq width 32 48 64 96 128 160 128 96 64 48 |
  seq volume 255 240 220 200 190 180 | 170 / 120 80 40 0
instrument 09 "Flute" tri
  level 0.45 attack 60 decay 800 sustain 0.75 release 180 pan 0.6 vibrato 14 lfo 5.0
instrument 0A "Tom" tri
  level 0.85 attack 0 decay 160 sustain 0 release 100 pan 0.4
  seq pitch -8 -8 -6 -6 -4 -4 -3 -2 -2 -1
instrument 0B "Open hat" noise
  level 0.28 pan 0.4 attack 0 decay 160 sustain 0 release 80
''' % NCH

NOTE = "C-C#D-D#E-F-F#G-G#A-A#B-"


def n(name):
    """'A3' / 'C#4' -> MIDI"""
    base = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}[name[0]]
    acc = 1 if name[1] == "#" else 0
    return base + acc + 12 * (int(name[1 + acc:]) + 1)


def nn(m):
    return NOTE[2 * (m % 12):2 * (m % 12) + 2] + str(m // 12 - 1)


class Pat:
    def __init__(self, rows=64):
        self.rows, self.c = rows, {}

    def put(self, row, ch, note=None, ins=None, vol=None, fx1=None, fx2=None):
        old = self.c.get((row, ch), {})
        new = dict(old)
        for k, v in (("note", note), ("ins", ins), ("vol", vol), ("fx1", fx1), ("fx2", fx2)):
            if v is not None:
                new[k] = v
        self.c[(row, ch)] = new

    def text(self, num):
        out = ["", "pattern %02X %d" % (num, self.rows)]
        for r in range(self.rows):
            cells = []
            last = max((ch for (rr, ch) in self.c if rr == r), default=-1)
            if last < 0:
                continue
            for ch in range(last + 1):
                e = self.c.get((r, ch), {})
                note = e.get("note")
                ns = "---" if note is None else "===" if note == "off" else "^^^" if note == "cut" else nn(note)
                ins = "%02X" % e["ins"] if "ins" in e else ".."
                vol = "%02X" % e["vol"] if "vol" in e else ".."
                f1 = "%02X%02X" % e["fx1"] if "fx1" in e else "...."
                f2 = "%02X%02X" % e["fx2"] if "fx2" in e else "...."
                cells.append(" %s %s %s %s %s " % (ns, ins, vol, f1, f2))
            out.append("%02X |%s" % (r, "|".join(cells)))
        return "\n".join(out)


CHORDS = [("A2", ["A3", "C4", "E4"]), ("F2", ["F3", "A3", "C4"]), ("C3", ["G3", "C4", "E4"]), ("G2", ["G3", "B3", "D4"])]


def drums(p, full=True, fill=False):
    for r in range(0, 64, 4):
        p.put(r, KICK, n("C4"), I_KICK)
    for r in range(4, 64, 8):
        p.put(r, SNARE, n("C4"), I_SNARE, 0xD0)
    for r in range(0, 64, 1 if full else 2):
        if r % 8 == 6 and full:
            p.put(r, HAT, n("C6"), I_OHAT, 0xC0)
        else:
            p.put(r, HAT, n("C7") if r % 2 else n("A6"), I_HAT, 0xFF if r % 4 == 0 else 0x90)
    if fill:                                           # the last bar: a snare roll (retrigger) and toms
        p.put(56, SNARE, n("C4"), I_SNARE, 0x80, (0x14, 3))
        p.put(58, SNARE, n("C4"), I_SNARE, 0xA0, (0x14, 2))
        for k, (r, t) in enumerate(((60, "A3"), (61, "F3"), (62, "D3"), (63, "A2"))):
            p.put(r, CHIP, n(t), I_TOM)


def bass(p, sweep=None):
    for b, (root, _) in enumerate(CHORDS):
        for k in range(8):
            r = 16 * b + 2 * k
            m = n(root) + (12 if k in (3, 6) else 0)
            p.put(r, BASS, m, I_BASS if k == 0 else None, 0xFF if k % 2 == 0 else 0xA8)
    if sweep:                                          # the filter's cutoff, walked up through the pattern
        for r in range(0, 64, 8):
            p.put(r, PAD3 if sweep == "pad3" else BASS, fx1=(0x24, 0x06))


def pads(p, vol=0xC0):
    for b, (_, ch) in enumerate(CHORDS):
        for k, c in enumerate((PAD1, PAD2, PAD3)):
            p.put(16 * b, c, n(ch[k]), I_PAD, vol, (0x08, (0x30, 0x80, 0xD0)[k]))
            p.put(16 * b + 15, c, "off")


MELODY = [  # (row, note or None (key off), len) - the sync lead's tune over the four chords
    (0, "E5", 6), (6, "D5", 2), (8, "C5", 4), (12, "A4", 4),
    (16, "C5", 6), (22, "D5", 2), (24, "E5", 4), (28, "F5", 4),
    (32, "G5", 6), (38, "E5", 2), (40, "C5", 4), (44, "D5", 4),
    (48, "B4", 8), (56, "G4", 4), (60, "B4", 4)]


def lead(p, variant=0):
    for i, (r, t, ln) in enumerate(MELODY):
        m = n(t) + (12 if variant and i in (8, 9) else 0)
        fx1 = (0x03, 0x20) if variant and i in (4, 12) else None        # portamento into some notes
        fx2 = (0x18, 0x10 if i % 2 else 0xF4) if variant else None      # the sync partner sweeps
        p.put(r, LEAD, m, I_LEAD if i == 0 else None, 0xFF, fx1, fx2)
        if ln >= 6:
            p.put(r + ln - 1, LEAD, "off")


def choir(p):
    for b, (_, ch) in enumerate(CHORDS):
        p.put(16 * b, CHOIR, n(ch[2]) + 12, I_CHOIR, 0xB0)
    p.put(63, CHOIR, "off")


def bells(p, panned=False):
    seq = [0, 7, 12, 7, 3, 7, 12, 15]
    for b, (root, ch) in enumerate(CHORDS):
        for k in range(8):
            r = 16 * b + 2 * k
            m = n(ch[0]) + 12 + seq[k] % 12 if k else n(ch[0]) + 12
            fx = (0x08, (0x40, 0xC0)[k % 2]) if panned else None
            p.put(r, BELL, m, I_BELL if r == 0 else None, 0xE0 - 8 * k, fx)


def chip(p):
    for b, (_, ch) in enumerate(CHORDS):
        for r in (0, 6, 8, 12):
            p.put(16 * b + r, CHIP, n(ch[0]) + 12, I_CHIP)
        p.put(16 * b + 14, CHIP, "off")


def flute(p):
    tune = [(0, "A5", 8), (8, "G5", 4), (12, "E5", 4), (16, "F5", 12), (28, "E5", 4),
            (32, "E5", 8), (40, "G5", 8), (48, "D5", 12), (60, "B4", 4)]
    for i, (r, t, ln) in enumerate(tune):
        p.put(r, CHIP, n(t), I_FLUTE if i == 0 else None, 0xD0, (0x04, 0xA3) if ln >= 8 else None)


def main():
    pats = []
    # 00 intro: drums and the filtered bass, the cutoff walking up; global volume swells in
    p = Pat()
    drums(p, full=False)
    bass(p, sweep="bass")
    p.put(0, KICK, fx1=(0x11, 0x60), fx2=(0x20, 0x40))
    for r in range(8, 64, 8):
        p.put(r, KICK, fx1=(0x11, 0x60 + r * 2))
    p.put(32, SNARE, fx2=(0x28, 0x50))                # more resonance halfway
    pats.append(p)
    # 01 A: everything but the bells
    p = Pat()
    drums(p)
    bass(p)
    pads(p)
    lead(p)
    choir(p)
    p.put(0, KICK, fx1=(0x11, 0xFF), fx2=(0x20, 0x90))
    pats.append(p)
    # 02 B: the ring bells and the chip arpeggios, the lead's partner sweeping
    p = Pat()
    drums(p)
    bass(p)
    bells(p)
    chip(p)
    lead(p, variant=1)
    p.put(0, PAD1, n("A4"), I_PAD, 0x70, (0x06, 0x48))   # one pad, slow PWM
    p.put(32, PAD1, n("C5"), fx1=(0x06, 0x2C))
    p.put(63, PAD1, "off")
    pats.append(p)
    # 03 break: half the drums, toms, the flute, the wah opened up, a delay and a cut
    p = Pat()
    for r in range(0, 64, 8):
        p.put(r, KICK, n("C4"), I_KICK)
    for r in (12, 28, 44):
        p.put(r, SNARE, n("C4"), I_SNARE, 0xB0, (0x13, 3))   # late snares
    for r in range(0, 64, 2):
        p.put(r, HAT, n("A6"), I_HAT, 0x70 + (r % 8) * 8)
    flute(p)
    for b, (root, _) in enumerate(CHORDS):
        p.put(16 * b, BASS, n(root), I_BASS, 0xE0)
        p.put(16 * b + 10, BASS, "cut")
        p.put(16 * b + 12, BASS, n(root) + 12, None, 0xC0, (0x01, 0x03))   # a slide up
    p.put(0, BASS, fx2=(0x29, 0xA0))                   # filter 1: sharper
    p.put(0, PAD2, fx1=(0x31, 0x30))                   # filter 1's sweep: faster
    p.put(32, PAD2, fx1=(0x2D, 0x90))                  # and deeper
    p.put(48, CHOIR, n("E5"), I_CHOIR, 0x80, (0x0A, 0x40))   # swell in
    p.put(56, SNARE, n("C4"), I_SNARE, 0x90, (0x14, 2))
    p.put(62, CHOIR, "off")
    pats.append(p)
    # 04 A': A with the bells panning and a faster partner
    p = Pat()
    drums(p, fill=True)
    bass(p)
    pads(p, 0xA0)
    lead(p, variant=1)
    bells(p, panned=True)
    choir(p)
    p.put(0, PAD2, fx1=(0x31, 0x08), fx2=(0x2D, 0x70))   # the wah back down
    p.put(0, BASS, fx2=(0x29, 0x60))
    pats.append(p)
    order = "order 00 01 02 03 04 02"
    with open(os.path.join(HERE, "demo.t1s"), "w") as f:
        f.write(HEAD)
        for i, p in enumerate(pats):
            f.write(p.text(i) + "\n")
        f.write("\n" + order + "\n")
    print("demo.t1s")


if __name__ == "__main__":
    main()
