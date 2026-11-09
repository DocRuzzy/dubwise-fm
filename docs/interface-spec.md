# Dubwise FM — Interface and workflow spec (v0.2, 2026-11-08)

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
| SEQ | STEPS | Steps → Length/rate/swing → Template |
| EDIT | SOUND | Voice pages |
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

### 4.2 ARRANGE page
- **White 1–16:** recall scene, quantised. **SAVE held + white key:** store.
- **Scene scope** (KNOB 4 at store time):
  - **Desk** = mutes, solos, sends, throw destinations, HPF step, return levels.
  - **Desk + Patterns** = also each channel's pattern. Shown with a P badge.
- **Black keys:** section markers at the current bar.
- **KNOB 1–4:** song position · section length · loop · scene scope.
- **REC + PLAY:** capture into the per-bar song, with the motion inside each bar (throws, mutes, knob moves: P2.7). **HOME + REC:** undo (the bars, not the motion). **SAVE + REC:** clear the motion.

### 4.3 STEPS page
- **White 1–16:** steps; OCT± page through 17–64.
- **Black 1–8:** channel. **Black 9–11:** accent / ghost / probability (hold + step).
- **REC** on bass, skank or lead turns the keys into a keyboard.
- **Templates** (one drop / rockers / steppers / flyers) are written as editable steps.

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
| SEQ taps | Tap tempo |
| ARP | Key/scale lock on/off |
| SAVE | Save as… |
| White key | Jump to pattern n (on DUB: solo) |
| Black key | Copy focused channel's pattern to channel n |

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
- Guard tier changes show as a badge plus a toast. Hold any button for help.

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
