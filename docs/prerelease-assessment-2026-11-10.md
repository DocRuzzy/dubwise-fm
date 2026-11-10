# Dubwise FM: prerelease assessment (2026-11-10)

Reviewed: `DocRuzzy/dubwise-fm` at `81d69a4` (main, merge of PR 13), with `base/` at its pin. Project-only doc, not a mirror of a repo file.

What was done: read the docs and the engine (`engine.c`, `echo.c`, `cmdq.h`, `control.h`, `params.h`), skimmed the UI and the manual, built the host simulator, ran `tests/run_tests.sh` (77 scenarios, all pass, 48 s), compiled the engine units with warnings on, and ran four small experiments of my own (below). Nothing was run on an FM-1. The UI pages, the voices and the storage converters were skimmed, not audited.

## Bottom line

The design is the strongest thing here, and it is original: no other FM-1 firmware treats the mixing desk as the instrument. The engineering foundations are better than most hobby firmware. The risk is not quality of ideas, it is breadth: about 21,800 lines and roughly 100 roadmap items were built in three days (8 to 10 November), every CPU figure is an estimate, and no line of it has run on hardware. Reviewers of FM-1 firmwares (the roundup in `roundup-review-2026-11.md`) mark down bugs by a whole tier and say outright that a thousand features count for little if they aren't implemented right. The next gain comes from freezing features and measuring, not from adding.

## 1. Findings to fix before a release

| # | Finding | Evidence | Fix |
|---|---|---|---|
| 1 | **The echo never goes idle after its first input.** A dangling `else` in `echo_run` (`app/engine/echo.c` ~line 278) binds to the inner `if`, so `quiet` is reset to 0 on every silent line sample and never reaches `IDLE_AFTER`. `echo_settle()` is unreachable from this path. | gcc `-Wdangling-else` flags it. Harness: an impulse, then 60 s of silence: `quiet` = 0, output still non-zero at 59.9 s. With braces added: idle at 2.8 s. | Braces around the inner `if`. Add a test: `eng` echo part cost returns to ~0 after a throw decays. Costs a few percent of a block for the rest of every session until fixed. |
| 2 | **The engine is compiled with all warnings off in the only build the dev loop runs.** fm1-base's host and web builds pass `-w` for the `o2` units, which is every file in `app/engine/`. | `base/tools/build.py` lines 1314, 1440. Finding 1 lived behind it. | A CI step in our repo: `cc -fsyntax-only -Wall -Wextra` over `app/engine/*.c` (control.c is part of the unity build, skip it), failing on `-Wdangling-else`, `-Wcomment`, `-Wparentheses`. |
| 3 | **Comment rot in `engine.c` and `engine.h`.** Six `"/*" within comment` warnings; orphaned trailing comments at lines 126, 130, 136; an unterminated comment at 202 that swallows the next one; comments attached to the wrong declarations in `engine.h`. Harmless today, but an unterminated comment can silently swallow a declaration. | `-Wcomment` output. | One cleanup PR; then finding 2's CI step keeps it clean. |
| 4 | **Studio "everything on" has outgrown the headroom rule.** The 58 % figure in `review-2026-11.md` predates blocks 5 to 8. A heavy scene now (B-3 with FULL Leslie, 808 models, MCI desk, cascaded phaser on the mix, ping-pong, 3T head, wow, mix drive, cross-sends, long spring, EQ and pan on every strip, a siren, two throws) measures about 3.2 times the default desk. | My sandbox: 110 % est. against 35 % for the default desk. This machine is 1.5 to 1.8 times slower than the roadmap's reference (spring: 23 to 27 % here, 14.7 % in roadmap 7.2), so by the roadmap's own ruler the heavy scene is roughly 60 to 70 %. The rule says keep the average under 60 %. | Expected behaviour is that the guard steps down, so this is not a dropout. But "Studio" may be a tier players rarely stay in. Decide after the device reading (finding 5). The spring is the first place to look: it is about two thirds of the default desk's cost. |
| 5 | **The ×80 rule is not portable.** `perf_pct` is host microseconds times 80, so the figure depends on which machine ran it (1.5 to 1.8 times apart between my sandbox and the reference). Only one scenario bounds CPU at all (`mixer.txt`, `< 80`). | As above. | Until a device reading exists, test ratios, not absolutes: each scene's cost against a fixed reference scene in the same run. After the reading, replace 80 with the measured factor per part (all-pass chains and table lookups will not scale the same; the 32 KB I-cache matters once the hot code outgrows it). |
| 6 | **No final limiter or clamp on the master.** `render_seg` ends in `(int32_t)(y * gain_now)`. The returns are limited to −6 dBFS each, the dry sum isn't. | Every fader at 127, push, crash, a siren, MASTER full: peak 0.0 dBFS and a clipped sample in the simulator, which clamps. What the device's DAC path does above full scale isn't something I could verify from the source. The USB recording path renders at full gain always. | A soft clip (the tape curve already exists) or a hard clamp at the last stage. Small cost, and it belongs to the "never compromise" list (feedback and loudness safety). |
| 7 | **The echo's decimator aliases.** Two one-poles at ~4 kHz ahead of a 4:1 drop to 11 kHz. | Harness, sine at −12 dBFS in, first repeat out: 1 kHz comes back at −14 dB; 9 kHz folds to 2 kHz at −31 dB; 10 kHz folds to 1 kHz at −29 dB. So a hat or snare-noise throw carries inharmonic fold-back only 15 to 17 dB under the wanted signal. | The spring had the same fault and got a half-band FIR (roadmap 1.2, "crushed"). Two cascaded half-bands on the echo's input would do the same job. It's a judgment call: some of this reads as grit. Listen on hat throws before deciding. |

