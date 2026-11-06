#!/usr/bin/env python3
"""Turn a C64 SID tune (PSID) into SID register frames for fpga/demo/sidplay.c.

The tune's own 6502 code runs here, on the host: its init routine once (with
the song number in A), then its play routine once a frame (50 Hz, or the CIA
timer's rate when the header asks for it). After each call the 25 SID
registers (D400-D418) are taken, and the frame is written as the registers
that changed: a 32-bit mask (bit r: register r follows), then their values.

  sid2regs.py TUNE.sid out.c [--song N] [--seconds S] [--name NAME]

out.c holds `const uint8_t NAME[]` (the frames), `NAME_frames`, `NAME_hz`
(play calls a second) and the header's title, author and released strings.

A PSID tune runs on the 6502 alone (its play routine called from outside);
an RSID tune needs a whole C64 (interrupts, the CIAs) and is refused. The
6502 here runs the documented instructions and the common undocumented ones
(LAX, SAX, DCP, ISB, SLO, RLA, SRE, RRA, the NOPs). Memory is flat RAM: the
C64's ROMs and I/O are not there, except the CIA 1 timer A latch (DC04/05),
read for the play rate, and the SID registers, kept as written."""

import argparse
import struct
import sys

PAL_HZ = 985248


class CPU6502:
    def __init__(self, mem):
        self.m = mem
        self.a = self.x = self.y = 0
        self.s = 0xFF
        self.p = 0x24
        self.pc = 0
        self.cycles = 0

    # ---- memory
    def rd(self, a):
        return self.m[a & 0xFFFF]

    def wr(self, a, v):
        self.m[a & 0xFFFF] = v & 0xFF

    def rd16(self, a):
        return self.rd(a) | self.rd(a + 1) << 8

    def rd16zp(self, a):
        return self.rd(a & 0xFF) | self.rd((a + 1) & 0xFF) << 8

    def push(self, v):
        self.wr(0x100 + self.s, v)
        self.s = (self.s - 1) & 0xFF

    def pull(self):
        self.s = (self.s + 1) & 0xFF
        return self.rd(0x100 + self.s)

    # ---- flags
    def nz(self, v):
        v &= 0xFF
        self.p = (self.p & ~0x82) | (v & 0x80) | (0x02 if v == 0 else 0)
        return v

    def flag(self, f, on):
        self.p = (self.p | f) if on else (self.p & ~f)

    # ---- addressing: the effective address of the operand
    def ea(self, mode):
        pc = self.pc
        if mode == 'imm':
            self.pc += 1
            return pc
        if mode == 'zp':
            self.pc += 1
            return self.rd(pc)
        if mode == 'zpx':
            self.pc += 1
            return (self.rd(pc) + self.x) & 0xFF
        if mode == 'zpy':
            self.pc += 1
            return (self.rd(pc) + self.y) & 0xFF
        if mode == 'abs':
            self.pc += 2
            return self.rd16(pc)
        if mode == 'abx':
            self.pc += 2
            return (self.rd16(pc) + self.x) & 0xFFFF
        if mode == 'aby':
            self.pc += 2
            return (self.rd16(pc) + self.y) & 0xFFFF
        if mode == 'izx':
            self.pc += 1
            return self.rd16zp(self.rd(pc) + self.x)
        if mode == 'izy':
            self.pc += 1
            return (self.rd16zp(self.rd(pc)) + self.y) & 0xFFFF
        raise ValueError(mode)

    # ---- arithmetic
    def adc(self, v):
        c = self.p & 1
        if self.p & 0x08:                    # decimal
            lo = (self.a & 0x0F) + (v & 0x0F) + c
            hi = (self.a >> 4) + (v >> 4)
            if lo > 9:
                lo += 6
                hi += 1
            r_bin = (self.a + v + c) & 0xFF
            self.flag(0x40, (~(self.a ^ v) & (self.a ^ (hi << 4)) & 0x80) != 0)
            if hi > 9:
                hi += 6
            self.flag(0x01, hi > 15)
            self.a = ((hi << 4) | (lo & 0x0F)) & 0xFF
            self.flag(0x02, r_bin == 0)
            self.flag(0x80, (self.a & 0x80) != 0)
            return
        r = self.a + v + c
        self.flag(0x01, r > 0xFF)
        self.flag(0x40, (~(self.a ^ v) & (self.a ^ r) & 0x80) != 0)
        self.a = self.nz(r)

    def sbc(self, v):
        if self.p & 0x08:
            c = self.p & 1
            lo = (self.a & 0x0F) - (v & 0x0F) - (1 - c)
            hi = (self.a >> 4) - (v >> 4)
            if lo < 0:
                lo -= 6
                hi -= 1
            if hi < 0:
                hi -= 6
            r = self.a - v - (1 - c)
            self.flag(0x01, r >= 0)
            self.flag(0x40, ((self.a ^ v) & (self.a ^ r) & 0x80) != 0)
            self.nz(r)
            self.a = ((hi << 4) | (lo & 0x0F)) & 0xFF
            return
        self.adc(v ^ 0xFF)

    def cmp(self, reg, v):
        r = reg - v
        self.flag(0x01, r >= 0)
        self.nz(r)

    def asl(self, v):
        self.flag(0x01, v & 0x80)
        return self.nz(v << 1)

    def lsr(self, v):
        self.flag(0x01, v & 1)
        return self.nz(v >> 1)

    def rol(self, v):
        c = self.p & 1
        self.flag(0x01, v & 0x80)
        return self.nz(v << 1 | c)

    def ror(self, v):
        c = self.p & 1
        self.flag(0x01, v & 1)
        return self.nz(v >> 1 | c << 7)

    def branch(self, cond):
        off = self.rd(self.pc)
        self.pc += 1
        if cond:
            self.pc = (self.pc + (off - 256 if off & 0x80 else off)) & 0xFFFF

    def step(self):
        op = self.rd(self.pc)
        self.pc = (self.pc + 1) & 0xFFFF
        self.cycles += 3
        e = OPS.get(op)
        if e is None:
            raise RuntimeError(f'opcode {op:02X} at {self.pc - 1:04X}')
        e(self)

    def call(self, addr, a=0, limit=2000000):
        """JSR addr and run until it returns (a return to 0 marks the end)"""
        self.a = a & 0xFF
        self.push(0xFF)                      # returns to 0x0000 (+1)
        self.push(0xFF)
        self.pc = addr
        self.s_end = self.s
        n = 0
        while not (self.pc == 0 and self.s == (self.s_end + 2) & 0xFF):
            self.step()
            n += 1
            if n > limit:
                raise RuntimeError(f'routine at {addr:04X} did not return')
        return n


