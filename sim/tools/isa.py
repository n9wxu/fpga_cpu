"""Load cpu/isa.yaml, flatten it into concrete encodings, validate.

Shared by gen.py (C + docs generation) and asm.py (assembler).
Run directly for a self-test:  python isa.py [--selftest]
"""
import os
import random
import re
import sys

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ISA_PATH = os.path.join(ROOT, "cpu", "isa.yaml")

REG_OPNDS = {"rd", "rs", "rt", "rx"}
PAIR_OPNDS = {"pd", "ps", "pt", "pn"}
IMM_OPNDS = {"imm", "disp", "target", "abs"}
MISC_OPNDS = {"cc", "sr", "scale", "ext"}
ALL_OPNDS = REG_OPNDS | PAIR_OPNDS | IMM_OPNDS | MISC_OPNDS
MEM_CLASSES = {"ld", "st", "ldp", "stp", "lea"}


class IsaError(Exception):
    pass


class FieldSpec:
    """How one pattern letter maps to operand(s)."""

    def __init__(self, letter, spec):
        self.letter = letter
        self.opnds = []
        self.sign = "u"
        self.scale = 1      # value = raw * scale
        self.div = 1        # value = raw / div   (pd/2: raw = 2*pd)
        self.pcrel = False
        if isinstance(spec, list):
            self.opnds = [self._parse(s) for s in spec]
            if len(set(self.opnds)) != len(self.opnds):
                raise IsaError(f"duplicate operand in {spec}")
        else:
            self.opnds = [self._parse(spec)]

    def _parse(self, s):
        m = re.fullmatch(r"(\w+)(?::([suw]))?(?:\*(\d+))?(?:/(\d+))?(@pc)?", s)
        if not m:
            raise IsaError(f"bad field spec {s!r}")
        name, sign, scale, div, pc = m.groups()
        if name not in ALL_OPNDS:
            raise IsaError(f"unknown operand {name!r} in {s!r}")
        if sign:
            self.sign = sign
        elif name in ("imm", "disp", "target"):
            raise IsaError(f"immediate {s!r} needs :s, :u or :w")
        if scale:
            self.scale = int(scale)
        if div:
            self.div = int(div)
        if pc:
            self.pcrel = True
        return name


class Field:
    """A placed field: spec + bit positions (MSB first) in the instruction word."""

    def __init__(self, spec, positions):
        self.spec = spec
        self.positions = positions
        self.width = len(positions)

    @property
    def opnd(self):
        return self.spec.opnds[0]

    def runs(self):
        """Contiguous runs (shift, width), MSB first."""
        runs = []
        for p in self.positions:
            if runs and runs[-1][0] - 1 == p:
                sh, w = runs[-1]
                runs[-1] = (p, w + 1)
            else:
                runs.append((p, 1))
        return runs

    def raw_range(self):
        w = self.width
        if self.spec.sign == "s":
            return -(1 << (w - 1)), (1 << (w - 1)) - 1
        if self.spec.sign == "w":
            return -(1 << (w - 1)), (1 << w) - 1
        return 0, (1 << w) - 1

    def range(self):
        w = self.width
        if self.spec.sign == "s":
            lo, hi = -(1 << (w - 1)), (1 << (w - 1)) - 1
        elif self.spec.sign == "w":
            lo, hi = -(1 << (w - 1)), (1 << w) - 1
        else:
            lo, hi = 0, (1 << w) - 1
        return lo * self.spec.scale, hi * self.spec.scale


