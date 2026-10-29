#ifndef STYLE_FRAMING_H
#define STYLE_FRAMING_H
#include "../asc/m10_asc.h"
#include "../framing/m10_gui_native.h"

/* One caller-consistent source configuration, no menu/settings reads. */
enum sfc_style { SFC_NORMAL, SFC_ASC_CLASSIC, SFC_ASC_CONTEMPORARY, SFC_NATIVE_FILM };
enum sfc_domain { SFC_STYLE=1, SFC_GUIDES=2, SFC_BOTH=3 };
enum sfc_status { SFC_OK, SFC_ARGUMENT, SFC_BUSY, SFC_STYLE_FAILED,
                  SFC_GUIDES_FAILED, SFC_PARTIAL };
struct sfc_config {
    uint8_t style_enabled;
    enum sfc_style style;
    m10_mode mode;
    uint8_t native_film_selection; /* 0..3: opaque, no recipes implemented */
    struct lvf_config guides; /* sole generation, eight presets/User1/User2 */
};
/* Caller already obtained the binding/context through the existing M10 adapter.
   This is ordinary borrowed data, never a native qualification token. */
struct sfc_gui_input { struct m10gui *context; struct lvf_binding binding; };
struct sfc {
    struct lvf guides;
    uint8_t busy, style_changed, style_needs_restore;
};
struct sfc_result {
    enum sfc_status status;
    uint32_t generation;
    unsigned requested, attempted, completed, needs_restore;
    m10_result style; /* original cause/stage/recovery confirmations retained */
    enum lvf_status guides;
    uint8_t guides_open;
};
/* Initialize struct sfc to zero once. Do not reset it while it has restoration
   responsibility. All controller/config/GUI work is serialized by the caller.
   Freeze only during a window in which the caller has assembled one coherent
   config and has no concurrent writer. Copy/generation are SOURCE contracts;
   they do not authenticate native GUI/capture callbacks or a settings lock.
   A scheduled/in-flight capture keeps its own frozen config until completion;
   later user choices must not overwrite that copy. out changes only on success. */
enum sfc_status sfc_freeze(const struct sfc_config *, struct sfc_config *out);
/* Each call freezes and validates ALL config fields before any native effects.
   Explicit domains avoid touching an unchanged domain or taking its owner.
   Style is applied first; guide failure retains a successful style and reports
   PARTIAL. No hardware cross-domain atomicity/automatic rollback is claimed.
   Style calls already run in the original scheduler/import context required by
   m10_asc_apply (which has no ctx argument). GUI inputs do not provide that.
   Enabled guides use the real m10gui_backend plus lvf_open/draw; disabled guides
   close/release and restore the borrowed native fields. Existing-open guides
   retain their original context/binding. frame is written by enabled success. */
struct sfc_result sfc_apply(struct sfc *, const struct sfc_config *, unsigned domains,
                          const struct sfc_gui_input *, struct lvf_frame *frame);
/* Explicit recovery/close: requested changed/uncertain style -> runtime NORMAL;
   guides -> lvf_close, restoring the exact borrowed geometry/color/visibility
   via the existing adapter. This is not arbitrary prior-look restoration.
   Both requested domains are attempted even if one fails; repeated calls retry
   remaining responsibility. No new owner/token/capture model is introduced.
   Native abort/epoch/idle failure may still require the real native integrator
   before a retry; this function cannot qualify or force recovery. */
struct sfc_result sfc_restore(struct sfc *, unsigned domains, m10_mode mode);
/* UNIMPLEMENTED native film adapter, deliberately closed in film_closed.c.
   Always stage0/M10_UNQUALIFIED with no effects in this batch. It is not a
   pluggable successful-film path: any implementation needs a separate proven
   film restoration contract before controller support. NORMAL restoration here
   applies only to ASC operations, never an unknown film/native style baseline.
   Not a ROM symbol, recipe encoder, ASC bank alias, or qualification bypass. */
m10_result sfc_native_film_apply(uint8_t selection, m10_mode mode);
#endif
