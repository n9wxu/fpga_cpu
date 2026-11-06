"""6502 opcode table (NMOS, documented plus the stable undocumented set) and a
small two-pass assembler.

The table is the single source for the Python reference (ref6502.py), the
assembler and the generated C header (gen_optable.py -> src/optable.h).
"""
import re

# Addressing modes
IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABX, ABY, IND, IZX, IZY, REL = range(13)
MODE_NAMES = ["imp", "acc", "imm", "zp", "zpx", "zpy", "abs", "abx", "aby", "ind", "izx", "izy", "rel"]
MODE_LEN = [1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2]

# One row of the matrix per high nibble: 16 entries "MNEM mode cycles[*]".
# '*' = +1 cycle when the indexed address crosses a page. "-" = illegal / unstable (trapped).
_T = """
BRK imp 7|ORA izx 6|-|SLO izx 8|NOP zp 3|ORA zp 3|ASL zp 5|SLO zp 5|PHP imp 3|ORA imm 2|ASL acc 2|ANC imm 2|NOP abs 4|ORA abs 4|ASL abs 6|SLO abs 6
BPL rel 2|ORA izy 5*|-|SLO izy 8|NOP zpx 4|ORA zpx 4|ASL zpx 6|SLO zpx 6|CLC imp 2|ORA aby 4*|NOP imp 2|SLO aby 7|NOP abx 4*|ORA abx 4*|ASL abx 7|SLO abx 7
JSR abs 6|AND izx 6|-|RLA izx 8|BIT zp 3|AND zp 3|ROL zp 5|RLA zp 5|PLP imp 4|AND imm 2|ROL acc 2|ANC imm 2|BIT abs 4|AND abs 4|ROL abs 6|RLA abs 6
BMI rel 2|AND izy 5*|-|RLA izy 8|NOP zpx 4|AND zpx 4|ROL zpx 6|RLA zpx 6|SEC imp 2|AND aby 4*|NOP imp 2|RLA aby 7|NOP abx 4*|AND abx 4*|ROL abx 7|RLA abx 7
RTI imp 6|EOR izx 6|-|SRE izx 8|NOP zp 3|EOR zp 3|LSR zp 5|SRE zp 5|PHA imp 3|EOR imm 2|LSR acc 2|ALR imm 2|JMP abs 3|EOR abs 4|LSR abs 6|SRE abs 6
BVC rel 2|EOR izy 5*|-|SRE izy 8|NOP zpx 4|EOR zpx 4|LSR zpx 6|SRE zpx 6|CLI imp 2|EOR aby 4*|NOP imp 2|SRE aby 7|NOP abx 4*|EOR abx 4*|LSR abx 7|SRE abx 7
RTS imp 6|ADC izx 6|-|RRA izx 8|NOP zp 3|ADC zp 3|ROR zp 5|RRA zp 5|PLA imp 4|ADC imm 2|ROR acc 2|ARR imm 2|JMP ind 5|ADC abs 4|ROR abs 6|RRA abs 6
BVS rel 2|ADC izy 5*|-|RRA izy 8|NOP zpx 4|ADC zpx 4|ROR zpx 6|RRA zpx 6|SEI imp 2|ADC aby 4*|NOP imp 2|RRA aby 7|NOP abx 4*|ADC abx 4*|ROR abx 7|RRA abx 7
NOP imm 2|STA izx 6|NOP imm 2|SAX izx 6|STY zp 3|STA zp 3|STX zp 3|SAX zp 3|DEY imp 2|NOP imm 2|TXA imp 2|-|STY abs 4|STA abs 4|STX abs 4|SAX abs 4
BCC rel 2|STA izy 6|-|-|STY zpx 4|STA zpx 4|STX zpy 4|SAX zpy 4|TYA imp 2|STA aby 5|TXS imp 2|-|-|STA abx 5|-|-
LDY imm 2|LDA izx 6|LDX imm 2|LAX izx 6|LDY zp 3|LDA zp 3|LDX zp 3|LAX zp 3|TAY imp 2|LDA imm 2|TAX imp 2|-|LDY abs 4|LDA abs 4|LDX abs 4|LAX abs 4
BCS rel 2|LDA izy 5*|-|LAX izy 5*|LDY zpx 4|LDA zpx 4|LDX zpy 4|LAX zpy 4|CLV imp 2|LDA aby 4*|TSX imp 2|-|LDY abx 4*|LDA abx 4*|LDX aby 4*|LAX aby 4*
CPY imm 2|CMP izx 6|NOP imm 2|DCP izx 8|CPY zp 3|CMP zp 3|DEC zp 5|DCP zp 5|INY imp 2|CMP imm 2|DEX imp 2|SBX imm 2|CPY abs 4|CMP abs 4|DEC abs 6|DCP abs 6
BNE rel 2|CMP izy 5*|-|DCP izy 8|NOP zpx 4|CMP zpx 4|DEC zpx 6|DCP zpx 6|CLD imp 2|CMP aby 4*|NOP imp 2|DCP aby 7|NOP abx 4*|CMP abx 4*|DEC abx 7|DCP abx 7
CPX imm 2|SBC izx 6|NOP imm 2|ISC izx 8|CPX zp 3|SBC zp 3|INC zp 5|ISC zp 5|INX imp 2|SBC imm 2|NOP imp 2|SBC imm 2|CPX abs 4|SBC abs 4|INC abs 6|ISC abs 6
BEQ rel 2|SBC izy 5*|-|ISC izy 8|NOP zpx 4|SBC zpx 4|INC zpx 6|ISC zpx 6|SED imp 2|SBC aby 4*|NOP imp 2|ISC aby 7|NOP abx 4*|SBC abx 4*|INC abx 7|ISC abx 7
"""

