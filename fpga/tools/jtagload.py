"""Load a program into the fpga_cpu system over JTAG and run it
(rtl/jtagldr.v), or read its status.

usage: jtagload.py image.bin [--base 0x1000] [--entry ADDR] [--no-clear]
       jtagload.py --status
       jtagload.py --verify image.bin [--base 0x1000]   (holds T1 in reset)
       jtagload.py --dump ADDR WORDS                     (holds T1 in reset)

Holds T1 in reset, streams the image into SRAM at --base, zeroes the text
buffer (0FC000, 4000 bytes: crt0 turns the text layer on, and SRAM is not
zero at power-up as it is in fsim) unless --no-clear, then releases T1 at
--entry (default: --base). Prints the status word afterwards:
running/halted, the halt code, and the low 16 bits of the instruction count.

Needs OpenOCD with the ftdi driver (as fpga/Makefile finds it, or $OPENOCD).
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FPGA = os.path.dirname(HERE)
CHUNK = 128                                   # words per drscan


def openocd():
    if os.environ.get("OPENOCD"):
        return os.environ["OPENOCD"]
    found = glob.glob(os.path.expanduser(
        "~/Library/Arduino15/packages/esp32/tools/openocd-esp32/*/bin/openocd"))
    return found[0] if found else "openocd"


def user2(value):
    return ["irscan xc3s.tap 0x03", user2_read(value)]


def read_status():
    # 31 bits of USER1: the status, and no word written (that takes 32);
    # USER2 with bit 30 clear: T1's last bus access, and no command
    return ["irscan xc3s.tap 0x02", "echo \"status [drscan xc3s.tap 31 0]\"",
            "irscan xc3s.tap 0x03", "echo \"lastacc [drscan xc3s.tap 32 0]\""]


def stream_raw(words):
    """One USER1 scan of words as the board takes them: the hardware forms
    word n from scan bits 32n-1 .. 32n+30 (one bit early), so the scan is the
    words shifted down one bit, and the first word's bit 0 is lost."""
    v = 0
    for k, w in enumerate(words):
        v |= w << (32 * k)
    return f"drscan xc3s.tap {32 * len(words) - 1} 0x{v >> 1:x}"


def load(base, words):
    """Write words at base (a multiple of 4). Each scan starts with a dummy
    word, written one word below its chunk (its bit 0 is lost); the chunks go
    last to first, so each dummy lands on a word a later scan rewrites. Only
    the first dummy is left, at base - 4."""
    cmds = []
    starts = list(range(0, len(words), CHUNK))
    for i in reversed(starts):
        part = words[i:i + CHUNK]
        dummy = words[i - 1] if i else 0
        cmds += user2(0x40000000 | ((base + 4 * i - 4) & 0xFFFFC))
        cmds += ["irscan xc3s.tap 0x02", stream_raw([dummy] + part)]
    return cmds


def stream(words):
    """Legacy name used by tests: words at the load pointer already set.
    (Kept for callers that set the pointer themselves; it loses bit 0 of the
    first word.)"""
    return ["irscan xc3s.tap 0x02", stream_raw(words)]


def decode(line):
    s = int(line.split()[-1], 16)
    halted, running, code, insts = s >> 30 & 1, s >> 29 & 1, s >> 16 & 0xFF, s & 0xFFFF
    state = f"halted, code 0x{code:02x}" if halted else ("running" if running else "held in reset")
    return f"T1 {state}; instructions (low 16 bits) {insts}"


