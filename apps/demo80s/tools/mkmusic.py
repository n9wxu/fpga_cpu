#!/usr/bin/env python3
"""mkmusic.py: the demo's two original modules, composed here and written as ProTracker files, then as C arrays.

    amiga.mod   4 channels (M.K.), A minor, 125 BPM: the Amiga part
    finale.mod  8 channels (8CHN), "T1 Rush": E harmonic minor, piano runs accelerating 150 -> 255 BPM

The samples are synthesized (8-bit signed): drums at C-3 (16574 Hz), looped single-cycle waves (bass 64
samples, lead 32: C-2 plays C3 and C4) and Amiga-style chord samples (a triad mixed into one loop, root C3
at C-2). Notes are written C-1..B-3 as "C-2", "A#1"; a note sequence is a list of (row, note, sample, fx).

    python3 tools/mkmusic.py out_dir      -> out_dir/amiga.mod, finale.mod, src/music_mods.h
"""
import math
import os
import random
import struct
import sys

random.seed(1987)
PERIODS = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
           428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
           214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]
NAMES = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"]
RATE_C3 = 16574          # PAL rate of C-3 (period 214)


def note(n):
    """'A-2' -> period"""
    if n is None:
        return 0
    k = NAMES.index(n[:2]) + 12 * (int(n[2]) - 1)
    return PERIODS[k]


