"""fpga_cpu assembler and linker, driven by cpu/isa.yaml.

usage: asm.py [-v A|B] src.s [more.s ...] [-o out.bin] [--lst out.lst]
              [--stats out.json] [--sym out.sym] [--base ADDR] [--lib lib.s ...]

Picks the shortest encoding that fits for every instruction, so the same
source measures each variant fairly. Branches and other label-dependent
sizes are settled over repeated passes; sizes only grow, so it converges.

Several source files are linked into one image: symbols are local to
their file unless declared .globl, and sections are laid out in the order
text, rodata, data, bss (bss is zeroed by the start-up code, not stored in
the image). The image starts at --base (default 0x1000) unless the first
file starts with .org. Files given with --lib are linked only when they
define a symbol that is still undefined, like members of an archive.

Syntax summary (see sim/README.md):
  registers r0-r15, pairs p0-p7 (sp = p7, lr = p6)
  ADD rd, rs, rt      ADD rd, rs  (= ADD rd, rd, rs)
  LDW r1, [p2 + 6]    LDW r1, [p2 + r3*2]   LDW r1, [p2 + r3.z*2]
  LDW r1, [p2]+       STW r1, -[p2]         LDW r1, [label]
  Bcc label  (EQ NE CS/HS CC/LO MI PL VS VC HI LS GE LT GT LE AL/RA)
  pseudo: LI LA CALL RET PUSH POP PUSHP POPP INC DEC CLR TST
  directives: .org .equ .set .byte .word .short .long .ascii .asciz .string
              .align .p2align .balign .space .zero .include .globl .comm
              .text .data .bss .section
"""
import ast
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from isa import Isa, IsaError  # noqa: E402


class AsmError(Exception):
    pass


class Unresolved(Exception):
    pass


COND_ALIASES = {"HS": "CS", "LO": "CC", "RA": "AL"}
PAIR_ALIASES = {"sp": 7, "lr": 6}
SR_NAMES = {"flags": 0, "cyclo": 1, "cychi": 2, "instret": 3}
SECTIONS = ("text", "rodata", "data", "bss")
IDENT = re.compile(r"(?<![\w.$])([A-Za-z_.$][\w.$]*)")
EXPR_FUNCS = ("lo", "hi", "lo8", "hi8")
IGNORED = {".file", ".ident", ".type", ".size", ".addrsig", ".addrsig_sym", ".hidden",
           ".protected", ".loc", ".cfi_sections", ".cfi_startproc", ".cfi_endproc",
           ".cfi_def_cfa_offset", ".cfi_offset", ".cfi_def_cfa", ".end"}


def split_operands(s):
    """Split on top-level commas (not inside [], () or quotes)."""
    out, depth, cur, q, esc = [], 0, "", None, False
    for ch in s:
        if q:
            cur += ch
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == q:
                q = None
            continue
        if ch in "\"'":
            q = ch
        elif ch in "[(":
            depth += 1
        elif ch in "])":
            depth -= 1
        elif ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
            continue
        cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def strip_comment(line):
    q, esc = None, False
    for i, ch in enumerate(line):
        if q:
            if esc:
                esc = False
            elif ch == "\\":
                esc = True
            elif ch == q:
                q = None
        elif ch in "\"'":
            q = ch
        elif ch == ";":
            return line[:i]
    return line


def section_of(name):
    n = name.strip('"')
    if n.startswith(".text"):
        return "text"
    if n.startswith(".rodata") or n.startswith(".data.rel.ro"):
        return "rodata"
    if n.startswith(".data") or n.startswith(".sdata"):
        return "data"
    if n.startswith(".bss") or n.startswith(".sbss") or n.startswith(".tbss"):
        return "bss"
    if n.startswith(".note") or n.startswith(".comment") or n.startswith(".debug"):
        return None  # discarded
    raise AsmError(f"unsupported section {name}")


class Stmt:
    def __init__(self, lineno, text, kind, name=None, args=None, labels=(), src="", section="text"):
        self.lineno = lineno
        self.text = text
        self.kind = kind          # 'inst', 'dir', 'equ'
        self.name = name
        self.args = args or []
        self.labels = list(labels)
        self.src = src
        self.section = section
        self.size = 0
        self.addr = 0
        self.expand = None        # for macro-expanded mem ops
        self.form = None


