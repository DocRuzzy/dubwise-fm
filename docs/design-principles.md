# Dubwise FM — Design principles

Set by Kirk on 2026-11-08. These govern every architecture and roadmap decision.

1. **Driven by user input, not generative.** Nothing changes unless the user changes it. Riddim templates are editable starting points, not generators.
   - *Clarified 2026-11-09 (Kirk, roadmap 5.6, 5.9):* chance is allowed only as a tool the user fires: Dice rolls a sound, and a step's probability follows the pattern's stored seed. Each is an explicit action, stored, undoable and repeatable (a captured pass replays identically). Neither ever changes the pattern, the desk or the song on its own.
2. **The mixer is the instrument.** Mutes, sends, throws, return self-sends and the stepped HPF are first-class state: quantisable, recordable into the song, and recallable as scenes or versions.
3. **Structure quantisation.** Launch quantise at beat (fired slightly early), 1, 2, 4, 8, 16 or 32 bars. A phrase counter is always visible. The song is a per-bar list that can capture a live dub pass.
4. **Own the instrument, reuse the platform.** Write our own engine, sequencer and UI on fm1-base / felucca-bsp. Learn from DUB-1 and X0X; don't inherit their priorities.
5. **Functionality over fidelity, and pay for it in style.** Hi-fi is preferred but negotiable. If the full set of critical functions only fits by lowering fidelity, lower it. Do it where the dub idiom already sounds that way: 1970s Kingston was 4-track, tape, mono and 7-inch.
6. **Mono core, stereo as a bonus.** The low end is mono: below ~150 Hz every channel sits in the middle. Above it, each channel has a pan (8.9, the owner's decision of 2026-11-10, after the studies found the records 4-15 dB wider than us above 125 Hz: their instruments were panned in the stereo mixes). Stereo also lives on the effect returns: an echo Width offset (P1), ping-pong (P3). The echo is one line with two read heads sized for 2T, so ping-pong is a routing change, not a rewrite. Everything must sum to mono cleanly, and the Seven-inch tier is mono. (Before 8.9 the dry path was mono throughout.)
7. **Open source from day one** (GPL-3.0-only, public repo).

## Fidelity policy (principle 5 in practice)

### Never compromise on these
- Timing: sample-accurate sequencing and mute/throw events, no jitter.
- No dropouts. Overload degrades through guard tiers; it never glitches.
- A clean low-end fundamental: sine-like bass, kick and bass that don't fight, mono below ~120 Hz.
- Feedback-loop stability: a gain cap, in-loop filtering and limiting, and no runaway or DC buildup.

### Fidelity we can spend (with the stylistic rationale)
| Lever | Saving | Why it still sounds like dub |
|---|---|---|
| Mono dry path below ~150 Hz (8.9: each channel pans above it; returns stereo) | ~½ of voice and FX cost on the dry path (a pan: ~1.5 % a panned channel, the voices stay mono) | Most 70s Jamaican dub was mono or near-mono for 7-inch and sound system |
| Tape echo at 11 kHz, int16, with saturation in the loop | RAM ÷4, CPU ÷4 | Tape and RE-201 repeats are band-limited and darker on every pass |
| Spring at 11–22 kHz | CPU ÷2–4 | Real spring tanks roll off around 4–5 kHz |
| Tonal voices (bass, organ, skank, melodica, brass) at 22.05 kHz | Voice CPU ÷2 | Little musical content above 10 kHz. Mild FM aliasing reads as grit (keep the index bounded) |
| Hats and rim at full rate, but cheap (noise + HPF, or short inharmonic FM) | Low | Hats are the only part that needs top end |
| Envelopes, LFOs and filter coefficients at control rate (every 16–32 samples) | Large | Inaudible if interpolated |
| Polyphony caps: skank 3 voices, organ 2–3 ops, brass section = 1 voice + short delay doubling | Voices | Period sections were often doubled by tape or overdubs anyway |
| int16 buffers everywhere; 8-bit mu-law or 4-bit ADPCM for vocal and found-sound snippets | RAM and flash | Dubplate, cassette and sound-system grit |
| Tape-style saturation on buses as glue | Small | It masks quantisation and aliasing and it's idiomatic |

### Guard tiers, presented as a character setting
Overload protection and fidelity choices are one ladder. The user can also pick a tier deliberately as a sound.

| Tier | Settings |
|---|---|
| **Studio** | Full-rate hats, 22 kHz tonal voices, spring at 22 kHz, stereo returns (Width / ping-pong) on, phaser on |
| **Four-track** | Mono returns, spring at 11 kHz, organ 2-op, phaser off when overloaded |
| **Seven-inch** | Everything at 22 kHz or below, heavier tape saturation, voice caps tightened (the last-resort guard tier) |

The firmware drops a tier automatically under load and shows it. The user can also lock a tier as a sound.
