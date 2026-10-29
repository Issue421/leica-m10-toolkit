"""Fast COMP1 (Leica private LZSS) encoder.

Bit-compatible with ``compr1.encode`` but replaces the O(n*d) brute-force
match search with a hash-chain search, so multi-megabyte payloads encode in
seconds instead of hours.

Policy kept identical to the reference encoder:
  * non-overlapping back-references only (L <= D),
  * greedy: longest match wins, smallest distance breaks ties,
  * a token is emitted only when it is strictly shorter than the literals
    it replaces,
  * a literal 0xAA byte is escaped as (0xAA, 0x00).

Round-trip correctness is checked against ``compr1.decode`` by the caller.
"""
from __future__ import annotations

TAG = 0xAA
MIN_MATCH = 4
MAX_MATCH = 512
MAX_DISTANCE = 952

_HASH_BITS = 16
_HASH_SIZE = 1 << _HASH_BITS
_HASH_MASK = _HASH_SIZE - 1
# a token costs 1 tag byte + 1 len byte + 1..2 dist bytes
_MAX_CHAIN = 512


def _hash3(data, i):
    return ((data[i] << 10) ^ (data[i + 1] << 5) ^ data[i + 2]) & _HASH_MASK


def _write_varint(value, out):
    buf = bytearray([value & 0x7F])
    value >>= 7
    while value:
        buf.append(0x80 | (value & 0x7F))
        value >>= 7
    out += bytes(reversed(buf))


def _token_len(length, distance):
    """Encoded size of a back-reference token."""
    n = 1
    v = length
    n += 1
    v >>= 7
    while v:
        n += 1
        v >>= 7
    v = distance
    n += 1
    v >>= 7
    while v:
        n += 1
        v >>= 7
    return n


def encode(plain, min_match=MIN_MATCH, max_match=MAX_MATCH,
           max_distance=MAX_DISTANCE, tag=TAG):
    out = bytearray([tag])
    n = len(plain)
    pos = 0
    literal_start = 0
    tokens = 0
    escaped = 0

    def flush_literals(upto):
        nonlocal literal_start, escaped
        while literal_start < upto:
            b = plain[literal_start]
            out.append(b)
            if b == tag:
                out.append(0)
                escaped += 1
            literal_start += 1

    # head[h] = most recent position with that hash, prev[p] = previous one
    head = [-1] * _HASH_SIZE
    prev = [0] * (n + 1)

    def insert(p):
        if p + 2 < n:
            h = _hash3(plain, p)
            prev[p] = head[h]
            head[h] = p

    while pos < n:
        best_len = 0
        best_dist = 0
        if pos + min_match <= n:
            limit = min(pos, max_distance)
            if limit > 0:
                h = _hash3(plain, pos)
                cand = head[h]
                chain = 0
                cap_overall = min(max_match, n - pos)
                while cand >= 0 and chain < _MAX_CHAIN:
                    d = pos - cand
                    if d > max_distance:
                        break
                    # non-overlapping constraint: match may not exceed d
                    cap = d if d < cap_overall else cap_overall
                    if cap > best_len:
                        k = 0
                        while k < cap and plain[cand + k] == plain[pos + k]:
                            k += 1
                        if k > best_len:
                            best_len = k
                            best_dist = d
                            if best_len >= cap_overall:
                                break
                    cand = prev[cand]
                    chain += 1

        profitable = (best_len >= min_match and
                      _token_len(best_len, best_dist) < best_len)
        if profitable:
            flush_literals(pos)
            out.append(tag)
            _write_varint(best_len, out)
            _write_varint(best_dist, out)
            tokens += 1
            for p in range(pos, min(pos + best_len, n)):
                insert(p)
            pos += best_len
            literal_start = pos
        else:
            insert(pos)
            pos += 1

    flush_literals(n)
    return bytes(out), {
        "tokens": tokens,
        "escaped_tag_bytes": escaped,
        "output_bytes": len(out),
    }
