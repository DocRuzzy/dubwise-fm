#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The built-in one-shots (roadmap P2.6), synthesized here (original sounds, nothing sampled), written as 8-bit
mu-law at 22,050 Hz into app/engine/oneshots_data.h. Re-run after a change:  python3 tools/make_oneshots.py

Your own one-shots (roadmap 5.14), from WAV files, into an image the FM-1 takes (app/shots.c has its layout):
  python3 tools/make_oneshots.py user A.wav B.wav C.wav -o mine.dwshots [--names TAG,HORN,ZAP]
Three files for black keys 8, 9 and 10 (fewer leaves the rest silent). Any rate, mono or stereo (mixed down), 8, 16,
24 or 32-bit PCM; each is resampled to 22,050 Hz, its last 10 ms faded (no click), and kept as it is otherwise: no
level change, so set the levels you want in the WAVs. 1.5 s in all (33,075 bytes), split any way. Send the image with
tools/dubwise_slots.py shots put mine.dwshots (or give it the WAVs directly).
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


USER_MAX = 33075                                  # 1.5 s at 22,050 Hz, all three (app/engine/fxv.h FXV_SHOT_MAX)
SH_MAGIC = b"SHT1"


def read_wav(path):
    """A WAV file as floats -1..1 at 22,050 Hz, mono."""
    import struct
    import wave
    with wave.open(str(path), "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        xs = [(b - 128) / 128.0 for b in raw]
    elif width == 2:
        xs = [v / 32768.0 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    elif width == 3:
        xs = [int.from_bytes(raw[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, len(raw), 3)]
    elif width == 4:
        xs = [v / 2147483648.0 for v in struct.unpack("<%di" % (len(raw) // 4), raw)]
    else:
        raise SystemExit(f"{path}: {width * 8}-bit samples aren't PCM this tool reads")
    mono = [sum(xs[i:i + ch]) / ch for i in range(0, len(xs) - ch + 1, ch)]
    if rate == RATE or not mono:
        return mono
    out, step = [], rate / RATE                  # linear, with a box low-pass first when coming down
    if step > 1.0:
        k = int(step)
        mono = [sum(mono[max(0, i - k + 1):i + 1]) / min(k, i + 1) for i in range(len(mono))]
    t = 0.0
    while t < len(mono) - 1:
        i = int(t)
        f = t - i
        out.append(mono[i] * (1.0 - f) + mono[i + 1] * f)
        t += step
    return out


def user_shot(path):
    s = read_wav(path)
    k = min(len(s), int(0.010 * RATE))
    return bytes(mulaw(v * min(1.0, (len(s) - 1 - i) / k) if k else v) for i, v in enumerate(s))


def fnv1a(data, h=2166136261):
    for b in data:
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


def user_image(shots):
    """shots: up to three (name, mu-law bytes). The image app/shots.c takes: the 48-byte header, then the data."""
    import struct
    shots = list(shots)[:3]
    data, entries = b"", []
    for name, d in shots:
        entries.append((len(data), len(d), name))
        data += d
    if len(data) > USER_MAX:
        raise SystemExit("the shots are %.2f s together; the FM-1 takes 1.5 s (%s): shorten them" % (
            len(data) / RATE, ", ".join("%s %.2f s" % (n, len(d) / RATE) for n, d in shots)))
    while len(entries) < 3:
        entries.append((0, 0, ""))
    head = SH_MAGIC + struct.pack("<II", len(data), fnv1a(data))
    for off, n, name in entries:
        nm = "".join(c for c in name.upper() if " " <= c <= "~")[:8].ljust(8)
        head += struct.pack("<HH", off, n) + nm.encode("ascii")
    return head + data


def image_info(image):
    import struct
    if len(image) < 48 or image[:4] != SH_MAGIC:
        return "not a Dubwise one-shots image"
    n, s = struct.unpack_from("<II", image, 4)
    if n + 48 != len(image) or fnv1a(image[48:]) != s:
        return "a damaged one-shots image"
    parts = []
    for k in range(3):
        off, ln = struct.unpack_from("<HH", image, 12 + 12 * k)
        nm = image[16 + 12 * k:24 + 12 * k].decode("ascii", "replace").strip() or "SHOT %d" % (k + 1)
        parts.append("%s %.2f s" % (nm, ln / RATE) if ln else "(silent)")
    return "one-shots: " + ", ".join(parts) + " (%.2f of 1.5 s)" % (n / RATE)


def user_main(argv):
    import argparse
    ap = argparse.ArgumentParser(prog="make_oneshots.py user")
    ap.add_argument("wavs", nargs="+")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--names")
    o = ap.parse_args(argv)
    if len(o.wavs) > 3:
        raise SystemExit("three at most (black keys 8, 9 and 10)")
    names = o.names.split(",") if o.names else [Path(w).stem for w in o.wavs]
    image = user_image(zip(names, (user_shot(w) for w in o.wavs)))
    Path(o.out).write_bytes(image)
    print(f"{o.out}: {image_info(image)}")


def main():
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == "user":
        return user_main(sys.argv[2:])
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
