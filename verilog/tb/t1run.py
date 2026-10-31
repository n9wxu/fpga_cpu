#!/usr/bin/env python3
"""Run a program image on the T1 RTL (Icarus) and check it against fsim.

  t1run.py IMAGE.bin             run to EXIT; compare the console output with fsim's
  t1run.py --lockstep IMAGE.bin  also compare registers and flags after every instruction
  t1run.py --json OUT IMAGE.bin  write {cycles, insts, exit, output, fsim_output, same}
  t1run.py --ext-x 0 IMAGE.bin   build the core without extension X (fsim runs without --ext)
  t1run.py --reset-pc ADDR ...   start somewhere other than the load address (default 0x1000)

Besides the console output, how the run ended must agree: EXIT code, SYS n,
illegal instruction (FE here, 254 in fsim) and fault (FD here, 3 in fsim).

The image is variant-A code loaded at 0x1000 (fcc and asm.py output).
"""
import argparse, json, os, re, subprocess, sys, tempfile, threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FSIM = os.path.join(ROOT, "sim", "build", "fsim")
BUILD = os.path.join(ROOT, "verilog", "build")
SRCS = [os.path.join(HERE, "t1_tb.v"), os.path.join(ROOT, "verilog", "rtl", "t1_core.v"),
        os.path.join(ROOT, "verilog", "rtl", "t1_rf.v")]


def build(ext_x=1):
    os.makedirs(BUILD, exist_ok=True)
    vvp = os.path.join(BUILD, f"t1_tb_x{ext_x}.vvp")
    if os.path.exists(vvp) and all(os.path.getmtime(vvp) > os.path.getmtime(s) for s in SRCS):
        return vvp
    r = subprocess.run(["iverilog", "-g2001", f"-Pt1_tb.EXT_X={ext_x}", "-o", vvp, *SRCS],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    return vvp


def to_hex(image, out):
    with open(image, "rb") as f, open(out, "w") as g:
        g.write("\n".join(f"{b:02x}" for b in f.read()) + "\n")


TRACE = re.compile(r"^([0-9a-f]{6})  (.*?)  ((?:[0-9a-f]{4} ){16})([NZCV-]{4})$")


def convert(src, dst):
    """fsim --trace lines -> 'pc r0..r15 flags kind' for the testbench."""
    for line in src:
        m = TRACE.match(line.rstrip("\n"))
        if not m:
            continue
        fl = sum(b for ch, b in zip(m.group(4), (8, 4, 2, 1)) if ch != "-")
        dis = m.group(2).lstrip()
        kind = 1 if dis.startswith("MFSR") else 2 if dis.startswith("LD") else 0
        try:
            dst.write(f"{m.group(1)} {m.group(3)}{fl:x} {kind}\n")
        except BrokenPipeError:       # the testbench stopped first
            return
    try:
        dst.close()
    except BrokenPipeError:
        pass


def fsim_run(image, max_cycles, ext_x=1, entry=None):
    """-> (console output, fsim exit code)"""
    cmd = [FSIM, "-q", "-v", "A", "--max-cycles", str(max_cycles)] + (["--ext"] if ext_x else [])
    if entry is not None:
        cmd += ["--entry", hex(entry)]
    r = subprocess.run(cmd + [image], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return r.stdout, r.returncode


def expected_rc(res):
    """The fsim exit code that corresponds to how T1 ended."""
    if "exit" in res:
        return res["exit"] & 0xFF
    if "halt" in res:
        return 3 if res["halt"] == 0xFD else 128 + (res["halt"] & 0x7F)
    return None


def run(image, lockstep=False, max_cycles=4_000_000_000, ext_x=1, entry=None):
    vvp = build(ext_x)
    with tempfile.TemporaryDirectory() as td:
        hx = os.path.join(td, "image.hex")
        to_hex(image, hx)
        args = ["vvp", "-n", vvp, f"+image={hx}", f"+max={max_cycles}"]
        if entry is not None:
            args.append(f"+entry={entry:x}")
        fs = None
        if lockstep:
            fifo = os.path.join(td, "trace")
            os.mkfifo(fifo)
            fs = subprocess.Popen([FSIM, "-q", "-v", "A", "--trace", "--max-cycles", str(max_cycles)]
                                  + (["--ext"] if ext_x else []) + (["--entry", hex(entry)] if entry is not None else []) + [image],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL,
                                  text=True, bufsize=1 << 20)
            def feed():
                try:
                    convert(fs.stderr, open(fifo, "w"))
                except (BrokenPipeError, ValueError, OSError):
                    pass            # the testbench stopped reading (a mismatch or a halt)
            th = threading.Thread(target=feed, daemon=True)
            th.start()
            args.append(f"+trace={fifo}")
        r = subprocess.run(args, capture_output=True, text=True)
        if fs:
            fs.kill()
    out = re.sub(r"(?m)^(.*\$finish called|WARNING: .*\$readmemh|\(bus error).*\n?", "", r.stdout)
    m = re.search(r"\nT1 (.*)\n?$", out)
    tail = m.group(1) if m else "no result"
    body = out[:m.start()] if m else out
    res = {"result": tail, "output": body}
    for key in ("cycles", "insts", "exit", "halt"):
        mm = re.search(rf"\b{key}=(\d+)", tail)
        if mm:
            res[key] = int(mm.group(1))
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("--lockstep", action="store_true")
    ap.add_argument("--max-cycles", type=int, default=4_000_000_000)
    ap.add_argument("--json")
    ap.add_argument("--ext-x", type=int, default=1)
    ap.add_argument("--reset-pc", type=lambda x: int(x, 0))
    a = ap.parse_args()
    res = run(a.image, a.lockstep, a.max_cycles, a.ext_x, a.reset_pc)
    want, rc = fsim_run(a.image, 20_000_000_000, a.ext_x, a.reset_pc)
    res["fsim_output"] = want
    res["fsim_exit"] = rc
    res["same"] = res["output"] == want and expected_rc(res) == rc
    sys.stdout.write(res["output"])
    print(f"T1 {res['result']}  fsim exit {rc}  {'same as fsim' if res['same'] else 'DIFFERENT from fsim'}")
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)
    sys.exit(0 if res["same"] else 1)


if __name__ == "__main__":
    main()
