#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Writes app/engine/leslie_data.h from tools/organ/setbfree-whirl.txt (setBfree's Leslie, GPL-2.0-or-later; see
tools/organ/README.md), for the B-3's FULL Leslie (app/engine/leslie.c). After setBfree-fm1's tools/gen_leslie.py
(R-Jeb, GPL-2.0-or-later), at 512 angles (every other row) and moved from setBfree's 44.1 kHz to the organ's half
rate (22,050 Hz), where the Leslie runs:
  - the delays and spacings halved (they are times);
  - the horn's 5-tap angle filter folded to 3 taps (each 44.1 kHz tap into the half-rate sample it falls in, the one
    between split in two: the same sum, so the same gain at low frequencies);
  - the three biquads moved by their bilinear transform: poles and zeros mapped through the analog plane, the gain
    matched at 1 kHz.

  LES_DISP[4][512]   the delay to the microphone by the rotor's angle, Q4 samples at 22,050 Hz: horn forward,
                     horn backward, drum forward, drum backward
  LES_BEAM[2][512*3] the horn's angle-dependent 3-tap filter, Q15: forward, backward
  LES_HSP, LES_DSP   the six paths' spacings (Q4 samples); LES_HPH, LES_DPH their phases (of 512 angles)
  LES_HA, LES_HB, LES_DF  the horn's two input filters and the drum's output filter: b0 b1 b2 a1 a2
"""
import cmath
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
N = 512
kv, rows = {}, []
for ln in (ROOT / "tools/organ/setbfree-whirl.txt").read_text().splitlines():
    p = ln.split()
    if not p:
        continue
    if p[0] == "t":
        rows.append([float(v) for v in p[2:]])
    else:
        kv[p[0]] = p[1:]
assert len(rows) == 1024
rows = rows[::1024 // N]


def arr(name, ctype, vals, per, fmt="%d"):
    s = ["static const %s %s = {" % (ctype, name)]
    s += ["    " + ", ".join(fmt % v for v in vals[i:i + per]) + "," for i in range(0, len(vals), per)]
    return s + ["};", ""]


FS1, FS2 = 44100.0, 22050.0
disp = [round(r[c] * 16 * FS2 / FS1) for c in range(4) for r in rows]


def fold(h):                                     # 5 taps at 44.1 kHz -> 3 at 22,050 Hz
    return [h[0] + h[1] / 2, h[1] / 2 + h[2] + h[3] / 2, h[3] / 2 + h[4]]


beam = [round(x * 32768) for c0 in (4, 9) for r in rows for x in fold(r[c0:c0 + 5])]
assert max(disp) < 65536 and max(abs(v) for v in beam) < 32768
out = ["/* SPDX-License-Identifier: GPL-2.0-or-later */",
       "/* Made by tools/organ/mkleslie.py from setBfree's Leslie (b_whirl/whirl.c; dumped by setBfree-fm1): don't edit.",
       " * setBfree: Copyright (C) Fredrik Kilander, Will Panther, Robin Gareus, Ken Restivo and others. */",
       "#pragma once", "#include <stdint.h>", "#define LES_ANG %du" % N, ""]
out += arr("LES_DISP[4][%d]" % N, "uint16_t", disp, 16)
out += arr("LES_BEAM[2][%d * 3]" % N, "int16_t", beam, 21)
out += arr("LES_HSP[6]", "uint16_t", [round(float(v) * 16 * FS2 / FS1) for v in kv["hornSpacing"]], 6)
out += arr("LES_DSP[6]", "uint16_t", [round(float(v) * 16 * FS2 / FS1) for v in kv["drumSpacing"]], 6)
out += arr("LES_HPH[6]", "uint16_t", [int(v) * N // 16384 for v in kv["hornPhase"]], 6)
out += arr("LES_DPH[6]", "uint16_t", [int(v) * N // 16384 for v in kv["drumPhase"]], 6)


def roots(c0, c1, c2):                           # of c0 + c1 z^-1 + c2 z^-2 (z = the roots of c0 z^2 + c1 z + c2)
    if abs(c0) < 1e-12:
        return [-c2 / c1] if abs(c1) > 1e-12 else []
    d = cmath.sqrt(c1 * c1 - 4 * c0 * c2)
    return [(-c1 + d) / (2 * c0), (-c1 - d) / (2 * c0)]


def move(r):                                     # a root at FS1 -> the same analog root at FS2 (bilinear both ways)
    if abs(r + 1) < 1e-6:                        # (at Nyquist: s at infinity, Nyquist again)
        return -1.0
    w = (r - 1) / (r + 1) * (FS1 / FS2)
    return (1 + w) / (1 - w)


def poly(rs):                                    # monic, in z^-1: (1 - r1 z^-1)(1 - r2 z^-1)
    p = [1.0, 0.0, 0.0]
    for r in rs:
        p = [p[0], p[1] - r * p[0], p[2] - r * p[1]]
    return [complex(x).real for x in p]


def resp(b, a, f, fs):
    z = cmath.exp(-2j * math.pi * f / fs)
    return abs((b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z))


def bq(name, key):
    v = [float(x) for x in kv[key]]          # setBfree's order: a0 a1 a2 b0 b1 b2 z0 z1
    b1, a1 = [v[3], v[4], v[5]], [1.0, v[1], v[2]]
    zs = [move(r) for r in roots(*b1)]
    while len(zs) < 2:                       # (a zero at infinity: at the half rate's Nyquist)
        zs.append(-1.0)
    b2, a2 = poly(zs), poly([move(r) for r in roots(*a1)])
    f0 = 1000.0                              # the gain matched where the analog frequency is the same
    w0 = 2 * math.atan(math.tan(math.pi * f0 / FS1) * FS1 / FS2) * FS2 / (2 * math.pi)
    g = resp(b1, a1, f0, FS1) / resp(b2, a2, w0, FS2)
    b2 = [x * g for x in b2]
    return arr(name, "float", [b2[0], b2[1], b2[2], a2[1], a2[2]], 5, "%.9ff")


out += bq("LES_HA[5]", "hafw") + bq("LES_HB[5]", "hbfw") + bq("LES_DF[5]", "drfL")
out.append("#define LES_HORN_LEVEL %.6ff   /* setBfree's hornLevel */" % float(kv["hornLevel"][0]))
out.append("#define LES_LEAK %.6ff         /* its leakage (0.15 x the horn's level) */" % float(kv["hornLevel"][2]))
(ROOT / "app/engine/leslie_data.h").write_text("\n".join(out) + "\n")
print("app/engine/leslie_data.h: %d angles at 22,050 Hz, %d bytes of tables" % (N, 2 * len(disp) + 2 * len(beam)))
