"""Decoder for a Leica M10 ``compr=1`` partial firmware package.

Usage:
    python decode.py <package.bin> [--out plaintext.bin] [--json decode.json]

Offline only: reads one local file, writes local files. No device, no network.

Format summary (derived from the official 48,069 B DEFECT_MAP patch):

Outer header (56 bytes, identical layout to the compr=2 full packages):
    +0x00 u32 magic        0x1E1CAF3E
    +0x04 u8  version[4]   e.g. 99,99,99,99 for the partial patch
    +0x08 u32 type         0x0CA1BDA7 (partial/defect-map bundle type)
    +0x0C u32 payload_len  len(raw) - 56
    +0x10 u32 expand       plaintext length after decompression
    +0x14 16B md5          MD5 of raw[56:]
    +0x24 u32 header_ver   4
    +0x28 u32 extent       56 (header length)
    +0x2C u32 compression  1 = COMP1, 2 = COMP
    +0x30 u32 hw_id        321 (M10), 323 (M10-P)
    +0x34 u32 magic2       0xE1E350C1

Payload (COMP1):
    +0  byte 0xAA          stream tag
    +1  ...                LZ token stream (see compr1.py)

Plaintext: the same TOC container the compr=2 packages carry.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compr1  # noqa: E402

MAGIC = 0x1E1CAF3E
MAGIC2 = 0xE1E350C1
TOC_MAGIC = 0x70C4EAD3
TOC_END = 0x8F3B152C
TOC_TERMINATOR = 0xDA7ADA7A
HEADER_LEN = 56
TOC_OFFSET = 36
RECORD_LEN = 584


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def parse_outer(raw):
    if len(raw) < HEADER_LEN:
        raise ValueError("file shorter than the 56-byte header")
    if u32(raw, 0) != MAGIC:
        raise ValueError("outer magic 0x%08X != 0x%08X" % (u32(raw, 0), MAGIC))
    if u32(raw, 0x34) != MAGIC2:
        raise ValueError("outer magic2 mismatch")
    header = {
        "file_bytes": len(raw),
        "version": list(raw[4:8]),
        "type": u32(raw, 8),
        "payload_len": u32(raw, 0x0C),
        "expand": u32(raw, 0x10),
        "md5_payload": raw[0x14:0x24].hex(),
        "header_version": u32(raw, 0x24),
        "extent": u32(raw, 0x28),
        "compression": u32(raw, 0x2C),
        "hardware": u32(raw, 0x30),
        "md5_ok": hashlib.md5(raw[HEADER_LEN:]).digest() == raw[0x14:0x24],
    }
    if header["payload_len"] + HEADER_LEN != len(raw):
        raise ValueError("payload length does not match the file size")
    return header


def parse_toc(plain):
    if u32(plain, 0) != TOC_MAGIC:
        raise ValueError("TOC magic mismatch")
    if u32(plain, 32) != TOC_END:
        raise ValueError("TOC end magic mismatch")
    count, size, off = u32(plain, 20), u32(plain, 24), u32(plain, 28)
    if off != TOC_OFFSET or size != count * RECORD_LEN:
        raise ValueError("TOC dimensions %d/%d/%d" % (count, size, off))
    if u32(plain, off + size) != TOC_TERMINATOR:
        raise ValueError("TOC terminator mismatch")
    directory_md5_ok = hashlib.md5(plain[off:off + size]).digest() == plain[4:20]
    if not directory_md5_ok:
        raise ValueError("TOC directory MD5 mismatch")

    base = off + size + 4
    entries = []
    for i in range(count):
        rec = plain[off + i * RECORD_LEN: off + (i + 1) * RECORD_LEN]
        relative, length = struct.unpack_from("<II", rec, 24)
        body = plain[base + relative: base + relative + length]
        if len(body) != length:
            raise ValueError("record %d body runs past the payload" % i)
        body_md5_ok = hashlib.md5(body).digest() == rec[8:24]
        entries.append({
            "index": i,
            "name": rec[52:72].split(b"\0")[0].decode("ascii", "replace"),
            "path": rec[72:].split(b"\0")[0].decode("ascii", "replace"),
            "offset": base + relative,
            "relative": relative,
            "bytes": length,
            "md5": rec[8:24].hex(),
            "md5_ok": body_md5_ok,
            "sha256": hashlib.sha256(body).hexdigest(),
            "flags_00": rec[0],
            "flags_01": rec[1],
            "meta_02": struct.unpack_from("<H", rec, 2)[0],
            "meta_04": u32(rec, 4),
            "required_hw": u32(rec, 32),
            "field_36": u32(rec, 36),
            "field_40": u32(rec, 40),
            "field_44": u32(rec, 44),
            "field_48": u32(rec, 48),
            "record": rec,
            "body": body,
        })
    tail = plain[base + sum(e["bytes"] for e in entries) + 0:]
    return {
        "count": count,
        "directory_bytes": size,
        "offset": off,
        "base": base,
        "directory_md5_ok": directory_md5_ok,
        "entries": entries,
        "trailing_bytes": len(plain) - (base + sum(e["bytes"] for e in entries)),
    }


def decode_file(path):
    raw = open(path, "rb").read()
    header = parse_outer(raw)
    if header["compression"] != 1:
        raise ValueError("compression=%d, this decoder handles compr=1 only"
                         % header["compression"])
    payload = raw[HEADER_LEN:]
    plain, stats = compr1.decode(payload)
    if len(plain) != header["expand"]:
        raise ValueError("decoded %d bytes, header says expand=%d"
                         % (len(plain), header["expand"]))
    toc = parse_toc(plain)
    return {"header": header, "stats": stats, "plain": plain, "toc": toc}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("--out")
    ap.add_argument("--json")
    args = ap.parse_args()

    result = decode_file(args.package)
    header, toc, plain = result["header"], result["toc"], result["plain"]

    print("outer    : magic ok, version=%s type=0x%08X hw=%d hdrver=%d extent=%d compr=%d"
          % (header["version"], header["type"], header["hardware"],
             header["header_version"], header["extent"], header["compression"]))
    print("md5      : %s (%s)" % (header["md5_payload"], "ok" if header["md5_ok"] else "BAD"))
    print("expand   : %d (decoded %d, %s)"
          % (header["expand"], len(plain),
             "match" if len(plain) == header["expand"] else "MISMATCH"))
    print("tokens   : %d back-references, %d escapes, %d literal bytes"
          % (result["stats"]["tokens"], result["stats"]["escapes"],
             result["stats"]["literals"]))
    print("toc      : %d records, directory MD5 %s"
          % (toc["count"], "ok" if toc["directory_md5_ok"] else "BAD"))
    for e in toc["entries"]:
        print("  [%d] %-20s %-52s %7d B  rel=%-6d md5=%s hw=%d"
              % (e["index"], e["name"], e["path"], e["bytes"], e["relative"],
                 "ok" if e["md5_ok"] else "BAD", e["required_hw"]))

    if args.out:
        with open(args.out, "wb") as fh:
            fh.write(plain)
        print("wrote plaintext ->", args.out)
    if args.json:
        doc = {
            "header": header,
            "stats": result["stats"],
            "toc": {k: v for k, v in toc.items() if k != "entries"},
            "entries": [{k: v for k, v in e.items() if k not in ("record", "body")}
                        for e in toc["entries"]],
        }
        with open(args.json, "w") as fh:
            json.dump(doc, fh, indent=2)
        print("wrote report ->", args.json)


if __name__ == "__main__":
    main()
