#!/usr/bin/env python3
"""t1mc: compile a T1M text source (.t1s) to a T1M module (.t1m), or print a module (docs/t1m.md).

    t1mc.py SONG.t1s OUT.t1m        compile (samples are found relative to the source)
    t1mc.py --dump SONG.t1m         print a module back
"""
import argparse
import os
import re
import struct
import sys
import wave

MAGIC = b"T1M\x1a"
SRC = {"saw": 0, "tri": 1, "triangle": 1, "pulse": 2, "noise": 3, "sample": 4, "pair": 5}
NEED_NOISE, NEED_SMP, NEED_SMP16, NEED_SYNC, NEED_RING, NEED_FILT, NEED_UNISON, NEED_SEQ, NEED_TAILS = (1 << i for i in range(9))
NNA = {"cut": 0, "release": 1}
SEQ_KIND = {"volume": 0, "arp": 1, "pitch": 2, "width": 3}
NOTES = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
MAXFRAMES, GUARD = 32764, 256


class Err(Exception):
    pass


def note_num(s):
    """'C-4', 'C#4', 'Db4' -> MIDI (C-4 = 60)"""
    m = re.fullmatch(r"([A-Ga-g])([-#b])(-?\d)", s)
    if not m:
        raise Err("bad note %r" % s)
    n = NOTES[m.group(1).upper()] + {"-": 0, "#": 1, "b": -1}[m.group(2)] + 12 * (int(m.group(3)) + 1)
    if not 0 <= n <= 127:
        raise Err("note out of range %r" % s)
    return n


