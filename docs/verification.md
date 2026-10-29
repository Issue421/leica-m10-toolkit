# Source release verification — 2026-10-29

Executor evidence for independent coordinator/reviewer acceptance. This is a developer
SOURCE release; no installable firmware or camera readiness is asserted. Firmware/source
processing used the existing offline wrapper. Only this public source directory was
written; no Git, network, device, firmware execution or ARM execution was used.

Final combined repair + additive candidate:

- **29 tests passed, zero failed, zero skipped**, using -B and all four local
  inputs. Public CI without private/local gamma/B2Y/reference data skips five
  corresponding optional tests explicitly; missing-input rejection stays active.
- **23 C objects compile:** 22 HOST and one pointer32 touch compile-only ARM object.
  Accepted native/source suites are reused; no target link/execution or full FW rebuild.
- **83 imported source/license provenance records**, including the exact new
  C composition. Original/public hashes and transformation reasons remain explicit.
- New identity glue executes selected config → ri JSON/freeze/bind → real cd/mb/cm/nb
  → reported A431/private serialization. Three selected labels include leading-zero
  and all-zero cases. Original record/raw8/S, descriptor restoration and full canonical
  uint64 MAX stay intact. Current edit/default/source failure do not change the old
  captured report. mb_thumb_builder/private-ready are explicitly HOST boundaries,
  not production native implementations; actual photo source is M10-only.
- Actual SFC host imports copy CC1 fields and consumed resource views into ordinary
  host buffers and verify them. ASC consumes147456 B; NORMAL restore consumes36864 B.
  The synthetic u16 GUI checks ID/state/opaque and geometry after restore. No bank
  encoding, native address ownership or physical hardware behavior is inferred.
- Original P1 repaired grkit hash and 51-sample parity, P2 reduced-kit frozen defaults,
  unavailable-module refusal, own-source run and byte-identical re-export still pass.
  Codec24/24 and underlying227-sample/955-C/etc. results are reused without rerunning.
- The prior actual localhost/browser evidence covers the repaired seven-style UI.
  This changed 22-style/dual-identity candidate has fresh handler/DOM/export checks;
  the coordinator separately owns its new focused browser check and fresh acceptance.

The bounded repair addresses two independent-review findings:

- The function-only GRKit extraction omitted `INV601=inverse(BT601)`. Before repair,
  exact-gamma black and white inputs raised `NameError` for all three slide looks
  (six reproduced failures). The deterministic curator now preserves that exact
  initializer after `inverse()` and before its users, in original source order.
  A single module-assignment audit found no other initializer omitted from this
  recipe path. Original reference bytes/hash and all licensing remain unchanged;
  provenance records the new public-source hash and the precise transformation.
- A reduced ZIP previously initialized every style as selected even when ASC/film
  modules were absent. Downloaded kits now load frozen `selection.json`, mark absent
  module controls/styles unavailable, and enforce module closure in shared validation
  used by CLI and API validation/export. Exports keep only compatible examples;
  bilingual quickstarts and CI use the kit's defaults.

Previously accepted bounded repair checks (historical evidence reused):

- **20 Python tests ran: 19 passed, one explicitly skipped.** Exact local GRKit gamma
  and the accepted original reference were supplied. Three looks × 17 black/white,
  neutral, saturated and representative RGB samples produced **51/51 exact output
  matches** through the public recipe runner. Finite/bounded outputs and distinct
  looks passed. Lifted blacks are retained exactly as in the accepted reference.
- Clearly labelled synthetic-gamma tests independently exercise all three models'
  boundary arithmetic and invalid gamma/nonfinite-input rejection. Synthetic gamma
  is rejected by the exact-gamma file loader; it is not a substitute recipe.
  Public CI explicitly skips the two exact-gamma/reference tests when local inputs
  are absent. The local ASC resource composition test was also skipped in this repair;
  its earlier accepted real-input composition/both-plan evidence is reused unchanged.
