# Dubwise FM: the hardware test list

**As of 2026-11-10.** Everything in Dubwise FM so far was built and tested in the simulator; none of it has run on an
FM-1. This list is what to check, measure and judge by ear once it is installed, for the owner and for beta testers.
Roadmap block 9.H acts on the results.

**To report:** [open an issue](https://github.com/DocRuzzy/dubwise-fm/issues/new?template=beta-report.md) (the
"Beta test report" form). One finding an issue is easiest to act on. You don't need to do the whole list: any
section helps, and **Install and remove** and **Saving** help most.

## How to report

With every finding, note:

- **The firmware version** (the release name, e.g. `FM-1_382…`; `fm1_install.py --info` shows it).
- **What you did,** step by step, starting from a known state (a new project, the demo song, UK STEPPERS).
- **What happened, and what you expected.**
- **How you were listening:** the FM-1's speaker, headphones, or the line out into what.
- **A phone recording or photo** if it's about a sound or the screen. A short clip says more than a description.
- **Whether it happens every time.**

If the sound drops out, clicks or freezes, open the PERF page straight away and write down what it shows (see
Stability and CPU).

## Install and remove

Do these first: they make sure nobody gets stuck with a firmware they can't leave. Start from an FM-1 on M-VAVE's V15.

- [ ] Install `dubwise.fwsc` with the hub's web installer (or `python3 base/tools/fm1_install.py app/build/dubwise.fwsc`). After the restart the boot splash shows the SpIllt Beats mark.
- [ ] On the computer the FM-1 shows up as **DUBWISE FM-1**, and `fm1_install.py --info` reports `FM-1_382…`.
- [ ] Note which flash chip your FM-1 has (`--info` prints its JEDEC id: `856014` is the older Puya, `B36014` the newer UC25HQ80). Both should work; we'd like a report from each.
- [ ] Leave it: install V15 (or another firmware) over Dubwise. `--info` shows the new identity.
- [ ] Come back from another firmware (a Felucca one, then SLOOP, LOOM-8 or X0X): install Dubwise and remove it again. Note any firmware that refuses.
- [ ] The recovery path: if an install fails, plug the FM-1 straight into the computer (no hub), restore stock with the web installer or M-VAVE's FM1 Open, then install again. Say whether the README's steps were enough.

## Stability and CPU

Every CPU figure in the roadmap is a simulator estimate (host time × 80). These readings replace them. Open SETUP →
PERFORMANCE (the PERF page): it shows each part's cost, the TOTAL, the PEAK block over 2 s, DROPOUTS, the CLOCK in MHz
and the TIER; bottom right, STACK M and I (bytes of the main and interrupt stacks never used since switch-on). Let
each case run a minute, then note its row.

| Case | TOTAL | PEAK | DROPOUTS | TIER | CLOCK | STACK M · I | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A new project, playing | | | | | | | |
| The demo song, start to end | | | | | | | |
| UK STEPPERS, every effect up (phaser on MIX, drive on MIX, cross-sends, spring gate, BBD) | | | | | | | |
| The heavy scene: all 8 channels EQ'd and panned, DESK MCI, sirens and a one-shot on top | | | | | | | |
| A scene recall, a slot load and a Dice roll while playing (watch PEAK) | | | | | | | |

- [ ] No clicks or dropouts in a 30-minute session, playing and stopped.
- [ ] The guard: does the TIER ever drop by itself? If it does, note what was playing and whether you heard it change.
- [ ] The dub push (black 3) held for 20 s with the feedback at its longest: it rings and saturates but never runs away or goes silent.
- [ ] After stopping, the echo and spring die away to real silence (no hiss, no hum, no DC thump).
- [ ] The screen stays responsive while playing: the knobs and the cycle bar never lag the sound.

## The panel

The simulator can't tell us how the keys feel. These are about touch, timing and what the eye catches.

