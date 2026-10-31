#!/usr/bin/env python3
"""Measured cycles per instruction on T1, one instruction at a time.

Each instruction is repeated 100 times in straight-line code (so every one
is fetched from memory and follows the same instruction) after a short
setup, and the cycles of that program minus the same program without the
repeats are divided by 100.  Reads instructions that need EXT_X only when
the core has it.

  t1cpi.py [--ext-x 0|1]              prints a markdown table
"""
import argparse, concurrent.futures, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import t1run  # noqa: E402

ASM = os.path.join(ROOT, "sim", "tools", "asm.py")
OUT = os.path.join(ROOT, "verilog", "build", "cpi")
N = 100

SETUP = """        LA sp, 0x0F0000
        LA p4, 0x8100
        LI r2, 0x1234
        LI r3, 0x0003
        LI r6, 4
        LI r7, 0x2468
        LI r12, 0x7FFF
        LI r13, 0x00FF
"""

# (group, label, text, extension X?); {i} is the repeat index for labels
ITEMS = [
    ("ALU", "ADD rd, rs (16-bit)", "ADD r2, r3", 0),
    ("ALU", "ADD rd, rs, rt (32-bit)", "ADD r2, r6, r7", 0),
    ("ALU", "MOV rd, rs", "MOV r2, r3", 0),
    ("ALU", "MOVI rd, imm8", "MOVI r2, 5", 0),
    ("ALU", "MOVI rd, imm16", "MOVI r2, 0x1234", 0),
    ("ALU", "ADDI rd, imm8", "ADDI r2, r2, 5", 0),
    ("ALU", "CMP rs, rt", "CMP r2, r3", 0),
    ("ALU", "AND rd, rs, rt (32-bit)", "AND r2, r6, r7", 0),
    ("Shift", "SHLI rd, 3", "SHLI r2, r2, 3", 0),
    ("Shift", "SHRI rd, 12", "SHRI r2, r2, 12", 0),
    ("Shift", "SAR rd, rt (count 3)", "SAR r2, r2, r3", 0),
    ("Multiply", "MUL, multiplier 8 bits (r3 = 3)", "MUL r2, r2, r3", 0),
    ("Multiply", "MUL, 16-bit operands", "MUL r2, r7, r12", 0),
    ("Multiply", "MULHU 16 x 16", "MULHU r2, r7, r12", 0),
    ("Multiply", "MULH 16 x 16", "MULH r2, r7, r12", 0),
    ("Pointer", "MOVP pd, ps", "MOVP p2, p3", 0),
    ("Pointer", "ADDPI pd, imm9 (small)", "ADDPI p2, p2, 4", 0),
    ("Pointer", "ADDPI pd, imm16", "ADDPI p2, p3, 1000", 0),
    ("Pointer", "ADDP pd, ps, rx", "ADDP p2, p2, r6", 0),
    ("Pointer", "LEA pd, [pn + disp16]", "LEA p2, [p4 + 100]", 0),
    ("Memory", "LDW rd, [pn + disp]", "LDW r2, [p4 + 4]", 0),
    ("Memory", "STW rd, [pn + disp]", "STW r2, [p4 + 4]", 0),
    ("Memory", "LDBU rd, [pn + disp]", "LDBU r2, [p4 + 3]", 0),
    ("Memory", "STB rd, [pn + disp]", "STB r2, [p4 + 3]", 0),
    ("Memory", "LDP pd, [pn + disp]", "LDP p2, [p4 + 8]", 0),
    ("Memory", "STP pd, [pn + disp]", "STP p2, [p4 + 8]", 0),
    ("Memory", "LDW rd, [sp + disp]", "LDW r2, [sp + 6]", 0),
    ("Memory", "LDW rd, [pn]+", "LDW r2, [p4]+", 0),
    ("Memory", "STW rd, -[pn]", "STW r2, -[p4]", 0),
    ("Memory", "LDW rd, [pn + rx*2]", "LDW r2, [p4 + r6*2]", 0),
    ("Memory", "LDW rd, [pn + disp16] (large)", "LDW r2, [p4 + 1000]", 0),
    ("Branch", "Bcc not taken", "BNE next{i}\nnext{i}:", 0),
    ("Branch", "Bcc taken (forward)", "BEQ next{i}\nnext{i}:", 0),
    ("Misc", "MFSR rd, flags", "MFSR r2, flags", 0),
    ("Misc", "NOP", "NOP", 0),
    ("Extension X", "MULPU pd, rs, rt (8-bit multiplier)", "MULPU p2, r7, r3", 1),
    ("Extension X", "MULPU pd, rs, rt (16-bit)", "MULPU p2, r7, r12", 1),
    ("Extension X", "CLZ rd, rs (7 leading zeros)", "CLZ r2, r13", 1),
    ("Extension X", "FSHR rd, ps, rt", "FSHR r2, p3, r3", 1),
    ("Extension X", "FSHL rd, ps, rt", "FSHL r2, p3, r3", 1),
    ("Extension X", "DSTEP pd, ps", "DSTEP p2, p3", 1),
]

EXIT = "        MOVI r2, 0\n        LA p0, 0xF00002\n        STB r2, [p0]\nhalt:   BRA halt\n"


def cycles(name, text, reps, ext_x):
    os.makedirs(OUT, exist_ok=True)
    src = os.path.join(OUT, name + ".s")
    body = "".join("        " + text.format(i=i).replace("\n", "\n        ") + "\n" for i in range(reps))
    if text.startswith("BEQ"):
        body = "        CMP r2, r2\n" + body
    elif text.startswith("BNE"):
        body = "        CMP r2, r2\n" + body
    open(src, "w").write("        .org 0x1000\nstart:\n" + SETUP + body + EXIT)
    img = os.path.join(OUT, name + ".bin")
    a = subprocess.run([sys.executable, ASM, "-q", "-v", "A", src, "-o", img], capture_output=True, text=True)
    if a.returncode:
        sys.exit(a.stdout + a.stderr)
    res = t1run.run(img, False, 10_000_000, ext_x)
    return res["cycles"]


def one(args):
    k, (group, label, text, needs_x), ext_x = args
    base = cycles(f"b{k}", text, 0, ext_x)
    full = cycles(f"i{k}", text, N, ext_x)
    return group, label, (full - base) / N


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ext-x", type=int, default=1)
    a = ap.parse_args()
    t1run.build(a.ext_x)
    todo = [(k, it, a.ext_x) for k, it in enumerate(ITEMS) if a.ext_x or not it[3]]
    print("| Group | Instruction | Cycles |")
    print("|---|---|---|")
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 2) as ex:
        for group, label, c in ex.map(one, todo):
            print(f"| {group} | `{label}` | {c:.2f} |")


if __name__ == "__main__":
    main()
