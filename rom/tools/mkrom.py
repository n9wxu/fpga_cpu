#!/usr/bin/env python3
"""mkrom: the Tang Nano 20K's flash image of the ROM (docs/rom.md).

usage: mkrom.py ROM.bin [--load 0xF80000] [--entry ADDR] [--version N]
                [--file NAME=PATH[@LOAD[,ENTRY]] ...] -o OUT.img [--hex OUT.hex]

Writes the bytes from flash offset 0x400000 on:
  0x400000  header, 32 bytes, little endian: "FCRM", body length, load address, entry,
            CRC-32 of the body, version, the ROM disk's flash offset, 0
  0x400020  the body: ROM.bin (fcc output linked at the load address), copied by the FPGA's
            boot copier (tang/rtl/spiflash.v) to the load address at power-up
  0x480000  the ROM disk (only with --file): "FCRD", file count, 0, 0, then 32-byte entries
            (name, 16 bytes NUL-padded; offset from the disk's start; length; load; entry) and
            the files, each at a multiple of 16. The BIOS loads a file through the flash port.
--hex writes the same bytes one hex byte a line, for tang/sim/system_tb.v +flash=.
"""
import argparse
import struct
import sys
import zlib

ROM_OFS = 0x400000
DISK_OFS = 0x480000
HDR = 32


def num(s):
    return int(s, 0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("--load", type=num, default=0xF80000)
    ap.add_argument("--entry", type=num)
    ap.add_argument("--version", type=num, default=1)
    ap.add_argument("--file", action="append", default=[])
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--hex")
    a = ap.parse_args()

    body = open(a.rom, "rb").read()
    if a.entry is None:
        a.entry = a.load
    if len(body) > DISK_OFS - ROM_OFS - HDR:
        sys.exit("mkrom: the ROM body is %d bytes, more than the %d before the ROM disk"
                 % (len(body), DISK_OFS - ROM_OFS - HDR))
    hdr = struct.pack("<4sIIIIIII", b"FCRM", len(body), a.load, a.entry,
                      zlib.crc32(body) & 0xFFFFFFFF, a.version, DISK_OFS, 0)
    img = bytearray(hdr + body)

    if a.file:
        files = []
        for spec in a.file:
            name, rest = spec.split("=", 1)
            path, _, where = rest.partition("@")
            load, _, entry = where.partition(",")
            load = num(load) if load else 0x1000
            entry = num(entry) if entry else load
            if len(name.encode()) > 15:
                sys.exit("mkrom: file name %r is longer than 15 bytes" % name)
            files.append((name.encode(), open(path, "rb").read(), load, entry))
        disk = bytearray(struct.pack("<4sIII", b"FCRD", len(files), 0, 0))
        off = 16 + 32 * len(files)
        data = bytearray()
        for name, d, load, entry in files:
            off16 = (off + len(data) + 15) & ~15
            data += bytes(off16 - off - len(data))
            disk += struct.pack("<16sIIII", name, off16, len(d), load, entry)
            data += d
        disk += data
        img += bytes(DISK_OFS - ROM_OFS - len(img))
        img += disk
    open(a.out, "wb").write(img)
    if a.hex:
        with open(a.hex, "w") as fh:
            fh.write("".join("%02x\n" % b for b in img))
    print("mkrom: body %d bytes at %06x, entry %06x, %d file(s), image %d bytes"
          % (len(body), a.load, a.entry, len(a.file), len(img)))


if __name__ == "__main__":
    main()
