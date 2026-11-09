# Plan: roadmap 5.4, sound patches (format, library, SysEx)

Status: **approved 2026-11-09** with the owner's decisions below (CLAUDE.md: a stored-format change needs a plan and an OK
before code).

## What the player gets

- On the SOUND view (EDIT), **PRESET** (the encoder) steps through the focused channel's patches: the factory ones
  first, then that channel's 16 user slots (empty slots skipped). Each step loads the patch at once, so you audition by
  turning. The header shows the patch's name. A run of PRESET turns is one undo step, like a knob run.
- **SAVE held + PRESET turned one click** stores the current sound in the channel's next free user slot, named
  automatically (KICK 01, BASS 07 …). A second SAVE + PRESET within 3 s overwrites the patch shown instead, asking
  twice (as 4.9 / QoL 5 confirmations do). All 16 full: "16 ALREADY: OVERWRITE ONE".
- Patches are a **library on the device**, like the SETUP preferences: they are not part of a project, a song slot or
  NEW PROJECT, so a sound you made is there in every project.
- A computer can back them up and share them: `tools/dubwise_slots.py patch get / put / list`, and the full backup
  (`fm1_backup.py`) includes them.

## What changes in storage

### 1. The patch (new, version 1): 48 bytes

| Bytes | Field | Notes |
|---|---|---|
| 0 | `ver` | 1. A patch with a higher version (from newer firmware) is shown as "NEWER FIRMWARE" and not loaded |
| 1 | `ch` | the channel it was made on (0–7). A patch loads only on its own channel |
| 2 | `model` | 0 now; 5.7's MODEL field when it comes |
| 3 | `flags` | bit 0: slot used |
| 4–11 | `name[8]` | A–Z, 0–9, space, `-` `.` `'` |
| 12–43 | `f[32]` | int8, the channel's **whole voice stride** (`P_VOICE(ch, 0..31)`): every raw field and the four macros |
| 44–45 | `sum` | CRC-16 over bytes 0–43; a bad sum reads as an empty slot |
| 46–47 | reserved | 0 |

Why the whole stride: a field appended to a voice later (5.7 MODEL, 5.15 B-3 fields) lands inside the stride, so it's
stored with no layout change. Patches made before it hold 0 there.

**The rule this depends on** (it's already 5.3's rule, made explicit in `params.h`): *a field appended to a voice must
sound neutral at 0, the value an older save or patch holds there.* A field that can't be neutral at 0 needs `ver` 2 and
a converter (a table of the fields each version added, with their defaults), planned like this one.

A compile-time check confirms every voice field's range fits int8 (they're all within −64..127 today).

### 2. The library: two new store objects

| Object | Holds | Flash (two 4 KiB sectors each) |
|---|---|---|
| `OBJ_PATCH0` | channels 1–4 × 16 patches | 0xC5000–0xC6FFF (free: between song slot 4 and `OBJ_SONG`) |
| `OBJ_PATCH1` | channels 5–8 × 16 | 0xDE000–0xDFFFF (free: after `OBJ_PANEL`) |

Each object: magic `"PAT1"`, 4 bytes, then 64 patches × 48 B = 3,076 B (the limit is 3,840 B). Both are appended to
the `OBJ_*` enum before `OBJ_NOBJ` (object numbers are written into their records, so the existing ones keep their
numbers), to `APP_STORE_MAP`, and to `APP_BACKUP_OBJS`.

**When the flash is written:** a flash erase or write briefly stops the sound (`plat.h`). So a save goes into a mirror
of the library in the pool at once and reaches the flash **only while stopped**: at once if stopped, otherwise at the
next stop ("SAVES AT STOP" shown). Browsing reads the mirror, never the flash.

### 3. `OBJ_SOUND` ("SND1"): no change

It already stores every voice ID (`P_VOICE_BASE..P_NIDS`, int16), so 5.3's fields and future ones are saved and loaded
as they are. The roadmap's planned "SND1 → SND2" converter would convert nothing, so it's dropped. The SND version is
bumped only when a field breaks the neutral-at-0 rule.

### 4. SysEx (on 4.3's framing, F0 7D 44 57)

| Code | Direction | Meaning |
|---|---|---|
| 31 ch slot | in | dump a user patch → reply 32 + the 48 bytes (7-bit packed) |
| 33 ch slot + 48 B | in | put a user patch → 16 kept / 15 refused (checked: version, channel, sum, ranges clamped) |
| 34 ch | in | dump a channel's 16 slots → 32 replies, then 12 end |
| 36 ch | in | dump the channel's current sound as a patch (an unsaved tweak) |
| 37 ch + 48 B | in | put a patch into the current sound (audition from a computer; one undo step) |
| 38–3F | — | kept for 5.5 kits |

