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

---

## P0 — Foundation

**P0.0 status (2026-11-08):** host tests pass; Bluetooth removed. Still open: claim an `id_prefix`. Device link
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
| 0.2 | Momentary throws with per-channel destination | C4, C10, spec 4.1 | Echo / Spring / Both. Hold or latch. Pre-fader. Lifts loop gain |
| 0.3 | Dub tape echo | C4, B5 | 11 kHz int16, two heads (T, 2T), T60 feedback + capped throw gain, in-loop band-pass + 5 all-passes + saturation, return limiter, time slew |
| 0.4 | Riddim templates + choke | D1 | One drop, rockers, steppers, flyers, as editable steps |
| 0.5 | Core voices | D3, C11 | FM bass, kick, rim, hats |
| 0.6 | Phrase counter, cycle bar and runway | spec 5.4 | Shared launch quantiser. Countdown in the last 4 bars, beat pulses in the last bar, "ONE" flash. Cued actions land on the boundary |
| 0.7 | Host scenario tests + WAV analysis + per-part CPU profiling | — | |
| 0.8 | **Key / scale / chord lock** | spec 4.5 | Off / Scale / Chord. Out-of-set keys are silent, never remapped. Reggae-oriented scale list. Key LEDs show the allowed set. HOME + ARP toggles. An input filter only. UI cost only |

**Exit test:** drills C1 and C4 drills 1–2 on the device, plus a first-timer runway test (can someone land a drop on bar 17 without counting?).

## P1 — Signature tone
| # | Feature | Ref | Notes |
|---|---|---|---|
| 1.1 | Stepped Big Knob HPF | C3 | 3rd-order, 10 steps + off, no smoothing; on master and echo return |
| 1.2 | Spring + crash | C5 | Black 1 crash. Held during a throw, it adds the spring |
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
