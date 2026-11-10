# Survey: prior art beyond the FM-1 (2026-11-10)

The owner asked for open-source plugins, papers and specs that could improve the melodica and the other voices. They
also listed mixing-desk and delay emulations to look at (AudioThing Dub Filter, MBSI, Airwindows ConsoleMC/MD,
bx_console, DubStation, the RE-201 models).

Everything here was read, not built or run. Costs are estimates (1 % of a core ≈ 160 cycles a sample at 22.05 kHz,
≈ 80 at 44.1 kHz).

**Licences:**
- These can be copied with credit in the file header and CREDITS.md: MIT (Airwindows, Mutable Instruments, STK's
  MIT-style notice), GPL-3.0 (chowdsp_dsp_utils, BYOD, Surge XT's chowdsp spring, ChowTapeModel), CC-BY (Parker 2011,
  Puranik & Scavone 2023).
- Papers under other terms give ideas and published constants only.

## The desk

- **The Big Knob is right.** The Altec 9069B is a passive 3rd-order (18 dB/oct) LC high-pass with ten steps (70, 100,
  150, 250, 500, 1k, 2k, 3k, 5k, 7.5k) plus off. That's AudioThing's "eleven" positions, and exactly ours (1.1).
  Sources: Audio-Merge's KTBK-1B replica page, Gearnews 2021.
  - The 9069B's sibling, the **9068B**, is a stepped *low-pass*; the pair together was sold as the 9067B. Its original
    steps are unpublished. A replica's run 250 Hz–10 kHz in ten.
  - **Candidate:** a LOW-PASS Big Knob mode on the same detents.
  - The click on a switch is authentic, and we already jump coefficients a detent.
- **Airwindows ConsoleMC / ConsoleMD** (MIT; `plugins/LinuxVST/src/ConsoleMC*`). Here's how its parts fit us.

  | ConsoleMC part | What it does | Verdict |
  |---|---|---|
  | Channel | The fader *before* the EQ ("for MCI consoles … which overdrives easily"), Pear crossovers, sin() stages a band, a 64-tap ResEQ2 mid, SubTight, `sin()` out | The whole channel is ~250–400 cycles a strip, ~25 % for eight: too much. Keep the end-stage `sin()` (~1 % for eight). |
  | Buss | Console7's decode: `0.618·asin(x\|x\|)/\|x\| + 0.382·asin(x)`, then a "Sinew" slew limit (`0.517·cos(last)` a sample) | ~1–2 %. |

  - **Candidate: DESK: CLEAN / MCI.** In MCI, each strip's `sin()` (after its fader, MCI-style), the asin blend and
    Sinew on the sum, before our limiter. Every input to the sum must pass a `sin()`, the returns included.
  - It's all `double` with noise-injected denormals. A port uses `sinf` / polynomials and drops the dither.
  - **Channel9's** per-desk constants (Neve, API, SSL, Teac, Mackie: an IIR amount, threshold, cutoff) are a cheap
    "flavour" table.
- **MCI JH-416/428 and Soundcraft (Class A):** no published measurements or open models. The fader-before-drive idea
  is the only MCI behaviour anyone has in code.

## The echo

- **RE-201 Space Echo:**
  - Its heads are at 1 : 2 : 3. We have T and 2T, so a **3T head** would complete the set.
  - Measured by Downing & Terjesen (U. Rochester 2016):
    - a speed-change glide τ of 1.89 s rising and 1.07 s falling;
    - wow mostly under ~1 Hz;
    - the echo path a 2nd-order Chebyshev band-pass;
    - a Baxandall tone on the echo only, at 750 Hz.
  - These are checks for our tape dive, wow (6.3) and loop filter.
  - ChowTapeModel (GPL-3.0) is a hysteresis model, too heavy for us; its loss filters could be borrowed.
- **BBD:**
  - Holters & Parker (DAFx 2018) and chowdsp_dsp_utils' `BBD` (GPLv3) give the Juno-60's input and output filter
    roots and poles, which scale with the clock. The full model costs ~150–300 cycles a sample.
  - **Candidate: a cheap ECHO TYPE BBD** on our 11 kHz line:
    - 2 biquads either side, their cutoff following the clock;
    - NE570-style companding (`x/√env` in, `×env` out);
    - ~0.4 %.

## The voices

| Voice | Best source | What to take | Cost |
|---|---|---|---|
| Spring (1.2) | Parker, EURASIP 2011 (CC-BY) | A longer dispersive chain (~50 stretched all-passes a spring at 11 kHz, the chirp straightened with double k and half M); the high chirp dropped (30 dB under). Ours is ~1/8 the reference's chirp length: the biggest "drip" upgrade available | ~1.8 %, +2 KB |
| Spring | Surge XT's chowdsp spring (GPL-3.0) | The "shake" (a 0.5–1 ms half-sine burst) for the crash; a chaos jitter of the loop delay; feedback from T60 | ~0 |
| Melodica (2.2) | Puranik & Scavone, DAFx-23 harmonium model (CC-BY) | A free-reed source (a 37-harmonic band-limited table, one discontinuity a period) into 3–4 peak biquads (the enclosure, which shapes the tone more than the reed); pressure drives level and droops f0 1–2 Hz. Fit the biquads to one recorded melodica note | ~0.4 %, 2–4 KB |
| Brass (2.2) | STK `Brass` (MIT-style) | The lip-reed tick: lip biquad (R 0.997) on pressure − 0.85·bore, squared and clipped, mixed into a 2·fs/f delay. Brightness follows breath by itself. Keep our section doubling | ~0.25 %, 1.6 KB int16 |
| Skank (1.6) | STK `Plucked` (MIT-style) | Karplus-Strong a string; the chop: loop gain → 1 − amp at the gate's end. A real muted strum | ~0.3 % for three, 1.2 KB |
| Kick, rim, hats (0.5, 5.7) | Mutable Plaits' drums (MIT) | Analog kick (a pulse into a high-Q SVF band-pass, a 6 ms FM pulse); snare (5 SVF modes at 1, 2, 3.18, 4.16, 5.62 × f0 plus rectified noise); 808 hat (six squares at 414 Hz × 1, 1.304, 1.466, 1.787, 1.932, 2.536; BP then HP; the swing VCA) | ~0.3–0.6 % each |
| Siren (2.1) | The NJD siren, Zen Instruments' redraw (ideas) | It's two-transistor astables, not a 555: the LFO's square through an RC with a fast charge and a slow discharge (the "rise" is an RC approach, not a ramp); the pulse width moves with the pitch; RC low-passes on the output | <0.1 % |

**Not found:** published Hohner melodica formant data, an 808 rimshot's verified modes, a syndrum source.

## Ranked (value for cost)

1. The Parker spring.
2. DESK: MCI (Airwindows).
3. The free-reed melodica.
4. The NJD siren shape.
5. The 3T head (RE-201).
6. Plaits drums as models.
7. STK brass.
8. The STK plucked skank.
9. The BBD echo type.
10. The low-pass Big Knob.
