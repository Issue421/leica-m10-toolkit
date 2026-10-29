# Contribution ideas that need no camera

`docs/status.md` section 3 lists what it would take to make these features general.
Most of those items need a second body, a firmware dump, or an official resource —
which rules out most people. This page is the complement: tasks that are **fully
self-contained in this repository**, need **no hardware and no official firmware**,
and have an acceptance criterion you can check yourself.

Every task below was checked against the current tree, so the entry point and the
commands in it actually run.

If you want to claim one, open an issue with the task number. Nothing here requires
you to own an M10.

---

## A. Strengthen what already runs offline

### A1. Add a golden-output test for the film engine

The 18 GRKit recipes evaluate through one host entry, but the only exact-value
regression tests are the two skipped ones in `tests/test_source.py`
(`test_exact_gamma_matches_accepted_original_for_all_samples`,
`test_three_exact_gamma_boundary_neutral_saturation_recipes`). They skip because
they need a local gamma file that is not redistributed. Nothing pins the *shape* of
the engine's math for people without that file.

- **Entry point**: `leica_source/reference.py` (`film_rgb`), `modules/film/recipe_engine.py`
- **Task**: add a synthetic-gamma test that asserts finite, bounded, monotone and
  look-distinct output for all 18 recipes plus 800T, across a spread of inputs
  (black, neutral, saturated, out-of-gamut). The existing synthetic-gamma tests in
  `tests/test_source.py` are the pattern to copy — they already prove synthetic
  gamma is rejected by the exact-gamma loader, so the two paths stay separate.
- **Acceptance**: `python3 -m unittest discover -s tests` still reports 27 tests and
  13 skips (i.e. you added assertions, not skips), and deliberately corrupting one
  recipe's delta makes your new test fail.

### A2. Pin the ASC resource parser against malformed input

`modules/asc/b2y.py` implements `parse`/`encode` for the 42- and 47-slot B2Y
resource layouts, and `leica_source/native/resource.py` mirrors it. The rejection
paths (`capacity/header`, `truncated directory`, `condition_count/payload range`,
`unterminated condition`, `non-ASCII condition`) are reachable but untested.

- **Entry point**: `modules/asc/b2y.py`, tests in `tests/test_additions.py`
- **Task**: build a small in-memory synthetic header + directory (the format is
  fully documented in the module itself) and assert each rejection branch fires,
  plus a round-trip `parse(encode(x)) == x` on a synthetic resource.
- **Acceptance**: new test fails if you delete any one `raise ValueError` in
  `b2y.py`, and passes on the unmodified tree.

### A3. Extend the symbol extractor and prove it on synthetic input

`tools/m10sym/extract.py` finds embedded debug-symbol tables in a system image. It
has no test, and it cannot be tested without an image — which is exactly what makes
it a good self-contained task.

- **Entry point**: `tools/m10sym/extract.py`, format documented in `docs/symbol-table.md`
- **Task**: add a `--selftest` (or a test module) that *synthesises* a byte buffer
  containing two known tables per the documented entry format — including the
  `0x42` high-byte trap, the `+1` Thumb interworking bit, and 4-byte name padding —
  then asserts the extractor recovers exactly the planted names, and recovers
  nothing from a buffer with the Thumb bit stripped.
- **Acceptance**: the new test exercises both a positive and a negative table, and
  fails if the `+1` mask is removed from the pointer check.

### A4. Port the symbol extractor to a second model's naming

The extractor hardcodes the M10's `0x42000000` image base in its plausibility check.
The doc explains the encoding; it does not explain how it would differ on another
body.

- **Entry point**: `tools/m10sym/extract.py` (`--help` for current flags)
- **Task**: make the image base a flag with the M10 value as default, and document
  how you would determine the correct base for another model *from a dump you
  already own*. Do not invent offsets for a camera you cannot check.
- **Acceptance**: default behaviour is byte-identical to today on synthetic input;
  a non-default base is rejected when it produces no chain-consistent table.

### A5. Reconcile the two B2Y implementations

`modules/asc/b2y.py` and `leica_source/native/resource.py` are two implementations
of the same container. If they can drift, a contributor can "fix" one and silently
break the other.

- **Entry point**: both files
- **Task**: write a differential test over randomly generated synthetic resources
  asserting both parsers agree (same entries, same names, same payload slices) and
  both encoders agree. If they legitimately differ, document precisely where.
- **Acceptance**: the test fails if you perturb either parser's slot stride or
  last-wins selection rule, and passes on the current tree.

---

## B. Verify claims that are currently prose only

These are the highest-value tasks in the list, because right now several documented
claims cannot be checked by a reader at all.

### B1. The XPan crop constants are not in the code — make them checkable

`docs/status.md` §1.1 states the crop math (`+896×cfg[+0x0c]` for the Y plane,
`+448×cfg[+0x0e]` for Cb/Cr, `cfg[+0x12]` = `used_h`, baseline 2224 / 2208), and
`ROADMAP.md` describes a 6016 × 2224 frame. **No file in this repository contains
`2224`, `2208`, `6016`, `896`, `0xc0c` or `0xc0e`.** The numbers exist only in
documentation. `modules/framing/` is live-view frameline geometry on a 3600 × 2400
logical surface — a different thing.

Note that the width comes from `cfg[+0x10]` (`used_w`, 6016), **not** from
`cfg[+0x14]`: `+0x14` is the 1-byte `ycc_smpl` chroma-sampling selector. An
earlier revision of this task, and of `docs/status.md` §1.1, said `cfg[+0x14]`;
that was wrong.