def note_name(n):
    return "%s%s%d" % ("C-C#D-D#E-F-F#G-G#A-A#B-"[2 * (n % 12):2 * (n % 12) + 2][0],
                       "C-C#D-D#E-F-F#G-G#A-A#B-"[2 * (n % 12) + 1], n // 12 - 1)


def level(s, top=255):
    """'0.7', '70%', or an integer 0-255"""
    if s.endswith("%"):
        v = float(s[:-1]) / 100
    elif "." in s:
        v = float(s)
    else:
        return max(0, min(top, int(s)))
    return max(0, min(top, int(round(v * top))))


def u16(v):
    return max(0, min(0xFFFF, int(round(v))))


def s16(v):
    return max(-0x8000, min(0x7FFF, int(round(v))))


# ---- samples
def load_wav(path):
    w = wave.open(path, "rb")
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    if width == 1:
        vals = [(b - 128) << 8 for b in raw]
    elif width == 2:
        vals = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
    else:
        raise Err("%s: only 8- and 16-bit WAVs" % path)
    frames = [sum(vals[i:i + ch]) // ch for i in range(0, len(vals), ch)]     # stereo: mixed to mono
    return frames, rate, width == 2


def make_sample(frames, rate, b16, loop, pingpong):
    """the engine-independent rules (docs/t1m.md SMPL): lengths and loops in multiples of 4 frames,
    loops of at least 256 frames, at most 32764 frames; ping-pong and short loops unrolled"""
    if loop:
        ls, ll = loop
        ls, ll = max(0, min(ls, len(frames))), max(0, min(ll, len(frames) - ls))
        body, seg = frames[:ls], frames[ls:ls + ll]
        if pingpong and len(seg) > 2:
            seg = seg + seg[-2:0:-1]
        if not seg:
            loop = None
        else:
            rep = seg
            while len(rep) < 256 or len(rep) % 4:
                rep = rep + seg
                if len(rep) > 4096 and len(rep) % 4 == 0:
                    break
            if len(rep) % 4:                 # an odd length that will not tile: trim to 4
                rep = rep[:len(rep) & ~3]
            pad = (-len(body)) % 4           # loop start on a 4-frame boundary: delay the body
            body = [body[0] if body else 0] * pad + body
            frames, loop = body + rep, (len(body), len(rep))
    if len(frames) > MAXFRAMES:
        if loop and loop[0] + loop[1] > MAXFRAMES:
            raise Err("sample too long with its loop (%d frames, at most %d)" % (loop[0] + loop[1], MAXFRAMES))
        frames = frames[:MAXFRAMES]
    frames = frames + [0] * ((-len(frames)) % 4)
    guard = frames[loop[0]:loop[0] + GUARD] if loop else []
    guard = (guard * (GUARD // max(1, len(guard)) + 1))[:GUARD] if guard else [0] * GUARD
    data = frames + guard
    if b16:
        raw = struct.pack("<%dh" % len(data), *data)
    else:
        raw = bytes((max(-128, min(127, (v + 128) >> 8)) & 0xFF) for v in data)
    raw += b"\0" * ((-len(raw)) % 4)
    return dict(frames=len(frames), loop=loop or (0, 0), rate=rate, b16=b16, data=raw)


# ---- the source
class Song:
    def __init__(self):
        self.title = self.author = self.comment = ""
        self.channels, self.tick, self.speed, self.mix, self.volume, self.restart = 4, 50.0, 6, 1.0, 255, 0
        self.samples, self.sample_ix = [], {}
        self.filters = {}
        self.inst = {}
        self.seqs = []
        self.patterns, self.pat_ix = [], {}
        self.order = []


def new_inst(name):
    return dict(name=name, src=0, flags=0, sample=0, level=255, pan=128, sustain=255, attack=0, decay=0,
                release=0, width=32768, lfo=0, vib=0, pwm=0, offset=0, partner=255, unison=1, detune=0,
                spread=0, transpose=0, fine=0, nna=0, dup=1, seq=[0xFF] * 4)


def parse_seq(words):
    kind = SEQ_KIND[words[0]]
    steps, loop, rel = [], 0xFF, 0xFF
    for w in words[1:]:
        if w == "|":
            loop = len(steps)
        elif w == "/":
            rel = len(steps)
        else:
            v = int(w, 0)
            if kind in (1, 2):
                if not -128 <= v <= 127:
                    raise Err("sequence step out of range: %d" % v)
                v &= 0xFF
            elif not 0 <= v <= 255:
                raise Err("sequence step out of range: %d" % v)
            steps.append(v)
    if not 1 <= len(steps) <= 255:
        raise Err("a sequence has 1-255 steps")
    if loop != 0xFF and loop >= len(steps):
        loop = 0xFF
    if rel != 0xFF and rel >= len(steps):
        rel = 0xFF
    return kind, steps, loop, rel


def parse_inst(song, ins, words):
    i = 0
    while i < len(words):
        k = words[i]
        arg = words[i + 1] if i + 1 < len(words) else None
        i += 2
        if k in ("saw", "tri", "triangle", "pulse", "noise"):
            ins["src"] = SRC[k]
            i -= 1
        elif k == "sample":
            if arg not in song.sample_ix:
                raise Err("unknown sample %r" % arg)
            ins["src"], ins["sample"] = 4, song.sample_ix[arg]
        elif k in ("sync", "ring"):
            ins["src"] = 5
            ins["flags"] |= 2 if k == "sync" else 4
            i -= 1
        elif k == "level": ins["level"] = level(arg)
        elif k == "pan": ins["pan"] = level(arg)
        elif k == "sustain": ins["sustain"] = level(arg)
        elif k == "attack": ins["attack"] = u16(float(arg))
        elif k == "decay": ins["decay"] = u16(float(arg))
        elif k == "release": ins["release"] = u16(float(arg))
        elif k == "width": ins["width"] = level(arg, 65535)
        elif k == "pwm": ins["pwm"] = level(arg, 65535)
        elif k == "lfo": ins["lfo"] = u16(float(arg) * 256)
        elif k == "vibrato": ins["vib"] = u16(float(arg))
        elif k == "offset": ins["offset"] = s16(float(arg))
        elif k == "partner": ins["partner"] = level(arg)
        elif k == "unison": ins["unison"] = max(1, min(4, int(arg)))
        elif k == "detune": ins["detune"] = u16(float(arg))
        elif k == "spread": ins["spread"] = level(arg)
        elif k == "transpose": ins["transpose"] = max(-128, min(127, int(arg)))
        elif k == "fine": ins["fine"] = max(-128, min(127, int(arg)))
        elif k == "filter": ins["flags"] = (ins["flags"] & ~1) | (arg in ("on", "1", "yes"))
        elif k == "nna":
            if arg not in NNA:
                raise Err("nna: cut or release, not %r" % arg)
            ins["nna"] = NNA[arg]
        elif k == "dup": ins["dup"] = 1 if arg in ("on", "1", "yes") else 0
        elif k == "freelfo": ins["flags"] = (ins["flags"] & ~8) | (8 if arg in ("on", "1", "yes") else 0)
        else:
            raise Err("unknown instrument word %r" % k)


CELL_RE = re.compile(r"\s*(\S{3})\s+(\S\S)\s+(\S\S)\s+(\S{4})\s+(\S{4})\s*$")


def parse_cell(text, ninst):
    if not text.strip():
        return None
    m = CELL_RE.match(text)
    if not m:
        raise Err("bad cell %r (want 'NNN II VV CCPP CCPP')" % text.strip())
    n, ins, vol, f1, f2 = m.groups()
    cell = {}
    if n == "===":
        cell["note"] = 0x80
    elif n == "^^^":
        cell["note"] = 0x81
    elif n not in ("---", "..."):
        cell["note"] = note_num(n)
    if ins != "..":
        cell["inst"] = int(ins, 16)
        if cell["inst"] >= ninst:
            raise Err("instrument %02X not defined" % cell["inst"])
    if vol != "..":
        cell["vol"] = int(vol, 16)
    fx = [f for f in (f1, f2) if f != "...."]
    for k, f in enumerate(fx):
        cell["fx%d" % (k + 1)] = (int(f[:2], 16), int(f[2:], 16))
    return cell or None


def parse(path):
    song = Song()
    base = os.path.dirname(os.path.abspath(path))
    lines = open(path).read().splitlines()
    cur_inst = None
    cur_pat = None
    inst_defs = {}
    for ln, raw in enumerate(lines, 1):
        try:
            line = raw.split("#", 1)[0].rstrip() if not re.match(r"\s*[0-9A-Fa-f]{2}\s*\|", raw) else raw.rstrip()
            if not line.strip():
                continue
            if cur_pat is not None and re.match(r"\s*[0-9A-Fa-f]{2}\s*\|", line):
                row_s, rest = line.split("|", 1)
                row = int(row_s, 16)
                if row >= cur_pat["rows"]:
                    raise Err("row %02X past the pattern's %d rows" % (row, cur_pat["rows"]))
                cells = rest.split("|")
                if len(cells) > song.channels:
                    if any(c.strip() for c in cells[song.channels:]):
                        raise Err("more cells than channels")
                for ch, c in enumerate(cells[:song.channels]):
                    cell = parse_cell(c, len(inst_defs))
                    if cell:
                        cur_pat["cells"][(row, ch)] = cell
                continue
            words = re.findall(r'"[^"]*"|\S+', line)
            k = words[0]
            if raw[:1].isspace() and cur_inst is not None:
                if k == "seq":
                    kind, steps, loop, rel = parse_seq(words[1:])
                    cur_inst["seq"][kind] = len(song.seqs)
                    song.seqs.append((kind, steps, loop, rel))
                else:
                    parse_inst(song, cur_inst, words)
                continue
            cur_inst = None
            if k != "pattern" and not (k == "order"):
                cur_pat = None if k not in ("pattern",) else cur_pat
            if k in ("title", "author", "comment"):
                setattr(song, k, words[1].strip('"'))
            elif k == "channels":
                song.channels = int(words[1])
                if not 1 <= song.channels <= 32:
                    raise Err("1-32 channels")
            elif k == "tick": song.tick = float(words[1])
            elif k == "speed": song.speed = int(words[1])
            elif k == "mix": song.mix = float(words[1])
            elif k == "volume": song.volume = level(words[1])
            elif k == "restart": song.restart = int(words[1])
            elif k == "sample":
                name, fn = words[1], os.path.join(base, words[2].strip('"'))
                frames, rate, b16 = load_wav(fn)
                bnote, loop, pp = 60, None, False
                j = 3
                while j < len(words):
                    if words[j] == "note": bnote = note_num(words[j + 1]); j += 2
                    elif words[j] == "loop": loop = (int(words[j + 1]), int(words[j + 2])); j += 3
                    elif words[j] == "pingpong": pp = True; j += 1
                    elif words[j] == "rate": rate = int(words[j + 1]); j += 2
                    elif words[j] == "bits": b16 = words[j + 1] == "16"; j += 2
                    else: raise Err("unknown sample word %r" % words[j])
                s = make_sample(frames, rate, b16, loop, pp)
                s["name"], s["note"] = name, bnote
                song.sample_ix[name] = len(song.samples)
                song.samples.append(s)
            elif k == "filter":
                u = int(words[1])
                if not 0 <= u <= 3:
                    raise Err("filters 0-3")
                f = dict(mode=0, pan=128, cutoff=1000, q=256, lfo=0, sweep=0, level=255)
                j = 2
                while j < len(words):
                    w = words[j]
                    if w in ("lowpass", "lp"): f["mode"] = 0; j += 1; continue
                    if w in ("bandpass", "bp"): f["mode"] = 1; j += 1; continue
                    a = words[j + 1]
                    if w == "cutoff": f["cutoff"] = u16(float(a))
                    elif w == "q": f["q"] = u16(float(a) * 256)
                    elif w == "lfo": f["lfo"] = u16(float(a) * 256)
                    elif w == "sweep": f["sweep"] = u16(float(a))
                    elif w == "level": f["level"] = level(a)
                    elif w == "pan": f["pan"] = level(a)
                    else: raise Err("unknown filter word %r" % w)
                    j += 2
                song.filters[u] = f
            elif k == "instrument":
                n = int(words[1], 16)
                if n in inst_defs:
                    raise Err("instrument %02X defined twice" % n)
                cur_inst = new_inst(words[2].strip('"') if len(words) > 2 and words[2].startswith('"') else "")
                inst_defs[n] = cur_inst
                parse_inst(song, cur_inst, words[3 if len(words) > 2 and words[2].startswith('"') else 2:])
            elif k == "pattern":
                n = int(words[1], 16)
                rows = int(words[2]) if len(words) > 2 else 64
                if not 1 <= rows <= 256:
                    raise Err("1-256 rows")
                cur_pat = dict(rows=rows, cells={})
                song.pat_ix[n] = cur_pat
            elif k == "order":
                song.order += [int(w, 16) for w in words[1:]]
            else:
                raise Err("unknown word %r" % k)
        except (Err, ValueError, KeyError, IndexError) as e:
            raise Err("%s:%d: %s" % (path, ln, e))
    if sorted(inst_defs) != list(range(len(inst_defs))):
        raise Err("instruments must be numbered 00, 01, ... without gaps")
    song.inst = [inst_defs[i] for i in range(len(inst_defs))]
    if sorted(song.pat_ix) != list(range(len(song.pat_ix))):
        raise Err("patterns must be numbered 00, 01, ... without gaps")
    song.patterns = [song.pat_ix[i] for i in range(len(song.pat_ix))]
    if not song.order:
        raise Err("no order list")
    for o in song.order:
        if o >= len(song.patterns):
            raise Err("order names pattern %02X, not defined" % o)
    if sorted(song.filters) != list(range(len(song.filters))):
        raise Err("filters must be numbered 0, 1, ... without gaps")
    return song


# ---- the module
def chunk(cid, data):
    return cid + struct.pack("<I", len(data)) + data + b"\0" * ((-len(data)) % 4)


def voices_of(ins):
    return ins["unison"] * (2 if ins["src"] == 5 else 1)


def voices_at_once(song):
    """the most engine voices sounding at once, the song played through its order list once (jumps
    not taken): each channel's note, and with the new-note action release its tails, each until its
    release has died away (6.9 time constants, -60 dB)"""
    nch = song.channels
    fg = [None] * nch                   # [inst, note, released at (or None), new-note action (or None)]
    tails = [[] for _ in range(nch)]    # [end, inst, note]
    cur = [None] * nch
    speed, hz, t, most = song.speed or 6, song.tick or 50, 0.0, 0

    def life(i):
        return 6.9 * song.inst[i]["release"] / 1000.0
    o, start = 0, 0
    while o < len(song.order):
        p = song.patterns[song.order[o]]
        brk = None
        for r in range(start, p["rows"]):
            for chn in range(nch):
                c = p["cells"].get((r, chn))
                if not c:
                    continue
                fx = [c[f] for f in ("fx1", "fx2") if f in c]
                nna = None
                for cmd, x in fx:
                    if cmd == 0x0F and x: speed = x
                    if cmd == 0x10 and x: hz = x
                    if cmd == 0x0D: brk = x
                    if cmd == 0x19 and x == 0: tails[chn] = []
                    if cmd == 0x19 and x in (1, 2): nna = x - 1
                if "inst" in c and c["inst"] < len(song.inst):
                    cur[chn] = c["inst"]
                n = c.get("note")
                if n is None or cur[chn] is None:
                    continue
                if n == 0x80:
                    if fg[chn] and fg[chn][2] is None: fg[chn][2] = t
                elif n == 0x81:
                    fg[chn] = None
                elif not any(cmd == 0x03 for cmd, _ in fx) or not fg[chn]:
                    i = cur[chn]
                    if song.inst[i]["dup"]:
                        tails[chn] = [x for x in tails[chn] if (x[1], x[2]) != (i, n)]
                    f = fg[chn]
                    if f and not (song.inst[i]["dup"] and (f[0], f[1]) == (i, n)):
                        if f[3] if f[3] is not None else song.inst[f[0]]["nna"]:
                            tails[chn].append([(f[2] if f[2] is not None else t) + life(f[0]), f[0], f[1]])
                    fg[chn] = [i, n, None, nna]
            for chn in range(nch):
                tails[chn] = [x for x in tails[chn] if x[0] > t]
                if fg[chn] and fg[chn][2] is not None and fg[chn][2] + life(fg[chn][0]) <= t:
                    fg[chn] = None
            most = max(most, sum(voices_of(song.inst[f[0]]) for f in fg if f) +
                       sum(voices_of(song.inst[x[1]]) for tl in tails for x in tl))
            t += speed / hz
            if brk is not None:
                break
        o, start = o + 1, brk or 0
    return most


def encode_patterns(song):
    """the row stream: 0x00 ends a row, 0x80 + n ends it and skips n empty rows"""
    blobs = []
    for p in song.patterns:
        out = bytearray(struct.pack("<H", p["rows"]))
        pending = 0                                    # rows ended but not yet written
        for r in range(p["rows"]):
            cells = [(ch, p["cells"][(r, ch)]) for ch in range(song.channels) if (r, ch) in p["cells"]]
            if cells:
                while pending:                         # the empty rows before this one
                    n = min(pending, 0x80)
                    out.append(0x80 + n - 1)
                    pending -= n
                for ch, c in cells:
                    fl = (("note" in c) | ("inst" in c) << 1 | ("vol" in c) << 2 | ("fx1" in c) << 3 | ("fx2" in c) << 4)
                    out += bytes([ch + 1, fl])
                    if "note" in c: out.append(c["note"])
                    if "inst" in c: out.append(c["inst"])
                    if "vol" in c: out.append(c["vol"])
                    for f in ("fx1", "fx2"):
                        if f in c: out += bytes(c[f])
                pending = 1                            # this row's end
            else:
                pending += 1
        while pending:
            n = min(pending, 0x80)
            out.append(0x80 + n - 1)
            pending -= n
        blobs.append(bytes(out))
    return blobs


def build(song):
    needs = 0
    for ins in song.inst:
        if ins["src"] == 3: needs |= NEED_NOISE
        if ins["src"] == 4:
            needs |= NEED_SMP
            if song.samples[ins["sample"]]["b16"]: needs |= NEED_SMP16
        if ins["src"] == 5:
            if ins["flags"] & 2: needs |= NEED_SYNC
            if ins["flags"] & 4: needs |= NEED_RING
        if ins["flags"] & 1: needs |= NEED_FILT
        if ins["unison"] > 1: needs |= NEED_UNISON
        if any(s != 0xFF for s in ins["seq"]): needs |= NEED_SEQ
        if ins["nna"]: needs |= NEED_TAILS
    if song.filters: needs |= NEED_FILT
    voices = voices_at_once(song)
    body = b""
    body += chunk(b"SONG", struct.pack("<BBHHBBHHBBBB", song.channels, song.speed, u16(song.tick * 256),
                                       u16(song.mix * 256), song.volume, song.restart, len(song.order),
                                       len(song.patterns), len(song.inst), len(song.samples), len(song.filters),
                                       len(song.seqs)))
    body += chunk(b"NAME", b"\0".join(s.encode("ascii", "replace") for s in (song.title, song.author, song.comment)) + b"\0")
    body += chunk(b"ORDR", bytes(song.order))
    if song.filters:
        body += chunk(b"FILT", b"".join(struct.pack("<BBHHHHB5x", f["mode"], f["pan"], f["cutoff"], f["q"], f["lfo"],
                                                    f["sweep"], f["level"]) for _, f in sorted(song.filters.items())))
    if song.samples:
        sd, tab = bytearray(), bytearray()
        for s in song.samples:
            tab += struct.pack("<IIIIHBB", len(sd), s["frames"], s["loop"][0], s["loop"][1], u16(s["rate"]), s["note"],
                               1 if s["b16"] else 0)
            sd += s["data"]
        body += chunk(b"SMPL", bytes(tab))
        body += chunk(b"SDAT", bytes(sd))
    ib = bytearray()
    for ins in song.inst:
        ib += struct.pack("<16sBBBBBBHHHHHHHhBBHBbbBBx4B4B8x", ins["name"].encode("ascii", "replace")[:16],
                          ins["src"], ins["flags"], ins["sample"], ins["level"], ins["pan"], ins["sustain"],
                          ins["attack"], ins["decay"], ins["release"], ins["width"], ins["lfo"], ins["vib"],
                          ins["pwm"], ins["offset"], ins["partner"], ins["unison"], ins["detune"], ins["spread"],
                          ins["transpose"], ins["fine"], ins["nna"], ins["dup"], *ins["seq"], 0xFF, 0xFF, 0xFF, 0xFF)
    body += chunk(b"INST", bytes(ib))
    if song.seqs:
        offs, data = [], bytearray()
        hdr = 2 * len(song.seqs)
        for kind, steps, loop, rel in song.seqs:
            offs.append(hdr + len(data))
            data += bytes([kind, len(steps), loop, rel]) + bytes(steps)
        body += chunk(b"SEQS", struct.pack("<%dH" % len(offs), *offs) + bytes(data))
    blobs = encode_patterns(song)
    offs, data = [], bytearray()
    hdr = 4 * len(blobs)
    for b in blobs:
        offs.append(hdr + len(data))
        data += b
        data += b"\0" * ((-len(data)) % 2)
    body += chunk(b"PATT", struct.pack("<%dI" % len(offs), *offs) + bytes(data))
    total = 16 + len(body)
    return MAGIC + struct.pack("<HHHHI", 1, needs, voices, 0, total) + body, needs, voices


# ---- the dump
def chunks(data):
    if data[:4] != MAGIC:
        raise Err("not a T1M module")
    ver, needs, voices, _, total = struct.unpack_from("<HHHHI", data, 4)
    out, p = {}, 16
    while p + 8 <= len(data):
        cid, n = data[p:p + 4].decode("latin-1"), struct.unpack_from("<I", data, p + 4)[0]
        out[cid] = data[p + 8:p + 8 + n]
        p += 8 + n + ((-n) % 4)
    return ver, needs, voices, total, out


FX = {0x00: "arp", 0x01: "slide up", 0x02: "slide down", 0x03: "portamento", 0x04: "vibrato", 0x05: "width",
      0x06: "pwm", 0x07: "width slide", 0x08: "pan", 0x09: "offset", 0x0A: "vol slide", 0x0B: "jump",
      0x0C: "volume", 0x0D: "break", 0x0E: "fine", 0x0F: "speed", 0x10: "tempo", 0x11: "global vol",
      0x12: "cut", 0x13: "delay", 0x14: "retrig", 0x15: "key off", 0x16: "routing", 0x17: "pair offset",
      0x18: "pair slide"}


def dump(path):
    data = open(path, "rb").read()
    ver, needs, voices, total, ch = chunks(data)
    names = ["noise", "samples", "16-bit", "sync", "ring", "filters", "unison", "sequences", "tails"]
    print("T1M version %d, %d bytes; needs %s; up to %d engine voices" %
          (ver, total, ", ".join(n for i, n in enumerate(names) if needs >> i & 1) or "nothing", voices))
    nch, speed, tick, mix, gvol, restart, nord, npat, nins, nsmp, nflt, nseq = struct.unpack("<BBHHBBHHBBBB", ch["SONG"])
    if "NAME" in ch:
        t = ch["NAME"].split(b"\0")
        print("%r by %r" % (t[0].decode(), t[1].decode()))
    print("%d channels, speed %d, tick %.2f Hz, mix %.2f, volume %d, restart %d" % (nch, speed, tick / 256, mix / 256, gvol, restart))
    print("order:", " ".join("%02X" % b for b in ch["ORDR"]))
    for i in range(nflt):
        mode, pan, cut, q, lfo, sw, lv = struct.unpack_from("<BBHHHHB", ch["FILT"], 16 * i)
        print("filter %d: %s %d Hz Q %.2f, LFO %.2f Hz sweep %d cents, level %d pan %d" %
              (i, ("low-pass", "band-pass")[mode], cut, q / 256, lfo / 256, sw, lv, pan))
    for i in range(nsmp):
        off, fr, ls, ll, rate, note, fl = struct.unpack_from("<IIIIHBB", ch["SMPL"], 20 * i)
        print("sample %d: %d frames %s-bit, %d Hz at %s, %s" % (i, fr, "16" if fl & 1 else "8", rate, note_name(note),
                                                             "loop %d+%d" % (ls, ll) if ll else "one-shot"))
    src = ["saw", "triangle", "pulse", "noise", "sample", "pair"]
    for i in range(nins):
        f = struct.unpack_from("<16sBBBBBBHHHHHHHhBBHBbbBBx4B4B8x", ch["INST"], 64 * i)
        name = f[0].rstrip(b"\0").decode()
        print("inst %02X %-16s %-8s flags %x level %d pan %d ADSR %d/%d/%d/%d width %d lfo %.2f vib %d pwm %d "
              "pair %+d/%d unison %d/%d/%d tr %d fine %d nna %s%s seq %s" %
              (i, name, src[f[1]], f[2], f[4], f[5], f[7], f[8], f[6], f[9], f[10], f[11] / 256, f[12], f[13], f[14],
               f[15], f[16], f[17], f[18], f[19], f[20], ("cut", "release")[f[21] & 1], " dup" if f[22] else "", f[23:27]))
    pt = ch["PATT"]
    for i in range(npat):
        off = struct.unpack_from("<I", pt, 4 * i)[0]
        rows = struct.unpack_from("<H", pt, off)[0]
        p, r, lines = off + 2, 0, {}
        while r < rows:
            b = pt[p]; p += 1
            if b == 0:
                r += 1
            elif b >= 0x80:
                r += b - 0x80 + 1
            else:
                c, fl = b - 1, pt[p]; p += 1
                s = []
                if fl & 1:
                    n = pt[p]; p += 1
                    s.append("===" if n == 0x80 else "^^^" if n == 0x81 else note_name(n))
                else:
                    s.append("---")
                if fl & 2: s.append("%02X" % pt[p]); p += 1
                else: s.append("..")
                if fl & 4: s.append("%02X" % pt[p]); p += 1
                else: s.append("..")
                for k in (8, 16):
                    if fl & k: s.append("%02X%02X" % (pt[p], pt[p + 1])); p += 2
                    else: s.append("....")
                lines.setdefault(r, {})[c] = " ".join(s)
        print("pattern %02X, %d rows" % (i, rows))
        for r in sorted(lines):
            print("%02X | %s" % (r, " | ".join(lines[r].get(c, "--- .. .. .... ....") for c in range(max(lines[r]) + 1))))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", action="store_true")
    ap.add_argument("src")
    ap.add_argument("out", nargs="?")
    a = ap.parse_args()
    try:
        if a.dump:
            dump(a.src)
            return
        song = parse(a.src)
        data, needs, voices = build(song)
        out = a.out or os.path.splitext(a.src)[0] + ".t1m"
        if os.path.dirname(out):
            os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out, "wb").write(data)
        print("%s: %d bytes, %d channels, %d patterns, %d instruments, %d samples, up to %d engine voices" %
              (out, len(data), song.channels, len(song.patterns), len(song.inst), len(song.samples), voices))
    except Err as e:
        sys.exit("t1mc: %s" % e)


if __name__ == "__main__":
    main()
