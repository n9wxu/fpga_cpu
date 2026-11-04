#!/usr/bin/env python3
"""Build T1S's decode ROM (verilog/rtl/t1s_rom_x0.vh, _x1.vh) from T1F's decoder.

T1S decodes through one block RAM: a 10-bit key (t1s_key in t1s_dec.v) made
of the instruction's opcode and whichever other bits choose what it does
indexes a ROM word of controls; the instruction's own fields (registers,
immediates) come from a registered copy of the parcels through small
selects whose codes are in the word too; and illegal encodings are a mask
of "these bits must be zero" tests, also in the word.

Nothing here is written by hand from the ISA: for every key the generator
runs T1F's decoder (t1_dec.v, through t1f_dec_dump.v) on
  - a representative (the key's bits, everything else zero): the controls,
  - the representative with each zero-test made true in turn: the mask,
  - random legal instructions with that key: for each field, the one
    candidate slice that matches all of them.
t1s_dec_tb.v then checks the whole decoder against T1F's on every first
parcel and many second parcels.

  t1sgen.py [--ext-x 0|1] [--samples N]
"""
import argparse, os, random, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RTL = os.path.join(ROOT, "verilog", "rtl")
BUILD = os.path.join(ROOT, "verilog", "build")

FIELDS = ["cls", "fn", "ra", "rb", "rd", "xr", "shn", "cc", "fsh", "mp", "bimm", "mld", "sfill", "zx",
          "shreg", "pstop", "mw", "msig", "narrow", "ifill", "corr", "imm", "pn", "sr", "msz", "mmode",
          "sc", "shk", "pa", "pb", "tcode"]


def bit(x, i):
    return (x >> i) & 1


def bits(x, hi, lo):
    return (x >> lo) & ((1 << (hi - lo + 1)) - 1)


# ---------------------------------------------------------------- the key
def key(ir, p2):
    top = bits(ir, 15, 12)
    if top == 0xA:
        return 1 << 8 | bits(ir, 11, 8) << 4 | bits(ir, 3, 0)
    if top == 0xE:
        if bit(ir, 11):
            return 2 << 8 | 1 << 7 | bits(ir, 10, 8) << 4
        pp = int(bits(p2, 11, 9) == 4)
        return 2 << 8 | bits(ir, 10, 8) << 4 | bit(ir, 0) << 3 | pp << 2 | bit(p2, 8) << 1
    if top == 0xF:
        if bit(ir, 11):
            return 3 << 8 | 1 << 7 | bits(ir, 10, 8) << 4 | bits(p2, 11, 8)
        return 3 << 8 | bits(ir, 10, 7) << 3
    sub = {0x0: bits(ir, 3, 0), 0x6: bit(ir, 4), 0x7: bit(ir, 5), 0x8: bit(ir, 7),
           0x9: bits(ir, 11, 8), 0xB: bit(ir, 4), 0xD: bit(ir, 8)}.get(top, 0)
    return top << 4 | sub


def fixed(k):
    """The bits key k fixes: (ir mask, ir value, p2 mask, p2 value), or None
    if no instruction has key k."""
    g = k >> 8
    if g == 0:
        top, sub = bits(k, 7, 4), bits(k, 3, 0)
        if top in (0xA, 0xE, 0xF):
            return None
        pos = {0x0: [3, 2, 1, 0], 0x6: [4], 0x7: [5], 0x8: [7], 0x9: [11, 10, 9, 8], 0xB: [4],
               0xD: [8]}.get(top, [])
        if sub >> len(pos):
            return None
        im, iv = 0xF000, top << 12
        for j, p in enumerate(reversed(pos)):
            im |= 1 << p
            iv |= bit(sub, j) << p
        return im, iv, 0, 0
    if g == 1:
        return 0xFF0F, 0xA000 | bits(k, 7, 4) << 8 | bits(k, 3, 0), 0, 0
    if g == 2:
        if bit(k, 7):
            if bits(k, 3, 0):
                return None
            return 0xFF00, 0xE800 | bits(k, 6, 4) << 8, 0, 0
        if bit(k, 0):
            return None
        im, iv = 0xFF01, 0xE000 | bits(k, 6, 4) << 8 | bit(k, 3)
        pp, b8 = bit(k, 2), bit(k, 1)
        pm, pv = 0x0100, b8 << 8
        if pp:
            pm, pv = 0x0F00, 0x0800 | b8 << 8
        return im, iv, pm, pv
    if bit(k, 7):
        return 0xFF00, 0xF800 | bits(k, 6, 4) << 8, 0x0F00, bits(k, 3, 0) << 8
    if bits(k, 2, 0):
        return None
    return 0xFF80, 0xF000 | bits(k, 6, 3) << 7, 0, 0


