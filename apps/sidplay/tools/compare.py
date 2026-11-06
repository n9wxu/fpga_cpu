#!/usr/bin/env python3
"""Cross-check sidmap.c: for one tune, each SID voice alone is rendered through the engine model
(host build -> script -> audio_ref.py) and through sidref.py, then compared per 100 ms window:
RMS-envelope correlation and rising-zero-crossing rate (pitch).
usage: compare.py tune.sid song seconds [workdir]"""
import os, subprocess, sys, wave, struct, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import audiorender, sidref

def read_wav(path):
    w = wave.open(path); n = w.getnframes(); ch = w.getnchannels()
    d = struct.unpack('<%dh' % (n * ch), w.readframes(n))
    return [x for x in d[0::ch]]

def feats(y, win=4800):
    out = []
    for s in range(0, len(y) - win + 1, win):
        seg = y[s:s + win]; m = sum(seg) / win
        rms = math.sqrt(sum((x - m) ** 2 for x in seg) / win)
        thr = 0.1 * max(abs(x - m) for x in seg) if rms else 0
        z, up = 0, False
        for x in seg:                              # rising crossings with hysteresis
            if x - m > thr and not up: z += 1; up = True
            elif x - m < -thr: up = False
        out.append((rms, z))
    return out

def corr(a, b):
    ma, mb = sum(a) / len(a), sum(b) / len(b)
    sa = math.sqrt(sum((x - ma) ** 2 for x in a)); sb = math.sqrt(sum((x - mb) ** 2 for x in b))
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / (sa * sb) if sa and sb else 0

def main():
    tune, song, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
    work = sys.argv[4] if len(sys.argv) > 4 else os.path.join(HERE, "..", "out")
    host = os.path.join(HERE, "..", "build", "sidplay_host")
    for v in range(3):
        mask = 1 << v
        env = dict(os.environ, SM_MUTE=str(7 ^ mask))
        sc, dp = os.path.join(work, "cmp.script"), os.path.join(work, "cmp.siddump")
        subprocess.run([host, tune, song, str(secs), sc, dp], env=env, check=True, stdout=subprocess.DEVNULL)
        e = [x for l, r in audiorender.render(sc, secs) for x in (l,)]
        r = sidref.render(dp, secs, mask, True)
        fe, fr = feats(e), feats(r)
        re_, rr = [f[0] for f in fe], [f[0] for f in fr]
        c = corr(re_, rr)
        ok = tot = 0; bad = []
        mr = max(rr) if rr else 0
        for i, (a, b) in enumerate(zip(fe, fr)):
            if b[0] > 0.15 * mr and a[0] > 0:
                tot += 1
                if b[1] and abs(a[1] - b[1]) <= max(2, 0.06 * b[1]): ok += 1
                elif len(bad) < 5: bad.append((round(i * 0.1, 1), a[1] * 10, b[1] * 10))
        gains = sorted(a[0] / b[0] for a, b in zip(fe, fr) if b[0] > 0.15 * mr and a[0] > 0)
        med = gains[len(gains) // 2] if gains else 0
        print("voice %d: RMS-envelope correlation %.3f, pitch (zero-crossing rate) match %d/%d windows, median gain engine/ref %.0f"
              % (v + 1, c, ok, tot, med), "mismatches (t, engine Hz, ref Hz):" if bad else "", bad if bad else "")
main()
