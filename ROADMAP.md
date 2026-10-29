# Roadmap

Delivered: strict feature selection, deterministic development-source download,
real HOST C integration, 19 licensed reference recipes with explicit local data,
ordinary u16 GUI correction, real dual original/reported capture and serialization
source, and the real reference codec.
The existing selector now includes 22 styles, and accepted sfc C composition connects
ASC/guides with independent domain failures and restoration responsibility.

Delivered native development: portable stock-bound extraction/build/verify/reverse
for ordinary M10 4.26.36.10. Accepted ASC8 windows + complete105-entry hybrid,
independent PTP/A431 identity3 windows and exclusive fine/legacy WB1 window produce
actual NOT-INSTALLABLE IMG and a complete277-record metadata plan. Configuration
is separate from HOST22-style source selection. No custom FW/admission is added.

Native work still required for installation or the remaining feature set:

- Bind touch producer/caller 7-argument ABI and persistent pool storage/lifetime.
- Extend beyond the accepted PTP/A431 success outputs to factory captured-identity callers and canonical capture/serializer owner, retaining
  original identity/calibration/history; resolve the new identity builder imports.
- Qualify ASC resource/calibration ownership, idle/cache/hardware/epoch/publication and
  failure restoration; establish film M10 domain/native-bank implementation separately.
- Bind GUI owner/lock/stable mapping/display, same-frame stock-hide and pending redraw
  restoration. JPEG crop needs its own capture/encoder/ROI implementation.
- Qualify full WB task stack/time and hardware timing beyond the accepted C628 metadata windows; HOST async capture routes retain their existing separate gaps.
- Establish low-noise shutter hardware/control/acoustic feasibility before code claims.
- Establish legal executable/data placement, primary loader/trust coverage, full ABI
  and recovery; resolve normal SD admission while keeping version identity truthful.

After those implementations, extend the patch provider/assembler and only then
offer a feature-bearing .FW for the intended manual SD workflow.

## Converged state (2026-10-29)

Read-only device work is now possible, and several long-standing assumptions were
corrected. See [docs/status.md](docs/status.md) for the per-feature state,
[docs/architecture.md](docs/architecture.md) for how the shipped pieces fit, and
[docs/contributing-ideas.md](docs/contributing-ideas.md) for work that needs no
hardware.

> **Not in this tree.** The read-only PTP/IP client (`tools/m10_ptp_readonly.py`)
> and the patch generators (`tools/m10_patch.py`, including its `xpan` mode) are
> summarised below and were verified against hardware, but they are **not part of
> this published source tree** — `tools/` here contains only `m10sym/extract.py`.
> The findings are recorded so the protocol and crop-constant work can be
> reproduced; the tooling itself is not shipped.

Newly established:

- **PTP/IP works on the ordinary M10.** The gateway omits `EndData` and puts a
  length-less name in `InitCommandAck`; handling both yields `GetDeviceInfo`
  (verified on hardware: `LEICA M10 / 03.22.23.38`), 27 operations, 159 properties.
- **The M10 has no shell.** Ports 22/23/80/443/5001 are closed; only 15740 answers.
  The main board appears to the Wi-Fi module as a PTP (Still Image) USB device, not
  storage, so there is no file-level path on this body.
- **The five vendor opcodes are MTP object-property calls**: `0x9801`/`0x9802`/
  `0x9803`/`0x9805` read, **`0x9804` writes** (object rating). Do not call `0x9804`.
- **B2Y parity**: the two editions share 82 keys with byte-identical payloads, and
  the four M10 "generic" entries are the first N bytes of the ASC edition's
  `_FILMMODE:UNKNOWN` entries. Adding `_FILMMODE` entries is therefore inert.
- **Menu unhide is 28 bytes but aims at the wrong target**: menu `0x38` is a
  container with no attribute, and the real controls (`0x39`–`0x42`) were never
  hidden. `hide()` removes from the pointer array; it does not write a sentinel.
- **`xmpDSA` is Leica-proprietary perspective metadata** (`digital-shift-assistant`),
  written unconditionally into every JPEG and read by nothing in the firmware.
- **Lens corrections**: `lensmaster.llf` is 82 × 467 B keyed by (6-bit code,
  frameline lever); CA and distortion slots are `disable` for all entries, so only
  vignetting is applied. Manual lens selection uses the same path as the 6-bit code.
- **One-shot flash, no recovery**: same-version packages are rejected at discovery;
  the restore preaction is a logging stub; there is no A/B or golden image.

Delivered **in the private research tree, not in this one**:
`tools/m10_ptp_readonly.py` (read-only, hardware-tested) and `tools/m10_patch.py`
(`menu` / `b2y` / `film` generators that operate on files the user supplies and
reproduce the analysis artifacts byte-for-byte, plus `xpan` which emits the crop
specification). Neither file is present here — see the note above.

**XPan is the flagship crop target.** 65:24 cannot be widened on a 3:2 sensor, so it
is a full-width, height-cropped 6016 × 2224 frame (13.4 MP, 0.12 % off 65:24, and the
only height near the ideal whose offset and height are both multiples of 16). The
implementation is narrow: a 4-byte injection at `0x15FD42` plus a ~64 B function that
offsets the three YCC pointers and rewrites `used_h`, and two horizontal frameline
segments at `y = 107 / 373`. The "16-pixel alignment" constraint that earlier analysis
attributed to the encoder turned out to be a **decode** callback (`0xD55C6`, mask
`0x0A002004`); the encoder has no proven alignment requirement, so the exact 2221/897
variant remains worth one attempt.

Still open, in rough priority order:

- Primary boot reader / length / entry / trust / fallback; installability evidence.
- A provable code-placement window (only 2 zero-runs ≥128 B remain in the code
  region; the page tables mark the whole space XN and cannot settle executability).
- Writers of `ctx+0x2CC/0x2D0` (decides whether real pixel cropping is reachable).
- Whether RAW receives shading correction (the prepro stage suggests yes).
- A real flash success/failure record — the scarcest artifact in the project.

### Flash-free entry found (lens corrections)

The first genuinely user-reachable, flash-free modification path in this project:
the lens-correction loader reads `/data/lens/{shb,cab,dib}/<name>` from SD **when a lens is
changed or selected** (host functions `0x420AF3D0` / `0x420AF07C` / `0x420AF228`, triggered via
`0x420AE218` from lens selection `0x420B416E` or the lens-change event `0x420AEAC4`). The read
path never writes flash; a success writes a cache under `/settings/episodic/lens/` that must be
cleared or it shadows the card file. Swapping a `.shb` for an in-table lens is flash-free;
adding a lens that is not in the table requires an `lensmaster.llf` update, and that path
(`hsk_FlashFileFromSDCard`) does write NOR. Open: whether `/data/lens` is on the removable card
or in internal storage — the evidence leans removable but no mount table was found.

This matters beyond lenses: it is the first evidence that **the body reads per-unit
configuration from media at runtime**, which is the shape any future non-flash path would take.
