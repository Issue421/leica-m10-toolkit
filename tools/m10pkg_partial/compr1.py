"""compr=1 ("COMP1") codec for Leica M10 partial firmware packages.

Derived offline from the official DEFECT_MAP patch package
(5322416_25_08_01_DefectMapFirmware.fw). No device, no network.

Stream layout (all integers little-endian unless stated):

    offset 0: 0xAA                         ; stream tag (not part of output)
    offset 1: LZ token stream ...

Token encoding:
    literal byte   b != 0xAA               -> copy b to output
    0xAA 0x00                              -> literal 0xAA  (escape, L == 0)
    0xAA <L> <D>                           -> back-reference: copy L bytes
                                              from output[len(out)-D]

    <L> and <D> are big-endian base-128 varints (7 bits per byte, high bit
    = continuation). L is at least 1 for a back-reference; D is at least 1.
    In the official package 4 <= L <= 512 and 4 <= D <= 952.
"""
from __future__ import annotations

TAG = 0xAA


def read_varint(data, i):
    """Big-endian base-128 varint. Returns (value, next_index)."""
    value = 0
    while True:
        if i >= len(data):
            raise ValueError("truncated varint at %d" % i)
        byte = data[i]
        i += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, i


def write_varint(value):
    if value < 0:
        raise ValueError("negative varint")
    out = bytearray([value & 0x7F])
    value >>= 7
    while value:
        out.append(0x80 | (value & 0x7F))
        value >>= 7
    return bytes(reversed(out))


def decode(data, start=1):
    """Decode a COMP1 stream to plaintext.

    ``start`` skips the leading tag byte(s); the official package uses 1.
    Returns (plaintext, stats).
    """
    out = bytearray()
    i = start
    literals = 0
    token_count = 0
    escaped = 0
    while i < len(data):
        byte = data[i]
        if byte != TAG:
            out.append(byte)
            i += 1
            literals += 1
            continue
        i += 1
        length, i = read_varint(data, i)
        if length == 0:
            out.append(TAG)
            escaped += 1
            continue
        distance, i = read_varint(data, i)
        if distance == 0 or distance > len(out):
            raise ValueError("bad distance %d at output %d" % (distance, len(out)))
        src = len(out) - distance
        token_count += 1
        for k in range(length):
            out.append(out[src + k])
    return bytes(out), {
        "literals": literals,
        "tokens": token_count,
        "escapes": escaped,
        "input_bytes": len(data) - start,
        "output_bytes": len(out),
    }


# --------------------------------------------------------------------- encode

MIN_MATCH = 4
MAX_MATCH = 512
MAX_DISTANCE = 952


def _token(length, distance, tag=TAG):
    return bytes([tag]) + write_varint(length) + write_varint(distance)


def encode(plain, min_match=MIN_MATCH, max_match=MAX_MATCH,
           max_distance=MAX_DISTANCE, tag=TAG):
    """Encode plaintext to a COMP1 stream (tag byte prepended).

    The official encoder is a greedy LZSS pass that only accepts
    *non-overlapping* back-references (L <= D), longest match wins, the
    smallest distance breaks ties. Reproducing that policy bit-for-bit is
    what makes encode(decode(x)) == x hold on the official package.
    """
    out = bytearray([tag])
    n = len(plain)
    pos = 0
    literal_start = 0
    stats = {"tokens": 0, "literal_bytes": 0}

    def flush_literals(upto):
        nonlocal literal_start
        while literal_start < upto:
            byte = plain[literal_start]
            if byte == tag and literal_start != upto:
                out.append(tag)
                out.append(0)
            else:
                out.append(byte)
            literal_start += 1

    while pos < n:
        best_len = 0
        best_dist = 0
        limit = min(pos, max_distance)
        for dist in range(1, limit + 1):
            # non-overlapping: the match may not run past the source window
            cap = min(dist, max_match)
            k = 0
            while k < cap and pos + k < n and plain[pos + k] == plain[pos + k - dist]:
                k += 1
            if k > best_len:
                best_len, best_dist = k, dist
        # the official encoder never emits a token that does not shrink the
        # stream: a token costs 1 + len(varint(L)) + len(varint(D)) bytes and
        # must be strictly cheaper than the L literal bytes it replaces.
        profitable = (best_len >= min_match and
                      len(_token(best_len, best_dist)) < best_len)
        if profitable:
            flush_literals(pos)
            out += _token(best_len, best_dist)
            stats["tokens"] += 1
            pos += best_len
            literal_start = pos
        else:
            pos += 1
    flush_literals(n)
    stats["escaped_tag_bytes"] = out.count(bytes([tag, 0]))
    stats["output_bytes"] = len(out)
    return bytes(out), stats
