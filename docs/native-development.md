# Portable native development

This CLI composes actual native byte changes in ordinary M10 4.26.36.10's
9898708-byte IMG. It is an offline development artifact. It never emits a FW,
recompresses modified payloads, changes codec admission, executes ARM or accesses
a camera. All source dependencies are inside this checkout. Python 3.10+ and its
standard library suffice. It reuses the accepted independent decoder and the
unchanged public B2Y hybrid implementation (`native/resource.py` is an exact copy).

## Exact local inputs

Obtain firmware independently from Leica and keep it outside the source checkout.
No official files, extracted IMG/B2Y, original TOC records, photographs, card data

| Input | Bytes | SHA256 |
|---|---:|---|
| Ordinary M10 4.26.36.10 FW | 127615696 | `d6ef5b77b70451d2b6a31fc4a090bf8f664c1e07a8bebade657aff7da692423c` |
| M10-P ASC 4.26.36.10 FW, required only for ASC | 121740584 | `aacf21aac8aa1cd1d82c281c5fa1092c158e0bd491d23d9a0ae5197d08c32463` |
| Extracted ordinary IMG | 9898708 | `393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460` |
| Complete 105-entry hybrid B2Y | 1678832 | `0ed4a05cc5b74c07108c0d7e6ab531a7a86a798420a246e2dafa8a1da1efcaf3` |

`extract-native` checks exact FW size and hash before decoding. The existing decoder
checks COMP integrity and the complete TOC. Extraction retains only stock IMG,
ordinary B2Y, concatenated original TOC records, canonical index and optionally the
hybrid B2Y, plus a receipt. Build/reverse verify independent pinned content, including
TOC SHA256 `21b3028826022605b49eba564c9015e16e3d28bc7e0ebb7af68e50f11873cb5f`
and canonical index SHA256
`e2faf45c03fe45fdf64968d1e602b32d300e167744328e94b4a5a85365e2f5e3`.
No receipt assertion, local absolute path or externally supplied qualification flag
grants trust. Extracted directories can be moved. No original expanded blob is
retained; the other payloads are not extracted to disk.

A body running 3.22.23.38 is environment context. Its package/IMG cannot be
used as this native build baseline. The conditional capacity formula below retains
its original 3.22 analysis scope and grants no runtime allocation qualification.

## Configuration and operation

Run commands from a complete source checkout. Example local output parents must
already exist, and the selected output directories must be new and outside the
source checkout. Parent traversal and symlink paths, nonregular inputs, existing
outputs and firmware-like output names are refused before output mutation.

```sh
python3 -B -m leica_source native-default-config
python3 -B -m leica_source validate-native examples/native-synthetic-combined.json
python3 -B -m leica_source extract-native --official-fw /path/to/M10-4.26.36.10-Customer.FW --asc-fw /path/to/M10-P-ASC-4.26.36.10-Customer.FW --output-dir /path/to/private/extracted
python3 -B -m leica_source build-native --inputs /path/to/private/extracted --config examples/native-synthetic-combined.json --output-dir /path/to/private/combined
python3 -B -m leica_source verify-native --inputs /path/to/private/extracted --input-dir /path/to/private/combined
python3 -B -m leica_source reverse-native --inputs /path/to/private/extracted --input-dir /path/to/private/combined --output-dir /path/to/private/reversed
```

The independent native schema is `m10-native-composition/v2`. Its exact component
keys are shown in the synthetic examples. Missing components preserve stock.
Unknown components, duplicate/escaped keys, extra fields, floats/bools pretending to
be integers, nonfinite JSON, bad versions and missing ASC resources are rejected.
`leica.source-selection/v1` and its HOST 22 styles are a separate source/reference
selection; neither the UI nor an existing `selection.json` silently selects native.

- ASC: `{"component":"asc","enabled":true}` selects eight accepted windows,
  1101 owned bytes. Original boolean states 10/01/00-or-11 mean
  Classic/Contemporary/Normal. The exact hybrid is mandatory. Section247 keeps its
  hw141/mode120/resource contract; no default/unknown ASC resources replace normal.
  zeros retained. `outputs` independently selects ptp, a431 or both. Three windows
  total 56B. This changes only selected software reports on accepted success paths;
  private0001 remains ORIGINAL, other body/ID2/history outputs remain UNKNOWN or
  unchanged. It does not alter physical identity, calibration, history or old photos.
- Fine WB: `component: "fine-wb"`, `backend: "fine-grid-1/256"`. Unequal R/G/B
  rational gains must be exactly k/256, 64..1024. Legal equal gains, null and ORIGINAL
  produce an empty delta. The accepted 258B stock-only fragment is unchanged apart
  from its three reduced integer literals at offsets246/250/254. No approximation,
  new assembly, new ABI, encoder or native algorithm is introduced.
- Legacy WB: `component: "wb"`, `backend: "five-power-of-two"` retains
  {1/4,1/2,1,2,4} unequal gains and the accepted encoding classes. Old native v1
  inputs retain this backend. Fine and legacy cannot coexist, even as ORIGINAL.

The checked-in `.S` files are existing project-authored source and documentation
for the accepted recipes. Runtime does not invoke an assembler. `recipes.json`
retains fixed encodings of project-authored changes and copy operations into the
user's pinned stock. It contains no original window bytes. Every source window is
checked by length/SHA before generating a change. Reverse retrieves original bytes
from the pinned stock, not a redistributed native instruction mirror.

## Results and boundaries

The synthetic combined example uses `0000123` and `(257,256,255)/256`, solely to
reproduce the accepted sample. It owns twelve nonoverlapping windows /1415B.

| Selection | IMG SHA256 | IMG MD5 | Planned directory MD5 |
|---|---|---|---|
| ORIGINAL | `393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460` | `2a0eb87b1e77a89c946149a6707df1ba` | `51736c0f9683ebe27330cbfb91dda770` |
| ASC + synthetic ID + fine | `157665072326f63cbd11d0f14101e55ccd70c14f1c895ec5aa73024a1bc06564` | `7be783e1c331bf2f4d0ef09b6b2da885` | `2bca5f5a7a8d130d4173cb23f0368125` |

Each build emits a strict patch manifest, normalized config, actual IMG and separate
complete 277-record metadata plan. The manifest binds actual asset/source hashes;
the plan binds the manifest hash. Verify regenerates and byte-compares the complete
output inventory, including plan and standalone record files. Reverse first does
that full verification, restores each window from stock, and checks the entire
9898708B result against the stock SHA256.

ASC enabled: planned expanded136541276B, header+copyback136541332B, conditional
geometry headroom8162156B; 275 nonreplacement payloads/nonlayout properties remain
unchanged, with29 offsets migrated. Nine1B gaps have only an opaque source-copy
plan. Compressed lengths/hashes are unknown/null. The plan is not an expanded blob,
outer header or FW and does not grant loader/admission/installability.

Fine WB retains accepted zero imports, extra transient stack52B, product upper
bound33553408<2^25 and conservative loop bound3876. Whole-task stack/time and
hardware timing remain UNKNOWN. Touch, nineteen film recipes, frame guides,
JPEG crop, low-noise shutter, complete task budget, primary boot/load/trust and
recovery remain outside this native delivery. Existing HOST implementations and
licenses retain their separate scope. Ordinary SD version/admission and truthful
version identity remain unresolved for a custom update.

Focused adapter checks:

```sh
python3 -B -m unittest discover -s tests -p test_native_portable.py -v
```

The old native numerical/static suites and full codec suites are reused as prior
evidence; these commands do not rerun them. The portable validation decodes ordinary
and ASC once each and performs no modified payload recompression.
