"""Load an fcc image into the Tang Nano 20K's T1 (tang/rtl/t1tang.v) over
the USB serial port and run it, printing what it sends; ends with the
program's EXIT code (or Ctrl-C).

usage: run.py image.bin [--base 0x1000] [--port /dev/cu.usbserial-...1]
"""
import argparse
import glob
import sys
import time

import serial


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x1000)
    ap.add_argument("--port")
    ap.add_argument("--timeout", type=float, default=0, help="seconds; 0 = until EXIT")
    a = ap.parse_args()
    port = a.port or sorted(glob.glob("/dev/cu.usbserial-*1"))[-1]
    data = open(a.image, "rb").read()
    s = serial.Serial(port, 115200, timeout=0.1)
    s.write(bytes([0xDE, 0xAD, 0xBE, 0xEF]))          # hold T1
    time.sleep(0.05)
    s.reset_input_buffer()
    b, n = a.base, len(data)
    s.write(b"L" + b.to_bytes(3, "big") + n.to_bytes(3, "big") + data)
    s.write(b"G" + b.to_bytes(3, "big"))
    s.flush()
    t0 = time.time()
    eot = False
    try:
        while True:
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


if __name__ == "__main__":
    sys.exit(main())