## 2. The project as a whole

**Strong.**
- The principles are short, real, and enforced. "Driven by user input, not generative" and "the mixer is the instrument" cut scope in the right places, and the clarified rule on chance (fired, seeded, stored, repeatable) is a good piece of design.
- Research feeds the build. The mastery guide, the prior-art notes and the two surveys turn into specific items with sources, and the fidelity policy explains each economy in period terms.
- The delivery kit is unusually complete for a prerelease: browser emulator, web installer, web patch editor, backup, a 518-line manual, help cards, a screen reader path, a release checklist, both flash chips handled.

**Weak.**
- Zero hardware time. Stack depth, cache behaviour, key feel, velocity curves, the speaker, and whether a throw feels immediate at a 5.8 ms block are all unknown.
- Pace has outrun verification. Tests check that each feature does what it was built to do. They can't tell you whether the instrument is playable, and nobody has played it.
- Docs are drifting under their own weight. `CLAUDE.md` still says "Current state (P0.0)". The README says 72 scenarios (77 run). `dub.c`'s header describes the P0.1 page and lists the runway and throw velocity as "not here yet". `roadmap.md` has become a changelog with table cells over a thousand characters. Since `docs/` is the source of truth and the agent's context, drift there costs more than it would in an ordinary repo.

## 3. Functionality against what users may want

**What fits.** The core loop is right and simple: white 1 to 8 mute, white 9 to 16 throw, black keys for the gestures, a knob per return, the Big Knob, and a runway that counts you to the one. That loop is the product. It is also the part a child can play.

**Where it may miss.**
- *Depth behind the loop is heavy.* The manual has 21 HOME combinations, hold-key-plus-knob layers on every channel, tap, hold and double-tap on the same key, and settings reached by tapping a key three times. That sits uneasily with "sparing shift functions" and "accessible, intuitive and fun". Nothing needs removing; the first ten minutes need protecting. A SIMPLE setting that turns the DUB page into mutes, throws, four knobs and the Big Knob, with everything else asleep, would serve the kid and the first-time reviewer equally.
- *The audience is narrow by design.* Five reggae riddims and a Kingston band. The dub-techno player, who is DUB-1's audience, gets the desk but not the palette. That's a defensible choice. A neutral template or two (a four-on-the-floor with chord stabs) would widen the door at almost no cost and without touching principle 1.
- *No radio.* Right for RAM. Some FM-1 owners expect Bluetooth MIDI; say so plainly on the listing.
- *The speaker.* BASS+ is the right answer and should probably be chosen automatically when no jack is in (the simulator has a `jack` command, so the platform knows).
- *Live input waits on upstream.* A dub desk with nothing to dub but its own band is half the idiom. If the fm1-base change stalls, user one-shots and the stems carry more weight; say what works today.

**What users will value most, in my reading:** the runway, throws with velocity, the kickable spring, songs that keep the moves, and the stems over USB. Lead with those; the B-3, the LPC deejay and the model count are garnish.

## 4. Originality and value against other FM-1 firmwares

Not independently verified firmware by firmware: this rests on the repo's own surveys (158 firmwares on fm1-emu triaged, about 25 read) plus a web check on 10 November.