The tool gains `patch get CH SLOT FILE`, `patch put CH SLOT FILE`, `patch list CH`, and `patch current CH FILE`.
Files are the 48 raw bytes (`.dwpatch`).

## Old saves, old backups, downgrades

- **A project saved before 5.4:** `OBJ_SOUND` loads as now. No patch objects exist, so the library is empty (factory
  patches still browse).
- **A backup made before 5.4,** restored: the platform writes an empty object for each one a backup lacks (`plat.h`), so
  the library is emptied: the device as it was then. The manual says so; `dubwise_slots.py patch get` keeps patches
  first.
- **A library object with a magic we don't know** (a newer firmware's `PAT2`, then a downgrade): read as empty, and
  **not overwritten** until the player confirms ("PATCHES FROM NEWER FIRMWARE: SAVE ANYWAY?", asked twice). A downgrade
  can't silently wipe them.
- **Song slots, scenes, NEW PROJECT:** untouched. The library isn't project data.

## Factory patches in 5.4

Recommendation: each channel's **INIT** (the defaults) plus the tuned sounds the firmware already has: the demo
("Righteous Dub") and UK STEPPERS sound settings, as named factory patches. That's two or three a channel, from sounds
already chosen by ear. The wider factory set comes with 5.5's kits (the same sound-design pass). Factory patches are a
const table in flash (a few hundred bytes).

## Tests

- `patches.txt`:
  - save to slot 1 by SAVE + PRESET (stopped: `store_writes` +1);
  - change the sound, browse back: every field equals the saved one;
  - a browse run is one undo step;
  - a second SAVE + PRESET within 3 s asks, then overwrites;
  - the 17th save says 16 ALREADY;
  - a save while playing waits for the stop (no `store_writes` until then; the WAV around it has no gap);
  - reboot keeps the library;
  - a patch from another channel and a newer-version patch are refused.
- **The old-save test** (`oldsound` host command):
  - writes an `OBJ_SOUND` as saved before 5.3 (no new fields, no macros), plus no patch objects;
  - reboots;
  - checks the 5.3 fields are at their defaults, every `movedC_K` is 0, the library is empty, and the kick and bass
    render the same WAV as the defaults (`cap_wav_diff` 0).
- **A downgrade case:** a `PAT2`-magic object is left alone until confirmed.
- **SysEx round trip** (`tests/slot_transfer_test.py` extended): get a patch, put it into another slot, read it back
  byte for byte; a damaged one (bad sum) is refused with 15.
- `backup.txt`: a backup round trip includes the two objects; an old backup leaves the library as it is.

## Cost (estimate)

- **Image:** ~4 KB (UI, SysEx, factory table).
- **RAM:** ~0.1 KB.
- **Pool:** +6.2 KB (the library's mirror; 300 of 344 KB).
- **Flash data area:** 16 KiB (the roadmap's "8 KB" missed the A/B sector pairs).
- **CPU:** 0. It's main loop only, and the render never sees a patch, only the fields as ctl_set moves them.
- **Tier:** all.

## Decisions (the owner, 2026-11-09)

1. **No saves while playing:** SAVE + PRESET says "STOP TO SAVE" (a flash write would cut the sound). The same holds
   for a SysEx put: refused (15) while playing. So no pool mirror is needed for deferred writes. The library is still
   read into the pool once at boot, so browsing never reads the flash.
2. **Device-wide library.** It doesn't limit the count: 16 a channel fits two store objects, and if that's ever tight,
   the unused XDATA1 window (92 KiB) holds over 1,500 more. Project templates ("NEW PROJECT FROM SLOT") are noted as a
   separate roadmap idea: a song slot already carries a whole project's sounds.
3. **No SND1 → SND2 converter;** the neutral-at-0 rule is written into `params.h`.
4. **Factory patches in 5.4:** INIT on every channel, plus what's already tuned: STEPPERS for the bass and skank (UK
   STEPPERS' settings) and the lead's other instrument. The rest come with 5.5.
5. **Automatic names** (KICK 01) on the device; renaming by the tool. On-device renaming is a later item.
6. **VOX gets patches** like the other seven.

Changed from the draft:
- **Overwriting:** SAVE + PRESET while a user patch is shown (as browsed to, or edited since) asks "OVERWRITE KICK 03",
  and a second SAVE + PRESET within 3 s overwrites it: tweak, then save over. On a factory patch, or a sound with no
  patch shown, it saves to the next free slot.
- **SysEx put into the current sound (37):** like a controller's moves, it isn't kept in undo (undo keeps what the
  panel's hands do).
