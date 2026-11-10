# Percussion recordings for fitting the channel 8 kit (roadmap 8.2)

Not committed (only `.md` files are): only numbers measured from the recordings go into the firmware, with credit.

## vsco2/ (fetched 2026-11-10)

From Versilian Studios' VSCO 2 Community Edition, https://github.com/sgossner/VSCO-2-CE, **CC0 1.0** (its `LICENSE`
is copied in). Used for the conga, quinto, tumba, bongos, the "giant" hand drum and the orchestral bass drum (as
stand-ins for the nyabinghi drums until real ones arrive), cowbell, claves, woodblock (`wood_click`), tambourine,
maracas and Camo's shaker.

## blackarc/ (the owner's, 2026-11-10)

From Native Instruments' **Black Arc** expansion, which the owner owns: 67 one-shots (Percussion and Shaker folders).
Licensed to the owner for use in productions, not for redistribution: **never committed**, and only measured numbers
(modes, decays, noise bands) go into the firmware. Credit in `CREDITS.md` as "fitted partly to measurements of the
owner's licensed Native Instruments Black Arc samples; no audio included".

There's no nyabinghi set in it, so the three drums are matched to the hits that play their roles, by measurement:

| Role | The nyabinghi drum | Stand-in | Measured |
|---|---|---|---|
| Bass ("thunder") | big, low, open, a padded beater | `Kick VaporIzer 1` (also `2`) for now: the only kicks that ring; a floor tom if the owner's Tom folder comes through (the first zip was empty) | modes 47 and 129 Hz, rings to -40 dB in 0.75 s (0.81) |
| Funde | slack head: low, dull, short | `Conga BlackSpark` (also `Conga LickShot`) | 221 Hz (226), -20 dB in 0.10 s (0.17) |
| Repeater (kete) | tight head: higher, rings | `Bongo SteppersRock` (also `Conga SkaBuster`) | 652 Hz (483), rings to -40 dB in 0.63 s (0.55) |

The rest of the kit is fitted to Black Arc's reggae-session hits first, with VSCO 2 as a cross-check:
- `Conga *`, `Bongo *`;
- `Cowbell *`, `Woodblock *`, `Tamb *`, `Shaker *`;
- `Afuche *` (cabasa), `Guiro *`, `Triangle *`.

Also from Black Arc (2026-11-10): `Hihat/` (91), `Snare/` (61), `Cymbal/` (19), `Kick/` (23). Besides the
nyabinghi bass drum, these are the reggae session's own drums: a reference for checking the kick, rim and hats models
against (the studies, 8.6). The kicks are tight: 40-80 Hz, most under -40 dB within 0.1-0.4 s.

## nyabinghi/ (if real recordings turn up)

Real nyabinghi drums (bass drum, funde, repeater / kete) from Freesound: credit each file's page and licence here.
