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