- **Original:** the desk as the instrument; quantised mutes with a visible countdown; pre-fader, pre-mute throws that lift the loop; the stepped Altec-style filter with unsmoothed clicks; a strikeable spring; cross-sends with a derived stability cap; motion captured sample-exactly and replayed bit-for-bit; guard tiers presented as period character. None of the surveyed firmwares does these as a set, and several do none.
- **Borrowed, with credit:** platform (fm1-base, felucca-bsp), Plaits drums, setBfree, STK, Airwindows, Parker's spring, the LPC voice. The licensing discipline (per-file headers, CREDITS, measured numbers only from recordings) is better than typical.
- **Nearest neighbour: DUB-1, and it is moving toward you.** `prior-art.md` reviews commit `501c4cf` and calls it generative dub techno. Its releases since describe 1.2.0 as "made for playing live", with a DJ filter on the main page and master actions, and 1.4.0 as "the desk and the set", adding songs. It still plays itself, which is the roundup reviewer's complaint about it ("doesn't feel like it is yours"). So the difference that matters is principle 1, not the feature list. Refresh `prior-art.md` against DUB-1 1.4 before the listing copy is written.
- **Value.** On a device this cheap, in a field of general grooveboxes (X0X, LOOM-8, SLOOP, FM1 Open, and the paid Groove OS), Dubwise is the only one that teaches a specific performance practice. The mastery guide plus the firmware is the value proposition; neither alone is. That pairing is also what makes it worth a reviewer's time.

Sources for the DUB-1 and field claims: https://newreleases.io/project/github/sbegueria/fm1-dub/release/v1.2.0 , https://newreleases.io/project/github/sbegueria/fm1-dub/release/v1.4.0 , https://www.matrixsynth.com/2026/11/fm-1-dub-1.html , https://synthanatomy.com/2026/10/groove-os-turns-the-m-vave-fm-1-into-an-8-track-groovebox.html

## 5. Code structure and choices

**Good choices.**
- The app sees only `plat.h`; the platform is a pinned submodule that is never edited, with changes proposed upstream.
- One append-only parameter ID space with compile-time range checks; mutes, sends and throws are parameters, so scenes, song, motion, MIDI learn and undo all get them for free. This is the decision that made the feature count possible.
- A lock-free SPSC queue with "pending, send the latest" overflow handling, instead of dropping or blocking.
- An exact integer transport, and a render that walks from event to event so cues, steps and motion land on their sample. Tested to the sample.
- Bit-exact replay as a test invariant. It forces determinism everywhere (wow phase from PLAY, idle settle to exact zero) and catches a whole class of bug.
- Idle paths, denormal handling, int16 reduced-rate buffers, stability caps derived rather than tuned.
- Stored-format discipline: a plan before code, converters, and tests that load old saves.
- Host scenario tests with WAV metrics. 77 of them is real coverage for firmware.

**Choices to revisit.**
- **`engine.c` is carrying too much:** 1,900 lines, about 60 file-scope statics, `render_seg` at 208 lines, and transport, cues, sequencer, song, motion glue, MIDI clock, spin-back, stems, guard and profiler in one unit. The effects are properly separate modules; the desk and the sequencer aren't. Splitting transport and sequencer from the mixer would make the second-core split (which the architecture section promises and the code doesn't have) tractable.
- **The second core is unused** while Studio sits at its limit. Either build the split (spring and echo are the obvious job, and are already behind clean `_run` calls) or strike it from the architecture section so the docs match.
- **Globals in `params.h` are being patched in**, out of order (0x035 declared before 0x034), with settings saved in "spare bytes" of other objects. It works and is tested, but it is format debt: the next few settings should get a versioned settings object rather than another spare byte.
- **The UI is one unity translation unit** (`app.c` includes the pages). Fine for size; it means every page shares every static, and the stale `dub.c` header shows nobody can hold it in view any more.
- **Tests assert construction, not limits.** Add per-scene CPU ratios (finding 5), an idle-after-decay check (finding 1), and a hot-mix peak check (finding 6).
- **Everything is machine-written at speed, and it shows in specific ways:** comments describing history rather than the code, orphaned comment fragments, headers that weren't updated. None is a design flaw. Together they say the review step is the bottleneck now.

## 6. Suggested order

1. Findings 1, 2, 3 and 6: small, certain, no hardware needed.
2. Feature freeze at v0.3.
3. Hardware: release-checklist steps 1 to 7, with the PERF page on the default desk, UK STEPPERS with every effect, and the heavy scene above. Log per-part figures and `plat_stack_least_ever`.
4. Recalibrate CPU from those readings; then decide finding 4 (spring cost, tiers) and the second core.
5. Play it, and have a first-timer play it. Then decide SIMPLE mode and finding 7 by ear.
6. Refresh `prior-art.md` for DUB-1 1.4, fix the stale docs, then list.
