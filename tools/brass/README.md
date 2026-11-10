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
