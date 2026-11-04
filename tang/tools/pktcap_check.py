"""Check HDMI data islands captured on the board by a -DPKTCAP build of
tang/rtl/system.v: the console's "#P xxxxxxxx" lines (from tools/run.py's
output, or a file of it), 8192 words a capture.

    python3 tools/pktcap_check.py console.log

A word {2'b0f, ch2, ch1, ch0} is an island's pixel (f: its first), and
{2'b10, 6'd0, gap} follows each island: the pixels from the previous
island's start to its own. Each capture is rebuilt as a pixel stream
(control periods between the islands) and checked by hdmi_audio_check.py
(preambles, guards, TERC4, packet ECC, ACR, InfoFrames, sample packets:
layout, B flags, channel status, parity). Not checked: the samples per
frame (a capture is under a frame), video guard bands (not captured).
Also printed: the island spacing, which must be whole lines (1650 pixels).
"""
import os
import re
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
os.environ["NOTONE"] = "1"
import hdmi_audio_check as hac

CTL0 = 0b1101010100
N = 8192


def captures(path):
    words = []
    for line in open(path, errors="replace"):
        # every whole word on the line (a dump cut short can leave "#P 2E2B9#P 4AB2AEAB")
        for m in re.finditer(r"#P ([0-9A-Fa-f]{8})(?![0-9A-Fa-f])", line):
            words.append(int(m.group(1), 16))
    # a log can start inside a dump (the serial link joined late): each
    # capture starts with an island's first word (2'b01), so take the first
    # offset where every capture boundary has one
    for off in range(min(N, len(words))):
        if all(words[b] >> 30 == 1 for b in range(off, len(words), N)):
            if off:
                print("(the first %d words are the end of an earlier dump: skipped)" % off)
            return [words[k:k + N] for k in range(off, len(words), N)]
    return [words[k:k + N] for k in range(0, len(words), N)]


def rebuild(cap):
    """the pixel stream: [(c0, c1, c2)], and the island gaps"""
    isl, cur, gaps = [], None, []
    for w in cap:
        t = w >> 30
        if t in (0, 1):
            if t == 1 or cur is None:
                cur = []
                isl.append(cur)
            cur.append((w & 1023, (w >> 10) & 1023, (w >> 20) & 1023))
        elif t == 2:
            gaps.append(w & 0xFFFFFF)
            cur = None
    # a gap word follows its island: gaps[k] is island k's start minus island k-1's
    out = [(CTL0, CTL0, CTL0)] * 4000
    start = len(out)
    for k, px in enumerate(isl):
        if k > 0:
            g = gaps[k] if k < len(gaps) else None
            if g is None:
                break                       # the last island, cut off by the full buffer
            want = start + g
            if want < len(out):
                print("  island %d overlaps the previous one (gap %d)" % (k, g))
                want = len(out)
            out += [(CTL0, CTL0, CTL0)] * (want - len(out))
            start = want
        out += px
    out += [(CTL0, CTL0, CTL0)] * 4
    return out, gaps[1:len(isl)]


def main(path):
    caps = captures(path)
    if not caps:
        print("no #P lines")
        return 1
    bad = 0
    for n, cap in enumerate(caps):
        print("capture %d: %d words%s" % (n, len(cap), "" if len(cap) == N else " (cut short)"))
        px, gaps = rebuild(cap)
        odd = sorted(set(g for g in gaps if g % 1650))
        print("  island spacing (pixels): %s%s" % (sorted(set(gaps))[:8], " NOT whole lines: %s" % odd if odd else ""))
        with tempfile.NamedTemporaryFile("w", suffix=".words", delete=False) as f:
            for c0, c1, c2 in px:
                f.write("%03x %03x %03x\n" % (c0, c1, c2))
            name = f.name
        # the checker's per-frame rate check cannot hold for a capture
        import io, contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            hac.main(name)
        os.unlink(name)
        fails = [l for l in buf.getvalue().splitlines()
                 if l.startswith("FAIL:") and "samples per frame" not in l and "errors" not in l]
        for l in buf.getvalue().splitlines():
            if not l.startswith("FAIL") and not l.startswith("PASS") and "video guard" not in l:
                print("  " + l)
        for l in fails:
            print("  " + l)
        print("  %s" % ("FAIL" if fails else "PASS"))
        bad += bool(fails)
        if odd:
            bad += 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
