#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The SpIllt Beats mark for the firmware, from the owner's brush drawing (spillt-mark-drawing.jpeg).

The drawing's blue strokes are cut from its tan ground and kept as three tones (deep, mid, light: the brush's
light along its strokes), cleaned of the JPEG's speckle (and any ERASE), then scaled down by area: each pixel keeps
how much of it is ink (its coverage, 0..3: soft edges, drawn over any ground) and the ink's shade (0..3, deep to
light). Written as app/logo_data.h: each size a run-length list, one byte a run (shade << 6 | coverage << 4 |
length - 1, lengths 1..16).
Usage: python3 -I tools/logo/mklogo.py [preview.png]
"""
import os
import sys
from collections import deque

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "spillt-mark-drawing.jpeg")
OUT = os.path.join(HERE, "..", "..", "app", "logo_data.h")
SIZES = {"SPLASH": 0.70, "SAVER": 0.40}      # of the drawing's 238 x 205
# erasures, in the drawing's pixels: x0, y0, x1, y1 (inclusive). None: the owner's three (2026-11-08) were stubs of
# the old traced version (a top bar past the centre line, the S's lower gap closed, a bottom-bar stub), which the
# drawing itself doesn't have
ERASE = []


def tones():
    im = np.asarray(Image.open(SRC).convert("RGB")).astype(float)
    r, g, b = im[..., 0], im[..., 1], im[..., 2]
    ink = (b - r) > -10
    lum = 0.3 * r + 0.59 * g + 0.11 * b
    t = np.zeros(ink.shape, np.uint8)
    t[ink & (lum < 140)] = 1
    t[ink & (lum >= 140) & (lum < 172)] = 2
    t[ink & (lum >= 172)] = 3
    for x0, y0, x1, y1 in ERASE:
        t[y0:y1 + 1, x0:x1 + 1] = 0
    return t


def majority(t):
    """each pixel the tone most of its 3 x 3 has (ink or not first, then which tone): the speckle out"""
    h, w = t.shape
    p = np.pad(t, 1)
    stack = np.stack([p[dy:dy + h, dx:dx + w] for dy in range(3) for dx in range(3)])
    inkn = (stack > 0).sum(0)
    out = np.zeros_like(t)
    counts = np.stack([(stack == k).sum(0) for k in (1, 2, 3)])
    best = counts.argmax(0) + 1
    out[inkn >= 5] = best[inkn >= 5]
    return out


def drop_small(t, n):
    """ink islands under n pixels: gone (stray flecks)"""
    h, w = t.shape
    seen = np.zeros(t.shape, bool)
    for y in range(h):
        for x in range(w):
            if t[y, x] and not seen[y, x]:
                q, comp = deque([(y, x)]), []
                seen[y, x] = True
                while q:
                    cy, cx = q.popleft()
                    comp.append((cy, cx))
                    for ny, nx in ((cy + 1, cx), (cy - 1, cx), (cy, cx + 1), (cy, cx - 1)):
                        if 0 <= ny < h and 0 <= nx < w and t[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            q.append((ny, nx))
                if len(comp) < n:
                    for cy, cx in comp:
                        t[cy, cx] = 0
    return t


def scaled(t, k):
    """to scale k by area: (shade, coverage) a pixel, coverage 0 the ground"""
    ink = Image.fromarray(((t > 0) * 255).astype(np.uint8))
    shade = Image.fromarray((np.where(t > 0, t - 1, 0) * 127).astype(np.uint8))   # 0, 127, 254
    h, w = t.shape
    H, W = int(h * k), int(w * k)
    cov = np.asarray(ink.resize((W, H), Image.BOX)).astype(float) / 255.0
    sh = np.asarray(shade.resize((W, H), Image.BOX)).astype(float) / 254.0
    sh = np.where(cov > 0, sh / np.maximum(cov, 1e-6), 0)   # the ink's own shade, not diluted by the ground
    c = np.clip(np.round(cov * 3.0), 0, 3).astype(np.uint8)
    d = np.clip(np.round(sh * 3.0), 0, 3).astype(np.uint8)
    v = np.where(c > 0, d << 2 | c, 0).astype(np.uint8)
    ys, xs = np.nonzero(c)
    return v[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def rle(t):
    b, flat, i = [], t.flatten(), 0
    while i < len(flat):
        j = i
        while j < len(flat) and flat[j] == flat[i] and j - i < 16:
            j += 1
        b.append(int(flat[i]) << 4 | (j - i - 1))
        i = j
    return b


def main():
    t = drop_small(majority(majority(tones())), 30)
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Copyright (C) 2026 Dubwise FM contributors */",
             "/* The SpIllt Beats mark, from the owner's brush drawing: made by tools/logo/mklogo.py, don't edit. Each",
             " * size a run-length list, row by row: one byte a run, shade << 6 | coverage << 4 | (length - 1); coverage",
             " * 0 the ground, 3 solid ink (1, 2: an edge); shade 0 the deepest blue .. 3 the brush's light. */", ""]
    total, previews = 0, []
    for name, k in SIZES.items():
        s = scaled(t, k)
        data = rle(s)
        total += len(data)
        h, w = s.shape
        lines.append(f"#define LOGO_{name}_W {w}")
        lines.append(f"#define LOGO_{name}_H {h}")
        lines.append(f"static const uint8_t LOGO_{name}[{len(data)}] = {{")
        for i in range(0, len(data), 20):
            lines.append("    " + ", ".join(f"0x{v:02X}" for v in data[i:i + 20]) + ",")
        lines.append("};")
        lines.append("")
        previews.append(s)
        print(f"{name}: {w} x {h}, {len(data)} B")
    open(OUT, "w").write("\n".join(lines))
    print(f"total {total} B -> {os.path.relpath(OUT)}")
    if len(sys.argv) > 1:
        s = previews[0]
        blues = np.array([(52, 92, 160), (70, 112, 180), (100, 150, 205), (150, 200, 228)], float)
        ground = np.array((188, 138, 90), float)
        a = ((s & 3) / 3.0)[..., None]
        img = ground * (1 - a) + blues[s >> 2] * a
        Image.fromarray(img.astype(np.uint8)).resize((s.shape[1] * 3, s.shape[0] * 3), Image.NEAREST).save(sys.argv[1])


if __name__ == "__main__":
    main()