- [ ] **Throws feel immediate.** Tap white 9–16 on a beat: the throw lands with the hit, not after it.
- [ ] **Velocity is even across the throw keys** (SETUP → THROW VELOCITY on). The same hit on white 9, 12 and 16 gives about the same size of throw. Note any key that reads hotter or colder.
- [ ] **The top-row throws (white 13–16) have velocity only.** Check a throw there is still usable without pressure.
- [ ] **Black 1 with a throw held:** a light touch adds a little spring, leaning in adds all of it.
- [ ] **Black 3, dub push:** a light touch only tips the echo over; leaning in screams; easing off settles it.
- [ ] **Black 4, tape dive:** a light touch sags; leaning in dives deep.
- [ ] **Black 5, drop to wet:** a light touch leaves part of the band; leaning in drops it all.
- [ ] **Blacks 6 · 7, sirens:** the pitch rises smoothly as you lean, up to an octave, with no stepping.
- [ ] **How far a light touch reaches** on those keys feels right. A plain tap is about half the move: too little, or about right?
- [ ] **Mutes land on the line** (the quantise): the runway counts down and the mute happens on the one.
- [ ] **The encoders' push switches** work on every knob (the Big Knob press toggles HP/LP; a press with a channel key held turns its filter off).
- [ ] **The Big Knob's steps** click audibly and evenly; none skipped when turned fast.
- [ ] **The LEDs:** the knob colours and key lights match the manual, and read clearly in daylight and in the dark.
- [ ] **The screen:** text readable at arm's length; the amber EQ mark and the pan tick on a strip are visible; nothing flickers or tears.
- [ ] **The knob caps** (Adafruit 5542, 6 mm shaft): they fit, and their colours line up with the knob colours on the screen.
- [ ] **The battery** reading and BATTERY LOW come at a sensible point.

## Saving

A lost save is the worst bug an instrument can have. Power-cycle often while testing.

- [ ] **SAVE, then power off and on:** the tempo, sounds, patterns, scenes, song, FX, SETUP and the desk (levels, sends, EQ, pan) all come back.
- [ ] **Autosave:** stop, leave the FM-1 alone for a few seconds, watch SAVE's light go out, switch off: it's all there.
- [ ] **Switching off while playing** (or pulling USB power) never corrupts the last save.
- [ ] **Song slots:** save to each of the four, load each back, and load a slot while playing.
- [ ] **Backup and restore over USB:** back up, change things, restore: it's as it was.
- [ ] **An old save:** a project saved by v0.3 loads after updating, EQ flat, everything else as it was.
- [ ] **Patches:** save a sound, rename it (SAVE held, KNOB 1 and KNOB 2), switch off: name and sound are kept.
- [ ] **Your own one-shots** (sent from the computer) load on channel 8 and play on blacks 8–10.
- [ ] **NEW PROJECT** gives a clean start and keeps SETUP and the slots.

## Connections

- [ ] **OUTPUT AUTO:** with nothing in the headphone jack the speaker gets BASS+; plug headphones in and it switches to the plain line (the SETUP row says which). Unplug: back to BASS+. The simulator can't detect the jack, so this is untested.
- [ ] **The low end stays mono** on headphones with channels panned hard: the kick and bass sit in the middle.
- [ ] **No hum, hiss or thump** at the line out, on USB power and on battery.
- [ ] **USB audio:** pick "DUBWISE FM-1" on the computer, record the mix, then the stems (SETUP → USB STEMS, CHANNELS): each channel on its own track, in time, no dropouts.
- [ ] **Playing a computer's audio through the FM-1** (DUBWISE FM-1 as the output): clean, and how much delay.
- [ ] **MIDI CLOCK IN:** a DAW or drum machine starts, stops and sets the tempo; the bars stay in step over 10 minutes.
- [ ] **MIDI CLOCK OUT:** Dubwise leads another device, in step.
- [ ] **MIDI LEARN:** a controller's knob, fader and pad each take over a Dubwise control.
- [ ] **A USB MIDI keyboard or controller** on the FM-1's own USB port (USB PORT: KEYBOARD), powered by a hub or battery.
- [ ] **The SMC-Mixer as the desk** (if you have one): its eight strips move Dubwise's.

