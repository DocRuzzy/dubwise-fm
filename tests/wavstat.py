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
  tail      RMS of the last 250 ms (an echo that rings on, or dies)
  head      RMS of the first 250 ms
  growth    tail - head in dB (feedback stability: a loop that runs away grows)
  seconds   the length (s)
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
        "tail": tail,
        "head": head,
        "growth": tail - head,
        "seconds": n / rate,
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
