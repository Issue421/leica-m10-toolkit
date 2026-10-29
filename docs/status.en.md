# Where this stands, and what is missing

This document answers three questions:

1. What **actually works** — verified on the author's own M10, with the author's own inputs.
2. What **only works on that one machine** — because it depends on that camera's
   calibration, that owner's official resources, that firmware version.
3. What is **still missing** for it to work on *any* M10.

The third item is why this repository exists.

---

## 0. Scope, stated plainly

**This repository contains no firmware writer.** No flashing path, no device
protocol sender, no way to produce a `.FW`. That is not because it is impossible;
it is because the responsible order of operations is to settle the approach in
public first, and let each owner act on their own body.

Everything here is one of two things: **analysis, validation, and offline
recomputation of files you already own.** You can use it to read a package,
test a hypothesis, or compute a crop parameter. It will not touch your camera.

---

## 1. What already works

Each item below has been demonstrated on real hardware — **the author's hardware.**
That is why each is worth continuing, and also why none of them is yet a shared
solution.

### 1.1 XPan framing (65:24)

The crop arithmetic is fully worked out, including the geometry and where the
values are written.

- All three plane pointers move by rows: Y base `+896×cfg[+0x0c]`, Cb and Cr
  bases each `+448×cfg[+0x0e]` (`448 = 896/2`, because chroma is vertically
  subsampled 2:1)
- Target height written to `cfg[+0x12]` (`used_h`, 3992 → 2224); the encoded
  width is always `cfg[+0x10]` (`used_w`, 6016). `cfg[+0x14]` is not a dimension —
  it is the 1-byte `ycc_smpl` chroma-sampling selector

**How far it got**: the arithmetic and patch bytes are complete; the patch was
applied to a self-supplied image (58 differing bytes, same length) and packaged
into a structurally valid firmware, `xpan-system-3.22.fw` — **45 of 45 container
checks pass**, including "273 unmodified components are byte-identical to the
official package" and "`re-encode(decode(payload))` reproduces the whole package".
That package is 127,550,092 B, 28 B larger than the official one; the delta is
compression-ratio drift from re-encoding, not a structural error. But it has
**never been flashed to a body**.

This repository contains the container-level splicing primitive
(`tools/targeted_swap.py`, which re-encodes only the affected COMP blocks) but not
the record-level replacement step that goes from a patched image to a complete
payload; and the higher-level `leica_source/package_edit.py` explicitly refuses
the `.FW` suffix and stamps `installable: false`. So "45/45" proves correctness at
the container and record level; it does **not** prove the firmware runs.

**One thing that has to be said plainly**: none of the constants above (`2224`,
`2208`, `6016`, `896`, `0xc0c`, `0xc0e`) appear in **any code file in this
repository**. They exist only in prose. `modules/framing/` is live-view frameline
geometry on a 3600×2400 logical surface — a different subsystem. So a reader
cannot currently re-check this claim in code, which is exactly what task B1 in
`docs/contributing-ideas.md` is for.

**Why it is not yet a shared solution**: the ratio baseline has two candidate
values (2224 / 2208) and static analysis cannot pick one. Measured against the
declared width 6016, 2224 is closer (0.122 % vs 0.602 %); measured against the
actual stride 5984, 2208 is closer (0.067 % vs 0.653 %). Both are defensible, so
this is not an arithmetic question — it is a question of which number counts as
"the width". **Neither has been tested on a body**, and only a real capture can
settle it.

**One confirmed defect**: the crop is **not centred**. The sensor luma height is
3992 and the crop height is 2224, so a centred crop starts at
`(3992 - 2224) / 2 = 884`. The patch uses **896 — twelve rows too low**. Check by
inversion: for 896 to be centred the height would have to be
`3992 - 2 x 896 = 2200`, not 2224. So the aspect ratio is right but the whole
frame sits 12 pixels low. No container check can catch this, because the package
is structurally valid either way. Two readings, neither proven: 896 is a multiple
of 64 while 884 is not even a multiple of 16, so if a downstream stage needs an
aligned plane origin then 896 is the nearest legal value; or it is a copy-paste
slip from the chroma term `2 x 448`.

**Two unresolved risks**: the patch landing site `0x421f6ee0` is 981 bytes of
zero padding in the file, but whether it is used as buffer/BSS at runtime cannot
be ruled out statically; and the semantics of the four scalars at
`cfg+0x18..+0x24` are not decoded, so if they participate in the crop rectangle
the patch would be self-contradictory.

**It needs one M10 and one measurement** — but note that flashing a real body
carries risk, see section 4.

### 1.2 ASC looks (10 Classic / 01 Contemporary)

A dual-Y-channel blend, with a real resource parser and an ordered C apply step.

- `modules/asc/`: 354 lines including b2y handling and selector integration
- Eight native windows plus a mandatory 105-entry B2Y hybrid

**How far it got**: the blend logic is complete; all ten Classic looks and the
one Contemporary look evaluate.

**Why it is not yet a shared solution**: a legitimate official ASC resource is
required to compute real pixels, and this repository **does not redistribute**
it. You must supply your own, from hardware or software you legitimately own.

