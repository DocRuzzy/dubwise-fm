# Survey: the FM-1 firmwares on fm1-emu (2026-11-10)

The owner asked for every firmware on [fm1-emu](https://github.com/joakimeriksson/fm1-emu) (158 at the time) to be
read for anything worth borrowing. The list was triaged first. About 25 touch what Dubwise does; four readers took them
by theme. The rest are generative instruments, games, trackers, ambient machines or other synths with nothing for a
user-driven dub desk.

Everything here was read, not built or run. Costs are estimates by inspection. What was adopted is roadmap 6.x, where
the costs were measured.

**Licences.** GPL-3.0 (most of them) and MPL-2.0 (loom, fizz, j6 and others) can be borrowed with credit in the file
header and CREDITS.md. Avoid:
- luxr's LXR transient samples: non-commercial, not GPL.
- fomajam's ROM kits: recordings of commercial machines.
- x0xrmx's Remix table: transcribed from an Orb; its own docs call the licensing unresolved.
- clatter: it carries section-7 additional terms.

**Not in the catalog:** Dubwise itself. A listing would let anyone try it in a browser.

## The B-3 organs (b3organ, b1)

Covered in roadmap 4.13. From **setBfree-fm1** ("B3 ORGAN", R-Jeb), whose figures are measured against setBfree:
- the scanner, the percussion, the rotors' speeds and pace, and the key click;
- the preamp (the overdrive) and the whirl (the FULL Leslie).

**B1** (SikkenWorks) added nothing setBfree-fm1 didn't do better.

## Echo, spring and dub

- **dub** (DUB-1, sbegueria). The echo's main ideas are already ours (prior-art.md). New:
  - the wow on the read (taken: 6.3);
  - the throw's release tail (taken: 6.4);
  - a strike ducking the echo returns except while a throw rings (`ec_dk`): an opt-in idea for later.
- **bb1.** Its echo is a full-rate float line (128 KB), with no all-passes, limiter or ping-pong. Its spring is
  Felucca's, single-loop. Both are weaker than ours. A head-select mode (T/3, 2T/3) is the only idea.
- **demake.** The best wow and flutter (`media_block`: the wow's rate wanders): taken, 6.3. Its spring is weaker.
- **daurade.** **BASS+** for the FM-1's speaker, Felucca's `spk_bass`: taken, 6.2. Its pump is an EDM sidechain:
  off-idiom.
- **loom.** Its "throws" are sequencer punch-ins (stutter, repeat), not sends into an echo. Its delay is plain. Nothing
  better than ours.
- **waveloop.** Momentary master effects. A tape stop could reuse our spin ring: an idea for later. Its DJ SVF is no
  better than the Big Knob.

## Drums

Nothing beats our voices once cost is counted. The circuit-model 808 voices (x0xrmx) cost 2–4× ours for differences
barely heard in a dub mix. Worth having:
- **Trig conditions:** 1ST, A:B, PRE (bigorna `tracks.c:seq_cond_ok`; fizz `cond_ok`, MPL-2.0). Planned: 6.7.
- **FILL / !FILL while a key is held** (luxr `lx_seq.c:step_trig`): a performance gesture. Part of 6.7.
- **A HOLD before the kick's decay** (bigorna `fmd.c`), optionally tribe's curve morph (`kicks.c:shape_at`). A long
  one-drop kick keeps its body. Later.
- **Procedural transients** for the kick's beater and the cross-stick (bigorna `fmd.c:fd_transient`: tick, snap,
  thump). Later.

## Bass, keys, chorus

- **kwasub, rupture.** The sub after the filter, and the "classic" odd-harmonic sub: taken, 6.1. kwasub's sub-harmonic
  exciter is close to BASS+.
- **monohull.** ADAA on a cubic clip (`clip3_adaa`) and resonance bass compensation. Candidates for the bass's DRIVE
  and a better filter later. Its filters are too heavy for a half-rate voice.
- **j6** (MPL-2.0). A ZDF 4-pole ladder, ~0.3–0.4 % at the half rate: a possible 24 dB/oct bass filter.
- **shineon.** A tri-phase string ensemble whose mono mode sums the taps: chorus colour on a mono dry path, for the
  melodica, the skank or the bubble.
- **fm106.** The Juno MN3009 chorus, for a stereo return. Check KR-106's licence before copying.
- **frodhes.** An amp-law tremolo (cheap) for the skank or organ. Its Rhodes is ~10× the skank's cost.
- **fmel.** Plays tapes from flash, which breaks the render rule. Nothing to take.

## Voice and workflow

- **hortator-bt.** `smc.c` drives the M-VAVE SMC-Mixer: faders, mute and solo, buttons, LEDs. It should work over our
  USB host: planned, 6.5.
- **spellbox.** An LPC speech chip at ~250–300 B a second of speech, played in key: a deejay voice on VOX, planned 6.6.
  It sounds robotic, Speak & Spell-like.
- **monk.** Formant vowels only (no consonants): a chant pad, not a deejay.
- **386dx.** Full eSpeak needs the second core and ~200 KB. No.
- **pulhra, mnm1, melodee.** Little new beyond our locks, motion and live recording.
