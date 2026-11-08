# Dubwise FM — Prioritized Technique Roadmap (v2.1, 2026-11-08)

Companion docs in this folder:
- `design-principles.md`
- `interface-spec.md`
- `prior-art.md`
- `mastery-guide.md`: technique refs C1–C13, D1–D3, B5

## Decisions to date
- **Open source from day one.** The app is GPL-3.0-only, in a public repo.
- **Platform: `charlesvestal/fm1-base` (MPL-2.0) as the `base/` submodule, with `hugelton/felucca-bsp` (MPL-2.0) pinned inside it.**
  - Our app sees only `plat.h`.
  - fm1-base is beta (`plat.h` and `fm1.toml` may change before 1.0), so pin the submodule and update deliberately.
- **No radio.** Set `ble = false` (and no WiFi). The radio stacks fill most of the FM-1's RAM, which we need for the dub bus.
- **User input, not generative.** The mixer is the instrument. Fidelity can be traded in period style.
- **Workflow:** Claude Code (web) for code, host-simulator tests and WAV renders. Local machine (WSL2 on Windows) for device builds, flashing and listening. Device builds need `pkgman.jieliapp.com` (toolchain) and gitee (AC79 SDK files).

## Hardware budget (measured by X0X/DUB-1 on real units)
| Resource | Figure | Planning rule |
|---|---|---|
| CPU | AC79 dual-core at **360 MHz** (no overclock headroom), single-precision FPU, ~8160 cycles/sample/core at 44.1 kHz | Floats only, no doubles. Host is ~80× faster than the device |
| Second core | Worst case 115% → 68% in X0X; **beta** in felucca-bsp | Design the split in, but stay **playable on one core** |
| RAM | 96 KB .data/.bss + 344 KB pool; 8 KB interrupt stack | int16 buffers at reduced rate. No allocation in the ISR |
| Image | ~581 KB app slot | Keep hot DSP code small (32 KB I-cache, code runs from flash) |
| Flash data | Data area + `PLAT_XDATA` windows (fm1-base) | A/B objects for project, song and motion. Samples as ADPCM or mu-law |
| Audio I/O | Stereo out only (I2S, 44,117.6 Hz); USB audio in from a host | No analog input |

### Memory plan (initial reservations)
| Item | Reservation |
|---|---|
| Tape echo, one line sized for 2T, 11 kHz int16 | ~35 KB |
| In-loop all-pass diffusers | ~2 KB |
| Spring reverb | ~8–16 KB |
| Phaser state | <1 KB |
| Sequencer, song, motion lanes | ~8–12 KB |
| Scenes (16 × ~60 B) | ~1 KB |
| Command queue (≥256) | ~2–4 KB |
| USB live-in ring buffer (P2) | ~2–4 KB |

## Engine architecture
1. **Block pipeline:**
   - Core 0: command queue → sequencer and performance events → timestamped event list.
   - Core 1 renders the drums while core 0 renders bass, harmony and live-in.
   - After the join, core 0 runs the dub bus (echo, spring, stepped HPF, phaser) and the master.
   - Mixing order is fixed, so output is bit-identical on one core.
2. **Mono dry path.** Stereo only on returns.
3. **Rates:** hats and rim at 44.1 kHz; tonal voices at 22.05 kHz; echo at 11 kHz; spring at 11–22 kHz; control rate every 16–32 samples.
4. **Guard tiers** Studio → Four-track → Seven-inch: automatic step-down plus user lock. PERF page.
5. **Plumbing:** one parameter ID space, a queue with overflow resend, per-module modulation state, and an ISR that never touches flash, allocates or waits.

### Plumbing as built (2026-11-08, `app/engine/`)
- **Parameter IDs** (`params.h`): 16-bit, fixed ranges, append-only (scenes and songs store IDs). `0x000` global,
  `0x040` channel strips (8 × 16 fields), `0x0C0` bus modules (echo, spring, HPF, phaser, master), `0x100` voices
  (8 × 32 fields). Values are int16 (every range fits). Commands carry an opcode (`CMD_SET`), separate from IDs.
- **Queue** (`cmdq.h`): 256-entry lock-free SPSC ring, main loop → audio interrupt, compile-time size check. Drained
  whole at the start of each block, so UI input lands within one block (≤ 5.8 ms). Sequencer events will carry
  sample offsets instead (P0.6).
