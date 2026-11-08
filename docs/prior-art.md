# Prior art: FM-1 alt firmwares — lessons for Dubwise FM

Reviewed 2026-11-08. Sources: `sbegueria/fm1-dub` (DUB-1, 501c4cf) and `charlesvestal/fm1-x0x` (X0X, `docs/plans/*.md`), both GPL-3.0-only. Also `charlesvestal/fm1-base` and `hugelton/felucca-bsp` (MPL-2.0), which we build on.

We take lessons from DUB-1 and X0X, not their app layers. DUB-1 is generative dub techno; X0X is a 909/808/303 groovebox.

## Hardware facts (measured by these projects)
| Item | Value |
|---|---|
| Chip | JieLi AC79 (AC791N), dual-core pi32v2, single-precision FPU, 32 KB I-cache, 32 KB D-cache |
| Clock | 360 MHz, measured; already above the SDK's 320 MHz table, so no overclocking |
| Budget | ~8160 cycles/sample/core at 44.1 kHz |
| RAM | 96 KB .data/.bss + 344 KB pool; the audio ISR runs on an 8 KB system stack |
| App image | ~581 KB slot |
| Host vs device | Host is ~80× faster |
| Doubles | The build fails if soft-double code is linked |

## Problems → solutions
1. **One core wasn't enough** (X0X worst case 115%). The second core brought it to 68%. Per block: the sequencer writes an event list, the cores render disjoint voice groups, then mixing happens in a fixed order (bit-exact with one core).
2. **Core 1 is fragile.**
   - Start it at power-on, opening PC limits during ROM boot.
   - It runs on `usp` after `rti`, so give that a real stack.
   - Its wait loop must be in RAM.
   - Some units misread RAM: use a per-job check word and fall back to one core until power-off.
3. **The split was imbalanced at first.** Rebalance by measured per-part cost; pick the split per block.
4. **Overload must not mean dropout.** Use a guard that drops oversampling and shortens tails, plus a PERF page with per-part CPU.
5. **Free savings:**
   - skip silent parts
   - stop decoding muted samples
   - compute compressor gain every 4 samples
   - no per-sample divides
   - end tails at −60 dB
   - quarter-rate (11 kHz int16) echo and grain lines
6. **Code runs from flash through a 32 KB I-cache.** Keep hot DSP small and put the ISR entry in RAM.
7. **Memory:** reduced-rate int16 buffers; samples in flash as IMA ADPCM (~44 KB/s at 22 kHz); A/B flash objects. The ISR never touches flash, allocates or waits.
8. **The UI→engine queue overflowed** (64 entries, which dropped PLAY). It's now 256 with a compile-time check and resend.
9. **Parameter IDs collided with UI codes** past 100 parameters. Design the ID space up front.
10. **Shared modulation state between modules caused bugs.** Keep each module's state separate.
11. **X0X song** = per-bar entries `{pat[5], mute}`, 192 bars; Song+REC writes live cues and mutes. Pattern cues land at the bar. Motion lanes record per step with interpolation, and the knob overrides the lane while turning.
12. **Process:** a host simulator with scenario scripts (input in; WAV and screenshots out), a WAV analyser, bit-exact refactors, a browser emulator, a web installer, and a safe mode after two failed boots.

## DSP techniques worth reusing (DUB-1 `amb.c`, GPL: credit if adapted)
- Quarter-rate int16 tape line, interpolated reads, wow, time slew, freeze, tape stop.
- Feedback from T60: g = 2^(−9.966·len/T60); throws add gain up to a cap.
- In-loop band-pass and five all-passes (37/89/151/276/441), so each repeat comes back darker while the first stays clean.
- A second head active only during throws. Per-part throws. Bipolar LP/HP channel filter. Kick ducking. Sirens into the echo.
- TPT SVF filters. A 4-op FM core with 8 algorithms.
