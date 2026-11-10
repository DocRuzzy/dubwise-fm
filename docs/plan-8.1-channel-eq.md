# Plan 8.1: each channel's own EQ and stepped filter (for the owner's OK)

Roadmap 8.1. The decisions it builds on (the owner, 2026-11-10): the EQ is saved in scenes; the second core is coming,
but until it does the work fits one core; no overclocking. CLAUDE.md asks for a plan before any stored format changes:
this plan covers the changes, their versions, the converters and the tests that load old saves.

## 1. What it does

Each of the eight strips gets, before its fader and sends:

| Stage | Range | How it sounds |
|---|---|---|
| LO | -64..63 (±15 dB) | a low shelf at 100 Hz, one-pole: blunt, as period consoles were |
| MID | -64..63 (±15 dB) | a peak, Q about 1.4 (one biquad) |
| FREQ | 0..127 (200 Hz .. 8 kHz, log) | the MID's frequency, swept smoothly (the coefficients ramp each block): Scientist's sweeps |
| HI | -64..63 (±15 dB) | a high shelf at 5 kHz, one-pole |
| FILTER | -40..40 | the Big Knob's steps on this channel alone: above 0 the high-pass (the 9069B), below 0 the low-pass (the 9068B). Stepped, unsmoothed, like the Big Knob |

0 is flat or off for every stage, and a flat stage is skipped entirely. The bass, skank and organ, which render at half
rate, are EQ'd at that rate before upsampling.

**Playing it:**
- **The STRIP bank.** The SOUND view gets a STRIP bank for the focused channel (ALGORITHM picks the channel): KNOB 1 LO,
  KNOB 2 MID, KNOB 3 FREQ, KNOB 4 HI. It is the first bank after MAIN; channel 8 has no MAIN, so there it comes first.
- **The channel's filter: press and turn the Big Knob.** That steps the focused channel's own filter, and the screen
  says "BASS FILTER 500 HZ" (or "LP 2.5K"). A press released without a turn still toggles the main Big Knob's high-pass
  and low-pass, as it does now. Holding a channel's key was considered instead, but pressing a key mutes the channel.
- **Recording and recall.** The song's motion records every one of these, so a sweep is a move like any other. Undo
  covers turns, as it does for the other knobs.
- **The strip on screen.** A strip shows a small mark while its EQ or filter is in.

**IDs:** five of each strip's six spare fields, appended (`CHF_LO, CHF_MID, CHF_MID_F, CHF_HI, CHF_FILT`), neutral at 0.

## 2. The stored formats that change

There are three changes: scenes, slots, and (if the owner agrees, section 3) the live desk.

### 2a. Scenes: SCN2 to SCN3

- **What changes:** `scene_t` gains `int8_t eq[NCH][5]` (40 bytes a scene; int8 because every field's range fits). A
  scene captures and recalls the EQ and the channel filters with the rest of the desk, at the line, as now.
- **The version:** the OBJ_SCENES object's magic goes from "SCN2" to "SCN3".
- **The converter:** an SCN2 object loads as before, with every scene's EQ set to 0 (flat). A scene stored before 8.1
  was stored with no EQ, so recalling it flattens the EQ. That is what the desk sounded like when it was stored.
- **Memory:** the scenes live in the pool: +640 B (pool free 40.7 KB to about 40.1 KB, just above the 40 KB rule).

### 2b. Slots: DSL4 to DSL5

- **What changes:** a slot keeps the scenes as their bytes, so the slot's layout grows by 640 B.
- **The version:** "DSL5".
- **The converter:** a DSL4 slot's scenes are read in the old layout, with the EQ set to 0; everything else is as
  DSL4. DSL3, DSL2 and DSL1 keep their converters and gain the same step.
- **What it costs:** a slot holds about 2,813 moves instead of 2,920 (the motion pool is 4,096). An old slot carrying
  more than 2,813 moves still loads whole. Saving it back into a slot is refused with "TOO MANY MOVES", as any save
  over the limit is today (the project itself is untouched). The test checks it.
- **Sharing:** slot sharing over USB (4.3) carries the slot's bytes. `tools/` and `tests/slot_transfer_test.py` accept
  DSL5, and a DSL4 file sent to a newer firmware converts as above.

### 2c. The live desk: a new DESK object (needs the owner's decision, section 3)

- **The finding (2026-11-10, a host test):** SAVE keeps the tempo, sounds, patterns, scenes, song, FX and SETUP, but
  not the strips. A channel's level and echo send come back at their defaults after a power cycle. Song slots do keep
  them. The manual's SAVE row lists what it keeps and leaves the desk out, so this may have been deliberate. But 8.1's
  EQ would then be lost at power-off unless it sits in a scene.
- **What changes:** if yes, a new store object, `OBJ_DESK` ("DSK1"), appended to the objects. It holds every channel
  field a slot keeps (`project_param` over the channel IDs: levels, mutes, solos, sends, throw destinations and
  amounts, patterns, and the new EQ; not the momentary throws), 256 bytes. SAVE and autosave write it, and boot loads
  it.
- **The converter:** none needed. A save from before has no DESK object and boots with the desk at its defaults,
  exactly as now.
- **Backup:** the object joins `APP_BACKUP_OBJS`. An older backup restored has no DESK, so the desk takes its defaults.
  A newer backup restored on older firmware has an object that firmware doesn't know, so check that fm1-base's restore
  skips it. If it doesn't, that is an upstream note in the roadmap, not a change to the submodule.

## 3. The owner's decision before code

**Should SAVE keep the desk (2c)?**
- **Recommended: yes.** A project you switch back on should sound as you left it, and the slots already work that way.
- **The alternative:** keep today's behaviour and put the EQ only in scenes and slots. The live EQ would then be lost at
  power-off.

## 4. Cost

| | Estimate | How it's checked |
|---|---|---|
| CPU | four biquads on a channel measured ~3 % of the device (×80). The design (two one-poles, one biquad, the filter only when stepped) is ~1.5 % a channel EQ'd, half that on a tonal voice, nothing when flat. Every tier | `perf` with 0, 2, 4 and 8 channels EQ'd |
| RAM | about 1 KB of filter state and coefficients (17.5 KB free to about 16.5; the floor is 16) | the device build's map |
| Pool | +640 B for scenes (to about 40.1 KB free, the floor is 40) | the device build |
| Flash | a few KB of code | the device build |

After 8.1, both RAM and pool sit at their floors. 8.2 (the percussion) needs no pool and under 0.5 KB of RAM, so it
still fits. Anything after it must free memory first (review §5).

## 5. Tests

- **The EQ itself:** each stage's response, measured with a sine sweep and checked in the WAV. Flat is bit-exact (the
  bypass). The low end stays mono. The filter's steps match the Big Knob's frequencies, and a step's click survives.
  The MID sweep has no zipper noise.
- **Old saves:** an SCN2 object loads with flat EQ and its desks recall as before. A DSL4 slot loads (patterns, scenes,
  song, moves) with flat EQ. A DSL4 slot with more moves than now fit loads whole, and saving it back says TOO MANY MOVES. A DSL3 slot still
  loads. An old backup restores.
- **The live desk (if 2c):** level, send and EQ survive SAVE and a reboot. A save from before boots with the desk at
  its defaults.
- **Playing it:** a scene with EQ recalls it at the line. The motion records a MID sweep and plays it back. The Big
  Knob pressed and turned steps the focused channel's filter, and a press without a turn still toggles LP. Undo covers
  a run of turns.
- **CPU:** `perf` bounds with EQ on 2 and on 8 channels. The demo's busiest bars are unchanged with the EQ flat.