DOC = set("""ADC AND ASL BCC BCS BEQ BIT BMI BNE BPL BRK BVC BVS CLC CLD CLI CLV CMP CPX CPY DEC DEX DEY EOR INC INX INY
JMP JSR LDA LDX LDY LSR NOP ORA PHA PHP PLA PLP ROL ROR RTI RTS SBC SEC SED SEI STA STX STY TAX TAY TSX TXA TXS TYA""".split())

# opcode -> (mnemonic, mode, base cycles, page-cross penalty) or None
OPS = [None] * 256
for hi, line in enumerate(_T.strip().split("\n")):
    for lo, ent in enumerate(line.split("|")):
        if ent == "-":
            continue
        m, mode, cyc = ent.split()
        pen = cyc.endswith("*")
        OPS[hi * 16 + lo] = (m, MODE_NAMES.index(mode), int(cyc.rstrip("*")), pen)

# flag masks as in P: N V - B D I Z C
FN, FV, FB, FD, FI, FZ, FC = 0x80, 0x40, 0x10, 0x08, 0x04, 0x02, 0x01

# mnemonic -> (flags read, flags written) used by the JIT's flag liveness
NZ = FN | FZ
FLAGS = {
    "ADC": (FC | FD, NZ | FC | FV), "SBC": (FC | FD, NZ | FC | FV), "AND": (0, NZ), "ORA": (0, NZ), "EOR": (0, NZ),
    "ASL": (0, NZ | FC), "LSR": (0, NZ | FC), "ROL": (FC, NZ | FC), "ROR": (FC, NZ | FC),
    "BIT": (0, NZ | FV), "CMP": (0, NZ | FC), "CPX": (0, NZ | FC), "CPY": (0, NZ | FC),
    "DEC": (0, NZ), "INC": (0, NZ), "DEX": (0, NZ), "DEY": (0, NZ), "INX": (0, NZ), "INY": (0, NZ),
    "LDA": (0, NZ), "LDX": (0, NZ), "LDY": (0, NZ), "TAX": (0, NZ), "TAY": (0, NZ), "TXA": (0, NZ),
    "TYA": (0, NZ), "TSX": (0, NZ), "PLA": (0, NZ), "PLP": (0, 0xFF),
    "CLC": (0, FC), "SEC": (0, FC), "CLV": (0, FV), "CLD": (0, FD), "SED": (0, FD), "CLI": (0, FI), "SEI": (0, FI),
    "PHP": (0xFF, 0), "BRK": (0xFF, FI), "RTI": (0, 0xFF),
    "BPL": (FN, 0), "BMI": (FN, 0), "BVC": (FV, 0), "BVS": (FV, 0), "BCC": (FC, 0), "BCS": (FC, 0),
    "BNE": (FZ, 0), "BEQ": (FZ, 0),
    "SLO": (0, NZ | FC), "RLA": (FC, NZ | FC), "SRE": (0, NZ | FC), "RRA": (FC | FD, NZ | FC | FV),
    "LAX": (0, NZ), "DCP": (0, NZ | FC), "ISC": (FC | FD, NZ | FC | FV),
    "ANC": (0, NZ | FC), "ALR": (0, NZ | FC), "ARR": (FC | FD, NZ | FC | FV), "SBX": (0, NZ | FC),
}

MNEMS = sorted({o[0] for o in OPS if o})
BY_MNEM = {}
for op, o in enumerate(OPS):
    if o:
        BY_MNEM.setdefault(o[0], {}).setdefault(o[1], op)
# NOP imm: 0x80 etc; keep the documented NOP (EA) as implied
BY_MNEM["NOP"][IMP] = 0xEA
BY_MNEM["SBC"][IMM] = 0xE9


class AsmError(Exception):
    pass


