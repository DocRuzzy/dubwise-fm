# Plan: roadmap 6.7, trig conditions and FILL (a step format change)

Status: **approved 2026-11-10.** The owner took all five decisions as recommended: one CONDITION field with
CHANCE in it; the 16 values; the note's top bit; FILL on OCT+ held (and learnable); PRE counts skipped steps.

Sources (read, not built):
- fizz `cond_ok` (MPL-2.0);
- bigorna `tracks.c:seq_cond_ok` (GPL-3.0);
- luxr `lx_seq.c:step_trig` (GPL-3.0): FILL while a key is held.

All three follow the Elektron model. A step has **one** condition, and probability is one of the conditions. Survey:
`docs/survey-fm1-emu-2026-11.md`.

## What the player gets

A step's **CONDITION** replaces 5.9's CHANCE. It is the same field with more values, so CHANCE stays exactly as built.
These are the 16 values; a step whose condition is false doesn't play.

| # | Shown | Plays when | Use in dub |
|---|---|---|---|
| 0 | (none) | always | |
| 1–3 | 75 / 50 / 25 % | the 5.9 hash (seeded, the same every launch) | ghost hats, a loose rim |
| 4 | 1ST | the first pass since the pattern launched | a pickup on the drop, a crash on the one |
| 5 | !1ST | every pass but the first | |
| 6 | 1:2 | passes 1, 3, 5… | an answer every other bar |
| 7 | 2:2 | passes 2, 4, 6… | |
| 8 | 1:4 | pass 1 of every 4 | a turnaround on the fourth bar |
| 9 | 2:4 | pass 2 of every 4 | |
| 10 | 3:4 | pass 3 of every 4 | |
| 11 | 4:4 | pass 4 of every 4 | |
| 12 | PRE | the last conditional step on this channel played | an either/or pair: 50 % then PRE (or !PRE) |
| 13 | !PRE | it didn't | |
| 14 | FILL | FILL is held | a drum roll, a bass run, played live |
| 15 | !FILL | FILL is not held | the groove steps a fill replaces |

**Passes** use the count CHANCE already uses: the step counter since the launch, divided by the length (`n / len` in
`engine.c`). So 1ST and A:B are a pure function of the launch and the step, like CHANCE, and a captured song replays
bit-exact. A scene recall or song bar that relaunches a pattern starts its passes again; one that leaves it playing
doesn't.

**PRE.** Each channel keeps one bit in the engine (8 bits in all): the result of the last conditional step it played
or skipped (conditions 1–15). A step with no condition doesn't touch it. The bit resets to "played" at a launch, so the
first PRE after a launch plays. Replay stays exact, because the bit depends only on things that are already
deterministic (FILL is captured as key events).