def legalize(k, ir, p2):
    """Force key k's bits; for E-form memory keys without pi/pd, keep
    p2[11:9] off 100 (that would be the pi/pd key)."""
    im, iv, pm, pv = fixed(k)
    ir = (ir & ~im & 0xFFFF) | iv
    p2 = (p2 & ~pm & 0xFFFF) | pv
    if k >> 8 == 2 and not bit(k, 7) and not bit(k, 2) and bits(p2, 11, 9) == 4:
        p2 |= 1 << 10
    assert key(ir, p2) == k, (hex(k), hex(ir), hex(p2))
    return ir, p2


# ---------------------------------------------------------------- zero tests
# (name, bits as (word, bit); the test is true when any is 1, or for
# "p2[9:8]==3" when both are)
FEATS = [
    ("ir7", [(0, 7)]), ("ir6_4", [(0, 6), (0, 5), (0, 4)]), ("ir3", [(0, 3)]),
    ("ir2_0", [(0, 2), (0, 1), (0, 0)]), ("ir4", [(0, 4)]), ("ir0", [(0, 0)]),
    ("p2_7_4", [(1, 7), (1, 6), (1, 5), (1, 4)]), ("p2_3_0", [(1, 3), (1, 2), (1, 1), (1, 0)]),
    ("p2_15_12", [(1, 15), (1, 14), (1, 13), (1, 12)]), ("p2_12", [(1, 12)]), ("p2_11", [(1, 11)]),
    ("p2_9_8", [(1, 9), (1, 8)]),
]
ALLBITS = {5: True}


def feat(i, ir, p2):
    name, bl = FEATS[i]
    w = (ir, p2)
    if name == "p2_9_8":
        return int(bits(p2, 9, 8) == 3)
    return int(any(bit(w[a], b) for a, b in bl))


def free_bits(k):
    im, iv, pm, pv = fixed(k)
    return im, pm


# ---------------------------------------------------------------- field candidates
def sx(v, n, to=24):
    return (v - (1 << n) if v >> (n - 1) else v) & ((1 << to) - 1)


def boff(ir):
    return (((bit(ir, 7) << 8) | bits(ir, 7, 0)) - 1) & 0x1FF


REG4 = [lambda i, p: 0, lambda i, p: bits(i, 11, 8), lambda i, p: bits(i, 7, 4), lambda i, p: bits(i, 3, 0),
        lambda i, p: bits(p, 15, 12), lambda i, p: bits(i, 11, 9) << 1, lambda i, p: bits(i, 6, 4) << 1,
        lambda i, p: bits(i, 2, 0) << 1, lambda i, p: bits(i, 3, 1) << 1]
PN = [lambda i, p: 0, lambda i, p: bits(i, 7, 5), lambda i, p: bits(i, 8, 6), lambda i, p: bits(i, 2, 0),
      lambda i, p: bits(i, 3, 1), lambda i, p: 7]
SHN = [lambda i, p: 0, lambda i, p: bits(i, 3, 0), lambda i, p: bits(p, 3, 0)]
SR = [lambda i, p: 0, lambda i, p: bits(i, 2, 0)]
TC = [lambda i, p: 0xFE, lambda i, p: 0xFF, lambda i, p: bits(i, 7, 0)]


def b16(i, p=0):
    b = boff(i)
    return (bit(b, 8) * 0xFF << 16 | bit(b, 8) * 0x7F << 9 | bits(b, 7, 0) << 1) & 0xFFFFFF


IMM = [lambda i, p: 0, lambda i, p: sx(bits(i, 7, 0), 8), lambda i, p: bits(i, 4, 0) << 1,
       lambda i, p: bits(i, 3, 0), lambda i, p: bits(i, 6, 0) << 1, b16,
       lambda i, p: 1, lambda i, p: 2, lambda i, p: 4, lambda i, p: sx(bits(i, 8, 0), 9),
       lambda i, p: bits(i, 7, 0) << 16 | p, lambda i, p: sx(p, 16), lambda i, p: p,
       lambda i, p: (bits(i, 6, 0) << 17 | p << 1) & 0xFFFFFF]