The live input (your computer's audio on channel 8) isn't in this release: it waits on a platform change.

## Listening

These sounds and levels were set on studio monitors from simulator renders. Judge them on the FM-1, on its speaker and
on headphones, and say what you'd change.

| What | Listen for |
| --- | --- |
| The demo song (SETUP → DEMO SONG) | Vox and sirens not too loud; the horns punchy and horn-like |
| The horns (HORN, FM+HORN) | A tongued attack and a brassy blare, not a synth pad |
| A hat throw (white 11, hats muted) | Audible in the echo? It comes back 24 dB under the hats. Gritty or clean? |
| A hat throw with black 1 leaned on | The spring brings it back about 9 dB louder and brighter. Should hats go to the spring by default? |
| The bass with SUB and DRIVE both up | The sub clean under the drive; changed from older sounds |
| The seven factory kits | Each sounds like its room or instrument; levels even between kits |
| The organ: BUBBLE and B-3, the Leslie slow and fast | The registrations and the Leslie's speed |
| Dice rolls (DICE bank) | Rolls stay musical; none silent, painful or too loud |
| The sirens, CLASSIC and NJD shapes | Level against the band; the lean's octave range |
| The deejay voice (SAY) | Intelligible enough; the level when thrown into the echo |
| The spring: crash and sends | Long and splashy, not crushed or metallic |
| The echo: TAPE and BBD, wow on | Warm repeats; the wow subtle |
| Big Knob RESONANCE LOW and HIGH | The steps ring musically, not painfully |
| Channel EQ (STRIP bank) and pan | The MID sweep smooth; the shelves useful |
| DESK MCI against CLEAN | MCI thickens loud channels without mud |
| The FM-1's speaker, OUTPUT BASS+ | The bass present without the speaker rattling |

## Playing it

The tests prove each feature works; only people can say whether it's playable. The most useful report here is from
someone who has never seen Dubwise.

- [ ] **The first ten minutes:** a first-timer follows the quick start at the top of the manual with no other help. Note where they get stuck and what they try first.
- [ ] **The runway:** can a first-timer land a drop on bar 17 without counting, from the countdown alone?
- [ ] **Drill C1** (mutes on the line) and **C4 drills 1–2** (the echo pushed to the edge and back) from the mastery guide, on the device.
- [ ] **A 15-minute dub of the demo or UK STEPPERS:** what did you reach for that wasn't there, or was too deep in the menus?
- [ ] **The help cards and the cheat sheet:** enough to find a feature without the full manual?
- [ ] **Is there too much?** Would a SIMPLE setting help: mutes, throws, four knobs and the Big Knob, everything else asleep?
- [ ] **Beyond reggae:** would a neutral template (four-on-the-floor with chord stabs) make you use it more?
- [ ] **Reading a value while turning:** would a large readout of the value, fading after a second, help?
- [ ] **Accessibility:** REDUCED MOTION, and the screen reader path if you use one.

## What these results decide (the owner's list)

| Decision | Waits on | Roadmap |
| --- | --- | --- |
| Replace the ×80 estimate with measured figures per part | The PERF table | 9.H2 |
| How much headroom the Studio tier keeps (the spring first: two thirds of the default desk) | The PERF table, the heavy scene | 9.H2 |
| Build the second-core split (echo and spring) or strike it from the docs | The PERF table | 9.H2 |
| Spread bulk changes (scene recall, slot load, kit, patch, Dice) over blocks | The PEAK reading during bulk changes | 9.H5 |
| A quieter first step before a tier drop | Whether a tier drop is heard | 9.H6 |
| A SIMPLE setting; a neutral template | Playing it | 9.H3 |
| The hats' throw: echo only, or the spring by default | Listening | 9.14 / 9.H3 |
| The echo's half-band decimator (in the simulator only) | Hat throws heard on the device | 9.7 |
| How far a light touch reaches on blacks 1, 3–7 | The panel | 9.15 / 9.16 |
| The large value readout; one rule for buttons with several pages | Playing it | 9.H7 |
| A versioned settings object for the next settings | Before any new setting is added | 9.H4 |

