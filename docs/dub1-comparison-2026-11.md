# DUB-1 and Dubwise FM compared (2026-11-10)

Roadmap 9.9 ("DUB-1 looked at again"). This document replaces the DUB-1 parts of `prior-art.md`, which describe
DUB-1 at commit `501c4cf`, before 1.0. It compares the two firmwares as they are today: what each is, how each plays,
how each is built, and what Dubwise should take from DUB-1 or do differently.

## Sources and what was checked

| Side | Source | Version read |
|---|---|---|
| DUB-1 | `github.com/sbegueria/fm1-dub`, cloned read-only, not built or run | `a50587f` (main, 2026-11-08). The code is past 1.4.3: it saves format 36, which 1.4.3's notes introduce |
| DUB-1 | Its `README.md`, `docs/MANUAL.md` (1,909 lines), `PLAN.md` (1,434 lines, the design log), `firmware/src/dsp/amb.h`, parts of `amb.c`, `project.c`, `app.ld`, `usb.c`, the test tree | as above |
| DUB-1 | Release notes on newreleases.io: v1.0.0, v1.1.0, v1.2.0, v1.3.0, v1.4.0, v1.4.2, v1.4.3 (v1.4.1 only as described in the others) | fetched 2026-11-10 |
| DUB-1 | MatrixSynth, "FM-1 DUB-1", 2 November 2026 | fetched 2026-11-10 |
| Dubwise | `README.md`, `docs/design-principles.md`, `docs/prior-art.md`, `docs/roadmap.md`, `app/docs/MANUAL.md`, `docs/roundup-review-2026-11.md`, `docs/prerelease-assessment-2026-11-10.md`, the engine and UI sources | `2ea2bb1` (main, 2026-11-10) |

Paths below are DUB-1's unless they start with `app/`, `docs/` or `base/` (Dubwise). Line numbers are at the commits
above. Release dates come from `PLAN.md` where the release pages give only "N days ago". Anything not checked in the
source is marked **(unverified)**, and §10 lists those.

---

## 1. DUB-1 in one paragraph

