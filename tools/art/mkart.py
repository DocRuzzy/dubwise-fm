#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The screen saver's picture and its smoke mark, as app/art_data.h.

selector.jpg (the owner's illustration, 2026-11-09) is scaled to the screen's 240 x 240, cut to its best 256
colours (no dither: the drawing's flat colours and ink lines stay clean) and packed (PackBits: a byte n < 128 is
n + 1 colours as they come, n >= 128 is the next colour 257 - n times). The SpIllt Beats mark the smoke gathers
into: points on its strokes, about 7.5 px apart (the lobes of a comic-book cloud), from the owner's brush drawing (tools/logo/).
Usage: python3 -I tools/art/mkart.py [preview.png]
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "logo"))
import mklogo  # noqa: E402  (the mark's drawing, cleaned)

OUT = os.path.join(HERE, "..", "..", "app", "art_data.h")
MARK_K = 0.80                                  # the smoke mark's size, of the drawing's 238 x 205
SPACING = 7.5                                  # its lobes' spacing (they overlap: r 5..8)


def rgb565(c):
    r, g, b = (int(v) for v in c)
    return (r >> 3) << 11 | (g >> 2) << 5 | b >> 3


def packbits(a):
    out, i, n = [], 0, len(a)
    while i < n:
        j = i
        while j + 1 < n and a[j + 1] == a[i] and j - i < 127:
            j += 1
        if j - i >= 2:                          # a run of 3..128
            out += [257 - (j - i + 1), int(a[i])]
            i = j + 1
            continue
        k = i                                   # colours as they come, up to the next run of 3
        while k < n and k - i < 128 and not (k + 2 < n and a[k] == a[k + 1] == a[k + 2]):
            k += 1
        out += [k - i - 1] + [int(v) for v in a[i:k]]
        i = k
    return out


def mark_points():
    t = mklogo.drop_small(mklogo.majority(mklogo.majority(mklogo.tones())), 30)
    ink = Image.fromarray(((t > 0) * 255).astype(np.uint8))
    h, w = t.shape
    m = np.asarray(ink.resize((int(w * MARK_K), int(h * MARK_K)), Image.BOX)) > 100
    H, W = m.shape
    pts, taken = [], []
    for y in range(H):                          # points on the strokes, none closer than SPACING
        for x in range(W):
            if m[y, x] and all((x - a) ** 2 + (y - b) ** 2 >= SPACING ** 2 for a, b in taken):
                taken.append((x, y))
    for x, y in taken:
        pts.append((x - W // 2, y - H // 2))
    return pts


def main():
    src = Image.open(os.path.join(HERE, "selector.jpg")).convert("RGB").resize((240, 240), Image.LANCZOS)
    q = src.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:768]).reshape(-1, 3)
    data = packbits(np.asarray(q).flatten())
    pts = mark_points()
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only",
             " * Copyright (C) 2026 Dubwise FM contributors */",
             "/* The screen saver's picture (the owner's illustration, 240 x 240, 256 colours, PackBits) and the points",
             " * of the SpIllt Beats mark its smoke gathers into. Made by tools/art/mkart.py: don't edit. */", "",
             f"static const uint16_t ART_PAL[256] = {{"]
    p = [rgb565(c) for c in pal] + [0] * (256 - len(pal))
    for i in range(0, 256, 12):
        lines.append("    " + ", ".join(f"0x{v:04X}" for v in p[i:i + 12]) + ",")
    lines += ["};", f"static const uint8_t ART[{len(data)}] = {{"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(f"{v}" for v in data[i:i + 24]) + ",")
    i = p = 0                                   # where the screen's second band (row 120) starts in the data
    while True:
        n = data[i]
        cnt, size = (n + 1, n + 2) if n < 128 else (257 - n, 2)
        if p + cnt > 120 * 240:
            break
        i += size
        p += cnt
    lines += ["};", f"#define ART_MID {i}u                      /* the packet holding row 120's first pixel .. */",
              f"#define ART_MID_P {p}u                   /* .. and the pixel it starts at */"]
    lines += [f"#define MARK_PTS {len(pts)}", f"static const int8_t MARK_PT[MARK_PTS][2] = {{"]
    for i in range(0, len(pts), 10):
        lines.append("    " + ", ".join(f"{{{x}, {y}}}" for x, y in pts[i:i + 10]) + ",")
    lines += ["};", ""]
    open(OUT, "w").write("\n".join(lines))
    print(f"picture {len(data)} B, palette 512 B, mark {len(pts)} points -> {os.path.relpath(OUT)}")
    if len(sys.argv) > 1:
        q.convert("RGB").resize((480, 480), Image.NEAREST).save(sys.argv[1])


if __name__ == "__main__":
    main()