class Assembler:
    def __init__(self, isa, vname):
        self.isa = isa
        self.v = isa.variants[vname]
        self.forms = self.v.by_op()
        self.conds = {c: i for i, c in enumerate(isa.conditions)}
        self.syms = {}
        self.here = 0

    # ---- expressions ----
    def eval(self, s, allow_unresolved):
        src = s.strip()
        src = re.sub(r"'(\\.|[^'])'", lambda m: str(ord(bytes(m.group(1), "utf-8").decode("unicode_escape"))), src)
        names = []

        def sub(m):
            name = m.group(1)
            rest = src[m.end():].lstrip()
            if name in EXPR_FUNCS and rest.startswith("("):
                return name
            names.append(name)
            return f"__s{len(names) - 1}"

        src = IDENT.sub(sub, src)
        try:
            tree = ast.parse(src, mode="eval")
        except SyntaxError:
            raise AsmError(f"bad expression {s!r}")

        def ev(n):
            if isinstance(n, ast.Expression):
                return ev(n.body)
            if isinstance(n, ast.Constant) and isinstance(n.value, int):
                return n.value
            if isinstance(n, ast.Name):
                name = names[int(n.id[3:])]
                if name == "$":
                    return self.here
                if name in self.syms and self.syms[name] is not None:
                    return self.syms[name]
                if allow_unresolved:
                    raise Unresolved(name)
                raise AsmError(f"undefined symbol {name!r}")
            if isinstance(n, ast.UnaryOp):
                x = ev(n.operand)
                return {ast.USub: -x, ast.UAdd: x, ast.Invert: ~x}[type(n.op)]
            if isinstance(n, ast.BinOp):
                a, b = ev(n.left), ev(n.right)
                ops = {ast.Add: lambda: a + b, ast.Sub: lambda: a - b, ast.Mult: lambda: a * b,
                       ast.FloorDiv: lambda: a // b, ast.Div: lambda: a // b, ast.Mod: lambda: a % b,
                       ast.LShift: lambda: a << b, ast.RShift: lambda: a >> b,
                       ast.BitAnd: lambda: a & b, ast.BitOr: lambda: a | b, ast.BitXor: lambda: a ^ b}
                if type(n.op) in ops:
                    return ops[type(n.op)]()
            if isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and len(n.args) == 1:
                x = ev(n.args[0])
                f = {"lo": lambda: x & 0xFFFF, "hi": lambda: (x >> 16) & 0xFFFF,
                     "lo8": lambda: x & 0xFF, "hi8": lambda: (x >> 8) & 0xFF}.get(n.func.id)
                if f:
                    return f()
            raise AsmError(f"unsupported expression {s!r}")

        return ev(tree)

    # ---- operands ----
    @staticmethod
    def reg(s):
        m = re.fullmatch(r"r(\d+)", s.strip().lower())
        if not m or int(m.group(1)) > 15:
            raise AsmError(f"expected register r0-r15, got {s!r}")
        return int(m.group(1))

    @staticmethod
    def pair(s):
        t = s.strip().lower()
        if t in PAIR_ALIASES:
            return PAIR_ALIASES[t]
        m = re.fullmatch(r"p([0-7])", t)
        if not m:
            raise AsmError(f"expected pair p0-p7/sp/lr, got {s!r}")
        return int(m.group(1))

    def mem(self, s, allow):
        t = s.strip()
        if t.startswith("-[") and t.endswith("]"):
            return {"mode": "pd", "pn": self.pair(t[2:-1])}
        if t.startswith("[") and t.endswith("]+"):
            return {"mode": "pi", "pn": self.pair(t[1:-2])}
        if not (t.startswith("[") and t.endswith("]")):
            raise AsmError(f"expected memory operand, got {s!r}")
        inner = t[1:-1].strip()
        m = re.match(r"(p[0-7]|sp|lr)(?![\w.$])\s*(.*)$", inner, re.I)
        if not m:
            return {"mode": "a", "abs": self.val(inner, allow)}
        pn, rest = self.pair(m.group(1)), m.group(2).strip()
        if not rest:
            return {"mode": "d", "pn": pn, "disp": 0}
        x = re.fullmatch(r"\+\s*r(\d+)(\.z)?\s*(?:\*\s*(\d+))?", rest, re.I)
        if x:
            return {"mode": "x", "pn": pn, "rx": self.reg("r" + x.group(1)),
                    "ext": 1 if x.group(2) else 0, "scale": int(x.group(3) or 1)}
        if rest[0] not in "+-":
            raise AsmError(f"bad memory operand {s!r}")
        return {"mode": "d", "pn": pn, "disp": self.val("0" + rest, allow)}

    def val(self, s, allow):
        try:
            return self.eval(s, allow)
        except Unresolved:
            return None

    # ---- statement -> list of (op, values) ----
    def lower(self, st, allow):
        mn = st.name.upper()
        a = st.args
        pseudo = {
            "LI": lambda: ("MOVI", a),
            "LA": lambda: ("LEA", [a[0], f"[{a[1]}]"]),
            "CALL": lambda: ("JAL", a),
            "RET": lambda: ("JR", ["lr"]),
            "PUSH": lambda: ("STW", [a[0], "-[sp]"]),
            "POP": lambda: ("LDW", [a[0], "[sp]+"]),
            "PUSHP": lambda: ("STP", [a[0], "-[sp]"]),
            "POPP": lambda: ("LDP", [a[0], "[sp]+"]),
            "INC": lambda: ("ADDI", [a[0], a[0], "1"]),
            "DEC": lambda: ("ADDI", [a[0], a[0], "-1"]),
            "CLR": lambda: ("MOVI", [a[0], "0"]),
            "TST": lambda: ("CMPI", [a[0], "0"]),
        }
        cc = None
        if mn in pseudo:
            mn, a = pseudo[mn]()
        elif mn not in self.isa.ops and mn.startswith("B"):
            c = COND_ALIASES.get(mn[1:], mn[1:])
            if c not in self.conds:
                raise AsmError(f"unknown mnemonic {st.name!r}")
            mn, cc = "B", self.conds[c]
        if mn not in self.isa.ops:
            raise AsmError(f"unknown mnemonic {st.name!r}")
        op = self.isa.ops[mn]
        cls = op["cls"]
        names = self.isa.classes[cls]["opnds"]
        if len(a) == len(names) - 1 and cls in ("bin", "bini", "addp", "addpi"):
            a = [a[0]] + a
        if len(a) != len(names):
            raise AsmError(f"{mn} expects {len(names)} operands ({', '.join(names)}), got {len(a)}")
        vals = {}
        if cc is not None:
            vals["cc"] = cc
        for name, text in zip(names, a):
            if name in ("rd", "rs", "rt", "rx"):
                vals[name] = self.reg(text)
            elif name in ("pd", "ps", "pt", "pn"):
                vals[name] = self.pair(text)
            elif name == "mem":
                vals.update(self.mem(text, allow))
            elif name == "sr":
                t = text.strip().lower()
                vals["sr"] = SR_NAMES[t] if t in SR_NAMES else self.val(re.sub(r"^sr", "", t), allow)
            else:
                vals[name] = self.val(text, allow)
        return mn, vals

    def candidates(self, mn, vals):
        mode = vals.get("mode")
        fs = [f for f in self.forms.get(mn, []) if f.mode == mode]
        if self.isa.ops[mn].get("comm") and "rs" in vals and "rt" in vals and vals["rs"] != vals["rt"]:
            sw = dict(vals, rs=vals["rt"], rt=vals["rs"])
            out = [(f, vals) for f in fs] + [(f, sw) for f in fs]
            out.sort(key=lambda fv: fv[0].len)
            return out
        return [(f, vals) for f in fs]

    def choose(self, mn, vals, pc, size=None, final=False):
        """Return (form, vals) for the shortest (or exactly `size`-byte) encoding that fits."""
        cands = self.candidates(mn, vals)
        errs = []
        for f, v in cands:
            if size is not None and f.nbytes != size:
                continue
            probe = {k: (0 if x is None else x) for k, x in v.items()}
            if not final and "target" in v and v["target"] is None:
                probe["target"] = pc
            try:
                f.encode(probe, pc=pc, check=final or all(x is not None for x in v.values()))
                return f, v
            except IsaError as e:
                errs.append(str(e))
        return None, errs

    def alternatives(self, mn, vals):
        """Each part of an instruction, as a list of equivalent (op, vals) choices."""
        if vals.get("mode") == "a" and not any(f.mode == "a" for f in self.forms.get(mn, [])):
            # no direct form in this variant: LEA p0, [abs] ; op [p0]
            return [[("LEA", {"pd": 0, "mode": "a", "abs": vals["abs"]})],
                    [(mn, dict({k: x for k, x in vals.items() if k != "abs"}, mode="d", pn=0, disp=0))]], "abs"
        if mn == "LEA" and vals.get("mode") == "d":
            # LEA pd, [ps + d] == ADDPI pd, ps, d (both full 32-bit adds)
            return [[(mn, vals), ("ADDPI", {"pd": vals["pd"], "ps": vals["pn"], "imm": vals["disp"]})]], None
        if mn == "ADDPI":
            return [[(mn, vals), ("LEA", {"pd": vals["pd"], "mode": "d", "pn": vals["ps"], "disp": vals["imm"]})]], None
        return [[(mn, vals)]], None

    # ---- parsing and linking ----
    def parse(self, lines, src):
        stmts = []
        globs = set()
        locs = set()   # .local: a following .comm stays file-local (C statics)
        section = "text"
        for i, raw in lines:
            line = strip_comment(raw).strip()
            labels = []
            while True:
                m = re.match(r"([A-Za-z_.$][\w.$]*)\s*:(?!=)\s*(.*)$", line)
                if not m:
                    break
                labels.append(m.group(1))
                line = m.group(2).strip()
            if not line:
                if labels:
                    stmts.append(Stmt(i, raw, "dir", ".label", [], labels, src, section))
                continue
            m = re.match(r"([A-Za-z_][\w.]*)\s*=\s*(.+)$", line)
            if m:
                stmts.append(Stmt(i, raw, "equ", m.group(1), [m.group(2)], labels, src, section))
                continue
            m = re.match(r"(\.?[A-Za-z_][\w.]*)\s*(.*)$", line)
            if not m:
                raise AsmError(f"{src}:{i}: cannot parse {raw.strip()!r}")
            name, rest = m.group(1), m.group(2)
            d = name.lower()
            if d in (".equ", ".set"):
                n, e = rest.split(",", 1)
                stmts.append(Stmt(i, raw, "equ", n.strip(), [e], labels, src, section))
            elif d in (".globl", ".global", ".weak"):
                globs.update(x.strip() for x in rest.split(","))
                if labels:
                    stmts.append(Stmt(i, raw, "dir", ".label", [], labels, src, section))
            elif d == ".local":
                locs.update(x.strip() for x in rest.split(","))
                if labels:
                    stmts.append(Stmt(i, raw, "dir", ".label", [], labels, src, section))
            elif d in (".text", ".data", ".bss", ".rodata", ".section"):
                sec = section_of(split_operands(rest)[0]) if d == ".section" else d[1:]
                section = sec if sec else "discard"
                if labels:
                    stmts.append(Stmt(i, raw, "dir", ".label", [], labels, src, section))
            elif d in (".comm", ".lcomm"):
                args = split_operands(rest)
                if d == ".comm" and args[0] not in locs:
                    globs.add(args[0])
                al = args[2] if len(args) > 2 else "2"
                stmts.append(Stmt(i, raw, "dir", ".balign", [al], [], src, "bss"))
                stmts.append(Stmt(i, raw, "dir", ".space", [args[1]], [args[0]], src, "bss"))
            elif d in IGNORED or d.startswith(".cfi_"):
                if labels:
                    stmts.append(Stmt(i, raw, "dir", ".label", [], labels, src, section))
            elif name.startswith("."):
                stmts.append(Stmt(i, raw, "dir", d, split_operands(rest), labels, src, section))
            else:
                stmts.append(Stmt(i, raw, "inst", name, split_operands(rest), labels, src, section))
        return [s for s in stmts if s.section != "discard"], globs

    @staticmethod
    def localize(stmts, globs, tag):
        """Rename the file-local symbols of one file to name$L<tag>."""
        local = set()
        for s in stmts:
            local.update(s.labels)
            if s.kind == "equ":
                local.add(s.name)
        local -= globs
        if not local:
            return {}
        ren = {n: f"{n}$L{tag}" for n in local}

        def fix(t):
            return IDENT.sub(lambda m: ren.get(m.group(1), m.group(1)), t)

        for s in stmts:
            s.labels = [ren.get(x, x) for x in s.labels]
            if s.kind == "equ":
                s.name = ren.get(s.name, s.name)
                s.args = [fix(a) for a in s.args]
            elif not (s.kind == "dir" and s.name in (".ascii", ".asciz", ".string")):
                s.args = [fix(a) for a in s.args]
        return ren

    def data_bytes(self, st, allow):
        d = st.name
        out = []
        if d in (".ascii", ".asciz", ".string"):
            for a in st.args:
                s = ast.literal_eval(a)
                out += list(s.encode("latin-1"))
                if d != ".ascii":
                    out.append(0)
            return out
        width = {".byte": 1, ".word": 2, ".short": 2, ".hword": 2, ".2byte": 2,
                 ".long": 4, ".int": 4, ".4byte": 4}[d]
        for a in st.args:
            if a.startswith('"'):
                out += list(ast.literal_eval(a).encode("latin-1"))
                continue
            x = self.val(a, allow)
            x = 0 if x is None else x
            out += [(x >> (8 * k)) & 0xFF for k in range(width)]
        return out

    def run_pass(self, stmts, final):
        self.here = self.base
        changed = False
        out = {}
        for st in stmts:
            for lab in st.labels:
                if self.syms.get(lab) != self.here:
                    changed = True
                self.syms[lab] = self.here
            st.addr = self.here
            bss = st.section == "bss"
            try:
                if st.kind == "equ":
                    v = self.val(st.args[0], not final)
                    if self.syms.get(st.name) != v:
                        changed = True
                    self.syms[st.name] = v
                elif st.kind == "dir":
                    d = st.name
                    if d == ".org":
                        self.here = self.eval(st.args[0], False)
                        st.addr = self.here
                    elif d in (".align", ".balign", ".p2align"):
                        n = self.eval(st.args[0], False)
                        if d == ".p2align":
                            n = 1 << n
                        pad = (-self.here) % max(n, 1)
                        if not bss:
                            out.update({self.here + k: 0 for k in range(pad)})
                        self.here += pad
                    elif d in (".space", ".zero", ".skip"):
                        n = self.eval(st.args[0], False)
                        fill = self.eval(st.args[1], False) & 0xFF if len(st.args) > 1 else 0
                        if not bss:
                            out.update({self.here + k: fill for k in range(n)})
                        self.here += n
                    elif d in (".byte", ".word", ".short", ".hword", ".2byte", ".long", ".int", ".4byte",
                               ".ascii", ".asciz", ".string"):
                        bs = self.data_bytes(st, not final)
                        if bss:
                            if final and any(bs):
                                raise AsmError("non-zero data in .bss")
                        else:
                            out.update({self.here + k: b for k, b in enumerate(bs)})
                        self.here += len(bs)
                    elif d == ".label":
                        pass
                    else:
                        raise AsmError(f"unknown directive {d}")
                else:
                    if bss:
                        raise AsmError("instruction in .bss")
                    changed |= self.inst(st, out, final)
            except (AsmError, IsaError) as e:
                raise AsmError(f"{st.src}:{st.lineno}: {e}\n    {st.text.strip()}")
        return changed, out

    def inst(self, st, out, final):
        pc = self.here
        mn, vals = self.lower(st, not final)
        parts, st.expand = self.alternatives(mn, vals)
        sizes = st.size if isinstance(st.size, list) else [0] * len(parts)
        changed = False
        forms = []
        for i, alts in enumerate(parts):
            if final:
                f = None
                errs = []
                for m, v in alts:
                    f, r = self.choose(m, v, pc, size=sizes[i], final=True)
                    if f is not None:
                        v = r
                        break
                    errs += r
                if f is None:
                    raise AsmError(f"no encoding of {alts[0][0]} fits: {'; '.join(errs) or 'no forms'}")
                word = f.encode(v, pc=pc)
                bs = []
                ps = [word] if f.len == 16 else [word >> 16, word & 0xFFFF]
                for p in ps:
                    bs += [p & 0xFF, p >> 8]
                out.update({pc + k: b for k, b in enumerate(bs)})
                forms.append(f)
            else:
                best, errs = None, []
                for m, v in alts:
                    f, r = self.choose(m, v, pc)
                    if f is None:
                        errs += r
                    elif best is None or f.nbytes < best:
                        best = f.nbytes
                if best is None:
                    raise AsmError(f"no encoding of {alts[0][0]} fits: {'; '.join(errs) or 'no forms for this mode'}")
                n = max(sizes[i], best)
                if n != sizes[i]:
                    changed = True
                sizes[i] = n
            pc += sizes[i]
        st.size = sizes
        st.form = forms
        self.here = pc
        return changed

    @staticmethod
    def preprocess(path, depth=0):
        """Inline .include "file" (relative to the including file). Returns (lineno, text) pairs."""
        if depth > 8:
            raise AsmError(".include nested too deeply")
        out = []
        for n, line in enumerate(open(path).read().splitlines(), 1):
            m = re.match(r'\s*\.include\s+"([^"]+)"', line)
            if m:
                out += Assembler.preprocess(os.path.join(os.path.dirname(path), m.group(1)), depth + 1)
            else:
                out.append((n, line))
        return out

    def scan(self, path):
        """(defined globals, referenced names) of one file, for library selection."""
        stmts, globs = self.parse(self.preprocess(path), os.path.basename(path))
        defined, refs = set(), set()
        for s in stmts:
            defined.update(s.labels)
            if s.kind == "equ":
                defined.add(s.name)
            if s.kind == "inst" or (s.kind == "dir" and s.name not in (".ascii", ".asciz", ".string")) \
                    or s.kind == "equ":
                for t in s.args:
                    refs.update(IDENT.findall(t))
        return globs & defined, refs - defined

    def select_libs(self, paths, libs):
        """Add the library files needed to resolve undefined symbols, transitively."""
        have, need = set(), set()
        for p in paths:
            d, r = self.scan(p)
            have |= d
            need |= r
        info = {p: self.scan(p) for p in libs}
        chosen = []
        while True:
            missing = need - have
            add = [p for p in libs if p not in chosen and info[p][0] & missing]
            if not add:
                break
            for p in add:
                chosen.append(p)
                have |= info[p][0]
                need |= info[p][1]
        return list(paths) + chosen

    def link(self, paths, base):
        """Parse every file, localize its symbols and order the result by section."""
        bysec = {s: [] for s in SECTIONS}
        allglobs = set()
        self.base = None
        self.renames = {}
        for k, p in enumerate(paths):
            stmts, globs = self.parse(self.preprocess(p), os.path.basename(p))
            if k == 0 and stmts and stmts[0].kind == "dir" and stmts[0].name == ".org" and not stmts[0].labels:
                self.base = self.eval(stmts[0].args[0], False)
            ren = self.localize(stmts, globs, k)
            self.renames.update({v: n for n, v in ren.items()})
            allglobs |= globs
            for s in stmts:
                bysec[s.section].append(s)
        if self.base is None:
            self.base = base
        marks = {"rodata": "__rodata_start", "data": "__data_start", "bss": "__bss_start"}
        out = []
        for sec in SECTIONS:
            if sec in marks:
                out.append(Stmt(0, "", "dir", ".balign", ["4"], [], "<link>", sec if sec != "bss" else "data"))
                out.append(Stmt(0, "", "dir", ".label", [], [marks[sec]], "<link>", sec))
            out += bysec[sec]
        out.append(Stmt(0, "", "dir", ".balign", ["4"], [], "<link>", "bss"))
        out.append(Stmt(0, "", "dir", ".label", [], ["__bss_end", "_end"], "<link>", "bss"))
        self.globals = allglobs
        return out

    def assemble(self, paths, base=0x1000, libs=()):
        if isinstance(paths, str):
            paths = [paths]
        if libs:
            paths = self.select_libs(paths, libs)
        self.inputs = paths
        stmts = self.link(paths, base)
        seen = {}
        for s in stmts:
            for lab in s.labels + ([s.name] if s.kind == "equ" else []):
                if lab in seen and s.kind != "equ":
                    raise AsmError(f"{s.src}:{s.lineno}: duplicate symbol {lab!r} (first in {seen[lab]})")
                seen[lab] = s.src
        for _ in range(40):
            changed, _ = self.run_pass(stmts, final=False)
            if not changed:
                break
        else:
            raise AsmError("sizes did not converge")
        _, out = self.run_pass(stmts, final=True)
        return stmts, self.base, out

    def display_name(self, sym):
        return self.renames.get(sym, sym)


