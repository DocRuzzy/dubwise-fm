#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The deejay voice's data (roadmap 6.6) -> app/engine/lpc_data.h.

The speech chip's tables and its 42 phonemes come from SPELLBOX (https://github.com/pjac1810/spellbox at d1b90ea,
GPL-3.0-only): its src/speech.h, made by its tools/gen_speech.py from espeak-ng's speech
(GPL-3.0) coded as 10th-order LPC frames (energy, pitch, ten reflection coefficients), the way the 1978 speech chips
did. Here: the quantising tables, the chirp, the 42 phonemes' bits (12.5 ms frames), and Dubwise's own shouts written
as phonemes with their stresses (no dictionary: the list below). The firmware copies it all into the pool at boot
(~2.4 KB: the render never reads the flash) and strings a shout's phonemes together with an intonation, as SPELLBOX's
tts_say does.

  python3 -I tools/vox/mklpc.py PATH/TO/spellbox/src/speech.h
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# The shouts: name (8 characters, for the screen), then phonemes (ARPAbet, as SPELLBOX's; a 1 after a vowel: stressed)
SHOUTS = [
    ("SELECTA", "S AX L EH1 K T AX"),
    ("WHEEL UP", "W IY1 L AH1 P"),
    ("BIG UP", "B IH1 G AH1 P"),
    ("RUBADUB", "R AH1 B AX D AH1 B"),
    ("FORWARD", "F AO1 R W AXR D"),
    ("JAH", "JH AA1"),
    ("ONE MORE", "W AH1 N M AO1 R"),
    ("SOUND", "S AW1 N D S IH1 S T AX M"),
    ("DUB", "D AH1 B"),
    ("PULL UP", "P UH1 L AH1 P"),
    ("LICKSHOT", "L IH1 K SH AA1 T"),
    ("BOOM", "B UW1 M"),
    ("YES I", "Y EH1 S AY1"),
    ("IRIE", "AY1 R IY"),
    ("ROOTS", "R UW1 T S"),
    ("STRICTLY", "S T R IH1 K T L IY"),
]


def arr(name, ctype, vals, per=20):
    s = ["static const %s %s = {" % (ctype, name)]
    s += ["    " + ", ".join(str(v) for v in vals[i:i + per]) + "," for i in range(0, len(vals), per)]
    return s + ["};", ""]


def main(src):
    h = Path(src).read_text()
    def nums(pat):
        m = re.search(pat + r"\s*=\s*\{([^}]*)\}", h)
        return [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    kbits = [int(x) for x in re.search(r"#define SP_KBITS \{([^}]*)\}", h).group(1).split(",")]
    ks = [nums(r"static const int16_t SP_K%d\[\d+\]" % (i + 1)) for i in range(10)]
    energy = nums(r"static const uint16_t SP_ENERGY\[16\]")
    period = nums(r"static const uint8_t SP_PERIOD\[32\]")
    chirp = nums(r"static const int8_t SP_CHIRP\[41\]")
    at = nums(r"static const uint32_t SP_AT\[SP_COUNT\]")
    bits = nums(r"static const uint8_t SP_BITS\[\d+\]")
    p0 = int(re.search(r"#define SP_PHONEME (\d+)", h).group(1))
    names = re.search(r"PH_NAME\[PH_N\] = \{([^}]*)\}", h).group(1)
    names = [x.strip().strip('"') for x in names.split(",") if x.strip()]
    assert len(names) == 42 and p0 + 42 <= len(at)
    at.append(len(bits) * 8)
    # the phonemes' bits, re-based: each one starts on a byte (its offset in bits kept)
    pbits, poff = [], []
    for i in range(42):
        a, b = at[p0 + i], at[p0 + i + 1]
        poff.append(len(pbits) * 8)
        n = b - a
        out, acc, k = [], 0, 0
        for j in range(n):
            bit = bits[(a + j) >> 3] >> (7 - ((a + j) & 7)) & 1
            acc = acc << 1 | bit
            k += 1
            if k == 8:
                out.append(acc)
                acc, k = 0, 0
        if k:
            out.append(acc << (8 - k))
        pbits += out
    poff.append(len(pbits) * 8)
    shouts, sat, snames = [], [], []
    for name, phs in SHOUTS:
        sat.append(len(shouts))
        for p in phs.split():
            stress = p.endswith("1")
            code = names.index(p.rstrip("1"))
            shouts.append(code | (0x40 if stress else 0))
        snames.append(name)
    sat.append(len(shouts))
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
           "/* Made by tools/vox/mklpc.py from SPELLBOX's src/speech.h (GPL-3.0-only; its speech espeak-ng's, GPL-3.0) and",
           " * Dubwise's shouts: don't edit. The deejay voice (roadmap 6.6, app/engine/fxv.c) */",
           "#pragma once", "#include <stdint.h>", "",
           "#define LPC_KBITS {%s}" % ", ".join(str(v) for v in kbits)]
    for i, k in enumerate(ks):
        out += arr("LPC_K%d[%d]" % (i + 1, len(k)), "int16_t", k)
    out += arr("LPC_ENERGY[16]", "uint16_t", energy)
    out += arr("LPC_PERIOD[32]", "uint8_t", period)
    out += arr("LPC_CHIRP[41]", "int8_t", chirp)
    out += ["#define LPC_PH_N 42", "#define LPC_PH_VOWELS 17", "#define LPC_STRESS 0x40"]
    out += arr("LPC_PH_AT[43]", "uint16_t", poff)
    out += arr("LPC_PH_BITS[%d]" % len(pbits), "uint8_t", pbits, 24)
    out += ["#define LPC_SHOUTS %d" % len(SHOUTS)]
    out += arr("LPC_SH_AT[%d]" % len(sat), "uint8_t", sat)
    out += arr("LPC_SH_PH[%d]" % len(shouts), "uint8_t", shouts)
    out.append("static const char LPC_SH_NAME[LPC_SHOUTS][9] = {%s};" % ", ".join('"%s"' % n for n in snames))
    (ROOT / "app/engine/lpc_data.h").write_text("\n".join(out) + "\n")
    print("app/engine/lpc_data.h: %d phonemes in %d bytes, %d shouts" % (42, len(pbits), len(SHOUTS)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/home/user/ext/spellbox/src/speech.h")
