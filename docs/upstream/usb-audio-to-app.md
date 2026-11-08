# Upstream proposal: the computer's USB audio as an input to `app_render`

**For:** fm1-base (MPL-2.0), https://github.com/charlesvestal/fm1-base
**Needed by:** Dubwise FM roadmap P2.8 (live input on channel 8)
**Patch:** [`usb-audio-to-app.patch`](usb-audio-to-app.patch), made against the fm1-base commit this repo pins (`b0976f2`). Submit it under MPL-2.0, the licence of the files it changes.

## Problem

With `usb_audio_out = true`, a computer can play through the FM-1. `render_block` (`device/main_fm1.c`) adds that audio
to the output **after** `app_render` returns, in `uac_mix` (`device/uac_mix.h`), at `plat_usb_play_gain` × MASTER. The
app never sees it, so the app can't treat it as an instrument: it can't mute it, send it to an effect, or put it on a
mixer channel.

A dub desk needs exactly that. Classic dub is mixed from a tape machine, and the FM-1's equivalent of the tape machine
is a computer playing stems over USB. The vocal or the riddim from the computer has to sit on a channel strip that the
player can drop out and throw into the echo.

## Proposal

A new opt-in `fm1.toml` key, `usb_audio_out_app = true` (it needs `usb_audio_out`; the default is false, so no
existing app changes). It defines `FM1B_UAC_OUT_APP=1`, and then:

- Before each `app_render`, `render_block` zeroes a block buffer and calls `uac_play` into it at
  `plat_usb_play_gain`'s level (not MASTER's: the app's own mix sets that). `uac_play` still runs at level 0, so its
  rate tracking stays locked, as it does now.
- New call `const int32_t *plat_usb_play_in(uint32_t *frames)`. Inside `app_render` it returns the computer's audio for
  the block: stereo int32 frames, full scale 1 << 23, already resampled to the render clock. Outside `app_render`, or
  without the option, it returns NULL with `*frames = 0`.
- `uac_mix` skips its own `uac_play` under the option: the app mixes the audio itself.
- The USB recording (`uac_tap`) is unchanged. It still takes the app's output, which now includes the computer's audio
  only as the app chose to mix it.
- Host simulator: `plat_usb_play_in` returns NULL, since there's no computer. An app tests its own stand-in input.

## Cost

- RAM: one block buffer, 2 KB (256 stereo int32 frames).
- CPU: unchanged. `uac_play` runs once per block, as it does now.
- Flash: a few dozen bytes.

## Verified

- The device package builds with Dubwise FM against the patched copy, and every `register access` / `soft-double`
  check passes.
- The host build passes, and Dubwise's `app/tests/livein.txt` passes against both the patched and the current base.

Not yet tested on hardware: the patch moves `uac_play` before the render rather than after it. That should be
equivalent, because the ring is read once per block either way, but someone should confirm it on a device with a
computer playing.

## In Dubwise FM

`app/app.c` `live_in()` calls `plat_usb_play_in` when `FM1B_UAC_OUT_APP` is set. Otherwise it passes no input; on the
host it uses a test tone (`livein HZ DB`). `app/fm1.toml` already sets `usb_audio_out_app = true`. Today's fm1-base
ignores the key, so the switch happens automatically once the submodule is bumped to a release that includes this
change.
