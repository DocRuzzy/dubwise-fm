#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Sound patches over SysEx (roadmap 5.4), through the host simulator and tools/dubwise_slots.py's own framing: a bass
patch saved on the device and dumped; renamed by the tool and put into another slot; read back byte for byte; a patch
for another channel and a damaged one refused; one put into the current sound. Run by tests/run_tests.sh."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import dubwise_slots as D  # noqa: E402

OUT = ROOT / "app/build/host"
HOST = OUT / "dubwise_host"
BASS = 3


def run(script, name):
    p = OUT / name
    p.write_text(script)
    r = subprocess.run([str(HOST), str(p), str(OUT)], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:])
    return r.returncode


def hexf(frame):
    return " ".join(f"{b:02X}" for b in frame)


fail = 0
A = """wait 600
listen
turn ALGO 3
wait 30
tap EDIT
wait 30
turn PRESET 2
wait 30
said BASS STEPPERS
turn K1 10 60
wait 900
press SAVE
wait 20
turn PRESET 1
wait 30
release SAVE
wait 30
said SAVED BASS 01
sysex_log out:patch.syx
sysex %s
wait 100
sysex %s
wait 1000
sysex %s
wait 100
sysex_log off
""" % (hexf(D.patch_frame(0x31, BASS, 0)), hexf(D.patch_frame(0x34, BASS)), hexf(D.patch_frame(0x36, BASS)))
(OUT / "patch.syx").unlink(missing_ok=True)
fail |= run(A, "patch_dump.txt")
raw = (OUT / "patch.syx").read_bytes() if (OUT / "patch.syx").exists() else b""
got = D.patch_replies(raw, BASS)
p = got.get(0)
ok = bool(p) and D.patch_ok(p) and D.patch_name(p) == "BASS 01" and p[1] == BASS
print("dump:", D.patch_info(p) if p else "none", "ok" if ok else "FAIL")
fail |= not ok
ok = got.get("end") and all(got.get(s) is None for s in range(1, 16)) and got.get(0x7F) and D.patch_ok(got[0x7F])
print("library and current:", "ok" if ok else "FAIL")
fail |= not ok
ok = got.get(0x7F) is not None and got[0x7F][12:44] == p[12:44]
print("the current sound is the saved one:", "ok" if ok else "FAIL")
fail |= not ok

q = D.patch_renamed(p, "dub siren")             # (8 characters: "DUB SIRE")
bad = bytearray(q)
bad[20] ^= 0x01                                  # (a field changed under the sum)
B = """wait 600
listen
sysex_log out:patch2.syx
sysex %s
wait 100
sysex %s
wait 100
sysex %s
wait 100
sysex %s
wait 100
sysex %s
wait 100
sysex_log off
expect pl_used4 1
expect pl_used1 0
expect ui.v4.m.tone 10
""" % (hexf(D.patch_frame(0x33, BASS, 4, q)), hexf(D.patch_frame(0x33, BASS, 5, bytes(bad))),
       hexf(D.patch_frame(0x33, 0, 4, q)), hexf(D.patch_frame(0x31, BASS, 4)), hexf(D.patch_frame(0x37, BASS, None, q)))
(OUT / "patch2.syx").unlink(missing_ok=True)
fail |= run(B, "patch_put.txt")
raw = (OUT / "patch2.syx").read_bytes() if (OUT / "patch2.syx").exists() else b""
answers = [f[4] for f in D.frames_of(raw) if len(f) > 6 and f[1:4] == D.HDR and f[4] in (0x15, 0x16)]
ok = answers == [0x16, 0x15, 0x15, 0x16]
print("put, damaged, wrong channel, current:", [hex(a) for a in answers], "ok" if ok else "FAIL")
fail |= not ok
back = D.patch_replies(raw, BASS).get(4)
ok = back == q and D.patch_name(back) == "DUB SIRE"
print("read back:", D.patch_info(back) if back else "none", "ok" if ok else "FAIL")
fail |= not ok
sys.exit(1 if fail else 0)
