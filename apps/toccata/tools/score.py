#!/usr/bin/env python3
"""toccata's score: J. S. Bach (attr.), Toccata and Fugue in D minor, BWV 565, complete: the
toccata, the fugue and the closing recitative, from data/ToccataFugue.mid, the Mutopia Project's
edition (piece 1780, typeset from the Bach-Gesellschaft Ausgabe of 1867 and placed in the public
domain by its typesetter; www.mutopiaproject.org).

The MIDI has the notes at one tempo, by hand (right, left, pedal). Here they get the edition's
tempo marks as a tempo map, the opening's mordents and fermatas (which the MIDI leaves out), and
real time: a tick is 32 engine blocks (1/46.875 s), so the falling notes move at one speed through
the tempo changes. Each hand's chords are dealt onto monophonic parts (6 right, 4 left, 2 pedal);
each note is shown as a voice by where it sits in its hand: the right hand's top note soprano and
the rest alto, the left hand's top note tenor and the rest bass, the pedal its own.

Writes a C header: an event list per part {start tick, length, MIDI, velocity, section}, the
sections {start tick, registration} with their names, and the bars' start ticks.
usage: score.py MIDI OUT.h"""
import struct
import sys

TICK = 32 / 1500                        # seconds
Q = 384                                 # the MIDI's ticks a quarter
BAR = 4 * Q
SOP, ALTO, TENOR, BASS, PEDAL = range(5)

# the tempo map: (bar, beat from 0, quarter notes a minute); the edition's marks, our speeds
TEMPO = [
    (1, 0, 44),       # Adagio: the flourish
    (4, 0, 76),       # Prestissimo
    (10, 0, 56),      # the held chords
    (12, 0, 84),      # the broken chords in sixteenths
    (22, 0, 76),      # Prestissimo
    (28, 0, 56),      # the cadence
    (30, 2, 92),      # Fuga
    (127, 0, 60),     # Recitativo
    (130, 0, 40),     # Adagissimo
    (133, 0, 66),     # Presto
    (136, 3, 58),     # Adagio vivace
    (141, 0, 34),     # Molto adagio
]
# fermatas and breaths: (bar, beat, seconds added there; a note sounding through it is held)
PAUSE = [
    (1, 0.5, 0.9), (1, 1.95, 0.6), (2, 0.5, 0.9), (2, 1.95, 0.6), (3, 0.5, 0.9), (3, 1.95, 0.7),
    (30, 1.95, 1.2), (127, 1.95, 0.8), (133, 1.95, 0.8),
]
# the sections shown, with the organ's registration (0 soft .. 3 full organ with the reeds)
SECTIONS = [
    (1, 0, "Toccata: Adagio, the flourish", 3),
    (4, 0, "Toccata: Prestissimo", 3),
    (12, 0, "Toccata: the broken chords", 2),
    (22, 0, "Toccata: Prestissimo", 3),
    (28, 0, "Toccata: the cadence", 3),
    (30, 2, "Fugue: the subject and its answers", 1),
    (52, 0, "Fugue: the subject in the pedals", 2),
    (66, 0, "Fugue: episodes and entries", 1),
    (87, 0, "Fugue: the pedals again", 2),
    (109, 0, "Fugue: the last entries, full organ", 3),
    (127, 0, "Recitativo", 3),
    (130, 0, "Adagissimo", 3),
    (133, 0, "Presto", 3),
    (136, 3, "Adagio vivace", 3),
    (141, 0, "Molto adagio", 3),
]
SLOTS = {1: ['r%d' % i for i in range(6)], 2: ['l%d' % i for i in range(4)], 3: ['p0', 'p1']}
PARTS = SLOTS[1] + SLOTS[2] + SLOTS[3]


def read_midi(fn):
    d = open(fn, 'rb').read()
    ntr, div = struct.unpack('>HH', d[10:14])
    assert div == Q
    p, tracks = 14, []

    def vlq(p):
        v = 0
        while True:
            b = d[p]; p += 1; v = (v << 7) | (b & 0x7F)
            if b < 0x80:
                return v, p
    for _ in range(ntr):
        ln = struct.unpack('>I', d[p + 4:p + 8])[0]
        q, end, p = p + 8, p + 8 + ln, p + 8 + ln
        tick, run, on, notes = 0, 0, {}, []
        while q < end:
            dt, q = vlq(q); tick += dt
            st = d[q]
            if st == 0xFF:
                l, q2 = vlq(q + 2); q = q2 + l; continue
            if st in (0xF0, 0xF7):
                l, q = vlq(q + 1); q += l; continue
            if st & 0x80:
                run = st; q += 1
            c = run & 0xF0
            if c in (0x80, 0x90):
                k, v = d[q], d[q + 1]; q += 2
                if c == 0x90 and v > 0:
                    on.setdefault(k, []).append(tick)
                elif on.get(k):
                    s = on[k].pop(0); notes.append((s, k, tick - s))
            elif c in (0xC0, 0xD0):
                q += 1
            else:
                q += 2
        tracks.append(sorted(notes))
    return tracks