**The gap is specific**: the native acquire/idle/cache/hardware call boundaries
are not closed.

### 1.3 Film looks (800T + 18 GRKit recipes)

A general engine, not a hardcoded lookup table.

- `modules/film/`: 31 files, 2165 lines
- Optional post/hue, monochrome, curve knots, selective colour
- 14 colour + 4 monochrome recipes

**How far it got**: the host-side reference implementation is complete and all
19 looks evaluate.

**Why it is not yet a shared solution**: it holds in the host reference domain
only. The M10's actual colour domain, the bank codec, and the native selector
have not been closed on hardware.

**The gap**: one real on-hardware colour-domain comparison.

### 1.4 Touch

- `modules/touch/`: 457 lines
- M10-P `calculate_gesture` is 3336 bytes, M10 is 3264 bytes; the first 15
  instructions are byte-identical, and M10-P additionally has a 106-byte
  whitelist dispatch at `0xCE5D8`–`0xCE646`

**How far it got**: what `modules/touch/` actually implements is **event routing
and pool lifetime management** — producer-tail repair (`tr_forward` /
`tr_completed_tail`), payload event filtering (`0x113`–`0x116`, `0x120`–`0x124`,
sentinel `0x211`), a debug-state predicate (`0x13a` / `0x13d`), and
`trc_pool_init/begin/finish/copy/reclaim_metadata`.

**Correcting an earlier misdescription**: a previous revision claimed this folder
held "the gesture accumulator state machine, with every state 0–3 transition".
**No such state machine exists in this repository** —
`grep -ri gesture modules/touch/` returns a single hit, the comment
`/* full raw status; no gesture wrapper */`. The gesture byte sizes (3336 / 3264)
come from private analysis and are not re-checkable in this tree.

**Why it is not yet a shared solution**: the producer call site, the 7-argument
ABI, and the pool lifetime and synchronisation are all still open.

**The gap**: this is genuine research, not a constant to flip. The good news is
that the path is already drawn.

---

## 2. Why these are "only on my machine"

Being honest about this is more useful than pretending they are universal.

| Dependency | Effect |
|---|---|
| That body's calibration | calibration-class data must be recomputed per camera |
| That owner's official resources | ASC resources are not distributed; you need your own |
| That firmware version | offsets are for 3.22.23.38; other versions must be re-verified |
| That single measurement | a ratio or colour domain cannot be settled from one machine |

This is not a shortcoming of the work; it is inherent to this kind of work.
**Any claim that a single approach covers all bodies should be treated with
suspicion — including this one.**

---

## 3. What it takes to make this a shared solution

Each item below can be picked up independently.

### 3.1 Second and third machines

- **XPan ratio baseline**: does your body want 2224 or 2208?
- **Colour domain**: run one RAW through both the host reference and the camera;
  where does the delta lie?

Neither requires modifying anything. Both require **measure and report**.

### 3.2 One clean hardware record

- A complete record of one firmware update (which blocks are written, in what
  order, how failure presents itself)
- A main-boot-stage dump, to establish whether a recovery path exists at all

This kind of record is disproportionately valuable, because it answers
"what happens if I get it wrong" — the precondition for every other plan.

### 3.3 Someone to carry analysis through to the write side

Touch, framing and low-noise shutter all stop at "understood, but not connected
to a write path."

That is not the dangerous part of the work; it is **the part that most needs
hands**. If you want to contribute, start with the `modules/touch/` state
machine — its path is the most complete.

### 3.4 Firmware version coverage

Every offset here targets **3.22.23.38**. The 4.x series has not been checked.

Valuable work: **apply the same symbol-location method to another version** and
see whether the offsets hold. The tool (`tools/m10sym/`) is in the repository
and the method is in `docs/symbol-table.md`.

---

## 4. On "why not just open-source everything"

The author's position is: **publish what can be discussed, and let the parts
that need accountability wait for that discussion's conclusion.**

Concretely — firmware writing, identity rewriting, and package construction, if
published outright, would first be used for things that should not be done
rather than for getting the approach right. So those stay in the private tree
until the public discussion has a conclusion.

This does not limit what you can do: every item in section 3 can be advanced
**without any write capability at all**.

---

## 5. How to start

```sh
# 1. confirm the toolchain itself is correct
python3 -m unittest discover -s tests

# 2. read one of your own packages (read-only; you supply the firmware)
python3 codec/independent_verify.py <your.fw>

# 3. see how the symbol extractor works
python3 tools/m10sym/extract.py --help
```

Step 2 needs a package you provide yourself. To exercise the codec with no firmware
at all, use `node --test codec/test_builder.mjs` (24 synthetic cases).

Then pick an item from section 3. If you want to claim one, open an issue with
your model and firmware version — that is how we find out whether any of this
generalises beyond one body.

**Tasks that need no hardware**, each with an entry point and an acceptance
criterion, are listed in [docs/contributing-ideas.md](contributing-ideas.md).

---

## 6. One sentence

**What could be done in isolation is done; what needs to be shared has not
started — because that takes more than one machine.**
