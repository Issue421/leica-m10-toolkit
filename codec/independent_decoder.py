"""Independent accepted Python decoder subset; no builder import or device code."""
from pathlib import Path
import hashlib
import os
import stat
import struct
MAX_BYTES=256*1024*1024

class InvalidData(ValueError):
    pass

def require(ok, reason):
    if not ok:
        raise InvalidData(reason)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def regular_bytes(path, limit=MAX_BYTES):
    """Bounded regular files only: no device/FIFO/socket or symlink opening."""
    path = Path(path)
    require(stat.S_ISREG(path.lstat().st_mode), 'regular non-symlink file required')
    fd = os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0) | getattr(os, 'O_NONBLOCK', 0))
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and 0 < before.st_size <= limit, 'file size/type bound')
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns), 'file changed during read')
    require(len(data) == before.st_size, 'short or overlong file read')
    return data

def u32(data, offset):
    require(0 <= offset <= len(data) - 4, 'u32 extent')
    return struct.unpack_from('<I', data, offset)[0]

def _cstring(data, fixed_width=False):
    # Labels are a fixed 20-byte field and can occupy all 20 bytes. Paths retain
    # a required terminator; neither kind is ever used as a host filesystem path.
    require(fixed_width or b'\0' in data, 'unterminated TOC text')
    try:
        return data.split(b'\0', 1)[0].decode('ascii')
    except UnicodeError as exc:
        raise InvalidData('TOC non-ASCII metadata') from exc

def decode_comp(raw, progress=None):
    """MJS COMP core translated to bounded in-memory Python, no target execution."""
    require(len(raw) >= 536 and raw[56:60] == b'COMP' and u32(raw, 44) == 2, 'COMP header')
    packed, expanded = u32(raw, 12), u32(raw, 16)
    require(56 + packed == len(raw) and 0 < expanded <= MAX_BYTES, 'COMP length bound')
    require(hashlib.md5(raw[56:]).digest() == raw[20:36], 'COMP outer MD5')
    permutation = raw[0x8c:0x18c]
    require(len(set(permutation)) == 256, 'COMP byte permutation')
    values, lengths = bytearray(65536), bytearray(65536)
    codes = []
    for category in range(16):
        n = struct.unpack_from('<H', raw, 0x6c + 2 * category)[0]
        code = struct.unpack_from('<H', raw, 0x4c + 2 * category)[0]
        if n:
            require(n <= 16 and category <= 8 and n + category <= 16, 'COMP prefix bound')
            codes.append((category, n, code))
    for word in range(65536):
        for category, n, code in codes:
            if word >> (16 - n) == code >> (16 - n):
                v = ((word >> (16 - n - category)) & ((1 << category) - 1)) if category else 0
                if category and v < 1 << (category - 1):
                    v -= (1 << category) - 1
                values[word], lengths[word] = permutation[(128 + v) & 255], n + category
                break
        require(lengths[word] != 0, 'incomplete COMP prefix code')
    # Decode all symbols wholly contained in each 16-bit prefix in one step.
    # Zero padding is only used for lookahead; every consumed bit is bounded.
    emits, takes = [], bytearray(65536)
    for word in range(65536):
        consumed, out = 0, bytearray()
        while consumed < 16:
            current = (word << consumed) & 65535
            n = lengths[current]
            if consumed + n > 16:
                break
            out.append(values[current])
            consumed += n
        emits.append(bytes(out))
        takes[word] = consumed
    count = u32(raw, 0x190)
    require(1 <= count <= 64 and raw[0x194:0x198] == b'PMOC', 'COMP block directory')
    require(0x198 + count * 16 <= len(raw), 'COMP directory extent')
    previous, result, blocks = 0x198 + count * 16 - 56, bytearray(), []
    for i in range(count):
        reserved, relative, size, want = struct.unpack_from('<4I', raw, 0x198 + i * 16)
        require(reserved == 0 and relative == previous and size > 0 and want > 0 and
                56 + relative + size <= len(raw) and len(result) + want <= expanded, 'COMP block extent')
        src = raw[56 + relative:56 + relative + size] + b'\0\0'
        block, bit = bytearray(), 0
        extend = block.extend
        while len(block) < want:
            at, shift = bit >> 3, bit & 7
            require(at < size, 'truncated COMP block')
            word = (((src[at] << 16) | (src[at + 1] << 8) | src[at + 2]) >> (8 - shift)) & 65535
            emit, n = emits[word], takes[word]
            if len(block) + len(emit) > want or bit + n > size * 8:
                emit, n = bytes((values[word],)), lengths[word]
            require(bit + n <= size * 8, 'truncated COMP symbol')
            extend(emit)
            bit += n
        require(size * 8 - bit <= (256 if i == count - 1 else 31), 'COMP trailing data bound')
        blocks.append({'index': i, 'bytes': len(block), 'consumed_bits': bit, 'padding_bits': size * 8 - bit,
                       'sha256': sha(block)})
        result.extend(block)
        previous = relative + size
        if progress:
            progress('decoded COMP block %d/%d' % (i + 1, count))
    require(len(result) == expanded and previous == packed, 'COMP payload coverage')
    return result, blocks

def inspect_toc(payload):
    require(len(payload) >= 40, 'TOC payload size')
    require(u32(payload, 0) == 0x70c4ead3, 'TOC payload magic')
    require(u32(payload, 32) == 0x8f3b152c, 'TOC start marker')
    count, size, off = u32(payload, 20), u32(payload, 24), u32(payload, 28)
    require(1 <= count <= 4096 and size == count * 584 and off == 36 and off + size + 4 <= len(payload),
            'TOC dimensions')
    require(u32(payload, off + size) == 0xda7ada7a, 'TOC terminator')
    require(hashlib.md5(payload[off:off + size]).digest() == payload[4:20], 'TOC directory MD5')
    base, result, spans = off + size + 4, [], []
    for i in range(count):
        record = payload[off + i * 584:off + (i + 1) * 584]
        relative, size = u32(record, 24), u32(record, 28)
        start, end = base + relative, base + relative + size
        require(size > 0 and base <= start < end <= len(payload), 'TOC component extent')
        body = payload[start:end]
        require(hashlib.md5(body).digest() == record[8:24], 'TOC component MD5')
        result.append({'id': str(i), 'name': _cstring(record[52:72], fixed_width=True), 'path': _cstring(record[72:]),
                       'bytes': size, 'payload_offset': start, 'md5': record[8:24].hex(), 'sha256': sha(body),
                       'component_type': u32(record, 0), 'selector': u32(record, 32),
                       'write_mode': u32(record, 40), 'serial_constraint': u32(record, 44)})
        spans.append((start, end))
    spans.sort()
    require(all(a[1] <= b[0] for a, b in zip(spans, spans[1:])), 'TOC component overlap')
    return result
