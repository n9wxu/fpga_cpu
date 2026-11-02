"""Print the HDMI AVI InfoFrame packet that tang/rtl/dvi_tx.v sends, as
Verilog constants: the header and subpackets with their BCH ECC bytes.

RGB, full range, 16:9, VIC 4 (1280x720p60), underscan (S = 2: show the whole
picture), IT content of type graphics (so TVs skip their processing).
"""


def ecc(bits):
    e = 0
    for b in bits:
        e = (e >> 1) ^ (0x83 if (e & 1) ^ b else 0)
    return e


def bits_of(data):
    return [(byte >> i) & 1 for byte in data for i in range(8)]


def main():
    hb = [0x82, 0x02, 0x0D]                  # AVI InfoFrame, version 2, 13 bytes
    pb = [0] * 28
    pb[1] = 0x12                             # RGB, active format present, underscan
    pb[2] = 0x28                             # 16:9, active format = picture
    pb[3] = 0x88                             # IT content, full-range RGB
    pb[4] = 4                                # VIC 4: 1280x720p60
    pb[5] = 0x00                             # content type: graphics
    pb[0] = (-(sum(hb) + sum(pb[1:14]))) & 0xFF
    hdr = hb + [ecc(bits_of(hb))]
    print("    localparam [31:0] AVI_HDR = 32'h%08X;" % int.from_bytes(bytes(hdr), "little"))
    for k in range(4):
        sb = pb[7 * k:7 * k + 7]
        sb = sb + [ecc(bits_of(sb))]
        print("    localparam [63:0] AVI_SB%d = 64'h%016X;" % (k, int.from_bytes(bytes(sb), "little")))


if __name__ == "__main__":
    main()
