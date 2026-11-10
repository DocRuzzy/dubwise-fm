# Release checklist: The Firmware Hub (thefirmwarehub.com/submit, read 2026-11-08)

| # | Requirement | Status |
|---|---|---|
| 1 | Open source, the complete source public | **Done.** GPL-3.0-only (`LICENSE`), with MPL-2.0 files from fm1-base's template. The repository is public. fm1-base is a public submodule, so the full source is browsable at each tag |
| 2 | A GitHub release with the `.fwsc` attached | **Ready, not done.** `.github/workflows/release.yml` is enabled: pushing a tag `vX.Y` (e.g. `v0.1`) on `main` builds the package with the shared FM-1 builder (`zvenson/fm1-build@v1`), runs `tests/run_tests.sh`, signs the files with build provenance and publishes the release. This earns the hub's "Built from source on GitHub" label. Assets: `dubwise-X.Y.fwsc`, the app, the `.rfm1` and `firmware.json`. A `0.x` version is tagged beta on the hub |
| 3 | Our own identity | **Done.** `FM-1_382` (releases `FM-1_382XXYYZZ`); free on the hub when chosen |
| 4 | The update loader in the package, so players can uninstall | **Done.** The package carries felucca-bsp's update loader (`loader/ota.bin`, 11.8 KB; its marker is in the `.fwsc`). It doesn't check the flash ID, so it installs on the newer UC25HQ80 chip (JEDEC `0xB36014`) as well as the older one (`0x856014`). The platform's own check (`fm1_flash_ok.h`) accepts both. Simulated with fm1-base's installer test against our real `dubwise.fwsc`, all three passing (0 bad reads; the loader runs; the FM-1 comes back as `FM-1_38200`): from stock V15 on the older chip, from V15 on the newer chip, and from another firmware on the newer chip |
| 5 | Tested on a real FM-1 | **Owner's to do** (emulator results don't count): see below |
| 6 | `fmhub.json` in the root | **Done.** `what` (254 of 300 characters, plain text), `author`, `kinds: ["groove"]`, `image` (240×240 PNG, 173 KB), `color` `#F27848`. `play` points at the browser version, which `.github/workflows/pages.yml` builds and publishes from `main` (Settings → Pages → Source: GitHub Actions, once) |

## The hardware test (requirement 5)

1. On an FM-1 running M-VAVE's V15: install `dubwise.fwsc` from the hub's web installer (or
   `python3 base/tools/fm1_install.py app/build/dubwise.fwsc`). After the restart, the boot splash shows the SpIllt
   Beats mark and the USB name is "DUBWISE FM-1". Check the identity: `python3 base/tools/fm1_install.py --info`
   reports `FM-1_382…`.
2. Play: the demo song (SETUP → DEMO SONG), mutes, throws, SAVE, then power off and on: the save is still there.
3. Leave it: install V15 (or another firmware) over Dubwise and check `--info` shows the new identity.
4. From another firmware (e.g. a Felucca one): install Dubwise again, and check step 1's identity.
5. From two or three popular alternative firmwares besides Felucca (e.g. SLOOP, LOOM-8, X0X), install Dubwise and remove it again (`roundup-review-2026-11.md`: one firmware reviewed there blocked every later install). Note any that refuse.
6. The recovery path, in the README: if an install fails, plug the FM-1 straight into the computer (no hub), restore stock with the web installer (or M-VAVE's FM1 Open), then install again.
7. **Every device session, not just the release:** the PERF page with the UK STEPPERS desk and every effect up, and
   then with the demo song. Note the average, the peak, the tier the guard settles on, and `plat_stack_least_ever`,
   in the project's Practice log ("Device measurements"). The roadmap's CPU figures are host estimates (×80) until
   these readings replace them.

## Then

- Tag a release (`v0.3`: blocks 6 and 7, the demo tour and the 8.x plan) once the PR is merged, on `main`; the workflow publishes it. (`v0.1-beta` was the first; `v0.2` the 5.x block, PR 10.)
- Email zven@thefirmwarehub.com:
  - the name (Dubwise FM) and the author (DocRuzzy, SpIllt Beats)
  - the repository link
  - `FM-1_382`
  - GPL-3.0-only
  - a short description
  - the picture (`app/docs/img/screen-playing.png`)
  - confirmation of the hardware test
- Once, after the merge: Settings → Pages → Source: **GitHub Actions**; the browser version then goes live at https://docruzzy.github.io/dubwise-fm/emu/ and follows every merge.
- Later, optional: a `cheat` sheet.
