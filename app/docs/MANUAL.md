# DUBWISE

DUBWISE (Dubwise FM, a SpIllt Beats release by DocRuzzy) is a reggae/dub groovebox for the M-VAVE FM-1 in the making, where the mixing desk is the
instrument. This early build has the desk's first move: the DUB page, where the white keys mute and solo the eight
channels, and every change can wait for the next beat or bar line so it lands on time.

Six channels have voices: an FM kick, rim, hats and bass, and a skank and an organ bubble that play the chords of
the chord lane (KEY & CHORDS page). A step sequencer plays them all. It starts with a
ONE DROP riddim, so press PLAY and you're dubbing. Press SEQ to edit the patterns or pick another riddim. You can also
play the voices over MIDI. Press GLO to put a test tone on every channel. There's no Bluetooth: the RAM it would take goes to the dub bus.

![The DUB page, stopped: 75.0 BPM, BAR 1.1, Q 1 BAR in the header; eight channel strips, all playing; the echo's repeats falling; TIME 1/8., FB 3.2s, ECHO 100, SPRG 90](img/screen-ready.png "The DUB page at rest")

---

## Contents

1. Install
2. The screen
3. Controls
4. Saving
5. MIDI and audio
6. The screen reader
7. Troubleshooting

---

## 1. Install

To try it first, open the emulator. It runs the same code in your browser, with sound, and you can play it with a
mouse, a touch screen or your computer keyboard.

To install it from the browser, use Chrome or Edge. Connect the FM-1 by USB, open the installer and press Install.
The browser asks once to allow MIDI, so allow it. Don't unplug the cable while it writes.

To install it from a terminal, run `python3 tools/fm1_install.py dubwise-VERSION.fwsc`.

To go back to the stock firmware, use the installer's last section, which writes M-VAVE's own firmware back. You
can recover an FM-1 that doesn't start, too. For details, see felucca-bsp's `tools/fm1_rescue.sh`.

---

## 2. The screen

![The DUB page playing: bar 2.3; HAT muted (outlined), BASS soloed (S) so the rest are hatched; ORGN cued, landing in 1 bar](img/screen-playing.png "Playing, with a mute, a solo and a cue")

| Part | What it shows |
| --- | --- |
| Header | The tempo, playing (triangle) or stopped (square), the cycle bar (a segment a bar, in fours, this bar filling beat by beat), the bar you're in out of the cycle (5/16), and the quantise setting (Q). It turns amber on the runway. After a change, a message shows here for a moment. |
| Channel strips | The eight channels: 1 KICK, 2 SNR (snare), 3 HAT, 4 BASS, 5 SKNK (skank), 6 ORGN (organ), 7 LEAD, 8 VOX. A filled box is playing, with its level brighter over it. An outlined box is muted. A hatched box is muted by someone else's solo. S marks a solo. A strip that blinks white has a change waiting. A line under a strip marks the focus. Under each box, where a throw sends the channel: E (echo), S (spring) or ES (both). A thrown channel's number lights up. |
| Centre, left | The echo: its time, and its repeats as they fall. They turn amber near endless feedback and red past it. A red line underneath shows the echo's limiter at work. |
| Centre, right | The Big Knob: its ten steps as an arc, and where it is. While something waits, what's waiting (CUED) and when it lands: NEXT BEAT, 1 BAR, 2 BARS and so on. On the runway, the countdown. |
| Bottom line | The four knobs and their values: echo TIME, feedback (FB, how long the repeats take to die away), the ECHO return and the spring return (SPRG). While you hold a mute key, the held channel's LEVEL, ECHO send, spring send (SPRG) and filter. |

---

## 3. Controls

| Control | What it does |
| --- | --- |
| White keys 1–8 | Mute or unmute channels 1–8. A tap toggles. Hold a key to flip the channel only while you hold it. Tap twice quickly to solo it. Solos add up: solo two channels and you hear both. A muted channel stays muted when you solo it. |
| HOME + white key 1–8 | Solo, without the double tap. |
| OCT− · OCT+ | The quantise setting: BEAT, 1, 2, 4, 8, 16 or 32 bars, or CYCLE (the next "one"). While playing, a mute or solo waits for the next line of that setting and lands on it, a hair (10 ms) early so it catches the downbeat. Its key blinks until then. |
| PLAY | Start and stop (when you let go). Starting counts from bar 1. Stopping makes anything waiting happen at once. Hold PLAY while you press a white key to make that change now instead of on the line. |
| SELECT · BIG KNOB | The Big Knob: a steep high-pass filter in steps, like King Tubby's: OFF, 70, 100, 150, 250, 500 Hz, 1, 2, 3, 5, 7.5 kHz. Each click jumps straight to the next step, which is part of its sound. Sweep it up to make the mix "evaporate", snap it back on the one. On the whole mix, or only the echo's return (SETUP). |
| HOME + SELECT | Tempo, 1 BPM a click. |
| ALGORITHM | Moves the focus between the channels. |
| EDIT | The SOUND view: KNOB 1–4 edit the focused channel's voice, four settings at a time. Press EDIT again for the next four, then once more to go back to the echo's knobs. ALGORITHM picks the channel. |
| GLO | The SETUP page (below). |
| MASTER | Sets the volume. |
| SAVE | Saves the tempo, the quantise setting and the wave. SAVE lights while there's something to save. |
| KNOB 1 · TIME | The echo's time, in steps of the beat: 1/16, 1/8T, 1/16., 1/8, 1/4T, 1/8. (dotted eighth, the default), 1/4, 1/2T, 1/4., 1/2. At slow tempos the longest don't fit, so you get the longest that does. A change glides like tape, bending the pitch. |
| KNOB 2 · FB | How long the repeats last: the time they take to fall 60 dB, from 0.1 s to endless (INF). |
| KNOB 3 · ECHO | The echo's return level. |
| KNOB 4 · SPRG | The spring's return level. |
| Hold white key 1–8 + KNOB 1–3 | That channel's level, echo send and spring send. Turning a knob while you hold the key keeps the key from flipping the mute. |
| Black key 2 | Echo hold, while held: no new sound goes in, and the tape loops as it is. |
| Black key 3 | Dub push, while held: feedback past endless, so the echo builds and rings. The tape's saturation and the limiter keep it from blowing up. |
| White keys 9–16 | Throw channels 1–8: while you hold the key, the channel goes to its destination, whatever its fader and even when it's muted, and the echo's feedback lifts so the throw rings on. Hit the key harder for a bigger throw: the softest sends about −8 dB, the hardest full level (SETUP: THROW VELOCITY; off, every throw is full). Tap twice quickly to latch a throw on; tap again to let it go. |
| Hold white key 9–16 + KNOB 1 | That channel's throw destination: ECHO, SPRING or BOTH. Choosing doesn't throw. By default the kick, hats, bass, lead and vox go to the echo, the snare to both, the skank and organ to the spring. |
| Black keys 6 · 7 | Siren A (two tones) and siren B (a rising sweep), on channel 8, while held, louder the harder you press. They go into the echo by default. Their pitch, rate, depth and wave: the SOUND view of channel 8 (ALGORITHM to VOX, then EDIT). |
| Black keys 8 · 9 · 10 | One-shots: HEY, HORN, LASER, on channel 8. A tap plays the whole shot; the harder you press, the louder. Like the sirens they go into the echo, so a shot comes back as a dub repeat. |
| Black key 4 | Tape dive, while held: the echo's tape slows, so its repeats bend down in pitch; let go and they come back up. How far and how fast: FX page, TAPE bank (DIVE ×1.00–2.00, DIVE TIME 0.1–3 s). |
| (Stereo) | The dry sound is mono. Only the echo and the spring are stereo: the spring always, the echo with WIDTH. Below about 150 Hz both sides stay the same, so the bass stays solid and mono. |
| Black key 1 | Spring crash: strikes the spring tank, a thunderclap even with nothing sent to it. Held while you hold a throw key, it sends that throw to the spring as well. |
| FX | The FX page: ECHO (time, feedback, tone, 2T head), SPRING (decay, tone, return), CROSS (the echo's return into the spring, the spring's into the echo), TAPE (the dive), PHASER (where it is: OFF, HATS, SKANK, ECHO or the whole MIX; RATE 0.1–18 Hz, DEPTH, FEEDBACK), GATE (the spring gate: each snare opens the spring for HOLD, then it shuts over RELEASE, so a long spring is chopped into a tail after every snare), DRIVE (tape saturation on the BASS or the whole MIX: DRIVE up to +14 dB into the tape, BLEND with the clean sound; quiet sounds keep their level, loud ones flatten and thicken) and WIDTH (the echo's right side 8–16 ms behind its left, for a wider echo; the phaser's sine or square sweep; PING-PONG, the echo's repeats left, right, left, while the bass in them stays in the middle); press FX again for the next. KNOB 1–4 change them. The cross-sends make a loop between the two effects, so they're held back automatically when the echo or the spring is set long: it never runs away. |
| Other black keys | Show their names. The rest come next. |

The keys light by one rule: bright is playing, dim is muted or not in use yet, and a slow blink means waiting.

---

## 3a. The runway

The music runs in cycles of 16 bars (8, 32 or 64 on the SETUP page), counted from PLAY. Over the last 4 bars of each
cycle (the runway), the header turns amber and a big number counts the bars to the one: 4, 3, 2, 1. In the last bar,
the screen's border pulses on every beat (the white keys too, if you turn that on). On the one, ONE flashes green.
A screen reader hears "4 bars", "3 bars", "2 bars", "last bar", "one". With quantise on CYCLE, a mute or solo lands on
the one, so you can line up a drop without counting.

## 3b. The SETUP page (GLO)

Turn KNOB 1 (or SELECT) to move through the list and KNOB 2 (or PRESET) to change an item. HOME goes back.

| Item | What it does |
| --- | --- |
| CYCLE | The cycle's length: 8, 16, 32 or 64 bars. |
| RUNWAY | How many bars count down: 1, 2, 4 or 8. |
| RUNWAY ON KEYS | The white keys flash the last bar's beats. |
| COUNTDOWN NUMERALS | The big countdown number on the screen. |
| CUE TICK | A short tick on the runway's beats, higher on each bar's first. |
| REDUCED MOTION | No pulsing border, no green flash. The count still shows, and the screen reader still hears it. |
| CUE LEAD | How early a quantised change lands before its line: 0–50 ms (10 by default, so it catches the downbeat). |
| LOCK MIDI IN | The key lock filters MIDI notes coming in, too (off by default). |
| BIG KNOB ON | Where the Big Knob sits: the whole MIX, or only the ECHO RETURN. |
| THROW VELOCITY | ON (the default): a throw is as big as the key was hit. OFF: every throw at full level. |
| TEST TONES | A test tone on every channel (not saved). |
| GUARD TIER | AUTO, or a tier locked as a sound: STUDIO (everything), FOUR-TRACK (mono returns, a simpler spring), SEVEN-INCH (thinner skank and organ, no sub, a plainer echo with heavier tape saturation: rougher, like a 1970s 7-inch). On AUTO, the FM-1 steps down a tier by itself when it's working too hard, and back up when it can; the header shows 4T or 7IN while it's down. |
| PERFORMANCE | Turn KNOB 2: the PERF page, what each part costs and the tier. |
| DEMO SONG | Turn KNOB 2 to load the demo: an original riddim and dub in the 1970s Kingston manner, 72 BPM in A minor, 16 bars (INTRO, VERSE, a DUB section of drum and bass that thins out under the Big Knob, a DROP without the bass, OUTRO). It replaces the patterns, scenes and song (until you SAVE, nothing is lost: switch off to get yours back). Press PLAY: the song plays once. Throw the snare into the echo on beat 3, crash the spring on the DUB section's downbeat, push the echo, dive the tape. |

## 3c. The STEPS page (SEQ)

![The STEPS page: every channel's 16 steps at the top, the bass's below with its notes; LEN 16, RIDDIM ONE DROP](img/screen-steps.png "The STEPS page")

Press SEQ for the steps, HOME to go back to the DUB page. The top shows every channel's 16 steps, the middle the
channel you're editing, bigger, with its notes.

| Control | What it does |
| --- | --- |
| White keys 1–16 | Turn a step on or off. |
| Black keys 1–8 | Pick the channel to edit. |
| Hold black key 9 or 10 + a white key | Make that step an accent or a ghost note (again: back to normal). |
| OCT− · OCT+ | The page of 16 steps: 1–16, 17–32, 33–48, 49–64. |
| KNOB 1 · LEN | The pattern's length, 1 to 64 steps. |
| KNOB 2 · NOTE | The note of the step you hold, or the last one you touched: a semitone a click. On the hats, closed or open. |
| KNOB 4 · RIDDIM | Load a riddim into the drums and bass: ONE DROP, ROCKERS, STEPPERS or FLYERS. It lands on the next bar line, and then it's just steps you can change. |
| REC (on the bass) | The keyboard: all 27 keys play the bass, from F in the octave set on the KEY page. While playing, each note you play is recorded at the nearest step. While stopped, notes go in one step after another. Press REC again for the steps. |

On the skank and organ, a step doesn't hold a note: it holds a voicing, LO, MD or HI (KNOB 2), and the notes are
the chord lane's chord for that bar. Change the chords and the parts follow.

The riddims: ONE DROP has the kick and rim on beat 3 only. ROCKERS puts the kick on 1 and 3 and doubles the rim.
STEPPERS has the kick on every beat. FLYERS opens the hat on every off-beat and closes it on the beat. All four bring a skank on 2 and 4 and an organ
bubble: low on the off-beat eighths, high on the sixteenth after.

## 3e. ARRANGE (HOME, then HOME again): scenes

A scene is a snapshot of the desk: every channel's level, mute, solo, echo and spring sends and throw destination,
the Big Knob and both returns. A scene with patterns (P) keeps every channel's pattern too, so one scene can be the
"dub" and another the "vocal" version of the same riddim.

| Control | What it does |
| --- | --- |
| White keys 1–16 | Recall that scene. Like a mute, it lands on the next line of the quantise setting (hold PLAY for now); the strips blink until then. |
| SAVE held + a white key | Store the desk as it is now into that scene. |
| KNOB 4 | What the next store keeps: DESK, or DESK+PAT (the patterns too, marked P). |
| PRESET (on the DUB page) | Step to the next or previous stored scene, on the line. |

SAVE on its own (pressed and let go) saves everything to the FM-1, scenes included.

### The song

The song is a list of bars (up to 192). Each bar can hold the mutes and solos, the Big Knob and a scene. To make
one, play it: press REC (on the DUB or ARRANGE page) to arm capture, press PLAY, and dub. At every bar line, that
bar takes what the desk is doing: your drops, mutes, solos, Big Knob moves and scene changes. Stop, and press REC to
disarm. A pass that starts from bar 1 becomes the song, as long as you played. HOME + REC takes the last pass back.

Then set KNOB 3 to ONCE (play it through and stop) or LOOP, and press PLAY: each bar lands on its line, as you played
it.

| Control | What it does |
| --- | --- |
| KNOB 1 · BAR | The cursor: the bar a marker goes on while stopped. |
| KNOB 2 · BARS | The song's length. |
| KNOB 3 · SONG | OFF (the desk is yours), ONCE or LOOP. |
| Black keys 1–11 | Put a section marker on the bar playing (stopped: the cursor's bar): INTRO, VERSE, CHORUS, DUB, DROP, BREAK, BRIDGE, BUILD, VOCAL, OUTRO, END. The same key again takes it off. In song mode the runway counts down to the next marker, not the cycle. |

**Motion.** While REC is on, everything your hands do inside a bar is captured too, at the moment you did it: throws,
mutes, freeze, push, tape dive, the crash, and every knob (echo, spring, FX, sounds). When the song plays, they play
back. Bars that hold motion have a white mark on the ARRANGE page.

- **Fix one move, keep the rest.** Record another pass and touch only what you want to redo. A control you touch
  replaces its own motion from that moment to the end of the bar; everything else keeps playing. A throw still held
  over a bar line goes on replacing in the next bar.
- **Take over by hand.** While the song plays, touching a control overrides its motion until the end of the bar.
- **Stop** lets go of any throw, freeze, push or dive that motion was holding.
- **SAVE + REC** (hold SAVE, press REC) clears all motion. HOME + REC (undo) takes back the bars' mutes and scenes,
  not the motion.
- A knob keeps one value per sixteenth (the latest), and a bar holds up to 32 moves. Press SAVE to keep the motion
  over a restart.

## 3d. KEY & CHORDS (ARP): the lock

The lock makes wrong notes impossible: keys outside the allowed notes are silent. They're never moved to a
neighbouring note, so the keyboard keeps its usual spacing and what you learn carries over to a real one. The lock
only filters what you play: notes already recorded play as recorded.

| Control | Bank 1 (ARP) | Bank 2 (ARP again): the chord lane |
| --- | --- | --- |
| KNOB 1 | The key's root | The lane's bar to edit |
| KNOB 2 | The scale: natural minor, dorian, minor pentatonic, minor blues, major, mixolydian, major pentatonic, harmonic minor, phrygian | Its chord's root |
| KNOB 3 | The lock: OFF, SCALE (the key's notes) or CHORD (only the notes of the chord playing now, which follow the lane bar by bar) | Its chord: minor, major, 7, minor 7, major 7, sus 4, diminished |
| KNOB 4 | The keyboard's octave | How many bars the lane has, 1–16 |

HOME + ARP turns the lock on or off from any page. While it's on, the header shows LOCK (or CHD in chord mode), and
the keys light the allowed notes: the root bright, the rest dim. A screen reader hears "lock on, A natural minor"
or "chord lock, A minor". On the STEPS page, KNOB 2 skips notes the lock doesn't allow. On the SETUP page, LOCK MIDI
IN makes the lock filter MIDI notes coming in too.

## 4. Saving

Press SAVE (and let go) to keep the tempo, the quantise setting, the echo's knobs, the SETUP page, the key, its lock and the chord lane, the sounds, the patterns, the scenes and the song. They come back the next time the FM-1 starts. The
mutes and solos aren't saved: scenes will do that.

![Stopped, after changes: 2 BARS quantise, the HAT muted and the BASS soloed](img/screen-settings.png "Changed, not saved yet")

---

## 5. MIDI and audio

MIDI comes in over USB and the TRS jack. On channel 10, General MIDI's drum notes play the drums: 35 or 36 the kick,
37 to 40 the rim, 42 or 44 the closed hat, 46 the open hat (a closed hat cuts an open one short). On channel 2, notes
play the lead; on any other channel, the bass, one at a time. DUBWISE has no Bluetooth.

The voices:

| Voice | Settings (SOUND view) |
| --- | --- |
| Kick | TUNE (where the pitch lands, 40–70 Hz), SWEEP (where it starts), PTIME (how fast it falls), DECAY, CLICK (the beater) |
| Rim | TUNE, DECAY, NOISE, INDEX (how metallic) |
| Hats | TONE, CLOSED and OPEN (their lengths), NOISE |
| Bass | INDEX (the pluck's brightness), IDECAY (how fast the pluck fades to a pure tone), DECAY, CUTOFF, RATIO (1:1 or 2:1, hollow), SUB (an octave below), SUSTAIN |
| Skank | INDEX (the chop's bite), DECAY, TONE (where its band sits), RATIO (1:1 or 1:3, nasal). It sends to the spring by default. |
| Organ | DRAWBAR (how much of the upper partials), DECAY (the bubble's length), CLICK (the key click) |
| Lead | INST (MELODICA: two reeds beating, vibrato after a moment; BRASS: brighter as it gets louder, a scoop up into each note, doubled like a section), BRIGHT, ATTACK, RELEASE, VIBRATO. It sends to the echo by default. Play it from the STEPS page (REC, two octaves above the bass) or MIDI channel 2. |
| Vox (sirens) | PITCH, RATE (how fast it moves), DEPTH (how far, 1–24 semitones), WAVE (sine or triangle); second bank (EDIT again): LIVE, the live input's level (0 off, 100 as it comes) |

The keys sense how hard you play them, with no change to the FM-1. The 20 lowest keys
(F3 to C5) give velocity and pressure. The top seven give velocity only.

Over the USB cable, the FM-1 is also a sound card. Your computer can record what it plays from an input named
"DUBWISE FM-1". It can also play through the FM-1: choose "DUBWISE FM-1" as the output, and you'll hear it from
the FM-1's speaker and headphones, mixed with the FM-1's own sound, at MASTER's volume.

**Live input (coming).** In a future firmware, your computer's audio will come in on channel 8 instead, so you can mix
a vocal or a stem like any other channel: drop it with its mute, throw it into the echo, capture its moves in the song.
Turn it up with channel 8's LIVE (SOUND view, second bank). This needs a small change to the FM-1 platform, which has
been proposed upstream. Until that change lands, the computer's audio is mixed in as described above, and LIVE does
nothing.

---

## 6. The screen reader

DUBWISE tells a screen reader what's on its screen, through the
[FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/). When the reader starts listening, it hears the
name and the settings, and then each change as it happens. The emulator does the same, with your own screen
reader or with its built-in voice.

---

## 7. Troubleshooting

If you hear no sound, turn MASTER up. The voice plays only while you hold a key.

If the installer doesn't find the FM-1, use Chrome or Edge, and allow MIDI when the browser asks. Close any other
programs that use the FM-1's MIDI port.

If the FM-1 doesn't start after an install, it starts in SAFE MODE after two failed starts. There's no sound, but
USB works, so the web installer can reach it. Install again, or put the stock firmware back from the same page.
Press PLAY to try starting the firmware again. If even safe mode fails twice, the FM-1 drops into the chip's own
update mode.

To put the FM-1 in update mode, hold REC while you switch it on. If you installed m-bootloader with the web
installer, the installer can reach the FM-1 from there, and holding PLAY instead opens m-bootloader's recovery
menu. Without m-bootloader, REC starts the chip's own update mode, which only the rescue can reach. For details,
see felucca-bsp's `tools/fm1_rescue.sh`.
