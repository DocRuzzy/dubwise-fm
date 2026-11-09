#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""A song slot out over SysEx and back in, through the host simulator and tools/dubwise_slots.py's own framing:
the demo saved to slot 2 and dumped; the image decoded, sent into slot 3; slot 3 loaded; its project checked.
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
turn K1 -2
wait 30
turn K2 1
wait 300
turn K1 -2
wait 30
turn K2 1
wait 30
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
turn K1 -3
wait 30
turn K2 2
wait 30
said LOAD SLOT SLOT 3 (SAVED)
tap SAVE
wait 300
said SLOT 3 LOADED
wait 200
expect ui.tempo 720
expect ui.song.len 32
expect mot_total 40
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
rc = run(B, "slot_put.txt")
print("slot transfer:", "ok" if rc == 0 else "FAIL")
sys.exit(rc)
