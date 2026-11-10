# REED's body, fitted to real melodicas (roadmap 7.3)

INST REED's three body filters (`app/engine/voices.c`, `voices_config`) were fitted on 2026-11-10 to recordings of
real melodicas, single held notes from three Freesound packs (credits in CREDITS.md):

- "Hohner Melodica" by fese, CC BY 4.0: https://freesound.org/people/fese/packs/1425/ (9 notes)
- "Melodica notes" by pinevoc, CC0: https://freesound.org/people/pinevoc/packs/26684/ (3 notes)
- "Melodica" by urlande, CC0: https://freesound.org/people/urlande/packs/9578/ (one recording at many pitches:
  only its distinct harmonic patterns are used, 22 of 38, since a pitch-shifted copy moves the body with it)

The recordings aren't in the repository (`samples/` is ignored); download the packs into `samples/melodica/` to
run the fit again:

```
MEL_OUT=build/melodica python3 -I tools/melodica/harm.py samples/melodica   # each note's pitch and harmonics
python3 -I tools/melodica/fit.py                                             # the body: real less REED's reed
python3 -I tools/melodica/fit2.py                                            # two peaks and a low-pass, fitted
```

`cmp.py REAL.json OURS.json` compares the pooled harmonic envelopes of two runs of `harm.py`.

Result: peak 1,455 Hz (Q 0.69, +15 dB), peak 2,929 Hz (Q 1.04, +12.7 dB), low-pass 5,965 Hz (Q 0.40); the fit's
error 1.83 dB RMS. REED rendered across F3–C6 now sits 3.1 dB RMS from the recordings' envelope over 550 Hz–7.4 kHz
(the estimated body before: 9.6 dB).

## Refit by ear (2026-11-10, the same day)

Played side by side, the real melodica was much brighter and buzzier than either REED. Two mistakes in the first fit:
pooling every note into one curve let the many high urlande copies outweigh the low notes, and REED's reed was kept
as built (open half the cycle) with only the body fitted. Looked at note by note, a real one's first ten or so
harmonics are near level, then a cliff by the 12th, at every pitch: a property of the reed, a short puff (open ~10 %
of the cycle). `fit_puff.py` fits the puff and the body together, per note: a half-sine puff 10 % wide, a resonant
low-pass at 1,992 Hz (Q 2.6) and a peak at 3,749 Hz (Q 1.37, +11.4 dB). REED now plays the puff from band-limited
tables (8 % wide blown hard, 15 % soft). Brightness centres, real against REED: C4 1,943 / 1,672 Hz, G4 1,866 /
1,883, C5 2,026 / 1,978 (the first REED: 464 Hz at C4; the first fit: 983).
