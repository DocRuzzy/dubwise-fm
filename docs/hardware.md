# FM-1 hardware revisions (2026-11-09)

What differs between FM-1 units in the field, what Dubwise needs for each, and what to check on a newer unit. Sources:
the Firmware Hub's notice to firmware authors (Sven, 2026-11), SLOOP's release notes (isod89/sloop-fm1 v2.7–v2.8.5),
and fm1-base / felucca-bsp at the commits Dubwise pins (`base/` at `b0976f2`).

## The flash chip (units sold since autumn 2026)

| | Stock | Newer units (mostly AliExpress) |
|---|---|---|
| Part | Puya P25Q80 | UC25HQ80 |
| JEDEC ID | `0x856014` | `0xB36014` |
| Size, commands | 1 MiB, plain SPI NOR | the same |

A firmware that accepts only `0x856014` treats a newer unit as unknown: saves stay in RAM ("flash error") and its
update loader refuses to install.

**Dubwise is covered by its platform.** It has no flash check of its own; fm1-base does all of it:
- `base/device/fm1_flash_ok.h`: `FM1B_FLASH_JEDEC_OK(id)` accepts both IDs, and nothing else. The ID is read once at
  boot (`main_fm1.c`), and that one answer gates everything that writes the flash: SAVE and autosave (the store),
  the song slots and the motion (the data area), backup and restore, the crash log and the resource region.
- The update loader (`base/bsp/loader/loader.c`) has no flash-ID check, so it can't refuse the new chip. The
  firmware's identity reply carries the chip's ID (felucca-bsp#29), and fm1-base's installer prints it
  (`fm1_install.py --info`: "flash: UC25HQ80 (B36014)").
- `firmware.json` lists both parts (`"flash": ["856014", "B36014"]`), as the build writes it from that header.
- **Guard:** `tests/run_tests.sh` compiles and runs fm1-base's `flash_ok_test.c` and checks `firmware.json`, so an
  update of `base/` that dropped a part fails CI.

The Hub's notice (accept both IDs "in the app and in the update loader", `ldr_flash_known`) is for firmwares with
their own flash code (SLOOP, OberVave, Dinghy). Dubwise needs no change; its first release (v0.1-beta) carries the
check.

## Supply voltages

SLOOP 2.7: the boot loader leaves SYSVDD at 1.26 V and VDC14 at 1.40 V, "too low on some units with both cores busy".
fm1-base raises them at power-on, before anything loads them (VDC14 1.60 V, SYSVDD 1.38 V; `main_fm1.c`). Nothing to
do in Dubwise.

## The second core

SLOOP 2.8.3: on some units the second core "can misread a word under heavy load". Dubwise doesn't use the second
core (no `cpu2` in `fm1.toml`; the engine is playable on one core, per the hard rules). If it ever does, each job
must be checked before it is used, and stopped safely when it isn't done in time (SLOOP 2.7.1, 2.8.3).

## The speaker

The FM-1's speaker, like a phone's, plays almost nothing below ~50 Hz. Dubwise's kick lands at 40–70 Hz and its
sub bass an octave under the bass: on the speaker they are felt as their harmonics (the kick's click, the bass's
pluck). Headphones or the line out for the low end. A hardware limit on every unit, not a revision.

## On a newer unit, when it arrives

1. `python3 base/tools/fm1_install.py --info`: the chip it reports (`UC25HQ80 (B36014)` or `Puya P25Q80 (856014)`).
2. Install the release package from the stock firmware, and again from Dubwise itself (the update path).
3. SAVE, switch off and on: the project is back (the store). Autosave the same.
4. SAVE TO SLOT, LOAD SLOT; a slot to a file and back (`tools/dubwise_slots.py`); backup and restore.
5. The PERF page reading (`release-checklist.md` step 7), logged in the Practice log beside the unit's chip.
6. Anything that differs from a stock unit: noted here, with the chip.
