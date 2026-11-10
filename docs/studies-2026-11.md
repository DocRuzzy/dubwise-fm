# The studies (roadmap 8.6): the five records against our demo

The owner supplied the five records (kept out of git in `samples/refs/`; only these numbers are committed).
`tools/studies/refstat.py` measures every file the same way:
- **Balance:** each octave band's share of the long-term power, in dB.
- **Side/mid:** a band's stereo difference against its centre, in dB.
- **Crest:** the median peak-to-RMS of 400 ms windows: how punchy or dense the mix is.
- **Tempo** (read as felt, half time) and **key:** estimates. A low r means the key reading is unsure.

This first pass compares whole records with the whole demo (RIGHTEOUS DUB, `demo.txt`'s render). The per-record
studies come next, built in each record's manner from original parts, and compared section by section.

| Record | Tempo | Key (r) | RMS dBFS | Crest dB |
|---|---|---|---|---|
| Tubby and Pablo, "King Tubby Meets Rockers Uptown" | 66.0 | Bbm (0.31, unsure) | -16.3 | 12.2 |
| Perry, "Zion's Blood" | 67.0 | Am (0.86) | -8.2 | **7.8** |
| Scientist, "Your Teeth In My Neck" | 72.8 | E (0.49) | -15.2 | 13.2 |
| The Abyssinians, "Satta Massagana" (dub) | 75.0 | Am (0.68) | -10.2 | **9.5** |
| Burning Spear, "Marcus Dub" | 74.8 | Em (0.77) | -21.9 | 12.4 |
| **Ours: RIGHTEOUS DUB** | 71.8 | Am (0.79) | -22.5 | 13.6 |

**Balance by octave band** (dB of the whole):

| Hz | 31 | 63 | 125 | 250 | 500 | 1k | 2k | 4k |
|---|---|---|---|---|---|---|---|---|
| Rockers Uptown | -3.1 | -7.8 | -10.2 | -13.7 | -12.6 | -10.7 | -13.1 | -23.2 |
| Zion's Blood | -3.2 | -5.5 | -13.6 | -12.0 | -10.6 | -14.4 | -20.0 | -26.5 |
| Your Teeth | -9.4 | -5.3 | **-7.2** | -13.5 | -9.0 | -10.0 | -12.7 | -12.0 |
| Satta (dub) | -5.7 | -3.7 | -12.5 | -10.9 | -11.1 | -13.3 | -15.3 | -20.9 |
| Marcus Dub | -10.6 | -1.4 | -13.5 | -14.3 | -13.3 | -15.3 | -17.1 | -23.9 |
| **Ours** | -6.2 | -4.0 | -14.0 | -13.0 | **-8.6** | -11.8 | -16.0 | -24.3 |

**Side/mid by band** (dB; more negative is more mono):

| Hz | 31 | 63 | 125 | 250 | 500 | 1k | 2k | 4k |
|---|---|---|---|---|---|---|---|---|
| Rockers Uptown | -9.1 | -7.8 | -5.4 | -4.8 | -3.8 | -3.1 | -0.4 | 3.0 |
| Zion's Blood | -47.2 | -24.4 | -9.6 | -10.1 | -10.3 | -9.0 | -8.5 | -8.8 |
| Your Teeth | -27.7 | -22.8 | -19.5 | -13.2 | -11.2 | -10.9 | -12.8 | -8.1 |
| Satta (dub) | -29.3 | -27.6 | -12.1 | -12.9 | -15.7 | -10.7 | -7.1 | -0.3 |
| Marcus Dub | -28.7 | -27.9 | -10.5 | -10.2 | -10.1 | -7.9 | -5.6 | -6.7 |
| **Ours** | -70.8 | -56.1 | -31.0 | -20.9 | -20.6 | -24.1 | -26.2 | -23.4 |

## What the numbers say (first pass)

1. **Density: our mix is peakier.**
   - Zion's Blood (7.8 dB) and Satta (9.5 dB) are far denser than our 13.6 dB. That is the bounce and the tape: Perry's
     generations and the single's cut.
   - The others sit at 12-13 dB, close to us.
   - This supports **8.5, the bounce**, and the tape drive on the mix as the way to get there.
2. **The low end is right, and the rest may be too narrow.**
   - Ours is mono below 125 Hz, as our rule says, and so are four of the five (Rockers Uptown's -9 dB at 31 Hz is
     probably the reissue's or the vinyl's noise, not the mix).
   - Above 125 Hz, the records keep 4-15 dB less side than mid; we keep 20-31 dB less. Their stereo comes from
     instruments panned in the stereo mixes (Rockers Uptown is almost a wide stereo mix). Ours comes only from the
     effect returns, because the dry path is mono by design.
   - **This needs the owner's decision.** Either keep the rule and widen the returns (more echo width, the spring's
     two sides further apart), or allow a per-channel pan above about 150 Hz, which changes a design principle.
3. **Tonal balance: a bump at 500 Hz, a dip at 125 Hz.**
   - We are 2-5 dB heavier at 500 Hz than all five (the skank, organ and melodica sit there).
   - Most of the records carry more at 125 Hz (Your Teeth +7 dB over us: the bass's body and the kick's).
   - 8.1's EQ lets a player fix this. The voices' defaults might be moved too, after the per-record studies.
4. **Tempo and key:** 66-75 BPM, minor keys (A, E). Our templates and the demo sit right in that range.
5. **Level:** ours is quieter (-22.5 dBFS RMS against -8 to -22), which is the test's master setting, not the sound.
   The denser records are also the louder ones.

## Next

- The per-record studies (`app/tests/study-*.txt`): each record's sections (drum and bass, the melodica's entries,
  the filter's sweeps, the echo's explosions) measured over a matching passage, not the whole.
- The order follows 8.1 to 8.5: each new feature is checked against the record that asked for it.
