#!/usr/bin/env python3
"""The demo song's samples, synthesized (no recordings): samples/kick.wav and snare.wav (8-bit,
22050 Hz) and choir.wav (16-bit, 22000 Hz, a looped 'aah' at A-3: exactly 100 frames a period)."""
import math
import os
import random
import struct
import wave

HERE = os.path.dirname(os.path.abspath(__file__))


def write(name, rate, bits, xs):
    os.makedirs(os.path.join(HERE, "samples"), exist_ok=True)
    w = wave.open(os.path.join(HERE, "samples", name), "wb")
    w.setnchannels(1)
    w.setsampwidth(bits // 8)
    w.setframerate(rate)
    if bits == 8:
        w.writeframes(bytes(max(0, min(255, int(round(x * 127)) + 128)) for x in xs))
    else:
        w.writeframes(struct.pack("<%dh" % len(xs), *(max(-32768, min(32767, int(round(x * 32767)))) for x in xs)))
    w.close()


def kick():
    r, n, ph, out = 22050, 6600, 0.0, []
    for i in range(n):
        t = i / r
        f = 45 + 110 * math.exp(-t * 28)
        ph += 2 * math.pi * f / r
        out.append(0.95 * math.sin(ph) * math.exp(-t * 9) + (0.3 * math.exp(-t * 300) if i < 60 else 0))
    return r, out


def snare():
    rnd = random.Random(7)
    r, n, out = 22050, 4400, []
    lp = 0.0
    for i in range(n):
        t = i / r
        nz = rnd.uniform(-1, 1)
        lp += 0.6 * (nz - lp)
        out.append(0.75 * lp * math.exp(-t * 16) + 0.45 * math.sin(2 * math.pi * 185 * t) * math.exp(-t * 30))
    return r, out


def choir():
    """an 'aah': harmonics of 220 Hz weighted by two formants (700 and 1200 Hz); 2000 frames of
    onset (rising brightness) then a 1000-frame loop (10 periods)"""
    r, per = 22000, 100
    amp = []
    for h in range(1, 40):
        f = 220 * h
        a = math.exp(-((f - 700) / 260) ** 2) + 0.6 * math.exp(-((f - 1200) / 300) ** 2) + 0.25 / h
        amp.append((h, a, random.Random(h).uniform(0, 2 * math.pi)))
    peak = sum(a for _, a, _ in amp)
    out = []
    for i in range(3000):
        b = min(1.0, i / 2000)
        x = sum(a * (b if h > 2 else 1) * math.sin(2 * math.pi * h * i / per + p) for h, a, p in amp)
        out.append(0.9 * x / peak * (0.4 + 0.6 * b))
    return r, out


if __name__ == "__main__":
    for name, bits, (rate, xs) in (("kick.wav", 8, kick()), ("snare.wav", 8, snare()), ("choir.wav", 16, choir())):
        write(name, rate, bits, xs)
    print("samples/kick.wav, snare.wav, choir.wav")