DUB-1 is Santiago Beguería's fork of Charles Vestal's AMB-1 (generative ambient), turned towards **dub techno** (Basic
Channel, Maurizio, Echospace). Its README's first lines: "a generative dub techno machine. It plays itself from the
moment it's switched on … You don't play it, you steer it." Six layers (chord stabs, FM "pings", a sub bass, kick,
hats, a noise machine) are generated from a scale and a chord walk, through a tape echo, a granular cloud and a
Dattorro plate. Since 1.1.0 it has grown a hands-on layer on top: mutes, solos, throws, filter moves, channel strips,
dub sirens, songs and sets. It is GPL-3.0-only, released as `FM-1_80XXYYZZ` (`PLAN.md:438`), with a browser emulator,
a web installer, a 1,909-line manual, fourteen cheat sheets and a YouTube channel. It has outside contributors
(KellyOffski's requests, Christian Meier's MIDI CC pull request #4, genkigenki's SMK-37 port) and real FM-1 owners
reporting bugs.

## 2. DUB-1 version by version

| Version | Date (from `PLAN.md`) | What it added | Source |
|---|---|---|---|
| 1.0.0 | 2026-10-16 | The generative engine: chord stabs on a Euclidean pattern, pings on loops of 13/17/23/29 eighths, a sub on five grooves, a ducking kick, shaker, open hat, rim, clap, crackle; eight-bar phrases with breaks and fills; eight pieces; voices by ALGORITHM (7 chord, 5 ping, 5 bass, 5 kits) on a 4-op FM core; the Chords page (eight slots, or the machine chooses); a drum Pattern page (5 lanes × 16 steps, accents, chance); PERFORM (mutes, throw, filter, kick drop, echo); REC tap = dub hold, REC held = a 16-effect grid; screen reader support | v1.0.0 notes |
| 1.1.0 | 2026-10-17 | PERFORM reworked: white 1–5 mutes, black 1–5 solos, held moves (Throw, Open, Shut, Drop, Echo); every live key lit; ping loops kept with the piece; Feedback shown as real loop gain (45–112 %) | v1.1.0 notes |
| 1.2.0 | 2026-10-19 | "Live performance": a bipolar DJ filter on HOME's KNOB 1 (low-pass ~230 Hz down, resonant high-pass ~400 Hz up, the kick left out); PERFORM's knobs become master actions (Filter, Dub, Energy, Space), not saved; a Key page | v1.2.0 notes; `PLAN.md:117-150` |
| 1.3.0 | 2026-10-23 | "The musical release": the chord walk made visible (Endless or Loop over eight slots; Home, Wander, Free); A A A B phrase plans for drums and bass; Motion in three zones; a noise machine (resonators tuned to the chord); nudge and shake (a layer's button held); Lock; the echo's blur (all-passes), width, tone; mono below 150 Hz; a load-shedding valve; help cards removed | v1.3.0 notes; `PLAN.md:541-590` |
| 1.4.0 | 2026-10-28 | "The desk and the set": the Arrangement page (LED meters per part and master, duck as gain reduction); six parts on white 1–6, held = a channel strip (Level, Echo, Plate, a centred LP/HP Filter); throws per part on black keys; four dub sirens with their own knobs; the DJ filter moved to SELECT on every page; the virtual dub mixer (the desk rides itself); Form page (Energy, Vary, Motion, Drift); LFO pages (wave plus random wander); songs (intro, main, outro) and Set mode (LP, Segue, Mix transitions); five Japanese scales; tempo from 40 BPM; big-ring value display | v1.4.0 notes; `PLAN.md:624-1136` |
| 1.4.1 | 2026-10-28 | PRESETS turned fast no longer floods the engine's queue | `PLAN.md:1138-1144`; v1.4.3 notes |
| 1.4.2 | 2026-10-29 | A Set's Mix without the silent gap between pieces | v1.4.2 notes |
| 1.4.3 | after 1.4.2 (date not given) | MIDI CC in on channel 1 (fixed map); a Chord filter page with an envelope graph; the stab filters' closing re-fitted to the reference records; fuller intros and outros; format 36 | v1.4.3 notes |
| next | planned | 1.5.0: set files (fewer than eight pieces), "cues" (moves at set bars in a song, then recorded on the device), a WebMIDI set editor. 1.6.0: songs and sound sets saved and loaded apart. After: a voices overhaul, a bass sequencer page, a MIDI follow mode | `PLAN.md:1194-1206`, `:758-772` |

The pace is fast: thirty-odd save formats (8 to 36) in about four weeks (`docs/MANUAL.md:1624-1690`).

## 3. How DUB-1 plays now

### What it does by itself

These run unless their knob is at zero (or the matching mode is chosen):

- **The harmony.** A weighted random walk over the scale's degrees writes chords into eight slots; in Endless the
  slots scroll and the walk writes slot 8 (`README.md`, "The random chords"; `amb.c` `chord_change`, `walk_fill`).
  Loop plays the eight in turn and writes nothing new. You can write any slot yourself.
- **The parts.** Stabs fall on a Euclidean pattern; the pings play four loops of unequal length; the bass picks one
  of five grooves and a phrase plan (A A A B, sparse or busy B bars drawn once a phrase); the kick takes a shape for
  two to six phrases, drawn around the Kick knob; the noise machine draws its own groove (`MANUAL.md:719-790`;
  `amb.h:1-25`). Only the drums can be written by hand (Pattern page, `P_DPAT*`, Play = Auto or Pattern).
- **The arrangement.** Every eight bars the engine picks the next section (Full, Break, Groove, Thin) by Motion's
  zones, adds fills, drops the kick for a bar or two, and sweeps a slow filter (`MANUAL.md:719-760`).
- **The desk.** The "virtual dub mixer" rides each part's level and sends, the chord filter and the echo feedback,
  a new place every 10–60 s, and now and then rides a part out and back or swells its echo (`MANUAL.md` §3, "The
  virtual dub mixer"; `PLAN.md:883-900`). Drift sets the rides; Motion the moves.
- **Throws.** Stabs are thrown into the echo now and then, scaled by Vary (`amb.h`, the format-30 note on throws).
- **Songs and sets.** In Song mode the intro brings the parts in, in a fixed order by the piece's Kind, and the
  outro takes them out; in Set mode the next piece follows by LP, Segue or Mix (`MANUAL.md:791-872`). The order and
  fades are the engine's, not recorded from your hands.

### What you play by hand (1.4.x)

It is fair to say that **DUB-1 1.4 lets you play the desk by hand**, live:

- SEL opens the Arrangement page. White 1–6 tap: mute the chord, pings, bass, kick, hats, noise. White 1–6 held: that
  part's channel strip on the knobs (Level, Echo send, Plate send, a centred LP/HP Filter). Black 1–5 tap: solo; held:
  a **throw**, the part into the echo at its own level whatever its send, the feedback pushed towards 0.93, eased in
  over 15 ms and out over 60 ms (`MANUAL.md:223-305`).
- White 9–12 held: Open, Shut, Drop (kick and bass out, back on the next beat), Echo. White 13–16 held: Time ×2,
  Freeze, Tape stop, Stutter. Black 8–11: four dub sirens, a tap short, held a loop with its own Pitch, Rate, Depth,
  Echo knobs; three of them tuned to the piece's key (`MANUAL.md:306-345`).
- SELECT is the DJ filter on every page (since 1.4.0). REC tap holds the echo (dub hold); REC held opens the
  sixteen-effect grid. HOME + PLAY calls the outro.
- Turning Vary, Motion and Drift to 0, writing the chords in Loop, and the drums on Pattern gets close to a fixed
  loop. Even then the stab pattern, the ping loops and the bass's groove are generated from the engine's seed, not
  written by you.

What it does **not** have, as of `a50587f`: mutes and solos land when you let go of the key, not on a quantised line
(Drop's release alone waits for the next beat); there is no countdown to a phrase line; nothing you play is recorded
(1.5.0's "cues" are planned, `PLAN.md:1197`); there are no scenes; velocity is not used on the keys (no reference
to it in `firmware/src/app/ui.c`) **(unverified on the device)**.

So the picture is a generative machine with a performable desk bolted on, and the desk itself partly automated.
The roundup reviewer's line ("the music doesn't feel like it is yours entirely", `docs/roundup-review-2026-11.md`)
still describes it.

### Pages and controls, briefly

Every layer has a button and several pages (part, voice, FM, LFO). HOME: Form (Energy, Vary, Motion, Drift), Song,
Key. FX: Dub (Echo, Echo time, Feedback, Reverb), Echo (Blur, Tone, Size, Cloud). GLO: Setup, Song mode. EDIT: Chords,
Walk. PRESETS picks one of eight pieces. A button held 0.5 s nudges its layer's sound; held 1.5 s, shakes it. One rule
for the buttons: a button with one page stays on it; one with several steps through them (`MANUAL.md:125-128`).
While a knob turns, a large ring in the middle shows its value and what it does; the knob numbers are hidden after a
1.5 s "peek" (`PLAN.md:1038-1050`).

### Sound, effects and engine

| Part | DUB-1 | Source |
|---|---|---|
| Voices | Chord: 4 voices, two detuned saws each or FM keys (7 voices); pings: short FM blips (5); bass: sub or FM (5); drums: kick (with a "rumble"), shaker, open hat, rim, clap (5 kits); noise machine: crackle groove ringing tuned resonators (Vinyl, Tape, Dust, Room, Machine) | `amb.h:3-15`; v1.0.0, v1.3.0 notes |
| FM | 4 operators, algorithms 1–9 | `amb.h`, `P_CALG` comment |
| Echo | Tape at 11.025 kHz, int16, 62,000 samples (5.6 s); two heads (the second moves); a band-pass in the feedback; soft-clipped feedback that may pass 100 %; all-passes so repeats blur; a short Echoplex; stereo by a few ms offset | `amb.c:3350-3360`; `amb.h:16-20` |
| Reverb | Dattorro plate, at 22.05 kHz since 1.3.0; a granular cloud after JIANT's | `amb.c:3240-3256` |
| Filters | The DJ filter (bipolar, kick left out); per-part LP 12 dB / HP 6 dB; the chord's resonant low-pass with envelope; the echo's band-pass | `MANUAL.md` §3 |
| Other | Sidechain duck from the kick; width (Haas plus a crossed chorus); 16-effect grid; four sirens | `MANUAL.md` §3 |
| Low end | Mono below ~80 Hz (the master's side channel high-passed; 150 Hz in 1.3.0) | `amb.c:6392` |
| Scales | 13 (church modes, two pentatonics, five Japanese) | `amb.h:31-35` |
| Tempo | 40–140 BPM | `PLAN.md:1076` |

### Saving, MIDI, accessibility

- **Saving:** one project with all eight pieces and the global settings; autosave a few seconds after a change, only
  while resting; GLO held saves now; GLO held 1.5 s resets the piece to its recipe (`MANUAL.md:1624-1640`). An
  untouched piece is recognised and takes the new version's recipe on upgrade; a changed piece keeps your values
  (`MANUAL.md:1642-1690`).
- **MIDI:** in on USB and TRS, channel 1: notes add or remove scale notes; a fixed CC map (since 1.4.3); clock in
  (EXT in the header) and out; notes out (stabs as note-ons, All Notes Off at a piece change) (`MANUAL.md:1577-1608`;
  `amb.c:1917`, `:4218`). No MIDI learn. USB audio exists as a build option (`usb.c:11-21`, `FELUCCA_UAC`, default
  0), the master only; whether release builds turn it on **(unverified)**.
- **Accessibility:** the FM-1 Screen Reader (page, knobs, values, the chord as it moves); fourteen printable cheat
  sheets; no help cards since 1.3.0.

---

## 4. Side by side

| Area | DUB-1 (1.4.3+) | Dubwise FM (`2ea2bb1`) |
|---|---|---|
| **Concept** | Generative dub techno that plays itself; you steer it | A Kingston dub mixing desk you play; nothing plays itself |
| **Mutes** | Six parts, a tap on release, immediate | Eight channels, quantised to beat, 1–32 bars or the cycle, landed 10 ms early; hold to flip while held |
| **Solos** | Black keys, additive | Double tap or HOME + key, additive, quantised |
| **Throws** | Black key held, at the part's level, feedback pushed | White 9–16, pre-fader and pre-mute, velocity-scaled, to echo, spring or both, latchable |
| **Runway / countdown** | None (the header shows the phrase's eight bar cells) | 4-bar countdown to the next 8/16/32/64-bar line or song marker; screen and key pulse; spoken by the reader |
| **Whole-mix filter** | Smooth bipolar DJ filter on SELECT (LP down, resonant HP up, kick excluded); Open/Shut moves | Stepped Big Knob on SELECT (Altec 9069B steps, unsmoothed clicks); pressed: stepped LP |
| **Per-channel filter** | Strip's centred LP/HP (one-pole/two-pole) | Each channel's Big Knob filter, held keys move together; plus LO/MID/FREQ/HI EQ |
| **Echo** | Tape, 5.6 s, two heads, band-pass and soft clip in loop, blur all-passes, Echoplex, Time ×2, Tape stop, Freeze (dub hold) | Tape with in-loop filters and saturation, push, freeze, dive, wow/flutter, 2T and 3T heads, ping-pong, width; BBD type |
| **Spring / reverb** | Dattorro plate, granular cloud | Two-spring reverb you can crash; snare-keyed spring gate; cross-sends echo ↔ spring |
| **Phaser** | None (a Wah in the grid) | Dual Bi-Phase, alone, cascaded or stereo |
| **Drive** | Bass throws driven; master soft clip | Tape drive on bass or mix; DESK MCI strip saturation |
| **Sirens and shots** | Four sirens (three tuned to the key), own knobs while held | Two sirens (CLASSIC, NJD), BIM/HORN/LASER one-shots (reversible), LPC deejay shouts, user one-shots |
| **Voices** | Chord stabs, pings, sub/FM bass, kick/shaker/hat/rim/clap, noise machine | Kick, rim, hats (incl. 808 models), FM bass with sub, skank (FM or plucked), organ (FM bubble or B-3 with Leslie), lead (FM melodica, brass, reed, horn), vox (sirens, shots, deejay) |
| **Sequencer** | Generated parts; drums on a 5 × 16 Pattern page with accents and per-lane chance | Five riddim templates, four patterns a channel, step locks, flams, ratchets, seeded chance, conditions (1ST, A:B, PRE, FILL), live recording |
| **Harmony** | Chord walk or eight written slots; 13 scales | Key and chord lane lock (ARP page) |
| **Arrangement** | Generated sections, fills, intro/outro (Song mode), sets with LP/Segue/Mix | Recorded: song of up to 512 bars capturing every move inside each bar, bit-exact replay; section markers; punch-in |
| **Scenes** | None | 16, desk or desk + patterns, quantised recall |
| **Sound design** | Voice per layer, voice/FM/LFO pages, nudge and shake, recipes per piece | SOUND view per channel, patches, factory kits, Dice (fired, seeded), morph between patches |
| **Sets / slots** | Eight pieces in one project; Set mode plays them in order | Four song slots; backup and transfer over USB; web editor |
| **MIDI / USB** | Notes in → scale; fixed CC map; clock in/out; notes out; USB audio a build option | Clock in/out, MIDI learn, USB MIDI keyboard, SMC-Mixer as a hardware desk, 10-channel USB audio (mix plus stems) |
| **Screen / UX** | Big value ring, LED meters, ripples/scope/spectrum/tape views, numbers hidden after a peek, cards under a second | Eight strips, the Big Knob arc, runway numerals, knob colours matched to caps, help card on any held button, reduced motion |
| **Undo** | None found | HOME + REC / HOME + GLO, step by step |
| **Low end** | Mono below ~80 Hz | Mono below ~150 Hz; each channel pans above it |
| **CPU safety** | Sheds least-audible work for 0.5 s past 75 % | Three guard tiers (Studio, Four-track, Seven-inch), automatic or locked as a sound |
| **Docs** | 1,909-line manual, 14 cheat sheets, emulator, web installer, videos | 518-line manual, a one-page cheat sheet, emulator, web installer and editor, mastery guide |
| **Hardware time** | Played on FM-1s by its author and users since October | None yet |

---

## 5. The differences of principle

1. **Who makes the music.** Dubwise principle 1: nothing changes unless the user changes it; chance only as a tool
   the user fires, stored and repeatable. DUB-1's core is the opposite: the harmony, the parts, the sections and
   (since 1.4.0) the faders themselves move on their own. Its hand controls are real, but they act on a stream that
   is being composed for you. Turning its variation knobs to zero stills the arrangement and the rides; it does not
   give you the notes. This is the difference that matters, and it is a matter of design, not of features: DUB-1 is
   not going to drop its walk or its grooves without becoming another firmware.
2. **The desk as the instrument.** Both now have a desk page. In Dubwise the desk *is* the product: mutes, sends,
   throws, the filter and the returns are parameters that can be quantised, recorded in the song, recalled as
   scenes and undone (principle 2). In DUB-1 the desk is one page among about twenty, its moves are not stored, and
   it is partly played by the machine (the dub mixer).
3. **Time.** Dubwise lands moves on the line and shows the line coming (the runway, principle 3). DUB-1 acts at
   once, which is natural when the machine already owns the phrase structure.
4. **Idiom.** DUB-1 is dub techno: minor-chord stabs, a four-on-the-floor kick, a long blurred echo, a plate, hiss,
   filter drift over minutes; its eight pieces are modelled on Basic Channel, Maurizio, Echospace, Monolake and Porter
   Ricks (`PLAN.md:1237-1245`). Dubwise is 1970s Kingston roots dub: one drop, rockers, steppers; bass and drum;
   skank, organ bubble, melodica; a stepped Altec filter, a struck spring, a Space Echo. They share ancestors (the
   tape echo, the throw, the siren) and little else.
5. **Low end.** Both keep the bass mono. DUB-1 moved its crossover from 150 Hz down to 80 Hz in its "classics pass"
   for a wider dub techno image; Dubwise keeps 150 Hz and pans each channel above it (8.9).
6. **Fidelity.** DUB-1 runs its voices at 44.1 kHz in float and spends CPU on a plate and a cloud. Dubwise trades
   voice rate (22.05 kHz) for more voices and effects, and turns overload into an audible, period-styled tier
   (principle 5).

## 6. Where they converge, and where DUB-1 is ahead

**Converging.** The owner is right that DUB-1 has moved towards Dubwise's ground since `501c4cf`:

- SELECT as the filter on every page (DUB-1 1.4.0; Dubwise's Big Knob from P0).
- A desk page with per-part mutes, solos, throws and a held-key channel strip with level, echo send, a second send
  and a filter: the same gesture Dubwise uses (hold a channel's key, turn a knob).
- Throws at the part's own level whatever its send, with the feedback pushed so the repeats stack.
- Dub sirens into the echo, with pitch, rate and depth.
- Songs with an intro and outro, and talk of recording "cues" in 1.5.0.

On the evidence this is convergence of the desk, not of the principle. DUB-1's `PLAN.md` frames every new hand
control as "how much it adds to playing it" while keeping the generator ("DUB-1 should be something you play live,
not only leave running", `PLAN.md:155`).

**Where DUB-1 is ahead, honestly:**

| | Why it matters | Dubwise today |
|---|---|---|
| **Hardware time and users** | Device-measured CPU (peaks 80–82 % of one core on the heaviest pieces, `PLAN.md:1296-1300`), real crash reports, an install stall diagnosed, outside contributors | Zero hardware time; every CPU figure is host × 80 |
| **The first ten minutes** | Switch on and it plays; a seven-step "Getting started" (`MANUAL.md:67-100`); good sound with no input | Silence until you load the demo or a riddim; the controls table is long; SIMPLE mode is only planned (9.H3) |
| **A smooth DJ filter** | The most-played control by DUB-1's own author (`PLAN.md:619`); one knob, everywhere, with the kick kept out | The Big Knob is stepped by design; a held channel set does a partial filter, but there is no "everything but the kick" in one move |
| **Set-building and transitions** | Set mode with LP, Segue and Mix (tempo and echo time glide), an hour of music without touching it | Four song slots, no chaining |
| **Performance feedback on screen** | LED meters per part with peak hold and duck as gain reduction; a big ring naming the value as you turn | Strips with meters; values in the knob row; no single large readout |
| **Instrumentation on the device** | Setup shows CPU now/peak, per-stage PEAK and MEAN in ms over 2 s, a dropout count, the CPU clock (`MANUAL.md:1531-1550`) | PERF page shows per-part averages and the tier; the max is tracked in `eng_prof_t` but not drawn; `plat_xruns()` exists in `base/include/plat.h:492-499` and is not shown |
| **Documentation for players** | Fourteen one-page cheat sheets, a page per part; release notes that say what changed for a user and how saves convert | One cheat sheet; a long manual |
| **Presence** | Videos, a channel, MatrixSynth coverage, thefirmwarehub listing | Not listed yet |

---

## 7. Architecture compared

Both are single-core, interrupt-rendered engines on the AC79, with a main-loop UI and a command queue between them.
They differ in how the engine and its interfaces are cut up.

| Topic | DUB-1 | Dubwise | Verdict |
|---|---|---|---|
| **Platform** | Its own copy of Felucca's HAL in `firmware/hal/` (20 files) plus its own USB, OTA, storage, LCD and loader (`firmware/src/*.c`). Does not set the CPU clock; runs at what the boot loader leaves (360 MHz stock, reportedly 320 MHz under m-bootloader 1.2, `PLAN.md:1086-1088`). Migration to fm1-base is an open question (`PLAN.md:1101`) | fm1-base as a pinned submodule; the app sees only `plat.h`. fm1-base sets the stock clock at start (`base/device/main_fm1.c:30-33`) | **Dubwise better.** Platform fixes arrive by a submodule bump, and the clock question is answered |
| **Engine structure** | One unit: `dsp/amb.c`, 6,615 lines, holds every voice, the walk, the arrangement, the songs and sets, the dub mixer, the echo, plate, cloud, noise, sirens, grid and master. Its log records a bug from that: the chord's LFO and the plate's wobble shared one variable from 1.3.0 to 1.4.0 (`PLAN.md:686`, `bfb8e9d`) | Effects and voices in their own modules (`echo.c`, `spring.c`, `phaser.c`, `voices.c`, `chordv.c`, `leslie.c`, `d808.c`, `lpc.c`, `motion.c`, `song.c`, `scenes.c` …); `engine.c` (1,908 lines) still holds transport, cues, sequencer, song and motion glue, MIDI clock, stems, guard and profiler | **Dubwise better**, and DUB-1's shared-variable bug is prior-art lesson 10 happening again. The lesson for Dubwise is the assessment's: split transport/sequencer from the mixer before `engine.c` grows further |
| **Parameters** | One `enum` of `P_*`, about 170 entries, appended with format notes; the queue packs the ID in 8 bits (`amb.c:739`, `a & 0xFF`), so the space is capped at 256 | One append-only 16-bit ID space in banks (global, strips, bus, voices), compile-time range checks (`app/engine/params.h`); mutes, sends and throws are parameters | **Dubwise better.** It is why scenes, song motion, MIDI learn and undo come for free. DUB-1's 8-bit field is a ceiling it will meet |
| **Command queue** | 512 × 32-bit words, `_Static_assert` that it holds a piece's parameters twice; a full queue **drops** the command and the UI re-sends the piece when the queue empties (`amb.c:726-742`). The drain has a **budget**: while playing, a burst with a regrow in it is spread over about seven blocks (28 commands a block), which brought a piece change's worst block back to the steady peak (`amb.c:4663-4680`; `PLAN.md:1282-1284`) | 256-entry SPSC, opcodes apart from IDs; a full queue never drops: the parameter is marked pending and its latest value is sent next frame (`app/engine/cmdq.h`, `app/engine/control.c`); the queue is drained whole at the block's start | **Dubwise better on correctness** (no drop, latest wins, order kept). **DUB-1 better on peaks**: it measured that a bulk change in one block was its worst CPU moment and spread it. **Adopt** the idea (§8, item 6) |
| **Threading** | Render in the I2S interrupt, UI in the main loop; one core. A written plan for the second core, (a) the effects bus a block behind, (b) the chord voices, (c) the UI; not built (`PLAN.md:1103-1115`) | Same model; one core. The architecture section describes a fork–join split with a fixed mix order; not built | **Same.** DUB-1's option (a), effects one block behind, is worth weighing: it avoids a join inside the block at the cost of 5.8 ms on the returns only |
| **Rates and buffers** | Voices at 44.1 kHz float; echo 11.025 kHz int16 (5.6 s, ~121 KB); plate 22.05 kHz float; noise rings at quarter rate | Tonal voices at 22.05 kHz; hats and rim at 44.1 kHz; echo 11 kHz int16; spring 11–22 kHz; control rate every 16–32 samples | **Different trade-off.** DUB-1 has fewer, fuller-rate voices; Dubwise more voices at half rate. Neither is wrong; Dubwise's is unmeasured on the device |
| **Hot code** | The current linker script puts `dsp_amb.o`'s text in a 64 KB RAM region (`firmware/app.ld:18-28`), taken from the pool (pool 0x44000). `PLAN.md` records a first try that evened the peaks but not the averages (`PLAN.md:1296-1300`) and a later build "B" with it; the log is not clear which is in the release **(unverified)** | Code runs from flash through the 32 KB I-cache | **Open.** Dubwise cannot spare 64 KB of pool (it has ~50 KB free, `roundup-review-2026-11.md` §3). If the device's PERF peaks are spiky, try only the few hottest kilobytes in RAM |
| **CPU guard** | One valve: a block over 75 % sheds for half a second what is least heard (tails end at −44 dB not −62, no new cloud grains or noise hits, width eased off), then eases back (`amb.c:6571-6575`, `:5278`; `MANUAL.md:1533-1537`) | Three tiers: down after 8 blocks (46 ms) over 75 %, up after 2 s under 45 % (`app/engine/engine.c:235-302`); each tier audible (mono returns, spring at 11 kHz, phaser off, voice caps); lockable as a sound | **Different trade-off.** Dubwise's is richer and makes overload a style. DUB-1's reacts after one heavy block and changes nothing you would notice. **Adopt** an inaudible first stage (§8, item 7) |
| **On-device figures** | PEAK and MEAN over 2 s, per stage, in ms; dropouts counted; CPU clock read from the registers | Per-part averages and the tier on PERF | **DUB-1 better.** **Adopt** (§8, item 3) |
| **Storage** | One project object (magic "DUB1", `PROJ_FORMAT` 36, `firmware/src/app/app.h:15-19`) for all eight pieces and the globals; a converter chain from format 8; an untouched piece recognised against its past recipes takes the new one; real saves from released versions kept as test fixtures (`tests/host/proj-1.2.0.bin`, `proj-1.3.0-test14.bin`, `proj-1.4.0-test18.bin`); reserved bytes reused too (`PROJ_WAVE_MARK` in `rsv[0]`, Values in a reserved byte) | Objects with their own magics and versions (`DUB@` settings, `DSK1`, `SND1`, `PAT1`, `PAT2`/`PST3`, `MAP1`); converters; tests that build old saves by script (`app/tests/conds.txt` `oldsteps2`, `prob.txt`, `patterns.txt`); some settings in spare bytes | **Mixed.** Dubwise's per-object split suits slots, patches and transfer, and its plan-before-code rule is stricter. DUB-1 does two things better: real binary fixtures from each release, and "an untouched factory item takes the new factory version". DUB-1 is also the warning: 28 format steps in a month. **Adopt** both ideas (§8, items 5 and 9) |
| **UI structure** | `firmware/src/app/ui.c`, 6,041 lines, one file | `app/app.c` is a unity build that includes about 25 page files (`dub.c` 2,008, `steps.c` 1,091 …) | **Dubwise better** organised, though its pages share every static. DUB-1's UI *rules* are worth more than its code (§8, item 8) |
| **Tests** | Engine unit tests (`tests/host/amb_test.c`, 3,107 lines); 34 app scenarios; listening and film scripts; storage, OTA, loader, install, encoder, fastmath and USB-audio resampler tests; JS emulator tests; `tools/analyze_wav.py`; "bit for bit on all eight pieces" as the rule for every refactor | About 80 scenario scripts in `app/tests/` (the count grows as items land) with WAV metrics (`tests/wavstat.py`), bit-exact song replay, CPU bounds, transfer and web tests, `tests/warnings.sh` | **Dubwise better** on scenario coverage and audio metrics. **DUB-1 better** on platform-layer tests and on real fixtures. Both use bit-exactness as the refactor rule |
| **Code size** | `firmware/src` 18,101 lines plus `firmware/hal` 1,944 (it carries its platform); RAM 94,248 of 98,304 B in a 1.4.0 build (`PLAN.md:791`) | `app/` 17,709 lines without the generated `*_data.h` (the platform is in `base/`); pool 294,020 of 344,064 B (`roundup-review-2026-11.md` §3) | **About the same app size**. DUB-1's `.data/.bss` is within 4 KB of full; Dubwise's device RAM figure should be stated the same way after 9.H1 |

**Summary of the architecture.** Dubwise's skeleton is better: the platform boundary, the parameter space, the queue
semantics and the module split are each a step up from DUB-1's. DUB-1 is better in the places that come from having
run on hardware: it measured where its peaks were, spread the one that mattered, shows the figures on the device,
sheds without being heard, and keeps real saves from real releases. None of those needs Dubwise to change its
design; each is small.

---

## 8. What Dubwise should do

In order. Items 1–5 need no hardware and fit the feature freeze (they are fixes, docs or instrumentation); 6–10 wait
for 9.H.

| # | Do this | Why | Cost | Roadmap |
|---|---|---|---|---|
| 1 | **Write the listing around principle 1.** Lead with: "You play every bar. Nothing plays itself." Then the runway, throws by velocity, the struck spring, songs that keep your moves bit for bit, the stems. Say "1970s Kingston roots dub", not "dub machine"; name the riddims | DUB-1 already owns "dub" on the FM-1 and is described everywhere as generative. The reviewer's complaint about it is exactly Dubwise's strength | Text | 9.10 |
| 2 | **Guard against being taken for DUB-1.** Avoid "DUB-1-like" phrasing, avoid a version "1.x" that invites comparison, and say on the listing, once and plainly, how it differs ("DUB-1 is a generative dub techno machine; Dubwise is a desk you play"). Note that the hub ID `FM-1_382` spells DUB on a keypad: harmless, but the card's text should carry the distinction | Two "dub" firmwares on the same hub page; a buyer skimming thumbnails may think one is a fork of the other | Text | 9.10 |
| 3 | **PERF page: peak, dropouts, clock.** Draw `eng_prof_t.max` as a 2 s peak beside the average, show `plat_xruns()`, and the clock fm1-base reports | DUB-1's figures were what made its device work tractable; 9.H1 needs the same | Small UI; no audio CPU | 9.H1 prep |
| 4 | **A first-ten-minutes page.** A numbered quick start at the top of the manual (load RIGHTEOUS DUB or ONE DROP, press PLAY, mute the bass on the one, throw the snare, turn the Big Knob), and per-page cheat sheets in DUB-1's manner | DUB-1's "Getting started" and its cheat sheets are its best documentation; Dubwise's manual is accurate but long | Docs | 9.6 |
| 5 | **Keep a real save from every release** as a binary fixture under `app/tests/`, and load it in a scenario | DUB-1's `proj-1.2.0.bin` style fixtures test what users actually have; scripted old saves test what we think they have | One file a release | with release v0.x |
| 6 | **Spread bulk changes over blocks.** Measure the worst block for a scene recall, a slot load, a kit and a patch load, a Dice roll; if any stands above the steady peak, give the drain a budget for non-cued bulk sets (DUB-1: 28 a block), leaving quantised cues sample-exact | DUB-1's worst CPU moment was a piece change; Dubwise's scene and slot recall are the same shape and unmeasured | Small; host-testable as a peak ratio | 9.H2 |
| 7 | **An inaudible first stage before a tier drop.** On one block over 75 %, end tails earlier (−44 dB), skip new spring-gate or phaser work, for half a second; drop a tier only if the load stays high | The tier needs 46 ms of overload, and every tier is audible. DUB-1's valve acts after one block and is not heard | Small; a guard test | 9.H2 |
| 8 | **Borrow UI rules, not code.** (a) A large readout of the value being turned, fading after a second. (b) DUB-1's button rule: one page stays, several step. (c) Key lighting: every live key lit (Dubwise already has "bright playing, dim muted, blink waiting"). (d) Cards that clear in under a second on the performance page | The roundup reviewer scores legibility; DUB-1 tuned these on the device with users | UI only | 5.11 follow-up |
| 9 | **Factory content that follows the new version.** When kits, patches or the demo change, an item the user never edited takes the new factory version; an edited one is kept. Recognise "never edited" by comparing to the past factory versions, as DUB-1 does with its recipes | Avoids users stuck on old factory sounds after an upgrade | Needs a stored-format plan first (CLAUDE.md) | after 9.H4 |
| 10 | **Measure throws per channel.** A scenario that throws each channel alone and reports the echo's tail against the channel's own peak. DUB-1 found its throws 11–24 dB under the part and its bass throw almost silent because the echo takes nothing below ~350 Hz; it now drives the bass into the echo (`MANUAL.md:282-290`; `PLAN.md:980-990`) | Dubwise's echo has in-loop filtering too; a bass throw that disappears would undercut the headline feature | A test | 9.x |
| 11 | **Consider a set chain** of the four song slots, with an LP gap or a straight segue at the next bar, fired by the player | DUB-1's sets let a player do an hour; a chain is user-driven and fits principle 1. Not a tempo-gliding Mix: Dubwise songs are played, not generated | Medium; stored format | after the freeze |
| 12 | **Consider an "all but the drums" option for the Big Knob** (a SETUP choice: MIX, or MIX BUT KICK AND BASS) | DUB-1's most-played move filters the music over a steady kick. Dubwise can already do it by holding channel keys; one setting makes it a one-hand move | Small | after the freeze |

**What not to borrow.** The chord walk, the generated grooves and phrase plans, Motion's sections, the virtual dub
mixer, LFO wander, nudge and shake, generated intros and outros: each breaks principle 1. The pings and the noise
machine belong to dub techno, not to Kingston dub. The 16-effect grid duplicates moves Dubwise has on single keys.

**GPL code worth adapting, with credit** (both are GPL-3.0-only; credit in the file header and `CREDITS.md`):

- The sirens' key tuning (`amb.c`, `en_siren`, `en_sir_set`): Wail, Hi-lo and Laser turn on the root, fifth and
  octave of the current key; Whoop is out of tune on purpose. Dubwise's sirens could take a KEY option from the chord
  lane. Check Dubwise's siren first: whether it is already key-aware was not checked here.
- The drain budget (`amb.c:4663-4680`), as a pattern rather than code.
- The echo's blur all-passes were already listed in `prior-art.md`; nothing new.

---

## 9. The risk, and the opportunity

- **Being seen as the same product.** Both are "dub" firmwares for the same device, both have a desk page with
  mutes, throws and sirens, both put the filter on SELECT. A reviewer who tries DUB-1 first may file Dubwise as
  "another one". The answer is item 1: the reviewer's own complaint about DUB-1 ("doesn't feel like it is yours") is
  the sentence Dubwise is built to refute. Show it in the first ten seconds of any video: silence, then a riddim
  you start, then a drop on the one.
- **DUB-1 moving further.** If 1.5.0's recorded "cues" arrive, DUB-1 will have recorded performance on top of a
  generator. It will still be a generator. The gap that remains is principle 1, the Kingston palette, quantised
  moves with a runway, scenes, the step sequencer and the stems.
- **Breadth before hardware.** DUB-1 shipped narrower and fixed what users found. Dubwise has more features and
  no hours on a device. The assessment's order (freeze, hardware, then play) stands; nothing in this comparison
  argues for adding features before 9.H.
- **An opportunity.** The two are complementary rather than rivals: a dub techno set from DUB-1 and a roots dub set
  from Dubwise are different evenings. A friendly cross-link and credit (`README.md` already credits DUB-1 for ideas)
  costs nothing.

## 10. Not verified

- DUB-1 was read, not built or run; no figure of its CPU, RAM or sound was measured here. Its CPU figures are its
  own (`PLAN.md`).
- Whether DUB-1's release builds run the engine from RAM (the linker script at `a50587f` does; the log is mixed).
- Whether DUB-1's release builds include USB audio (`FELUCCA_UAC` defaults to 0 in `usb.c`).
- Whether DUB-1 uses key velocity anywhere (none found in `ui.c`).
- DUB-1 1.4.1's own release page was not read; its content comes from the 1.4.3 notes and `PLAN.md`. Release dates
  are from `PLAN.md`, not the release pages.
- Whether the roundup's "top one" is DUB-1 remains the inference recorded in `roundup-review-2026-11.md`.
- The MatrixSynth post gives no version; it calls DUB-1 "a generative dub techno machine that plays itself from the
  moment it is on" and notes a firmware, NACHTFAHRT, based on it (not looked at).
- Dubwise's own figures are host estimates (× 80); none is from a device.
