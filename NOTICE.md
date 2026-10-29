# Attribution and distribution boundaries

This independent project is not affiliated with Leica Camera AG. Product and
film names identify compatibility and source models, not endorsement.

Project-original configurator, host adapters, C source adaptations and package
codec code are MIT under LICENSE. Per-file imported source identity and explicit
portability changes are in provenance.json. The manifest records research paths
relative to the original workspace; none is a runtime dependency.

`modules/film/frame800t.py` is GPL-2.0-only. The FRAME800T formula/bridge derives
from zhaoo/m10v commit `b0ea721e01d091044a8e0db52851fd925b57442c` and the FRAME
color bridge, with the accepted portable reference implementation. See
[GPL-2.0-only.txt](LICENSES/GPL-2.0-only.txt). This separately licensed Python
module is not relicensed MIT. Its software recipe does not include spatial
grain/halation or represent measured physical film.

`modules/film/grkit.py` is ported from ahonn/grkit commit
`cc9ff4e2a97962a47b4bd825d50e84988da5e70f`, MIT, Copyright (c) 2026 ahonn.
See [GRKit-MIT.txt](LICENSES/GRKit-MIT.txt). Math function/class bodies are
preserved; acquisition/Git/private-workspace routines were removed.

The three slide effect definitions in `modules/film/effects/` are CC-BY-SA-4.0,
fits by GRKit contributors derived from spektrafilm 0.3.4 by Andrea Volpato.
See [applicable effect attribution](LICENSES/GRKit-effects-NOTICE.md) and
[upstream license notice](LICENSES/SPEKTRAFILM-LICENSE.txt). They retain their
ShareAlike license, including modifications to those data definitions.

The accepted generalized `modules/film/recipe_engine.py` now executes all 18
GRKit definitions in `modules/film/deltas/`. Its mathematical definitions and
INV601/FLAT601 initializers are preserved; workspace-specific loaders are omitted
in favor of portable provenance checks and the user's exact local gamma.
The repaired three-look `grkit.py` remains unchanged as a regression reference.

| Current recipe data | Count | License / attribution |
|---|---:|---|
| spektrafilm 0.3.4 fits | 11 | CC-BY-SA-4.0; Andrea Volpato and GRKit contributors |
| Spectral Film LUT fits | 6 | MIT; Jan Lohse and GRKit contributors |
| t3mujinpack HP5 Plus 400 fit | 1 | MIT; João Almeida and GRKit contributors |

All applicable original notices and upstream changes are bundled under
[modules/film/licenses](modules/film/licenses/GRKIT_EFFECT_NOTICE.md), including
[Spectral MIT](modules/film/licenses/SPECTRAL_FILM_LUT_LICENSE.txt),
[t3mujinpack MIT](modules/film/licenses/T3MUJINPACK_LICENSE.txt),
[spektrafilm](modules/film/licenses/SPEKTRAFILM_LICENSE.txt) and
[GRKit code MIT](modules/film/licenses/GRKIT_CODE_LICENSE.txt).
Per-recipe source revision/LUT identity/data license remain in recipe_catalog.json.
The preserved upstream notice uses its original `sources/` layout names; those
applicable license/change files are adjacent in the bundled licenses directory.
Original LUTs, icons and fonts are not distributed. These are fitted software
models; no original-LUT or physical-film equivalence is claimed.

The exact GRKit gamma table is publicly available in the upstream catalog but
has no separately established redistribution license. It is intentionally
omitted. Supply a user local copy with the documented hash; no generic gamma
is silently substituted. The full catalog and golden binary artifacts are also
omitted. Upstream: https://grkit.app/effects/gr3-2.10/catalog.json .

Original Leica FW/IMG/B2Y data, recovered native instruction mirrors, private
identity/calibration/history/photos and device senders are excluded, and the
whole reported-identity subsystem is not part of this bundle. Obtain official
inputs directly from Leica and keep any
local extraction or generated reference outside the public source/artifact.

Source filenames can contain inert documentary addresses. They are not device
accessors, installers or evidence that those addresses are owned executable space.

## Portable native development additions

`leica_source/native/portable.py`, the adapter tests, CLI/export integration and
documentation are project-original portable adaptations under the project's MIT
license. Existing project-authored fine/legacy normalizers, native templates, ASC
selectors and loader source retain their accepted semantics. `resource.py` is a
byte-identical relocation of this project's public `modules/asc/b2y.py`; the
independent decoder is reused unchanged. No third-party film license is changed.

Native recipes distribute project-authored replacement encodings and stock-copy
operations, plus window preimage/replacement hashes. Original instruction windows
are read only from the user's exact verified official IMG; no recovered original
instruction mirrors or original TOC records are distributed. `.S` files are the
existing authored source for those changes, not an installation authorization.
The required official inputs remain local, independently obtained prerequisites.
Recipe provenance pins describe accepted source revisions; no private workspace
path, self-reported permission or license claim is a runtime dependency.
