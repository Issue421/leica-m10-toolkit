#ifndef TOUCH_CALLER_RUNTIME_H
#define TOUCH_CALLER_RUNTIME_H
#include "m10_native.h"

/* Fixed, caller-owned metadata only. Initialize an object with this macro,
 * then call trc_pool_init exactly before first use. The object must stay at
 * one address for its whole lifetime; do not copy, memset, reinitialize its
 * storage, or reuse the address while any handle can exist. No reset API.
 * These APIs require external serialization across threads/interrupts, but
 * support same-thread synchronous callback reentry on the same pool.
 */
#define TRC_CAPACITY 4u
#define TRC_POOL_INITIALIZER {0}
typedef struct trc_pool trc_pool;
typedef struct {
    const trc_pool *owner;
    uint64_t generation;
    unsigned slot;
} trc_handle;
typedef enum {
    TRC_OK = 0, TRC_CORE_CALLED, TRC_BAD_ARGUMENT, TRC_NOT_INITIALIZED,
    TRC_ALREADY_INITIALIZED, TRC_FULL, TRC_IN_PROGRESS, TRC_STALE_HANDLE,
    TRC_RETAINED
} trc_status;
/* Storage layout is public solely to permit C99 static/automatic ownership.
 * All private_* fields are runtime-owned, not an extension or recovery API.
 */
typedef struct {
    uint64_t private_generation;
    unsigned private_phase;
    tr_result private_result;
} trc_slot;
struct trc_pool {
    unsigned private_ready;
    trc_slot private_slots[TRC_CAPACITY];
};

/* A repeat init returns ALREADY_INITIALIZED without changing any byte.
 * Pass a valid C object made with TRC_POOL_INITIALIZER, never uninitialized RAM.
 */
trc_status trc_pool_init(trc_pool *pool);
/* On CORE_CALLED, *out_handle identifies a completed persistent result, even
 * when the core returned TR_ARGUMENT. It is assigned BEFORE core callbacks.
 * Other return statuses mean the core was NOT called, out_handle is untouched,
 * and original payload responsibility remains with the caller. NULL payload,
 * dependencies/imports or snapshot are forwarded for the core's own semantics.
 * out_handle must be independent writable caller storage, not pool/payload or
 * snapshot storage; keep each handle rather than overwriting it on reentry.
 * Payload/snapshot and dependencies/imports have the original core contract.
 * Snapshot is only borrowed during this call and is never stored in the pool.
 */
trc_status trc_completed_tail(trc_pool *pool, const tr_dependencies *dependencies,
    unsigned ui_state, unsigned event, void *payload, const void *snapshot,
    unsigned debug_code, trc_handle *out_handle);
/* Source-level M10 entry; link ONLY with the unchanged 32-bit native adapter
 * in a separately qualified target build. No binding/loader or bridge here.
 */
trc_status trc_m10_completed_tail(trc_pool *pool, const tr_m10_imports *imports,
    unsigned event, void *payload, const void *snapshot, unsigned debug_code,
    trc_handle *out_handle);
/* Copies only completed result metadata; every other status leaves *out intact.
 * Retained pointers are opaque, potentially dead aliases: never dereference,
 * free or retry them from this record. Copies are not authoritative later.
 */
trc_status trc_copy(const trc_pool *pool, trc_handle handle, tr_result *out);
/* Reclaims METADATA ONLY after ACCEPTED/DROPPED/REJECTED with both retained
 * aliases NULL. It does not touch or certify native resources, and in
 * particular cannot certify success of the native void payload_free seam.
 * UNKNOWN/cleanup-pending/ARGUMENT and any unexpected result stay retained.
 * Calling records cannot be read or reclaimed, even after synchronous consume.
 * No resolve/free/retry/drain/cancel API is supplied.
 */
trc_status trc_reclaim_metadata(trc_pool *pool, trc_handle handle);
#endif
