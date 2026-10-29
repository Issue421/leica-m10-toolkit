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

## 3. Why the version field is `[99,99,99,99]`

Partial packages carry a placeholder version rather than a real firmware version.
The camera's admission check compares the package version against the installed
one; a calibration-style package is not a firmware upgrade and must not be ranked
against it, so a saturated placeholder is used.

This document deliberately stops at describing the container. It does not
describe how to author a partial package, and the tooling to do so is not part
of this repository. If you want to *read* a partial package you can already own,
`codec/` decodes the outer header and TOC.

## 4. Licensing and redistribution

The toolchain here is MIT (see the repository `LICENSE`). It contains no firmware
bytes, no extracted resources and no keys. Any package you build from your own local
images is yours to handle; this repository neither ships nor hosts one.
