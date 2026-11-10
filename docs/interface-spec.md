# Dubwise FM — Interface and workflow spec (v0.3, 2026-11-09)

v0.3 adds the sound-design block and the B-3 organ model (roadmap 5.x, from `roundup-review-2026-11.md`): 4.1.3 stop twice, 4.3 step locks and seeded probability, 4.9 the SOUND view, 4.10 Dice, 4.11 one-gesture reach and the help index, 5.5 the legibility floor.

Goal: make the "dubwise" method (mixer as instrument; mutes, throws, feedback, Big Knob) playable by feel, with building a riddim as the quick, secondary task. Roadmap refs are in brackets.

## 1. The hardware (felucca-bsp / fm1-base)
| Control | Count | Type | Notes |
|---|---|---|---|
| MASTER | 1 | Absolute pot | Volume only |
| SELECT, PRESETS, ALGORITHM | 3 | Endless detented encoders (left) | One click = one step |
| KNOB 1–4 | 4 | Endless detented encoders (under the screen) | No value jumps when banks change |
| Buttons | 14 | FX, SEL, ENV, LFO, EDIT, GLO / HOME, SAVE, ARP, SEQ, PLAY, REC; OCT−, OCT+ | Have LEDs |
| White / black keys | 16 / 11 | Piano layout starting on C | LEDs with levels; velocity and pressure available (`APP_TOUCH`) |
| Screen | 1 | 240×240 colour ST7789, SPI + DMA | Dirty-rect strips only; drawn in the main loop |
| Audio | — | Stereo I2S out to codec, 44,117.6 Hz | No analog input. USB audio in from a host exists (§8) |

**Still to verify on the device:** encoder push switches, LED colours, how usable velocity is for throws.

## 2. Interaction rules
1. **Tap toggles, hold is momentary.** Throws act only while held, with a latch available.
2. **Performance actions never need a shift.** Shift (hold HOME) is only for editing, file and setup.
3. **Hold-to-focus.** Holding a mute key turns KNOB 1–4 into that channel's strip until you release it.
4. **The left encoders mean the same thing everywhere:** SELECT = Big Knob; ALGORITHM = focus channel; PRESETS = whatever the page is about (scene, pattern or sound preset).
5. **A button opens its page; pressing it again goes to the next bank.**
6. **One visible quantise setting.** On the DUB page, OCT− and OCT+ step through Beat / 1 / 2 / 4 / 8 / 16 / 32 bars. PLAY held + action = fire immediately.
7. **One LED language:** bright = on/playing; dim = available/muted; slow blink = cued; fast blink = recording.
8. **At most two simultaneous presses.** No long press is needed to start a performance action.

## 3. Channels
| # | Channel | Default voice | Default throw destination |
|---|---|---|---|
| 1 | Kick | Pitch-envelope sine | Echo |
| 2 | Snare / rim | Inharmonic FM + noise | Both |
| 3 | Hats | Noise + HPF / inharmonic FM, choke | Echo |
| 4 | Bass | 2-op FM, mono | Echo |
| 5 | Skank | 2-op FM chop, follows the chord lane | Spring |
| 6 | Organ bubble | 2–3-op additive, follows the chord lane | Spring |
| 7 | Lead | Melodica / horn section | Echo |
| 8 | Vox / FX / Live in | Samples, siren, USB audio in | Echo |

One colour per channel, always paired with its number and a shape cue (filled = playing, outlined = muted, hatched = solo-muted).

## 4. Pages
| Button | Page | Banks (press again) |
|---|---|---|
| **HOME** | **DUB** (home: live desk) | — |
| HOME ×2 | ARRANGE | Scenes → Song list |
| FX | Effects | Echo → Spring → Phaser/Width → Cross-sends |
| LFO | MIXER | Levels/sends → Filters/drive → Throw destinations |
| SEQ | STEPS | Steps; four patterns a channel (A–D); length, grid (1/8–1/32, triplets), swing; template; live recording (REC) |
| EDIT | SOUND (4.9) | MAIN (macros) → raw banks → DICE (4.10) → back |
| ARP | KEY & CHORDS | Key → Chord lane |
| GLO | SETUP | MIDI · tier lock · quantise · cycle/runway · accessibility · live input |
| SEL | LIST | Any page as a text list |
| SAVE / PLAY / REC / ENV | Save (hold = copy) / transport / arm recording / accent and knob reset | — |

