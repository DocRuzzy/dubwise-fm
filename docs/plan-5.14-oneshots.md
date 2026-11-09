# Plan: roadmap 5.14, your own one-shots on channel 8

Status: **approved 2026-11-09.** The owner chose µ-law at 22,050 Hz (1.5 s, clean). The technical choices, as
recommended: 36 KiB of XDATA1, and the tool saves the shots beside the store backup.

## What the player gets

- **Your sounds:** BIM, HORN and LASER (black keys 8–10) can be replaced with your own: a vocal tag, an air horn, a
  "selecta!", a gunshot.
- **Making them:** on a computer, `tools/make_oneshots.py` turns WAV files into the FM-1's format, and
  `tools/dubwise_slots.py shots put a.wav b.wav c.wav` sends them over USB (stopped).
- **The budget:** 1.5 s in all, split any way between the three (the same 32.4 KB the built-in shots use).
- **Getting the originals back:** SETUP → RESTORE FACTORY SHOTS (asked twice).
- **Survives:** they stay over a reboot, a firmware update and NEW PROJECT, and they go into the backup.

## Where they live: a new area of the flash

- **Where:** `PLAT_XDATA1` (92 KiB at 0xE5000, unused today), the first 9 sectors (36 KiB).
  - It's not a store object: 32 KB is beyond a store object's 3,840 bytes.
  - It isn't in the pool's way: at boot the shots are copied into the pool, as now, and the render never reads the
    flash.
- **The loader's rule:** the update loader looks for a record in bytes 0xF00–0xF07 of every sector in this window
  (`plat.h`). The layout leaves those 8 bytes of each sector erased (0xFF), with the data stepping around them, so a
  shot's bytes can never read as a record.

| Bytes | Field |
|---|---|
| 0–3 | magic "SHT1" |
| 4–7 | total data length |
| 8–11 | FNV-1a sum over the data (a cut-off write reads as no user shots: the factory ones play) |
| 12–35 | three entries: offset, length (bytes, ≤ 33,075 together), a name (8 characters, for the screen) |
| 36… | the data: 8-bit µ-law at 22,050 Hz (the built-in shots' format), with the 8-byte hole at 0xF00 of every sector |

- **Written header last:** as the song slots are, so a cut-off write leaves no header and the factory shots play.
- **Its own version:** "SHT1". Nothing older exists, so there's no converter. A future SHT2 would add one, planned
  then.
- **The backup:** `fm1_backup.py` copies store objects only, so the backup won't carry the shots by itself. Two
  choices (decision 3 below).

## SysEx (on 4.3's framing)

| Code | Meaning |
|---|---|
| 51 | dump the shots → pieces (as the slots' 11 / 12) |
| 52 n | a piece of a put (256 bytes, 7-bit packed; the first erases the area; refused while playing) |
| 53 len | the end: checked (length, sum), then the header written; 16 kept / 15 refused |
| 54 | restore factory (erases the header) |

## Tests

- **`oneshots.txt`:**
  - a put of three short shots (a sine, a click, noise), a reboot, each played: its level and length as put (`wav`);
  - a put cut off halfway: after a reboot the factory shots play;
  - a put while playing is refused;
  - RESTORE FACTORY is asked twice and works.
- **The loader's rule:** after a put, bytes 0xF00–0xF07 of every sector in the area read 0xFF (a host check, `data`).
- **`tests/shots_transfer_test.py`:** WAVs through `make_oneshots.py` and the tool's framing into the host simulator
  and back, byte for byte.
- **CPU:** unchanged (the same copy into the pool at boot, the same player).

## Cost (estimate)

- **Image:** ~2 KB.
- **Pool:** 0 net (the same 32.4 KB budget).
- **RAM:** ~0.3 KB (the put's piece buffer).
- **Flash:** 36 KiB of XDATA1.
- **CPU:** as now.
- **Tier:** all.

## Decisions for the owner

1. **µ-law at 22,050 Hz** (as the built-in shots: 1.5 s in all), or IMA ADPCM (twice the time, ~3 s, if its decode
   costs under 1 % of a block). Recommended: µ-law, the same player and no new decode.
2. **36 KiB of XDATA1** for it (the window is otherwise unused).
3. **The backup:** have the tool save the shots beside the store backup (a second file), or add the area to
   fm1-base's backup (an upstream change). Recommended: the tool, no upstream change needed.