OPS = {}


def _op(code, f):
    OPS[code] = f


def _load_ops():
    modes = {'imm': None, 'zp': None, 'zpx': None, 'zpy': None, 'abs': None, 'abx': None,
             'aby': None, 'izx': None, 'izy': None}

    def ld(reg, mode):
        def f(c):
            setattr(c, reg, c.nz(c.rd(c.ea(mode))))
        return f

    def st(reg, mode):
        def f(c):
            c.wr(c.ea(mode), getattr(c, reg))
        return f

    def alu(fn, mode):
        def f(c):
            fn(c, c.rd(c.ea(mode)))
        return f

    def rmw(fn, mode):
        def f(c):
            a = c.ea(mode)
            c.wr(a, fn(c, c.rd(a)))
        return f

    groups = {
        'ORA': (lambda c, v: setattr(c, 'a', c.nz(c.a | v)),
                {0x09: 'imm', 0x05: 'zp', 0x15: 'zpx', 0x0D: 'abs', 0x1D: 'abx', 0x19: 'aby', 0x01: 'izx', 0x11: 'izy'}),
        'AND': (lambda c, v: setattr(c, 'a', c.nz(c.a & v)),
                {0x29: 'imm', 0x25: 'zp', 0x35: 'zpx', 0x2D: 'abs', 0x3D: 'abx', 0x39: 'aby', 0x21: 'izx', 0x31: 'izy'}),
        'EOR': (lambda c, v: setattr(c, 'a', c.nz(c.a ^ v)),
                {0x49: 'imm', 0x45: 'zp', 0x55: 'zpx', 0x4D: 'abs', 0x5D: 'abx', 0x59: 'aby', 0x41: 'izx', 0x51: 'izy'}),
        'ADC': (lambda c, v: c.adc(v),
                {0x69: 'imm', 0x65: 'zp', 0x75: 'zpx', 0x6D: 'abs', 0x7D: 'abx', 0x79: 'aby', 0x61: 'izx', 0x71: 'izy'}),
        'SBC': (lambda c, v: c.sbc(v),
                {0xE9: 'imm', 0xEB: 'imm', 0xE5: 'zp', 0xF5: 'zpx', 0xED: 'abs', 0xFD: 'abx', 0xF9: 'aby', 0xE1: 'izx', 0xF1: 'izy'}),
        'CMP': (lambda c, v: c.cmp(c.a, v),
                {0xC9: 'imm', 0xC5: 'zp', 0xD5: 'zpx', 0xCD: 'abs', 0xDD: 'abx', 0xD9: 'aby', 0xC1: 'izx', 0xD1: 'izy'}),
        'CPX': (lambda c, v: c.cmp(c.x, v), {0xE0: 'imm', 0xE4: 'zp', 0xEC: 'abs'}),
        'CPY': (lambda c, v: c.cmp(c.y, v), {0xC0: 'imm', 0xC4: 'zp', 0xCC: 'abs'}),
        'BIT': (lambda c, v: (c.flag(0x02, (c.a & v) == 0), c.flag(0x80, v & 0x80), c.flag(0x40, v & 0x40)),
                {0x24: 'zp', 0x2C: 'abs'}),
        'LAX': (lambda c, v: (setattr(c, 'a', c.nz(v)), setattr(c, 'x', c.a)),
                {0xA7: 'zp', 0xB7: 'zpy', 0xAF: 'abs', 0xBF: 'aby', 0xA3: 'izx', 0xB3: 'izy', 0xAB: 'imm'}),
        'NOPR': (lambda c, v: None,
                 {0x80: 'imm', 0x82: 'imm', 0x89: 'imm', 0xC2: 'imm', 0xE2: 'imm', 0x04: 'zp', 0x44: 'zp',
                  0x64: 'zp', 0x14: 'zpx', 0x34: 'zpx', 0x54: 'zpx', 0x74: 'zpx', 0xD4: 'zpx', 0xF4: 'zpx',
                  0x0C: 'abs', 0x1C: 'abx', 0x3C: 'abx', 0x5C: 'abx', 0x7C: 'abx', 0xDC: 'abx', 0xFC: 'abx'}),
        'ANC': (lambda c, v: (setattr(c, 'a', c.nz(c.a & v)), c.flag(0x01, c.a & 0x80)), {0x0B: 'imm', 0x2B: 'imm'}),
        'ALR': (lambda c, v: setattr(c, 'a', c.lsr(c.a & v)), {0x4B: 'imm'}),
    }
    for name, (fn, codes) in groups.items():
        for code, mode in codes.items():
            _op(code, alu(fn, mode))

    for code, mode in {0xA9: 'imm', 0xA5: 'zp', 0xB5: 'zpx', 0xAD: 'abs', 0xBD: 'abx', 0xB9: 'aby', 0xA1: 'izx', 0xB1: 'izy'}.items():
        _op(code, ld('a', mode))
    for code, mode in {0xA2: 'imm', 0xA6: 'zp', 0xB6: 'zpy', 0xAE: 'abs', 0xBE: 'aby'}.items():
        _op(code, ld('x', mode))
    for code, mode in {0xA0: 'imm', 0xA4: 'zp', 0xB4: 'zpx', 0xAC: 'abs', 0xBC: 'abx'}.items():
        _op(code, ld('y', mode))
    for code, mode in {0x85: 'zp', 0x95: 'zpx', 0x8D: 'abs', 0x9D: 'abx', 0x99: 'aby', 0x81: 'izx', 0x91: 'izy'}.items():
        _op(code, st('a', mode))
    for code, mode in {0x86: 'zp', 0x96: 'zpy', 0x8E: 'abs'}.items():
        _op(code, st('x', mode))
    for code, mode in {0x84: 'zp', 0x94: 'zpx', 0x8C: 'abs'}.items():
        _op(code, st('y', mode))
    for code, mode in {0x87: 'zp', 0x97: 'zpy', 0x8F: 'abs', 0x83: 'izx'}.items():   # SAX
        _op(code, lambda c, m=mode: c.wr(c.ea(m), c.a & c.x))

    shifts = {'ASL': (CPU6502.asl, {0x06: 'zp', 0x16: 'zpx', 0x0E: 'abs', 0x1E: 'abx'}),
              'LSR': (CPU6502.lsr, {0x46: 'zp', 0x56: 'zpx', 0x4E: 'abs', 0x5E: 'abx'}),
              'ROL': (CPU6502.rol, {0x26: 'zp', 0x36: 'zpx', 0x2E: 'abs', 0x3E: 'abx'}),
              'ROR': (CPU6502.ror, {0x66: 'zp', 0x76: 'zpx', 0x6E: 'abs', 0x7E: 'abx'}),
              'INC': (lambda c, v: c.nz(v + 1), {0xE6: 'zp', 0xF6: 'zpx', 0xEE: 'abs', 0xFE: 'abx'}),
              'DEC': (lambda c, v: c.nz(v - 1), {0xC6: 'zp', 0xD6: 'zpx', 0xCE: 'abs', 0xDE: 'abx'})}
    for name, (fn, codes) in shifts.items():
        for code, mode in codes.items():
            _op(code, rmw(fn, mode))
    _op(0x0A, lambda c: setattr(c, 'a', c.asl(c.a)))
    _op(0x4A, lambda c: setattr(c, 'a', c.lsr(c.a)))
    _op(0x2A, lambda c: setattr(c, 'a', c.rol(c.a)))
    _op(0x6A, lambda c: setattr(c, 'a', c.ror(c.a)))

    # undocumented read-modify-write
    def dcp(c, v):
        v = (v - 1) & 0xFF
        c.cmp(c.a, v)
        return v

    def isb(c, v):
        v = (v + 1) & 0xFF
        c.sbc(v)
        return v

    def slo(c, v):
        v = c.asl(v)
        c.a = c.nz(c.a | v)
        return v

    def rla(c, v):
        v = c.rol(v)
        c.a = c.nz(c.a & v)
        return v

    def sre(c, v):
        v = c.lsr(v)
        c.a = c.nz(c.a ^ v)
        return v

    def rra(c, v):
        v = c.ror(v)
        c.adc(v)
        return v

    und = {dcp: (0xC7, 0xD7, 0xCF, 0xDF, 0xDB, 0xC3, 0xD3),
           isb: (0xE7, 0xF7, 0xEF, 0xFF, 0xFB, 0xE3, 0xF3),
           slo: (0x07, 0x17, 0x0F, 0x1F, 0x1B, 0x03, 0x13),
           rla: (0x27, 0x37, 0x2F, 0x3F, 0x3B, 0x23, 0x33),
           sre: (0x47, 0x57, 0x4F, 0x5F, 0x5B, 0x43, 0x53),
           rra: (0x67, 0x77, 0x6F, 0x7F, 0x7B, 0x63, 0x73)}
    umodes = ('zp', 'zpx', 'abs', 'abx', 'aby', 'izx', 'izy')
    for fn, codes in und.items():
        for code, mode in zip(codes, umodes):
            _op(code, rmw(fn, mode))

    def setf(f, on):
        return lambda c: c.flag(f, on)
    _op(0x18, setf(0x01, False)); _op(0x38, setf(0x01, True))
    _op(0x58, setf(0x04, False)); _op(0x78, setf(0x04, True))
    _op(0xB8, setf(0x40, False))
    _op(0xD8, setf(0x08, False)); _op(0xF8, setf(0x08, True))
    for code in (0xEA, 0x1A, 0x3A, 0x5A, 0x7A, 0xDA, 0xFA):
        _op(code, lambda c: None)
    _op(0xAA, lambda c: setattr(c, 'x', c.nz(c.a)))
    _op(0xA8, lambda c: setattr(c, 'y', c.nz(c.a)))
    _op(0x8A, lambda c: setattr(c, 'a', c.nz(c.x)))
    _op(0x98, lambda c: setattr(c, 'a', c.nz(c.y)))
    _op(0xBA, lambda c: setattr(c, 'x', c.nz(c.s)))
    _op(0x9A, lambda c: setattr(c, 's', c.x))
    _op(0xE8, lambda c: setattr(c, 'x', c.nz(c.x + 1)))
    _op(0xC8, lambda c: setattr(c, 'y', c.nz(c.y + 1)))
    _op(0xCA, lambda c: setattr(c, 'x', c.nz(c.x - 1)))
    _op(0x88, lambda c: setattr(c, 'y', c.nz(c.y - 1)))
    _op(0x48, lambda c: c.push(c.a))
    _op(0x08, lambda c: c.push(c.p | 0x30))
    _op(0x68, lambda c: setattr(c, 'a', c.nz(c.pull())))
    _op(0x28, lambda c: setattr(c, 'p', (c.pull() & ~0x10) | 0x20))
    for code, (f, on) in {0x10: (0x80, 0), 0x30: (0x80, 1), 0x50: (0x40, 0), 0x70: (0x40, 1),
                          0x90: (0x01, 0), 0xB0: (0x01, 1), 0xD0: (0x02, 0), 0xF0: (0x02, 1)}.items():
        _op(code, lambda c, f=f, on=on: c.branch(bool(c.p & f) == bool(on)))

    def jmp(c):
        c.pc = c.rd16(c.pc)

    def jmpi(c):
        a = c.rd16(c.pc)
        c.pc = c.rd(a) | c.rd((a & 0xFF00) | ((a + 1) & 0xFF)) << 8   # the page-wrap bug

    def jsr(c):
        t = c.rd16(c.pc)
        r = (c.pc + 1) & 0xFFFF
        c.push(r >> 8)
        c.push(r & 0xFF)
        c.pc = t

    def rts(c):
        lo = c.pull()
        c.pc = ((c.pull() << 8 | lo) + 1) & 0xFFFF

    def rti(c):
        c.p = (c.pull() & ~0x10) | 0x20
        lo = c.pull()
        c.pc = c.pull() << 8 | lo

    def brk(c):
        # a tune that breaks has ended its routine: return as RTS would
        raise RuntimeError(f'BRK at {c.pc - 1:04X}')
    _op(0x4C, jmp); _op(0x6C, jmpi); _op(0x20, jsr); _op(0x60, rts); _op(0x40, rti); _op(0x00, brk)


