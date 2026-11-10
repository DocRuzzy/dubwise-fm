# The lip-reed brass (roadmap 7.7): tried twice, not built

STK's Brass tick (lips: a two-pole resonance on mouth pressure less the bore's; area = position squared, capped;
the bore a delay with a DC blocker) simulated offline in C, as the firmware would run it, floats at 44.1 kHz.

`lipsim.c` searches every note from C2 to E6 for a lip tension (0.3–3.0 in 0.01 steps) and a bore mode (its
fundamental or second) that speaks within 80 ms and plays within 30 cents of the note at breaths 0.3, 0.5, 0.7 and
0.9; the pitch measured by autocorrelation. `probe.c` prints the pitch over tension × mode × six breaths.

Result (2026-11-10): nothing below ~E5 (660 Hz) plays the note. Across the horn section's range (200–700 Hz) the
model is silent, or plays another note — a fourth or an octave up, two octaves down — and which one changes with the
breath at every tension. Above E5 it speaks at a middle breath within ~20 cents, not at a soft one. A per-note
tension table can't fix a model whose pitch depends on the breath; making it playable would be a different, larger
model. The memory was there (~4 KB of pool, ~0.25 % CPU); the instrument wasn't.

A first attempt ported it straight (an int16 bore): it never started, its first tiny values rounding to 0.

# INST HORN: fitted to recordings instead (2026-11-10)

The owner sent single notes from Freesound (credits in CREDITS.md): sgossner's VSCO 2 CE trumpet sustains (CC0, soft
and loud takes), MTG's tenor and baritone sax single notes (CC BY 3.0), thatjeffcarter's trombone tones (CC BY 4.0).
The recordings stay out of git (`samples/` is ignored).

- **The tone:** a source whose harmonics fall a set number of dB an octave of harmonic number (steeper soft, shallower
  loud: the blare), each horn's own uneven pattern on top, into a bell: high-pass 366 Hz (Q 1.74), peak 1,824 Hz
  (Q 1.16, +6.9 dB), low-pass 6.2 kHz (Q 2.5, capped). Fitted on 17 trumpet and tenor-sax notes, each note's strongest
  harmonics (within 30 dB of its loudest) weighted: 2.9 dB RMS. The trombone first: its low notes pulled the bell
  darker (it fitted, but loud trumpet came out dull), so the bell is the trumpet's and the sax's; the baritone sax,
  not fitted on, sits within 2.5–3.8 dB of it.
- **The life in it:** measured on the recordings and put back: the pitch wanders ~1.5–3 cents and jitters ~1–3.6, the
  level ~0.3–1.5 dB, breath noise sits 22–40 dB under the tone. The model before it had none of these; the owner
  heard it as "not nearly as complex".
- `fit/`: the scripts as run (their paths are the session's; `brsurvey.py` finds steady notes, `fitbrass.py` and
  `fit5.py` fit, `mkres.py` takes each horn's pattern, `texture.py` measures the movement, `render6.py` renders the
  offline model). `mkhorn.py` writes `app/engine/horn_data.h` from the fit.
