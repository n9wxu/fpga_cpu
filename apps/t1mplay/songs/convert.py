#!/usr/bin/env python3
"""The Mozart and the Bach the engine plays (apps/sym40, apps/toccata) as T1M modules: the same
notes, instruments and samples, said the T1M way (docs/t1m.md). The notes and samples are read from
the apps' own generated headers (their tools/score.py, instruments.py, pipes.py), and each engine
setting of the apps is turned back into physical terms (ms, Hz, cents, levels).

    convert.py sym40   OUT.t1m       Symphony No. 40, I (15 parts, 15 channels)
    convert.py toccata OUT.t1m       Toccata and Fugue in D minor (12 parts: flues and reeds, 24 channels)
"""
import math
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
APPS = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "tools"))
import t1mc  # noqa: E402

BLOCKS = 1500.0                          # the engine's blocks a second (its envelope and LFO units below)


# ---- the engine's units, back to physical ones
def attack_ms(a):        # Q15 a block, rising
    return 32767 / a / BLOCKS * 1000 if a else 0


def tc_ms(k):            # Q15 of the distance a block: the time constant
    return 32768 / k / BLOCKS * 1000 if k else 0


def lfo_hz(r):           # phase a block, << 16 dropped
    return r * BLOCKS / 65536


def vib_cents(q):        # Q15 depth of the step
    return 1200 * math.log2(1 + q / 32768)


def rate_for(base_hz):
    """a sample whose loop sounds base_hz at 48 kHz: its rate and base note (the nearest MIDI note
    below, the rate scaled so the note is exact)"""
    m = math.floor(69 + 12 * math.log2(base_hz / 440))
    f = 440 * 2 ** ((m - 69) / 12)
    return int(round(48000 * f / base_hz)), m


# ---- reading the apps' generated headers
def build(app, *targets):
    subprocess.check_call(["make", "-s", "-C", os.path.join(APPS, app)] + list(targets))
    return os.path.join(APPS, app, "build")


def c_array(text, name):
    m = re.search(r"\b%s\[[^\]]*\][^=]*=\s*\{(.*?)\};" % re.escape(name), text, re.S)
    if not m:
        raise SystemExit("convert: no array %s" % name)
    return [int(v) for v in re.findall(r"-?\d+", m.group(1))]


def c_events(text, name, fields):
    m = re.search(r"\bsc_%s\[\]\s*=\s*\{(.*?)\};" % re.escape(name), text, re.S)
    out = []
    for g in re.findall(r"\{([^{}]*)\}", m.group(1)):
        v = [int(x, 0) for x in g.split(",")]
        if v[0] == 0xFFFF:
            break
        out.append(dict(zip(fields, v)))
    return out


def inst(name, **kw):
    d = t1mc.new_inst(name)
    d.update(kw)
    return d


def sample(song, name, frames, rate, note):
    s = t1mc.make_sample(frames, rate, True, (0, len(frames)), False)
    s["name"], s["note"] = name, note
    song.sample_ix[name] = len(song.samples)
    song.samples.append(s)
    return song.sample_ix[name]


