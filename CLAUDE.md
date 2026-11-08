# Dubwise FM — notes for Claude Code

Dubwise FM is an open-source (GPL-3.0-only) firmware that turns the M-VAVE FM-1 into a reggae/dub groovebox. The design goal: the mixing desk is the instrument (mutes, throws, feedback delay, stepped "Big Knob" HPF, struck spring). Everything is driven by user input; nothing is generative.

## Read first
- `docs/design-principles.md`: governing rules and fidelity policy. Every change must comply.
- `docs/roadmap.md`: P0–P3 features (item numbers like 0.3, 1.1), hardware budget, memory plan, engine architecture.
- `docs/interface-spec.md`: controls, pages, screen layout, runway countdown, accessibility.
- `docs/prior-art.md`: measured hardware facts and lessons from X0X and DUB-1.
- `docs/mastery-guide.md`: dub technique references (C1–C13, D1–D3, B5) and drills.

## Layout
- `app/`: our firmware, started from fm1-base's `template/`. The app talks only to `plat.h`. Licensing is per file: files copied from the template keep their MPL-2.0 header (MPL is file-level, so modified copies stay MPL-2.0); new files are GPL-3.0-only with an SPDX header. The firmware as a whole is distributed under GPL-3.0-only.
- **Current state (P0.0):** `app/` is fm1-base's template renamed to DUBWISE, with the radio removed (`ble = false`, no `APP_BTDEV`/`OBJ_BLE`). Velocity and pressure (`APP_TOUCH`) are kept. Our `id_prefix` is `FM-1_57`.
- **Engine plumbing** is in `app/engine/` (GPL-3.0-only): `params.h` (the one ID space; append only), `cmdq.h` (UI→engine queue), `control.c` (main-loop side, overflow resend), `engine.c` (audio side: 8-strip desk, compiled at -O2). Host tests: `app/tests/mixer.txt` (`param`, `flood` commands; `eng.*`, `ui.*`, `chN_db`, `echo_db`, `q_*` checks).
- A SessionStart hook (`.claude/hooks/session-start.sh`) checks out `base/` when `base/tools/build.py` is missing.
- `base/`: fm1-base as a git submodule (MPL-2.0), pinned. It contains `bsp/` (felucca-bsp). **Don't edit files in the submodule.** If the platform needs a change, propose it as an upstream PR (MPL-2.0) and note it in `docs/roadmap.md`.
- `docs/`: design docs (source of truth; keep them in sync with decisions).

## Build and test
- Host simulator (no JieLi toolchain needed): `tests/run_tests.sh` (builds and runs all scenarios), or `./build.sh --host` and then run `app/build/host/dubwise_host app/tests/<script>.txt app/build/host`.
- Browser emulator: `./build.sh --web` (needs emscripten).
- Device package: `./build.sh` (needs the JieLi Linux toolchain via `base/tools/get_toolchain.sh` and the AC79 SDK files; see fm1-base's README). Normally built by CI or locally, not in the cloud sandbox.
- Prefer changes that can be checked in the host simulator: scenario scripts, WAV output, screenshots.

## Hard rules (firmware)
- AC79 dual-core at 360 MHz, single-precision FPU. **Floats only: no `double`, no double literals or libm double calls.** Use `1.0f`.
- RAM: 96 KB plus a 344 KB pool, and an 8 KB interrupt stack. Audio buffers are int16 at reduced rate (echo 11 kHz, spring 11–22 kHz, tonal voices 22.05 kHz).
- The audio render never touches flash, never allocates and never waits. The UI draws in the main loop with dirty rectangles only.
- Must stay playable on one core. The second core is optional acceleration. Respect the guard tiers (Studio / Four-track / Seven-inch).
- One parameter ID space. The UI→engine command queue holds ≥256 entries with overflow handling. Each module keeps its own modulation state.
- Mono dry path; stereo only on effect returns.
- No radio (`ble = false`): the RAM goes to the dub bus.
- Never compromise timing, dropout-free playback, a clean low end, or feedback-loop stability.

## Working style
- State CPU, RAM and flash cost, and the guard tier, for any new DSP feature. Profile on the host (~80× faster than the device).
- Reference roadmap item numbers in commits and PRs (e.g. "P0.3 tape echo: in-loop all-passes").
- Code that adapts GPL code from fm1-x0x or fm1-dub must credit it in the file header and in `CREDITS.md`.
- Update `docs/` when a decision changes.
