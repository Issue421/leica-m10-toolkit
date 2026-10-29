# Leica M10 toolkit — the four features and how far each got

[Chinese](README.md). Independent project, not affiliated with Leica Camera AG.

Four features run today **on the author's own body**: **XPan framing, ASC looks,
film looks, and touch.** None of them is yet a shared solution, because each is
tied to that one machine's calibration, that one owner's official resources, and
that one firmware version.

That is what this repository is for: to carry them from "works on one machine"
to "works on any M10". **Section 1 says what already runs, section 2 says what is
missing, section 3 says why it only holds on one body.** Every gap can be picked
up independently.

## Start here

| Document | Content |
|---|---|
| **[docs/status.en.md](docs/status.en.md)** ([中文](docs/status.md)) | **Where this stands**: per feature — how far it got, what is missing, how to claim it |
| **[docs/quickstart.en.md](docs/quickstart.en.md)** ([中文](docs/quickstart.md)) | **Five-minute start**: fully offline, no camera needed |
| **[docs/contributing-ideas.md](docs/contributing-ideas.md)** | **Tasks that need no camera**: each with an entry point and an acceptance criterion |
| [docs/architecture.md](docs/architecture.md) | **How the pieces fit**: source layout, host vs native boundary, what is qualified |

## What this repository does not do

**There is no firmware writer.** No flashing path, no device protocol sender, no
way to produce a `.FW`. Everything here analyses, validates, or offline-recomputes
files you already own. It will not touch your camera.

Firmware writing, identity rewriting and package construction stay in a private
tree until the public discussion reaches a conclusion. That does **not** limit
what you can do: every gap listed above can be advanced without any write
capability at all.

## Read these first

| Document | Content |
|---|---|
| **[docs/quickstart.en.md](docs/quickstart.en.md)** | **Five-minute start**: fully offline, no camera — confirm the toolchain, evaluate a look, run the host reference |
| **[docs/contributing-ideas.md](docs/contributing-ideas.md)** | **Concrete work you can do offline**: verify a documented claim, harden a parser, test the symbol extractor |
| [docs/native-development.md](docs/native-development.md) | **Native development**: the portable composition/extract/verify/reverse workflow, and what stays unqualified |
| [docs/verification.md](docs/verification.md) | **Verification record**: what was executed, with which local inputs, and what a passing host test does *not* establish |

## Verify before you believe

This repository redistributes no Leica firmware. Every claim here is a statement about
bytes inside **your own** copy, so each one can be recomputed locally:

```sh
python3 codec/independent_verify.py /path/to/M10-4.26.36.10-Customer.FW
```

It re-checks the package independently of the JavaScript codec: outer magic and
format, COMP decode, TOC inspection, and against pinned full-package SHA-256 and
277-record payload identity. It prints JSON with `accepted`, `payload_sha256`,
`components` and `blocks`; a mismatch fails on the specific pin.

It needs a package you supply yourself. With no firmware at all you can still
exercise the codec on synthetic fixtures: `node --test codec/test_builder.mjs`
(24 cases, including negative ones). See
[docs/quickstart.en.md](docs/quickstart.en.md).

**No camera, no private working files, and nothing written** (components are unpacked to a
temporary directory). A full run takes two to three minutes because it expands a 127 MB
package.
Modifying and flashing are a different matter. **This body gives you one flash and
no independent recovery path.** That risk is yours — see section 6 of the
modification path.

## Note: the `package-*` / `models` / `identify` CLI is registered but not usable

Those subcommands appear in `--help` but the modules they import
(`leica_source/package_edit.py`, `package_profile.py`, `models.py`) are **not in this
tree**. Invoking them refuses with a clear message and points at the read-only
alternative instead of raising `ImportError`:

```sh
node codec/package_builder.mjs inspect YOUR.FW
node codec/package_builder.mjs diff ORIGINAL.FW REBUILT.bin
```

The packaging sections below describe container layout and acceptance criteria;
they are documentation of intent, not shipped capability. If you need package
authoring, say so in an issue rather than assuming the code is already public.

Python 3.10+ standard library is enough for the UI/source build:

```sh
python3 launch.py
# Open http://127.0.0.1:8780 (Windows: py launch.py)
# Source checkout only: create selection.json once. Downloaded kits already contain it.
python3 -m leica_source default-config --output selection.json
python3 -m leica_source validate selection.json
python3 -m leica_source build-source --config selection.json --output source.zip
```

The UI and CLI use the frozen `selection.json` in downloaded kits. Controls for absent
modules are disabled; validation and re-export reject unavailable modules clearly.
Only examples compatible with the exported module closure are included. A checkout
without frozen selection starts with every available style enabled. Advanced ASC/film
examples below require the corresponding bundled module and documented local inputs.

C11 clang/gcc (Windows: MinGW-w64, optionally set CC) is needed for real HOST execution:

```sh
python3 -m leica_source compile-sources --build-dir build/objects
python3 -m leica_source run-reference --config selection.json --rgb 120,80,40
python3 -m unittest discover -s tests -v
node --test codec/test_builder.mjs
```

The integrated host run freezes one configuration, calls actual touch producer-tail
logic, routes the selected report through real ri configuration/freeze and checked
original-to-reported A431/private serialization while preserving the original,
executes frame geometry with labelled
host callbacks, freezes WB gains per shot, and invokes the selected film recipe.
Native M10 GUI sources include the accepted u16 ID correction. JPEG crop and low-noise
shutter selections are recorded but execution rejects them because no implementation
exists. Guides do not crop JPEG or raw DNG data.

