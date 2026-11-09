# The FM-1 firmware roundup review: lessons for Dubwise (2026-11-09)

Source: a YouTube review of the groovebox firmwares on thefirmwarehub.com (about 30 of the 154 firmwares as of 4 Nov 2026), https://www.youtube.com/watch?v=B8uoeL0djDk, read from its transcript. Firmware names are as transcribed and some are probably wrong. **"Top one" (12:29, 4★) is almost certainly DUB-1:** at 15:03 the reviewer says "just like dub one and hype one". That is an inference. Dubwise isn't in the review.

Kirk approved every recommendation on 2026-11-09. They are roadmap items 5.1–5.14 (`roadmap.md`) and spec 4.9–4.11 (`interface-spec.md`). Companion: `qol-review-2026-11.md` (READMEs of 16 firmwares, general quality of life).

## 1. How the reviewer scores
His criteria: how well the firmware works with the hardware, ease of use, workflow. "A thousand different features doesn't mean much… if they aren't implemented right." Bugs cost a whole tier (Ravel, Tribe: "great ideas", 3★). Reskins of another firmware get 1★.

| ★ | Firmware | Praised | Complaint |
|---|---|---|---|
| 5 | "Giant" | Charm and animations; one LFO page shows every destination; you **watch the modulation happen** | — |
| 5 | X0X | Step sequencer as the core; fun | — |
| 5 | FM1 Open (M-VAVE) | Fast, easy, intuitive; **web editor**; step record/edit; full DX7 engine | Wouldn't install on his original unit; **live-mode recording didn't capture exactly what he played** |
| 5 | LOOM-8 | **Step editor as the main input** on a small device; effects; familiar groovebox workflow | — |
| 5 | Chipbox | Interface, sound and feel fit together; **on-device manual** | — |
| 5 | Tesora | **Generators as tools** (bounce, Euclidean, Turing machine…) "but you're still the one in control"; normal recording | — |
| 4 | SLOOP | Sounds; home interface | Save menu, LFO workflow |
| 4 | Skiff | Main features on the homepage | **Can't change track length from home** |
| 4 | Ghoulbox | MIDI info under the instrument | **A decay that never stopped while recording** |
| 4 | DUB-1 ("top one") | More control than other generative firmwares | "The music doesn't feel like it is yours entirely" |
| 3 | Felucca, Mini Tone, Jenada | Engines and sounds; Mini Tone's drum page (black keys pick a sound, white keys are steps) | No overview; workflow "isn't there yet" |
| 3 | Hype 1, Perf 1 | Sound | **"I don't feel like I'm playing this instrument"** |
| 3 | Makohai | You change sounds and melody; **fades out when stopped** | — |
| 3 | GR!MM | Latching drone; granular | Missing common features |
| 3 | TRIBE | Fast track switching; **any engine on any track** | Bugs |
| 3 | Mono 2K | Built around the step editor; lo-fi envelope graphic | Some things didn't work as expected |
| 2 | MC1 | Pattern system | Effects page unreadable; **"no way to edit the different instruments… quite vital"** |
| 1 | ORBIT, "Spam", Dungeon tracker, WinAmp | — | Reskins of SLOOP. WinAmp: **text too small, waits between menus** |
| 1 | Rude, Sill, Jam playground, one unnamed | — | Generative with no control; buttons "do random stuff"; **can't stop it** |
| 0 | Bubba Machine | — | **Blocked installing any other firmware** (recovered by plugging straight into a computer and installing through FM1 Open) |

## 2. Against Dubwise as built
**Already answered** (checked against the code):
- **Not generative** (principle 1). Generative firmwares lost points for "not mine", DUB-1 included.
- **Step editing, live recording, patterns** (roadmap 0.4, 4.8). The drum pads and the black-key channel select match Mini Tone's praised page.
- **Help cards** (4.7), **one-gesture SAVE** plus autosave (4.5), **undo** (4.6), **confirm before destructive actions** (4.9).
- **Installing and removing.** The update loader is in the package, and installs from stock V15 on both flash chips and from another firmware pass in simulation (`release-checklist.md` row 4). The hardware test is still the owner's to do.
- **STOP lets the echo and spring ring out.** `transport_stop()` releases notes and motion holds and leaves the effects alone; Makohai was praised for this.

**Gaps → roadmap 5.x:**

| Lesson | Evidence | Item |
|---|---|---|
| Recording must replay exactly what was played; every sound must end | FM1 Open, Ghoulbox | 5.1 |
| A firmware you can't silence is a 1★ firmware | Jam playground | 5.2 (second STOP kills the tails) |
| **Editing the instruments is "vital"**; sounds go stale | MC1; Kirk's concern | 5.3–5.7 (parameters, patches, kits, Dice, models) |
| Variety without generative playback | Tesora's tools, the generative 1★s | 5.6 Dice, 5.8 drum locks, 5.9 seeded probability |
| Watch the modulation happen | "Giant", Mono 2K's envelope | 5.10 |
| Legibility, no menu waits; key settings reachable from home | WinAmp, MC1, Skiff | 5.11 |
| A web editor | FM1 Open | 5.13 |
| Install from several firmwares; a documented recovery | Bubba Machine, FM1 Open | `release-checklist.md` |

## 3. Why sound design fits the budget
- **The worst-case CPU is set by topology** (operators, rate, longest tail), not by parameter values. More parameters, macros, patches, kits and Dice cost flash, a little RAM and UI, not audio CPU, as long as decays stay bounded (they set voice lifetime) and the bass's FM index stays bounded (a clean fundamental).
- **Voice models (5.7) are the only item that touches CPU.** Each model must measure at or below its channel's present cost (perf tests), so the guard tiers and the "everything on" figure (~58 %, review-2026-11) don't move.
- **Pool headroom is the binding limit.** 294,020 of 344,064 B are used and review-2026-11 says keep ≥ 40 KB free, so new pool use stays under ~10 KB. If more is needed, take review-2026-11 §5.4 (spin ring as µ-law, −17.6 KB) first. User one-shots (5.14) therefore replace the factory shots' 32.4 KB in the pool rather than adding to it.
