# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Roadmap 7.7: writes app/engine/horn_data.h, INST HORN's numbers, from the fit (tools/brass/README.md). The fit itself
# (the bell, each note's source slope) is the JSON fit_horn.py writes; this takes each horn's own harmonic pattern
# from its reference recordings: the measured harmonics less the fitted smooth model (source slope + bell), by
# harmonic number, the first 32, clipped to -15 .. +10 dB.
#   python3 -I tools/brass/mkhorn.py FIT.json RESIDUALS.json > app/engine/horn_data.h
import json, sys
fit = json.load(open(sys.argv[1])); res = json.load(open(sys.argv[2]))
fh, qh, f1, q1, g1, fl, ql = fit["p"]
print("/* SPDX-License-Identifier: GPL-3.0-only")
print(" * Copyright (C) 2026 Dubwise FM contributors */")
print("/* Made by tools/brass/mkhorn.py (roadmap 7.7): INST HORN's bell and its horns' harmonic patterns, fitted to")
print(" * Freesound recordings (credits in CREDITS.md). Do not edit. */")
print("#pragma once")
print("#define HORN_HP_HZ %.1ff" % fh); print("#define HORN_HP_Q %.3ff" % qh)
print("#define HORN_PK_HZ %.1ff" % f1); print("#define HORN_PK_Q %.3ff" % q1); print("#define HORN_PK_DB %.2ff" % g1)
print("#define HORN_LP_HZ %.1ff" % fl); print("#define HORN_LP_Q %.3ff" % min(ql, 2.5))
print("#define HORN_NK 32")
names = ["TRUMPET soft", "TRUMPET loud", "SAX soft", "SAX loud"]
print("/* the source's fall, dB an octave of harmonic number: [horn][soft, loud] */")
print("static const float HORN_SLOPE[2][2] = {{%.1ff, %.1ff}, {%.1ff, %.1ff}};" % tuple(res["slopes"]))
print("/* each table's pattern, dB by harmonic (1 ..): %s */" % ", ".join(names))
print("static const signed char HORN_RES[4][HORN_NK] = {")
for r in res["res"]:
    v = [max(-15, min(10, int(round(x)))) for x in (r + [0] * 32)[:32]]
    print("    {" + ", ".join("%d" % x for x in v) + "},")
print("};")
print("static const float HORN_NOISE_DB[2] = {%.1ff, %.1ff};   /* the breath, against the tone: trumpet, sax */" % tuple(res["noise"]))
