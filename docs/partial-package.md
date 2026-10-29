# Partial (`compr=1`) package toolchain

Everything needed to reproduce, inspect and build a Leica M10 **calibration-style
partial firmware package** — the small `.FW` files that carry one or two data files
instead of a whole system image.

No firmware bytes are distributed with this repository. The tools operate on images
and packages you supply locally, and every integrity layer is verified before a
package is considered usable.

---

## 1. Two package kinds

| | Full package | Partial package |
|---|---|---|
| outer type | `0x07E1EA5E` | `0x0CA1BDA7` |
| compression | `2` | `1` |
| version | real version, e.g. `3.22.23.38` | `[99,99,99,99]` |
| hardware | model-specific | `321` (M10) |
| carries | the whole system image | a few data files |
| version gate | **rejected if package < camera** | **skipped entirely** |

The version-gate asymmetry is the important part. The updater's comparison lives at
`0x131F3C`; the partial branch is taken when the record type equals `0x0CA1BDA7`,
and that branch never performs the `<` comparison. This is the manufacturer's own
calibration-patch mechanism: it is version-agnostic and re-flashable.

## 2. Container layout

Outer header, 56 bytes:

```
+0   u32  0x1E1CAF3E   magic
+4   u8[4] version
+8   u32  type
+12  u32  len - 56
+16  u32  expanded length
+20  u8[16] MD5 of raw[56:]
+36  u32  header version (4)
+40  u32  56            header extent
+44  u32  compression
+48  u32  hardware
+52  u32  0xE1E350C1   magic2
```

TOC, at offset 36 of the decoded payload:

```
+0   u32  0x70C4EAD3   TOC magic
+4   u8[16] MD5
+20  u32  record count N
+24  N * 584 bytes of records
+... u32  0xDA7ADA7A   terminator
     bodies
```

Record, 584 bytes:

```
+0x00  8   meta
+0x08  16  MD5 of the body
+0x18  u32 relative offset of the body
+0x1C  u32 body length
+0x20  u32 hardware
+0x24  u32 (not consulted by the record loop)
+0x28  u32 mode
+0x2C  u32 serial, 0 = wildcard
+0x34  20  name
+0x48  512 path
```

Mode dispatch at `0x132934` decides what happens to a record:

| mode | target | effect |
|---|---|---|
| `0x30000` | `0x132A1C` | — |
| `0x100` | `0x13296A` | write to NOR |
| `0x120` | `0x1329BC` | **write a file** |
| `0x100000` | `0x132A54` | battery-gated path |
| `0x200000` | `0x132A56` | WLAN component |

`mode=0x120` is the one that matters for data files. It requires record `+0x20` to be
the model's hardware id (`0x141` = 321 for M10), passes when `+0x2C` is zero
(wildcard), and then calls `0x42193BB4` with the path and length. **There is no path
whitelist.** Success is defined as the returned length matching `+0x1C`.

> `0x42193BB4` is **ARM** code, not Thumb: `push {r3,r4,r5,r6,r7,lr}`. Disassembling
> it as Thumb produces nonsense. It is a 17-instruction open/write/close wrapper.

## 3. The codec (`compr=1`)

`compr=1` is a private LZSS variant, not zlib and not LZO. The reference decoder is
`decode.py`; the fast encoder is `compr1_fast.py`.

```
tag 0xAA starts a token
  length byte, distance as 1 or 2 bytes depending on magnitude
a literal 0xAA byte is escaped as (0xAA, 0x00)
non-overlapping back-references only (L <= D)
greedy: longest match wins, smallest distance breaks ties
```

`compr1.py` is the straightforward O(n·d) reference encoder — correct but slow
(~40 s for 256 KB, so hours for a 9.8 MB image). `compr1_fast.py` replaces the
brute-force search with a hash chain and encodes 9.8 MB in about 86 seconds while
producing **byte-identical output** to the official packages on their own payloads.

Correctness is established by round-trip: encode, decode, compare MD5 with the input.

> `compr1.decode()` returns a **tuple** `(bytes, stats)`. Treating it as bytes is a
> common mistake and shows up as `decoded len 2 / MISMATCH`.

## 4. Workflow

```sh
# inspect any package (prints header, MD5, expand size, every TOC record)
python3 decode.py <package.fw>

# prove the codec on an official package: rebuild must be byte-exact
python3 encode.py roundtrip <official-partial.fw>

# build a candidate
python3 encode.py build --template <official-partial.fw> \
    --path data/calib/JDI_tp_configuration.bin \
    --file <your-file.bin> \
    --name "Touch-Pad-config" \
    --out candidate.analysis.bin

# verify every layer (--calib is the file you intended to ship)
python3 verify.py candidate.analysis.bin --calib <your-file.bin>
```

## 5. What verification must cover

A package is only usable when **all** of these pass. `verify.py` checks them:

1. `magic` and `magic2` are the expected constants.
2. `len - 56` equals the actual body length.
3. `MD5(raw[56:])` matches header `+20`.
4. Declared expanded length matches the decoded length.
5. TOC magic, count, terminator and end magic are intact.
6. Each record's MD5 matches its body.
7. Re-encoding the decoded payload reproduces the file byte for byte.
8. Hardware id and mode are what the target accepts.

Skipping step 3 or 7 is how a package that "looks fine" bricks an update.

## 6. Field notes worth knowing before you build anything

- **`/system/system.bin` is written with `mode=0x120`**, not `0x200000`. In the 3.22
  full package, record 264 is `system.bin`, length 9,824,064, hardware `0x141`,
  mode `0x120`, which is the same file as the extracted image.
- **No official package uses `mode=0x100` at all.** If you see it in a draft record,
  the draft is wrong.
- **Filename discovery is not in `system.bin`.** The update filename is resolved by
  the bootloader, and it looks for a fixed official name. A package under an
  unrecognised name may simply never be scanned. Only a name the bootloader already
  knows will be picked up.
- **`version=[99,99,99,99]`** (`0x63636363`) is the calibration convention. It passes
  every gate on unknown versions and permits unlimited re-flash.
- **`mode=0x200000` is a WLAN component**, and the function it reaches
  (`0x42100A60`) has a gate on `[+0x11] == 1` that looks like a
  production/authorisation flag. Do not assume it is a general-purpose file writer.
- **M10 has no SD-card recovery mode.** A `system.bin` that fails to boot means
  factory service. Data files under `/data` are a much smaller risk than `/system`.

## 7. Licensing and redistribution

The toolchain here is MIT (see the repository `LICENSE`). It contains no firmware
bytes, no extracted resources and no keys. Any package you build from your own local
images is yours to handle; this repository neither ships nor hosts one.