### 4.1 DUB page
| Control | Function |
|---|---|
| White 1–8 | Mute ch 1–8. Tap toggles; hold inverts momentarily and focuses the strip. **Double-tap = solo** (additive; double-tap again to un-solo; quantised). HOME + white = solo, as an alternative |
| White 9–16 | **Throw** ch 1–8 to the channel's destination (Echo / Spring / Both, set in MIXER bank 3). Held: send to max and loop gain lifted. Double-tap latches. With velocity, amount follows velocity |
| Black 1 | Spring crash. Held while a throw is held, it adds the spring to that throw |
| Black 2 | Echo hold/freeze |
| Black 3 | Dub push (feedback past unity, capped) |
| Black 4 | Tape dive |
| Black 5 | Drop to wet |
| Black 6–7 | Siren A / B |
| Black 8–10 | Vox/FX one-shots 1–3 |
| Black 11 | Drum & bass only |
| KNOB 1–4 | Echo time · Feedback · Echo return · Spring return |
| KNOB 1–4 (mute key held) | Level · Echo send · Spring send · Filter |
| SELECT / ALGORITHM / PRESETS | Big Knob / focus / cue scene |
| OCT− / OCT+ | Launch quantise |
| REC | Arm song capture |

#### 4.1.1 As built in P0.1 (2026-11-08)
- **Tap / hold / double tap on a mute key.** The press cues the toggle at once (no wait to tell a tap from a hold). A hold (≥ 300 ms) cues the mute back on release. A second press within 300 ms of a tap's release cues the first toggle back and cues the solo toggle. All of them use the current quantise, so a double tap inside one quantum never flickers the mute.
- **Solo:** additive; a muted channel stays muted when soloed (mute wins); solo-muted strips are hatched.
- **Quantise:** BEAT / 1 / 2 / 4 / 8 / 16 / 32 bars from the transport's start (4/4). A cue fires `P_LEAD` = 10 ms before the line (spec: "fired slightly early"; it also covers the 2.9 ms gain ramp). A cue made inside that window fires at once. While stopped, everything acts at once; STOP fires anything waiting.
- **PLAY** toggles on release; held with a key it is "now" for that key and does not stop the transport.
- **Tempo** on HOME + SELECT (1 BPM a click), as in 4.6. **GLO** toggles test tones on every channel until SETUP and the voices exist.
- Throws (P0.2) are immediate, not quantised (a throw catches a word). Destination on hold + KNOB 1 until MIXER bank 3 exists.
- Hold-to-focus (P0.3): a held mute key turns KNOB 1–3 into level / echo send / spring send; turning one cancels the key's momentary mute flip.
- Not yet: the channel filter, the other black-key effects, the Big Knob in the centre.

#### 4.1.2 As built in P0.6
- Quantise gains **CYCLE** (after 32 bars): the next "one" of the cycle. Templates (STEPS) land on the bar line itself, not early.
- GLO opens a first **SETUP** page (a list: KNOB 1 / SELECT moves, KNOB 2 / PRESET changes); test tones moved there.

#### 4.1.3 Stop twice: tails off (roadmap 5.2; built 2026-11-09)
- A stop (PLAY tapped while playing) stays as built: notes and motion let go; the echo and spring ring out.
- **A second PLAY tap within 400 ms of the stop** doesn't start the transport. It fades the returns over 20 ms, then clears the echo tape, spring and phaser. Said and shown: "TAILS OFF".
- After 400 ms, PLAY starts as before.

### 4.2 ARRANGE page
- **White 1–16:** recall scene, quantised. **SAVE held + white key:** store.
- **Scene scope** (KNOB 4 at store time):
  - **Desk** = mutes, solos, sends, throw destinations, HPF step, return levels.
  - **Desk + Patterns** = also each channel's pattern. Shown with a P badge.
