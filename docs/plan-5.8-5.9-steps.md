# Plan: roadmap 5.8 (drum step locks, flam, ratchet) and 5.9 (seeded step probability), one format change

Status: **draft, waiting for the owner's OK** (CLAUDE.md: a stored-format change needs a plan and an OK before code).
Both items change what a step holds, so they share **one** version bump and one converter. A wrong guess here would
cost a second converter.

## What the player gets

- **5.8:**
  - **Locks:** a drum step (kick, rim, hats) gets a TUNE lock and a DECAY lock, −8..+7 from the voice, 0 none.
    With a drum step held on STEPS, KNOB 2 sets the TUNE lock (kick, rim; the hats keep closed / open) and KNOB 1
    the DECAY lock (with no step held, KNOB 1 stays the pattern's length).
  - **REPEAT:** off · FLAM (a ghost 20 ms early) · ×2 · ×3 · ×4 within the step. ENV held + a white key cycles it.
- **5.9: a step's CHANCE** (100 / 75 / 50 / 25 %; black 11 held + a white key cycles it).
  - It's decided by a hash of the pattern's SEED, the step and the loop count since launch, not a free-running random
    number, so the same launches play the same notes and a captured song replays identically.
  - Steps that will skip this loop are marked on screen.
  - RESEED (HOME + ENV on STEPS) is an explicit, undoable action.

## Where it goes (no step grows: the steps object is 3,080 of the store's 3,840 bytes)

| What | Where | An old save holds there |
|---|---|---|
| CHANCE | `step.lvl` bits 2–3 (0 = 100 %, 1 = 75, 2 = 50, 3 = 25) | 0: 100 %, as before |
| REPEAT | `step.lvl` bits 4–6 (0 off, 1 FLAM, 2 ×2, 3 ×3, 4 ×4) | 0: off, as before |
| TUNE lock, DECAY lock | a **drum** step's `gate` byte, two signed nibbles (0 none) | possibly a gate from live recording: **converted** |
| SEED | `pmeta.rsv`, 1 byte | 0: a valid seed |

- `step.lvl` uses bits 0–1 today (OFF / GHOST / NORMAL / ACCENT), so the new bits are spare, and 0 in every save.
- A drum's `gate` means nothing to the sound (a drum is a one-shot), but live recording (4.8) stores one, so old drum
  steps may hold a non-zero gate that would read as locks. The converter clears it.
- No flash is left for a separate locks object: the two free 8 KiB runs went to 5.4's patch library.
- **The queue** (`step_pack`, 25 bits used): CHANCE and REPEAT take bits 25–29; the locks travel in the gate's bits
  as they are. `pmeta_pack` (17 bits) takes the seed in bits 17–24. No queue format changes beyond that.

## The version bumps and the converter

| Store | Now | After | Converter (on load) |
|---|---|---|---|
| Steps (`OBJ_PST0..2`) | "PST1" | "PST2" | drum channels' steps: `gate = 0`; everything else as is |
| Song slots (`slots.c`) | "DSL2" | "DSL3" | the same, on LOAD SLOT (and a DSL2 file put over SysEx loads through it) |
| Patterns (`OBJ_PATTERN`) | "PAT2" | unchanged | `rsv` is now the seed; 0 is fine |

- A PST2 save loaded by older firmware reads as an unknown magic, so its patterns load empty. That's the platform's
  usual downgrade behaviour for steps; the manual's release notes say so.
- `tools/dubwise_slots.py info` learns DSL3.

## Tests

- **`oldsave` extended:** a PST1 save whose kick steps hold live-recorded gates (non-zero) reboots as PST2 with
  those gates 0, every CHANCE 100 % and REPEAT off. The demo riddim renders the same as before the change
  (`cap_wav_diff` 0 against a capture from the old build's steps).
- **`locks.txt` (5.8):**
  - a TUNE lock moves that hit's pitch (`zc_hz`), a DECAY lock its length;
  - FLAM puts a ghost 20 ms early, and ×2–×4 are evenly spaced inside the step (from the event log);
  - undo, and a pass captured and replayed with locks and repeats (`cap_ev_diff` 0).
- **`prob.txt` (5.9):**
  - 50 % steps over 8 loops: the same hits on two runs of the same song (`cap_ev_diff` 0, `cap_wav_diff` 0);
  - RESEED changes them and is undone;
  - the screen's skip marks agree with what plays.
- **A slot round trip:** a DSL2 slot image (SysEx put) loads converted.

## Cost (estimate)

- **Image:** ~3 KB.
- **RAM and pool:** 0 (the fields fit in bytes we already keep).
- **CPU:** repeats are ordinary note-ons on the event list, and the chance is one hash a step (~0).
- **Tier:** all.

## Decisions for the owner

1. **Locks in a drum step's gate byte, with the PST2/DSL3 converter** (recommended). The other way is a separate locks
   object, but there's no flash left for one.
2. **The seed in the spare byte: 256 seeds, not the roadmap's 2 bytes** (recommended; plenty for "reseed until it
   grooves", and no format change).
3. **CHANCE values 100 / 75 / 50 / 25 %** as the roadmap has them, and on every channel (not only the drums).
4. **Locks on the three drums only** (kick, rim, hats), as the roadmap has them.
