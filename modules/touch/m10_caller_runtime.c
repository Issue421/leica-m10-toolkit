#include "caller_runtime_internal.h"

/* Intentionally separate from the host executable. This is a direct call to
 * the unchanged M10 adapter, never a host substitute for its pointer32 ABI.
 */
trc_status trc_m10_completed_tail(trc_pool *pool, const tr_m10_imports *imports,
    unsigned event, void *payload, const void *snapshot, unsigned code,
    trc_handle *handle) {
    trc_slot *slot;
    trc_status status = trc_private_begin(pool, handle, &slot);
    if (status != TRC_OK) return status;
    tr_m10_completed_tail(imports, event, payload, snapshot, code,
                          &slot->private_result);
    trc_private_finish(slot);
    return TRC_CORE_CALLED;
}
