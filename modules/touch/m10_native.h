#ifndef M10_TOUCH_NATIVE_H
#define M10_TOUCH_NATIVE_H
#include "touch_repair.h"
/* Dependency import seam, to be bound by a separate loader/integrator.
 * No callable binding initializer/loader is supplied. Exact M10 4.26 anchors
 * are in tr_m10_required_bindings; all function addresses include Thumb bit.
 */
typedef struct {
    const volatile uint8_t *routes; /* 0x40975801, destination indexed */
    int (*message_allocate)(int pool, unsigned bytes, void **out);
    int (*message_release)(void *envelope);
    int (*mailbox_send)(int mailbox, void *envelope);
    void (*payload_free)(void *payload);
    int (*current_task_id)(int16_t *out);
    unsigned (*ui_state)(unsigned machine);
    void (*debug)(const void *snapshot, unsigned code);
} tr_m10_imports;
/* Only compile this header's adapter implementation for a 32-bit target. */
void tr_m10_completed_tail(const tr_m10_imports *, unsigned event, void *payload,
                          const void *independent_stack_snapshot,
                          unsigned debug_code, tr_result *out);
typedef struct {
    char img_sha256[65];
    uint32_t routes, allocate, release_envelope, send_mailbox, free_payload;
    uint32_t current_task_id, ui_state, debug;
    uint32_t producer_free_site, producer_debug_site;
} tr_m10_binding_requirements;
extern const tr_m10_binding_requirements tr_m10_required_bindings;
#endif
