"""Make the island ROM for rtl/hdmiload.v -DREPLAY from a -DPKTCAP capture
(tools/pktcap_check.py's input): the islands' words in order, one a line,
{last, ch2, ch1, ch0} with bit 31 set on each island's last word and bit 30
on the last island's last word (the ROM loops). The capture's last island
(cut off by the full buffer) is dropped.

    python3 tools/pktcap_rom.py console.log N out.hex     (N: which capture, 0 first)
"""
import sys
sys.path.insert(0, __import__("os").path.dirname(__file__))
import pktcap_check as pc

caps = pc.captures(sys.argv[1])
cap = caps[int(sys.argv[2])]
isl, cur = [], None
for w in cap:
    t = w >> 30
    if t in (0, 1):
        if t == 1 or cur is None:
            cur = []
            isl.append(cur)
        cur.append(w & 0x3FFFFFFF)
    else:
        cur = None
GB = 0b0100110011


def whole(px):
    # 8 preamble + 2 guard + 32 per packet + 2 guard, ending in the guard
    return (len(px) - 12) % 32 == 0 and len(px) > 12 and all(
        (w >> 10) & 1023 == GB and (w >> 20) & 1023 == GB for w in px[-2:])


done = list(isl)
while done and not whole(done[-1]):
    done.pop()                              # the buffer filled during the last island
assert all(whole(px) for px in done), "a broken island inside the capture"
words = []
for k, px in enumerate(done):
    for j, w in enumerate(px):
        last = j == len(px) - 1
        words.append(w | (last << 31) | ((last and k == len(done) - 1) << 30))
assert len(words) <= 8192, len(words)
with open(sys.argv[3], "w") as f:
    for w in words + [0] * (8192 - len(words)):
        f.write("%08x\n" % w)
print("%d islands, %d words" % (len(done), len(words)))