IFILL = [lambda i, p: 0, lambda i, p: bit(i, 7), lambda i, p: bit(p, 15), lambda i, p: bit(boff(i), 8),
         lambda i, p: bit(i, 8)]
NARROW = [lambda i, p: 0, lambda i, p: 1, lambda i, p: int(bit(boff(i), 8) == bit(boff(i), 7))]
ZX = [lambda i, p: 0, lambda i, p: bit(p, 10), lambda i, p: bit(p, 8)]
SC = [lambda i, p: 0, lambda i, p: 1, lambda i, p: bits(p, 9, 8)]

SELS = [("ra", REG4), ("rb", REG4), ("rd", REG4), ("xr", REG4), ("pn", PN), ("shn", SHN), ("sr", SR),
        ("tcode", TC), ("imm", IMM), ("ifill", IFILL), ("narrow", NARROW), ("zx", ZX), ("sc", SC)]
# from the key alone
CTL = ["cls", "fn", "cc", "shk", "fsh", "mp", "bimm", "mld", "sfill", "shreg", "pstop", "mw", "msig", "corr",
       "msz", "mmode", "pa", "pb"]

# the ROM word: (name, width), low bits first
LAYOUT = [("cls", 4), ("fn", 4), ("cc", 4), ("shk", 2), ("fsh", 1), ("mp", 1), ("bimm", 1), ("mld", 1),
          ("sfill", 1), ("shreg", 1), ("pstop", 1), ("mw", 1), ("msig", 1), ("corr", 1), ("msz", 2),
          ("mmode", 2), ("pa", 2), ("pb", 2),
          ("s_ra", 4), ("s_rb", 4), ("s_rd", 4), ("s_xr", 4), ("s_pn", 3), ("s_shn", 2), ("s_sr", 1),
          ("s_tcode", 2), ("s_imm", 4), ("s_ifill", 3), ("s_narrow", 2), ("s_zx", 2), ("s_sc", 2),
          ("mask", len(FEATS))]
WIDTH = sum(w for _, w in LAYOUT)