def transpose(n, semis):
    k = NAMES.index(n[:2]) + 12 * (int(n[2]) - 1) + semis
    return NAMES[k % 12] + str(k // 12 + 1)


def s8(x):
    return max(-128, min(127, int(round(x))))


# ---------------------------------------------------------------- samples
def kick():
    out, ph = [], 0.0
    n = int(RATE_C3 * 0.30)
    for i in range(n):
        t = i / RATE_C3
        f = 45 + 140 * math.exp(-t * 28)
        ph += 2 * math.pi * f / RATE_C3
        a = math.exp(-t * 9)
        click = 40 * math.exp(-t * 400)
        out.append(s8(118 * a * math.sin(ph) + click))
    return out


def snare():
    out, ph, lp = [], 0.0, 0.0
    n = int(RATE_C3 * 0.22)
    for i in range(n):
        t = i / RATE_C3
        ph += 2 * math.pi * 185 / RATE_C3
        nz = random.uniform(-1, 1)
        lp += 0.6 * (nz - lp)
        out.append(s8(95 * math.exp(-t * 16) * lp + 45 * math.exp(-t * 30) * math.sin(ph)))
    return out


def hat(open_=False):
    out, prev = [], 0.0
    n = int(RATE_C3 * (0.16 if open_ else 0.045))
    for i in range(n):
        t = i / RATE_C3
        nz = random.uniform(-1, 1)
        hp = nz - prev
        prev = nz
        out.append(s8(70 * math.exp(-t * (14 if open_ else 70)) * hp))
    return out


def saw_loop(n, harmonics=24, amp=100, soft=1.0):
    """band-limited saw over n samples"""
    return [s8(amp * sum((-1) ** (k + 1) * math.sin(2 * math.pi * k * i / n) / k * soft ** k
                         for k in range(1, harmonics + 1)) * 0.55) for i in range(n)]


def pulse_loop(n, duty=0.5, harmonics=14, amp=90):
    out = []
    for i in range(n):
        v = 0.0
        for k in range(1, harmonics + 1):
            v += math.sin(math.pi * k * duty) / k * math.cos(2 * math.pi * k * (i / n - duty / 2))
        out.append(s8(amp * v * 0.9))
    return out


def tri_loop(n, amp=110):
    return [s8(amp * (1 - 4 * abs(i / n - 0.5))) for i in range(n)]


def chord_loop(ratios, base_div, n, harmonics=8, amp=110):
    """a chord: saws at ratios/base_div cycles per n samples (the loop holds whole cycles of every note)"""
    out = []
    for i in range(n):
        v = 0.0
        for r in ratios:
            cyc = r / base_div
            for k in range(1, harmonics + 1):
                v += (-1) ** (k + 1) * math.sin(2 * math.pi * k * cyc * i / n) / k * (0.75 ** k)
        out.append(v)
    m = max(abs(x) for x in out)
    return [s8(amp * x / m) for x in out]


# ---------------------------------------------------------------- the MOD writer
class Song:
    def __init__(self, title, nch, speed=6, bpm=125):
        self.title, self.nch, self.samples, self.patterns, self.order = title, nch, [], [], []
        self.speed, self.bpm = speed, bpm

    def sample(self, name, data, vol=64, loop=None):
        """loop: (start, length) in samples, or None"""
        if len(data) & 1:
            data = data + [0]
        self.samples.append((name, data, vol, loop))
        return len(self.samples)          # 1-based

    def pattern(self):
        p = [[(0, 0, 0, 0) for _ in range(self.nch)] for _ in range(64)]
        self.patterns.append(p)
        return p

    def write(self, path):
        b = bytearray(self.title.encode()[:20].ljust(20, b"\0"))
        for i in range(31):
            if i < len(self.samples):
                name, data, vol, loop = self.samples[i]
                rs, rl = (loop[0] // 2, loop[1] // 2) if loop else (0, 1)
                b += name.encode()[:22].ljust(22, b"\0") + struct.pack(">HBBHH", len(data) // 2, 0, vol, rs, rl)
            else:
                b += bytes(22) + struct.pack(">HBBHH", 0, 0, 0, 0, 1)
        b += bytes([len(self.order), 127]) + bytes(self.order).ljust(128, b"\0")
        b += b"M.K." if self.nch == 4 else b"%dCHN" % self.nch
        for p in self.patterns:
            for row in p:
                for (per, smp, fx, arg) in row:
                    b += bytes([(smp & 0xF0) | (per >> 8), per & 0xFF, ((smp & 15) << 4) | fx, arg])
        for (_, data, _, _) in self.samples:
            b += bytes((x & 0xFF) for x in data)
        open(path, "wb").write(b)
        return b


def put(p, row, ch, n, smp=0, fx=0, arg=0):
    if 0 <= row < 64:
        p[row][ch] = (note(n), smp, fx, arg)


def fx_only(p, row, ch, fx, arg):
    per, smp, _, _ = p[row][ch]
    p[row][ch] = (per, smp, fx, arg)


# ---------------------------------------------------------------- the Amiga song
def amiga():
    s = Song("t1 flex: amiga part", 4)
    K = s.sample("kick", kick())
    S = s.sample("snare", snare())
    H = s.sample("hat", hat())
    O = s.sample("open hat", hat(True), vol=48)
    B = s.sample("bass", saw_loop(64, 20, 105, 0.92), vol=60, loop=(0, 64))
    L = s.sample("lead", pulse_loop(32, 0.3), vol=50, loop=(0, 32))
    MAJ = s.sample("chord maj", chord_loop([4, 5, 6], 1, 256), vol=46, loop=(0, 256))
    MIN = s.sample("chord min", chord_loop([10, 12, 15], 1, 640), vol=46, loop=(0, 640))
    # Am F C G, a bar (16 rows) each; chord roots for the C-2 = C3 chord samples
    prog = [("A-1", MIN, "A-1"), ("F-1", MAJ, "F-1"), ("C-2", MAJ, "C-1"), ("G-1", MAJ, "G-1")]

    def drums(p, fill=False, half=False):
        for bar in range(4):
            o = bar * 16
            put(p, o + 0, 0, "C-3", K); put(p, o + 8, 0, "C-3", K)
            if not half:
                put(p, o + 4, 0, "C-3", S); put(p, o + 12, 0, "C-3", S)
                put(p, o + 10, 0, "C-3", K)
            for r in (2, 6, 14):
                put(p, o + r, 0, "C-3", H)
            if bar == 3 and fill:
                for r in (13, 14, 15):
                    put(p, o + r, 0, "C-3", S, 0xC, 0x20 + 0x10 * (r - 13))
            elif not half:
                put(p, o + 15, 0, "C-3", O)

    def bass(p, pat="oct"):
        for bar, (root, _, broot) in enumerate(prog):
            o = bar * 16
            for i in range(8):
                n = broot if (i % 2 == 0 or pat != "oct") else transpose(broot, 12)
                put(p, o + 2 * i, 1, n, B, 0xC, 0x30 if i % 2 else 0x38)

    def chords(p, stabs=True):
        for bar, (root, smp, _) in enumerate(prog):
            o = bar * 16
            if stabs:
                for r, v in ((0, 0x30), (3, 0x20), (6, 0x28), (10, 0x20), (12, 0x24)):
                    put(p, o + r, 2, root, smp, 0xC, v)
                    fx_only(p, o + r + 1, 2, 0xA, 0x04) if o + r + 1 < 64 and p[o + r + 1][2][0] == 0 else None
            else:
                put(p, o, 2, root, smp, 0xC, 0x28)
                for r in range(1, 16, 2):
                    fx_only(p, o + r, 2, 0xA, 0x01)

    # the lead: a phrase over Am F C G (C-2 plays C4)
    melody_a = [(0, "E-2"), (2, "A-2"), (4, "C-3"), (6, "B-2"), (8, "A-2"), (11, "E-2"), (14, "G-2"),
                (16, "F-2"), (18, "A-2"), (20, "C-3"), (22, "D-3"), (24, "C-3"), (27, "A-2"), (30, "F-2"),
                (32, "G-2"), (34, "C-3"), (36, "E-3"), (38, "D-3"), (40, "C-3"), (43, "G-2"), (46, "E-2"),
                (48, "D-2"), (50, "G-2"), (52, "B-2"), (54, "D-3"), (56, "B-2"), (58, "G-2"), (60, "A-2")]
    melody_b = [(0, "A-2"), (1, "C-3"), (2, "E-3"), (4, "A-3"), (8, "G-3"), (10, "E-3"), (12, "C-3"),
                (16, "F-3"), (18, "E-3"), (20, "C-3"), (22, "A-2"), (24, "C-3"), (28, "F-3"),
                (32, "E-3"), (34, "D-3"), (36, "C-3"), (38, "D-3"), (40, "E-3"), (44, "G-3"),
                (48, "G-3"), (50, "F-3"), (52, "D-3"), (54, "B-2"), (56, "D-3"), (60, "G-2")]

    def lead(p, mel, arp=False):
        for (r, n) in mel:
            put(p, r, 3, n, L, 0xC, 0x30)
            if r + 2 < 64 and p[r + 2][3][0] == 0:
                fx_only(p, r + 2, 3, 0x4, 0x43)          # vibrato on held notes
        if arp:
            for r in range(0, 64, 4):
                if p[r][3][0] == 0:
                    p[r][3] = (0, 0, 0x0, 0x37)

    # patterns: 0 intro, 1 groove, 2 groove+chords, 3 melody A, 4 melody B, 5 break
    p0 = s.pattern(); drums(p0, half=True); bass(p0)
    p0[0][3] = (0, 0, 0xF, 125)
    p1 = s.pattern(); drums(p1, fill=True); bass(p1)
    p2 = s.pattern(); drums(p2); bass(p2); chords(p2)
    p3 = s.pattern(); drums(p3); bass(p3); chords(p3, stabs=False); lead(p3, melody_a)
    p4 = s.pattern(); drums(p4, fill=True); bass(p4); chords(p4); lead(p4, melody_b)
    p5 = s.pattern(); chords(p5, stabs=False); lead(p5, melody_a[:14])
    for bar in range(4):
        put(p5, bar * 16, 0, "C-3", K)
    s.order = [0, 1, 2, 3, 3, 4, 2, 4, 5, 1, 3, 4]
    return s


# ---------------------------------------------------------------- the finale: "T1 Rush" (8 channels)
def piano(f0, secs=0.9):
    """a struck string: harmonics decaying, the higher ones faster, a little inharmonic"""
    out, n = [], int(RATE_C3 * secs)
    for i in range(n):
        t = i / RATE_C3
        v = 0.0
        for k, amp in ((1, 1.0), (2, 0.55), (3, 0.3), (4, 0.18), (5, 0.1), (6, 0.06)):
            fk = f0 * k * (1 + 0.0004 * k * k)
            v += amp * math.exp(-t * (2.5 + 2.2 * k)) * math.sin(2 * math.pi * fk * t)
        v *= min(1.0, i / 30.0)                     # the hammer, softened
        out.append(s8(70 * v))
    return out


def bell(f0, secs=0.7):
    out, n = [], int(RATE_C3 * secs)
    for i in range(n):
        t = i / RATE_C3
        mod = 2.2 * math.exp(-t * 4) * math.sin(2 * math.pi * f0 * 3.5 * t)
        out.append(s8(80 * math.exp(-t * 4.5) * math.sin(2 * math.pi * f0 * t + mod)))
    return out


def crash(secs=1.1):
    out, prev, n = [], 0.0, int(RATE_C3 * secs)
    for i in range(n):
        t = i / RATE_C3
        nz = random.uniform(-1, 1)
        out.append(s8(75 * math.exp(-t * 3.2) * (nz - 0.5 * prev)))
        prev = nz
    return out


SCALE = ["E", "F#", "G", "A", "B", "C", "D#"]        # E harmonic minor


def nn(name, octave):
    """'F#', 2 -> 'F#2' in our C-1..B-3 naming"""
    return (name if len(name) == 2 else name + "-") + str(octave)


SCALE_PC = sorted(NAMES.index((n if len(n) == 2 else n + "-")) for n in SCALE)   # pitch classes of the scale


def run_from(note_name, steps, up=True):
    """a scale run from the scale note at or below note_name ('D-2'), steps long, in our naming"""
    k = NAMES.index(note_name[:2]) + 12 * (int(note_name[2]) - 1)
    while (k % 12) not in SCALE_PC:
        k -= 1
    out = []
    for _ in range(steps):
        out.append(NAMES[k % 12] + str(k // 12 + 1))
        k += 1 if up else -1
        while (k % 12) not in SCALE_PC:
            k += 1 if up else -1
    return out


def rush():
    s = Song("t1 rush", 8, bpm=150)
    K = s.sample("kick", kick())
    S = s.sample("snare", snare())
    H = s.sample("hat", hat())
    CR = s.sample("crash", crash(0.45), vol=48)
    B = s.sample("bass", saw_loop(64, 22, 108, 0.93), vol=60, loop=(0, 64))
    PL = s.sample("piano low", piano(261.63, 0.6), vol=60)      # C-3 plays C4
    PH = s.sample("piano high", piano(523.25, 0.42), vol=56)     # C-3 plays C5
    BE = s.sample("bell", bell(523.25, 0.4), vol=40)
    # chords: Em C D B (root for the low piano at C-3 = C4: octave 1-2 here is C2..B3)
    chords = [("E", ["E", "G", "B"]), ("C", ["C", "E", "G"]), ("D", ["D", "F#", "A"]), ("B", ["B", "D#", "F#"])]

    def drums(p, dense, crash_first=False):
        for bar in range(4):
            o = bar * 16
            put(p, o, 0, "C-3", K); put(p, o + 8, 0, "C-3", K)
            if dense: put(p, o + 4, 0, "C-3", K); put(p, o + 12, 0, "C-3", K)
            put(p, o + 4, 1, "C-3", S); put(p, o + 12, 1, "C-3", S)
            for r in range(0, 16, 2 if not dense else 1):
                if p[o + r][2][0] == 0: put(p, o + r, 2, "C-3", H, 0xC, 0x20 if r % 2 else 0x30)
            if dense and bar == 3:
                for r in (14, 15): put(p, o + r, 1, "C-3", S, 0xC, 0x30)
        if crash_first: put(p, 0, 7, "C-3", CR)

    def bass(p):
        for bar, (root, _) in enumerate(chords):
            for r in range(0, 16, 2):
                put(p, bar * 16 + r, 3, nn(root, 1 if root in ("E", "D") else 1), B, 0xC, 0x38 if r % 4 == 0 else 0x28)

    def left_hand(p, every=4):
        for bar, (root, tones) in enumerate(chords):
            for r in range(0, 16, every):
                put(p, bar * 16 + r, 4, nn(tones[(r // every) % 3], 2), PL, 0xC, 0x30)

    def right_hand(p, ch, sixteenths, octave_shift=0, start=0):
        """runs over each chord, from its root, up then down"""
        for bar, (root, _) in enumerate(chords):
            top = run_from(nn(root, 2 + octave_shift), 9, up=True)
            notes = top[:8] + run_from(top[8], 8, up=False)
            step = 1 if sixteenths else 2
            for i, r in enumerate(range(0, 16, step)):
                n = notes[i % len(notes)]
                if int(n[2]) <= 3:
                    put(p, bar * 16 + r + start, ch, n, PH, 0xC, 0x30 if i % 4 == 0 else 0x24)

    def dnb_drums(p, crash_first=False):
        # a broken beat: kick on 1 and the 'and' of 2 (row 10), snare on 2 and 4, ghost snares, 16th hats
        for bar in range(4):
            o = bar * 16
            for r in (0, 10):
                put(p, o + r, 0, "C-3", K)
            if bar % 2: put(p, o + 7, 0, "C-3", K, 0xC, 0x28)
            put(p, o + 4, 1, "C-3", S); put(p, o + 12, 1, "C-3", S)
            for r in (7, 9, 15):
                if p[o + r][1][0] == 0: put(p, o + r, 1, "C-3", S, 0xC, 0x12)     # ghosts
            for r in range(16):
                put(p, o + r, 2, "C-3", H, 0xC, 0x28 if r % 4 == 2 else 0x16)
        if crash_first: put(p, 0, 7, "C-3", CR)

    def dnb_bass(p):
        # a syncopated sub line on the roots: 1, the 'and' of 2, 3-and, a pickup
        for bar, (root, tones) in enumerate(chords):
            o = bar * 16
            for r, n, v in ((0, nn(root, 1), 0x40), (6, nn(root, 1), 0x30), (10, nn(root, 1), 0x38),
                            (11, nn(tones[2], 1) if tones[2] not in ("C",) else nn(tones[2], 2), 0x28),
                            (14, nn(root, 2) if root in ("C", "D") else nn(root, 1), 0x30)):
                put(p, o + r, 3, n, B, 0xC, v)

    def bells(p):
        for bar, (root, tones) in enumerate(chords):
            put(p, bar * 16, 7, nn(tones[0], 2), BE, 0xC, 0x30)
            put(p, bar * 16 + 6, 7, nn(tones[2], 2), BE, 0xC, 0x20)
            put(p, bar * 16 + 10, 7, nn(tones[1], 3), BE, 0xC, 0x20)

    def tempo(p, speed, bpm):
        p[0][6] = (0, 0, 0xF, speed)
        p[1][6] = (0, 0, 0xF, bpm)

    p0 = s.pattern(); left_hand(p0, 8); right_hand(p0, 5, False); tempo(p0, 6, 150)            # intro: piano, a bass pedal
    for bar, (root, _) in enumerate(chords):
        put(p0, bar * 16, 3, nn(root, 1), B, 0xC, 0x20)
        put(p0, bar * 16 + 8, 0, "C-3", K, 0xC, 0x20)
    p1 = s.pattern(); drums(p1, False, True); bass(p1); left_hand(p1); right_hand(p1, 5, False); bells(p1)
    p2 = s.pattern(); drums(p2, False); bass(p2); left_hand(p2); right_hand(p2, 5, True); bells(p2)
    p3 = s.pattern(); dnb_drums(p3, True); dnb_bass(p3); left_hand(p3); right_hand(p3, 5, True); right_hand(p3, 6, True, -1); tempo(p3, 5, 180)
    p4 = s.pattern(); dnb_drums(p4, True); dnb_bass(p4); left_hand(p4, 2); right_hand(p4, 5, True); right_hand(p4, 6, True, -1); bells(p4); tempo(p4, 4, 220)
    p5 = s.pattern(); dnb_drums(p5, True); dnb_bass(p5); left_hand(p5, 1); right_hand(p5, 5, True); right_hand(p5, 6, True, -1); bells(p5); tempo(p5, 3, 255)
    p6 = s.pattern()                                                                          # the end: chords, then silence
    for r, (root, tones) in zip((0, 8, 16, 24), chords):
        for i, t in enumerate(tones):
            put(p6, r, 4 + i, nn(t, 2), PL, 0xC, 0x40)
        put(p6, r, 0, "C-3", K); put(p6, r, 7, "C-3", CR)
        put(p6, r, 3, nn(root, 1), B, 0xC, 0x40)
    put(p6, 32, 4, "E-2", PL, 0xC, 0x40); put(p6, 32, 5, "G-2", PL, 0xC, 0x40); put(p6, 32, 6, "B-2", PL, 0xC, 0x40)
    put(p6, 32, 3, "E-1", B, 0xC, 0x40); put(p6, 32, 7, "C-3", CR)
    for r in range(34, 64, 2): fx_only(p6, r, 3, 0xA, 0x02)
    tempo(p6, 6, 125)
    s.order = [0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 6]
    return s


def c_array(name, data):
    lines = ["static const uint8_t %s[%d] __attribute__((aligned(4))) = {" % (name, len(data))]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join("%d" % x for x in data[i:i + 24]) + ",")
    lines.append("};")
    return "\n".join(lines)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "build"
    os.makedirs(out, exist_ok=True)
    a = amiga().write(os.path.join(out, "amiga.mod"))
    f = rush().write(os.path.join(out, "finale.mod"))
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "music_mods.h")
    with open(here, "w") as h:
        h.write("/* generated by tools/mkmusic.py: the demo's two original modules */\n#include <stdint.h>\n")
        h.write(c_array("mod_amiga", a) + "\n" + c_array("mod_finale", f) + "\n")
    print("amiga.mod %d bytes, finale.mod %d bytes" % (len(a), len(f)))
