"""Check the HDMI audio that tang/rtl/dvi_tx.v (AUDIO=1) puts on the wire, from
sim/hdmiaudio_tb.v's dump of the three channels' 10-bit words, one pixel a
line (hex "ch0 ch1 ch2").

    make sim TOP=hdmiaudio SIMARGS="+frames=5"      # writes build/hdmiaudio.words (3 min)
    python3 tools/hdmi_audio_check.py build/hdmiaudio.words

It decodes the TMDS control, guard and TERC4 words and checks:
  - every data island: 8-pixel data-island preamble, 2-pixel leading guard,
    whole 32-pixel packets (at most 18, start flag only on pixel 0), 2-pixel
    trailing guard, then control; control period before the preamble
  - the BCH ECC of every packet header and subpacket (the same polynomial as
    tools/avi_packet.py, here a separate bytewise implementation)
  - the Audio Clock Regeneration packets: N = 6144, CTS = 74250, four equal
    subpackets, about one every 8 lines
  - the AVI and Audio InfoFrames: checksum, once a frame, the fields
  - the Audio Sample packets: layout, present and B flags (B on every 192nd
    frame), L/R 16-bit samples, V = U = 0, the channel status bits (consumer,
    L-PCM, 48 kHz, 16 bits), IEC 60958 parity; absent subpackets are zero
  - the samples are the 440/660 Hz test tone of hdmi_audio_tone.v, in
    order, with no sample lost or repeated after the FIFO fills (leading
    zeros are the FIFO's silence)
  - the samples between AVI InfoFrames (one per frame) average 800
  - video preamble and guard band on every line
"""
import math
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from avi_packet import ecc as ecc_serial   # used only to cross-check the AVI constants

TERC4 = ["1010011100", "1001100011", "1011100100", "1011100010", "0101110001", "0100011110",
         "0110001110", "0100111100", "1011001100", "0100111001", "0110011100", "1011000110",
         "1010001110", "1001110001", "0101100011", "1011000011"]
TERC4 = {int(t, 2): n for n, t in enumerate(TERC4)}
CTRL = {0b1101010100: 0, 0b0010101011: 1, 0b0101010100: 2, 0b1010101011: 3}   # {c1, c0}
GB = 0b0100110011                       # channel 1 guard; channel 2 in an island
VGB = 0b1011001100                      # video guard, channels 0 and 2
PIX_PER_LINE, LINES = 1650, 750

# the same BCH(32,24)/(64,56), x^8+x^7+x+1, by polynomial division on bytes
def bch(data):
    """data: bytes, LSB of byte 0 first on the wire; returns the ECC byte."""
    reg = 0
    for byte in data:
        for k in range(8):
            fb = ((reg ^ (byte >> k)) & 1)
            reg >>= 1
            if fb:
                reg ^= 0x83
    return reg


CS = bytearray(24)                      # consumer, L-PCM, no copyright; 48 kHz; 16 bits of 20
CS[0], CS[3], CS[4] = 0x04, 0x02, 0x02