def place(song, events, rows_per_pattern=256):
    """events: (row, channel, cell) -> patterns of up to 256 rows and the order"""
    total = max(r for r, _, _ in events) + 2
    npat = (total + rows_per_pattern - 1) // rows_per_pattern
    if npat > 255:
        raise SystemExit("convert: too long (%d patterns)" % npat)
    song.patterns = [dict(rows=min(rows_per_pattern, total - k * rows_per_pattern), cells={}) for k in range(npat)]
    for r, ch, cell in events:
        p = song.patterns[r // rows_per_pattern]["cells"]
        key = (r % rows_per_pattern, ch)
        if key in p and "note" in p[key] and p[key]["note"] != 0x80 and cell.get("note") == 0x80:
            continue                                   # a note starting here beats the old one's key off
        p.setdefault(key, {}).update(cell)
    song.order = list(range(npat))


# ---- Mozart: Symphony No. 40, I (apps/sym40)
SYM_PARTS = [  # part, nodes, patch
    ("vn1", 4, "VN1"), ("vn2", 4, "VN2"), ("va1", 1, "VA"), ("va2", 1, "VA"), ("vc", 2, "VC"), ("cb", 1, "BASS"),
    ("fl", 1, "FL"), ("ob1", 1, "OB"), ("ob2", 1, "OB"), ("cl1", 1, "CL"), ("cl2", 1, "CL"), ("bn1", 1, "BN"),
    ("bn2", 1, "BN"), ("hn1", 1, "HN"), ("hn2", 1, "HN")]
# sym40.c's patches: sample or source, volume (Q15 at velocity 127), a, d, r, s, width, lfo, vib, route
SYM_PATCH = {
    "VN1": ("violin", 0x1C00, 0x0300, 0x0100, 0x0500, 0x6800, 0, 0, 0, 0),
    "VN2": ("violin", 0x0B00, 0x0300, 0x0100, 0x0500, 0x6000, 0, 0, 0, 0),
    "VA": ("lowstr", 0x0900, 0x0400, 0x0100, 0x0400, 0x6000, 0, 0, 0, 0),
    "VC": ("lowstr", 0x0D00, 0x0300, 0x0100, 0x0400, 0x6000, 0, 0, 0, 0),
    "BASS": ("lowstr", 0x1000, 0x0400, 0x0100, 0x0300, 0x6800, 0, 0, 0, 0),
    "HN": ("horn", 0x0A00, 0x0180, 0x0080, 0x0300, 0x6800, 0, 0, 0, 0),
    "FL": ("tri", 0x1800, 0x0500, 0x0080, 0x0400, 0x6000, 0, 260, 420, 0),
    "OB": ("pulse", 0x0A00, 0x0800, 0x0100, 0x0600, 0x5800, 1400, 250, 250, 1),
    "CL": ("pulse", 0x0C00, 0x0600, 0x0100, 0x0500, 0x6000, 2048, 0, 0, 1),
    "BN": ("pulse", 0x0900, 0x0600, 0x0100, 0x0500, 0x6000, 1400, 0, 0, 1),
}


def sym40():
    b = build("sym40", "build/score.h", "build/instruments.h")
    score, smp = open(os.path.join(b, "score.h")).read(), open(os.path.join(b, "instruments.h")).read()
    n = int(re.search(r"#define INS_LEN (\d+)", smp).group(1))
    song = t1mc.Song()
    song.inst = []
    song.title, song.author = "Symphony No. 40 in G minor, K. 550: I", "W. A. Mozart (arr. for the Tang)"
    song.comment = "converted from apps/sym40 by apps/t1mplay/songs/convert.py"
    song.channels, song.speed, song.mix, song.volume, song.restart = len(SYM_PARTS), 1, 1.0, 255, 0
    song.tick = 1500 / 208                     # an eighth note (sym40's TICK_BLOCKS)
    rate, note = rate_for(750)                 # each loop: 128 cycles of 750 Hz in 8192 frames
    for nm in ("violin", "lowstr", "horn"):
        sample(song, nm, c_array(smp, "ins_" + nm)[:n], rate, note)
    # the winds' filter: sym40's flt node (F 0x2800, damping 0x5800), a low-pass at about 2.4 kHz
    song.filters[0] = dict(mode=0, pan=128, cutoff=int(round(48000 / math.pi * math.asin(0x2800 / 65536))),
                           q=int(round(256 * 32768 / 0x5800)), lfo=0, sweep=0, level=255)
    ix = {}
    for part, nodes, pname in SYM_PARTS:
        key = (pname, nodes)
        if key in ix:
            continue
        src, vol, a, d, r, s, width, lfo, vib, route = SYM_PATCH[pname]
        kw = dict(level=min(255, round(vol / 128)), pan=128, attack=round(attack_ms(a)), decay=round(tc_ms(d)),
                  release=round(tc_ms(r)), sustain=min(255, round(s / 128)), unison=nodes,
                  detune=round(1200 * math.log2((1 + (nodes - 1) / 1024) / (1 - (nodes - 1) / 1024))) if nodes > 1 else 0,
                  flags=1 if route else 0)
        if src in ("violin", "lowstr", "horn"):
            kw.update(src=4, sample=song.sample_ix[src])
        else:
            kw.update(src=t1mc.SRC[src], width=width * 16, lfo=round(lfo_hz(lfo) * 256), vib=round(vib_cents(vib)))
        ix[key] = len(song.inst)
        song.inst.append(inst("%s%s" % (pname.title(), "" if nodes == 1 else " x%d" % nodes), **kw))
    events = []
    for ch, (part, nodes, pname) in enumerate(SYM_PARTS):
        last_inst = None
        for e in c_events(score, part, ("t", "midi", "len", "vel")):
            if not 0 < e["midi"] < 128:
                continue                       # (a rest moved an octave by the arrangement: no note)
            cell = dict(note=e["midi"], vol=min(255, 2 * e["vel"]))
            i = ix[(pname, nodes)]
            if i != last_inst:
                cell["inst"] = last_inst = i
            events.append((e["t"], ch, cell))
            events.append((e["t"] + e["len"], ch, dict(note=0x80)))
    place(song, events)
    return song


# ---- Bach: Toccata and Fugue in D minor (apps/toccata)
TIMBRES = ["man_soft", "man_mid", "man_full", "ped", "ped_full", "reed"]
# toccata.c's registrations, by the section's number: (timbre, semitones, reed detune (1/4096 of the step), vol, attack)
MANUAL = [[("man_soft", 0, 0, 0x1080, 0x0500)], [("man_mid", 0, 0, 0x1080, 0x0500)], [("man_full", 0, 0, 0x1200, 0x0600)],
          [("man_full", 0, 0, 0x1080, 0x0600), ("reed", 0, 6, 0x0A80, 0x0900)]]
PEDAL = [[("ped", -12, 0, 0x1E00, 0x0300)], [("ped", -12, 0, 0x1E00, 0x0300)], [("ped_full", -12, 0, 0x1E00, 0x0300)],
         [("ped_full", -12, 0, 0x1C80, 0x0300), ("reed", -12, 5, 0x0F00, 0x0500)]]
DECAY, RELEASE = 0x0100, 0x0060          # toccata.c: node_init and REL_RATE


def toccata():
    b = build("toccata", "build/score.h", "build/pipes.h")
    score, pipes = open(os.path.join(b, "score.h")).read(), open(os.path.join(b, "pipes.h")).read()
    n = int(re.search(r"#define PIPE_LEN (\d+)", pipes).group(1))
    top = c_array(pipes, "pipe_top")
    parts = re.search(r"sc_part\[SC_PARTS\]\s*=\s*\{([^}]*)\}", score).group(1).replace("sc_", "").replace(" ", "").split(",")
    pedal = c_array(score, "sc_part_pedal")
    sec_tick, sec_reg = c_array(score, "sc_sec_tick"), c_array(score, "sc_sec_reg")
    song = t1mc.Song()
    song.inst = []
    song.title, song.author = "Toccata and Fugue in D minor, BWV 565", "J. S. Bach (attr.); Mutopia 1780"
    song.comment = "converted from apps/toccata by apps/t1mplay/songs/convert.py"
    song.channels = 2 * len(parts)           # each part: its flue rank, and its reed at full organ
    song.speed, song.tick, song.mix, song.volume, song.restart = 1, 1500 / 32, 1.25, 255, 0   # (toccata pans louder: x1.31 on the near side)
    for t in TIMBRES:                        # a pipe sample a range: a 1024-frame loop of whole cycles
        for r in range(4):
            rate, note = rate_for(375 * 2 ** r)
            sample(song, "%s%d" % (t, r), c_array(pipes, "pipe_%s%d" % (t, r))[:n], rate, note)
    ix = {}

    def rank_inst(rk, rng):
        key = rk + (rng,)
        if key not in ix:
            t, semis, det, vol, att = rk
            ix[key] = len(song.inst)
            song.inst.append(inst("%s %d%s" % (t.replace("_", " "), rng, "" if not semis else " 16'"), src=4,
                                  sample=song.sample_ix["%s%d" % (t, rng)], level=min(255, round(vol / 128)),
                                  attack=round(attack_ms(att)), decay=round(tc_ms(DECAY)), sustain=255,
                                  release=round(tc_ms(RELEASE)), transpose=semis,
                                  fine=round(1200 * math.log2(1 + det / 4096))))
        return ix[key]

    events = []
    for p, part in enumerate(parts):
        last = [None, None]
        for e in c_events(score, part, ("t", "len", "midi", "vel", "sec")):
            s = max(i for i, st in enumerate(sec_tick) if st <= e["t"])
            reg = (PEDAL if pedal[p] else MANUAL)[sec_reg[s]]
            pan = 128 if pedal[p] else (168 if e["midi"] & 1 else 88)    # the C and C# chests
            for k, rk in enumerate(reg):
                m = e["midi"] + rk[1]
                while m > 108:
                    m -= 12                  # a rank too high breaks back an octave
                rng = 0
                while rng < 3 and m > top[rng]:
                    rng += 1
                i = rank_inst(rk, rng)
                ch = p + k * len(parts)
                cell = dict(note=m - rk[1], vol=min(255, 2 * (64 + e["vel"] // 2)), fx1=(0x08, pan))
                if i != last[k]:
                    cell["inst"] = last[k] = i
                events.append((e["t"], ch, cell))
                events.append((e["t"] + e["len"], ch, dict(note=0x80)))
    place(song, events)
    return song


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("sym40", "toccata"):
        raise SystemExit(__doc__)
    song = sym40() if sys.argv[1] == "sym40" else toccata()
    data, needs, voices = t1mc.build(song)
    open(sys.argv[2], "wb").write(data)
    print("%s: %d bytes, %d channels, %d patterns, %d instruments, %d samples, up to %d engine voices" %
          (sys.argv[2], len(data), song.channels, len(song.patterns), len(song.inst), len(song.samples), voices))


if __name__ == "__main__":
    main()