**FILL is momentary**: principle 1, hold is momentary. Where it lives:
- **On the FM-1: OCT+ held** (≥ 300 ms, the mute keys' hold threshold) on every page but STEPS. A tap still steps
  the quantise, at its let-go. The DUB page has no other free key (§4.1).
- **Elsewhere:** FILL becomes a MIDI-learn target (`midilearn.c`; a CC ≥ 64 is on). So a pedal, a pad, or any SMC
  button (6.5) can hold it. 6.5 handles only notes 0x00–0x1F and 0x5D–0x5F, and whether the SMC sends the Mackie
  CYCLE (0x56) gets checked on the hardware before it's mapped.
- **Latch:** OCT− tapped while OCT+ holds FILL latches it, until the next OCT+ tap. (Built 2026-11-10: the plan's
  double tap on OCT+ caught the quick taps that step the quantise.)

The header shows **FILL** while it's on.

**Editing**: black 11 held + a white key cycles the condition. It now goes through 16 values, not 4. With black 11
still held, **KNOB 4 scrolls the list directly**, so no one has to tap 14 times to reach FILL.

The big view:
- names the condition under the step (`%50`, `1ST`, `2:4`, `PRE`, `FIL`, `!FL`);
- marks the steps that won't play this pass hollow, as now. Those marks use the same functions as the engine:
  1ST, A:B and % exactly; FILL from the live key; PRE from a scan back to the previous conditional step, and when that
  step is in an earlier pass (unknown to the page), no mark.

## Where it goes (no step grows)

Four bits are needed. A step has two spare bits today, and the two CHANCE bits are reused:

| Bit of the condition | Where | Every older save holds |
|---|---|---|
| 0–1 | `step.lvl` bits 2–3 (5.9's CHANCE) | 0–3: none / 75 / 50 / 25 %, the same meaning |
| 2 | `step.lvl` bit 7 (spare; the setters mask it) | 0 |
| 3 | `step.note` bit 7 (MIDI notes are 0–127; `step_pack` already drops it) | 0 |

- So an old step's CHANCE reads as the same condition, with no remap.
- **The note's top bit is the one risk.** Every reader of `.note` (about 25, mostly in `steps.c` and `patstore.c`)
  moves to `STEP_NOTE(s)` = `s.note & 0x7F`, and writers to `STEP_NOTE_SET`. A test plays every note 0–127 with every
  condition set and checks the pitch (below).
- The other choice is a separate conditions object. There is no flash for one (5.8's plan), and 16 bytes per pattern
  × 32 patterns would push the steps object past the store.
- **The queue** (`step_pack`):
  - bits 0–29 are used today;
  - the condition's bits 2 and 3 go in bits 30 and 31 (bit 31 is the sign: `step_unpack` reads it with an unsigned
    shift);
  - `pmeta_pack` doesn't change.
- **The engine:** a `cond_plays(ch, p, i, pass, cond, seed, fill, pre)` replaces `chance_plays`. It stays in
  `riddim.h`, shared by the engine and the page. CHANCE's hash and its constants are unchanged, so every 5.9 song plays
  the same notes.

## The version bumps and the converter

| Store | Now | After | Converter (on load) |
|---|---|---|---|
| Steps (`OBJ_PST0..2`) | "PST2" | "PST3" | `lvl &= 0x7F`, `note &= 0x7F` (they should already be 0; a hand-made or SysEx'd save is cleaned, not misread) |
| Steps, older | "PST1" | (as now) | the 5.8 converter, then the one above |
| Song slots (`slots.c`) | "DSL3" | "DSL4" | the same, on LOAD SLOT and on a SysEx put; DSL2 runs through both converters |
| Patterns, settings, scenes, song | — | unchanged | — |

- Why bump the magic when old data needs nothing? So that older firmware doesn't play a PST3 save wrongly:
  - it would read a note with the top bit set as a negative note;
  - FILL steps would play every pass.

  With the new magic it sees an unknown magic and loads empty patterns. That's the usual downgrade behaviour; the
  release notes say so.
- `tools/dubwise_slots.py info` learns DSL4 and prints the slot's version.
- FILL's state is never stored. A saved latch would surprise the player at the next boot.

## Tests

- **`oldsave` extended:**
  - a PST2 save with CHANCE steps (the 5.9 test's) reboots as PST3 and plays the same events (`cap_ev_diff` 0);
  - a PST1 save still converts through both steps.
- **`conds.txt` (new):**
  - over 8 passes, from the event log: 1ST plays once; !1ST seven times; 1:2 on passes 1, 3, 5, 7; 2:4 on passes 2
    and 6;
  - a relaunch restarts the count;
  - PRE / !PRE after a 50 % step play exactly when it didn't / did (both cases);
  - FILL: `key OCT+ down` plays the FILL steps from the next step and drops the !FILL ones; the release brings them
    back; the latch; a learned CC; the tap still changes the quantise;
  - the page's hollow marks match what plays, for every condition but a PRE whose predecessor was in an earlier pass;
  - undo of a condition edit;
  - a captured song with FILL presses replays bit-exact (`cap_wav_diff` 0).
- **`notes7.txt` (the risk):** the bass and lead play notes 0–127 with conditions 8–15 (bit 3 set); `zc_hz` matches
  each note.
- **A slot round trip:** a DSL3 image over SysEx loads, and a DSL4 slot saves and loads back the same.

## Cost (estimate)

- **Image:** ~3–4 KB (the table of names, the page, the converter).
- **RAM:** 2 bytes (PRE's eight bits, FILL).
- **Pool:** 0.
- **CPU:** one switch a step instead of one compare, ~0.
- **Tier:** all.

## Decisions for the owner

1. **One CONDITION field, with CHANCE as four of its values** (recommended; the Elektron model, and every 5.9 save
   plays the same). The other way is a separate condition beside CHANCE, which needs more bits than a step has.
2. **These 16 values.** There's no 1:3 / 2:3 / 3:3: dub phrases run in 2s and 4s. If you'd rather have 1:3–3:3, they
   would take !1ST, !PRE and 4:4.
3. **The fourth bit in the note's top bit, with `STEP_NOTE` everywhere** (recommended; no store grows). The test above
   is there because it's the risky part.
4. **FILL on OCT+ held, latched by OCT− while it is held (built; the plan first had a double tap), and MIDI-learnable.** The other choice is black 11
   held on the DUB page, but that key is the drum & bass drop.
5. **PRE counts the last conditional step, including skipped ones** (the Elektron rule; fizz and bigorna both do it).
