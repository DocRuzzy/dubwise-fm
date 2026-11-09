#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The "first dub" video walkthrough, made from the host simulator: the real firmware's screen and sound, the FM-1's
panel (the browser emulator's own drawing) with the controls lit as they are pressed, and captions. No voice: the
captions are also written as an .srt and a narration script, for a voiceover recorded later.

  python3 tools/video/first_dub.py          (after ./build.sh --host and ./build.sh --web)

Writes app/build/video/: first-dub.mp4, first-dub.srt, first-dub-narration.txt (and its frames, scratch).
Needs ffmpeg, Pillow and Playwright (Chromium) for the panel picture.
"""
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / "app/build/host/dubwise_host"
OUT = ROOT / "app/build/video"
FRAMES = OUT / "frames"
FPS, STEP = 20, 50                               # a frame every 50 ms of the simulator's time
W, H = 1280, 720
FONT = ROOT / "base/assets/fonts/BarlowSemiCondensed-Bold.ttf"
FONT_M = ROOT / "base/assets/fonts/BarlowSemiCondensed-Medium.ttf"

# ---------------------------------------------------------------- the storyboard
# 75 BPM: a beat 800 ms, a bar 3,200. PLAY is let go at 4,060 ms: bar 1 beat 1 just after (T0).
T0, BEAT, BAR = 4065, 800, 3200


def at(bar, beat=1, plus=0):                     # bar and beat from 1
    return T0 + (bar - 1) * BAR + (beat - 1) * BEAT + plus


CAPTIONS = [                                     # (from ms, text)
    (0, ""),
    (3000, "Press PLAY: the ONE DROP riddim starts."),
    (7500, "Eight channels: kick, snare, hats, bass, skank, organ, lead and vox. White keys 1 to 8 are their mutes."),
    (11000, "Tap white key 4: the bass drops out, right on the next bar line."),
    (15000, "Tap it again: the bass comes back on the one."),
    (21000, "The throw: hold white key 10 on beat 3, and the snare goes into the tape echo."),
    (30000, "Black key 1 kicks the spring tank: a thunderclap."),
    (34500, "SELECT is the Big Knob, King Tubby's stepped filter. Turn it up a step a beat: the mix evaporates..."),
    (41500, "...and snap it back on the one."),
    (43500, "Throw the snare, then hold black key 2: the echo freezes, and the tape loops as it is."),
    (47800, "Black key 3 is the dub push: feedback past endless. Let go before it gets too wild."),
    (53500, "Hold black key 11: drum and bass. Let go on the one, and the band is back."),
    (62000, "Black key 6: the siren. The harder you press, the louder."),
    (66500, "Black keys 8 to 10 are one-shots. BIM!"),
    (72000, "KNOB 2 sets how long the repeats ring. Turn it up for a long, deep echo."),
    (78500, "Hold black key 5: drop to wet. The dry band drops out; throw the snare and the skank in, and only the echo and the spring play."),
    (85000, "KNOB 2 back down."),
    (88000, "HOME and PLAY together: the rewind. Pull up, selecta... and come again."),
    (99500, "Press PLAY to stop."),
    (102000, "Now keep it as a song. On the DUB page, press REC (song capture on), then PLAY, and play your moves."),
    (112000, "Bass out on bar 3, back on bar 5, a throw, a crash: every bar and every move is recorded."),
    (124300, "Stop, and press REC again to finish."),
    (127500, "Press HOME for the ARRANGE page. KNOB 3 sets the song to play ONCE. Then PLAY..."),
    (132000, "...and your dub plays back by itself, every move where you made it."),
]
END_MS = 158000
TITLE_UNTIL, OUTRO_FROM = 3000, 151500
NAME = "first-dub"                               # the files' name (another storyboard sets its own: patterns_dub.py)
PRELUDE = ["wait 600", "listen", "master 3400"]  # the host's first lines
TITLE_CARD = ("Your first dub", ["Dubwise FM on the M-VAVE FM-1", "A SpIllt Beats release", "",
                                 "(the real firmware, in its simulator)"])
OUTRO_CARD = ("That's a dub.", ["Next: load RIGHTEOUS DUB (SETUP, DEMO SONG)", "and play along with it.", "",
                                "Cheat sheet: docruzzy.github.io/dubwise-fm/cheat", "Play it in your browser:",
                                "docruzzy.github.io/dubwise-fm"])

EVENTS = []                                      # (ms, host command, cost ms, the control it lights or lets go)


def press(ms, btn, hold=60):
    EVENTS.append((ms, f"press {btn}", 2, ("btn", btn, True)))
    EVENTS.append((ms + hold, f"release {btn}", 2, ("btn", btn, False)))


def key(ms, k, hold=120, vel=100):
    EVENTS.append((ms, f"key {k} down {vel}", 2, ("key", k, True)))
    EVENTS.append((ms + hold, f"key {k} up", 2, ("key", k, False)))


def turn(ms, enc, n, gap=100):
    s = 1 if n > 0 else -1
    for i in range(abs(n)):
        EVENTS.append((ms + i * gap, f"turn {enc} {s} 0", 0, ("knob", enc, True)))


press(4000, "PLAY")
key(at(3, 3), "w3")                              # the bass out at bar 4
key(at(5, 2), "w3")                              # back at bar 6
key(at(7, 3, -60), "w9", 650, 110)              # the snare thrown, twice
key(at(8, 3, -60), "w9", 650, 110)
key(at(10, 1), "b0", 130)                        # the crash
for i in range(6):                               # the Big Knob up a step a beat
    turn(at(11, 1) + i * BEAT, "SELECT", 1)
turn(at(13, 1), "SELECT", -6, 15)                # snapped back on the one
key(at(13, 3, -60), "w9", 600, 110)
key(at(13, 4, 300), "b1", 2300)                  # freeze
key(at(15, 3, -60), "w9", 600, 110)
key(at(15, 3, 500), "b2", 2100)                  # push
key(at(17, 1, -40), "b10", 6400)                 # drum and bass, two bars
key(63000, "b5", 2500, 90)                       # siren
key(at(21, 1, -40), "b7", 100)                   # BIM
key(at(22, 1, -40), "b7", 100)
turn(73000, "K2", 10)                            # long echo
key(at(24, 3, -60), "w9", 650, 110)
key(at(25, 1, -40), "b4", 3200)                  # drop to wet, a bar, with the snare and the skank thrown into it
key(at(25, 1, -30), "w9", 3150, 110)
key(at(25, 1, -20), "w12", 3100, 110)
turn(85500, "K2", -10)
EVENTS.append((90000, "press HOME", 2, ("btn", "HOME", True)))   # the rewind
press(90100, "PLAY")
EVENTS.append((90300, "release HOME", 2, ("btn", "HOME", False)))
press(100500, "PLAY")                            # stop
# the song
press(103000, "REC")
press(105000, "PLAY")
T1 = 105065


def at1(bar, beat=1, plus=0):
    return T1 + (bar - 1) * BAR + (beat - 1) * BEAT + plus


key(at1(2, 3), "w3")                             # the bass out at bar 3
key(at1(4, 3), "w3")                             # back at bar 5
key(at1(5, 3, -60), "w9", 650, 110)
key(at1(6, 1), "b0", 130)
press(at1(7, 1, -800), "PLAY")                   # stop after 6 bars
press(126000, "REC")                             # capture off
press(128000, "HOME")                            # ARRANGE
turn(129800, "K3", 1)                            # SONG: ONCE
press(131000, "PLAY")                            # the song plays itself, 6 bars, and stops


def build_script():
    lines = PRELUDE + [f"wav {OUT.name}/{NAME}.wav"]
    ev = sorted(EVENTS, key=lambda e: e[0])
    j, t = 0, 0
    for f in range(END_MS // STEP):
        start = f * STEP
        lines.append(f"shot {OUT.name}/frames/s{f:05d}.png")
        cost = 0
        while j < len(ev) and ev[j][0] < start + STEP:
            lines.append(ev[j][1])
            cost += ev[j][2]
            j += 1
        lines.append(f"wait {STEP - cost}")
    lines.append("wavstop")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------- the panel
# fm1-base's web/fm1panel.js geometry (viewBox 1000 x 640): where each control is drawn
KNOBS = {"SELECT": (215, 100, 24), "PRESET": (105, 205, 24), "ALGO": (215, 205, 24), "MASTER": (105, 100, 24),
         **{f"K{i + 1}": (610 + i * 104, 100, 22) for i in range(4)}}
BTNS = {"OCT-": (72, 268, 82, 38), "OCT+": (170, 268, 82, 38)}
for i, b in enumerate(["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"]):
    BTNS[b] = (582 + i * 63, 174, 54, 48)
for i, b in enumerate(["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]):
    BTNS[b] = (582 + i * 63, 236, 54, 48)
PITCH = (928 - 24) / 16
KW = PITCH * 0.78
KEYS = {}
for k, a in enumerate([0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]):
    KEYS[f"b{k}"] = (36 + 12 + (a + 1) * PITCH - KW / 2, 340 + 270 * 0.06, KW, 270 * 0.33)
for i in range(16):
    KEYS[f"w{i}"] = (36 + 12 + i * PITCH + (PITCH - KW) / 2, 340 + 270 * 0.43, KW, 270 * 0.52)
SCREEN = (318, 62, 200, 200)


def panel_picture(path):
    """The emulator page's panel drawing, 1000 x 640, its screen left black."""
    js = f"""
const {{ chromium }} = require('playwright');
(async () => {{
  const b = await chromium.launch(); const p = await b.newPage({{ viewport: {{ width: 1100, height: 900 }} }});
  await p.goto('file://{ROOT}/app/build/emu/index.html');
  await p.evaluate(() => {{ document.getElementById('start').hidden = true;
    const d = document.getElementById('device'); d.style.width = '1000px'; d.style.minWidth = '1000px'; }});
  await (await p.$('#device')).screenshot({{ path: '{path}', omitBackground: true }});
  await b.close(); }})();"""
    subprocess.run(["node", "-e", js], check=True, cwd="/tmp")


# ---------------------------------------------------------------- the frames
def wrap(draw, text, font, width):
    words, lines, cur = text.split(), [], ""
    for w in words:
        t = (cur + " " + w).strip()
        if draw.textlength(t, font=font) <= width:
            cur = t
        else:
            lines.append(cur)
            cur = w
    if cur:
        lines.append(cur)
    return lines


def caption_at(ms):
    c = ""
    for t, text in CAPTIONS:
        if t <= ms:
            c = text
    return c


def card(title, lines, splash):
    im = Image.new("RGB", (W, H), (23, 22, 26))
    im.paste(splash.resize((420, 420), Image.NEAREST), (90, 150))
    d = ImageDraw.Draw(im)
    d.text((580, 200), title, font=ImageFont.truetype(str(FONT), 72), fill=(242, 120, 72))
    y = 310
    for ln in lines:
        d.text((580, y), ln, font=ImageFont.truetype(str(FONT_M), 34), fill=(236, 230, 218))
        y += 52
    return im


def compose():
    panel = Image.open(OUT / "panel.png").convert("RGBA")
    sc = 0.76
    panel = panel.resize((int(1000 * sc), int(640 * sc)), Image.LANCZOS)
    px, py = 492, 28
    splash = Image.open(OUT / "splash.png").convert("RGB")
    f_cap = ImageFont.truetype(str(FONT), 34)
    f_small = ImageFont.truetype(str(FONT_M), 22)
    title = card(TITLE_CARD[0], TITLE_CARD[1], splash)
    outro = card(OUTRO_CARD[0], OUTRO_CARD[1], splash)
    held, flash = set(), {}
    ev = sorted(EVENTS, key=lambda e: e[0])
    j = 0
    n = END_MS // STEP
    for f in range(n):
        ms = f * STEP
        while j < len(ev) and ev[j][0] < ms:
            kind, name, down = ev[j][3]
            if kind == "knob":
                flash[name] = ev[j][0]
            elif down:
                held.add((kind, name))
            else:
                held.discard((kind, name))
            j += 1
        if ms < TITLE_UNTIL:
            title.save(FRAMES / f"f{f:05d}.png")
            continue
        if ms >= OUTRO_FROM:
            outro.save(FRAMES / f"f{f:05d}.png")
            continue
        im = Image.new("RGB", (W, H), (23, 22, 26))
        shot = Image.open(FRAMES / f"s{f:05d}.png").convert("RGB")
        im.paste(shot.resize((432, 432), Image.NEAREST), (36, 40))
        d = ImageDraw.Draw(im)
        d.text((36, 482), "DUBWISE", font=ImageFont.truetype(str(FONT), 30), fill=(242, 120, 72))
        d.text((170, 489), "the FM-1's screen", font=f_small, fill=(150, 145, 135))
        layer = panel.copy()
        ld = ImageDraw.Draw(layer)
        s2 = shot.resize((int(SCREEN[2] * sc), int(SCREEN[3] * sc)), Image.BILINEAR)
        layer.paste(s2, (int(SCREEN[0] * sc), int(SCREEN[1] * sc)))
        hot = (242, 120, 72, 255)
        for kind, name in held:
            x, y, w, h = (BTNS if kind == "btn" else KEYS)[name]
            ld.rounded_rectangle([x * sc, y * sc, (x + w) * sc, (y + h) * sc], radius=10 * sc, outline=hot, width=5,
                                 fill=(242, 120, 72, 110))
        for name, t in flash.items():
            if ms - t < 350:
                x, y, r = KNOBS[name]
                ld.ellipse([(x - r - 12) * sc, (y - r - 12) * sc, (x + r + 12) * sc, (y + r + 12) * sc],
                           outline=hot, width=6)
        im.paste(layer, (px, py), layer)
        cap = caption_at(ms)
        if cap:
            lines = wrap(d, cap, f_cap, W - 2 * 40)
            y0 = H - 24 - 44 * len(lines)
            d.rectangle([0, y0 - 18, W, H], fill=(10, 10, 12))
            for i, ln in enumerate(lines):
                d.text((40, y0 + i * 44), ln, font=f_cap, fill=(250, 246, 238))
        im.save(FRAMES / f"f{f:05d}.png")


def srt_time(ms):
    return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"


def write_texts():
    caps = [(t, c) for t, c in CAPTIONS if c]
    srt, nar = [], [f"Dubwise FM: {TITLE_CARD[0].lower()}. Narration script (the captions, with their times).", ""]
    for i, (t, c) in enumerate(caps):
        end = caps[i + 1][0] if i + 1 < len(caps) else OUTRO_FROM
        srt += [str(i + 1), f"{srt_time(t)} --> {srt_time(end)}", c, ""]
        nar.append(f"[{t // 60000}:{t // 1000 % 60:02d}] {c}")
    (OUT / f"{NAME}.srt").write_text("\n".join(srt))
    (OUT / f"{NAME}-narration.txt").write_text("\n".join(nar) + "\n")


def main():
    if FRAMES.exists():
        shutil.rmtree(FRAMES)
    FRAMES.mkdir(parents=True)
    script = OUT / f"{NAME}.txt"
    script.write_text(build_script())
    host_out = ROOT / "app/build"                # (the script's paths are under app/build/video/)
    subprocess.run([str(HOST), str(script), str(host_out)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([str(HOST), "/dev/stdin", str(OUT)], input="splash\nshot splash.png\n", text=True, check=True,
                   stdout=subprocess.DEVNULL)
    panel_picture(OUT / "panel.png")
    compose()
    write_texts()
    mp4 = OUT / f"{NAME}.mp4"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", str(FRAMES / "f%05d.png"),
                    "-i", str(OUT / f"{NAME}.wav"), "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "20",
                    "-preset", "medium", "-c:a", "aac", "-b:a", "192k", "-shortest", str(mp4)], check=True)
    print(f"{mp4}: {mp4.stat().st_size // 1024} KB, {END_MS / 1000:.0f} s")


if __name__ == "__main__":
    sys.exit(main())
