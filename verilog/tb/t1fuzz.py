#!/usr/bin/env python3
"""Random register and memory programs, run on T1 in lockstep with fsim.

Each program loads random values into the registers, fills a 512-byte buffer
with an LCG sequence, runs a few hundred random items, reads the whole
buffer back and exits.  An item is one ALU/shift/multiply/pair instruction, a
short forward branch over a few more (or over a 300-byte gap, which forces
the 32-bit branch form), or a memory template: load a base pointer (p4,
or r8/r9 as data to alias it), then any load/store/LEA in any addressing
mode and size.  With --ext-x the extension X instructions are included.
Registers and flags are compared after every instruction, so a wrong store
shows up in the read-back.

  t1fuzz.py [-n PROGRAMS] [--seed S] [--items N] [--ext-x 0|1] [-j JOBS] [--keep]
"""
import argparse, concurrent.futures, os, random, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import t1run  # noqa: E402

ASM = os.path.join(ROOT, "sim", "tools", "asm.py")
OUT = os.path.join(ROOT, "verilog", "build", "fuzz")
BUF = 0x8000

R = [0, 1, 2, 3, 4, 5, 6, 7, 12, 13]            # registers the random ALU items use
P = [0, 1, 2, 3, 6]                              # pairs over those registers
CCS = ["EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE"]


class Gen:
    def __init__(self, rnd, ext):
        self.r = rnd
        self.ext = ext
        self.n = 0

    def reg(self):
        return f"r{self.r.choice(R)}"

    def pair(self):
        return f"p{self.r.choice(P)}"

    def imm16(self):
        r = self.r
        return r.choice([0, 1, -1, 2, 255, 256, 0x7FFF, -0x8000, 0xFFFF, r.randint(-128, 127),
                         r.randint(-32768, 65535), r.randint(0, 15)])

    def alu(self):
        r = self.r
        k = r.randrange(24 if self.ext else 20)
        d, s, t = self.reg(), self.reg(), self.reg()
        if r.random() < 0.3:
            s = d
        if k < 6:
            return f"{r.choice(['ADD', 'ADC', 'SUB', 'SBC', 'AND', 'OR', 'XOR'])} {d}, {s}, {t}"
        if k < 8:
            return f"{r.choice(['SHL', 'SHR', 'SAR', 'MUL'])} {d}, {s}, {t}"
        if k == 8:
            return f"{r.choice(['MOV', 'NEG', 'NOT'])} {d}, {s}"
        if k == 9:
            return f"{r.choice(['CMP', 'CPC'])} {s}, {t}"
        if k == 10:
            return f"{r.choice(['ADDI', 'ANDI', 'ORI', 'XORI'])} {d}, {s}, {self.imm16()}"
        if k == 11:
            return f"{r.choice(['SHLI', 'SHRI', 'SARI'])} {d}, {s}, {r.randint(0, 15)}"
        if k == 12:
            return f"MOVI {d}, {self.imm16()}"
        if k == 13:
            return f"CMPI {s}, {self.imm16()}"
        if k == 14:
            return f"{r.choice(['MULH', 'MULHU'])} {d}, {s}, {t}"
        if k == 15:
            return f"{r.choice(['MOVP'])} {self.pair()}, {self.pair()}"
        if k == 16:
            return f"{r.choice(['ADDP', 'ADDPU'])} {self.pair()}, {self.pair()}, {t}"
        if k == 17:
            return f"ADDPP {self.pair()}, {self.pair()}, {self.pair()}"
        if k == 18:
            return f"ADDPI {self.pair()}, {self.pair()}, {r.choice([r.randint(-256, 255), r.randint(-32768, 32767), 0, 4])}"
        if k == 19:
            return r.choice([f"MFSR {d}, flags", f"MTSR flags, {s}", f"LEA {self.pair()}, [{self.pair()} + {r.randint(-300, 300)}]"])
        if k == 20:
            return f"MULPU {self.pair()}, {s}, {t}"
        if k == 21:
            return f"{r.choice(['FSHR', 'FSHL'])} {d}, {self.pair()}, {t}"
        if k == 22:
            return f"CLZ {d}, {s}"
        return f"DSTEP {self.pair()}, {self.pair()}"

    def label(self):
        self.n += 1
        return f"L{self.n}"

    def branch(self):
        r = self.r
        lab = self.label()
        body = [self.alu() for _ in range(r.randint(1, 4))]
        cc = r.choice(CCS + ["AL"])
        far = ["        .space 300"] if r.random() < 0.15 else []
        return [f"        B{cc} {lab}"] + [f"        {x}" for x in body] + far + [f"{lab}:"]

    def mem(self):
        r = self.r
        out = []
        base = BUF + 256 + r.randrange(-48, 49) * 2 + (1 if r.random() < 0.1 else 0)
        mode = r.randrange(7)
        sz = r.choice(["B", "W", "P"])
        odd = base & 1
        if odd:
            sz = "B"
        load = r.random() < 0.5
        alias = r.random() < 0.2 and not odd
        dreg = r.choice(["r8", "r9"]) if (alias and sz != "P") else f"r{r.choice(R)}"
        dpair = "p4" if alias else f"p{r.choice(P)}"
        if sz == "B":
            op = (r.choice(["LDBU", "LDBS"]) if load else "STB")
            d = dreg
        elif sz == "W":
            op, d = ("LDW" if load else "STW"), dreg
        else:
            op, d = ("LDP" if load else "STP"), dpair
        step = {"B": 1, "W": 2, "P": 4}[sz]
        out.append(f"        LA p4, {base}")
        if mode == 0:                                  # [p + disp]
            disp = r.randint(-40, 40) * (1 if sz == "B" else 2)
            if r.random() < 0.4 and sz != "B":
                disp = r.randint(0, 15) * 2
            out.append(f"        {op} {d}, [p4 + {disp}]")
        elif mode == 1:                                # [p + r*s]
            sc = r.choice([1, 2, 4] if sz == "B" else [2, 4])
            z = r.random() < 0.3
            idx = r.randint(0 if z else -12, 12) * (1 if sz == "B" else 1)
            out.append(f"        MOVI r10, {idx}")
            if sz != "B" and sc == 1:
                sc = 2
            out.append(f"        {op} {d}, [p4 + r10{'.z' if z else ''}*{sc}]")
        elif mode in (2, 3):                           # [p]+  /  -[p]
            fm = "[p4]+" if mode == 2 else "-[p4]"
            if mode == 3:
                out[-1] = f"        LA p4, {base + step}"
            out.append(f"        {op} {d}, {fm}")
        elif mode == 4:                                # sp-relative word
            out.append(f"        LA sp, {base & ~1}")
            if sz == "W":
                out.append(f"        {'LDW' if load else 'STW'} {dreg}, [sp + {r.randint(0, 30) * 2}]")
            else:
                out.append(f"        {op} {d}, [sp + {r.randint(-10, 10) * 2}]")
        elif mode == 5:                                # LEA
            if r.random() < 0.5:
                out.append(f"        LEA {r.choice(['p1', 'p2', 'p3'])}, [p4 + {r.randint(-1000, 1000)}]")
            else:
                out.append(f"        MOVI r10, {r.randint(-40, 40)}")
                out.append(f"        LEA {r.choice(['p1', 'p2', 'p3'])}, [p4 + r10*{r.choice([1, 2, 4])}]")
        else:                                          # read back what a store wrote
            out.append(f"        {op} {d}, [p4 + {2 * r.randint(-4, 4)}]")
            out.append(f"        {'LDW' if r.random() < 0.5 and not odd else 'LDBU'} r{r.choice(R)}, [p4 + {2 * r.randint(-4, 4)}]")
        return [x for x in out]

    def program(self, items):
        r = self.r
        lines = ["        .org 0x1000", "start:  LA sp, 0x0F0000"]
        for k in R:
            lines.append(f"        LI r{k}, {r.randint(0, 65535)}")
        lines += [f"        LA p4, {BUF}", "        LI r10, 256", "        LI r6, 4321",
                  "fill:   LI r7, 25173", "        MUL r6, r6, r7", "        ADDI r6, r6, 13849",
                  "        STW r6, [p4]+", "        DEC r10", "        BNE fill"]
        for k in R:
            lines.append(f"        LI r{k}, {r.randint(0, 65535)}")
        for _ in range(items):
            x = r.random()
            if x < 0.45:
                lines.append("        " + self.alu())
            elif x < 0.58:
                lines += self.branch()
            else:
                lines += ["        " + y.strip() for y in self.mem()]
        lines += [f"        LA p4, {BUF}", "        LI r10, 256", "        LI r7, 0",
                  "sweep:  LDW r6, [p4]+", "        ADD r7, r7, r6", "        DEC r10", "        BNE sweep",
                  "        MOVI r2, 0", "        LA p0, 0xF00002", "        STB r2, [p0]", "done:   BRA done"]
        return "\n".join(lines) + "\n"


