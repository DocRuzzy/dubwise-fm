# Listing drafts (2026-11-11): fm1-emu and RemoteFM1

Drafts for the owner to send. Neither is sent yet. Both need the Pages change in this PR merged first, so that
`https://docruzzy.github.io/dubwise-fm/firmware/dubwise-latest.fwsc` exists. Browser emulators need CORS headers,
which github.io sends and GitHub's release downloads don't.

**Before sending, try it in fm1-emu yourself.** It runs the real device package in an emulated FM-1, which is not
the same as our own browser version (that one is our code compiled to WebAssembly):
<https://joakimeriksson.github.io/fm1-emu/?package=https://docruzzy.github.io/dubwise-fm/firmware/dubwise-latest.fwsc&name=Dubwise%20FM>

If it boots to the DUB page and plays the demo, send the fm1-emu pull request. If it doesn't, tell Claude what the
emulator shows; that's a finding worth having before any hardware.

## 1. fm1-emu: a pull request

Repository: <https://github.com/joakimeriksson/fm1-emu>. Fork it, edit `web/firmwares-own.json`, and add this entry
to the `firmwares` list (after the last one; mind the comma before it):

```json
    {
      "id": "dubwise",
      "name": "Dubwise FM",
      "version": "0.4.2",
      "author": "DocRuzzy",
      "license": "GPL-3.0-only",
      "about": "A reggae and dub mixing desk you play live: an eight-channel band (FM drums, bass, skank, organ, melodica or horns, sirens), each channel with a quantised mute and a throw into a tape echo you can push into feedback and a spring you can kick, and a stepped Big Knob filter. Nothing plays itself. Beta: not yet tested on a real FM-1.",
      "repo": "https://github.com/DocRuzzy/dubwise-fm",
      "package": "https://docruzzy.github.io/dubwise-fm/firmware/dubwise-latest.fwsc",
      "installer": "https://docruzzy.github.io/dubwise-fm/install/"
    }
```

The README's firmware table is made by the maintainer's tools (`node tools/hub_catalog.mjs`,
`node tools/readme_firmwares.mjs`), so the pull request only needs the entry.

**Pull request title:** Add Dubwise FM (reggae and dub mixing desk) to the own list

**Pull request text:**

> Dubwise FM is a GPL-3.0 firmware that turns the FM-1 into a reggae/dub groovebox where the mixing desk is the
> instrument: quantised mutes, throws into a feedback tape echo and a kickable spring, a stepped Big Knob filter,
> riddim templates and FM voices. It's built on fm1-base and felucca-bsp.
>
> It isn't on the Firmware Hub yet: the Hub asks for a test on a real FM-1, and mine hasn't arrived. Listing it here
> lets people try it in the meantime. Once it's on the Hub, the entry can go (or keep its text, as your note says).
>
> - The package is the release workflow's build (`zvenson/fm1-build@v1`, attested), copied to GitHub Pages so the
>   page can fetch it: `dubwise-latest.fwsc` follows each release.
> - Tried here first: [the link above] boots and plays the demo (SETUP → DEMO SONG).
>
> Thanks for the emulator: its survey of the firmwares shaped a good part of Dubwise's 6.x block.

## 2. RemoteFM1FW: an issue

Repository: <https://github.com/Quixotic7/RemoteFM1FW>. Their README: "if you'd rather your firmware weren't listed
here, or want to publish `.rfm1` files yourself, open an issue".

**Issue title:** Add Dubwise FM (DocRuzzy/dubwise-fm)

**Issue text:**

> Hi! I'd like Dubwise FM to be in RemoteFM1's catalog.
>
> **What it is:** a reggae and dub mixing desk you play live, for the FM-1. Eight channels (FM drums, bass, skank,
> organ, melodica or horns, sirens and shouts), each with a quantised mute and a throw into a tape echo and a
> kickable spring, a stepped Big Knob filter, a countdown that lands every move on the one. Nothing plays itself.
>
> - Repository: https://github.com/DocRuzzy/dubwise-fm (GPL-3.0-only; files from fm1-base's template stay MPL-2.0)
> - Built on fm1-base and felucca-bsp, laid out as fm1-base's template expects (`./build.sh`, `app/`, `base/`).
> - Releases: from v0.4.2, each release carries a `.rfm1` built by our release workflow (fm1-base's
>   `./build.sh --rfm1 --release X.Y.Z`, Emscripten 6.0.10 from emsdk), attested with the `.fwsc`. Use it if that
>   helps; build your own from source if you prefer.
> - `RemoteFM1Metadata.json` is in the repository root from v0.4.2: the card, a 480×480 card image, a colour theme
>   and the panel labels.
> - USB MIDI port name: `DUBWISE FM-1`.
> - It uses the FM-1's velocity and pressure (black keys 1 and 3–7 respond to how hard you lean), and the USB host
>   port for MIDI keyboards; no Bluetooth.
>
> **Status:** beta, not yet tested on a real FM-1 (mine hasn't arrived). That's partly why RemoteFM1 appeals: owners
> could play it with their FM-1 as the panel without installing it. Hardware test list:
> https://github.com/DocRuzzy/dubwise-fm/blob/main/docs/hardware-test-list.md
>
> Happy to change anything in the metadata file if it doesn't fit how you build.