class Form:
    """One concrete encoding of one op in one addressing mode."""

    limits = {}     # operand -> max value (set by Isa, e.g. cc)

    def __init__(self, variant, op, length, mask, value, fields, fixed, mode, pat, group):
        self.variant = variant
        self.op = op
        self.len = length          # bits: 16 or 32
        self.mask = mask
        self.value = value
        self.fields = fields
        self.fixed = fixed         # opnd -> value
        self.mode = mode           # None or d/x/pi/pd/a
        self.pat = pat
        self.group = group
        self.index = -1

    @property
    def nbytes(self):
        return self.len // 8

    def specificity(self):
        return bin(self.mask).count("1")

    def name(self):
        m = f".{self.mode}" if self.mode else ""
        fx = "".join(f",{k}={v}" for k, v in sorted(self.fixed.items()))
        return f"{self.op}{m}/{self.len}{fx}"

    # ---- encoding ----
    def encode(self, vals, pc=0, check=True):
        """vals: operand -> int. Returns the instruction word or raises IsaError."""
        for k, v in self.fixed.items():
            if vals.get(k, v) != v:
                raise IsaError(f"{self.name()}: {k} must be {v}")
        for k, lim in Form.limits.items():
            if k in vals and not (0 <= vals[k] <= lim):
                raise IsaError(f"{self.name()}: {k}={vals[k]} out of range")
        word = self.value
        for f in self.fields:
            vs = [vals.get(o) for o in f.spec.opnds]
            if any(v is None for v in vs):
                raise IsaError(f"{self.name()}: missing {f.spec.opnds}")
            if len(set(vs)) != 1:
                raise IsaError(f"{self.name()}: {f.spec.opnds} must be equal")
            v = vs[0]
            if f.spec.pcrel:
                v = v - pc
            if v % f.spec.scale:
                raise IsaError(f"{self.name()}: {f.opnd}={v} not a multiple of {f.spec.scale}")
            raw = (v // f.spec.scale) * f.spec.div
            lo, hi = f.raw_range()
            if check and not (lo <= raw <= hi):
                raise IsaError(f"{self.name()}: {f.opnd}={vals[f.opnd]} out of range")
            raw &= (1 << f.width) - 1
            # scatter raw bits into positions (MSB first)
            for i, p in enumerate(f.positions):
                bit = (raw >> (f.width - 1 - i)) & 1
                word |= bit << p
        return word

    def decode(self, word, pc=0):
        if (word & self.mask) != self.value:
            return None
        vals = dict(self.fixed)
        for f in self.fields:
            raw = 0
            for p in f.positions:
                raw = (raw << 1) | ((word >> p) & 1)
            if f.spec.sign in ("s",) and raw & (1 << (f.width - 1)):
                raw -= 1 << f.width
            v = raw // f.spec.div * f.spec.scale
            if f.spec.pcrel:
                v += pc
            for o in f.spec.opnds:
                vals[o] = v
        return vals


class Variant:
    def __init__(self, name, doc, long_prefixes):
        self.name = name
        self.doc = doc
        self.long_rules = []       # (mask, value) on the first parcel
        for p in long_prefixes:
            n = len(p)
            self.long_rules.append((((1 << n) - 1) << (16 - n), int(p, 2) << (16 - n)))
        self.forms = []

    def is_long(self, parcel):
        return any((parcel & m) == v for m, v in self.long_rules)

    def by_op(self):
        d = {}
        for f in self.forms:
            d.setdefault(f.op, []).append(f)
        for v in d.values():
            v.sort(key=lambda f: f.len)
        return d


class Isa:
    def __init__(self, path=ISA_PATH):
        with open(path) as fh:
            self.y = yaml.safe_load(fh)
        y = self.y
        self.conditions = y["conditions"]
        Form.limits = {"cc": len(self.conditions) - 1, "sr": 7}
        self.tables = y["tables"]
        self.classes = y["classes"]
        self.modes = y["modes"]
        self.ops = {}
        for name, o in y["ops"].items():
            if o["cls"] not in self.classes:
                raise IsaError(f"{name}: unknown class {o['cls']}")
            self.ops[name] = o
        self.op_list = list(self.ops)
        self.variants = {}
        for vname, vy in y["variants"].items():
            v = Variant(vname, vy.get("doc", ""), vy["long"])
            prefix = vy["prefix"]
            for gname, gforms in y["groups"].items():
                if gname not in prefix:
                    raise IsaError(f"variant {vname}: no prefix for group {gname}")
                for spec in gforms:
                    self._flatten(v, spec, prefix[gname], gname)
            for spec in vy.get("forms", []):
                grp = spec.get("grp")
                self._flatten(v, spec, prefix[grp] if grp else "", grp or vname)
            self._validate(v)
            self.variants[vname] = v

    # ---- flattening ----
    def _flatten(self, v, spec, prefix, group):
        pat_src = spec["pat"]
        pat = pat_src.replace("{P}", prefix).replace(" ", "")
        if len(pat) not in (16, 32):
            raise IsaError(f"{group}: pattern {pat_src!r} is {len(pat)} bits")
        if "op" in spec:
            choices = [(spec["op"], {})]
        else:
            ((letter, table),) = spec["opsel"].items()
            if isinstance(table, str):
                table = self.tables[table]
            choices = [(op, {letter: code}) for code, op in enumerate(table) if op]
        enum_letter, enum_vals = None, {None: {}}
        if "enum" in spec:
            ((enum_letter, enum_vals),) = spec["enum"].items()
        for op, opsel_bits in choices:
            if op not in self.ops:
                raise IsaError(f"{group}: unknown op {op}")
            o = self.ops[op]
            cls = o["cls"]
            mode = spec.get("mode")
            if mode and mode not in o.get("modes", list(self.modes)):
                continue
            if cls in MEM_CLASSES and not mode:
                raise IsaError(f"{group}: {op} needs a mode")
            if mode and cls not in MEM_CLASSES:
                raise IsaError(f"{group}: {op} takes no memory mode")
            fmap = dict(spec.get("f", {}))
            fo = spec.get("fo", {})
            fmap.update(fo.get(cls, {}))
            fmap.update(fo.get(op.lower(), {}))
            for ecode, evals in enum_vals.items():
                bits = {}
                if enum_letter is not None:
                    bits[enum_letter] = ecode
                bits.update(opsel_bits)
                self._make_form(v, op, cls, pat, pat_src, mode, fmap, bits,
                                dict(spec.get("fix", {}), **evals), group)

    def _make_form(self, v, op, cls, pat, pat_src, mode, fmap, fixed_letters, fixed, group):
        n = len(pat)
        letters = {}
        for i, ch in enumerate(pat):
            if ch in "01":
                continue
            letters.setdefault(ch, []).append(n - 1 - i)
        mask = value = 0
        fields = []
        for i, ch in enumerate(pat):
            if ch in "01":
                mask |= 1 << (n - 1 - i)
                value |= int(ch) << (n - 1 - i)
        for ch, pos in letters.items():
            if ch in fixed_letters:
                code = fixed_letters[ch]
                for j, p in enumerate(pos):
                    mask |= 1 << p
                    value |= ((code >> (len(pos) - 1 - j)) & 1) << p
                continue
            if ch not in fmap:
                raise IsaError(f"{group}: {op}: letter {ch!r} not mapped in {pat_src!r}")
            fspec = fmap[ch]
            if fspec is None:
                for p in pos:
                    mask |= 1 << p
                continue
            fields.append(Field(FieldSpec(ch, fspec), pos))
        # every operand the op needs must be provided
        provided = set(fixed)
        for f in fields:
            provided.update(f.spec.opnds)
        need = [x for x in self.classes[cls]["opnds"] if x != "mem"]
        if op == "B":
            need.append("cc")
        if mode:
            need += self.modes[mode]["needs"]
        missing = [x for x in need if x not in provided]
        extra = [x for x in provided if x not in need]
        if missing or extra:
            raise IsaError(f"{group}: {op}.{mode} in {pat_src!r}: missing {missing} extra {extra}")
        if mode == "x":
            fixed.setdefault("ext", 0)
        form = Form(v.name, op, n, mask, value, fields, fixed, mode, pat_src, group)
        v.forms.append(form)

    # ---- validation ----
    def _validate(self, v):
        for f in v.forms:
            p0m, p0v = (f.mask >> (f.len - 16)) & 0xFFFF, (f.value >> (f.len - 16)) & 0xFFFF
            if f.len == 16:
                for rm, rv in v.long_rules:
                    if ((p0v ^ rv) & p0m & rm) == 0:
                        raise IsaError(f"{v.name}: 16-bit form {f.name()} {f.pat!r} can look like a 32-bit prefix")
            else:
                if not any((rm & p0m) == rm and (p0v & rm) == rv for rm, rv in v.long_rules):
                    raise IsaError(f"{v.name}: 32-bit form {f.name()} {f.pat!r} lacks a long prefix")
        for L in (16, 32):
            fs = [f for f in v.forms if f.len == L]
            for i, a in enumerate(fs):
                for b in fs[i + 1:]:
                    if ((a.value ^ b.value) & a.mask & b.mask) != 0:
                        continue
                    # overlap: allowed only if one is strictly more specific
                    if (a.mask & b.mask) == a.mask and a.mask != b.mask:
                        continue
                    if (a.mask & b.mask) == b.mask and a.mask != b.mask:
                        continue
                    raise IsaError(f"{v.name}: {a.name()} {a.pat!r} overlaps {b.name()} {b.pat!r}")
        # decode order: most specific first within each length
        v.forms.sort(key=lambda f: (f.len, -f.specificity()))
        for i, f in enumerate(v.forms):
            f.index = i

    # ---- helpers ----
    def decode(self, vname, word32, pc=0):
        """Decode from a 32-bit window (first parcel in the high half)."""
        v = self.variants[vname]
        p0 = (word32 >> 16) & 0xFFFF
        if v.is_long(p0):
            cand, w = [f for f in v.forms if f.len == 32], word32
        else:
            cand, w = [f for f in v.forms if f.len == 16], p0
        for f in cand:
            vals = f.decode(w, pc)
            if vals is not None:
                return f, vals
        return None, None


def selftest(isa, n=200):
    rnd = random.Random(1)
    total = 0
    for v in isa.variants.values():
        for f in v.forms:
            for _ in range(n):
                vals = dict(f.fixed)
                for fl in f.fields:
                    lo, hi = fl.range()
                    sc = fl.spec.scale
                    if fl.spec.sign == "w":
                        lo = 0
                    x = rnd.randint(lo // sc, hi // sc) * sc
                    if fl.opnd in Form.limits:
                        x = min(x, Form.limits[fl.opnd])
                    if fl.spec.div > 1:
                        x = rnd.randint(0, ((1 << fl.width) - 1) // fl.spec.div)
                    if fl.spec.pcrel:
                        x += 0x1000
                    for o in fl.spec.opnds:
                        vals[o] = x
                w = f.encode(vals, pc=0x1000)
                win = w << 16 if f.len == 16 else w
                g, got = isa.decode(v.name, win, pc=0x1000)
                if g is not f or got != vals:
                    raise IsaError(f"{v.name}: round trip failed for {f.name()}: {vals} -> {w:#x} -> "
                                   f"{g.name() if g else None} {got}")
                total += 1
    return total


if __name__ == "__main__":
    isa = Isa()
    for v in isa.variants.values():
        n16 = sum(1 for f in v.forms if f.len == 16)
        print(f"variant {v.name}: {len(v.forms)} encodings ({n16} 16-bit, {len(v.forms) - n16} 32-bit)")
    if "--selftest" in sys.argv:
        print(f"selftest: {selftest(isa)} encode/decode round trips OK")
