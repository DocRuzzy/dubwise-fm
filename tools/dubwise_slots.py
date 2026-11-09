#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Dubwise FM's song slots to and from a computer, over USB-MIDI (SysEx; app/slots.c has the protocol).

  python3 tools/dubwise_slots.py get SLOT FILE [--port NAME]    a slot (1-4) from the FM-1 into FILE (.dws)
  python3 tools/dubwise_slots.py put SLOT FILE [--port NAME]    FILE into a slot (stop the FM-1 first)
  python3 tools/dubwise_slots.py info FILE                      what a file holds

A .dws file is the slot exactly as the FM-1's flash holds it: a whole project (the desk, sounds, key, patterns, scenes,
the song and its moves). Share it; anyone with Dubwise can put it into a slot and LOAD SLOT it (SETUP).
Needs mido with python-rtmidi (pip install mido python-rtmidi), as base/tools/fm1_install.py does.
"""
import argparse
import struct
import sys
import time

HDR = bytes([0x7D, 0x44, 0x57])                   # non-commercial, "DW"
PIECE = 256
MAGIC = 0x324C5344                                # "DSL2" (the session, 4.8)
MAGIC1 = 0x314C5344                               # "DSL1" (before it: still loads)


def pack7(data):
    out = bytearray()
    for i in range(0, len(data), 7):
        chunk = data[i:i + 7]
        top = 0
        for k, b in enumerate(chunk):
            top |= (b >> 7) << k
        out.append(top)
        out.extend(b & 0x7F for b in chunk)
    return bytes(out)


def unpack7(data):
    out, i = bytearray(), 0
    while i < len(data):
        top = data[i]
        i += 1
        for k in range(7):
            if i >= len(data):
                break
            out.append(data[i] | ((top >> k) & 1) << 7)
            i += 1
    return bytes(out)


def dump_request(slot):                           # slot 0-3
    return bytes([0xF0, *HDR, 0x01, slot, 0xF7])


def put_frames(slot, image):
    frames = []
    for idx, off in enumerate(range(0, len(image), PIECE)):
        frames.append(bytes([0xF0, *HDR, 0x21, slot, idx & 0x7F, idx >> 7 & 0x7F]) + pack7(image[off:off + PIECE]) + b"\xF7")
    n = len(image)
    frames.append(bytes([0xF0, *HDR, 0x22, slot, n & 0x7F, n >> 7 & 0x7F, n >> 14 & 0x7F, 0xF7]))
    return frames


def frames_of(raw):
    """Split a byte stream into whole SysEx frames (F0 .. F7)."""
    i = 0
    while True:
        a = raw.find(b"\xF0", i)
        if a < 0:
            return
        b = raw.find(b"\xF7", a)
        if b < 0:
            return
        yield raw[a:b + 1]
        i = b + 1


class Dump:
    """Collects a dump's frames into the image; done is True (image) or an error string."""

    def __init__(self, slot):
        self.slot, self.parts, self.done = slot, {}, None

    def take(self, f):
        if len(f) < 7 or f[1:4] != HDR or f[5] != self.slot:
            return
        cmd = f[4]
        if cmd == 0x11:
            idx = f[6] | f[7] << 7
            self.parts[idx] = unpack7(f[8:-1])
        elif cmd == 0x12:
            n = f[6] | f[7] << 7 | f[8] << 14
            image = b"".join(self.parts[i] for i in sorted(self.parts))
            self.done = image if len(image) == n and sorted(self.parts) == list(range(len(self.parts))) else "incomplete"
        elif cmd == 0x13:
            self.done = "the slot is empty"


def info(image):
    magic, length, _sum, moves = struct.unpack_from("<IIII", image, 0)
    if magic not in (MAGIC, MAGIC1) or length + 16 != len(image):
        return "not a Dubwise slot file"
    tempo = struct.unpack_from("<h", image, 16)[0]  # parameter 0 (P_TEMPO), BPM x 10
    return f"a Dubwise project: {tempo / 10:.1f} BPM, {moves} recorded moves, {len(image)} bytes"


def open_ports(name):
    import mido
    ins, outs = mido.get_input_names(), mido.get_output_names()
    pick = lambda ns: next((n for n in ns if (name or "DUBWISE") in n.upper() or (name and name in n)), None)
    i, o = pick(ins), pick(outs)
    if not i or not o:
        sys.exit(f"no FM-1 running Dubwise found (ports: {ins}); try --port NAME")
    return mido.open_input(i), mido.open_output(o)


def send(out, frame):
    import mido
    out.send(mido.Message("sysex", data=list(frame[1:-1])))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["get", "put", "info"])
    ap.add_argument("a")
    ap.add_argument("b", nargs="?")
    ap.add_argument("--port")
    o = ap.parse_args(argv)
    if o.cmd == "info":
        print(info(open(o.a, "rb").read()))
        return 0
    slot = int(o.a) - 1
    if not 0 <= slot < 4 or not o.b:
        sys.exit("SLOT is 1-4, and FILE is needed")
    inp, out = open_ports(o.port)
    if o.cmd == "get":
        d = Dump(slot)
        send(out, dump_request(slot))
        end = time.time() + 20
        while d.done is None and time.time() < end:
            for m in inp.iter_pending():
                if m.type == "sysex":
                    d.take(bytes([0xF0, *m.data, 0xF7]))
            time.sleep(0.005)
        if not isinstance(d.done, bytes):
            sys.exit(f"slot {slot + 1}: {d.done or 'no answer'}")
        open(o.b, "wb").write(d.done)
        print(f"slot {slot + 1} -> {o.b}: {info(d.done)}")
    else:
        image = open(o.b, "rb").read()
        print(info(image))
        for f in put_frames(slot, image):
            send(out, f)
            time.sleep(0.02)                      # (a flash write a piece: let it keep up)
        end, ok = time.time() + 5, None
        while ok is None and time.time() < end:
            for m in inp.iter_pending():
                if m.type == "sysex" and bytes(m.data[:3]) == HDR and m.data[4] == slot and m.data[3] in (0x15, 0x16):
                    ok = m.data[3] == 0x16
            time.sleep(0.01)
        if not ok:
            sys.exit(f"slot {slot + 1}: refused (stop the FM-1 first, or the file is damaged)" if ok is False else "no answer")
        print(f"{o.b} -> slot {slot + 1}: kept. On the FM-1: SETUP, LOAD SLOT {slot + 1}, then SAVE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