def cs_bit(n):
    return (CS[n // 8] >> (n % 8)) & 1


def tone_model(n):
    """hdmi_audio_tone.v's n-th (left, right) sample."""
    def tab(k):
        return round(12000 * math.sin((k + 0.5) * math.pi / 512))

    def sine(ph10):
        a = ~ph10 & 255 if ph10 & 256 else ph10 & 255
        v = tab(a)
        return -v if ph10 & 512 else v
    pl = (n * 39370534) & 0xFFFFFFFF
    pr = (n * 59055800) & 0xFFFFFFFF
    return sine(pl >> 22), sine(pr >> 22)


# NOTONE=1 in the environment: the samples are not the test tone (a dump that
# starts mid-stream, from tang/sim/system_tb.v +words=FILE): check only the protocol
TONE = not os.environ.get("NOTONE")


def main(path):
    ch = [[], [], []]
    for line in open(path):
        w = line.split()
        for c in range(3):
            ch[c].append(int(w[c], 16) if "x" not in w[c].lower() else -1)   # X: before the pipeline fills
    n = len(ch[0])
    c0, c1, c2 = ch
    errs = []

    def err(msg):
        if len(errs) < 20:
            errs.append(msg)
        errs.append(None)

    # ---- video preambles and guard bands -------------------------------------
    vguards = 0
    for i in range(4000, n - 2):         # (the first lines are the pipeline filling)
        if c0[i] == VGB and c2[i] == VGB and c1[i] == GB and c0[i - 1] != VGB:
            vguards += 1
            if c0[i + 1] != VGB or c1[i + 1] != GB or c2[i + 1] != VGB:
                err("video guard band is not 2 pixels at %d" % i)
            if not all(c1[j] == 0b0010101011 and c2[j] == 0b1101010100 for j in range(i - 8, i)):
                err("video preamble wrong before %d" % i)

    # ---- data islands -------------------------------------------------------------
    packets = []                        # (pixel, header bytes, [subpacket bytes x4], flags)
    islands = 0
    max_pk = 0
    min_ctl = 10**9
    i = 12
    while i < n - 2:
        if (c1[i] == GB and c2[i] == GB and TERC4.get(c0[i], 0) >= 12
                and all(c1[j] == 0b0010101011 and c2[j] == 0b0010101011 for j in range(i - 8, i))):
            islands += 1
            j = i - 9
            while j >= 0 and c1[j] in CTRL:
                j -= 1
            min_ctl = min(min_ctl, i - 8 - 1 - j)
            if not (c1[i + 1] == GB and c2[i + 1] == GB and TERC4.get(c0[i + 1], 0) >= 12):
                err("leading guard is not 2 pixels at %d" % i)
            hs_pre = CTRL.get(c0[i - 1], -1) & 1
            p = i + 2
            npk = 0
            while c1[p] != GB:
                if npk == 18:
                    err("more than 18 packets in the island at %d" % i)
                    break
                h = bytearray(4)
                sb = [bytearray(8) for _ in range(4)]
                for k in range(32):
                    d0, d1, d2 = TERC4.get(c0[p + k]), TERC4.get(c1[p + k]), TERC4.get(c2[p + k])
                    if d0 is None or d1 is None or d2 is None:
                        err("non-TERC4 word in a packet at %d" % (p + k))
                        d0 = d1 = d2 = 0
                    if (d0 >> 3) != (1 if k else 0):
                        err("packet start flag wrong at %d" % (p + k))
                    if k == 0 and npk == 0 and (d0 & 1) != hs_pre:
                        err("hsync changes between preamble and island at %d" % p)
                    h[k // 8] |= ((d0 >> 2) & 1) << (k % 8)
                    for s in range(4):
                        sb[s][(2 * k) // 8] |= ((d1 >> s) & 1) << ((2 * k) % 8)
                        sb[s][(2 * k + 1) // 8] |= ((d2 >> s) & 1) << ((2 * k + 1) % 8)
                if bch(h[:3]) != h[3]:
                    err("header ECC wrong at %d" % p)
                for s in range(4):
                    if bch(sb[s][:7]) != sb[s][7]:
                        err("subpacket %d ECC wrong at %d" % (s, p))
                packets.append((p, bytes(h[:3]), [bytes(x[:7]) for x in sb]))
                p += 32
                npk += 1
            if not (c1[p + 1] == GB and c2[p + 1] == GB and TERC4.get(c0[p], 0) >= 12
                    and TERC4.get(c0[p + 1], 0) >= 12 and c2[p] == GB):
                err("trailing guard is not 2 pixels at %d" % p)
            if c1[p + 2] not in CTRL:
                err("no control period after the island at %d" % p)
            max_pk = max(max_pk, npk)
            i = p + 2
        else:
            i += 1

    # ---- packets ---------------------------------------------------------------------
    acr, avi, aif, smp_pk = [], [], [], 0
    samples = []                        # (pixel, l, r)
    avi_at = []
    b_seen = False
    frame = 0                           # frame index in the 192-frame block of the next sample
    for (p, h, sb) in packets:
        t = h[0]
        if t == 0x01:
            acr.append(p)
            if h[1] or h[2]:
                err("ACR header at %d" % p)
            if len(set(sb)) != 1:
                err("ACR subpackets differ at %d" % p)
            b = sb[0]
            cts = ((b[1] & 15) << 16) | (b[2] << 8) | b[3]
            nn = ((b[4] & 15) << 16) | (b[5] << 8) | b[6]
            if b[0] or (nn, cts) != (6144, 74250):
                err("ACR N/CTS = %d/%d at %d" % (nn, cts, p))
        elif t in (0x82, 0x84):
            length = h[2]
            pb = sb[0] + sb[1] + sb[2] + sb[3]
            if (sum(h) + sum(pb[:length + 1])) & 255:
                err("InfoFrame 0x%02X checksum at %d" % (t, p))
            if any(pb[length + 1:]):
                err("InfoFrame 0x%02X: bytes past its length at %d" % (t, p))
            if t == 0x82:
                avi_at.append(p)
                if (h[1], length) != (2, 13) or pb[1:6] != bytes([0x12, 0x28, 0x88, 4, 0]):
                    err("AVI InfoFrame fields at %d" % p)
                if bch(h[:3]) != 0xE4 or ecc_serial([(x >> k) & 1 for x in h[:3] for k in range(8)]) != 0xE4:
                    err("AVI header ECC differs from the known E4")
                if sb[0] != bytes.fromhex('A912288804 0000'.replace(' ', '')) or bch(sb[0]) != 0x10:
                    err('AVI subpacket 0 differs from the known one (ECC 10)')
                avi.append(p)
            else:
                if (h[1], length) != (1, 10) or pb[1] != 0x01 or any(pb[2:11]):
                    err("Audio InfoFrame fields at %d" % p)
                aif.append(p)
        elif t == 0x02:
            smp_pk += 1
            present, layout = h[1] & 15, (h[1] >> 4) & 1
            bflags, flat = h[2] >> 4, h[2] & 15
            if h[1] >> 5 or layout or flat:
                err("sample packet header %s at %d" % (h.hex(), p))
            if present not in (1, 3, 7, 15):
                err("sample packet present bits %x at %d" % (present, p))
            for s in range(4):
                b = sb[s]
                if not (present >> s) & 1:
                    if any(b) or (bflags >> s) & 1:
                        err("absent subpacket not zero at %d" % p)
                    continue
                l = b[1] | (b[2] << 8)
                r = b[4] | (b[5] << 8)
                if b[0] or b[3]:
                    err("sample low byte not zero at %d" % p)
                if not b_seen and (bflags >> s) & 1:
                    b_seen, frame = True, 0
                if b_seen:
                    if bool((bflags >> s) & 1) != (frame == 0):
                        err("B flag wrong (frame %d) at %d" % (frame, p))
                    cbit = cs_bit(frame)
                    for side, v, sh in (("L", l, 0), ("R", r, 4)):
                        bits = (b[6] >> sh) & 15
                        if bits & 1 or bits & 2:        # V, U
                            err("V/U set at %d" % p)
                        if ((bits >> 2) & 1) != cbit:
                            err("channel status bit wrong (frame %d) at %d" % (frame, p))
                        if (bin(v).count("1") + ((bits >> 1) & 1) + ((bits >> 2) & 1) + (bits & 1)
                                + (bits >> 3)) & 1:
                            err("parity wrong (%s, frame %d) at %d" % (side, frame, p))
                    frame = (frame + 1) % 192
                elif TONE:
                    err("a sample before the first B flag at %d" % p)
                sg = lambda v: v - 65536 if v & 0x8000 else v
                samples.append((p, sg(l), sg(r)))
        else:
            err("unknown packet type 0x%02X at %d" % (t, p))

    # ---- the tone: no sample lost or repeated -------------------------------------
    first = next((k for k, s in enumerate(samples) if s[1] or s[2]), None)
    nz = 0
    if not TONE:
        pass                            # NOTONE=1: any samples (a dump from the system)
    elif first is None:
        err("only silence")
    else:
        nz = first
        if any(s[1] or s[2] for s in samples[:first]):
            err("?")
        for k in range(first, len(samples)):
            if samples[k][1:] != tone_model(k - first):
                err("sample %d is %s, the tone has %s" % (k, samples[k][1:], tone_model(k - first)))
                break

    # ---- rates ---------------------------------------------------------------------------
    per_frame = []
    for a, b in zip(avi_at, avi_at[1:]):
        per_frame.append(sum(1 for s in samples if a <= s[0] < b))
    if not per_frame or sum(per_frame) / len(per_frame) != 800:
        err("samples per frame %s, want 800 on average" % per_frame)
    for a, b in zip(avi_at, avi_at[1:]):
        if b - a != PIX_PER_LINE * LINES:
            err("AVI InfoFrames %d pixels apart" % (b - a))
        if not any(a < x < b for x in aif):
            err("no Audio InfoFrame between AVIs at %d" % a)
    lines = n / PIX_PER_LINE
    print("%d pixels (%.1f frames), %d islands (max %d packets, min control period before one %d), "
          "%d video guard bands" % (n, n / (PIX_PER_LINE * LINES), islands, max_pk, min_ctl, vguards))
    print("packets: %d ACR (one per %.1f lines), %d AVI, %d Audio InfoFrame, %d sample packets "
          "with %d samples (%d leading silent)" % (len(acr), lines / max(1, len(acr)), len(avi), len(aif),
                                                   smp_pk, len(samples), nz))
    print("samples between AVI InfoFrames:", per_frame)
    bad = [e for e in errs if e]
    for e in bad:
        print("FAIL:", e)
    nerr = sum(1 for e in errs if e is None)
    print("FAIL: %d errors" % nerr if nerr else "PASS")
    return 1 if nerr else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/hdmiaudio.words"))
