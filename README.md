# Dubwise FM

A reggae/dub groovebox firmware for the **M-VAVE FM-1**, where the mixing desk is the instrument: quantised mutes and solos, send throws into a feedback tape echo and a kickable spring reverb, a stepped "Big Knob" high-pass filter, riddim templates (one drop, rockers, steppers, flyers) and FM voices for bass, rim, hats, skank, organ bubble, melodica, horns and siren.

Everything is driven by the player: nothing is generative. A phrase counter and a countdown "runway" show when the next 8/16/32-bar boundary is coming, so drops land on the one.

A SpIllt Beats release, by DocRuzzy.

**Status: pre-alpha.** The P0 foundation is built and host-tested (desk, quantised mutes and solo, tape echo, throws, FM kick / rim / hats / bass, step sequencer with riddim templates, runway, key lock); P1 is in progress. Not yet tried on a real FM-1. See `docs/roadmap.md` and the manual in `app/docs/MANUAL.md`.

## Layout
| Path | What |
|---|---|
| `app/` | The firmware (started from fm1-base's template) |
| `base/` | [fm1-base](https://github.com/charlesvestal/fm1-base) as a pinned submodule (MPL-2.0), with [felucca-bsp](https://github.com/hugelton/felucca-bsp) inside |
| `docs/` | Design principles, roadmap, interface spec, prior-art notes |
| `CLAUDE.md` | Rules and context for AI-assisted development |

## Build
```
git clone --recursive https://github.com/DocRuzzy/dubwise-fm
tests/run_tests.sh        # host simulator + scenario tests (any Linux/macOS with a C compiler and Python 3 + Pillow)
./build.sh --web          # browser emulator (emscripten)
./build.sh                # device package app/build/dubwise.fwsc (JieLi Linux x86-64 toolchain + AC79 SDK files; see base/README.md)
python3 base/tools/fm1_install.py app/build/dubwise.fwsc
```
On Windows, use WSL2: the JieLi toolchain is Linux x86-64 only.

Installing custom firmware is at your own risk. The web installer and M-VAVE's updater can restore the stock firmware.

## Licence
GPL-3.0-only (see `LICENSE`). Files derived from fm1-base's template keep their MPL-2.0 headers, and fm1-base and felucca-bsp are MPL-2.0. See `CREDITS.md`.

M-VAVE and FM-1 are trademarks of their owners. Dubwise FM is not affiliated with M-VAVE, Hügelton Instruments or the fm1-base project.
