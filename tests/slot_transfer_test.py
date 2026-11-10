#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""A song slot out over SysEx and back in, through the host simulator and tools/dubwise_slots.py's own framing:
the demo saved to slot 2 and dumped; the image decoded, sent into slot 3; slot 3 loaded; its project checked. The
same image as a slot from before 5.8 (DSL2, a kick step's gate set): loaded with its drum gates cleared.
Run by tests/run_tests.sh (after the build)."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import dubwise_slots as D  # noqa: E402

OUT = ROOT / "app/build/host"
HOST = OUT / "dubwise_host"


def run(script, name):
    p = OUT / name
    p.write_text(script)
    r = subprocess.run([str(HOST), str(p), str(OUT)], capture_output=True, text=True)
    print(r.stdout[-2000:])
    return r.returncode


A = """wait 600
listen
tap GLO
wait 30
turn K1 -3
wait 30
turn K2 1
wait 30
turn K2 1
wait 300
turn K1 -3
wait 30
turn K2 1
wait 30
tap SAVE
wait 300
tap SAVE
wait 300
said SLOT 2 SAVED
sysex_log out:slot.syx
sysex F0 7D 44 57 01 01 F7
wait 2000
sysex_log off
sysex F0 7D 44 57 01 02 F7
wait 100
"""
(OUT / "slot.syx").unlink(missing_ok=True)
rc = run(A, "slot_dump.txt")
raw = (OUT / "slot.syx").read_bytes() if (OUT / "slot.syx").exists() else b""
d = D.Dump(1)
for f in D.frames_of(raw):
    d.take(f)
ok = isinstance(d.done, bytes)
print("dump:", D.info(d.done) if ok else d.done)
if rc or not ok:
    sys.exit(1)
lines = ["wait 600", "listen"] + ["sysex " + f.hex(" ").upper() + "\nwait 5" for f in D.put_frames(2, d.done)]
B = "\n".join(lines) + """
wait 100
tap GLO
wait 30
turn K1 -5
wait 30
turn K2 2
wait 30
said LOAD SLOT SLOT 3 (SAVED)
tap SAVE
wait 300
said SLOT 3 LOADED
wait 200
expect ui.tempo 720
expect ui.song.len 41
expect mot_total 111
expect scene_used1 1
"""
# a damaged image is refused (the slot stays empty)
bad = bytearray(d.done)
bad[len(bad) // 2] ^= 0x55
B += "\n".join("sysex " + f.hex(" ").upper() + "\nwait 5" for f in D.put_frames(3, bytes(bad))) + """
wait 100
turn K2 1
wait 30
said LOAD SLOT SLOT 4 (EMPTY)
"""
# a slot saved before 5.8 (DSL2): a kick step's gate (a live-recorded length) reads as no locks once loaded
import struct  # noqa: E402
old = bytearray(d.done)
STEPS = 16 + 512 * 2 + 8 * 4 * 4                # (the header, the parameters, the patterns' settings: the kick's A)
SCENES = STEPS + 8 * 4 * 64 * 4                 # the scenes: 16 of 152 bytes since 8.1 (DSL5), 112 before (no EQ)
body = old[:SCENES]
for sc in range(16):
    body += old[SCENES + 152 * sc:SCENES + 152 * sc + 112]
body += old[SCENES + 16 * 152:]
old = bytearray(body)
struct.pack_into("<I", old, 4, len(old) - 16)
k = next(i for i in range(64) if old[STEPS + 4 * i] & 3)
old[STEPS + 4 * k + 3] = 37
h = 2166136261
for b in old[16:]:
    h = ((h ^ b) * 16777619) & 0xFFFFFFFF
struct.pack_into("<I", old, 0, D.MAGIC2)
struct.pack_into("<I", old, 8, h)
B += "\n".join("sysex " + f.hex(" ").upper() + "\nwait 5" for f in D.put_frames(3, bytes(old))) + f"""
wait 100
turn K2 -1
wait 30
turn K2 1
wait 30
said LOAD SLOT SLOT 4 (SAVED)
tap SAVE
wait 300
tap SAVE
wait 300
said SLOT 4 LOADED
wait 300
expect step1_{k + 1} >0
expect gate1_{k + 1} 0
expect lockt1_{k + 1} 0
"""
rc = run(B, "slot_put.txt")
print("slot transfer:", "ok" if rc == 0 else "FAIL")
sys.exit(rc)