One source package can include all **22 styles**: Normal, two ASC, 800T and 18 GRKit
recipes (14 color / 4 monochrome). Existing seven public IDs remain compatible;
the additional recipes retain their accepted effect IDs. `styles.enabled` is the packaged
capability set and `styles.default` is exactly one current/default style in that set.
Each host shot freezes that choice and routes to ASC resources or a film recipe,
without stacking incompatible color domains. Touch/serial/guides remain independent.

The actual accepted C `sfc_freeze/apply/restore` now composes ASC, LVF and the
ordinary-M10 u16 backend. STYLE/GUIDES are independent; a successful style followed
by failed guides reports PARTIAL, and restoration tracks both responsibilities.
HOST callbacks use constructed ordinary-memory records, never recovered firmware
bytes. Native film remains closed at stage 0; guides can run independently.

ASC Classic/Contemporary use real local resource selection and CC1/RGBY plans, not a
mock pixel renderer. Native imports remain closed. Use `extract-asc` with local pinned
official M10 and ASC packages, then `prepare-asc`, then `run-reference --asc-resource`;
exact commands and pins are in README.md. Extraction reuses the existing decoder;
composition reuses the accepted B2Y host source. No original resources are redistributed.

800T uses the GPL-2.0-only software recipe. All 18 GRKit recipes use the accepted
generalized MIT engine: 11 spektrafilm fits are CC-BY-SA-4.0, six Spectral fits and
one t3mujinpack fit are MIT. Monochrome absent-post/hue, curve-knots and selective
color stages retain the accepted semantics. Supply the exact local GRKit gamma
with `--gamma /local/catalog.json`; no generic replacement is provided. Its separate
redistribution license is unresolved, so it is omitted. See NOTICE.md. Optional tests
cover black/white boundaries, neutrals and saturated colors with exact local gamma;
a local accepted GRKit reference enables exact output comparison. Missing inputs
produce explicit skips. Synthetic gamma tests check arithmetic/error branches only.

Obtain [official M10 input directly from Leica](https://leica-camera.com/sites/default/files/M10-4.26.36.10-Customer.FW):
127615696 B, SHA256 `d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c`.
This project never mirrors firmware. Local paths are not embedded in source exports.

```sh
python3 -m leica_source verify-official /local/M10.FW
# Optional, expensive, unchanged official reference only; needs Node.js 20+:
python3 -m leica_source rebuild-reference --input /local/M10.FW --output-dir build/reference
```

The real COMP encoder already reconstructed all 277 records / 9 blocks / 135899932
expanded symbols byte-identically to the above original. This release reuses that
evidence and runs synthetic codec tests; it does not repeat the full rebuild.
Reference rebuilding applies no selected features and outputs `.analysis.bin`.

The public release excludes original firmware, recovered native instruction mirrors,
private identity/calibration/photos, target binaries and device protocol senders.
The new identity builder C helper is present with its unresolved native import;
no test double is offered as its production implementation.

The identity demo now exercises the accepted dual original/reported source rather
than a separate label renderer. Leading zeros survive; editing the current report,
default/clear and later source failure do not change the old captured report.
Original record/raw8/S and full canonical uint64 identity stay separate. Use
`run-reference --original-serial 0001234 --next-reported-serial 0000009` for declared
synthetic host inputs. Actual photo-builder source is ordinary-M10-only; M10-P
photos, factory callers and native owner/import/serializer bindings are unimplemented.
The host external-segment/private-ready doubles are explicitly labelled, never a
production mb_thumb_builder. No persistence, all-ID or no-trace claim is made.

The pointer32 touch wrapper is compiled as a freestanding ARM object using an
ARM-capable clang; other C objects compile for the host. Neither is native-linked.
The verification sandbox prohibits socket binding, so HTTP tests use the actual
handler's in-memory request/response streams. The coordinator separately passed real
localhost/browser checks for full and extracted-minimal kits, including frozen
selection, unavailable controls/styles and unchanged minimal source re-export.

```sh
python3 -m leica_source source-eval --style all --gamma /local/catalog.json --rgb 120,80,40
```

This one command evaluates the 19 film recipes; it does not invent a Normal/ASC
pixel oracle or native bank encoder. The repaired three-look model remains an
unchanged regression reference, including its required INV601 initializer.

See [architecture](docs/architecture.md), [contributing](CONTRIBUTING.md),
[roadmap](ROADMAP.md), and [license boundaries](NOTICE.md). Developers extend the source
normally; there is no external approval-token service. A future real provider must
implement exact preimages, nonoverlapping placement, ABI/imports, loader coverage and
recovery. Caller JSON booleans cannot replace those missing implementations.

## Portable native development

The complete source checkout adds strict `extract-native`, `build-native`,
`verify-native` and `reverse-native` commands. They extract exact official local
M10 4.26.36.10 inputs and compose the accepted native ASC, independent PTP/A431
software report and exclusive fine/legacy WB windows into a NOT-INSTALLABLE IMG.
The HOST 22-style selection remains a separate schema; it never enables native
features implicitly. No modified compressed package, FW, boot/recovery or device
writer is delivered. See [native workflow and pins](docs/native-development.md).
