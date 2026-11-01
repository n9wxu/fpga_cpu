"""Turn a program image (fcc / asm.py output, loaded at --base) into
$readmemh data for sim/sram_model.v: 32-bit little-endian words with an
@ word address.

usage: bin2hex.py image.bin out.hex [--base 0x1000]
"""
import argparse


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("out")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0x1000)
    a = ap.parse_args()
    data = open(a.image, "rb").read()
    data += b"\0" * (-len(data) % 4)
    with open(a.out, "w") as f:
        f.write(f"@{a.base // 4:x}\n")
        for i in range(0, len(data), 4):
            f.write(f"{int.from_bytes(data[i:i + 4], 'little'):08x}\n")


if __name__ == "__main__":
    main()
