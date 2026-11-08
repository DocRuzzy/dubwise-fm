#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The built-in one-shots (roadmap P2.6), synthesized here (original sounds, nothing sampled), written as 8-bit
mu-law at 22,050 Hz into app/engine/oneshots_data.h. Re-run after a change:  python3 tools/make_oneshots.py
  1 BIM    a deejay's shout: a voiced "b" (a low murmur, the lips' burst, the formants gliding up out of it), a
           strained, shouted "ee", closing into an "m" (a hum); a sharp pulse train, the pitch up then down, saturated
  2 HORN   a sound-system horn stab: three detuned saws, an octave under, a fast attack, a short fall
  3 LASER  a sound-system zap: a sine swept down from 3 kHz with a fast vibrato, decaying
"""
import math
import random
from pathlib import Path

RATE = 22050


def mulaw(x):
    x = max(-1.0, min(1.0, x))
    y = math.copysign(math.log1p(255.0 * abs(x)) / math.log1p(255.0), x)
    return int(round((y + 1.0) * 127.5)) & 0xFF


def norm(s, peak=0.9, fade=0.010):
    """Scale to the peak and fade out over the last 10 ms (a shot never stops on a step: no click)."""
    m = max(abs(v) for v in s) or 1.0
    k = int(fade * RATE)
    return [v * peak / m * min(1.0, (len(s) - 1 - i) / k) for i, v in enumerate(s)]


def biquad_bp(x, f, q):
    w = 2 * math.pi * f / RATE
    a = math.sin(w) / (2 * q)
    b0, b2, a0, a1, a2 = a, -a, 1 + a, -2 * math.cos(w), 1 - a
    y, x1, x2, y1, y2 = [], 0.0, 0.0, 0.0, 0.0
    for v in x:
        o = (b0 * v + b2 * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, o
        y.append(o)
    return y


def bp_glide(x, f_of, q):
    """A band-pass whose centre moves (f_of(i) in Hz at sample i): a formant gliding, as the lips open."""
    y, x1, x2, y1, y2 = [], 0.0, 0.0, 0.0, 0.0
    for i, v in enumerate(x):
        w = 2 * math.pi * f_of(i) / RATE
        a = math.sin(w) / (2 * q)
        a0, a1, a2 = 1 + a, -2 * math.cos(w), 1 - a
        o = (a * v - a * x2 - a1 * y1 - a2 * y2) / a0
        x2, x1, y2, y1 = x1, v, y1, o
        y.append(o)
    return y


def bim():
    """A deejay's BIM: the lips shut (a voiced murmur), burst open, the vowel's formants glide up out of the "b"
    (F2 900 -> 2300 Hz: the cue that makes it a b), a strained, shouted "ee", then the lips shut again on an "m"."""
    n = int(0.32 * RATE)
    t_open, t_m, xf = 0.030, 0.19, 0.03
    src, ph = [], 0.0
    for i in range(n):
        t = i / RATE
        f0 = 280.0 + 60.0 * math.sin(math.pi * min(1.0, t / 0.12)) - 90.0 * max(0.0, t - 0.12) / 0.20   # up, then down
        f0 *= 1.0 + 0.012 * (random.random() - 0.5)     # a little rasp (jitter)
        ph += f0 / RATE
        if ph >= 1.0:
            ph -= 1.0
        g = (1.0 - ph) ** 6                             # a sharp, strained pulse: many harmonics
        if t < t_open:
            env = 0.22 * min(1.0, t / 0.006)            # the closure: a quiet voice bar
        else:
            env = min(1.0, (t - t_open) / 0.004) * (1.0 - (t - t_open) / (0.32 - t_open)) ** 0.45
        src.append((g - 0.14) * env + 0.03 * env * (random.random() - 0.5))
    k0 = int(t_open * RATE)
    glide = lambda lo, hi, dur: (lambda i: lo + (hi - lo) * min(1.0, max(0.0, (i - k0) / (dur * RATE))))
    vowel = [0.0] * n                                   # shouted "ee": F1 up a little (a shout opens the jaw)
    for f_of, q, gain in ((glide(220, 360, 0.030), 5, 1.0), (glide(900, 2300, 0.045), 8, 0.75),
                          (glide(2200, 3000, 0.045), 10, 0.45), (lambda i: 3600, 12, 0.25)):
        for i, v in enumerate(bp_glide(src, f_of, q)):
            vowel[i] += gain * v
    nasal = [0.9 * v for v in biquad_bp(src, 250, 4)]   # "m": a low hum
    murmur = biquad_bp(src, 180, 2)                     # the closure's voice bar: low only
    out = []
    for i in range(n):
        t = i / RATE
        if t < t_open:
            out.append(0.8 * murmur[i])
            continue
        m = min(1.0, max(0.0, (t - t_m) / xf))
        out.append((1.0 - m) * vowel[i] + m * nasal[i])
    burst = biquad_bp([random.random() - 0.5 for _ in range(int(0.008 * RATE))], 700, 0.9)   # the lips: low-weighted
    for i, v in enumerate(burst):
        out[k0 + i] += 3.0 * v * math.exp(-i / (0.0025 * RATE))
    peak = max(abs(v) for v in out) or 1.0
    return norm([math.tanh(2.2 * v / peak) for v in out])   # tape-like saturation: dense, punchy


def horn():
    n = int(0.55 * RATE)
    out, phs = [], [0.0, 0.0, 0.0, 0.0]
    fs = (233.1 * 0.995, 233.1, 233.1 * 1.006, 116.5)
    for i in range(n):
        t = i / RATE
        s = 0.0
        for k, f in enumerate(fs):
            phs[k] = (phs[k] + f / RATE) % 1.0
            s += (2.0 * phs[k] - 1.0) * (0.5 if k == 3 else 1.0)
        env = min(1.0, t / 0.01) * math.exp(-t * 4.0)
        out.append(s * env)
    return norm(biquad_bp(out, 1200, 0.7))


def laser():
    n = int(0.5 * RATE)
    out, ph = [], 0.0
    for i in range(n):
        t = i / RATE
        f = 3000.0 * math.exp(-t * 7.0) + 180.0 + 60.0 * math.sin(2 * math.pi * 31.0 * t)
        ph = (ph + f / RATE) % 1.0
        out.append(math.sin(2 * math.pi * ph) * math.exp(-t * 5.0) * min(1.0, t / 0.003))
    return norm(out)


def main():
    random.seed(7)
    shots = (("BIM", bim()), ("HORN", horn()), ("LASER", laser()))
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Copyright (C) 2026 Dubwise FM contributors */",
             "/* Generated by tools/make_oneshots.py: the built-in one-shots (P2.6), 8-bit mu-law at 22,050 Hz. Original,",
             " * synthesized sounds. Do not edit: re-run the script. */",
             "#pragma once", "#include <stdint.h>", "", "#define NSHOTS %d" % len(shots)]
    for name, s in shots:
        data = [mulaw(v) for v in s]
        lines.append("static const uint8_t SHOT_%s[%d] = {" % (name, len(data)))
        for k in range(0, len(data), 24):
            lines.append("    " + ", ".join(str(v) for v in data[k:k + 24]) + ",")
        lines.append("};")
    lines.append("static const uint8_t *const SHOT_DATA[NSHOTS] = {%s};" % ", ".join("SHOT_" + n for n, _ in shots))
    lines.append("static const uint32_t SHOT_LEN[NSHOTS] = {%s};" % ", ".join("sizeof SHOT_" + n for n, _ in shots))
    lines.append('static const char *const SHOT_NAME[NSHOTS] = {%s};' % ", ".join('"%s"' % n for n, _ in shots))
    Path(__file__).resolve().parent.parent.joinpath("app/engine/oneshots_data.h").write_text("\n".join(lines) + "\n")
    print("oneshots: %s" % ", ".join("%s %d B" % (n, len(s)) for n, s in shots))


if __name__ == "__main__":
    main()