def _num(tok, syms, pc):
    tok = tok.strip()
    if tok[:1] in "<>":
        v = _num(tok[1:], syms, pc)
        return (v & 0xFF) if tok[0] == "<" else (v >> 8) & 0xFF
    e = tok
    e = re.sub(r"\$([0-9A-Fa-f]+)", r"0x\1", e)
    e = re.sub(r"%([01]+)", r"0b\1", e)
    e = re.sub(r"(?<![\w])\*(?![\w(])", str(pc), e) if e.strip() == "*" else e
    env = {k: v for k, v in syms.items()}
    env["lo"] = lambda v: v & 0xFF
    env["hi"] = lambda v: (v >> 8) & 0xFF
    try:
        return int(eval(e, {"__builtins__": {}}, env))
    except NameError:
        raise KeyError(tok)


def _is_zp(t, v):
    return 0 <= v < 256 and not (t.startswith("$") and len(t) > 3)


def assemble(src, org=0x0200):
    """Assemble text; returns (bytes dict addr->byte, symbols). Two passes."""
    lines = []
    for ln in src.split("\n"):
        ln = ln.split(";")[0].rstrip()
        if ln.strip():
            lines.append(ln)
    syms = {}
    out = {}
    for pas in (0, 1, 2):
        pc = org
        out = {}
        for ln in lines:
            s = ln.strip()
            m = re.match(r"^([A-Za-z_][\w]*):\s*(.*)$", s)
            if m:
                syms[m.group(1)] = pc
                s = m.group(2).strip()
                if not s:
                    continue
            m = re.match(r"^([A-Za-z_]\w*)\s*=\s*(.+)$", s)
            if m and not s.startswith("."):
                try:
                    syms[m.group(1)] = _num(m.group(2), syms, pc)
                except KeyError:
                    if pas:
                        raise
                continue
            parts = s.split(None, 1)
            op = parts[0].upper() if not parts[0].startswith(".") else parts[0].lower()
            arg = parts[1].strip() if len(parts) > 1 else ""

            def val(t):
                try:
                    return _num(t, syms, pc)
                except KeyError:
                    if pas == 2:
                        raise AsmError("undefined symbol in: " + ln)
                    return 0
            if op == ".org":
                pc = val(arg)
                continue
            if op in (".byte", ".db"):
                for t in arg.split(","):
                    out[pc] = val(t) & 0xFF
                    pc += 1
                continue
            if op in (".word", ".dw"):
                for t in arg.split(","):
                    v = val(t)
                    out[pc] = v & 0xFF
                    out[pc + 1] = (v >> 8) & 0xFF
                    pc += 2
                continue
            if op == ".fill":
                n, v = (arg.split(",") + ["0"])[:2]
                for _ in range(val(n)):
                    out[pc] = val(v) & 0xFF
                    pc += 1
                continue
            if op.startswith("."):
                raise AsmError("unknown directive " + op)
            if op not in BY_MNEM:
                raise AsmError("unknown mnemonic: " + ln)
            tab = BY_MNEM[op]
            a = arg
            mode = None
            v = 0
            if a == "":
                mode = IMP if IMP in tab else ACC
            elif a.upper() == "A" and ACC in tab:
                mode = ACC
            elif a.startswith("#"):
                mode, v = IMM, val(a[1:].lstrip("<>")) if a[1] not in "<>" else None
                if v is None:
                    x = val(a[2:])
                    v = (x & 0xFF) if a[1] == "<" else (x >> 8) & 0xFF
            elif re.match(r"^\(.*,\s*[xX]\)$", a):
                mode, v = IZX, val(a[1:a.rindex(",")])
            elif re.match(r"^\(.*\)\s*,\s*[yY]$", a):
                mode, v = IZY, val(a[1:a.rindex(")")])
            elif a.startswith("(") and a.endswith(")"):
                mode, v = IND, val(a[1:-1])
            elif re.match(r".*,\s*[xX]$", a):
                t = a[:a.rindex(",")].strip()
                v = val(t)
                mode = ZPX if (_is_zp(t, v) and ZPX in tab) else ABX
            elif re.match(r".*,\s*[yY]$", a):
                t = a[:a.rindex(",")].strip()
                v = val(t)
                mode = ZPY if (_is_zp(t, v) and ZPY in tab) else ABY
            else:
                v = val(a)
                if REL in tab:
                    mode = REL
                elif _is_zp(a, v) and ZP in tab:
                    mode = ZP
                else:
                    mode = ABS
            if mode not in tab:
                # fall back to absolute forms if zp form missing
                alt = {ZP: ABS, ZPX: ABX, ZPY: ABY}.get(mode)
                if alt in tab:
                    mode = alt
                else:
                    raise AsmError("bad mode for: " + ln)
            opc = tab[mode]
            out[pc] = opc
            if mode == REL:
                off = v - (pc + 2)
                if pas == 2 and not -128 <= off <= 127:
                    raise AsmError("branch out of range: " + ln)
                out[pc + 1] = off & 0xFF
            elif MODE_LEN[mode] == 2:
                out[pc + 1] = v & 0xFF
            elif MODE_LEN[mode] == 3:
                out[pc + 1] = v & 0xFF
                out[pc + 2] = (v >> 8) & 0xFF
            pc += MODE_LEN[mode]
    return out, syms