def main(argv):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("src", nargs="+")
    ap.add_argument("-v", "--variant", default="A")
    ap.add_argument("-o", "--out")
    ap.add_argument("--lst")
    ap.add_argument("--stats")
    ap.add_argument("--sym")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x1000)
    ap.add_argument("--lib", nargs="+", default=[], help="library files, linked only if needed")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args(argv)
    isa = Isa()
    a = Assembler(isa, args.variant)
    try:
        stmts, base, mem = a.assemble(args.src, args.base, args.lib)
    except AsmError as e:
        sys.exit(f"{e}")
    end = a.syms.get("__bss_start", max(mem) + 1 if mem else base)
    top = max([base] + [x + 1 for x in mem if x < end])
    image = bytes(mem.get(x, 0) for x in range(base, top))
    out = args.out or os.path.splitext(args.src[0])[0] + f".{args.variant}.bin"
    with open(out, "wb") as fh:
        fh.write(image)
    insts = [s for s in stmts if s.kind == "inst"]
    by_form, by_op = {}, {}
    n16 = n32 = code = 0
    for s in insts:
        for f in s.form:
            by_form[f.name()] = by_form.get(f.name(), 0) + 1
            by_op[f.op] = by_op.get(f.op, 0) + 1
            code += f.nbytes
            if f.len == 16:
                n16 += 1
            else:
                n32 += 1
    entry = a.syms.get("start", a.syms.get("_start", base))
    stats = {"variant": args.variant, "base": base, "entry": entry, "image_bytes": len(image),
             "bss_bytes": a.syms.get("__bss_end", end) - end,
             "code_bytes": code, "insts": n16 + n32, "len16": n16, "len32": n32,
             "expanded": sum(1 for s in insts if s.expand), "ops": by_op, "forms": by_form}
    if args.stats:
        with open(args.stats, "w") as fh:
            json.dump(stats, fh, indent=1)
    if args.sym:
        with open(args.sym, "w") as fh:
            rows = sorted((v, a.display_name(k)) for k, v in a.syms.items()
                          if v is not None and not a.display_name(k).startswith(".L") and k != "$")
            for v, k in rows:
                fh.write(f"{v & 0xFFFFFF:06x} {k}\n")
    if args.lst:
        with open(args.lst, "w") as fh:
            cur = None
            for s in stmts:
                if s.src != cur and s.src != "<link>":
                    cur = s.src
                    fh.write(f"; ---- {cur}\n")
                if s.kind == "inst":
                    n = sum(s.size)
                    bs = " ".join(f"{mem.get(s.addr + k, 0):02x}" for k in range(n))
                    fh.write(f"{s.addr:06x}  {bs:<24} {s.text.rstrip()}\n")
                elif s.src != "<link>":
                    fh.write(f"{'' if s.addr is None else format(s.addr, '06x'):6}  {'':24} {s.text.rstrip()}\n")
    if not args.quiet:
        print(f"{' '.join(os.path.basename(x) for x in args.src)} [{args.variant}]: {n16 + n32} insts "
              f"({n16} x16, {n32} x32), code {code} bytes, image {len(image)} bytes @ {base:#x}, "
              f"bss {stats['bss_bytes']} bytes")


if __name__ == "__main__":
    main(sys.argv[1:])
