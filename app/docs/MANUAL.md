# DUBWISE

DUBWISE (Dubwise FM, a SpIllt Beats release by DocRuzzy) turns the M-VAVE FM-1 into a reggae and dub mixing desk
you play live. Eight channels (FM kick, snare, hats, bass, skank, organ bubble, melodica or brass, and sirens and
shouts) play a riddim; your hands do the dub: mutes and solos that land on the one, throws into a tape echo and a
spring you can kick, the stepped Big Knob filter, freezes, pushes, tape dives, drop to wet, drum and bass, rewinds.
Record every move into a song, recall scenes, keep four projects in slots.

Press PLAY and you're dubbing: the ONE DROP riddim starts. SEQ edits the patterns (or loads another riddim), FX the
effects, ARP the key and the chord lane, GLO the SETUP page, HOME the scenes and the song. For a ready-made dub to
play along with, load the demo, RIGHTEOUS DUB (SETUP → DEMO SONG). The one-page cheat sheet:
https://docruzzy.github.io/dubwise-fm/cheat/

## Your first ten minutes

1. **Press PLAY.** ONE DROP starts: kick and rim on beat 3, the bass, the skank on 2 and 4. Press PLAY again to stop
   (it stops when you let go).
2. **Drop the bass on the one.** Tap white key 4 (BASS). It doesn't cut at once: it blinks, and lands on the next bar's
   first beat. Tap it again to bring the bass back, on the next one. The middle of the screen says when it lands.
3. **Throw the snare.** Hold white key 10 (the snare's throw) for half a beat: the rim goes into the tape echo and
   rings on. Hit it harder for a bigger throw.
4. **Turn the Big Knob.** Click it clockwise a few steps: the band thins from the bottom up, as King Tubby's filter
   did. Click it back to OFF on the one.
5. **Kick the spring.** Black key 1: a thunderclap from the spring, even with nothing sent to it.
6. **Drop to wet.** Hold black key 5: the band goes, only the echo and spring play on. Let go: the band is back.
7. **Load the demo and play over it.** GLO (SETUP), turn KNOB 1 to DEMO SONG, turn KNOB 2 twice, press HOME, then
   PLAY: RIGHTEOUS DUB, a 2-minute tour of every sound. Throw, mute and turn over the top of it.

That's the core of it: mutes on the one, throws, the Big Knob, the spring. Everything after this is depth: the
channel EQ and pan (3f), scenes and the song (3e), the patterns (3c).

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
| Header | The tempo, playing (triangle) or stopped (square), the cycle bar (a segment a bar, in fours, this bar filling beat by beat), the bar you're in out of the cycle (5/16), and the quantise setting (Q), with the battery beside it (three bars; red when it's nearly empty, and the FM-1 says BATTERY LOW once; none while USB powers it). It turns amber on the runway. After a change, a message shows here for a moment. While playing, the PLAY button flashes on every beat, in both colours on a bar's first: a metronome for your eyes. |
| Channel strips | The eight channels: 1 KICK, 2 SNR (snare), 3 HAT, 4 BASS, 5 SKNK (skank), 6 ORGN (organ), 7 LEAD, 8 VOX. A filled box is playing, with its level brighter over it. An outlined box is muted. A hatched box is muted by someone else's solo. S marks a solo. A strip that blinks white has a change waiting. A line under a strip marks the focus. Under each box, where a throw sends the channel: E (echo), S (spring) or ES (both). A thrown channel's number lights up. |
| Centre, left | The echo: its time, and its repeats as they fall. They turn amber near endless feedback and red past it. A red line underneath shows the echo's limiter at work. |
| Centre, right | The Big Knob: its steps as an arc (10, 20 or 40: SETUP), and where it is. While something waits, what's waiting (CUED) and when it lands: NEXT BEAT, 1 BAR, 2 BARS and so on. On the runway, the countdown. |
| Bottom line | The four knobs and their values: echo TIME, feedback (FB, how long the repeats take to die away), the ECHO return and the spring return (SPRG). While you hold a mute key, the held channel's LEVEL, ECHO send, spring send (SPRG) and filter. |

---

## 3. Controls

