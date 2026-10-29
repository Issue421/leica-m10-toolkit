"""Encoder for a Leica M10 ``compr=1`` partial firmware package.

Usage:
    # 1. byte-exact rebuild check on the official package
    python encode.py roundtrip /Users/kai/Downloads/5322416_25_08_01_DefectMapFirmware.fw

    # 2. build the touch-calibration candidate package
    python encode.py build --template <official.fw> \
        --path data/calib/JDI_tp_configuration.bin --file <calib.bin> \
        --name "Touch-Pad-config" --out DefectMap-style-TouchCalib.analysis.bin

Offline only: local files in, local files out. No device, no network.

The container logic (56-byte outer header, 584-byte TOC records, MD5 layout)
mirrors ``package_codec.py`` from publish/leica-m10-open-firmware, which is the
verified implementation for the compr=2 format; only the codec layer differs.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import compr1  # noqa: E402
from decode import (HEADER_LEN, MAGIC, MAGIC2, RECORD_LEN, TOC_END,  # noqa: E402
                    TOC_MAGIC, TOC_OFFSET, TOC_TERMINATOR, decode_file, u32)


def md5(data):
    return hashlib.md5(data).digest()


# ------------------------------------------------------------------ container

def build_plaintext(entries, header_template):
    """Lay out a TOC container: 36-byte header, records, terminator, bodies.

    ``entries`` items need ``record`` (584 B), ``path``, ``body`` and
    ``name``. Records are copied verbatim except for the MD5/offset/length
    triple, exactly as ``package_codec.build_toc`` does for compr=2.
    """
    count = len(entries)
    directory_bytes = count * RECORD_LEN
    base = TOC_OFFSET + directory_bytes + 4

    header = bytearray(header_template[:TOC_OFFSET])
    header[0:4] = struct.pack("<I", TOC_MAGIC)
    header[20:24] = struct.pack("<I", count)
    header[24:28] = struct.pack("<I", directory_bytes)
    header[28:32] = struct.pack("<I", TOC_OFFSET)
    header[32:36] = struct.pack("<I", TOC_END)

    directory = bytearray()
    bodies = bytearray()
    cursor = 0
    for entry in entries:
        record = bytearray(entry["record"])
        record[8:24] = md5(entry["body"])
        record[24:28] = struct.pack("<I", cursor)
        record[28:32] = struct.pack("<I", len(entry["body"]))
        name = entry["name"].encode("ascii")
        path = entry["path"].encode("ascii")
        if len(name) > 20 or len(path) > RECORD_LEN - 72 - 1:
            raise ValueError("name/path too long for a 584-byte record")
        record[52:72] = name.ljust(20, b"\0")
        record[72:] = path.ljust(RECORD_LEN - 72, b"\0")
        directory += record
        bodies += entry["body"]
        cursor += len(entry["body"])

    header[4:20] = md5(bytes(directory))
    plain = bytes(header) + bytes(directory) + struct.pack("<I", TOC_TERMINATOR) + bytes(bodies)
    return plain, base


def build_package(plain, header_template):
    """Wrap plaintext in the 56-byte outer header (compr=1)."""
    payload, stats = compr1.encode(plain)
    header = bytearray(header_template[:HEADER_LEN])
    header[0:4] = struct.pack("<I", MAGIC)
    header[8:12] = struct.pack("<I", u32(header_template, 8))
    header[12:16] = struct.pack("<I", len(payload))
    header[16:20] = struct.pack("<I", len(plain))
    header[20:36] = md5(payload)
    header[36:40] = struct.pack("<I", 4)      # header version
    header[40:44] = struct.pack("<I", 56)     # header extent
    header[44:48] = struct.pack("<I", 1)      # compression = COMP1
    header[48:52] = struct.pack("<I", u32(header_template, 48))
    header[52:56] = struct.pack("<I", MAGIC2)
    return bytes(header) + payload, stats


# --------------------------------------------------------------------- build

def build_candidate(template_path, entries_spec, out_path):
    """Replace the template's component list with ``entries_spec``."""
    decoded = decode_file(template_path)
    template_plain = decoded["plain"]
    template_header = open(template_path, "rb").read()

    entries = []
    for spec in entries_spec:
        if spec.get("from_template") is not None:
            src = decoded["toc"]["entries"][spec["from_template"]]
            entries.append({"record": src["record"], "path": src["path"],
                            "name": src["name"], "body": src["body"]})
        else:
            body = open(spec["file"], "rb").read()
            record = bytearray(spec["record"])
            entries.append({"record": bytes(record), "path": spec["path"],
                            "name": spec["name"], "body": body})

    plain, base = build_plaintext(entries, template_plain)
    package, stats = build_package(plain, template_header)
    with open(out_path, "wb") as fh:
        fh.write(package)
    return package, plain, stats, base


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    rt = sub.add_parser("roundtrip", help="decode+encode an official package")
    rt.add_argument("package")

    bd = sub.add_parser("build", help="build a candidate partial package")
    bd.add_argument("--template", required=True)
    bd.add_argument("--path", required=True)
    bd.add_argument("--file", required=True)
    bd.add_argument("--name", default="")
    bd.add_argument("--out", required=True)

    args = ap.parse_args()

    if args.cmd == "roundtrip":
        raw = open(args.package, "rb").read()
        decoded = decode_file(args.package)
        rebuilt, stats = compr1.encode(decoded["plain"])
        identical = rebuilt == raw[HEADER_LEN:]
        print("payload %d B -> plaintext %d B -> payload %d B"
              % (len(raw) - HEADER_LEN, len(decoded["plain"]), len(rebuilt)))
        print("re-encoded payload byte-identical to the original: %s" % identical)
        print("tokens=%d escaped_tag_bytes=%d"
              % (stats["tokens"], stats["escaped_tag_bytes"]))
        if not identical:
            for i, (a, b) in enumerate(zip(rebuilt, raw[HEADER_LEN:])):
                if a != b:
                    print("first difference at payload offset %d" % i)
                    break
        return 0 if identical else 1

    template = open(args.template, "rb").read()
    decoded = decode_file(args.template)
    # keep the official DEFECT_MAP record layout, retarget path/name/body
    record = bytearray(decoded["toc"]["entries"][0]["record"])
    package, plain, stats, base = build_candidate(
        args.template,
        [{"record": bytes(record), "path": args.path, "name": args.name,
          "file": args.file}],
        args.out)
    print("wrote %s (%d B, plaintext %d B, tokens %d)"
          % (args.out, len(package), len(plain), stats["tokens"]))
    print("sha256 %s" % hashlib.sha256(package).hexdigest())
    return 0


if __name__ == "__main__":
    sys.exit(main())