- Minimal and all-style ZIPs were extracted. Each kit's own actual HTTP handler and
  browser JavaScript were exercised using memory request/response streams and a Node
  DOM model. Initial frozen selection → unchanged download payload → API validation
  → source ZIP → own-source host run passed. Unavailable-module API requests returned
  clear 400 responses; compatible examples and CLI defaults validated. This is script/
  API regression coverage, **not a real-browser/localhost verification claim**.
  The coordinator separately reported successful live localhost/browser checks for
  the full and extracted-minimal kits: frozen selections, absent controls/styles,
  unchanged minimal re-export, matching selection, no module/host/firmware members,
  and successful ZIP integrity inspection. A nearby static note clarifies that
  capability descriptions cover the full project and unavailable modules are absent
  from the downloaded kit.
- Actual C calls passed normal/service/reject/unknown/sentinel touch cases, software
  label plus preserved original capture/checked consumer/descriptor restoration, all
  eight frame presets, and frozen per-shot WB rational gains. The combined downloaded
  source ZIP compiled/invoked its own modules and refused `.FW` without emitting one.
- **20 C source objects compiled:** 19 host objects plus the pointer32 touch wrapper
  as a compile-only freestanding ARMv5TE/Thumb object. Native imports remain unresolved;
  no native link, loader, device or ARM execution occurred.
- Source provenance/member hashes, public data boundaries, selected closure,
  deterministic archive equality, strict configuration and Origin rejection passed.
  All 51 imported source/license records retain their original hashes/licenses.
- The existing **24/24 synthetic codec** results are reused; codec algorithms did not
  change. Full 128 MB official reference reconstruction was not rerun. Accepted
  byte-identical 277-record/9-COMP-block evidence remains unchanged.

Initial additive integration checks (before new identity integration):

- **26/26 Python tests passed** with all four local-input variables supplied.
  The earlier repaired INV601 initializer and 51 exact first-three-look matches
  remain. The repaired grkit.py SHA256 is unchanged:
  `6d5b5deb9e9942440cb468b7a7cbe6edfc0b9b725a2714b1c515486cf4118332`.
- Product configuration/UI/CLI now include 22 styles with original-seven-ID
  compatibility. One enabled/default config still drives the host shot and export.
  Minimal/all-style extracted-kit UI/API/own-source/re-export regressions pass.
- One source-eval entry executes 800T and all 18 actual GRKit recipes. Accepted
  engine bodies and INV601/FLAT601 initializers are retained. Focused new checks
  cover four monochrome absent-stage semantics, knots/selective extensions and
  11 CC-BY-SA / 6 Spectral MIT / 1 t3mujinpack MIT attribution. The accepted
  227-sample independent C++ recipe suite is reused, not rerun.
- The actual sfc_freeze/apply/restore C source calls existing ASC/LVF/u16 backend.
  Pure synthetic GUI fields replace recovered byte fixtures. Real local ASC inputs
  exercise success, GUI-after-style PARTIAL and both restore responsibilities.
  Native film stage0 refusal remains; a separate GUIDES-only call succeeds.
- Imports of frozen original reference Python modules now explicitly suppress
  bytecode writes even when a caller forgets -B. No new external reference cache
  was found in the exact original GRKit cache location inspected.
- Codec code/results are unchanged; no codec or full-original rebuild is repeated.

Reproduce portable checks from the source root:

```sh
python3 -m unittest discover -s tests -v
node --test codec/test_builder.mjs
python3 -m leica_source compile-sources --build-dir build/objects
```

Optional inputs use `LOCAL_GRKIT_GAMMA`, `LOCAL_GRKIT_REFERENCE`,
`LOCAL_ORDINARY_B2Y` and `LOCAL_ASC_B2Y`. Their paths/contents are not published.
Exact-reference parity needs both GRKit variables; local-data skips are reported
explicitly. Frozen selection, SOURCE-HASHES and the deterministic ZIP identify the
public source being tested. No same-version update qualification, native GUI owner,
native film bank, loader or recovery is asserted.
