#!/usr/bin/env python3
"""A console on the Tang Nano 20K's serial port (115200), for the ROM (docs/rom.md).

usage: console.py [--prog FILE.fs] [--send TEXT] [--wait S] [--time S] [-i] [--port P]

Opens the port first, so nothing the board sends is missed; --prog then loads a bitstream into
the FPGA (SRAM, openFPGALoader) and the boot that follows is seen from its first byte. --send
types TEXT (\\n ends a line) after --wait seconds, a line at a time; --time ends the session
(default 10 s); -i passes the terminal's keystrokes through until Ctrl-] instead.
"""
import argparse
import glob
import os
import select
import subprocess
import sys
import termios
import time
import tty

import serial

OSS = os.path.expanduser("~/oss-cad-suite/bin")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--prog")
    ap.add_argument("--send", default="")
    ap.add_argument("--wait", type=float, default=3.0)
    ap.add_argument("--time", type=float, default=10.0)
    ap.add_argument("-i", "--interactive", action="store_true")
    ap.add_argument("--port")
    a = ap.parse_args()
    port = a.port or sorted(glob.glob("/dev/cu.usbserial-*1"))[-1]
    s = serial.Serial(port, 115200, timeout=0.05)
    s.reset_input_buffer()
    if a.prog:
        subprocess.run([os.path.join(OSS, "openFPGALoader"), "-b", "tangnano20k", a.prog],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    t0 = time.time()
    lines = [l + "\r" for l in a.send.replace("\\n", "\n").split("\n") if l] if a.send else []
    next_send = t0 + a.wait
    old = None
    if a.interactive and sys.stdin.isatty():
        old = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
    try:
        while a.interactive or time.time() - t0 < a.time:
            d = s.read(256)
            if d:
                sys.stdout.write(d.decode("latin-1").replace("\r", ""))
                sys.stdout.flush()
            if lines and time.time() >= next_send:
                for ch in lines.pop(0):          # a character at a time, as typed
                    s.write(ch.encode())
                    time.sleep(0.002)
                next_send = time.time() + 0.6
            if old is not None and select.select([sys.stdin], [], [], 0)[0]:
                k = sys.stdin.read(1)
                if k == "\x1d":
                    break
                s.write(k.encode("latin-1", "replace"))
    finally:
        if old is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)


if __name__ == "__main__":
    main()
