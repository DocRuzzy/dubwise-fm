#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
"""Dubwise FM's song slots to and from a computer, over USB-MIDI (SysEx; app/slots.c has the protocol).

  python3 tools/dubwise_slots.py get SLOT FILE [--port NAME]    a slot (1-4) from the FM-1 into FILE (.dws)
  python3 tools/dubwise_slots.py put SLOT FILE [--port NAME]    FILE into a slot (stop the FM-1 first)
  python3 tools/dubwise_slots.py info FILE                      what a file holds
  python3 tools/dubwise_slots.py patch get CH SLOT FILE          a user patch (channel 1-8, slot 1-16) into FILE (.dwpatch)
  python3 tools/dubwise_slots.py patch put CH SLOT FILE          FILE into a user slot (stop the FM-1 first)
  python3 tools/dubwise_slots.py patch list CH                   a channel's 16 user slots, by name
  python3 tools/dubwise_slots.py patch current CH FILE           the channel's sound as it is now, into FILE
  python3 tools/dubwise_slots.py patch name FILE NAME            rename a .dwpatch file (8 characters)

A .dwpatch file is one sound patch (48 bytes, roadmap 5.4; app/patches.c has the layout): one channel's voice.
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
MAGIC = 0x334C5344                                # "DSL3" (5.8, 5.9: step locks, repeats, chance, seeds)
MAGIC2 = 0x324C5344                               # "DSL2" (the session, 4.8: still loads)
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
    if magic not in (MAGIC, MAGIC2, MAGIC1) or length + 16 != len(image):
        return "not a Dubwise slot file"
    tempo = struct.unpack_from("<h", image, 16)[0]  # parameter 0 (P_TEMPO), BPM x 10
    return f"a Dubwise project: {tempo / 10:.1f} BPM, {moves} recorded moves, {len(image)} bytes"


PATCH_LEN = 48
CHANNELS = ["KICK", "SNARE", "HATS", "BASS", "SKANK", "ORGAN", "LEAD", "VOX"]


def crc16(data):                                  # CRC-16/CCITT-FALSE, as app/patches.c
    c = 0xFFFF
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
            c &= 0xFFFF
    return c


def patch_ok(p):
    return len(p) == PATCH_LEN and p[3] & 1 and struct.unpack_from("<H", p, 44)[0] == crc16(p[:44])


def patch_name(p):
    return p[4:12].split(b"\0")[0].decode("ascii", "replace")


def patch_renamed(p, name):
    """The patch with a new name (A-Z, 0-9, space - . '), its sum made again."""
    name = "".join(c for c in name.upper() if c.isalnum() or c in " -.'")[:8]
    p = bytearray(p)
    p[4:12] = name.encode("ascii").ljust(8, b"\0")
    struct.pack_into("<H", p, 44, crc16(p[:44]))
    return bytes(p)


def patch_info(p):
    if not patch_ok(p):
        return "not a Dubwise patch (or damaged)"
    return f"a Dubwise patch for {CHANNELS[p[1]] if p[1] < 8 else '?'}: {patch_name(p)} (version {p[0]})"


def patch_frame(cmd, ch, slot=None, patch=None):
    body = [cmd, ch] + ([slot] if slot is not None else [])
    return bytes([0xF0, *HDR, *body]) + (pack7(patch) if patch else b"") + b"\xF7"


def patch_replies(raw, ch):
    """The patches in a stream of replies: {slot: bytes or None (empty)}; 'end' when a channel's dump finished."""
    got = {}
    for f in frames_of(raw):
        if len(f) < 7 or f[1:4] != HDR or f[5] != ch:
            continue
        if f[4] == 0x32:
            got[f[6]] = unpack7(f[7:-1])[:PATCH_LEN]
        elif f[4] == 0x13:
            got[f[6]] = None
        elif f[4] == 0x12:
            got["end"] = True
        elif f[4] in (0x15, 0x16):
            got["ok"] = f[4] == 0x16
    return got


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
    ap.add_argument("cmd", choices=["get", "put", "info", "patch"])
    ap.add_argument("a")
    ap.add_argument("b", nargs="?")
    ap.add_argument("c", nargs="?")
    ap.add_argument("d", nargs="?")
    ap.add_argument("--port")
    o = ap.parse_args(argv)
    if o.cmd == "patch":
        return patch_main(o)
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


def ask(out, inp, frame, ch, until, secs=5):
    send(out, frame)
    raw, end = bytearray(), time.time() + secs
    while time.time() < end:
        for m in inp.iter_pending():
            if m.type == "sysex":
                raw += bytes([0xF0, *m.data, 0xF7])
        got = patch_replies(bytes(raw), ch)
        if until(got):
            return got
        time.sleep(0.005)
    return patch_replies(bytes(raw), ch)


def patch_main(o):
    sub = o.a
    if sub == "name":
        p = open(o.b, "rb").read()
        open(o.b, "wb").write(patch_renamed(p, o.c or ""))
        print(patch_info(open(o.b, "rb").read()))
        return 0
    if sub == "info":
        print(patch_info(open(o.b, "rb").read()))
        return 0
    ch = int(o.b or 0) - 1
    if not 0 <= ch < 8:
        sys.exit("CH is 1-8 (KICK SNARE HATS BASS SKANK ORGAN LEAD VOX)")
    inp, out = open_ports(o.port)
    if sub == "list":
        got = ask(out, inp, patch_frame(0x34, ch), ch, lambda g: "end" in g, 10)
        for s in range(16):
            p = got.get(s)
            print(f"{s + 1:2}  {patch_name(p) if p and patch_ok(p) else '-'}")
    elif sub == "current":
        got = ask(out, inp, patch_frame(0x36, ch), ch, lambda g: 0x7F in g)
        p = got.get(0x7F)
        if not p or not patch_ok(p):
            sys.exit("no answer")
        open(o.c, "wb").write(p)
        print(f"{CHANNELS[ch]} now -> {o.c}")
    elif sub in ("get", "put"):
        slot = int(o.c or 0) - 1
        if not 0 <= slot < 16 or not o.d:
            sys.exit("SLOT is 1-16, and FILE is needed")
        if sub == "get":
            got = ask(out, inp, patch_frame(0x31, ch, slot), ch, lambda g: slot in g)
            p = got.get(slot)
            if not p or not patch_ok(p):
                sys.exit(f"{CHANNELS[ch]} {slot + 1}: empty" if slot in got else "no answer")
            open(o.d, "wb").write(p)
            print(f"{CHANNELS[ch]} {slot + 1} -> {o.d}: {patch_info(p)}")
        else:
            p = open(o.d, "rb").read()
            if not patch_ok(p) or p[1] != ch:
                sys.exit(f"{o.d}: {patch_info(p)}; it goes on channel {p[1] + 1 if len(p) > 1 else '?'}")
            got = ask(out, inp, patch_frame(0x33, ch, slot, p), ch, lambda g: "ok" in g)
            if not got.get("ok"):
                sys.exit("refused (stop the FM-1 first, or the file is damaged)" if "ok" in got else "no answer")
            print(f"{o.d} -> {CHANNELS[ch]} {slot + 1}: kept")
    else:
        sys.exit("patch get | put | list | current | name | info")
    return 0


if __name__ == "__main__":
    sys.exit(main())
