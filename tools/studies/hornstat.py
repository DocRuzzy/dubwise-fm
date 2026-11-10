#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Roadmap 8.6 / 7.7: how the horns in a record attack, measured the same way on our HORN. The harmonic part of the
mix is kept (a median-filter split of the spectrogram: horizontal lines are pitched, vertical are hits), then the
energy in the horns' range (500 Hz .. 3 kHz) is followed, and at each clear note start: the rise (10 % to the peak),
the overshoot (the peak against 150-350 ms later), and the brightness (the spectral centroid, 500 Hz .. 6 kHz, in
the first 30 ms against 200 ms on). Usage: hornstat.py FILE [START_S LEN_S]. Numbers only: no audio is kept."""
import subprocess, sys, numpy as np

SR, N, HOP = 22050, 1024, 128

def load(p, t0, dur):
    cmd = ['ffmpeg', '-loglevel', 'error', '-ss', str(t0), '-t', str(dur), '-i', p, '-ac', '1', '-ar', str(SR), '-f', 'f32le', '-']
    return np.frombuffer(subprocess.run(cmd, capture_output=True).stdout, np.float32)

def medfilt(a, k, axis):
    pad = [(0, 0), (0, 0)]
    pad[axis] = (k // 2, k // 2)
    ap = np.pad(a, pad, mode='edge')
    w = np.lib.stride_tricks.sliding_window_view(ap, k, axis=axis)
    return np.median(w, axis=-1)

def analyse(x):
    n = (len(x) - N) // HOP
    fr = np.stack([x[i * HOP:i * HOP + N] * np.hanning(N) for i in range(n)])
    S = np.abs(np.fft.rfft(fr, axis=1)).T                 # freq x time
    f = np.fft.rfftfreq(N, 1 / SR)
    H = medfilt(S, 17, 1)                                 # along time: pitched lines
    P = medfilt(S, 17, 0)                                 # along frequency: hits
    Hm = S * (H ** 2 / (H ** 2 + P ** 2 + 1e-12))
    band = (f >= 500) & (f <= 3000)
    E = np.sqrt((Hm[band] ** 2).sum(0))
    cb = (f >= 500) & (f <= 6000)
    cent = (Hm[cb] * f[cb, None]).sum(0) / (Hm[cb].sum(0) + 1e-12)
    fps = SR / HOP
    Ed = 20 * np.log10(E + 1e-9)
    floor = np.percentile(Ed, 30)
    res = []
    i = 5
    gap = int(0.25 * fps)
    while i < len(E) - int(0.4 * fps):
        # a start: the energy climbs 9 dB within 60 ms, from near the floor, to well above it
        j = i + int(0.06 * fps)
        if Ed[j] - Ed[i] > 9 and Ed[j] > floor + 15:
            pk = i + np.argmax(E[i:i + int(0.25 * fps)])
            lo = E[i]
            t10 = i + np.argmax(E[i:pk + 1] >= lo + 0.1 * (E[pk] - lo))
            rise = (pk - t10) / fps * 1000
            later = np.median(E[pk + int(0.15 * fps):pk + int(0.35 * fps)])
            over = 20 * np.log10(E[pk] / (later + 1e-9))
            c0 = cent[t10:t10 + int(0.03 * fps) + 1].mean()
            c1 = cent[t10 + int(0.2 * fps):t10 + int(0.23 * fps) + 1].mean()
            res.append((rise, over, c0, c1))
            i = pk + gap
        else:
            i += 1
    return res

if __name__ == '__main__':
    p = sys.argv[1]
    t0 = float(sys.argv[2]) if len(sys.argv) > 2 else 0
    dur = float(sys.argv[3]) if len(sys.argv) > 3 else 90
    r = np.array(analyse(load(p, t0, dur)))
    if not len(r):
        print(p, 'no clear starts'); sys.exit()
    q = lambda c: np.percentile(r[:, c], [25, 50, 75])
    print(f"{p}: {len(r)} starts | rise ms {q(0).round(0)} | overshoot dB {q(1).round(1)} | "
          f"centroid start {q(2).round(0)} | 200 ms on {q(3).round(0)} | start/on {np.median(r[:,2]/r[:,3]):.2f}")