| Control | What it does |
| --- | --- |
| White keys 1–8 | Mute or unmute channels 1–8. A tap toggles. Hold a key to flip the channel only while you hold it. Tap twice quickly to solo it. Solos add up: solo two channels and you hear both. A muted channel stays muted when you solo it. |
| HOME + white key 1–8 | Solo, without the double tap. |
| OCT− · OCT+ | The quantise setting: BEAT, 1, 2, 4, 8, 16 or 32 bars, or CYCLE (the next "one"). While playing, a mute or solo waits for the next line of that setting and lands on it, a hair (10 ms) early so it catches the downbeat. Its key blinks until then. (OCT+ changes it when you let go.) |
| Hold OCT+ | **FILL**, while held: the steps set to FILL play, the ones set to NOT FILL rest (a drum roll, a bass run; set them on STEPS). The header shows FILL. Tap OCT− while holding it to latch FILL on; tap OCT+ to let it go. FILL can also be learned to a pedal or a controller's button (MIDI LEARN). |
| PLAY | Start and stop (when you let go). Starting counts from bar 1. Stopping makes anything waiting happen at once; the echo and spring ring on. **Tap PLAY again within 0.4 s of a stop for TAILS OFF:** the echo and spring fade out in 20 ms and are emptied, without starting (a tap after that starts as usual). Hold PLAY while you press a white key to make that change now instead of on the line. |
| SELECT · BIG KNOB | The Big Knob: a steep high-pass filter in steps, like King Tubby's: OFF, 70, 100, 150, 250, 500 Hz, 1, 2, 3, 5, 7.5 kHz. Each click jumps straight to the next step, which is part of its sound. Sweep it up to make the mix "evaporate", snap it back on the one. On the whole mix, or only the echo's return (SETUP). For finer steps between those, SETUP's BIG KNOB STEPS: 20 or 40. **Press the Big Knob** to make it a low-pass instead (as the Altec 9068B, the high-pass's sibling): the same clicks, from 10 kHz at the first step down to 250 Hz at the last, so turning it up muffles the mix as if behind a wall; press again for the high-pass. |
| HOME + SELECT | Tempo, 1 BPM a click. |
| HOME + SEQ, tapped | Tap tempo: from the second tap, the average of the last four gaps (a pause over 2 s starts again). Refused while MIDI clock in has the tempo. |
| Any button, held a second on its own | Its help card for this page. It stays when you let go, and the next press (which does nothing else) or 12 s puts the page back. A combo you hold while thinking still works: a knob or key pressed with the button held closes the card and does its job. |
| ALGORITHM | Moves the focus between the channels. |
| EDIT | The SOUND view of the focused channel. While it's open, the middle of the screen draws the voice's shape from its settings: its loudness (white, filled) and a second curve in amber (the kick's pitch falling, the rim's ring, the open hat, the bass's pluck, the lead's vibrato, the siren's sweep), redrawn as you turn a knob, with a line running across it on each hit (not with REDUCED MOTION). First MAIN: four macros, each turning several settings at once along a shaped curve (the kick's PUNCH: the sweep up, the fall quicker, the click up); in the middle (0) everything is at its default. Press EDIT again for the raw settings, four at a time, then once more to go back to the echo's knobs. A raw setting turned after its macro leaves the macro where it is, and MAIN shows a small dot by it. ALGORITHM picks the channel. Channel 8 (VOX) has no MAIN: its first bank is the siren's. **Patches:** on the SOUND view, PRESET steps through the channel's patches and loads each as you turn (INIT and the factory ones first, then your own; a run of turns is one undo step); the footer shows the patch's name, with a * once you've changed it. Hold SAVE and turn PRESET to keep the sound: into the next free slot of the channel's 16 (named BASS 01, BASS 02 ...), or, if one of your patches is showing, over it (it asks twice). Stopped only (writing the flash would interrupt the sound). Your patches belong to the FM-1, not to a project: they're there in every project. **Renaming one:** with it showing, hold SAVE and turn KNOB 1 to move along the name and KNOB 2 to change the letter (the screen shows it in brackets); let go of SAVE to keep the name. Stopped only, and only your own patches. **Dice** (the last bank, after the raw ones): press ENV to roll the channel a new sound inside safe bounds (KNOB 2: ROLL), or nudge the current one (MUTATE, by KNOB 1's AMOUNT, 5–100 %). KNOB 4 steps back through the channel's last 8 results, each played in as you reach it. KNOB 3 MORPHs from your sound (as it was when you started turning) towards the result KNOB 4 shows: at 50 % halfway between the two, at 100 % the result, back at 0 % your sound exactly. A good way to find the in-between: roll something wild, load your patch again (PRESET), then turn MORPH. To morph towards a patch in the library instead, hold ENV and turn PRESET on the DICE bank: it picks the patch as the target without loading it (MORPH TO STEPPERS ...); KNOB 4 goes back to the history. To keep a setting out of it, hold ENV and turn that knob on MAIN or a raw bank: a lock shows. HOME + EDIT rolls from any page. Dice only ever changes the focused voice: never levels, sends, the dub bus, patterns or other channels. Each roll is one undo step; SETUP's DICE ON LINE makes it land on the next quantise line while playing. **STRIP** (after DICE, the last bank): the channel's EQ, KNOB 1 LO (a shelf at 100 Hz, ±15 dB), KNOB 2 MID (±15 dB), KNOB 3 FREQ (the MID's frequency, 200 Hz to 7.9 kHz, swept smoothly), KNOB 4 HI (a shelf at 5 kHz). The quick way there: hold the channel's key and press EDIT. |
| GLO | The SETUP page (below). |
| MASTER | Sets the volume. |
| SAVE | Saves your project: the tempo, the desk (levels, mutes, solos, sends, each channel's EQ and filter), sounds, patterns, scenes, the song and its moves, the FX and SETUP. SAVE lights while there's something to save. On SETUP's slot rows it saves to (or loads from) a song slot instead. |
| KNOB 1 · TIME | The echo's time, in steps of the beat: 1/16, 1/8T, 1/16., 1/8, 1/4T, 1/8. (dotted eighth, the default), 1/4, 1/2T, 1/4., 1/2. At slow tempos the longest don't fit, so you get the longest that does. A change glides like tape, bending the pitch. |
| KNOB 2 · FB | How long the repeats last: the time they take to fall 60 dB, from 0.1 s to endless (INF). |
| KNOB 3 · ECHO | The echo's return level. |
| KNOB 4 · SPRG | The spring's return level. |
| Hold white key 1–8 + KNOB 1–4 | That channel's **level** (KNOB 1), **its own filter** (KNOB 2), **echo send** (KNOB 3) and **spring send** (KNOB 4): the echo always on KNOB 3 and the spring on KNOB 4, as their returns are. Turning a knob while you hold the key keeps the key from flipping the mute. |
| Hold white key 1–8 + BIG KNOB | **That channel's own filter** (as KNOB 2), on the Big Knob's steps: turn up for the high-pass (70 Hz to 7.5 kHz), down past OFF for the low-pass (closing as you go). Hold several keys and they move together: hold the kick, snare and hats keys for the drums' filter as one. The strip's name turns amber, underlined, while its EQ or filter is in. See 3f. |
| Hold white key 1–8 + press BIG KNOB | Those channels' filters straight back OFF: the snap back on the one. |
| Hold white key 1–8 + ALGORITHM | That channel's **pan**: left, centre, right (several keys held: all of them). Only the part above about 150 Hz moves; the lows stay centred. A small amber tick at the strip's foot shows where it sits. SAVE and slots keep it; the song records it, so it can move. |
| Hold white key 1–8 + EDIT | That channel's **STRIP** at once (its EQ: LO, MID, FREQ, HI), with no banks to step through. ALGORITHM then moves to the next channel's STRIP. See 3f. |
| Black key 2 | Echo hold, while held: no new sound goes in, and the tape loops as it is. |
| Black key 3 | Dub push, while held: feedback past endless, so the echo builds and rings. The tape's saturation and the limiter keep it from blowing up. |
| White keys 9–16 | Throw channels 1–8: while you hold the key, the channel goes to its destination, whatever its fader and even when it's muted, and the echo's feedback lifts so the throw rings on. Hit the key harder for a bigger throw: the softest sends about −8 dB, the hardest full level (SETUP: THROW VELOCITY; off, every throw is full). Tap twice quickly to latch a throw on; tap again to let it go. |
| Hold white key 9–16 + KNOB 1 | That channel's throw destination: ECHO, SPRING or BOTH. Choosing doesn't throw. By default the kick, hats, bass, lead and vox go to the echo, the snare to both, the skank and organ to the spring. |
| Black keys 6 · 7 | Siren A (two tones) and siren B (a rising sweep), on channel 8, while held, louder the harder you press. They go into the echo by default. Their pitch, rate, depth and wave: the SOUND view of channel 8 (ALGORITHM to VOX, then EDIT). |
| Black keys 8 · 9 · 10 | One-shots: BIM, HORN, LASER, on channel 8. A tap plays the whole shot; the harder you press, the louder. Like the sirens they go into the echo, so a shot comes back as a dub repeat. Hold HOME and tap one to play it backwards: it swells up instead of dying away. |
| Black key 4 | Tape dive, while held: the echo's tape slows, so its repeats bend down in pitch; let go and they come back up. How far and how fast: FX page, TAPE bank (DIVE ×1.00–2.00, DIVE TIME 0.1–3 s). The same bank's WOW is the tape's wow and flutter on every repeat (a little by default; 0 for none): each repeat bends a few cents more than the last, as on a real tape echo. Its 3T HEAD adds a third playback head at three times the echo time, as on a Space Echo (whose heads sit at 1 : 2 : 3); with the ECHO bank's 2T HEAD it gives the triplet-feel scatter of the three heads together. When 3T would be longer than the tape (an echo time over about 0.55 s) it reads from the tape's end, 1.67 s. The FX page's last bank, ECHO TYPE: TAPE (as it was) or BBD, the sound of a bucket-brigade delay pedal: the longer the echo time, the darker its repeats (its chip runs slower), and each repeat darker than the one before. |
| Black key 5 | Drop to wet, while held: every channel's dry sound drops out, and only the echo and the spring play on. Let go and the band is back. |
| Black key 11 | Drum and bass, while held: the skank, organ, lead and vox drop out (their sends too), leaving the kick, snare, hats and bass. |
| HOME + PLAY | Rewind (the selector calls it: REWIND, COME AGAIN / PULL UP, SELECTA / WHEEL UP): the last second and a half spins backwards like a record pulled back, the echo and spring stop, and after a beat's silence the tune comes again from bar 1. Stopped, it just spins back. |
| HOME + REC · HOME + GLO | Undo · redo, a step at a time: a mute, a key, a knob's turning, a step, a riddim, a scene. Throws, freezes, pushes and the like let go by themselves, so they aren't kept. When your last action was a song capture, HOME + REC takes that pass back first. |
| (Stereo) | Each voice is mono; you place it with its pan (hold its key and turn ALGORITHM). The pan moves only what's above about 150 Hz, so the kick and bass stay solid in the middle whatever you do. The echo and the spring are stereo: the spring always, the echo with WIDTH. The Seven-inch tier (when the FM-1 is overloaded) folds everything to mono. |
| Black key 1 | Spring crash: strikes the spring tank, a thunderclap even with nothing sent to it. **With a throw key held**, it doesn't crash: it adds the spring to that throw, for as long as you hold it, and **how much spring is how hard you press**. A light touch adds a little, lean in for all of it; let go and the throw goes back to its own destination. The song records the lean. (The keys sense pressure up to C5, so black key 1 does; on the top-row throws, white 13–16, the amount comes from how hard you hit black 1.) |
| FX | The FX page: ECHO (time, feedback, tone, 2T head), SPRING (decay, tone, return), CROSS (the echo's return into the spring, the spring's into the echo), TAPE (the dive), PHASER (where it is: OFF, HATS, SKANK, ECHO or the whole MIX; RATE 0.1–18 Hz, DEPTH, FEEDBACK), GATE (the spring gate: each snare opens the spring for HOLD, then it shuts over RELEASE, so a long spring is chopped into a tail after every snare), DRIVE (tape saturation on the BASS or the whole MIX: DRIVE up to +14 dB into the tape, BLEND with the clean sound; quiet sounds keep their level, loud ones flatten and thicken) and WIDTH (the echo's right side 8–16 ms behind its left, for a wider echo; the phaser's sine or square sweep; PING-PONG, the echo's repeats left, right, left, while the bass in them stays in the middle) and BI-PHASE (a second phaser: MODE SINGLE, CASCADE, the two in a row for deeper sweeps with the second at its own RATE 2, or STEREO, the left and right sweeping against each other, on the echo's return; elsewhere STEREO works as CASCADE so the dry sound stays mono); press FX again for the next. KNOB 1–4 change them. The cross-sends make a loop between the two effects, so they're held back automatically when the echo or the spring is set long: it never runs away. |

**The knobs' colours.** Each knob keeps one job across the pages, so it can wear a colour, and the screen draws its
labels in that colour. The default set:

| Knob | Colour | Its job |
| --- | --- | --- |
| BIG KNOB (SELECT) | Red | The filter: the whole mix's, or a held channel's |
| ALGORITHM | Yellow | Where things sit: the focus, or a held channel's pan |
| PRESET | (none) | Moving around: scenes, patterns, patches |
| KNOB 1 | White | The main one: echo TIME, a channel's LEVEL, LO |
| KNOB 2 | Orange | The heat: feedback (FB), a channel's FILTER, MID |
| KNOB 3 | Blue | The echo: its return, a channel's echo send |
| KNOB 4 | Green | The spring: its return, a channel's spring send |

Colour your caps (or a dot of paint on each) to match, or set the screen to match the caps you have: SETUP's COLOUR
rows, one a knob, OFF for the page's own colours.

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
| SCREEN SAVER | OFF, 1, 5 (the default) or 15 minutes: while stopped and untouched that long, a selector at his desk; every 20 seconds his smoke billows out into a big comic-book cloud in the shape of the SpIllt Beats mark, holds, and breaks up, leaving the picture clear for the other half. Any key, button or knob wakes it, and that touch does nothing else, so waking it never mutes or starts anything. It never comes on while playing. |
| CUE LEAD | How early a quantised change lands before its line: 0–50 ms (10 by default, so it catches the downbeat). |
| LOCK MIDI IN | The key lock filters MIDI notes coming in, too (off by default). |
| BIG KNOB ON | Where the Big Knob sits: the whole MIX, or only the ECHO RETURN. |
| BIG KNOB STEPS | 10 ALTEC (the default): the ten steps of King Tubby's filter, the stepped sound. 20 or 40: finer steps, the ten still among them (evenly between them in pitch, and down to 42 Hz under 70), for a smoother climb. A song or scene sounds the same whichever you choose. |
| THROW VELOCITY | ON (the default): a throw is as big as the key was hit. OFF: every throw at full level. |
| MIDI CLOCK | OFF (the default); IN: follow a computer or drum machine over USB: its Start starts Dubwise from bar 1, its Stop stops it, and its clock sets the tempo and keeps the bars in step; OUT: lead: Dubwise sends Start, 24 clocks a beat and Stop, so other gear follows it. |
| REC QUANTISE | Live recording: FEEL (the default; as you played it), HALF (halfway to the grid) or GRID. |
| REC MODE | Recording into a pattern with steps: OVERDUB (adds) or REPLACE (clears what the playhead passes, keeping what you play). |
| REC OFFSET | 0–60 ms (12 by default) taken off each played note's time, for the sound's way to your ears. |
| CLICK | COUNT-IN (the default: a click on every beat while a pass waits for its line), RECORDING (and while recording), or OFF. |
| USB STEMS | What a computer recording the FM-1 over USB gets on channels 3–10, beside the mix on 1–2: CHANNELS (the default: each of the eight channels on its own, dry, after its fader and mute), BUSES (drums, bass, chords, lead and vox, then the echo's return left and right and the spring's left and right), or OFF. |
| MIDI LEARN | Turn KNOB 2 up, then move a DUBWISE control (any knob, the Big Knob, a mute or throw key, a black key's move), then your controller's knob, fader or pad: it now moves that control, over its whole range (a switch: on from the middle up). Up to 32; SAVE keeps them. Turn KNOB 2 down twice to forget them all. A controller's moves are recorded into the song like your hands', but undo doesn't keep them. |
| USB PORT | COMPUTER (the default) or KEYBOARD: a USB-MIDI keyboard or controller plugged into the FM-1 itself, through a powered hub or adapter (the FM-1 gives the port no power). It plays as MIDI does (below), and its knobs work as learned. While it's a keyboard, there's no computer: no USB audio, mirror or backup. |
| AUTOSAVE | ON (the default): what SAVE keeps is saved by itself once you've stopped and left the FM-1 alone for a few seconds (and the echo has died away), so switching off never loses a session; SAVE's light going out says it's done. OFF: only SAVE saves. |
| DICE ON LINE | OFF (the default): a Dice roll lands at once. ON: while playing, it waits for the next quantise line, like a cued mute. |
| HELP | Every help card, to read anywhere: turn KNOB 2 and each card comes up in turn (the same cards as holding a button for a second). Any press puts the page back. |
| TEST TONES | A test tone on every channel (not saved). |
| OUTPUT | AUTO (the default on a new FM-1): BASS+ for the FM-1's own speaker when nothing is plugged into the headphone jack, the plain line when something is; the row says which it's using now. LINE: always the plain output (headphones, the line out). SPEAKER (BASS+): always the speaker's version, the lows heard through their harmonics on the small speaker. An FM-1 that was set up before keeps its choice. |
| DESK | CLEAN (the default) or MCI: each channel after its fader goes through a gentle saturation, a little hot, the way the MCI desk at King Tubby's overdrove when a fader was pushed (its fader came before its drive). A loud channel thickens; a quiet one passes as it is. After Airwindows' ConsoleMC. Studio tier only: when the guard steps down it goes back to CLEAN. |
| COLOUR BIG KNOB … COLOUR KNOB 4 | Seven rows, one a knob (the Big Knob, ALGORITHM, PRESET, KNOB 1–4): the colour of its cap, OFF, WHITE, RED, ORANGE, YELLOW, GREEN, BLUE or PURPLE. The screen draws that knob's labels (and the Big Knob's arc, the focus mark) in it. The default set: red, yellow, off, white, orange, blue, green. |
| BIG KNOB RESONANCE | OFF (the default: the Altec's, clean steps), LOW or HIGH: each step of the Big Knob rings as it clicks, a peak at the step (about +6 or +12 dB), like the "impedance" of a hardware dub filter. Best with the Big Knob on the echo's return: the repeats whistle up as you climb. |
| ONE SHOTS | FACTORY, or YOURS when you've put your own on the FM-1 (below, "Your own one-shots"). Turn KNOB 2 twice to have BIM, HORN and LASER back (stopped). |
| GUARD TIER | AUTO, or a tier locked as a sound: STUDIO (everything), FOUR-TRACK (mono returns, a simpler spring), SEVEN-INCH (thinner skank and organ, no sub, a plainer echo with heavier tape saturation: rougher, like a 1970s 7-inch). On AUTO, the FM-1 steps down a tier by itself when it's working too hard, and back up when it can; the header shows 4T or 7IN while it's down. |
| PERFORMANCE | Turn KNOB 2: the PERF page, what each part costs and the tier. |
| NEW PROJECT | Turn KNOB 2 twice: a clean start. The desk, sounds, key and chords back to how they were at the first switch-on, ONE DROP in pattern A of each channel, the other patterns, the scenes, the song and its motion empty. Your SETUP preferences and the song slots stay. Until you SAVE (or autosave does), switching off brings your last save back. |
| SAVE TO SLOT | Keep the whole project in one of four slots: the desk, the sounds, the key and chords, the patterns, the scenes, the song and its recorded moves. KNOB 2 picks the slot (it says whether it's empty), then press SAVE. Stopped only. Your SETUP preferences aren't part of it. |
| LOAD SLOT | Bring a slot's project back (KNOB 2 picks it, SAVE loads it), in place of the current one. Press SAVE again afterwards to make it the one that's there at power-on. Stopped only. KNOB 3 picks WHOLE (everything) or AS NEW: a slot as a template, with its desk, sounds, key and patterns but no scenes, song or recorded moves, ready for a new tune. |
| DEMO SONG | Turn KNOB 2 to load RIGHTEOUS DUB, the demo and a tour of the instrument: an original riddim and its dub in the 1970s Kingston manner, 72 BPM in A minor, 41 bars (2 min 17 s). Each section brings something in, and the screen names it as it comes: INTRO, drum and bass on a CLEAN desk; VERSE, the band with the MELODICA, and halfway the desk turns MCI (the same band, coloured); CHORUS, the REED melodica and the B-3 with its Leslie slow, fast, slow; DUB, a freeze, the spring's crash, siren A, the echo's 3T head, a swell and the Big Knob climbing; DROP, the PLUCK skank, the Big Knob as a low-pass closing, the NJD siren and a tape dive; VOCAL, the 808 drums, phased hats, HORN (trumpet) then FM+HORN (sax) and the deejay's shouts; VERSION, the BBD echo and a dub push; OUTRO, the tape drive on the mix, the desk CLEAN then MCI again, a FILL; END, everything drops to the returns. Every move and sound change is the song's motion, so you can see how each was done (and record your own). The desk's colour is the demo's own: your SETUP choice comes back when it stops. It replaces the patterns, sounds, scenes, song and motion (until you SAVE, nothing is lost: switch off to get yours back). Press PLAY: the song plays once. Play on top of it: sirens, one-shots, more throws. |
| LOAD KIT | Seven starting points, each a sound for every voice and a suggested dub bus, after the rooms the music came from: TUBBY'S, CHANNEL ONE, BLACK ARK, STUDIO ONE, SHAKA STEPPERS, SLENG TENG (the digital turn) and ROCKERS UPTOWN (the melodica forward). KNOB 2 picks one, KNOB 3 VOICES or VOICES + BUS (the echo, spring, phaser and drive as well), then press SAVE to load it (it asks twice if you have unsaved changes; HOME + REC undoes it). Each voice is set whole, so a kit sounds the same whatever was there before. They're homages, not the rooms' own sounds: tune them by ear. A kit's voices are also patches: on the SOUND view, PRESET finds them by the kit's name (TUBBYS, CH.ONE, BLACKARK ...). |
| UK STEPPERS | Turn KNOB 2 to set the desk up for UK steppers: 140 BPM, A minor, the UK STEPPERS riddim, the sub bass through the tape, the rim into the echo, the skank into the spring, siren A ready. It changes the patterns, sounds and effects, not your song or scenes. Press PLAY, then throw the rim, hold the siren, take the kick out for a bar. 140 BPM is a starting point: set the tempo by ear. |

## 3c. The STEPS page (SEQ)

![The STEPS page: every channel's 16 steps at the top, the bass's below with its notes; LEN 16, RIDDIM ONE DROP](img/screen-steps.png "The STEPS page")

Press SEQ for the steps, HOME to go back to the DUB page. From the DUB page, hold a channel's mute key and press SEQ to land straight on that channel (its mute stays as it was). The top shows every channel's 16 steps, the middle the
channel you're editing, bigger, with its notes.

| Control | What it does |
| --- | --- |
| White keys 1–16 | Turn a step on or off. Hold an on step to edit it (nudge, gate, locks): it only turns off if you let go without turning a knob. |
| Black keys 1–8 | Pick the channel to edit. |
| Hold black key 9 or 10 + a white key | Make that step an accent or a ghost note (again: back to normal). |
| Hold black key 11 + a white key | That step's CONDITION, the next one each press; with black 11 still held, KNOB 4 runs through them all. Any channel. **75 / 50 / 25 %**: whether it plays is worked out from the pattern's seed, the step and which time round the pattern is, so a song plays the same every time. **1ST** only the first time round after the pattern starts, **NOT 1ST** every time but that. **1:2, 2:2** every other time round (the first or the second of each two); **1:4 … 4:4** one time in four (a turnaround on the fourth bar: 4:4). **PRE** plays when the channel's last step with a condition played, **NOT PRE** when it didn't (a 50 % step, then PRE or NOT PRE: both or neither, or one of the two). **FILL** only while FILL is held, **NOT FILL** only when it isn't. A step that won't play this time round is drawn hollow; the letters above a step name its condition (75, 1S, !1, 12 for 1:2, 34 for 3:4, PR, !P, FL, !F). |
| HOME + ENV | RESEED the pattern: other steps play and skip (HOME + REC takes it back). |
| Hold ENV + a white key | A drum step's REPEAT: OFF, FLAM (a ghost hit just before it), ×2, ×3, ×4 (hits evenly inside the step). Kick, rim and hats. |
| Hold a drum step + KNOB 1 · KNOB 2 | Its DECAY lock and TUNE lock (−8 to +7; the hats keep KNOB 2 for closed / open): that hit only, shorter or longer, lower or higher. A white tick marks a step with locks. |
| OCT− · OCT+ | The page of 16 steps: 1–16, 17–32, 33–48, 49–64. |
| KNOB 1 · LEN | The pattern's length, 1 to 64 steps. Hold HOME: its GRID instead, 1/8, 1/8T, 1/16, 1/16T (triplets) or 1/32. |
| KNOB 2 · NOTE | The note of the step you hold, or the last one you touched: a semitone a click. On the hats, closed or open. |
| KNOB 3 · SWING | The pattern's swing, 50 % (straight) to 75 %: every other step later. Hold a step: NUDGE it off its line instead, a tick (about 3 ms) a click, up to half a step either way. A step off its line shows a small mark. |
| KNOB 4 · RIDDIM | Load a riddim into the drums and bass: ONE DROP, ROCKERS, STEPPERS, FLYERS or UK STEPPERS. It lands on the next bar line, and then it's just steps you can change. Hold a step: its GATE instead, how long the bass or lead note holds (AUTO: a step for the bass, two for the lead). |
| REC | The keyboard and live recording: see below. |

### Four patterns a channel (the session)

Every channel has four patterns, **A, B, C and D**, each with its own length, grid and swing. One plays; the others
wait. It works like a session in Ableton Live, or an OP-Z: put a bass line in A, its version in B, a busier one in C,
and launch them as the tune goes, while the drums keep their own. The letter by each channel shows which plays, amber
while a launch waits for its line.

| Control | What it does |
| --- | --- |
| HOME + white 1–4 | Launch this channel's A–D. Playing, it lands on the next line of the quantise setting (hold PLAY: now) and starts from its top. While HOME is held, whites 1–4 show the patterns: the one playing bright, others with steps dim. |
| HOME + white 9–12 | Launch A–D on every channel at once. |
| SAVE + white 1–4 | Copy the pattern you're editing into A–D (then change the copy). |
| HOME + OCT+ | Double the pattern: its steps again after it, so 16 becomes 32 (64 at most). Then vary the second half. |
| HOME + OCT− | Clear the pattern. |
| A mute key held + PRESET (DUB page) | Launch that channel's next or previous pattern without leaving the desk. The mute stays as it was. |

The STEPS page edits the pattern you launched, at once, while the one playing carries on to its line. Every one of
these is a step of the undo history (HOME + REC).

### Recording live

Press **REC** while playing, and play: the keyboard (bass and lead: all 27 keys, from F in the octave set on the KEY
page), or the pads.

- **An empty pattern** waits for the next bar line, with the click counting you in, records one pass of the pattern
  (set its length first: 16 steps is a bar), then loops it and lets you play along. Like an RC-505.
- **A pattern with steps** records at once, until you press REC again. REC MODE (SETUP) is OVERDUB, which adds, or
  REPLACE, which clears what the playhead passes, keeping what you play.
- **Where it lands:** a note keeps where you played it, to about 3 ms (REC QUANTISE: FEEL). HALF moves it halfway to
  the grid; GRID puts it on the grid. A note played just before the line of an armed pass starts the pass, so an
  early downbeat counts.
- **How hard and how long:** a soft note becomes a ghost, a hard one an accent. The bass and lead keep how long you
  held each note.
- **The pads:** on a drum channel, whites 1–4 are the kick, rim, closed hat and open hat, each into its own channel.
  On the skank or organ, whites 1–3 play the chord lane's chord low, middle or high.
- **Stopped,** REC turns the keyboard on, and notes go in one step after another at a cursor. Press PLAY with it on:
  a bar of click, then the pass.
- **SEQ** leaves the keyboard. A pass is one step of the undo history: HOME + REC takes the whole pass back.
- **REC OFFSET** (SETUP) makes up for the time the sound takes to reach your ears: if your notes land late, raise it.
  In a browser, try 40–80 ms.

On the skank and organ, a step doesn't hold a note: it holds a voicing, LO, MD or HI (KNOB 2), and the notes are
the chord lane's chord for that bar. Change the chords and the parts follow.

The riddims: ONE DROP has the kick and rim on beat 3 only. ROCKERS puts the kick on 1 and 3 and doubles the rim.
STEPPERS has the kick on every beat. FLYERS opens the hat on every off-beat and closes it on the beat. These four bring a skank on 2 and 4 and an organ
bubble: low on the off-beat eighths, high on the sixteenth after. UK STEPPERS is the sound-system branch: the kick on
every beat with the snare only on 3, so it walks in half time over the kick; off-beat hats, a rolling sub line, the
skank chopping every off-beat, no organ.

## 3e. ARRANGE (HOME, then HOME again): scenes

A scene is a snapshot of the desk: every channel's level, mute, solo, echo and spring sends, throw destination, EQ and
filter, the Big Knob and both returns. A scene with patterns (P) also keeps which pattern (A–D) each channel plays, like a
row of the session: one scene can be the "vocal" version (bass A, skank on) and another the "dub" (bass B, the
skank out).

| Control | What it does |
| --- | --- |
| White keys 1–16 | Recall that scene. Like a mute, it lands on the next line of the quantise setting (hold PLAY for now); the strips blink until then. |
| SAVE held + a white key | Store the desk as it is now into that scene. |
| KNOB 4 | What the next store keeps: DESK, or DESK+PAT (which pattern each channel plays too, marked P). |
| PRESET (on the DUB page) | Step to the next or previous stored scene, on the line. |
| A mute key held + PRESET (DUB page) | That channel's next or previous pattern (A–D), on the line. |

SAVE on its own (pressed and let go) saves everything to the FM-1, scenes included.

### The song

The song is a list of bars (up to 512: about 28 minutes at 72 BPM, 15 at 140). Each bar can hold the mutes and solos, the Big Knob, a scene, and which pattern each channel plays (so launching B on the bass mid-take is in the song). To make
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
| Black keys 1–11 | Put a section marker on the bar playing (stopped: the cursor's bar): INTRO, VERSE, CHORUS, DUB, DROP, BREAK, BRIDGE, BUILD, VOCAL, OUTRO, END. The same key again takes it off. In song mode the runway counts down to the next marker, not the cycle. Hold HOME for the dub-plate words on black 1–4: VERSION, DUBPLATE, SPECIAL, RUB-A-DUB. |

**Motion.** While REC is on, everything your hands do inside a bar is captured too, at the moment you did it: throws,
mutes, freeze, push, tape dive, the crash, the sirens and one-shots, and every knob (echo, spring, FX, sounds). When
the song plays, they play back exactly: the same moves on the same samples, so the song sounds as the pass did, bit
for bit. (While playing, a move lands on the song's next motion tick, at most 1.7 ms later at 75 BPM, so the pass
you hear and its replay are the same.) Bars that hold motion have a white mark on the ARRANGE page.

- **Fix one move, keep the rest.** Record another pass and touch only what you want to redo. A control you touch
  replaces its own motion from that moment to the end of the bar; everything else keeps playing. A throw still held
  over a bar line goes on replacing in the next bar.
- **Take over by hand.** While the song plays, touching a control overrides its motion until the end of the bar.
- **Stop** lets go of any throw, freeze, push or dive that motion was holding.
- **SAVE + REC** (hold SAVE, press REC) clears all motion. HOME + REC (undo) takes back the bars' mutes and scenes,
  not the motion.
- A knob keeps one value per sixteenth (the latest), a bar holds up to 128 moves, and the whole song 4,096. Press SAVE to keep the motion
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

## 3f. Dub moves with the channel EQ

The engineers mixed with blunt tools: a stepped filter and a few EQ knobs per channel, played as a performance, not set
and forgotten (mastery guide C3, C12). Each channel has them here: the Big Knob's steps of its own, and an EQ (LO, MID,
FREQ, HI). The song's motion records every move, and a scene keeps the whole desk's EQ.

| The move | How |
| --- | --- |
| **Thin one channel to nothing** (Tubby: a horn "into a faint squeal, then gone") | Hold its key and turn the Big Knob up, a click at a time, on the beat. Let go of the key: the mute stays as it was. |
| **Filter the drums, the band left alone** (the drum bus drill) | Hold the kick, snare and hats keys together and turn the Big Knob: the three filters move as one. |
| **Snap back to bypass on the one** | Keep the keys held and press the Big Knob: their filters are off at once, as you press (time it for the downbeat). |
| **Muffle a channel behind a wall** | Hold its key and turn the Big Knob *down*, past OFF: the low-pass (the Altec 9068B's steps) closes as you go. |
| **Scientist's mid sweep** | Hold the channel's key and press EDIT (its STRIP), set MID up (KNOB 2, +10 dB or so), then sweep FREQ (KNOB 3). The peak glides, so the sweep sings rather than steps. Bring MID back to FLAT to end it. |
| **"A little bass, take off bass"** (Tubby's EQ) | On the STRIP: LO and HI, blunt shelves. Take the bass's LO down for a thinner, harder line; lift the skank's HI to cut through. |
| **A different desk per section** | Store a scene with the EQ as it is (ARRANGE: SAVE held + a white key). Recalling it brings every channel's EQ and filter back on the line. |
| **Record it** | Arm the song (REC) and play the moves: the filter's clicks and the mid's sweep are kept bar by bar and play back as you made them. |

The whole mix still has the Big Knob itself (turned with no key held). Channel filters and the Big Knob stack: a
channel thinned on its own, then the whole mix evaporating behind it.

### A horn dub, the classic chain

The Kingston horns were recorded dry and close, then thrown, filtered and fed back at the desk. On the FM-1, with
HORN (or FM+HORN) on the lead, channel 7:

| Step | On the FM-1 |
| --- | --- |
| **Dry and driven** | SETUP's DESK: MCI, the console's drive on every strip. The lead's BRIGHT is HORN's blare: the default is the reggae sections' brightness, up for a harder edge, down for a softer one. |
| **The dub throw** | Keep the horns muted (white 7) and catch a stab at a phrase's end with its throw (white 15): it goes to the echo at full level, muted or not, harder for more. The FX page's TAPE bank: WOW for the repeats' wobble; hold black 3 (push) to break them up. |
| **Filter the tail** | SETUP's BIG KNOB ON: ECHO puts the Big Knob on the echo's return, and BIG KNOB RESONANCE (LOW or HIGH) makes each step ring: turn it up as the repeats ring, and they whistle up and away. Send the echo into the spring (FX: CROSS) for the splash. |
| **The wall** | FX's ECHO TYPE: BBD, darker as it repeats. Hold black 5 (drop to wet): the band goes, the horns' echo carries on; hold black 3 with it for the edge of self-oscillation. |

## 4. Saving

With AUTOSAVE on (SETUP, the default), this happens by itself a few seconds after you stop and leave the FM-1
alone. Press SAVE (and let go) to keep the tempo, the quantise setting, the echo's knobs and the FX page, the SETUP page, the key, its lock and the chord lane, the desk (every channel's level, mute, solo, sends, EQ and filter), the sounds, the patterns, the scenes, the song and its
recorded moves. They come back the next time the FM-1 starts, sounding as you left them. To keep more than one project, use SETUP's song slots (four of them); to keep
them off the FM-1, see Backing up, below.

![Stopped, after changes: 2 BARS quantise, the HAT muted and the BASS soloed](img/screen-settings.png "Changed, not saved yet")

---

## 5. MIDI and audio

**Asking twice.** What can't be undone asks first, and says so (AGAIN TO CONFIRM): do the same thing again within 3
seconds. That's clearing the motion (SAVE + REC), loading the demo, UK STEPPERS or a slot over unsaved changes,
saving over a slot that has a song in it, and NEW PROJECT. Everything else, HOME + REC can take back.

MIDI comes in over USB (from a computer, or a keyboard plugged straight in: SETUP, USB PORT) and the TRS jack.
Control changes move what you've taught them to (SETUP, MIDI LEARN). Each message is timed where it lands on the FM-1 (to 0.05 ms), so MIDI
CLOCK IN follows a computer or drum machine tightly. On channel 10, General MIDI's drum notes play the drums: 35 or 36 the kick,
37 to 40 the rim, 42 or 44 the closed hat, 46 the open hat (a closed hat cuts an open one short). On channel 2, notes
play the lead; on any other channel, the bass, one at a time. DUBWISE has no Bluetooth.

The voices:

| Voice | MAIN (macros) | Settings (SOUND view) |
| --- | --- | --- |
| Kick | PITCH · PUNCH · DECAY · DRIVE | TUNE (where the pitch lands, 40–70 Hz), SWEEP (where it starts), PTIME (how fast it falls), DECAY, CLICK (the beater), DRIVE (a soft clip on the body: rounder to harder), CTONE (the click's pitch: duller below 0, brighter above), MODEL: SINE (the swept sine), THUMP (an FM burst in the body: a punchier, woodier kick), BOOM (lower, longer and driven: a sound-system boom), 808 (after Mutable Instruments Plaits' model of the TR-808's circuit: a pulse ringing a tuned filter; here TUNE is its pitch, SWEEP the attack's pitch kick, DECAY its ring, CLICK its tone, DRIVE its punch) |
| Rim | TUNE · SNAP · DECAY · BODY | TUNE, DECAY, NOISE, INDEX (how metallic), DROP (a fast pitch drop on the hit), HPF (its high-pass: fuller below 0, thinner above), MODEL: CROSS (the cross-stick), SNARE (a lower body with a longer rattle of noise), SYNDRUM (a pure tone falling in pitch: the Channel One fill), 808 (the TR-808 snare, after Plaits: its shell rings in two modes, five as INDEX goes up, NOISE is the snappy) |
| Hats | TONE · CLOSED · OPEN · HPF | TONE, CLOSED and OPEN (their lengths), NOISE, HPF (lower: more body; higher: sizzle only), MODEL: METAL (four detuned squares), NOISE (noise alone: softer, airier), 808 (after Plaits: six squares through a resonant band-pass, HPF its colour, NOISE how noisy) |
| Bass | TONE · PLUCK · LENGTH · WEIGHT | INDEX (the pluck's brightness), IDECAY (how fast the pluck fades to a pure tone), DECAY, CUTOFF, RATIO (1:1 or 2:1, hollow), SUB (an octave below, added after the drive and the filter so it stays clean), SUSTAIN, FEEDBACK (a growl: the modulator feeds itself, capped so the note stays on top), DRIVE (saturation before the filter), GLIDE (slides between notes held together; none from silence), MODEL: ROUND, PLUCK (a brighter attack, the filter opened), HOLLOW (2:1, whatever RATIO says), STEPPERS (the growl and the sub under it), SUB TONE: SINE, or CLASSIC (a little of the sub's 3rd and 5th: you hear it on small speakers too) |
| Skank | TONE · LENGTH · COLOUR · ATTACK | INDEX (the chop's bite), DECAY, TONE (where its band sits), RATIO (1:1 or 1:3, nasal), ATTACK (0 the chop, up: a softer swell), MODEL: FM (as built) or PLUCK (real plucked strings, one a note of the chord, picked harder the harder you play, ringing for DECAY and then muted by the fretting hand: the chop; INDEX and RATIO do nothing on it). It sends to the spring by default. |
| Organ | DRAWBAR · LENGTH · CLICK · DETUNE | DRAWBAR (how much of the upper partials), DECAY (the bubble's length), CLICK (the key click), DETUNE (the voices drift apart: a chorus), MODEL: BUBBLE (the stab the riddims are built on) or B-3 (a tonewheel organ through a rotary speaker; below) |
| Organ, B-3 | REGISTRATION · PERC · ROTARY · DRIVE | REGISTRATION steps through eight drawbar settings, soft to bright (the middle is 888 000 000, the full upper); PERC: OFF, 2ND, 3RD (the harmonic struck at a chord's start, and only from silence: a chord played straight after another doesn't strike it, as on the real one); ROTARY: OFF, SLOW, FAST (each rotor speeds up or slows down at its own pace, the horn quickly, the drum slowly; BRAKE in the raw settings stops them); DRIVE (setBfree's tube preamp; 0 is off, and higher is a different character, not only more: it doesn't rise evenly). The raw settings: the nine drawbars 16′ 5⅓′ 8′ 4′ 2⅔′ 2′ 1⅗′ 1⅓′ 1′, 0–8 as on the real bars, then PERC FAST, VIB/CHORUS (V1–3, C1–3: the scanner) and the rest, ending with LESLIE: LIGHT (ours, cheap) or FULL (setBfree's own Leslie model: the horn's beam and the cabinet's reflections; it costs more, so it's there in the STUDIO tier only, and the FM-1 falls back to LIGHT when it's working hard). Percussion takes the 1′ drawbar's place while it's on, and with it on the organ is a little quieter, as on the real one. A B-3 chord holds as long as its step's GATE (a step if AUTO) and dies over DECAY when let go. In the STUDIO tier the horn is stereo (only above ~500 Hz: the low end stays mono); FOUR-TRACK sums it; SEVEN-INCH keeps the rotary's throb but drops the horn's pitch wobble and the scanner. The factory patches B3 SOFT and B3 PERC are a start (PRESET on the organ's SOUND view). |
| Lead | BRIGHT · ATTACK · RELEASE · VIBRATO | INST (MELODICA: two reeds beating, vibrato after a moment; BRASS: brighter as it gets louder, a scoop up into each note, doubled like a section; HORN: a trumpet or a saxophone (HORN TYPE) fitted to recordings of real ones, brighter and brassier the harder you play, with the small wavers and breath of a real player; FM+HORN: BRASS and HORN layered, BLEND from half each to all HORN; REED: a free-reed melodica built from how a real reed works, its air cut off once a cycle and shaped by the instrument's body, brighter and a touch flatter the harder you blow, with a little breath in it), BRIGHT, ATTACK, RELEASE, VIBRATO, DETUNE (the reeds or the section closer, below 0, or wider). It sends to the echo by default. Play it from the STEPS page (REC, two octaves above the bass) or MIDI channel 2. |
| Vox (sirens) | — | PITCH, RATE (how fast it moves), DEPTH (how far, 1–24 semitones), WAVE (sine or triangle); second bank (EDIT again): LIVE, the live input's level (0 off, 100 as it comes), then the deejay voice: SAY (OFF, or a shout: SELECTA, WHEEL UP, BIG UP, RUBADUB, FORWARD, JAH, ONE MORE, SOUND SYSTEM, DUB, PULL UP, LICKSHOT, BOOM, YES I, IRIE, ROOTS, STRICTLY), SAY PITCH (±12 semitones), STUTTER (a syllable caught and said again), CRUSH (fewer bits), and SHAPE: CLASSIC (clean tones, as before) or NJD (after the classic dub siren box: siren A swings up fast and eases back down, siren B rises quickly then slows into the top, both as a buzzier pulse rounded off). With SAY on a shout, black key 8 says it instead of BIM. It's a 1978-style speech chip: robotic on purpose, best thrown into the echo |

The keys sense how hard you play them, with no change to the FM-1. The 20 lowest keys
(F3 to C5) give velocity and pressure. The top seven give velocity only.

Over the USB cable, the FM-1 is also a sound card. Your computer can record what it plays from an input named
"DUBWISE FM-1": ten channels, the mix on 1–2 and the stems on 3–10 (see "Recording into a computer" below). It can also play through the FM-1: choose "DUBWISE FM-1" as the output, and you'll hear it from
the FM-1's speaker and headphones, mixed with the FM-1's own sound, at MASTER's volume.

**Live input (coming).** In a future firmware, your computer's audio will come in on channel 8 instead, so you can mix
a vocal or a stem like any other channel: drop it with its mute, throw it into the echo, capture its moves in the song.
Turn it up with channel 8's LIVE (SOUND view, second bank). This needs a small change to the FM-1 platform, which has
been proposed upstream. Until that change lands, the computer's audio is mixed in as described above, and LIVE does
nothing.

---

**Recording into a computer.** Over USB the FM-1 is an audio interface with ten channels in: the mix on 1–2, and
on 3–10 the stems SETUP's USB STEMS chooses. CHANNELS gives each channel dry, as the desk hears it before the
effects: record a take as a multitrack, the way a dub was mixed from the tape. BUSES gives the drums, bass, chords and
lead with vox, and the echo's and the spring's returns in stereo: mix the dub again in your computer, or keep the
effects as they happened. 16-bit, 44.1 kHz; MASTER doesn't change the stems' level.

**Screen mirror.** Over USB, a computer can show the FM-1's screen and panel, and you can play the panel from it:
fm1-base's mirror page (base/web/mirror/) or `tools/fm1_mirror.py`. Handy for teaching, streaming or a screen
reader on the computer.

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

---

## Backing up, and sharing songs

**The whole FM-1 to a file.** `python3 base/tools/fm1_backup.py backup FILE` saves what's stored (your project and
settings); `restore FILE` puts it back (the FM-1 asks you to confirm with SEL; stop playing first). The backup includes your
sound patches. Restoring a backup made before patches existed empties the patch library (it puts the FM-1 back as it
was then), so keep the patches you want with `dubwise_slots.py patch get` first.

**One song slot to a file, to keep or share.** With the FM-1 connected over USB and stopped:

```
python3 tools/dubwise_slots.py get 2 mydub.dws     # slot 2 to a file
python3 tools/dubwise_slots.py put 3 mydub.dws     # a file into slot 3
python3 tools/dubwise_slots.py info mydub.dws      # what a file holds
```

A `.dws` file is a whole project: the desk, sounds, key, patterns, scenes, the song and every recorded move. After a
`put`, load it on the FM-1 with SETUP → LOAD SLOT. (Both tools need `pip install mido python-rtmidi`.)

**Sound patches to a file, to keep or share.** Channels are 1–8 (KICK SNARE HATS BASS SKANK ORGAN LEAD VOX), slots
1–16:

```
python3 tools/dubwise_slots.py patch list 4                 # the bass's 16 slots, by name
python3 tools/dubwise_slots.py patch get 4 1 mybass.dwpatch # bass slot 1 to a file
python3 tools/dubwise_slots.py patch put 4 7 mybass.dwpatch # a file into bass slot 7 (stopped)
python3 tools/dubwise_slots.py patch current 4 now.dwpatch  # the bass as it sounds now
python3 tools/dubwise_slots.py patch name mybass.dwpatch "SUB ROLL"   # rename a file (8 characters)
```

A `.dwpatch` file is one channel's sound (48 bytes); it goes back onto the same channel.

**Your own one-shots.** Black keys 8, 9 and 10 play BIM, HORN and LASER. Put your own sounds there instead: a vocal
tag, an air horn, a "selecta!". Make WAV files on a computer (any rate, mono or stereo; set their levels as you want
them: nothing is changed but a 10 ms fade at the end), 1.5 seconds in all, split any way between the three. Then, with
the FM-1 connected and stopped:

```
python3 tools/dubwise_slots.py shots put tag.wav horn.wav zap.wav   # black keys 8, 9, 10 (named by the files)
python3 tools/dubwise_slots.py shots get myshots.dwshots            # what's on the FM-1, to a file
python3 tools/dubwise_slots.py shots put myshots.dwshots            # .. and back
python3 tools/dubwise_slots.py shots factory                        # BIM, HORN and LASER back
```

They play at once, and they stay through a reboot, a firmware update and NEW PROJECT; the screen shows their names
(the files', 8 characters). `python3 tools/make_oneshots.py user a.wav b.wav c.wav -o mine.dwshots --names TAG,AIR,ZAP`
makes a `.dwshots` file without the FM-1. A backup (`fm1_backup.py`) doesn't carry them: keep `shots get`'s file beside
it. SETUP → ONE SHOTS also brings the factory ones back.

**M-VAVE's SMC-Mixer as the desk.** Set SETUP → USB PORT to KEYBOARD and plug the SMC-Mixer (in its DAW mode) into
the FM-1's USB port; the FM-1 gives the port no power, so the mixer needs its own (its battery, or a powered hub). The
screen says SMC-MIXER: THE DESK, and its eight strips are Dubwise's:

| On the mixer | Does |
|---|---|
| Faders | The strips' levels. A fader takes over once you bring it to where the level is (its LED blinks until then), so nothing jumps |
| MUTE | Mutes, on the next quantise line, like a tap of the mute key |
| SOLO | Solo |
| REC | A throw, while you hold it: to the strip's destination, in full |
| SELECT | The focus (ALGORITHM's) |
| Knobs 1–8 | BIG KNOB, ECHO TIME, ECHO FB, ECHO RETURN, SPRING RETURN, the focused strip's echo and spring sends, TEMPO |
| PLAY / STOP / REC | The transport, and the song's capture |

Its LEDs follow the FM-1 (what's muted, soloed, thrown, focused, playing). Everything it does is undone with HOME + REC
and captured into the song like the panel's own moves. While it's connected there's no computer on the port.

**The sound editor in your browser.** On the project's site, the Sound editor page shows every setting of all eight
voices as sliders. Connect the FM-1 over USB, press Connect (Chrome or Edge: they let a page talk MIDI; allow SysEx when
asked), pick a channel's tab and move the sliders: the FM-1 changes as if you'd turned its knobs, so a song that's
capturing keeps the moves and HOME + REC undoes them. The page also reads your 16 patches a channel, loads one into the
sound or keeps the sound in a slot, saves or opens `.dwpatch` files, and loads any of the seven kits (with or without
its dub bus), as SETUP's LOAD KIT does.

---

## For those who read to the end

- **ROOTS NAMES.** On the SETUP page, tap black key 11 three times within two seconds. The screen reader then names
  some tools after the hands and houses they come from: the Big Knob is KING TUBBY KNOB, the Bi-Phase BLACK ARK PHASE
  (Lee Perry), the ping-pong MAD PROFESSOR PING-PONG, the sirens SHAKA SIREN A and B (Jah Shaka), the tape drive
  ON-U DRIVE (On-U Sound), the STEPPERS riddim CHANNEL ONE STEPPERS and the FLYERS riddim BUNNY LEE FLYERS. The page
  labels stay plain. Three taps again turn it off.
- Other things to find: the rewind has more than one call; the dub-plate section words are HOME + black 1–4 on
  ARRANGE; play at 72.0 BPM and watch the header; leave the screen saver alone for an hour.
