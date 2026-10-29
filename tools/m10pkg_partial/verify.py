"""Independent verifier for the candidate ``compr=1`` partial package.

This module deliberately re-implements the container and codec checks from
scratch instead of calling ``decode.py``/``compr1.py``: if a bug existed in the
decoder it must not be able to vouch for itself.

Usage:
    python verify.py <package.bin> --calib <JDI_tp_configuration.bin> \
        [--official <5322416...fw>]

Exit code 0 = every check passed.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys

MAGIC = 0x1E1CAF3E
MAGIC2 = 0xE1E350C1
TOC_MAGIC = 0x70C4EAD3
TOC_END = 0x8F3B152C
TOC_TERMINATOR = 0xDA7ADA7A
HEADER_LEN = 56
RECORD_LEN = 584

EXPECTED_TYPE = 0x0CA1BDA7
EXPECTED_VERSION = [99, 99, 99, 99]
EXPECTED_HW = 321
EXPECTED_HDRVER = 4
EXPECTED_EXTENT = 56
EXPECTED_COMPRESSION = 1

RESULTS = []


def check(name, ok, detail=""):
    RESULTS.append((name, bool(ok), detail))
    print("[%s] %-46s %s" % ("PASS" if ok else "FAIL", name, detail))
    return ok


# ------------------------------------------------------------------ codec

def decompress(payload):
    """Plain COMP1 decoder, written independently of compr1.py."""
    if payload[0] != 0xAA:
        raise ValueError("stream tag byte is 0x%02X, expected 0xAA" % payload[0])
    out = bytearray()
    i = 1
    n = len(payload)
    while i < n:
        if payload[i] != 0xAA:
            out.append(payload[i])
            i += 1
            continue
        i += 1
        length = 0
        while True:
            if i >= n:
                raise ValueError("truncated length varint")
            byte = payload[i]
            i += 1
            length = (length << 7) | (byte & 0x7F)
            if not byte & 0x80:
                break
        if length == 0:
            out.append(0xAA)
            continue
        dist = 0
        while True:
            if i >= n:
                raise ValueError("truncated distance varint")
            byte = payload[i]
            i += 1
            dist = (dist << 7) | (byte & 0x7F)
            if not byte & 0x80:
                break
        if not 1 <= dist <= len(out):
            raise ValueError("back-reference distance %d invalid at %d"
                             % (dist, len(out)))
        src = len(out) - dist
        for k in range(length):
            out.append(out[src + k])
    return bytes(out)


# ------------------------------------------------------------------ checks

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package")
    ap.add_argument("--calib", required=True)
    ap.add_argument("--official")
    args = ap.parse_args()

    raw = open(args.package, "rb").read()
    calib = open(args.calib, "rb").read()

    def u32(data, off):
        return struct.unpack_from("<I", data, off)[0]

    # ---- outer header
    check("outer magic", u32(raw, 0) == MAGIC, "0x%08X" % u32(raw, 0))
    check("outer magic2", u32(raw, 52) == MAGIC2, "0x%08X" % u32(raw, 52))
    check("version == [99,99,99,99]", list(raw[4:8]) == EXPECTED_VERSION,
          str(list(raw[4:8])))
    check("type == 0x0CA1BDA7", u32(raw, 8) == EXPECTED_TYPE,
          "0x%08X" % u32(raw, 8))
    check("hardware == 321", u32(raw, 48) == EXPECTED_HW, str(u32(raw, 48)))
    check("header version == 4", u32(raw, 36) == EXPECTED_HDRVER,
          str(u32(raw, 36)))
    check("header extent == 56", u32(raw, 40) == EXPECTED_EXTENT,
          str(u32(raw, 40)))
    check("compression == 1", u32(raw, 44) == EXPECTED_COMPRESSION,
          str(u32(raw, 44)))

    # ---- lengths + MD5
    payload = raw[HEADER_LEN:]
    check("len field == file size - 56", u32(raw, 12) == len(payload),
          "%d vs %d" % (u32(raw, 12), len(payload)))
    stored_md5 = raw[20:36]
    actual_md5 = hashlib.md5(payload).digest()
    check("outer MD5 covers raw[56:]", stored_md5 == actual_md5,
          stored_md5.hex())

    # ---- codec
    try:
        plain = decompress(payload)
        check("COMP1 decompression", True, "%d -> %d bytes"
              % (len(payload), len(plain)))
    except Exception as exc:                       # noqa: BLE001
        check("COMP1 decompression", False, str(exc))
        return report()

    check("decoded length == expand", len(plain) == u32(raw, 16),
          "decoded %d, expand %d" % (len(plain), u32(raw, 16)))

    # ---- TOC
    check("TOC magic", u32(plain, 0) == TOC_MAGIC, "0x%08X" % u32(plain, 0))
    check("TOC end magic", u32(plain, 32) == TOC_END, "0x%08X" % u32(plain, 32))
    count, size, off = u32(plain, 20), u32(plain, 24), u32(plain, 28)
    check("TOC dimensions", size == count * RECORD_LEN and off == 36,
          "count=%d size=%d off=%d" % (count, size, off))
    check("TOC terminator", u32(plain, off + size) == TOC_TERMINATOR,
          "0x%08X" % u32(plain, off + size))
    directory = plain[off:off + size]
    check("TOC directory MD5", hashlib.md5(directory).digest() == plain[4:20],
          hashlib.md5(directory).hexdigest())

    base = off + size + 4
    records = []
    for i in range(count):
        rec = plain[off + i * RECORD_LEN: off + (i + 1) * RECORD_LEN]
        rel, length = struct.unpack_from("<II", rec, 24)
        body = plain[base + rel: base + rel + length]
        records.append((rec, rel, length, body))

    # ---- the single component
    check("exactly one TOC record", count == 1, str(count))
    if count == 1:
        rec, rel, length, body = records[0]
        path = rec[72:].split(b"\0")[0].decode("ascii", "replace")
        name = rec[52:72].split(b"\0")[0].decode("ascii", "replace")
        check("path string", path == "data/calib/JDI_tp_configuration.bin", path)
        check("component name", name == "Touch-Pad-config", name)
        check("component length == 424", length == 424, str(length))
        check("component offset == TOC base", rel == 0, str(rel))
        check("component MD5", hashlib.md5(body).digest() == rec[8:24],
              hashlib.md5(body).hexdigest())
        check("embedded body sha256 == source file",
              hashlib.sha256(body).hexdigest() == hashlib.sha256(calib).hexdigest(),
              hashlib.sha256(body).hexdigest())
        check("embedded body == source bytes", body == calib, "")
        check("calib magic +0x00 == 0x27092009",
              struct.unpack_from("<I", body, 0)[0] == 0x27092009,
              "0x%08X" % struct.unpack_from("<I", body, 0)[0])
        check("calib magic +0x1A0 == 0x09111976",
              struct.unpack_from("<I", body, 0x1A0)[0] == 0x09111976,
              "0x%08X" % struct.unpack_from("<I", body, 0x1A0)[0])
        check("required_hw field == 321", u32(rec, 32) == 321, str(u32(rec, 32)))
        check("no trailing bytes after component",
              len(plain) == base + length, "%d vs %d" % (len(plain), base + length))

    # ---- the stream must decode to itself through a re-encode
    sys.path.insert(0, __file__.rsplit("/", 1)[0])
    import compr1
    reencoded, _ = compr1.encode(plain)
    check("re-encode(decode(payload)) reproduces plaintext",
          decompress(reencoded) == plain, "%d bytes" % len(reencoded))
    check("re-encoded payload byte-identical", reencoded == payload,
          "sha256 %s" % hashlib.sha256(reencoded).hexdigest())

    # ---- regression: the official package must still rebuild byte-exactly
    if args.official:
        official = open(args.official, "rb").read()
        off_plain = decompress(official[HEADER_LEN:])
        off_reenc, _ = compr1.encode(off_plain)
        check("official package rebuild is byte-identical",
              off_reenc == official[HEADER_LEN:],
              "%d bytes" % len(off_reenc))
        check("official plaintext length == its expand field",
              len(off_plain) == u32(official, 16),
              "%d vs %d" % (len(off_plain), u32(official, 16)))

    return report()


def report():
    failed = [r for r in RESULTS if not r[1]]
    print("\n%d checks, %d passed, %d failed"
          % (len(RESULTS), len(RESULTS) - len(failed), len(failed)))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