def run_ref(vecs, ext_x):
    os.makedirs(BUILD, exist_ok=True)
    vvp = os.path.join(BUILD, f"t1f_dec_dump_x{ext_x}.vvp")
    r = subprocess.run(["iverilog", "-g2001", f"-Pt1f_dec_dump.EXT_X={ext_x}", "-o", vvp,
                        os.path.join(HERE, "t1f_dec_dump.v"), os.path.join(RTL, "t1_dec.v")],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as f:
        for ir, p2 in vecs:
            f.write(f"{ir:04x} {p2:04x}\n")
        path = f.name
    out = subprocess.run(["vvp", "-n", vvp, f"+vec={path}"], capture_output=True, text=True).stdout
    os.unlink(path)
    rows = [l.split() for l in out.splitlines() if len(l.split()) == len(FIELDS)]
    assert len(rows) == len(vecs), (len(rows), len(vecs))
    return [dict(zip(FIELDS, (int(x, 16) for x in r))) for r in rows]


def is_ill(o):
    return o["cls"] == 12 and o["tcode"] == 0xFE


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ext-x", type=int, default=0)
    ap.add_argument("--samples", type=int, default=48)
    a = ap.parse_args()
    rnd = random.Random(1)

    keys = [k for k in range(1024) if fixed(k) is not None]
    # pass 1: representatives and zero-test probes
    vecs, plan = [], []
    for k in keys:
        rep = legalize(k, 0, 0)
        plan.append((k, "rep", None, len(vecs))); vecs.append(rep)
        im, pm = free_bits(k)
        for fi, (name, bl) in enumerate(FEATS):
            ir, p2 = rep
            if feat(fi, ir, p2):
                continue                                   # true by the key: the key's own business
            freeb = [(w, b) for w, b in bl if not ((im if w == 0 else pm) >> b) & 1]
            if not freeb:
                continue
            for w, b in (freeb if name == "p2_9_8" else freeb[:1]):
                if w == 0:
                    ir |= 1 << b
                else:
                    p2 |= 1 << b
            ir, p2 = legalize(k, ir, p2)
            if not feat(fi, ir, p2):
                continue                                   # cannot be true with this key
            plan.append((k, "probe", fi, len(vecs))); vecs.append((ir, p2))
    out = run_ref(vecs, a.ext_x)
    rep_of, mask_of = {}, {}
    for k, kind, fi, idx in plan:
        if kind == "rep":
            rep_of[k] = (vecs[idx], out[idx]); mask_of[k] = 0
        elif not is_ill(rep_of[k][1]) and is_ill(out[idx]):
            mask_of[k] |= 1 << fi

    # pass 2: random legal samples of each key, for the field selects
    vecs2, owner = [], []
    for k in keys:
        if is_ill(rep_of[k][1]):
            continue
        pats = [(0, 0), (0xFFFF, 0xFFFF), (0x5555, 0x5555), (0xAAAA, 0xAAAA), (0x00FF, 0xFF00), (0xFF00, 0x00FF)]
        pats += [(1 << b, 0) for b in range(16)] + [(0, 1 << b) for b in range(16)]       # one bit set
        pats += [(0xFFFF ^ 1 << b, 0xFFFF) for b in range(16)] + [(0xFFFF, 0xFFFF ^ 1 << b) for b in range(16)]
        for j in range(a.samples + len(pats)):
            ir, p2 = pats[j] if j < len(pats) else (rnd.getrandbits(16), rnd.getrandbits(16))
            ir, p2 = legalize(k, ir, p2)
            for fi, (name, bl) in enumerate(FEATS):          # keep it legal: the masked tests false
                if mask_of[k] >> fi & 1:
                    if name == "p2_9_8":
                        if bits(p2, 9, 8) == 3:
                            p2 &= ~(1 << 9)
                    else:
                        for w, b in bl:
                            if w == 0:
                                ir &= ~(1 << b)
                            else:
                                p2 &= ~(1 << b)
            ir, p2 = legalize(k, ir, p2)
            vecs2.append((ir, p2)); owner.append(k)
    out2 = run_ref(vecs2, a.ext_x)
    samples = {}
    for (v, o, k) in zip(vecs2, out2, owner):
        samples.setdefault(k, []).append((v, o))

    rom = {}
    for k in keys:
        (rir, rp2), ro = rep_of[k]
        word = {n: ro[n] for n in CTL}
        for (ir, p2), o in samples.get(k, []):                # the key must decide the controls
            if not is_ill(o):
                for n in CTL:
                    if o[n] != ro[n]:
                        sys.exit(f"key {k:03x}: {n} varies ({ir:04x} {p2:04x}: {o[n]:x}, rep {ro[n]:x})")
        word["mask"] = mask_of[k]
        if is_ill(ro):
            for n, _ in SELS:
                word["s_" + n] = 0
        else:
            for n, cands in SELS:
                ok = [ci for ci, c in enumerate(cands)
                      if all(is_ill(o) or c(ir, p2) == o[n] for (ir, p2), o in samples[k])]
                if not ok:
                    ex = samples[k][0]
                    sys.exit(f"key {k:03x} (rep {rir:04x} {rp2:04x}): no candidate for {n}; "
                             f"e.g. {ex[0][0]:04x} {ex[0][1]:04x} -> {ex[1][n]:x}")
                word["s_" + n] = ok[0]
        v, pos = 0, 0
        for n, w in LAYOUT:
            assert word[n] < (1 << w), (k, n, word[n])
            v |= word[n] << pos
            pos += w
        rom[k] = v

    path = os.path.join(RTL, f"t1s_rom_x{a.ext_x}.vh")
    with open(path, "w") as f:
        f.write(f"// T1S decode ROM: generated by verilog/tb/t1sgen.py (EXT_X={a.ext_x}) from T1F's decoder.\n")
        f.write(f"// {WIDTH} bits a word: " + ", ".join(f"{n}[{w}]" for n, w in LAYOUT) + "\n")
        f.write("case (rkey)\n")
        for k in sorted(rom):
            f.write(f"10'h{k:03x}: rword <= {WIDTH}'h{rom[k]:0{(WIDTH + 3) // 4}x};\n")
        f.write(f"default: rword <= {WIDTH}'h0;\nendcase\n")
    print(f"{len(rom)} keys, {WIDTH} bits a word -> {path}")


if __name__ == "__main__":
    main()
