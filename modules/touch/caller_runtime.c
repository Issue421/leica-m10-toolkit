#include "caller_runtime_internal.h"

#define TRC_READY 0x74524331u
#define TRC_FREE 0u
#define TRC_CALLING 1u
#define TRC_COMPLETE 2u

trc_status trc_pool_init(trc_pool *pool) {
    if (!pool) return TRC_BAD_ARGUMENT;
    if (pool->private_ready == TRC_READY) return TRC_ALREADY_INITIALIZED;
    if (pool->private_ready != 0u) return TRC_BAD_ARGUMENT;
    /* Initializer owns zero-initialization; never clear slots or generations. */
    pool->private_ready = TRC_READY;
    return TRC_OK;
}

trc_status trc_private_begin(trc_pool *pool, trc_handle *handle,
                             trc_slot **reserved) {
    unsigned n;
    if (!pool || !handle || !reserved) return TRC_BAD_ARGUMENT;
    if (pool->private_ready != TRC_READY) return TRC_NOT_INITIALIZED;
    for (n = 0; n < TRC_CAPACITY; ++n) {
        trc_slot *slot = &pool->private_slots[n];
        if (slot->private_phase != TRC_FREE ||
            slot->private_generation == UINT64_MAX) continue;
        ++slot->private_generation; /* exhausted slots never wrap/reuse */
        slot->private_phase = TRC_CALLING;
        slot->private_result.outcome = TR_ARGUMENT;
        slot->private_result.raw_status = 0;
        slot->private_result.cleanup_status = 0;
        slot->private_result.retained_envelope = NULL;
        slot->private_result.retained_payload = NULL;
        handle->owner = pool;
        handle->slot = n;
        handle->generation = slot->private_generation;
        *reserved = slot;
        return TRC_OK;
    }
    return TRC_FULL;
}

void trc_private_finish(trc_slot *slot) {
    /* No callback between the core return and publication of completeness. */
    slot->private_phase = TRC_COMPLETE;
}

trc_status trc_completed_tail(trc_pool *pool, const tr_dependencies *d,
    unsigned state, unsigned event, void *payload, const void *snapshot,
    unsigned code, trc_handle *handle) {
    trc_slot *slot;
    trc_status status = trc_private_begin(pool, handle, &slot);
    if (status != TRC_OK) return status;
    tr_completed_tail(d, state, event, payload, snapshot, code,
                      &slot->private_result);
    trc_private_finish(slot);
    return TRC_CORE_CALLED;
}

static trc_status lookup(const trc_pool *pool, trc_handle h,
                         const trc_slot **found) {
    const trc_slot *slot;
    if (!pool) return TRC_BAD_ARGUMENT;
    if (pool->private_ready != TRC_READY) return TRC_NOT_INITIALIZED;
    if (h.owner != pool || h.slot >= TRC_CAPACITY || h.generation == 0u)
        return TRC_STALE_HANDLE;
    slot = &pool->private_slots[h.slot];
    if (slot->private_phase == TRC_FREE ||
        slot->private_generation != h.generation) return TRC_STALE_HANDLE;
    if (slot->private_phase == TRC_CALLING) return TRC_IN_PROGRESS;
    if (slot->private_phase != TRC_COMPLETE) return TRC_RETAINED;
    *found = slot;
    return TRC_OK;
}

trc_status trc_copy(const trc_pool *pool, trc_handle h, tr_result *out) {
    const trc_slot *slot;
    trc_status status;
    if (!out) return TRC_BAD_ARGUMENT;
    status = lookup(pool, h, &slot);
    if (status != TRC_OK) return status;
    /* COMPLETE is stable under the original serialization/nonalias contract.
     * Volatile source reads prevent memcpy synthesis on this freestanding
     * target; they do not provide thread/interrupt synchronization. */
    {
        const volatile tr_result *source = &slot->private_result;
        out->outcome = source->outcome;
        out->raw_status = source->raw_status;
        out->cleanup_status = source->cleanup_status;
        out->retained_envelope = source->retained_envelope;
        out->retained_payload = source->retained_payload;
    }
    return TRC_OK;
}

trc_status trc_reclaim_metadata(trc_pool *pool, trc_handle h) {
    const trc_slot *slot;
    trc_status status = lookup(pool, h, &slot);
    if (status != TRC_OK) return status;
    if (slot->private_result.retained_envelope ||
        slot->private_result.retained_payload) return TRC_RETAINED;
    switch (slot->private_result.outcome) {
    case TR_ACCEPTED: case TR_DROPPED: case TR_REJECTED:
        /* Keep generation, never call dependencies or operate on resources. */
        pool->private_slots[h.slot].private_phase = TRC_FREE;
        return TRC_OK;
    case TR_UNKNOWN: case TR_REJECTED_CLEANUP_PENDING: case TR_ARGUMENT:
    default:
        return TRC_RETAINED;
    }
}
