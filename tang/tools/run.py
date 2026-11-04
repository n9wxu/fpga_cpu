"""Load an fcc image into the Tang Nano 20K's T1 (tang/rtl/system.v) over
the USB serial port and run it, printing what it sends; ends with the
program's EXIT code (or Ctrl-C).

usage: run.py image.bin [--base 0x1000] [--port /dev/cu.usbserial-...1]
              [--data ADDR:FILE ...] [--tune ADDR:FILE ...] [--zero ADDR:N ...] [-i]

Extra loads are done before the image is started: --data puts a file at ADDR (hex or decimal),
--tune is --data plus the file's length as 4 little-endian bytes at ADDR+0xFFFC (the slot
convention of apps/sidplay: 64 KB slots), --zero writes N zero bytes at ADDR.
"""
import argparse
import glob
import select
import sys
import termios
import time
import tty

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x1000)
    ap.add_argument("--port")
    ap.add_argument("--data", action="append", default=[], metavar="ADDR:FILE", help="load FILE at ADDR before running")
    ap.add_argument("--tune", action="append", default=[], metavar="ADDR:FILE",
                    help="like --data, and store the file length (4 bytes LE) at ADDR+0xFFFC")
    ap.add_argument("--zero", action="append", default=[], metavar="ADDR:N", help="write N zero bytes at ADDR")
    ap.add_argument("-i", "--interactive", action="store_true",
                    help="send the terminal's keystrokes to the program (CON_IN) while it runs")
    ap.add_argument("--timeout", type=float, default=0, help="seconds; 0 = until EXIT")
    a = ap.parse_args()
    port = a.port or sorted(glob.glob("/dev/cu.usbserial-*1"))[-1]
    data = open(a.image, "rb").read()
    s = serial.Serial(port, 115200, timeout=0.1)
    s.write(bytes([0xDE, 0xAD, 0xBE, 0xEF]))          # hold T1
    time.sleep(0.05)
    s.reset_input_buffer()
    def load(addr, blob):
        s.write(b"L" + addr.to_bytes(3, "big") + len(blob).to_bytes(3, "big") + blob)

    def split(spec):
        addr, _, rest = spec.partition(":")
        return int(addr, 0), rest

    b, n = a.base, len(data)
    for spec in a.data:
        addr, fn = split(spec)
        load(addr, open(fn, "rb").read())
    for spec in a.tune:
        addr, fn = split(spec)
        blob = open(fn, "rb").read()
        load(addr, blob)
        load(addr + 0xFFFC, len(blob).to_bytes(4, "little"))
    for spec in a.zero:
        addr, cnt = split(spec)
        load(addr, bytes(int(cnt, 0)))
    load(b, data)
    s.write(b"G" + b.to_bytes(3, "big"))
    s.flush()
    t0 = time.time()
    eot = False
    old = None
    if a.interactive and sys.stdin.isatty():
        old = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
    try:
        while True:
            if old is not None and select.select([sys.stdin], [], [], 0)[0]:
                k = sys.stdin.read(1)
                if k:
                    s.write(k.encode("latin-1", "replace"))
            c = s.read(1)
            if a.timeout and time.time() - t0 > a.timeout:
                print("\n[run.py] timeout")
                return 1
            if not c:
                continue
            if eot:
                print(f"\n[run.py] exit code {c[0]}")
                return c[0]
            if c[0] == 4:
                eot = True
            else:
                sys.stdout.write(c.decode("latin-1"))
                sys.stdout.flush()
    except KeyboardInterrupt:
        return 130
    finally:
        if old is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)


if __name__ == "__main__":
    sys.exit(main())
