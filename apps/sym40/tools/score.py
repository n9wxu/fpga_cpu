#!/usr/bin/env python3
"""The stress test's score: Mozart, Symphony No. 40 in G minor, K. 550, I. Molto allegro, an
arrangement for the microcoded engine (public domain music; the arrangement is ours and condensed:
the first theme, its restatement with winds, a tutti, the second theme in B flat, and round again).
Writes a C header: one event list per part, {start tick, MIDI note, length, velocity}, ticks are
eighth notes (8 a bar, alla breve), and the section of each bar.
usage: score.py OUT.h"""
import sys

NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(n):
    """'Eb5', 'F#4', 'C5' -> MIDI number"""
    k = NOTE[n[0]]
    i = 1
    while n[i] in '#b':
        k += 1 if n[i] == '#' else -1
        i += 1
    return k + 12 * (int(n[i:]) + 1)


def line(s):
    """'Eb5/1 D5/2 r/2' (lengths in eighths) -> [(midi or 0, length)]"""
    out = []
    for tok in s.split():
        n, d = tok.split('/')
        out.append((0 if n == 'r' else midi(n), int(d)))
    return out


# chords: name -> the root (MIDI, octave 3) and the chord's tones (semitones above it)
CH = {
    'Gm': (55, (0, 3, 7)), 'Cm': (48, (0, 3, 7)), 'D': (50, (0, 4, 7)), 'D7': (50, (0, 4, 7, 10)),
    'Bb': (58, (0, 4, 7)), 'Eb': (51, (0, 4, 7)), 'F7': (53, (0, 4, 7, 10)), 'C7': (48, (0, 4, 7, 10)),
    'Gm7': (55, (0, 3, 7, 10)),
}

# ---- the sections: bars, harmony a bar, the first violins' line, dynamics (velocity 0-127)
THEME = line(
    "r/6 Eb5/1 D5/1 "
    "D5/2 Eb5/1 D5/1 D5/2 Eb5/1 D5/1 "
    "D5/2 Bb5/2 r/2 Bb5/1 A5/1 "
    "G5/2 G5/1 F5/1 Eb5/2 Eb5/1 D5/1 "
    "C5/2 C5/2 r/2 D5/1 C5/1 "
    "C5/2 D5/1 C5/1 C5/2 D5/1 C5/1 "
    "C5/2 A5/2 r/2 A5/1 G5/1 "
    "F#5/2 F#5/1 E5/1 D5/2 D5/1 C5/1 "
    "Bb4/2 Bb4/2 r/2 A4/1 G4/1 "
    "A4/4 A4/2 Bb4/2 "
    "C5/2 D5/2 Eb5/2 C5/2 "
    "D5/6 Eb5/1 D5/1")
THEME_H = ['Gm', 'Gm', 'Gm', 'Gm', 'D7', 'D7', 'D7', 'D', 'Gm', 'Gm', 'Cm', 'D']
TUTTI_H = ['Gm', 'Gm', 'Cm', 'Cm', 'D7', 'D7', 'Gm', 'F7']
SECOND = line("F5/4 E5/4 Eb5/4 D5/4 Db5/4 C5/4 B4/4 C5/4 D5/2 C5/2 Bb4/2 A4/2 G4/4 A4/4 Bb4/8 A4/4 F#4/2 G4/1 A4/1")
SECOND_H = ['Bb', 'Bb', 'Gm7', 'C7', 'F7', 'F7', 'Bb', 'D7']

SECTIONS = [  # (name, harmony, first violins, dynamic, winds)
    ("Exposition: the first theme (strings, piano)", THEME_H, THEME, 70, False),
    ("The first theme again, winds doubling", THEME_H, THEME, 90, True),
    ("Tutti, forte", TUTTI_H, None, 120, True),
    ("The second theme, in B flat", SECOND_H, SECOND, 85, True),
]

PARTS = ['vn1', 'vn2', 'va1', 'va2', 'vc', 'cb', 'fl', 'ob1', 'ob2', 'cl1', 'cl2', 'bn1', 'bn2', 'hn1', 'hn2']


def tones(name, octave_from, n):
    root, iv = CH[name]
    out, o = [], 0
    while len(out) < n:
        for k in iv:
            m = root + k + 12 * o
            if m >= octave_from:
                out.append(m)
                if len(out) == n:
                    break
        o += 1
    return out


