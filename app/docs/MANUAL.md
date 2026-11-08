# DUBWISE

DUBWISE (Dubwise FM) is a reggae/dub groovebox for the M-VAVE FM-1 in the making, where the mixing desk is the
instrument. This early build has the desk's first move: the DUB page, where the white keys mute and solo the eight
channels, and every change can wait for the next beat or bar line so it lands on time.

There are no drum or bass voices yet. Press GLO to put a test tone on every channel so you can hear the mutes. MIDI
in plays a test voice on the bass channel. There's no Bluetooth: the RAM it would take goes to the dub bus.

![The DUB page, stopped: 75.0 BPM, BAR 1.1, Q 1 BAR in the header; eight channel strips, all playing](img/screen-ready.png "The DUB page at rest")

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
| Header | The tempo, playing (triangle) or stopped (square), the bar and beat, and the quantise setting (Q). After a change, a message shows here for a moment. |
| Channel strips | The eight channels: 1 KICK, 2 SNR (snare), 3 HAT, 4 BASS, 5 SKNK (skank), 6 ORGN (organ), 7 LEAD, 8 VOX. A filled box is playing, with its level brighter over it. An outlined box is muted. A hatched box is muted by someone else's solo. S marks a solo. A strip that blinks white has a change waiting. A line under a strip marks the focus. |
| Centre | What's waiting (CUED) and when it lands: NEXT BEAT, 1 BAR, 2 BARS and so on. |
| Bottom line | The four knobs' places: echo time, feedback, echo and spring. They arrive with the echo. |

---

## 3. Controls

| Control | What it does |
| --- | --- |
| White keys 1–8 | Mute or unmute channels 1–8. A tap toggles. Hold a key to flip the channel only while you hold it. Tap twice quickly to solo it. Solos add up: solo two channels and you hear both. A muted channel stays muted when you solo it. |
| HOME + white key 1–8 | Solo, without the double tap. |
| OCT− · OCT+ | The quantise setting: BEAT, 1, 2, 4, 8, 16 or 32 bars. While playing, a mute or solo waits for the next line of that setting and lands on it, a hair (10 ms) early so it catches the downbeat. Its key blinks until then. |
| PLAY | Start and stop (when you let go). Starting counts from bar 1. Stopping makes anything waiting happen at once. Hold PLAY while you press a white key to make that change now instead of on the line. |
| HOME + SELECT | Tempo, 1 BPM a click. |
| ALGORITHM | Moves the focus between the channels. |
| PRESET | Picks the test voice's wave: sine or triangle. |
| GLO | Test tones on every channel, on or off, so you can hear the desk until the voices arrive. |
| MASTER | Sets the volume. |
| SAVE | Saves the tempo, the quantise setting and the wave. SAVE lights while there's something to save. |
| White keys 9–16, black keys, the four knobs | Show their names. Throws, the echo, the spring and the rest come next. |

The keys light by one rule: bright is playing, dim is muted or not in use yet, and a slow blink means waiting.

---

## 4. Saving

Press SAVE to keep the tempo, the quantise setting and the wave. They come back the next time the FM-1 starts. The
mutes and solos aren't saved: scenes will do that.

![Stopped, after changes: 2 BARS quantise, the HAT muted and the BASS soloed](img/screen-settings.png "Changed, not saved yet")

---

## 5. MIDI and audio

MIDI comes in over USB and the TRS jack. A note on any channel plays the test voice on the bass channel, and a
note off releases it. DUBWISE has no Bluetooth.

The keys sense how hard you play them, with no change to the FM-1. The 20 lowest keys
(F3 to C5) give velocity and pressure. The top seven give velocity only.

Over the USB cable, the FM-1 is also a sound card. Your computer can record what it plays from an input named
"DUBWISE FM-1". It can also play through the FM-1: choose "DUBWISE FM-1" as the output, and you'll hear it from
the FM-1's speaker and headphones, mixed with the FM-1's own sound, at MASTER's volume.

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