- **Overflow** (`control.c`): a refused set marks the parameter pending; `ctl_flush` (every frame) sends its *latest*
  value. While anything is pending, new sets join the pending set (ID order), so an old value never lands after a
  newer one. Host test: 291 sets at once, final values applied.
- **Desk** (`engine.c`): 8 mono strips mixed in fixed channel order (bit-identical on one or two cores). Each strip
  owns its gain ramps (2.9 ms linear): no clicks on mute, solo or level.
  - **Solo** is additive. A soloed channel that is muted stays muted. Solo-muted behaves as muted.
  - **Sends** to echo and spring are post-fader and post-mute. Throws (P0.2) will be pre-fader.
  - Fader law: gain (v/100)², 100 = unity, 127 = +4 dB. Send law: (v/127)².
- **Cost** (device link, guard tier: all, it is the floor): `.data+.bss` +8,744 B (two 1 KB value tables, the 2 KB
  queue, four 1 KB block buffers), image +1,980 B. CPU: ~0.07 % of a block on the host for 8 active channels with
  sends, so roughly 5 % of one core on the device by the ×80 rule (an upper bound; constant-gain fast path not
  reflected on the host). To be measured on the device with P0.7's per-part profiling.

---

## P0 — Foundation

**P0.0 status (2026-11-08):** host tests pass; Bluetooth removed; `id_prefix` = `FM-1_382` (first `FM-1_57`, but thefirmwarehub.com's catalog has spellbox on `FM-1_57\d`; 382 is "DUB" on a keypad, and a release's 9 digits match no identity on the hub). Listing on the hub (thefirmwarehub.com/submit): `fmhub.json` in the root, a GitHub release with the `.fwsc`, tested on a real FM-1 (from V15 and from another firmware, and removable), then a mail to the hub. Device link
(JieLi toolchain 20260730.1, AC79 SDK V1.2.1), template with Bluetooth → without:

| | With BLE | No radio | Limit |
|---|---|---|---|
| Image | 225,716 B | 87,492 B | ~581 KB slot |
| RAM code `.ram_text` | 9,164 B | 4,964 B | 32,768 B |
| RAM `.data+.bss` | 76,420 B | 27,828 B | 98,304 B |
| Pool | 93,960 B | 69,384 B | 344,064 B (8 KB kept spare) |