- **Black keys:** section markers at the current bar.
- **KNOB 1–4:** song position · section length · loop · scene scope.
- **REC + PLAY:** capture into the per-bar song, with the motion inside each bar (throws, mutes, knob moves: P2.7). **HOME + REC:** undo (the bars, not the motion). **SAVE + REC:** clear the motion.

### 4.3 STEPS page
**Built for 5.8 / 5.9 (2026-11-09; plan `docs/plan-5.8-5.9-steps.md`):** an on step pressed turns off at its let-go unless a knob edited it meanwhile (hold to edit); REPEAT on ENV held + a white key (the drums); CHANCE on black 11 held + a white key (every channel); RESEED on HOME + ENV; a drum step held: KNOB 1 its DECAY lock, KNOB 2 its TUNE lock (kick, rim); the big view marks CHANCE (%), REPEAT (F or dots), locks (a tick) and the steps that skip this loop (hollow).

- **White 1–16:** steps; OCT± page through 17–64.
- **Black 1–8:** channel. **Black 9 / 10 held + white:** accent / ghost (built). **Black 11 held + white:** the step's chance, 100 → 75 → 50 → 25 % (roadmap 5.9).
- **REC** on bass, skank or lead turns the keys into a keyboard.
- **Templates** (one drop / rockers / steppers / flyers) are written as editable steps.
- **Drum step locks (5.8).** A kick, rim or hat step held: KNOB 2 = its TUNE lock (hats: closed / open as now), KNOB 1 = its DECAY lock (with no step held, KNOB 1 is still the length). **ENV held + white** cycles the step's REPEAT: off · FLAM · ×2 · ×3 · ×4. A locked step shows a dot; a repeat shows its count.
- **Probability is seeded, never free-running (5.9; principle 1).** Whether a step plays this loop is a hash of the pattern's seed, the step and the loops since launch. The same launches give the same notes, so a captured song replays exactly. Steps that will skip this loop are drawn hollow. **HOME + ENV** = RESEED (one undo step).

### 4.5 Key / scale / chord lock ("can't play a wrong note") [0.8]
When the lock is on, keys outside the allowed set are **silent**. They are never remapped, so the keyboard keeps normal piano spacing and what you learn on it transfers to a real keyboard.

- **Modes:** Off · **Scale** (notes of the key and scale) · **Chord** (only the tones of the chord the chord lane is on now; follows chord changes live).
- **Scales offered** (reggae/dub-oriented; treat the list as convention, not statistics):
  - Natural minor (Aeolian), Dorian, minor pentatonic, minor blues: roots and rockers minor vamps, bass lines.
  - Major (Ionian), Mixolydian, major pentatonic: lovers rock, ska/rocksteady-derived and major riddims.
  - Harmonic minor and Phrygian: the "Far East" melodica colour.
- **Where it applies:** live keyboard play and REC keyboard entry (bass, skank, lead); step-pitch entry on STEPS; optionally external MIDI note input (SETUP: "Lock MIDI in", default off).
  - It is an **input** filter only: notes already recorded play as recorded, even if the key changes later. A per-pattern "scrub out-of-key notes" tool is a separate, explicit command.
- **Visual guide (key LEDs):** root = bright, other allowed notes = dim, disallowed = dark. In Chord mode the lit set changes with the chord, so the keybed shows the current chord.
- **Controls:** ARP → KEY & CHORDS bank 1: KNOB 1 root · KNOB 2 scale · KNOB 3 lock mode · KNOB 4 octave. **HOME + ARP** toggles the lock from any page. When on, the header shows a `LOCK` chip, plus `CHD` in Chord mode.
- **Screen reader:** announces "lock on, D dorian" and "chord lock, G minor".
- **Cost:** a 12-bit mask per scale and a chord-tone mask updated on chord change. UI only, no audio CPU.

### 4.6 Shift (hold HOME)
| HOME + | Function |
|---|---|
| REC / GLO | Undo / redo (built 2026-11-09: GLO, because HOME + PLAY became the rewind, P3.5) |
| SELECT | Tempo |
| SEQ taps | Tap tempo (built 2026-11-09: the last four gaps averaged; 2 s pause restarts; refused under MIDI clock in) |
| ARP | Key/scale lock on/off |
| SAVE | Save as… |
| White key | On STEPS (built 2026-11-09, 4.8): white 1–4 launch the channel's pattern A–D, 9–12 every channel's. On DUB: solo |
| Black key | (On STEPS the copy is SAVE + white 1–4: into A–D of the same channel, 4.8. Black keys here: one-shots reversed, section words on ARRANGE) |