def at(bar, beat):
    return round((bar - 1) * BAR + beat * Q)


def seconds(t):
    """MIDI tick -> seconds, through the tempo map and the pauses before it"""
    s, marks = 0.0, [(at(b, bt), bpm) for b, bt, bpm in TEMPO]
    for i, (m, bpm) in enumerate(marks):
        nxt = marks[i + 1][0] if i + 1 < len(marks) else 1 << 30
        if t <= m:
            break
        s += (min(t, nxt) - m) / Q * 60 / bpm
    return s + sum(sec for b, bt, sec in PAUSE if at(b, bt) < t)


def ornaments(tracks):
    """the opening's mordents: an eighth-note A with a fermata, played A-G-A"""
    for tr in (1, 2):
        out = []
        for s, k, l in tracks[tr]:
            if s < 2 * BAR and k % 12 == 9 and l == Q // 2:
                out += [(s, k, 12), (s + 12, k - 2, 12), (s + 24, k, l - 24)]
            else:
                out.append((s, k, l))
        tracks[tr] = sorted(out)


def main():
    tracks = read_midi(sys.argv[1])
    ornaments(tracks)
    ev = {p: [] for p in PARTS}
    last = 0
    for tr, slots in SLOTS.items():
        notes = tracks[tr]
        busy = {p: (-1, 60) for p in slots}               # each slot: until (MIDI tick), its last pitch
        for s, k, l in notes:
            last = max(last, s + l)
            if tr == 3:
                sec = PEDAL
            else:                                          # where it sits among its hand's notes
                with_it = [kk for ss, kk, ll in notes if ss <= s < ss + ll]
                top = k >= max(with_it)
                sec = (SOP if top else ALTO) if tr == 1 else (TENOR if top else BASS)
            free = [p for p in slots if busy[p][0] <= s]
            if not free:
                print("dropped", tr, s, k, file=sys.stderr)
                continue
            p = min(free, key=lambda p: abs(busy[p][1] - k))   # the slot that played nearest
            busy[p] = (s + l, k)
            a, b = round(seconds(s) / TICK), round(seconds(s + l) / TICK)
            n = b - a
            if n > 4:
                n -= 1                                     # a breath: repeated notes show apart
            vel = 100 if tr != 3 else 110
            ev[p].append((a, max(1, n), k, vel, sec))
    total = round(seconds(last) / TICK) + 160              # and the church rings on
    bars = [round(seconds(at(b, 0)) / TICK) for b in range(1, last // BAR + 2) if at(b, 0) <= last]
    secs = [(round(seconds(at(b, bt)) / TICK), name, reg) for b, bt, name, reg in SECTIONS]
    with open(sys.argv[2], 'w') as f:
        f.write("/* generated by apps/toccata/tools/score.py from data/ToccataFugue.mid (Mutopia 1780, public domain) */\n")
        f.write("#define SC_TICKS %d\n#define SC_BARS %d\n#define SC_SECTIONS %d\n" % (total, len(bars), len(secs)))
        f.write("typedef struct { uint16_t t, len; uint8_t midi, vel, sec, pad; } sc_ev_t;\n")
        nev = 0
        for p in PARTS:
            e = sorted(ev[p])
            nev += len(e)
            f.write("static const sc_ev_t sc_%s[] = {\n" % p)
            for t, n, k, v, s in e:
                f.write("    { %d, %d, %d, %d, %d, 0 },\n" % (t, n, k, v, s))
            f.write("    { 0xFFFF, 0, 0, 0, 0, 0 } };\n")
        f.write("#define SC_PARTS %d\n" % len(PARTS))
        f.write("static const sc_ev_t *const sc_part[SC_PARTS] = { %s };\n" % ", ".join("sc_" + p for p in PARTS))
        f.write("static const uint8_t sc_part_pedal[SC_PARTS] = { %s };\n" % ", ".join(str(int(p[0] == 'p')) for p in PARTS))
        f.write("static const uint16_t sc_bar[SC_BARS] = { %s };\n" % ", ".join(map(str, bars)))
        f.write("static const uint16_t sc_sec_tick[SC_SECTIONS] = { %s };\n" % ", ".join(str(s[0]) for s in secs))
        f.write("static const uint8_t sc_sec_reg[SC_SECTIONS] = { %s };\n" % ", ".join(str(s[2]) for s in secs))
        f.write("static const char *const sc_sec_name[SC_SECTIONS] = {\n%s };\n" % ",\n".join('    "%s"' % s[1] for s in secs))
    print("score: %d ticks (%d:%02d), %d bars, %d events" % (total, total * TICK // 60, total * TICK % 60, len(bars), nev))


if __name__ == "__main__":
    main()