def run_tcl(cmds):
    tcl = os.path.join(FPGA, "build", "jtagload.tcl")
    os.makedirs(os.path.dirname(tcl), exist_ok=True)
    open(tcl, "w").write("\n".join(cmds) + "\n")
    r = subprocess.run([openocd(), "-f", os.path.join(FPGA, "openocd", "opella-ld.cfg"),
                        "-f", os.path.join(FPGA, "openocd", "s3starter.cfg"), "-f", tcl],
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    if r.returncode:
        sys.exit(out)
    return out


# USER2 scans are 33 bits: the register takes the command from the last 32
# bits in (the same one-bit offset as USER1), and the 32 bits it shifts out
# first are the word read.
def user2_read(cmd):
    return f"drscan xc3s.tap 33 0x{cmd << 1:09x}"


def scan_value(line):
    return int(line.split()[-1], 16) & 0xFFFFFFFF


def read_words(start, n):
    """Read n SRAM words from byte address start (T1 is held in reset)."""
    cmds = ["init", "irscan xc3s.tap 0x03"]
    for i in range(n + 1):
        a = start + 4 * min(i, n - 1)
        cmds.append(f"echo \"w [{user2_read(0x60000000 | (a & 0xFFFFC))}]\"")
    cmds.append("shutdown")
    out = run_tcl(cmds)
    vals = [scan_value(l) for l in out.splitlines() if l.startswith("w ")]
    return vals[1:n + 1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image", nargs="?")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x1000)
    ap.add_argument("--entry", type=lambda s: int(s, 0))
    ap.add_argument("--no-clear", action="store_true")
    ap.add_argument("--status", action="store_true")
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--dump", nargs=2, type=lambda s: int(s, 0))
    ap.add_argument("--trace", type=int, metavar="N",
                    help="print T1's first N bus accesses (of 512) since its reset")
    a = ap.parse_args()

    if a.trace:
        cmds = ["init", "irscan xc3s.tap 0x03"]
        for i in range(a.trace + 1):
            k = min(i, a.trace - 1)
            cmds.append(f"echo \"w [{user2_read(0x70000000 | (4 * k))}]\"")
        cmds.append("shutdown")
        vals = [scan_value(l) for l in run_tcl(cmds).splitlines() if l.startswith("w ")][1:]
        for v in vals:
            kind = "W" if v >> 30 & 1 else "R"
            print(f"[t1] {v >> 8 & 0x3FFFFF:06x} {kind} {v & 0xFF:02x}")
        return

    if a.verify or a.dump:
        if a.dump:
            start, n, want = a.dump[0] & ~3, a.dump[1], None
        else:
            data = open(a.image, "rb").read()
            data += b"\0" * (-len(data) % 4)
            want = [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]
            start, n = a.base, len(want)
        got = read_words(start, n)
        bad = 0
        for i, w in enumerate(got):
            if want is None:
                print(f"{start + 4 * i:06x}: {w:08x}")
            elif w != want[i]:
                bad += 1
                if bad <= 20:
                    print(f"{start + 4 * i:06x}: got {w:08x}, want {want[i]:08x}")
        if want is not None:
            print(f"verify: {n - bad} of {n} words match")
        return

    cmds = ["init"]
    if a.status or not a.image:
        cmds += read_status()
    else:
        data = open(a.image, "rb").read()
        data += b"\0" * (-len(data) % 4)
        words = [int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4)]
        entry = a.entry if a.entry is not None else a.base
        cmds += user2(0x40000000 | (a.base & 0xFFFFC))      # hold
        cmds += load(a.base, words)
        if not a.no_clear:
            cmds += load(0x0FC000, [0] * 1000)
        cmds += user2(0xC0000000 | entry)                   # run
        cmds += ["sleep 100"] + read_status()
    cmds += ["shutdown"]

    tcl = os.path.join(FPGA, "build", "jtagload.tcl")
    os.makedirs(os.path.dirname(tcl), exist_ok=True)
    open(tcl, "w").write("\n".join(cmds) + "\n")
    r = subprocess.run([openocd(), "-f", os.path.join(FPGA, "openocd", "opella-ld.cfg"),
                        "-f", os.path.join(FPGA, "openocd", "s3starter.cfg"), "-f", tcl],
                       capture_output=True, text=True)
    out = r.stdout + r.stderr
    status = [l for l in out.splitlines() if l.startswith("status")]
    if r.returncode or not status:
        sys.exit(out)
    if a.image and not a.status:
        print(f"loaded {len(data)} bytes at 0x{a.base:06x}, running from 0x{entry:06x}")
    print(decode(status[-1]))
    last = [l for l in out.splitlines() if l.startswith("lastacc")]
    if last:
        v = int(last[-1].split()[-1], 16)
        kind = ("write" if v >> 31 else "read") + (" I/O" if v >> 30 & 1 else "")
        print(f"last bus access: {kind} {v >> 8 & 0xFFFFF:05x} = {v & 0xFF:02x}")


if __name__ == "__main__":
    main()
