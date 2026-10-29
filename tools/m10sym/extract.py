#!/usr/bin/env python3
"""Extract embedded debug-symbol name tables from a Leica M10 system image.

Format of an entry (there are ~30 small tables scattered through the image):

    u32  ptr     = 0x42000000 + file_offset_of_target + 1
                   (low bit = ARM/Thumb interworking bit; mask it off)
    char name[]  NUL terminated, padded with zeros to 4-byte alignment

Why naive scans miss it: the high byte of every pointer is 0x42, which is
ASCII 'B'. Scanning for printable strings therefore starts one byte late.
And you cannot test `ptr == base + offset` because of the +1 Thumb bit.

Accepted entries either chain to a neighbour (next entry begins at the
4-byte-aligned end of this name) or point at a plausible function prologue.

Offline tool: reads one local file.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys

IMAGE_BASE = 0x42000000
ADDR_LO = 0x42000000
ADDR_HI = 0x43000000


def _name(buf: bytes, pos: int, limit: int = 96) -> str:
    end = pos
    while end < len(buf) and end - pos < limit and 0x20 <= buf[end] < 0x7F:
        end += 1
    return buf[pos:end].decode("ascii", "replace")


def _ident(s: str) -> bool:
    if len(s) < 3:
        return False
    if not (s[0].isalpha() or s[0] == "_"):
        return False
    return all(c.isalnum() or c in "_." for c in s)


def _is_prologue(buf: bytes, t: int) -> bool:
    """Function prologue at file offset t.

    Thumb PUSH {...,LR} is the halfword 0xB5xx, which little-endian means the
    *second* byte is 0xB5. (Checking the first byte finds nothing, because that
    is just the register mask.) ARM PUSH is 0xE92Dxxxx, again little-endian, so
    the bytes on disk start with the register list and `2d e9` follows.
    """
    if not (0 <= t < len(buf) - 3):
        return False
    if buf[t + 1] == 0xB5:            # Thumb push {.., lr}
        return True
    if buf[t + 1] == 0xB4:            # Thumb push {..} without lr
        return True
    if buf[t + 2:t + 4] == b"\xe9\x2d":   # ARM push {.., lr}
        return True
    if buf[t:t + 4] == b"\x04\xe0\x2d\xe5":  # ARM push {lr}
        return True
    return False


def extract(buf: bytes):
    c = {}
    for fo in range(0, len(buf) - 8, 4):
        ptr = struct.unpack_from("<I", buf, fo)[0]
        if not (ADDR_LO <= ptr < ADDR_HI):
            continue
        t = (ptr & ~1) - IMAGE_BASE
        if not (0 <= t < len(buf)):
            continue
        nm = _name(buf, fo + 4)
        if not nm or not _ident(nm):
            continue
        c[fo] = (ptr, nm, t)

    seen, chains = set(), []
    for fo in sorted(c):
        if fo in seen:
            continue
        ch, cur = [], fo
        while cur in c and cur not in seen:
            seen.add(cur)
            ch.append(cur)
            cur = (cur + 4 + len(c[cur][1]) + 3) & ~3
        chains.append(ch)

    out = set()
    for ch in chains:
        if len(ch) >= 2:
            for f in ch:
                out.add(c[f])
        elif _is_prologue(buf, c[ch[0]][2]):
            out.add(c[ch[0]])
    return sorted(out, key=lambda x: (x[1], x[0]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--filter", default=None)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--save", default=None)
    a = ap.parse_args()
    with open(a.image, "rb") as fh:
        buf = fh.read()
    e = extract(buf)
    if a.filter:
        e = [x for x in e if a.filter.lower() in x[1].lower()]
    if a.save:
        with open(a.save, "w") as fh:
            for ptr, nm, t in e:
                fh.write("%08X %06X %s\n" % (ptr & ~1, t, nm))
    if a.json:
        print(json.dumps([{"va": p & ~1, "tfo": t, "name": n} for p, n, t in e], indent=2))
    else:
        print("# %s : %d symbols" % (a.image, len(e)))
        for ptr, nm, t in e:
            print("%08X  tfo=%06X  %s" % (ptr & ~1, t, nm))
    return 0


if __name__ == "__main__":
    sys.exit(main())
