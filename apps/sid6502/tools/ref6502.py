"""Reference 6502 (NMOS) in Python: documented opcodes, the stable undocumented
set, decimal mode, exact cycle counts, SID write log.  Written independently
of the C interpreter and JIT; the tests compare all three.

Conventions shared with the C engines (see ../README.md):
  - SID writes: any store to $D400-$D7FF is logged as (cycle, addr & 31, value)
    where cycle = cycles before the instruction + the instruction's cycle count - 1.
  - Reads of $D011/$D012 return a raster position derived from the cycle count
    (63 cycles a line, 312 lines); other I/O reads return what was stored.
  - N=1 and Z=1 together cannot be represented by the JIT (PLP/RTI of such a value
    is the one place the engines may differ); tests avoid it.
"""
from m6502 import *   # noqa

STOP = 0xFFF0
KIL = 2


class Trap(Exception):
    pass


class CPU:
    def __init__(self, mem=None):
        self.m = bytearray(mem if mem else 65536)
        self.a = self.x = self.y = 0
        self.s = 0xFF
        self.n = self.v = self.d = self.i = self.z = self.c = 0
        self.pc = 0
        self.cycles = 0
        self.sid = []
        self.status = 0   # 0 stopped at STOP, 1 cycle limit, 2 illegal opcode

    # ---- state helpers
    def getp(self, b=0):
        return (self.n << 7 | self.v << 6 | 0x20 | b << 4 | self.d << 3 | self.i << 2 | self.z << 1 | self.c)

    def setp(self, p):
        self.n = p >> 7 & 1
        self.v = p >> 6 & 1
        self.d = p >> 3 & 1
        self.i = p >> 2 & 1
        self.z = p >> 1 & 1
        self.c = p & 1

    # ---- memory
    def rd(self, a, t):
        a &= 0xFFFF
        if 0xD000 <= a < 0xE000:
            line = (t // 63) % 312
            if a == 0xD012:
                return line & 0xFF
            if a == 0xD011:
                return (self.m[a] & 0x7F) | ((line >> 8) << 7)
        return self.m[a]

    def wr(self, a, v, t):
        a &= 0xFFFF
        self.m[a] = v & 0xFF
        if 0xD400 <= a < 0xD800:
            self.sid.append((t, a & 31, v & 0xFF))

    def push(self, v):
        self.m[0x100 + self.s] = v & 0xFF
        self.s = (self.s - 1) & 0xFF

    def pull(self):
        self.s = (self.s + 1) & 0xFF
        return self.m[0x100 + self.s]

    def nz(self, v):
        v &= 0xFF
        self.n = v >> 7
        self.z = int(v == 0)
        return v

    # ---- ALU
    def adc(self, m):
        a = self.a
        if self.d:
            al = (a & 15) + (m & 15) + self.c
            if al >= 10:
                al = ((al + 6) & 15) + 16
            r = (a & 0xF0) + (m & 0xF0) + al
            bin_ = (a + m + self.c) & 0xFF
            self.z = int(bin_ == 0)
            self.n = (r >> 7) & 1
            self.v = int((~(a ^ m) & (a ^ r) & 0x80) != 0)
            if r >= 0xA0:
                r += 0x60
            self.c = int(r >= 0x100)
            self.a = r & 0xFF
        else:
            r = a + m + self.c
            self.v = int((~(a ^ m) & (a ^ r) & 0x80) != 0)
            self.c = r >> 8
            self.a = self.nz(r)

    def sbc(self, m):
        a = self.a
        r = a - m - (1 - self.c)
        if self.d:
            al = (a & 15) - (m & 15) + self.c - 1
            if al < 0:
                al = ((al - 6) & 15) - 16
            rr = (a & 0xF0) - (m & 0xF0) + al
            if rr < 0:
                rr -= 0x60
            self.v = int(((a ^ m) & (a ^ r) & 0x80) != 0)
            self.c = int(r >= 0)
            self.nz(r)
            self.a = rr & 0xFF
        else:
            self.v = int(((a ^ m) & (a ^ r) & 0x80) != 0)
            self.c = int(r >= 0)
            self.a = self.nz(r)

    def cmp(self, r, m):
        t = r - m
        self.c = int(t >= 0)
        self.nz(t)

    def asl(self, v):
        self.c = v >> 7
        return self.nz(v << 1)

    def lsr(self, v):
        self.c = v & 1
        return self.nz(v >> 1)

    def rol(self, v):
        c = self.c
        self.c = v >> 7
        return self.nz(v << 1 | c)

    def ror(self, v):
        c = self.c
        self.c = v & 1
        return self.nz(v >> 1 | c << 7)

    # ---- execution
    def run(self, max_cycles=1 << 40, stop=STOP):
        m = self.m
        while True:
            if self.pc == stop:
                self.status = 0
                return
            if self.cycles >= max_cycles:
                self.status = 1
                return
            if not self.step():
                self.status = 2
                return

    def step(self):
        m = self.m
        pc = self.pc
        opc = m[pc]
        ent = OPS[opc]
        if ent is None:
            return False
        mn, mode, cyc, pen = ent
        if mn == "ARR" and self.d:
            return False
        b1 = m[(pc + 1) & 0xFFFF]
        b2 = m[(pc + 2) & 0xFFFF]
        ln = MODE_LEN[mode]
        npc = (pc + ln) & 0xFFFF
        t0 = self.cycles
        tr = t0 + cyc - 1       # cycle of the final (read or write) access
        ea = None
        cross = 0
        if mode == ZP:
            ea = b1
        elif mode == ZPX:
            ea = (b1 + self.x) & 0xFF
        elif mode == ZPY:
            ea = (b1 + self.y) & 0xFF
        elif mode == ABS:
            ea = b2 << 8 | b1
        elif mode == ABX:
            base = b2 << 8 | b1
            ea = (base + self.x) & 0xFFFF
            cross = int((base & 0xFF00) != (ea & 0xFF00))
        elif mode == ABY:
            base = b2 << 8 | b1
            ea = (base + self.y) & 0xFFFF
            cross = int((base & 0xFF00) != (ea & 0xFF00))
        elif mode == IZX:
            p = (b1 + self.x) & 0xFF
            ea = m[p] | m[(p + 1) & 0xFF] << 8
        elif mode == IZY:
            base = m[b1] | m[(b1 + 1) & 0xFF] << 8
            ea = (base + self.y) & 0xFFFF
            cross = int((base & 0xFF00) != (ea & 0xFF00))
        cycles = cyc + (cross if pen else 0)

        def get():
            if mode == IMM:
                return b1
            return self.rd(ea, tr)

        def put(v):
            self.wr(ea, v, tr)

        nxt = npc
        a = self.a
        if mn == "LDA":
            self.a = self.nz(get())
        elif mn == "LDX":
            self.x = self.nz(get())
        elif mn == "LDY":
            self.y = self.nz(get())
        elif mn == "STA":
            put(a)
        elif mn == "STX":
            put(self.x)
        elif mn == "STY":
            put(self.y)
        elif mn == "ADC":
            self.adc(get())
        elif mn == "SBC":
            self.sbc(get())
        elif mn == "AND":
            self.a = self.nz(a & get())
        elif mn == "ORA":
            self.a = self.nz(a | get())
        elif mn == "EOR":
            self.a = self.nz(a ^ get())
        elif mn == "CMP":
            self.cmp(a, get())
        elif mn == "CPX":
            self.cmp(self.x, get())
        elif mn == "CPY":
            self.cmp(self.y, get())
        elif mn == "BIT":
            v = get()
            self.z = int((a & v) == 0)
            self.n = v >> 7
            self.v = v >> 6 & 1
        elif mn in ("ASL", "LSR", "ROL", "ROR"):
            f = {"ASL": self.asl, "LSR": self.lsr, "ROL": self.rol, "ROR": self.ror}[mn]
            if mode == ACC:
                self.a = f(a)
            else:
                put(f(get()))
        elif mn == "INC":
            put(self.nz(get() + 1))
        elif mn == "DEC":
            put(self.nz(get() - 1))
        elif mn == "INX":
            self.x = self.nz(self.x + 1)
        elif mn == "INY":
            self.y = self.nz(self.y + 1)
        elif mn == "DEX":
            self.x = self.nz(self.x - 1)
        elif mn == "DEY":
            self.y = self.nz(self.y - 1)
        elif mn == "TAX":
            self.x = self.nz(a)
        elif mn == "TAY":
            self.y = self.nz(a)
        elif mn == "TXA":
            self.a = self.nz(self.x)
        elif mn == "TYA":
            self.a = self.nz(self.y)
        elif mn == "TSX":
            self.x = self.nz(self.s)
        elif mn == "TXS":
            self.s = self.x
        elif mn == "PHA":
            self.push(a)
        elif mn == "PLA":
            self.a = self.nz(self.pull())
        elif mn == "PHP":
            self.push(self.getp(1))
        elif mn == "PLP":
            self.setp(self.pull())
        elif mn == "CLC":
            self.c = 0
        elif mn == "SEC":
            self.c = 1
        elif mn == "CLI":
            self.i = 0
        elif mn == "SEI":
            self.i = 1
        elif mn == "CLD":
            self.d = 0
        elif mn == "SED":
            self.d = 1
        elif mn == "CLV":
            self.v = 0
        elif mn == "NOP":
            if mode not in (IMP, IMM):
                get()
        elif mn == "JMP":
            if mode == ABS:
                nxt = b2 << 8 | b1
            else:
                p = b2 << 8 | b1
                nxt = m[p] | m[(p & 0xFF00) | ((p + 1) & 0xFF)] << 8
        elif mn == "JSR":
            r = (pc + 2) & 0xFFFF
            self.push(r >> 8)
            self.push(r & 0xFF)
            nxt = b2 << 8 | b1
        elif mn == "RTS":
            lo = self.pull()
            hi = self.pull()
            nxt = ((hi << 8 | lo) + 1) & 0xFFFF
        elif mn == "RTI":
            self.setp(self.pull())
            lo = self.pull()
            hi = self.pull()
            nxt = hi << 8 | lo
        elif mn == "BRK":
            r = (pc + 2) & 0xFFFF
            self.push(r >> 8)
            self.push(r & 0xFF)
            self.push(self.getp(1))
            self.i = 1
            nxt = m[0xFFFE] | m[0xFFFF] << 8
        elif mode == REL:
            cond = {"BPL": not self.n, "BMI": self.n, "BVC": not self.v, "BVS": self.v,
                    "BCC": not self.c, "BCS": self.c, "BNE": not self.z, "BEQ": self.z}[mn]
            if cond:
                tgt = (npc + (b1 - 256 if b1 & 0x80 else b1)) & 0xFFFF
                cycles += 1 + int((tgt & 0xFF00) != (npc & 0xFF00))
                nxt = tgt
        # ---- undocumented (stable)
        elif mn == "LAX":
            self.a = self.x = self.nz(get())
        elif mn == "SAX":
            put(a & self.x)
        elif mn in ("SLO", "RLA", "SRE", "RRA", "DCP", "ISC"):
            v = get()
            if mn == "SLO":
                v = self.asl(v)
                put(v)
                self.a = self.nz(a | v)
            elif mn == "RLA":
                v = self.rol(v)
                put(v)
                self.a = self.nz(a & v)
            elif mn == "SRE":
                v = self.lsr(v)
                put(v)
                self.a = self.nz(a ^ v)
            elif mn == "RRA":
                v = self.ror(v)
                put(v)
                self.adc(v)
            elif mn == "DCP":
                v = (v - 1) & 0xFF
                put(v)
                self.cmp(a, v)
            else:
                v = (v + 1) & 0xFF
                put(v)
                self.sbc(v)
        elif mn == "ANC":
            self.a = self.nz(a & b1)
            self.c = self.n
        elif mn == "ALR":
            self.a = self.lsr(a & b1)
        elif mn == "ARR":
            r = (a & b1)
            r = (r >> 1) | (self.c << 7)
            self.a = self.nz(r)
            self.c = r >> 6 & 1
            self.v = (r >> 6 & 1) ^ (r >> 5 & 1)
        elif mn == "SBX":
            t = (a & self.x) - b1
            self.c = int(t >= 0)
            self.x = self.nz(t)
        else:
            return False
        self.pc = nxt
        self.cycles += cycles
        return True
