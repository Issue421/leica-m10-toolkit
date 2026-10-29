# Contributing

Keep runtime code portable and offline. Run Python tests, original synthetic codec
tests and compile-sources. Add behavior tests at the affected real source boundary;
do not replace core implementations with parallel manifest/state models.

Use source-relative paths. Document every copied/changed upstream source and license
in provenance.json and NOTICE.md. Do not commit official FW/IMG/resources, generated
native mirrors, photos/calibrations/identity/history, local input paths or debug senders.
The maintainer curator is a manual offline allowlist, not required to build this repo.

Maintainer refresh order with the accepted local research workspace is
`tools/curate_sources.py --workspace PATH`, then
`tools/import_additions.py --workspace PATH`. Both are source-only allowlists;
the second retains the repaired original model and appends accepted new modules
and their provenance. These commands are not firmware acquisition/build steps.

Preserve original source and error results. Explicitly label host callbacks and
native unresolved imports. A passing host test, caller-supplied flag, hash or receipt
does not establish real GUI, capture owner, loader or installation qualification.

Future native work should implement a concrete missing callsite/ABI/storage/loader
edge with exact preimages and recovery evidence, using the extension contract.
No device operations, credentials or online authority are part of the source tool.
