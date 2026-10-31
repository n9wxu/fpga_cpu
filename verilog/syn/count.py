#!/usr/bin/env python3
"""Transistor count of T1.

Runs Yosys with count.ys (synthesis onto the cell libraries t1_nmos.lib and
t1_cmos.lib, where a cell's area is its transistor count), sums the cells,
and adds the register file (t1_rf.v) as a macro, since Yosys sees it as a
black box.

  count.py [--ext-x 0|1|both] [--cells] [--check]

  --cells   also list the cells of each core
  --check   instead: elaborate the RTL and fail on any latch or other
            `check -assert` problem (both EXT_X settings)

Register file macro (docs/t1.md): 18 rows by 16 bits of static cells, each
a 6-transistor latch plus one read transistor per read port (two), so 8
per bit; for each read port a row decoder (one NOR3 per row, with the
inverters for the address bits) and a byte select (a 2:1 mux and a
buffer per bit line); for the write port a row decoder and a driver
(two inverters) per bit line.  The cell and decoder areas come from the
library being counted, so the macro follows the technology.
"""
import argparse, os, re, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
RTL = os.path.join(HERE, "..", "rtl")
TECH = {"nMOS": "t1_nmos.lib", "CMOS": "t1_cmos.lib"}
ROWS, BITS, RPORTS = 18, 16, 2


def areas(lib):
    text = open(os.path.join(HERE, lib)).read()
    return {m.group(1): float(m.group(2)) for m in re.finditer(r"cell\((\w+)\)\s*\{\s*area\s*:\s*([\d.]+)", text)}


def rf_macro(a):
    cell = 8 * ROWS * BITS                                 # 6T latch + 2 read transistors
    dec = ROWS * a["NOR3"] + 5 * a["INV"]                   # one row decoder
    rd = RPORTS * (dec + BITS // 2 * a["MUX2"] + BITS * a["BUF"])
    wr = dec + BITS * 2 * a["INV"]
    return cell + rd + wr


def run(tech, ext_x):
    lib = TECH[tech]
    script = open(os.path.join(HERE, "count.ys")).read().replace("EXT_X_VALUE", str(ext_x)).replace("LIB", lib)
    with tempfile.NamedTemporaryFile("w", suffix=".ys", dir=HERE, delete=False) as t:
        t.write(script)
    try:
        r = subprocess.run(["yosys", "-q", "-l", "/dev/stdout", t.name], cwd=HERE, capture_output=True, text=True)
    finally:
        os.unlink(t.name)
    if r.returncode:
        sys.exit(r.stdout + r.stderr)
    cells = {}
    for m in re.finditer(r"^\s+(\d+)\s+([\d.]+)\s+(\w+)\s*$", r.stdout, re.M):
        if m.group(3) != "cells":
            cells[m.group(3)] = (int(m.group(1)), float(m.group(2)))
    m = re.search(r"Chip area for module '\\t1_core': ([\d.]+)", r.stdout)
    if not m:
        sys.exit(r.stdout[-2000:])
    return float(m.group(1)), cells


def check():
    for ext_x in (0, 1):
        cmd = ("read_verilog -lib ../rtl/t1_rf.v; read_verilog ../rtl/t1_core.v; "
               f"chparam -set EXT_X {ext_x} t1_core; hierarchy -top t1_core -check; proc; opt_clean; check -assert")
        r = subprocess.run(["yosys", "-q", "-p", cmd], cwd=HERE, capture_output=True, text=True)
        print(f"EXT_X={ext_x}: {'clean (no latches, no check problems)' if r.returncode == 0 else 'FAILED'}")
        if r.returncode:
            sys.exit(r.stdout + r.stderr)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ext-x", default="both")
    ap.add_argument("--cells", action="store_true")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if a.check:
        check()
        return
    exts = [0, 1] if a.ext_x == "both" else [int(a.ext_x)]
    print("| Technology | EXT_X | core logic | register file | total |")
    print("|---|---|---|---|---|")
    for tech in TECH:
        rf = rf_macro(areas(TECH[tech]))
        for ext_x in exts:
            core, cells = run(tech, ext_x)
            print(f"| {tech} | {ext_x} | {core:,.0f} | {rf:,.0f} | {core + rf:,.0f} |")
            if a.cells:
                for k, (n, ar) in sorted(cells.items()):
                    print(f"|  | | {k}: {n} cells, {ar:,.0f} | | |")


if __name__ == "__main__":
    main()