_load_ops()


def load_psid(data):
    magic = data[0:4]
    if magic == b'RSID':
        sys.exit('sid2regs: an RSID tune needs a whole C64; only PSID tunes play here')
    if magic != b'PSID':
        sys.exit('sid2regs: not a PSID file')
    (version, offset, load, init, play, songs, start, speed) = struct.unpack('>HHHHHHHI', data[4:22])
    name = data[22:54].split(b'\0')[0].decode('latin-1')
    author = data[54:86].split(b'\0')[0].decode('latin-1')
    released = data[86:118].split(b'\0')[0].decode('latin-1')
    body = data[offset:]
    if load == 0:
        load = body[0] | body[1] << 8
        body = body[2:]
    return dict(load=load, init=init, play=play, songs=songs, start=start, speed=speed,
                body=body, name=name, author=author, released=released)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('sid')
    ap.add_argument('out')
    ap.add_argument('--song', type=int, help='1 up (default: the tune\'s start song)')
    ap.add_argument('--seconds', type=float, default=120)
    ap.add_argument('--name', default='tune')
    a = ap.parse_args()

    t = load_psid(open(a.sid, 'rb').read())
    song = (a.song or t['start']) - 1
    mem = bytearray(65536)
    mem[t['load']:t['load'] + len(t['body'])] = t['body']
    cpu = CPU6502(mem)
    cpu.call(t['init'], song)
    play = t['play']
    if play == 0:                            # the tune set its own interrupt vector
        play = mem[0x0314] | mem[0x0315] << 8
        if play == 0:
            play = mem[0xFFFE] | mem[0xFFFF] << 8
    cia = (t['speed'] >> min(song, 31)) & 1
    hz = 50.0
    if cia:
        latch = mem[0xDC04] | mem[0xDC05] << 8
        hz = PAL_HZ / latch if latch else 60.0
    frames = int(a.seconds * hz)
    out = bytearray()
    prev = [None] * 25
    for _ in range(frames):
        cpu.call(play)
        regs = list(mem[0xD400:0xD419])
        mask = 0
        vals = bytearray()
        for r in range(25):
            if regs[r] != prev[r]:
                mask |= 1 << r
                vals.append(regs[r])
        prev = regs
        out += struct.pack('<I', mask) + vals

    def cstr(s):
        return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
    n = a.name
    with open(a.out, 'w') as f:
        f.write(f'/* {t["name"]} ({t["author"]}, {t["released"]}): song {song + 1}, '
                f'{frames} frames at {hz:.3f} Hz, made by fpga/tools/sid2regs.py */\n')
        f.write('#include <stdint.h>\n')
        f.write(f'const char {n}_title[] = {cstr(t["name"])};\n')
        f.write(f'const char {n}_author[] = {cstr(t["author"])};\n')
        f.write(f'const char {n}_released[] = {cstr(t["released"])};\n')
        f.write(f'const uint32_t {n}_frames = {frames};\n')
        f.write(f'const uint16_t {n}_hz100 = {round(hz * 100)};   /* play calls a second x 100 */\n')
        f.write(f'const uint8_t {n}[{len(out)}] = {{\n')
        for i in range(0, len(out), 20):
            f.write('    ' + ', '.join(str(b) for b in out[i:i + 20]) + ',\n')
        f.write('};\n')
    print(f'{t["name"]} / {t["author"]}: song {song + 1} of {t["songs"]}, {frames} frames at {hz:.2f} Hz, '
          f'{len(out)} bytes', file=sys.stderr)


if __name__ == '__main__':
    main()
