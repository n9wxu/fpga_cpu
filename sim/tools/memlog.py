#!/usr/bin/env python3
"""Read an fsim --log file: what a program did, frame by frame.

  memlog.py LOG                          totals: cycles, instructions, bus bytes
  memlog.py LOG --sym S --marker frame_done --vram 0xD700:128:80 [--ref DIR] [--out DIR]

The log has one 8-byte record per instruction (pc, length, start cycle)
and one per data access (read or write, size, address, value); see
log_rec in src/cpu.c. A frame ends at each write to the marker symbol.
For each frame this prints the cycles and instructions it took and the
bytes it moved: instruction bytes fetched, data bytes read and written,
and bytes written to video memory. With --vram the frame buffer is
rebuilt from the writes into it, compared with DIR/frameN.raw (--ref,
the REFERENCE build's frames) and saved as frameN.raw in --out.
"""
import argparse
import os
import struct
import sys


def records(path):
    """Yield (word0, word1) per 8-byte record. A partial record at the end
    (a log cut short by a crash or a full disk) is dropped with a warning."""
    rest = b""
    with open(path, "rb") as f:
        while True:
            buf = f.read(8 << 20)
            if not buf:
                break
            buf = rest + buf
            n = len(buf) & ~7
            rest = buf[n:]
            yield from struct.iter_unpack("<II", buf[:n])
    if rest:
        print(f"memlog: {path}: dropped a partial trailing record ({len(rest)} bytes)", file=sys.stderr)


def symbol(path, name):
    for line in open(path):
        p = line.split()
        if len(p) >= 2 and p[1] == name:
            return int(p[0], 16)
    sys.exit(f"{name} is not in {path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--sym", help="symbol file (fcc writes it next to the .bin)")
    ap.add_argument("--marker", help="symbol whose writes end a frame")
    ap.add_argument("--vram", help="ADDR:WIDTH:HEIGHT of an 8-bit frame buffer")
    ap.add_argument("--ref", help="directory of frameN.raw files to compare with")
    ap.add_argument("--out", help="directory to write the rebuilt frameN.raw files")
    a = ap.parse_args()

    marker = symbol(a.sym, a.marker) if a.marker else None
    vbase = vsize = 0
    if a.vram:
        b, w, h = a.vram.split(":")
        vbase, vsize = int(b, 0), int(w) * int(h)
        vram = bytearray(vsize)

    # segments end at marker writes: the value written names what ended
    # (0 = start-up, n = frame n - 1); the last segment runs to the exit
    segs = []
    new = lambda: dict(insts=0, ibytes=0, rbytes=0, wbytes=0, vbytes=0, cycles=0)
    cur = new()
    # The log holds the low 32 bits of the cycle count, so unwrap it (cycles
    # never go backwards). An instruction's cycles are counted up to the start
    # of the next one, which charges the last instruction of a segment to it.
    wrap, last_raw = 0, 0
    pend, pend_cyc = None, 0
    for w0, w1 in records(a.log):
        kind = w0 >> 28
        if kind == 0:
            if w1 < last_raw:
                wrap += 1 << 32
            last_raw = w1
            cyc = w1 + wrap
            if pend is not None:
                pend["cycles"] += cyc - pend_cyc
            pend, pend_cyc = cur, cyc
            cur["insts"] += 1
            cur["ibytes"] += (w0 >> 24) & 15
            continue
        size = (w0 >> 24) & 15
        addr = w0 & 0xFFFFFF
        if kind == 1:
            cur["rbytes"] += size
            continue
        cur["wbytes"] += size
        if vsize and vbase <= addr < vbase + vsize:
            cur["vbytes"] += size
            vram[addr - vbase] = w1 & 0xFF
            if size == 2 and addr + 1 < vbase + vsize:
                vram[addr + 1 - vbase] = w1 >> 8
        if addr == marker:
            v = w1 & 0xFF
            if v == 0 and segs and segs[-1]["name"] == "init":
                prev = segs[-1]         # start-up code that clears memory writes 0 too
                for k in ("insts", "ibytes", "rbytes", "wbytes", "vbytes", "cycles"):
                    prev[k] += cur[k]
                if pend is cur:
                    pend = prev
                cur = new()
                continue
            cur["name"] = "init" if v == 0 else str(v - 1)
            if v and vsize:
                cur["frame"] = (v - 1, bytes(vram))
            segs.append(cur)
            cur = new()
    cur["name"] = "exit" if marker is not None else "all"
    segs.append(cur)

    if a.out:
        os.makedirs(a.out, exist_ok=True)
    bad = 0
    print(f"{'':>5} {'cycles':>10} {'insts':>9} {'CPI':>5} {'inst B':>9} {'read B':>9} {'write B':>8} "
          f"{'VRAM B':>7} {'bus B':>9}  frame")
    tot, n = dict(cycles=0, insts=0, ibytes=0, rbytes=0, wbytes=0, vbytes=0), 0
    for sg in segs:
        check = ""
        if "frame" in sg:
            k, px = sg["frame"]
            if a.out:
                open(os.path.join(a.out, f"frame{k}.raw"), "wb").write(px)
            if a.ref:
                want = open(os.path.join(a.ref, f"frame{k}.raw"), "rb").read()
                diff = sum(x != y for x, y in zip(px, want)) + abs(len(px) - len(want))
                check = "same as reference" if not diff else f"{diff} pixels DIFFER from reference"
                bad += diff != 0
            for key in tot:
                tot[key] += sg[key]
            n += 1
        bus = sg["ibytes"] + sg["rbytes"] + sg["wbytes"]
        cpi = sg["cycles"] / sg["insts"] if sg["insts"] else 0
        print(f"{sg['name']:>5} {sg['cycles']:>10,} {sg['insts']:>9,} {cpi:>5.2f} {sg['ibytes']:>9,} {sg['rbytes']:>9,} "
              f"{sg['wbytes']:>8,} {sg['vbytes']:>7,} {bus:>9,}  {check}")
    if n:
        bus = tot["ibytes"] + tot["rbytes"] + tot["wbytes"]
        print(f"{'mean':>5} {tot['cycles'] // n:>10,} {tot['insts'] // n:>9,} {tot['cycles'] / tot['insts']:>5.2f} "
              f"{tot['ibytes'] // n:>9,} {tot['rbytes'] // n:>9,} {tot['wbytes'] // n:>8,} {tot['vbytes'] // n:>7,} "
              f"{bus // n:>9,}  ({n} frames)")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