def one(args):
    seed, items, ext_x = args
    rnd = random.Random(seed)
    src = os.path.join(OUT, f"fuzz{seed}.s")
    img = os.path.join(OUT, f"fuzz{seed}.bin")
    open(src, "w").write(Gen(rnd, ext_x).program(items))
    a = subprocess.run([sys.executable, ASM, "-q", "-v", "A", src, "-o", img], capture_output=True, text=True)
    if a.returncode:
        return seed, False, "assembler: " + (a.stdout + a.stderr).strip()[-300:]
    res = t1run.run(img, True, 20_000_000, ext_x)
    want, rc = t1run.fsim_run(img, 200_000_000, ext_x)
    ok = res["output"] == want and t1run.expected_rc(res) == rc
    msg = f"T1 {res['result']} fsim exit {rc}"
    if not ok:
        msg += "\n" + res["output"].strip()
    return seed, ok, msg


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-n", type=int, default=20)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--items", type=int, default=250)
    ap.add_argument("--ext-x", type=int, default=1)
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--keep", action="store_true", help="keep passing programs too")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    t1run.build(a.ext_x)
    fails = 0
    with concurrent.futures.ThreadPoolExecutor(a.j) as ex:
        for seed, ok, msg in ex.map(one, [(a.seed + i, a.items, a.ext_x) for i in range(a.n)]):
            if not ok:
                fails += 1
                print(f"FAIL seed {seed}: {msg}   (program: {OUT}/fuzz{seed}.s)")
            elif not a.keep:
                for ext in (".s", ".bin"):
                    os.unlink(os.path.join(OUT, f"fuzz{seed}{ext}"))
    print(f"{a.n - fails}/{a.n} programs matched fsim in lockstep")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