- **Entry point**: `docs/status.md` §1.1, `ROADMAP.md` (XPan section), `modules/framing/lvf.c`
- **Task**: add a pure-arithmetic module (no I/O, no camera) that takes a sensor
  width and a target ratio and emits the crop rectangle using the documented
  constants, plus a test that asserts the 6016 × 2224 result and the exact ratio
  error stated in the roadmap (0.12 % off 65:24). This makes the claim falsifiable
  without hardware.
- **Acceptance**: given the documented constants the module reproduces 6016 × 2224
  and the documented error; flipping the baseline constant to the other candidate
  (2208) changes the asserted output, proving the two candidates are actually
  distinguishable in code.

### B2. Audit every documented command against the tree

The docs and the tree have drifted: the README links several documents that do not
exist, and references tools that are not shipped. A mechanical audit is useful and
repeatable.

- **Entry point**: `README.md`, `README.en.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `docs/*.md`
- **Task**: add a standard-library test that extracts fenced `sh` command lines and
  relative markdown links from the docs and asserts (a) every relative link target
  exists, and (b) every `python3 -m leica_source <cmd>` / `python3 <path>` names a
  real subcommand or file. Allow an explicit, commented allowlist for commands that
  intentionally need a user-supplied input.
- **Acceptance**: the test passes on a tree where the known-bad links are fixed, and
  fails if you reintroduce a link to a non-existent document.

### B3. Check the codec against the container format, independently

`codec/package_builder.mjs` and `codec/independent_decoder.py` are two independent
readers of the same container. `docs/partial-package.md` documents the layout
including the version-gate asymmetry (`compr=1` partial packages skip the `<`
comparison because of the `0x0CA1BDA7` branch at `0x131F3C`).

- **Entry point**: `codec/package_builder.mjs`, `codec/independent_decoder.py`,
  `docs/partial-package.md`
- **Task**: extend the synthetic fixture builder so it can emit a *partial* package
  header (type `0x0CA1BDA7`, compr `1`, version `[99,99,99,99]`, hardware `321`) and
  assert both readers parse it, agree on the TOC, and that the `compr=2` code paths
  correctly reject a partial-typed outer header. This tests the documented format
  claim without any real package.
- **Acceptance**: `node --test codec/test_builder.mjs` still reports 24 passing
  tests plus yours, and the negative case fails if the type check is removed.

### B4. Justify the `105`-entry and `8`-window ASC claims

`docs/status.md` claims "eight native windows plus a mandatory 105-entry B2Y
hybrid". Both numbers appear in code as hardcoded assertions
(`m10_asc.c`: `r->count != 105u`; `portable.py`: `!= 105`; the plan holds
`m10_gamma_view gamma[8]`, and `m10_asc_preflight` sets
`p.gamma_count = look==M10_NORMAL ? 2u : 8u`). Nothing demonstrates *why* 8 and 105
are the right bounds rather than observations from one image.

- **Entry point**: `modules/asc/m10_asc.c` (`m10_asc_preflight`), `modules/asc/m10_asc.h`,
  `leica_source/native/portable.py`
- **Task**: document where 8 and 105 come from (category layout and per-look window
  count), and add a compile-time or test-time assertion tying `gamma_count` to the
  declared `gamma[8]` array bound so a future edit cannot silently overflow it. Note
  that the count is currently a constant, not caller-controlled — the risk is a
  future change, not current input.
- **Acceptance**: a synthetic resource with exactly 105 entries parses; one with 106
  is rejected at the documented check; and removing the `gamma[8]` bound assertion
  makes a test (not the compiler) fail.

---

## C. Documentation work that unblocks the next reader

### C1. Reconcile `docs/status.md` and `README` against reality

The status document is the repo's contract with a contributor. Where it overstates,
the fix is either code or wording — decide which, and be explicit.

- **Entry point**: `docs/status.md`, `docs/status.en.md`, `README.md`, `README.en.md`
- **Task**: for each of the four features, add a one-line "how you can check this
  yourself, today, with no hardware" pointer, or state plainly that no such check
  exists. Keep both language versions in sync.
- **Acceptance**: every claim in §1 of the status docs either links to a runnable
  command/test, or is explicitly marked "no offline check available".

### C2. Give the gamma file a reachable acquisition path

`run-reference` with any GRKit style refuses without `--gamma`, because the exact
256-entry file is not redistributable and its required SHA-256
(`2c8f2166…7ae0f`) is printed only inside an error string and a README line.

- **Entry point**: `leica_source/reference.py` (`gamma_file`), `README.md`
- **Task**: document, precisely and without redistributing anything, where a lawful
  owner obtains that array from their own files, and what the expected shape is
  (256 big-endian u16, each ≤ 16384). A reader currently cannot tell whether they
  are even able to follow the film instructions.
- **Acceptance**: a reader with the file in hand can follow the doc to a successful
  `source-eval` without reading the source code; the hash check is described.

---

## What is *not* on this list, and why

Anything in `docs/status.md` §3 — a second body's ratio baseline, a colour-domain
comparison, a firmware-update trace, a main-boot dump — requires hardware this
repository cannot provide. Those items are real, but they are asks of camera owners,
not of contributors in general. Presenting them as the main on-ramp is what makes the
contribution surface look larger than it is.

The honest summary: **offline, a stranger can harden and falsify what is already
here (§A, §B) and fix the documentation (§C). They cannot advance any of the four
features toward working on a second M10**, because that step is gated on a camera.
