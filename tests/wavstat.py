#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""WAV analysis for the host tests (roadmap P0.7). Pure Python (the CI installs nothing for it).

  tests/wavstat.py FILE [CHECK ...]

Prints the metrics, then checks each CHECK, "metric<value" or "metric>value"; exits 1 if any fails.
Metrics (dBFS unless noted; -200 for silence):
  peak      largest |sample|, either side
  rms       RMS of the left side (L = R on the mono dry path)
  dc        |mean| of the left side
  click     largest |second difference| of the left side: a step or a discontinuity shows here; a sine at f
            and amplitude A gives A*(2 pi f / 44100)^2 at most (110 Hz at -20 dBFS: about -84)
  side_lo   RMS of (L - R)/2 below ~120 Hz: the low end must be mono (design principles)
  side      RMS of (L - R)/2, all of it: how much stereo there is
  tail      RMS of the last 250 ms (an echo that rings on, or dies)
  tail2     RMS of the last 2 s (the same, over a pulsing echo's gaps)
  head      RMS of the first 250 ms
  growth    tail - head in dB (feedback stability: a loop that runs away grows)
  seconds   the length (s)
  zc_hz     the pitch by zero crossings over the middle half (where the sound is over -60 dBFS): a kick's or a
            bass note's fundamental
  hf        the energy above ~4 kHz against all of it, dB (0: all highs; hats near it, a kick far under)
  fundHZ    the share of the energy at HZ Hz (a Goertzel bin, Hann window), dB: 0 all of it (a check only)
  crest     peak - rms, dB: a sine 3.0; saturation flattens it
  mod       the spread between the loudest and the quietest 20 ms window, dB, after the first 100 ms (a phaser's
            notches moving through a steady tone show here; a steady tone alone, about 0)
  pp        ping-pong: over the first three repeats (each repeat_ms long, after the first sound), how far each is
            to its side, L - R in dB, odd ones left and even ones right; the smallest of the three (0 if one is on
            the wrong side or there is no repeat)
  repeat_ms the time from the first sound's onset to the next one (an echo's repeat): where a 5 ms envelope
            rises past -40 dBFS, and again after it has fallen 6 dB under that (-1 if there is no second onset)
