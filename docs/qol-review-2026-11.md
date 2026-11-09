# Quality-of-life ideas from other FM-1 firmwares (2026-11-09)

A survey of the READMEs and manuals of 16 firmwares listed on thefirmwarehub.com, and of fm1-base's optional parts. It
looks only at general quality of life, not at their sound engines. Each "In Dubwise" entry was checked against the
code.

**Sources:**
- X0X: https://github.com/charlesvestal/fm1-x0x
- LOOM-8: https://github.com/charlesvestal/fm1-loom
- Polly, FoMni, AMB-1 and CTL-1: charlesvestal's `fm1-*` repositories
- SLOOP: https://github.com/isod89/sloop-fm1
- Melodee: https://github.com/keremimo/melodee
- Amenity: https://github.com/ejtb76/amenity
- GR!MM: https://github.com/GRiMMBIENT/grimm
- Ravel: https://github.com/De-Pattern/Ravel_FM1
- FM Stud1o: https://github.com/MPASTEAU/FM-Stud1o-FM-1-Firmware
- Dinghy: https://github.com/zvenson/dinghy
- Skiff: https://github.com/illuminus5/skiff
- ORBIT: https://github.com/dspaudio/orbit
- fm1-base: https://github.com/charlesvestal/fm1-base

Hortator, Bubba Machine and Felucca [Salt] couldn't be found, and TRIBE's repository returned 404.

| Feature | Seen in | In Dubwise |
|---|---|---|
| Autosave when stopped and idle (a few seconds), can be switched off | X0X, SLOOP, LOOM-8, FoMni, GR!MM, Melodee | **Yes** (2026-11-09: roadmap 4.5) |
| SAVE lit while there are unsaved changes | X0X, LOOM-8, Polly | **Yes** (`dub.c`: SAVE lit while dirty) |
| Undo and redo for everything (X0X: 32 steps) | X0X, SLOOP, Skiff, LOOM-8 | **Yes** (2026-11-09: HOME + REC / GLO, roadmap 4.6) |
| Hold a button about 1 s for a help card | X0X, LOOM-8, Polly, AMB-1 | **Yes** (2026-11-09: roadmap 4.7) |
| Tap tempo | X0X, SLOOP, Ravel, Skiff, FM Stud1o | **Yes** (2026-11-09: HOME + SEQ taps) |
| Fine tempo (0.1 BPM); halve or double on the beat | Amenity | No |
| Visual metronome (PLAY flashing on the beat); count-in | SLOOP, LOOM-8, FM Stud1o | **Yes** (2026-11-09: PLAY flashes on the beat; a count-in for recording, 4.8) |
| Battery level in the header | X0X, LOOM-8, Ravel, GR!MM | **Yes** (2026-11-09: header icon, BATTERY LOW once) |
| Dim glow on unlit buttons, for dark rooms | X0X, FoMni, Melodee | Keys only |
| Ask before destructive actions (SEL yes, HOME no; or press twice) | X0X, LOOM-8, Ravel | **Yes** (2026-11-09: press again within 3 s; plus NEW PROJECT) |
| CPU load shown, overload flash | FM Stud1o, Polly, Amenity | Partly (guard-tier badge, PERF page) |
| Screen mirror (play the panel from a computer) | LOOM-8; fm1-base `mirror` | **Yes** (2026-11-09: `mirror = true`, `app_redraw`) |
| USB MIDI host (a keyboard straight into the FM-1) | LOOM-8, Melodee; fm1-base `usb_host` | **Yes** (2026-11-09: SETUP USB PORT: KEYBOARD) |
| MIDI learn (a controller knob to any parameter) | SLOOP, Melodee, GR!MM, FM Stud1o | **Yes** (2026-11-09: SETUP MIDI LEARN, 32 CCs) |
| Per-channel USB audio (each track separately into a DAW) | fm1-base `usb_audio_channels` | **Yes** (2026-11-09: 10 channels, SETUP USB STEMS: channels or buses) |
| MIDI timestamps on arrival (tighter clock in) | fm1-base `midi_stamp` | **Yes** (2026-11-09: `midi_stamp = true`, clock in uses it) |
| Clock follows only the device that sent Start; stops after 0.5 s of silence | Amenity | No |
| Encoder acceleration | ORBIT, Amenity | No |
| Separate USB playback volume | Melodee | No |
| Safe mode, rescue at boot, crash records | X0X, LOOM-8, Dinghy, SLOOP, Melodee | Yes (fm1-base) |

## Recommendations for Dubwise, in order

1. **Autosave when stopped.** A dub session is improvised, and losing it at power-off hurts. Save a few seconds after
   the last change while stopped, with a SETUP switch to turn it off.
2. **Undo and redo for desk moves**, not just the song capture. Keep a short history of mutes, scenes, the Big Knob and
   knob values; HOME + REC undoes and a redo key is added. The interface spec already asks for "undo everything".
3. **The help cards and tap tempo the spec already lists.** Dubwise puts taps, holds and double taps on every key, so
   "hold for a card" pays off. Tap tempo locks the desk to a record or a band.
4. **Per-channel USB audio**, so each channel can be recorded into a computer: the dub engineer's multitrack. Cost:
   the USB stream's extra channels, and the stems added in the render.
5. **Small ones together:** confirming before slot overwrite, factory reset and motion clear; battery in the header;
   PLAY flashing on the beat; the screen mirror (`mirror = true`); MIDI timestamps for the clock.
6. **Later:** MIDI learn, then the USB MIDI host (check its RAM against the dub bus first).
