# Submitting the USB-audio-to-app change to fm1-base

This session can only reach the Dubwise repository, so the pull request to
[charlesvestal/fm1-base](https://github.com/charlesvestal/fm1-base) has to come from your GitHub account. It takes about
ten minutes. The patch is [`usb-audio-to-app.patch`](usb-audio-to-app.patch). It applies cleanly to the fm1-base commit
Dubwise pins (`b0976f2`). It was checked with `git apply --check` on 2026-11-09, and it builds for the device and the
host.

## Steps

1. On GitHub, open https://github.com/charlesvestal/fm1-base and click **Fork**.
2. On your computer, in a clone of the Dubwise repository:
   ```
   cd base
   git remote add mine https://github.com/<your-username>/fm1-base.git
   git checkout -b usb-audio-out-app
   git apply ../docs/upstream/usb-audio-to-app.patch
   git commit -am "usb_audio_out_app: the computer's audio handed to app_render (plat_usb_play_in)"
   git push mine usb-audio-out-app
   ```
3. GitHub then shows a **Compare & pull request** button on your fork. Use the title and description below, then click
   **Create pull request**.
4. Afterwards, in Dubwise's `base/`, go back with `git checkout -` so the submodule is on its pinned commit again.

If fm1-base has moved on and the patch no longer applies, update `base/` to its latest `main` first. If it still
doesn't apply, ask me to rebase the patch.

## Title

```
usb_audio_out_app: hand the computer's USB audio to app_render (plat_usb_play_in)
```

## Description

```
An opt-in fm1.toml key, usb_audio_out_app = true (needs usb_audio_out; default false, so no existing app changes):
the computer's audio is handed to app_render instead of being mixed in after it.

Why: an app that treats the computer's audio as an instrument (a channel it can mute, send into an effect, record)
can't, because render_block adds it to the output after app_render returns. Dubwise FM (a dub desk) wants the
computer's stems on a channel strip that can be dropped out and thrown into a tape echo.

What it changes (5 files, +49 / -1):
- tools/build.py: the key -> -DFM1B_UAC_OUT_APP=1 (refused without usb_audio_out).
- include/plat.h: const int32_t *plat_usb_play_in(uint32_t *frames): inside app_render, the computer's audio for the
  block (stereo int32, full scale 1 << 23, resampled to the render clock, at plat_usb_play_gain's level); NULL
  outside it or without the option.
- device/main_fm1.c: before each app_render, uac_play into a zeroed block buffer (it still runs at level 0, so its
  rate tracking stays locked, as now); plat_usb_play_in's definition.
- device/uac_mix.h: no uac_play there under the option (the app mixes it).
- host/plat_host.c: plat_usb_play_in returns NULL (no computer on the host).
The USB recording (uac_tap) is unchanged.

Cost: 2 KB RAM (one block buffer) with the option on; no CPU change (uac_play runs once a block, as now).

Tested: the device package builds with Dubwise FM against the patched copy (every register-access and soft-double
check passes); the host build passes, and Dubwise's live-input scenario passes against both the patched and the
current base. Not yet tested on hardware with a computer playing: the change moves uac_play before the render rather
than after it, which should be equivalent (the ring is read once a block either way), but deserves a listen.
```
