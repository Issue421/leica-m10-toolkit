#ifndef M10_GUI_H
#define M10_GUI_H
#include "m10_gui_native.h"

/* Immutable provenance contract, never a live-source authenticator or lease.
   Code addresses below are documentary values, not function pointers. */
struct m10gui_profile {
    const char *model, *version, *img_sha256;
    uint32_t img_bytes, table_address, manager_address, child_address[3];
    uint32_t stock_callback; /* Thumb identity of M10's already registered stub */
};
const struct m10gui_profile *m10gui_fixed_profile(void);

/* UNIMPLEMENTED. The real serialized GUI owner must authenticate the exact M10
   build and stable mappings, qualify display/axes/clipping and parent visibility,
   hold the existing stock-hide scheduling seam, and restore/merge scheduling and
   pending redraw as specified by m10_gui_native.h. A caller flag/hash/token or
   the profile pointer cannot establish those facts. All five imports are required.
   Inspect/check success is the prerequisite for reading mapped target fields.
   Acquire is all-or-none and does not mutate family fields before snapshot. */
int m10gui_owner_inspect(void *, const struct m10gui_profile *, const struct m10gui_memory *, struct m10gui_domain *);
int m10gui_owner_acquire(void *, const struct m10gui_profile *, const struct m10gui_memory *, uint64_t *);
int m10gui_owner_check(void *, const struct m10gui_profile *, uint64_t, const struct m10gui_memory *, struct m10gui_domain *);
void m10gui_owner_abort(void *, const struct m10gui_profile *, uint64_t);
int m10gui_owner_finish(void *, const struct m10gui_profile *, uint64_t, const struct m10gui_redraw_receipt *);
#endif
