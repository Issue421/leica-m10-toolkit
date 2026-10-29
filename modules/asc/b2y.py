"""Accepted B2Y host composition subset; no firmware output."""
from dataclasses import dataclass
import struct

@dataclass(frozen=True)
class Entry:
    category: int
    offset: int
    size: int
    names: tuple
    slots: bytes
    payload: bytes

@dataclass(frozen=True)
class Resource:
    capacity: int
    entries: tuple
    tail: bytes

    def select(self, category, condition):
        """Firmware semantics: exact strncmp(...,64); last matching entry wins.

        The first matching name within one entry returns the same payload pointer,
        so membership is equivalent without changing the outer-loop last-wins rule.
        """
        key = condition.encode('ascii')
        if len(key) >= 64 or b'\0' in key:
            raise ValueError('host query must be a NUL-terminated <64-byte condition')
        result = None
        for e in self.entries:
            if e.category == category and condition in e.names:
                result = e
        return result

def parse(body, capacity):
    if capacity not in (42, 47) or len(body) < 16:
        raise ValueError('capacity/header')
    magic, version, count, reserved = struct.unpack_from('<4I', body)
    if (magic, version, reserved) != (0xca11bf11, 1, 0) or not 0 < count <= 1000:
        raise ValueError('header')
    stride = 16 + capacity*64
    data_at = 16 + count*stride
    if data_at > len(body):
        raise ValueError('truncated directory')
    entries = []
    for i in range(count):
        at = 16 + i*stride
        cat, offset, size, n = struct.unpack_from('<4I', body, at)
        if not 0 < n <= capacity or offset + size > len(body)-data_at or not size:
            raise ValueError('condition_count/payload range')
        slots = body[at+16:at+stride]
        names = []
        for j in range(n):
            slot = slots[j*64:(j+1)*64]
            if b'\0' not in slot:
                raise ValueError('unterminated condition')
            try:
                names.append(slot.split(b'\0',1)[0].decode('ascii'))
            except UnicodeDecodeError as e:
                raise ValueError('non-ASCII condition') from e
        entries.append(Entry(cat, offset, size, tuple(names), slots,
                             body[data_at+offset:data_at+offset+size]))
    return Resource(capacity, tuple(entries), body[data_at:])

def encode(r, capacity):
    stride = 16 + capacity*64
    out = bytearray(struct.pack('<4I', 0xca11bf11, 1, len(r.entries), 0))
    for e in r.entries:
        if len(e.names) > capacity:
            raise ValueError('cannot shrink active conditions')
        old_slots = e.slots
        if len(old_slots) > capacity*64 and any(old_slots[capacity*64:]):
            raise ValueError('would discard nonzero inactive-slot bytes')
        slots = old_slots[:capacity*64].ljust(capacity*64, b'\0')
        out += struct.pack('<4I', e.category, e.offset, e.size, len(e.names)) + slots
    assert len(out) == 16 + len(r.entries)*stride
    return bytes(out) + r.tail

def hybrid(p, asc):
    """Candidate data composition: ordinary entries intact, explicit ASC only.

    UNKNOWN/default ASC entries are deliberately not substituted for P entries.
    Selecting this data requires code changes documented separately in README.
    """
    entries = list(p.entries)
    tail = bytearray(p.tail)
    for e in asc.entries:
        if any('_FILMMODE:ASC' in n for n in e.names):
            assert all(n in ('_FILMMODE:ASCCLASSIC','_FILMMODE:ASCCONTEMPORARY') for n in e.names)
            entries.append(Entry(e.category,len(tail),e.size,e.names,
                                 e.slots[:42*64],e.payload))
            tail += e.payload
    return Resource(42,tuple(entries),bytes(tail))
