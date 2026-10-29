# Five-minute start — fully offline, no camera

Every command on this page has been run against this exact tree. They need Python
3.10+ and, for two of them, Node.js 20+. **No camera, no firmware download, no
network access.**

If a command needs a file you must supply yourself, it says so. Those steps are
marked **[needs your own file]** — everything else works on a fresh clone.

---

## 1. Confirm the toolchain is self-consistent (5 s)

```sh
python3 -m unittest discover -s tests
```

Expected: `Ran 27 tests` and `OK (skipped=13)`.

The 13 skips are deliberate. They are *not* a failure. They split as follows:

- **8** skip because `modules/identity/` is not published. Their gate is
  `skipUnless(all((ROOT/'modules'/m).exists() for m in ('touch','identity','framing','wb','asc','film')))`;
  creating an empty `modules/identity/` directory un-skips them (they then fail,
  because the identity sources really are absent — which is the point).
- **5** skip because a local input is not redistributed: the exact GRKit gamma
  (3 tests) and the official B2Y resources (2 tests).

`docs/contributing-ideas.md` §A covers adding tests that run without those inputs.

## 2. Evaluate a film look, no inputs required (1 s)

The 800T look is a closed-form recipe, so it needs no external data:

```sh
python3 -m leica_source source-eval --style cinestill800t --rgb 120,80,40
```

You get a JSON result with the rendered `rgb8` and the declared colour domain
(`FRAME800T software recipe, RGB8 code input`). This is real: the numbers come from
`modules/film/frame800t.py`.

The 18 GRKit recipes need one local file you must supply:

**[needs your own file]** ```sh
python3 -m leica_source source-eval --style all --gamma /local/catalog.json
```

Without `--gamma` the command refuses clearly rather than substituting a fake — that
is intentional. The expected shape is a JSON array (or object with `base_gamma`) of
256 integers in `0..16384` whose big-endian u16 packing hashes to the exact value
documented in `README.md`.

## 3. Check the container codec against its own tests (1 s)

```sh
node --test codec/test_builder.mjs
```

Expected: `pass 24`, `fail 0`. These exercise the COMP/TOC decoder against synthetic
fixtures, including negative cases (bad checksums, overlapping blocks, truncation).

## 4. Read a config, then run the host reference (2 s)

```sh
python3 -m leica_source default-config --output selection.json
python3 -m leica_source validate selection.json
python3 -m leica_source run-reference --config selection.json --rgb 120,80,40
```

`validate` prints the normalised config. `run-reference` compiles and runs the host
C reference and prints per-module JSON. Look for `"scope": "HOST reference
execution; not camera qualification or firmware"` and `"firmware_available": false`.

This is a **host** run. It does not touch a camera and does not produce firmware.

## 5. See what the symbol extractor does (instant)

```sh
python3 tools/m10sym/extract.py --help
```

It needs a system image to do anything real. The documented entry format is in
`docs/symbol-table.md`; a task that tests it without any image is
`docs/contributing-ideas.md` §A3.

## 6. Build the development source bundle (2 s)

```sh
python3 -m leica_source build-source --config selection.json --output development-source.zip
```

Expected: `"artifact_class": "development-source"` and `"firmware": false`. The ZIP
is source only — do not copy it to an SD card.

---

## What you cannot do from a fresh clone

These need files only you can provide. Each fails with a clear refusal, which is the
intended behaviour:

| Command | Needs |
|---|---|
| `run-reference --config … --gamma …` for any GRKit style | your local exact gamma array |
| `run-reference --config … --asc-resource …` | a combined B2Y resource you prepared locally |
| `extract-native` / `rebuild-reference` / `verify-official` | your own official firmware package |
| `package-*`, `models`, `identify` | modules not shipped in this tree — the CLI says so explicitly and points you at `codec/` |

## Where next

- `docs/status.md` — per feature: what works, what is missing.
- `docs/contributing-ideas.md` — concrete tasks that need **no hardware**, each with
  an entry point and an acceptance criterion.
- `docs/architecture.md` — how the pieces fit.
