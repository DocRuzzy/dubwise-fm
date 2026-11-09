#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The second walkthrough: patterns and recording (roadmap 4.8). The same machinery as first_dub.py (the host
simulator's screen and sound, the panel lit as it is played, captions; no voice), its own storyboard: a bass line
recorded into an empty pattern, copied, launched, doubled, switched from the desk, and drum pads overdubbed.

  python3 tools/video/patterns_dub.py        (after ./build.sh --host and ./build.sh --web)

Writes app/build/video/: patterns-dub.mp4, patterns-dub.srt, patterns-dub-narration.txt.
"""
import sys

import first_dub as fd

T0, BEAT, BAR = 10065, 800, 3200                # 75 BPM; PLAY let go at 10,060 ms


def at(bar, beat=1, plus=0):
    return T0 + (bar - 1) * BAR + (beat - 1) * BEAT + plus


fd.NAME = "patterns-dub"
fd.PRELUDE = ["wait 600", "listen", "master 3400", "param rec.offset 0"]   # (the host has no output buffers)
fd.TITLE_CARD = ("Patterns and recording", ["Dubwise FM on the M-VAVE FM-1", "Four patterns a channel, and live loops",
                                             "", "(the real firmware, in its simulator)"])
fd.OUTRO_CARD = ("That's a loop.", ["Next: store a scene with patterns for each", "version (ARRANGE, KNOB 4: DESK+PAT),",
                                    "then capture the song (REC on the DUB page).", "",
                                    "Cheat sheet: docruzzy.github.io/dubwise-fm/cheat"])
fd.CAPTIONS = [
    (0, ""),
    (3000, "Press SEQ for the STEPS page, then black key 4: the bass."),
    (6200, "Every channel has four patterns, A to D. Hold HOME and press white key 2: the bass's pattern B. It's empty."),
    (9600, "Press PLAY. The riddim plays, and the bass is silent."),
    (at(2, 1), "Press REC. An empty pattern waits for the next bar line, and the click counts you in."),
    (at(3, 1, -300), "Play! One bar of bass: it records one pass..."),
    (at(4, 1), "...then loops it as you played it: each note lands where you put it, not on a stiff grid."),
    (at(5, 1), "SEQ leaves the keyboard. Hold SAVE and press white key 3: the line is copied into pattern C."),
    (at(5, 4), "Hold HOME and press white key 3: C takes over at the next bar line, from its top."),
    (at(6, 1, 400), "HOME and OCT+ double it: two bars. On the second page, take out its first note: a gap where the "
                    "dub breathes."),
    (at(7, 1), "HOME goes back to the DUB page. The letter by each channel is the pattern it plays."),
    (at(8, 1), "Hold the bass's mute key and turn PRESET: back to B, on the line. The mute stays as it was."),
    (at(9, 2), "On a drum channel, white keys 1 to 4 are pads: kick, rim, closed hat, open hat. REC records over "
               "the loop at once."),
    (at(11, 1, 200), "REC stops. Not happy with it? HOME and REC take the whole pass back."),
    (at(12, 1, 200), "Patterns, launches and loops: the dub engineer's multitrack, under your fingers."),
]
fd.OUTRO_FROM = at(12, 1, 3000)
fd.END_MS = fd.OUTRO_FROM + 5500
fd.TITLE_UNTIL = 3000
fd.EVENTS.clear()
E = fd.EVENTS


def hold(btn, t0, t1):                           # a button held from t0 to t1
    E.append((t0, f"press {btn}", 2, ("btn", btn, True)))
    E.append((t1, f"release {btn}", 2, ("btn", btn, False)))


fd.press(3500, "SEQ")
fd.key(4500, "b3")                               # the bass
hold("HOME", 7000, 7700)
fd.key(7250, "w1")                               # its B
fd.press(10000, "PLAY")
fd.press(at(2, 1, 300), "REC")                   # armed for bar 3
# a bar of bass in A minor (the keyboard: white 3 is A1, 5 C2, 6 D2, 7 E2), a little off the grid here and there
fd.key(at(3, 1, 5), "w2", 350, 110)
fd.key(at(3, 1, 610), "w2", 120, 70)
fd.key(at(3, 2, 390), "w4", 300, 100)
fd.key(at(3, 3, 415), "w6", 200, 100)
fd.key(at(3, 4, -10), "w5", 150, 90)
fd.key(at(3, 4, 400), "w4", 250, 100)
fd.press(at(5, 1, 200), "SEQ")                   # the keyboard off
hold("SAVE", at(5, 2), at(5, 2, 500))
fd.key(at(5, 2, 200), "w2")                      # copied into C
hold("HOME", at(5, 4), at(5, 4, 400))
fd.key(at(5, 4, 150), "w2")                      # C launched for bar 6
hold("HOME", at(6, 1, 500), at(6, 1, 900))
fd.press(at(6, 1, 650), "OCT+")                  # doubled
fd.press(at(6, 2, 600), "OCT+")                  # its second page
fd.key(at(6, 3), "w0")                           # step 17 out
fd.press(at(6, 4), "OCT-")
fd.press(at(7, 1, 200), "HOME")                  # the DUB page
fd.key(at(8, 1), "w3", 1500)                     # the bass's mute key held ..
fd.turn(at(8, 1, 500), "PRESET", -1)             # .. and PRESET: B, on bar 9's line
fd.press(at(9, 2), "SEQ")
fd.key(at(9, 2, 400), "b0")                      # the kick's channel: the pads
fd.press(at(9, 3), "REC")                        # an overdub, at once
for b in (2, 3, 4):                              # rims on the off-beats, an open hat to lift it
    fd.key(at(10, b, 400), "w1", 80, 80)
fd.key(at(10, 4, 0), "w3", 80, 100)
fd.press(at(11, 1), "REC")                       # stop recording
hold("HOME", at(11, 3), at(11, 3, 400))
fd.press(at(11, 3, 150), "REC")                  # the pass undone
fd.press(at(12, 1), "PLAY")                      # stop

if __name__ == "__main__":
    sys.exit(fd.main())
