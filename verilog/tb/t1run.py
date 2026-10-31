#!/usr/bin/env python3
"""Run a program image on the T1 RTL (Icarus) and check it against fsim.

  t1run.py IMAGE.bin             run to EXIT; compare the console output with fsim's
  t1run.py --lockstep IMAGE.bin  also compare registers and flags after every instruction
  t1run.py --json OUT IMAGE.bin  write {cycles, insts, exit, output, fsim_output, same}

The image is variant-A code loaded at 0x1000 (fcc and asm.py output).
"""
import argparse, json, os, re, subprocess, sys, tempfile, threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FSIM = os.path.join(ROOT, "sim", "build", "fsim")
BUILD = os.path.join(ROOT, "verilog", "build")
VVP = os.path.join(BUILD, "t1_tb.vvp")
SRCS = [os.path.join(HERE, "t1_tb.v"), os.path.join(ROOT, "verilog", "rtl", "t1_core.v"),
        os.path.join(ROOT, "verilog", "rtl", "t1_rf.v")]


def build():
    os.makedirs(BUILD, exist_ok=True)
    if os.path.exists(VVP) and all(os.path.getmtime(VVP) > os.path.getmtime(s) for s in SRCS):
        return
    r = subprocess.run(["iverilog", "-g2001", "-o", VVP, *SRCS], capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)


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


def fsim_output(image, max_cycles):
    r = subprocess.run([FSIM, "-q", "-v", "A", "--max-cycles", str(max_cycles), image],
                       capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return r.stdout


def run(image, lockstep=False, max_cycles=4_000_000_000):
    build()
    with tempfile.TemporaryDirectory() as td:
        hx = os.path.join(td, "image.hex")
        to_hex(image, hx)
        args = ["vvp", "-n", VVP, f"+image={hx}", f"+max={max_cycles}"]
        fs = None
        if lockstep:
            fifo = os.path.join(td, "trace")
            os.mkfifo(fifo)
            fs = subprocess.Popen([FSIM, "-q", "-v", "A", "--trace", "--max-cycles", str(max_cycles), image],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, stdin=subprocess.DEVNULL,
                                  text=True, bufsize=1 << 20)
            th = threading.Thread(target=lambda: convert(fs.stderr, open(fifo, "w")), daemon=True)
            th.start()
            args.append(f"+trace={fifo}")
        r = subprocess.run(args, capture_output=True, text=True)
        if fs:
            fs.kill()
    out = re.sub(r"(?m)^(.*\$finish called|WARNING: .*\$readmemh).*\n?", "", r.stdout)
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
    a = ap.parse_args()
    res = run(a.image, a.lockstep, a.max_cycles)
    want = fsim_output(a.image, 20_000_000_000)
    res["fsim_output"] = want
    res["same"] = res["output"] == want
    sys.stdout.write(res["output"])
    print(f"T1 {res['result']}  output {'same as fsim' if res['same'] else 'DIFFERENT from fsim'}")
    if a.json:
        json.dump(res, open(a.json, "w"), indent=1)
    sys.exit(0 if res["same"] and "exit" in res else 1)


if __name__ == "__main__":
    main()
