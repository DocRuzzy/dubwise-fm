#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""The web sound editor's field table (roadmap 5.13), from the firmware itself: the host simulator's "fields" command
prints every voice field (id, channel, field, name, range, default) as JSON; this writes it into fields.js beside the
page. tests/run_tests.sh runs it with --check: the committed table must match the firmware (no drift).
Usage: python3 -I tools/site/editor/mkfields.py [--check]   (after ./build.sh --host)
"""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HOST = ROOT / "app/build/host/dubwise_host"
OUT = Path(__file__).resolve().parent / "fields.js"


def fields():
    script = ROOT / "app/build/host/fields_dump.txt"
    script.write_text("wait 100\nfields\n")
    r = subprocess.run([str(HOST), str(script), str(ROOT / "app/build/host")], capture_output=True, text=True)
    line = next(l for l in r.stdout.splitlines() if l.startswith("FIELDS "))
    return json.loads(line[len("FIELDS "):])


def main():
    text = ("// Made by mkfields.py from the firmware's own parameter table: don't edit.\n"
            "// Every voice field: id, channel (0-7), field (0-31; 28-31 the MAIN macros), name, min, max, default.\n"
            "const FIELDS = " + json.dumps(fields(), separators=(",", ":")) + ";\n")
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != text:
            print("tools/site/editor/fields.js is stale: run python3 -I tools/site/editor/mkfields.py")
            return 1
        return 0
    OUT.write_text(text)
    print(f"{OUT.relative_to(ROOT)}: {len(json.loads(text.split('= ', 1)[1].rstrip(';\n')))} fields")
    return 0


if __name__ == "__main__":
    sys.exit(main())
