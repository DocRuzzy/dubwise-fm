#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Your own one-shots (roadmap 5.14; app/shots.c) through the host simulator, with the tools' own code: three WAVs (a
440 Hz sine, 16-bit 44.1 kHz; a click, 8-bit; noise, 24-bit stereo 48 kHz) through make_oneshots.py into an image, put
over SysEx with dubwise_slots.py's framing (kept: 66), each played (its pitch, its length), kept over a reboot, dumped
back byte for byte; the loader's bytes (0xF00..0xF07 of every sector) left erased. Then: a put cut off halfway (after
a reboot the factory shots play), a put while playing (refused: 65), a damaged image (refused), the factory back by
SysEx 54 and by SETUP's ONE SHOTS (asked twice). Run by tests/run_tests.sh (after the build)."""
import math
import random
import struct
import subprocess
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import dubwise_slots as D  # noqa: E402
import make_oneshots as M  # noqa: E402

OUT = ROOT / "app/build/host"
HOST = OUT / "dubwise_host"
fails = 0


def check(ok, what):
    global fails
    print(("  ok " if ok else "FAIL ") + what)
    fails += not ok


def run(script, name):
    p = OUT / name
    p.write_text(script)
    r = subprocess.run([str(HOST), str(p), str(OUT)], capture_output=True, text=True)
    bad = [line for line in r.stdout.splitlines() if "FAIL" in line]
    print("\n".join(bad) or f"  ok {name}")
    return r.returncode


def write_wav(path, rate, width, ch, xs):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(width)
        w.setframerate(rate)
        raw = bytearray()
        for x in xs:
            for _ in range(ch):
                v = max(-1.0, min(1.0, x))
                if width == 1:
                    raw.append(int(round(v * 127)) + 128)
                elif width == 2:
                    raw += struct.pack("<h", int(round(v * 32767)))
                else:
                    raw += int(round(v * 8388607)).to_bytes(3, "little", signed=True)
        w.writeframes(bytes(raw))


def hexs(frame):
    return " ".join("%02X" % b for b in frame)


def put_lines(frames):
    return "".join(f"sysex {hexs(f)}\nwait 5\n" for f in frames)


random.seed(5)
write_wav(OUT / "shot_sine.wav", 44100, 2, 1, [0.5 * math.sin(2 * math.pi * 440 * i / 44100) for i in range(int(0.3 * 44100))])
write_wav(OUT / "shot_click.wav", 22050, 1, 1, [0.8 if i < 20 else 0.0 for i in range(int(0.05 * 22050))])
write_wav(OUT / "shot_noise.wav", 22050, 3, 2, [0.3 * (random.random() * 2 - 1) for _ in range(33075 - 6615 - 1102)])
shots = [(n, M.user_shot(OUT / f)) for n, f in (("tag", "shot_sine.wav"), ("click", "shot_click.wav"),
                                                ("noise", "shot_noise.wav"))]
image = M.user_image(shots)
print(M.image_info(image))
lens = [len(d) for _, d in shots]
check(abs(lens[0] - 6615) <= 2 and lens[1] == 1102 and sum(lens) <= 33075, f"lengths at 22,050 Hz {lens}")
try:                                              # (one byte over the 1.5 s: refused by the tool)
    M.user_image([("a", b"\x80" * 33076)])
    check(False, "an image over 1.5 s refused by the tool")
except SystemExit:
    check(True, "an image over 1.5 s refused by the tool")
frames = D.shots_frames(image)

A = f"""wait 600
listen
param ch8.echo 0
sysex_out
{put_lines(frames)}expect_sysex F0 7D 44 57 66 F7
expect shots_user 1
expect shot_len1 {lens[0]}
expect shot_len2 {lens[1]}
expect shot_len3 {lens[2]}
expect shots_holes 1
wav shot-tag.wav
tapkey b7
wait 40
said ONE SHOT TAG
expect ch8_db >-40
wait 400
expect ch8_db <-80
wavstop
tapkey b8
wait 20
said ONE SHOT CLICK
tapkey b9
wait 20
said ONE SHOT NOISE
wait 600
reboot
wait 600
expect shots_user 1
expect shot_len1 {lens[0]}
sysex_log out:shots.syx
sysex F0 7D 44 57 51 F7
wait 8000
sysex_log off
"""
(OUT / "shots.syx").unlink(missing_ok=True)
rc = run(A, "shots_put.txt")
check(rc == 0, "put (the whole 1.5 s, across all nine sectors), played, kept over a reboot")
w = subprocess.run([sys.executable, str(ROOT / "tests/wavstat.py"), str(OUT / "shot-tag.wav"), "peak<-1", "dc<-40",
                    "zc_hz>430", "zc_hz<450"], capture_output=True, text=True)
print(w.stdout.strip())
check(w.returncode == 0, "the sine shot plays at 440 Hz (resampled from 44.1 kHz)")
raw = (OUT / "shots.syx").read_bytes() if (OUT / "shots.syx").exists() else b""
d = D.ShotsDump()
for f in D.frames_of(raw):
    d.take(f)
check(d.done == image, "dumped back byte for byte (%s)" % (len(d.done) if isinstance(d.done, bytes) else d.done))

bad = bytearray(image)
bad[200] ^= 0x55
B = f"""wait 600
{put_lines(frames)}expect shots_user 1
sysex_out
{put_lines(frames[:len(frames) // 2])}reboot
wait 600
expect shots_user 0
expect shot_len1 7056
listen
tapkey b7
wait 30
said ONE SHOT BIM
wait 600
sysex_out
{put_lines(D.shots_frames(bytes(bad)))}expect_sysex F0 7D 44 57 65 F7
expect shots_user 0
sysex_out
{put_lines(frames)}expect_sysex F0 7D 44 57 66 F7
expect shots_user 1
tap PLAY
wait 200
sysex_out
sysex {hexs(frames[0])}
wait 20
expect_sysex F0 7D 44 57 65 F7
sysex F0 7D 44 57 54 F7
wait 20
expect_sysex F0 7D 44 57 65 F7
expect shots_user 1
tap PLAY
wait 500
sysex_out
sysex F0 7D 44 57 54 F7
wait 20
expect_sysex F0 7D 44 57 66 F7
expect shots_user 0
expect shot_len1 7056
sysex_out
sysex F0 7D 44 57 51 F7
wait 200
expect_sysex F0 7D 44 57 63 F7
listen
tap GLO
wait 30
turn K1 26
wait 30
said ONE SHOTS FACTORY
turn K2 1
wait 30
said FACTORY ALREADY
{put_lines(frames)}expect shots_user 1
turn K1 1
turn K1 -1
wait 30
said ONE SHOTS YOURS
turn K2 1
wait 30
said FACTORY SHOTS AGAIN TO CONFIRM
expect shots_user 1
turn K2 1
wait 30
said ONE SHOTS FACTORY
expect shots_user 0
expect shot_len3 11025
"""
check(run(B, "shots_cut.txt") == 0, "cut off: the factory's; damaged and playing refused; factory by 54 and by SETUP "
      "(asked twice)")
sys.exit(1 if fails else 0)