### 4.8 The session: patterns and live recording (built 2026-11-09)
Modelled on Ableton Live's session view, the OP-Z's per-track patterns and the RC-505's loops.
- **Patterns.** Four a channel (A–D), each with its own length (1–64 steps), grid (1/8, 1/8T, 1/16, 1/16T, 1/32) and
  swing (50–75 %). One plays (CHF_PAT). A launch lands on the quantise line and starts the pattern from its top.
  Scenes "P" store which pattern each channel plays (a row); the song's bars store them too.
- **Timing.** 240 fine ticks a beat (3.3 ms at 75 BPM). A step lands on its grid line plus swing plus its own offset
  (within half a step: played, or nudged with KNOB 3 while held) and holds for its gate (KNOB 4 while held).
- **STEPS controls.** HOME + white 1–4 launch; HOME + white 9–12 launch on every channel; SAVE + white 1–4 copy into;
  HOME + OCT+ double; HOME + OCT− clear; HOME + KNOB 1 grid; KNOB 3 swing. DUB: a mute key held + PRESET launches that
  channel's next pattern (the mute as it was).
- **Live recording.** REC while playing: an empty pattern is armed for the next bar line (the click counts in),
  records one pass and loops; a pattern with steps records at once until REC (OVERDUB or REPLACE). Notes keep their
  timing (REC QUANTISE FEEL / HALF / GRID), less REC OFFSET; velocity becomes the level; bass and lead keep the
  held length. Drum channels: whites 1–4 are pads (kick, rim, closed hat, open hat); skank and organ: whites 1–3, the
  chord low / mid / high. Stopped: REC is step entry at a cursor, and PLAY then counts in a bar. A pass is one undo step.

### 4.9 The SOUND view (roadmap 5.3–5.5, 5.7, 5.10)
EDIT opens it for the focused channel (ALGORITHM changes the focus). EDIT again steps through the banks: **MAIN** (four macros) → the raw banks, four fields each → **DICE** (4.10) → back to the page.