So the dub bus starts with ~70 KB of `.data+.bss` and ~266 KB of pool free.
| # | Feature | Ref | Build notes |
|---|---|---|---|
| 0.0 | **Repo from the fm1-base template** | — | Template app runs in the host simulator and emulator. Strip Bluetooth. Claim an `id_prefix`. CI runs the host tests |
| 0.1 | Quantised mutes + solo | C1, spec 4.1 | Launch quantise Beat/1/2/4/8/16/32. Double-tap solo (additive) |
| 0.2 | Momentary throws with per-channel destination | C4, C10, spec 4.1 | Echo / Spring / Both. Hold or latch. Pre-fader. Lifts loop gain. **Done (2026-11-08):** `CHF_THROW`, `CHF_THROW_DEST` (per-channel defaults, spec 3); a throw adds a full pre-fader, pre-mute send onto the bus(es) through the strip's send ramps (no extra per-sample cost); an echo throw lifts the loop halfway to unity (cap 0.95) and brings in the 2T head at ≥ 0.35. White 9–16 hold / double-tap latch; hold + K1 sets the destination (until the MIXER page). Not quantised: a throw catches a word. Cost: ~0 CPU, 16 B RAM |
| 0.3 | Dub tape echo | C4, B5 | 11 kHz int16, two heads (T, 2T), T60 feedback + capped throw gain, in-loop band-pass + 5 all-passes + saturation, return limiter, time slew **Done (2026-11-08)**, `app/engine/echo.c`: 18,432-sample int16 line in the pool (T ≤ 0.81 s; at slow tempos the longest division that fits); loop HP ~80 Hz + tone LP + five short all-passes (7/13/23/37/53, g 0.3; DUB-1's 37–441 at g 0.5 delayed a bass repeat ~270 ms a pass, measured 657 ms for 600, so they were shortened and their delay taken off the loop's read: 601 ms now) + tanh; outputs read the raw line (first repeat clean); push g = 1.15 (the loop filters lose ~5 % at best), freeze loops the tape unfiltered; return limiter −6 dBFS; 180 ms time slew. Denormal-flushed (a decayed loop cost 30× on the host). Cost: pool +38 KB (line + all-pass state), CPU ≈ 3 % of a device block (all tiers). Tests: repeat time measured in the audio (605.6 ms for 600), 20 s of push bounded, freeze holds, limiter acts |
| 0.4 | Riddim templates + choke | D1 | One drop, rockers, steppers, flyers, as editable steps. **Done (2026-11-08):** `app/engine/riddim.c` (templates as D1 grids) and the sequencer in `engine.c`: 8 × 64 sixteenth steps (level off/ghost/normal/accent, note), per-channel length; steps fire on their sample (the render goes event to event: the next sixteenth or cue, computed, no per-sample stepping); bass released after a step (staccato). Templates are ordinary steps, cued to the bar line. STEPS page (`app/steps.c`); patterns in `OBJ_PATTERN`. Tests: step 9 at sample 70,560 (75 BPM) and 69,632 (76 BPM); a 512-step reload through the queue's resend. Swing, probability and REC keyboard entry are still to do |
| 0.5 | Core voices | D3, C11 | FM bass, kick, rim, hats. **Done (2026-11-08)**, `app/engine/voices.c`: kick (swept sine + 1:1 click burst), rim (FM 1:1.41 + 1:3.5, noise burst, HP 300 Hz), hats (4 squares 1/1.48/2.27/3.6 + noise, HP 7 kHz, closed chokes open), bass (2-op FM 1:1 / 2:1, index decay to near-sine, sub, sustain, 2× LP). All at 44.1 kHz for now: the four together and the echo measure ~12 % of a device block (×80), so the planned 22 kHz voices are kept as a guard-tier lever, not needed yet. MIDI: ch 10 GM drums, others the bass. SOUND view on EDIT; sounds in `OBJ_SOUND`. Tests: pitch by zero crossings (kick 48 Hz for 50, bass 54.7 for 55), highs share, DC, mono low end |
| 0.6 | Phrase counter, cycle bar and runway | spec 5.4 | Shared launch quantiser. Countdown in the last 4 bars, beat pulses in the last bar, "ONE" flash. Cued actions land on the boundary **Done (2026-11-08):** header cycle bar + n/len, amber on the runway; big countdown numeral; last-bar border pulses (and optional key flash); green ONE flash and border; reader counts; quantise CYCLE (cues land on the one: tested at sample 1,128,519 for an 8-bar cycle); optional audible tick (engine blip on runway beats). SETUP page (GLO, `app/setup.c`): cycle, runway, keys, numerals, tick, reduced motion, cue lead, test tones. Cost: UI only, plus a blip generator (~0 CPU) |
| 0.7 | Host scenario tests + WAV analysis + per-part CPU profiling | — | **Done (2026-11-08).** `tests/wavstat.py` (peak, RMS, DC, click, low-band side, head/tail/growth) from `#wav` lines in scenarios; engine profiler on `plat_ticks24` (per channel, mix, echo, spring, out; avg and max), `perf` command, `perf_pct` bound. The profiler runs on the device too (for the PERF page, 1.9) |
| 0.8 | **Key / scale / chord lock** | spec 4.5 | Off / Scale / Chord. Out-of-set keys are silent, never remapped. Reggae-oriented scale list. Key LEDs show the allowed set. HOME + ARP toggles. An input filter only. UI cost only **Done (2026-11-08):** 9 scales, 7 chord qualities (bit tables checked against the interval lists), a 16-bar chord lane (default Am Dm Am Em) that the lock follows bar by bar; KEY & CHORDS page (`app/keys.c`), HOME + ARP, LOCK / CHD chip; the keyboard (REC on the bass's STEPS page: record at the nearest sixteenth playing, cursor entry stopped); K2 step notes skip disallowed ones; LOCK MIDI IN (SETUP) filters MIDI in the audio interrupt. The chord lane is ready for the skank and organ (1.6) |

**P0.1 status (2026-11-08):** built and host-tested (`app/dub.c`, the transport and cues in `app/engine/engine.c`;
decisions in interface-spec 4.1.1). The transport is an exact integer beat clock (a beat = 26,460,000 / (BPM × 10)
samples, no drift); a cue splits the audio block and lands on its sample (`app/tests/quantise.txt` checks exact
samples at 75, 76 and 180 BPM). Cost: `.data+.bss` +968 B (32-entry cue list, a quantum per pending parameter),
image +6.2 KB (the page); CPU only while a cue waits (the transport stepped per sample). Device check still to do.

**Exit test:** drills C1 and C4 drills 1–2 on the device, plus a first-timer runway test (can someone land a drop on bar 17 without counting?).

## P1 — Signature tone
| # | Feature | Ref | Notes |
|---|---|---|---|
| 1.1 | Stepped Big Knob HPF | C3 | 3rd-order, 10 steps + off, no smoothing; on master and echo return. **Done (2026-11-08)**, `app/engine/bigknob.c`: TPT one-pole + SVF (Q 1) high-pass = 3rd-order Butterworth, coefficients jump per detent (stable); 70 Hz–7.5 kHz; on the mix or the echo return (SETUP). SELECT on any page but SETUP. Tested against \|H\| = r³/√(1+r⁶) (−0.3, −1.9, −39.5 dB at 110 Hz through 70, 100, 500 Hz). Cost ≈ 15 flops a sample, all tiers |
| 1.2 | Spring + crash | C5 | Black 1 crash. Held during a throw, it adds the spring. **Done (2026-11-08)**, `app/engine/spring.c`: two springs at 22.05 kHz, delay loops 37/43 ms through 12 stretched all-passes (z⁻², a 0.62: the dispersive chirp) and a damping LP (2–7 kHz), input HP ~200 Hz; DECAY 0.5–6 s (loop feedback; the damping shortens it in the highs); crash = 6 ms noise burst + thump into both. Mono return for now (stereo with P1.5). FX page (FX: ECHO, SPRING banks). Pool +8 KB; CPU ≈ 5.5 % of a device block while it sounds (×80), ~0.3 % idle: the spring and the echo go idle when nothing goes in and nothing is left in them (the echo only once its whole tape reads zero, so a gap between repeats never stops it) |
| 1.3 | Return cross-sends | B5 | Echo ↔ spring, loop-gain clamped |
| 1.4 | Tape-dive control | C4 drill 3 | |
| 1.5 | Echo Width | C8 | 8–16 ms right offset |
| 1.6 | Skank + organ bubble | D2, D3 | Follow the chord lane |
| 1.7 | Scenes: desk or desk + patterns | C2, spec 4.2 | Scope chosen at store time, P badge |
| 1.8 | Song per-bar list + live capture + section markers | spec 4.2 | The runway follows section boundaries in song mode |
| 1.9 | Guard tiers v1 + PERF page | principles §5 | |

**Exit test:** drills C3 and C5, and a Tubby-style emulation.

## P2 — Colour, lead voices, live input
| # | Feature | Ref | Notes |
|---|---|---|---|
| 2.1 | Siren | D3, C9 | |
| 2.2 | Melodica, brass | D2, D3 | |
| 2.3 | Single phaser | C6 | |
| 2.4 | Tape drive | C7 | |
| 2.5 | Snare-keyed spring gate | C13 | |
| 2.6 | Vox/FX one-shots from flash | C9, C10 | |
| 2.7 | Motion recording | — | |
| 2.8 | Live in (USB audio) on ch 8 | spec §8 | Tap USB audio-in before the post-app mix (fm1-base change, upstream MPL-2.0). Mono, mutable and throwable |
| 2.9 | Velocity-scaled throws (`APP_TOUCH`) | spec 4.1 | Once velocity is proven reliable |

## P3 — Wishlist
| # | Feature | Notes |
|---|---|---|
| 3.1 | Ping-pong echo | Cheap version on the existing 2T heads; full version with two cross-fed lines |
| 3.2 | Dual Bi-Phase | |
| 3.3 | Reverse samples | |
| 3.4 | Dub-techno mode | |
| 3.5 | Spin-back / rewind | |

## Open questions
1. Panel and black-key groupings, and velocity reliability, on the device.
2. Real device CPU cost of 22 kHz voices with upsampling.
3. Whether USB audio input reaches the app cleanly (latency and jitter) on the device.
4. How to track fm1-base beta changes: pin the submodule and review `plat.h` diffs on each bump.
