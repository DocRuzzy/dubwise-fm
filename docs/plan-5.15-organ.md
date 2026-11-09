# Plan: roadmap 5.15, the tonewheel organ (B-3 style) and rotary speaker

Status: **draft, waiting for the owner's OK and open question 5** (CLAUDE.md: the organ's new patch fields are a
stored-format matter, so a plan comes first). The pool room it needs is free: review-2026-11 §5.4 (the spin ring as
µ-law) gave back 17 KB, so 59 KB of the pool is free.

## What the player gets

As the roadmap describes:
- **The model:** a second MODEL on channel 6 (B-3, beside BUBBLE).
- **Tone:**
  - nine drawbars (16′ 5⅓′ 8′ 4′ 2⅔′ 2′ 1⅗′ 1⅓′ 1′, 0–8 like the real bars), summed into one single-cycle table
    when a bar moves;
  - percussion (2nd or 3rd, fast or slow, single-trigger), the key click, and the scanner VIBRATO / CHORUS (V1–3,
    C1–3).
- **Rotary:** a two-band split with a Doppler horn and an AM drum. SLOW / FAST / BRAKE at each rotor's own pace,
  with SPEED a parameter (MIDI-learnable, captured as motion, thrown on a scene).
- **The SOUND view:** MAIN gives REGISTRATION · PERC · ROTARY · DRIVE; the drawbars sit in two raw banks, drawn as
  bars.

## The stored-format question: fields that aren't neutral at 0

The organ's stride has 4 fields today (DRAWBAR, DECAY, CLICK, DETUNE). B-3 adds about 15: MODEL, nine drawbars, PERC,
PERC FAST, VIB/CHORUS, ROTARY (speed), DRIVE. That makes 19 of the 28 raw fields, which fits.

The rule since 5.4 (`params.h`) is that an appended field must sound neutral at 0, because an older save or patch
holds 0 there. MODEL = 0 is BUBBLE, so an old sound stays BUBBLE, which is fine. But **a drawbar at 0 is silent**, so
an old save switched to B-3 would play nothing until its bars were pulled out. Three ways round it:

| Option | How | Cost |
|---|---|---|
| **A. Stored as offsets** (recommended) | Each B-3 field stores the difference from a default registration (e.g. 88 8000 000, "the full upper"): 0 is the default sound, and the screen shows stored + default (8, 8, 8, 0 …). | No converter, no version bump; the neutral-at-0 rule holds. The field's range is asymmetric (e.g. 16′ −8..0); a patch's value means "from the default", which is fine since only firmware reads patches. |
| B. A version and a converter | Patches gain version 2; `OBJ_SOUND` gains "SND2". The converter writes the defaults into the B-3 fields of any older sound or patch. | Two converters (the sound object and patches, plus song slots that carry sounds): what CLAUDE.md warns costs most. |
| C. Defaults applied when MODEL changes | Switching to B-3 writes the default registration into the fields if they're all 0. | No format change, but a registration of all 0 (silence) can't be saved on purpose, and it's a rule hidden in the UI. |

## Open question 5 (the owner's): the stereo horn

- **Mono** (default, as 7-inches heard it): the horn's movement summed. All tiers.
- **Stereo horn, Studio tier only:**
  - the drum band and everything under ~150 Hz stay mono (the Width rule);
  - the organ strip sends its mono sum to the echo and spring, and adds only the horn's side at the output, muted
    and levelled with the strip.
  - Principle 6 puts stereo only on effect returns, so this would be its one exception.

## Tests

- **`organ.txt`:**
  - drawbar harmonic levels read from a spectrum (each bar's partial at its level);
  - the rotor rates measured from the AM (SLOW ~0.8 Hz, FAST ~6.7 Hz horn, the drum slower), and BRAKE stopping;
  - percussion single-triggered (a legato second note without it);
  - `perf_pct` per tier;
  - with the stereo horn: `side_lo` under −80 dB.
- **Old saves:** an organ sound saved before 5.15 loads as BUBBLE, unchanged (`cap_wav_diff` 0). Switched to B-3, it
  plays the default registration (option A), with no silence.
- **A patch round trip** with the B-3 fields.

## Cost (the roadmap's estimate, ×80 rule, to measure)

- **CPU:**
  - tone ≤ BUBBLE's 7.5 % (one table read a note);
  - rotary and scanner ~2–4 % of a device block while the organ sounds.
- **Pool:** ~6 KB (tables, double-buffered; 3 KB with 256-point tables if they sound the same), plus <1 KB of delay
  lines.
- **Image:** ~4 KB.
- **Guard tiers:**
  - Studio: everything;
  - Four-track: a mono rotary;
  - Seven-inch: rotary AM only, no scanner.

## Decisions for the owner

1. **Option A, B or C** for the fields that aren't neutral at 0 (A recommended).
2. **Open question 5:** mono only, or the Studio-tier stereo horn.
3. **The default registration** that 0 means: 88 8000 000 ("full upper", the classic bubble-and-chord sound) or
   another.