| Ch | MAIN macros (bank 1) | Raw fields (built → **new, appended**) | Models (5.7) |
|---|---|---|---|
| 1 Kick | PITCH · PUNCH · DECAY · DRIVE | TUNE, SWEEP, PTIME, DECAY, CLICK → **DRIVE, CLICK TONE** | SINE · FM THUMP · BOOM |
| 2 Rim | TUNE · SNAP · DECAY · BODY | TUNE, DECAY, NOISE, INDEX → **PITCH DROP, HPF** | CROSS-STICK · SNARE · SYNDRUM |
| 3 Hats | TONE · CLOSED · OPEN · HPF | TONE, CLOSED, OPEN, NOISE → **HPF** | METAL · NOISE |
| 4 Bass | TONE · PLUCK · LENGTH · WEIGHT | INDEX, IDECAY, DECAY, CUTOFF, RATIO, SUB, SUSTAIN → **FEEDBACK, DRIVE, GLIDE** | ROUND · PLUCK · HOLLOW · STEPPERS |
| 5 Skank | TONE · LENGTH · COLOUR · ATTACK | INDEX, DECAY, TONE, RATIO → **ATTACK** | — |
| 6 Organ | BUBBLE: DRAWBAR · LENGTH · CLICK · DETUNE. B-3: REGISTRATION · PERC · ROTARY · DRIVE | DRAWBAR, DECAY, CLICK → **DETUNE**; B-3 (5.15): **nine drawbars, PERC (off / 2nd / 3rd), PERC FAST, CLICK, VIB/CHORUS (off, V1–3, C1–3), ROTARY (off / slow / fast / brake), DRIVE** | BUBBLE · B-3 |
| 7 Lead | BRIGHT · ATTACK · RELEASE · VIBRATO | INST, BRIGHT, ATTACK, RELEASE, VIBRATO → **DETUNE** | (INST: melodica / brass) |
| 8 Vox/FX | (MAIN = the siren's PITCH · RATE · DEPTH · WAVE) | as built; LIVE stays on its own | — |

- **Macros** are stored voice fields. Turning one rewrites its raw targets along fixed curves (e.g. kick PUNCH: SWEEP up, PTIME down, CLICK up). The engine reads only raw values. Turning a raw field afterwards leaves the macro where it was; MAIN marks a macro whose targets have since moved with a small dot.
- **MODEL** (built 5.7, 2026-11-09): the last raw field of the kick, rim, hats and bass, not the first: putting it first would have moved every knob a player already knows. The models are settings of the one voice, so every field still means the same thing in each. Shown by name.
- **Built (5.3, 2026-11-09):** `app/sound.c`. Bank 1 is MAIN on channels 1–7; VOX keeps the siren's fields as its first bank, no macros. A macro runs −64..63 with 0 at every target's default (so an older save, whose macro fields read 0, sounds as it did), and each target runs a straight line from its value at −64 through its default to its value at +63 (the table `MACRO_T`). A macro learned by MIDI moves its targets too. The footer drops the stored names' "m." prefix.
- **Bounded on purpose:** the bass's index has a cap that keeps the fundamental sine-like; decays have caps (they set how long a voice lives); the kick ends 40–60 Hz.
- **Patches (5.4; built 2026-11-09, `app/patches.c`, plan `docs/plan-5.4-patches.md`).** On a user patch shown (as browsed to, or edited since: the footer's name gets a *), SAVE + PRESET overwrites it, asked twice; otherwise it saves to the next free slot, named KICK 01 … Never while playing (STOP TO SAVE). The library is the device's, in every project. PRESETS browses the focused channel's patches: factory first, then the 16 user slots; the name shows in the header. **SAVE + PRESETS** stores into the next free user slot (a second SAVE + PRESETS within 3 s overwrites the one shown, asked twice as in roadmap 4.9). A load is one undo step.
- **Your own one-shots (5.14; built 2026-11-09, `app/shots.c`).** Black keys 8–10 play whatever is on the FM-1: the factory's BIM, HORN, LASER, or three sounds put over USB (`dubwise_slots.py shots put`), announced by their names (8 characters). SETUP → ONE SHOTS shows FACTORY or YOURS; KNOB 2, asked twice, brings the factory's back (stopped). A put is refused while playing.
- **Kits (5.5; built 2026-11-09, `app/kits.c`).** SAVE loads the kit picked (as SETUP's slot rows do); its voices are factory patches too, named TUBBYS, CH.ONE, BLACKARK, STUDIO1, SHAKA, SLENG, ROCKERS. SETUP → LOAD KIT: TUBBY'S, CHANNEL ONE, BLACK ARK, STUDIO ONE, SHAKA STEPPERS, SLENG TENG, ROCKERS UPTOWN. KNOB 3 picks VOICES or VOICES + BUS. Asks twice over unsaved sounds.
- **Envelope view (5.10; built 2026-11-09, `app/envview.c`):** the whole centre (not on the DICE bank); the loudness filled with a white line, the second curve amber; the playhead from the engine's per-channel hit count (`eng_trig_count`); the drawing's width each voice's longest time (a drum to its 60 dB, a tonal voice held 400–900 ms then released). The centre draws the focused voice's amp envelope and its pitch or index envelope from the current values, with a playhead from each note-on at ~20 Hz. Reduced motion: no playhead.
- **B-3 (5.15; built 2026-11-09).** MODEL (raw bank 2) picks BUBBLE or B-3. The raw banks after the bubble's four: MODEL 16′ 5⅓′ 8′ | 4′ 2⅔′ 2′ 1⅗′ | 1⅓′ 1′ PERC PERC FAST | VIB/CHORUS ROTARY DRIVE LESLIE (v0.3: LIGHT, or FULL, setBfree's model, Studio only; roadmap 4.13); drawbar values show 0–8 like the real bars (stored as offsets from 888 000 000). With the B-3 the SOUND view's centre draws the nine drawbars pulled down as on the real one (16′ and 5⅓′ amber for brown; 8′ 4′ 2′ 1′ white; the rest dim), each with its value and footage, and the rotor's state (FAST, SLOW …) at the right. MAIN is REGISTRATION · PERC · ROTARY · DRIVE: REGISTRATION steps through eight drawbar settings, soft to bright (the middle, 0, the default; the list `B3_REGS` in `sound.c`, chosen by ear: **to be reviewed by ear**). ROTARY's speed is a parameter: MIDI-learnable (4.11, e.g. a mod wheel), captured as motion, stored in scenes. A B-3 chord holds for its step's GATE (a step if AUTO) and dies over DECAY; a pad strikes it as a stab. Stereo horn in the Studio tier only (open question 5: the owner chose it).
- **Screen reader:** "SOUND, KICK, MAIN", then each value as it turns; patch and kit names on load.

### 4.10 Dice: the sound randomiser (roadmap 5.6, 5.12)
**Built (5.6, 2026-11-09, `app/dice.c`):** as below; the bounds table `DICE_B`; the history in the pool (8 a channel); MORPH (KNOB 3) built in 5.12: A is the sound when a run of turns began from 0 %, B the history entry KNOB 4 shows; 5 % a detent; a switch at 50 %; held fields left. DICE ON LINE is stored in the settings record's spare byte (no format change).

A sound-design tool, fired only by the user (principle 1, clarified). It acts on the focused channel's voice and nothing else: never levels, sends, the dub bus, patterns, LIVE, or another channel.

| Control | Function |
|---|---|
| **ENV** (on the DICE bank) | Fire: ROLL or MUTATE, as KNOB 2 says |
| **HOME + EDIT** (any page) | Fire on the focused channel without opening the view |
| KNOB 1 | AMOUNT, 5–100 % (MUTATE's reach) |
| KNOB 2 | MODE: ROLL (a new patch inside the bounds) / MUTATE (a step away from the current) |
| KNOB 3 | MORPH A↔B (5.12; blank until built): A = the current, B = the history entry shown |
| KNOB 4 | HISTORY: the last 8 results of this channel; turning it plays each one in. Leaving it there keeps it |
| **ENV held + KNOB n** (MAIN or a raw bank) | HOLD that field (Dice leaves it alone). A held field shows a lock |
| SAVE + PRESETS | Keep a result as a user patch (4.9) |

- **Undo and capture:** each fire is one undo step, and is captured as motion like a knob turn, so a recorded pass replays the same sound changes.
- **When it lands:** at once by default. SETUP → DICE ON LINE makes a fire wait for the next quantise line (a performance move; the key blinks like any cue).
- **Said:** "ROLL, KICK" / "MUTATE 20, BASS"; the history position as "3 OF 8".

### 4.11 One-gesture reach and the help index (roadmap 5.11)
- **DUB → a channel's pattern settings:** hold its mute key + SEQ. STEPS opens on that channel with KNOB 1 length, HOME + KNOB 1 grid and KNOB 3 swing to hand, and the mute is left as it was. HOME returns to DUB.
- **SETUP → HELP:** every help card (4.7) in one list, readable anywhere (SELECT scrolls, KNOB 2 opens one).

## 5. Screen
### 5.1 DUB page layout (240×240)
```
┌──────────────────────────────────────┐ 0
│ 74▶● DROP ▮▮▮▮▯▯▯▯▯▯▯▯▯▯▯▯ 5/16 Q:1BAR│ header (20 px): tempo, transport, section, cycle bar, bar n/len,
├──────────────────────────────────────┤        quantise, guard tier badge; amber on the runway
│ 1  2  3  4  5  6  7  8         SOLO  │ channel strips (~70 px): level/mute, send ticks,
│ ▮  ▮  ▯  ▮  ▮  ▯  ▮  ▮               │ throw-destination glyph (E / S / ES), cued blink, throw flash
├───────────────────┬──────────────────┤
│  ECHO             │  BIG KNOB      ③ │ centre (~100 px): echo repeats (red past unity), HPF arc;
│  ▌▌▍▍▎▎▏          │   ╭───╮ 500 Hz   │ runway countdown numeral
├───────────────────┴──────────────────┤
│ TIME 1/8.  FB 4.2s  ECHO 70  SPRG 40 │ footer (~40 px): the four knobs
└──────────────────────────────────────┘ 240  + 2 px border for runway pulses
```

### 5.2 Feedback rules
- A ~1 s large readout overlay when any control turns.
- Cued actions blink on key and screen, with "lands in n bars".
- A throw flashes its channel and creates a new echo repeat on screen.
- The echo panel turns amber near unity and red past it. The limiter's gain reduction is shown as a bar.
- Guard tier changes show as a badge plus a toast. Hold any button for help (built 2026-11-09, `help.c`: a second on its own shows the button's card for this page in the page's place; it stays after the let-go until the next press, swallowed, or 12 s; a press or turn while the button is still held passes through).

### 5.3 Rendering budget
Dirty regions only. Meters at 20–30 Hz, echo panel at ~20 Hz, runway border on beat edges. No full redraws during performance.

### 5.4 Cycle countdown ("runway")
- **Cycle length:** 8 / 16 / 32 / 64 bars (default 16), separate from launch quantise. In song mode it follows the next section boundary.
- **Cycle bar:** in the header, one segment per bar (grouped in 4s); the current segment fills beat by beat.
- **Last 4 bars:** the header turns amber and a big numeral counts 4 → 3 → 2 → 1.
- **Last bar:** the screen border pulses each beat. Optionally the white-key LEDs flash too.
- **Downbeat:** a green "ONE" flash.
- **Cued actions** show "lands in n bars" and fire on the one.
- **Settings:** runway length 1/2/4/8; key runway; numerals; optional audible cue tick (default off).
- **Accessibility:** not colour-only. The screen reader announces "four bars… one". Reduced-motion mode is available.
- **Cost:** UI only.

### 5.5 Legibility floor (roadmap 5.11)
The roundup review docked firmwares for unreadable pages and waits between menus (`roundup-review-2026-11.md`). Every page meets these, checked against the host screenshots:
- Any value you perform with (knob readouts, the Big Knob, quantise, the bar count, the patch name) uses the page font or larger, never the smallest one. The smallest font is only for labels beside a value.
- No page transition is animated. A page or bank change draws its first frame within one main-loop frame of the press, and input is never ignored while drawing.
- At most eight fields of text a bank; nothing scrolls during performance.
- Contrast and shapes as §6 (not colour-only).

## 6. Accessibility
- Screen reader (fm1-base built-in) for pages, values, focus, cued and fired actions, bars and runway.
- Not colour-only. High contrast. SEL list view.
- **Comfort settings:** tolerant tap/hold, latch-by-default throws, overlay duration, left/right swap, reduced motion, runway options.
- Undo everything. Fixed MIDI CC map.

## 7. Workflow loop
1. **Build:** template → steps; key and chords; sounds.
2. **Dub:** HOME. Mutes, solos, throws, feedback, Big Knob, spring; the runway tells you when moves land.
3. **Capture:** the song records your pass. Store scenes.
4. **Version:** recall scenes to make vocal / instrumental / DJ / dub versions.

## 8. External audio input
- **No analog input** exists in the BSP. fm1-base's USB audio from a host is mixed **after** the app.
- **P2.8:** USB audio from the computer feeds channel 8 "Live in" (mono) at VOX's LIVE level, so it can be muted, soloed and thrown into the echo and spring. The app side is done. It needs fm1-base's proposed `usb_audio_out_app` (`docs/upstream/usb-audio-to-app.md`).
  - Needs a small fm1-base change (upstream, MPL-2.0).
  - Cost: ~2–4 KB ring buffer plus jitter handling. Latency: a few ms plus one block.
- **Out of scope:** USB host audio (host mode is MIDI only) and Bluetooth audio in.

## 9. Open questions
1. Physical black-key grouping (2+3): regroup effects vs voices if needed.
2. Velocity-scaled throws (P2.9, built; SETUP: THROW VELOCITY, on by default): confirm velocity is even across the throw keys on the device.
3. Prototype the DUB page and runway in the browser emulator; run a usability pass (C1, C4, C3, and a first-timer runway test).
