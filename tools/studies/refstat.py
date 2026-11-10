#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Roadmap 8.6: measure a record (or our render) the same way, to compare. Prints: tempo, key, the long-term balance
by octave band (dB against the whole), the side/mid ratio by band (stereo), the crest factor and the level.
Usage: refstat.py FILE [FILE ...] (any format ffmpeg reads). Numbers only: no audio is kept."""
import subprocess, sys, numpy as np

SR = 22050

def load(p):
    raw = subprocess.run(['ffmpeg', '-loglevel', 'error', '-i', p, '-ac', '2', '-ar', str(SR), '-f', 'f32le', '-'],
                         capture_output=True).stdout
    x = np.frombuffer(raw, np.float32).reshape(-1, 2)
    return x

def tempo(m):
    hop = 256
    n = len(m) // hop
    fr = m[:n * hop].reshape(n, hop)
    spec = np.abs(np.fft.rfft(fr * np.hanning(hop), axis=1))
    flux = np.maximum(np.diff(np.log1p(spec * 10), axis=0), 0).sum(1)
    flux -= flux.mean()
    ac = np.correlate(flux, flux, 'full')[len(flux) - 1:]
    fps = SR / hop
    bpm = np.arange(55, 100, 0.25)          # (reggae: a half-time reading, as the bar is felt)
    lag = fps * 60 / bpm
    sc = np.interp(lag, np.arange(len(ac)), ac) + 0.5 * np.interp(lag * 2, np.arange(len(ac)), ac)
    return bpm[np.argmax(sc)]

MAJ = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MIN = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17])
NAMES = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']

def key(m):
    N = 8192
    ch = np.zeros(12)
    f = np.fft.rfftfreq(N, 1 / SR)
    ok = (f > 55) & (f < 2000)
    pc = (np.round(12 * np.log2(f[ok] / 440.0)) + 9) % 12
    for i in range(0, len(m) - N, N):
        S = np.abs(np.fft.rfft(m[i:i + N] * np.hanning(N)))[ok]
        np.add.at(ch, pc.astype(int), S)
    best = max(((np.corrcoef(np.roll(t, k), ch)[0, 1], NAMES[k] + q) for t, q in ((MAJ, ''), (MIN, 'm'))
                for k in range(12)))
    return best[1], best[0]

BANDS = [(31, 63), (63, 125), (125, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000)]

def bands(x):
    N = 4096
    f = np.fft.rfftfreq(N, 1 / SR)
    P = np.zeros(len(f)); Ps = np.zeros(len(f))
    w = np.hanning(N)
    for i in range(0, len(x) - N, N // 2):
        mid = (x[i:i + N, 0] + x[i:i + N, 1]) * 0.5
        side = (x[i:i + N, 0] - x[i:i + N, 1]) * 0.5
        P += np.abs(np.fft.rfft(mid * w)) ** 2
        Ps += np.abs(np.fft.rfft(side * w)) ** 2
    tot = P.sum()
    out = []
    for lo, hi in BANDS:
        s = (f >= lo) & (f < hi)
        out.append((10 * np.log10(P[s].sum() / tot + 1e-12), 10 * np.log10(Ps[s].sum() / (P[s].sum() + 1e-12) + 1e-12)))
    return out

for p in sys.argv[1:]:
    x = load(p)
    m = x.mean(1)
    rms = np.sqrt(np.mean(m ** 2))
    pk = np.abs(m).max()
    # crest over 400 ms windows, the median: the punch the mix keeps
    w = int(0.4 * SR)
    cr = [20 * np.log10(np.abs(m[i:i + w]).max() / (np.sqrt(np.mean(m[i:i + w] ** 2)) + 1e-9))
          for i in range(0, len(m) - w, w) if np.abs(m[i:i + w]).max() > pk * 0.05]
    k, kc = key(m)
    print(f"{p}: {len(m) / SR:.0f} s, tempo {tempo(m):.1f} BPM, key {k} (r {kc:.2f}), rms {20 * np.log10(rms):.1f} dBFS, "
          f"crest (median, 400 ms) {np.median(cr):.1f} dB")
    print('  band Hz       ' + ' '.join(f'{lo:>5d}' for lo, _ in BANDS))
    b = bands(x)
    print('  balance dB    ' + ' '.join(f'{v:5.1f}' for v, _ in b))
    print('  side/mid dB   ' + ' '.join(f'{s:5.1f}' for _, s in b))