def main():
    ev = {p: [] for p in PARTS}
    bar_section = []
    t0 = 0

    def put(p, t, m, d, v):
        if m:
            ev[p].append((t, m, d, v))

    for si, (name, harm, mel, vel, winds) in enumerate(SECTIONS):
        nb = len(harm)
        for b, ch in enumerate(harm):
            bar_section.append(si)
            t = t0 + 8 * b
            tn = tones(ch, 55, 4)               # chord tones from G3 up
            root = CH[ch][0]
            # violas: divided eighths on two chord tones (the famous accompaniment), held in the 2nd theme
            if mel is SECOND:
                put('va1', t, tn[1], 8, vel - 20); put('va2', t, tn[0], 8, vel - 20)
            else:
                for k in range(8):
                    put('va1', t + k, tn[1 + (k & 1)], 1, vel - 10)
                    put('va2', t + k, tn[0] + (12 if k & 1 else 0) if tn[0] < 58 else tn[0], 1, vel - 15)
            # cellos and basses: the root in quarters (halves in the 2nd theme)
            step = 4 if mel is SECOND else 2
            for k in range(0, 8, step):
                put('vc', t + k, root - 12, step, vel)
                put('cb', t + k, root - 24, step, vel)
            if winds:
                # horns: root and fifth, held; bassoons: the bass line in halves
                put('hn1', t, root + (7 if root < 52 else -5), 8, vel - 15)
                put('hn2', t, root, 8, vel - 15)
                put('bn1', t, root - 12, 4, vel - 10); put('bn1', t + 4, root - 12, 4, vel - 10)
                put('bn2', t, tn[1] - 12, 8, vel - 20)
            if mel is None:
                # the tutti: first violins in arpeggios over two octaves, the winds on the chord
                arp = tones(ch, 67, 6)
                seq = [arp[0], arp[1], arp[2], arp[3], arp[4], arp[5], arp[4], arp[2]]
                for k in range(8):
                    put('vn1', t + k, seq[k], 1, vel)
                    put('vn2', t + k, seq[(k + 4) % 8] - 12, 1, vel - 10)
                top = tones(ch, 72, 3)
                put('fl', t, top[2], 8, vel - 10); put('ob1', t, top[1], 8, vel - 15); put('ob2', t, top[0], 8, vel - 15)
                put('cl1', t, tn[2], 8, vel - 20); put('cl2', t, tn[1], 8, vel - 20)
        if mel is not None:
            t = t0
            for m, d in mel:
                put('vn1', t, m, d, vel)
                put('vn2', t, m - 12, d, vel - 10)
                if winds and mel is THEME:
                    put('fl', t, m + 12, d, vel - 20)
                    put('ob1', t, m, d, vel - 25)
                if mel is SECOND:
                    # the winds answer: the clarinet a bar behind, the flute an octave up two bars on
                    put('cl1', t + 8, m - 12, d, vel)
                    put('fl', t + 16, m + 12, d, vel - 15)
                t += d
        t0 += 8 * nb
    total = t0

    with open(sys.argv[1], 'w') as f:
        f.write("/* generated by apps/sym40/tools/score.py: Mozart, Symphony No. 40, I (arranged) */\n")
        f.write("#define SC_TICKS %d\n#define SC_BARS %d\n" % (total, total // 8))
        f.write("typedef struct { uint16_t t; uint8_t midi, len, vel; } sc_ev_t;\n")
        for p in PARTS:
            e = sorted(x for x in ev[p] if x[0] < total)
            f.write("static const sc_ev_t sc_%s[] = {\n" % p)
            for t, m, d, v in e:
                f.write("    { %d, %d, %d, %d },\n" % (t, m, d, max(1, min(127, v))))
            f.write("    { 0xFFFF, 0, 0, 0 } };\n")
        f.write("static const uint8_t sc_bar_section[SC_BARS] = { %s };\n" % ", ".join(map(str, bar_section)))
        f.write("static const char *const sc_section_name[] = {\n%s };\n" % ",\n".join('    "%s"' % s[0] for s in SECTIONS))
    print("score: %d bars, %d ticks, %d events" % (total // 8, total, sum(len(v) for v in ev.values())))


if __name__ == "__main__":
    main()