"""
import math
import struct
import sys
import wave


def db(x):
    return 20.0 * math.log10(x) if x > 1e-10 else -200.0


def read(path):
    with wave.open(path, "rb") as w:
        ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw == 2:
        v = struct.unpack("<%dh" % (len(raw) // 2), raw)
        full = 32768.0
    elif sw == 3:
        v = [int.from_bytes(raw[i:i + 3], "little", signed=True) for i in range(0, len(raw), 3)]
        full = 8388608.0
    elif sw == 4:
        v = struct.unpack("<%di" % (len(raw) // 4), raw)
        full = 2147483648.0
    else:
        raise SystemExit(f"wavstat: {path}: {sw * 8}-bit samples")
    left = [x / full for x in v[0::ch]]
    right = [x / full for x in v[1::ch]] if ch > 1 else left
    return rate, left, right


def rms(xs):
    return math.sqrt(sum(x * x for x in xs) / len(xs)) if xs else 0.0


def lowpass(xs, rate, fc):            # two one-pole low-passes: about 12 dB/oct above fc
    a = 1.0 - math.exp(-2.0 * math.pi * fc / rate)
    y1 = y2 = 0.0
    out = []
    for x in xs:
        y1 += a * (x - y1)
        y2 += a * (y1 - y2)
        out.append(y2)
    return out


def repeat_ms(xs, rate):
    a = 1.0 - math.exp(-1.0 / (0.005 * rate))
    env, e = [], 0.0
    for x in xs:
        e += a * (abs(x) - e)
        env.append(e)
    thr = 10 ** (-40 / 20.0)
    n = len(env)
    on1 = next((i for i, v in enumerate(env) if v > thr), None)
    if on1 is None:
        return -1.0
    i = on1
    while i < n and env[i] > thr / 2:          # the first sound over (6 dB under the threshold) ..
        i += 1
    while i < n and env[i] <= thr:             # .. and the next one past it
        i += 1
    return (i - on1) * 1000.0 / rate if i < n else -1.0


def onset(xs, rate):
    a = 1.0 - math.exp(-1.0 / (0.005 * rate))
    e, thr = 0.0, 10 ** (-40 / 20.0)
    for i, x in enumerate(xs):
        e += a * (abs(x) - e)
        if e > thr:
            return i
    return None


def pp_db(L, R, rate):
    top = max((abs(x) for x in L), default=0.0)
    if top <= 0.0:
        return 0.0
    Ln = [x / top for x in L]                  # (the onsets relative to the loudest: -40 dB under it)
    r = repeat_ms(Ln, rate)
    on = onset(Ln, rate)
    if r <= 0 or on is None:
        return 0.0
    k = int(r * rate / 1000.0)
    worst = 200.0
    for j in (1, 2, 3):
        a, b = on + j * k - k // 4, on + j * k + 3 * k // 4
        if b > len(L):
            return 0.0
        d = db(rms(L[a:b])) - db(rms(R[a:b]))
        d = d if j % 2 else -d
        if d <= 0:
            return 0.0
        worst = min(worst, d)
    return worst


def zc_hz(xs, rate):
    n = len(xs)
    seg = xs[n // 4: 3 * n // 4]
    thr = 10 ** (-60 / 20.0)
    last = max((i for i, x in enumerate(seg) if abs(x) > thr), default=-1)   # the sound's end in the window
    if last < 64:
        return -1.0
    seg = seg[:last + 1]
    crossings = sum(1 for a, b in zip(seg, seg[1:]) if (a < 0.0) != (b < 0.0))
    return crossings * rate / (2.0 * len(seg))


def highpass(xs, rate, fc):            # two one-pole high-passes
    a = math.exp(-2.0 * math.pi * fc / rate)
    out, x1, y1, x2, y2 = [], 0.0, 0.0, 0.0, 0.0
    for x in xs:
        y1 = a * (y1 + x - x1)
        x1 = x
        y2 = a * (y2 + y1 - x2)
        x2 = y1
        out.append(y2)
    return out


def mod_db(xs, rate):
    w = rate // 50
    wins = [db(rms(xs[i:i + w])) for i in range(rate // 10, len(xs) - w, w)]
    return max(wins) - min(wins) if wins else 0.0


def fund_db(xs, rate, f):              # the share of the energy at f (a Goertzel bin over the whole), in dB
    import math
    n = len(xs)
    if not n:
        return -200.0
    w = 2.0 * math.cos(2.0 * math.pi * f / rate)
    s1 = s2 = 0.0
    for i, x in enumerate(xs):
        win = 0.5 - 0.5 * math.cos(2.0 * math.pi * i / n)
        s1, s2 = x * win + w * s1 - s2, s1
    p = 8.0 * (s1 * s1 + s2 * s2 - w * s1 * s2) / (n * n)   # (a sine of amplitude a: |X| = a n / 4 with Hann; power a^2 / 2)
    e = sum(x * x for x in xs) / n
    return db(math.sqrt(p / e)) if e > 0 else -200.0


def metrics(path):
    rate, L, R = read(path)
    n = len(L)
    q = max(1, rate // 4)
    d2 = max((abs(L[i] - 2.0 * L[i - 1] + L[i - 2]) for i in range(2, n)), default=0.0)
    side = lowpass([(a - b) * 0.5 for a, b in zip(L, R)], rate, 120.0)
    head, tail = db(rms(L[:q])), db(rms(L[-q:]))
    return {
        "peak": db(max(max((abs(x) for x in L), default=0.0), max((abs(x) for x in R), default=0.0))),
        "rms": db(rms(L)),
        "dc": db(abs(sum(L) / n) if n else 0.0),
        "click": db(d2),
        "side_lo": db(rms(side)),
        "side": db(rms([(a - b) * 0.5 for a, b in zip(L, R)])),
        "tail": tail,
        "tail2": db(rms(L[-2 * rate:])),
        "head": head,
        "growth": tail - head,
        "seconds": n / rate,
        "repeat_ms": repeat_ms(L, rate),
        "pp": pp_db(L, R, rate),
        "zc_hz": zc_hz(L, rate),
        "hf": db(rms(highpass(L, rate, 4000.0))) - db(rms(L)) if rms(L) > 0 else -200.0,
        "mod": mod_db(L, rate),
        "crest": db(max((abs(x) for x in L), default=0.0)) - db(rms(L)),
    }


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    m = metrics(argv[1])
    print("wavstat: %s  " % argv[1] + "  ".join("%s %.1f" % (k, v) for k, v in m.items()))
    bad = 0
    for c in argv[2:]:
        op = "<" if "<" in c else ">"
        k, v = c.split(op)
        if k.startswith("fund") and k[4:].isdigit():   # fundHZ: the energy at HZ against the whole, dB (0: all of it)
            rate, L, _R = read(argv[1])
            m[k] = fund_db(L, rate, float(k[4:]))
            print("  %s %.1f" % (k, m[k]))
        if k not in m:
            print(f"FAIL wavstat: no metric {k}")
            bad += 1
            continue
        ok = m[k] < float(v) if op == "<" else m[k] > float(v)
        print(("  ok " if ok else "FAIL ") + "%s %s %s (%.1f)" % (k, op, v, m[k]))
        bad += not ok
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
