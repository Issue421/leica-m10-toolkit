#ifndef TOUCH_REPAIR_H
#define TOUCH_REPAIR_H
#include <stddef.h>
#include <stdint.h>
/* C99 source repair, not a loader. No device I/O. One completed producer tail
 * transfers its freshly allocated 360-byte payload exactly once to this call.
 * Calls/dependencies require the caller's normal target synchronization. No
 * generation, cancellation, drain, completion callback or target lock is added.
 * The original SYS consumer remains responsible for accepted payload/envelope.
 */
typedef enum {
    TR_ACCEPTED, TR_DROPPED, TR_REJECTED, TR_UNKNOWN,
    TR_REJECTED_CLEANUP_PENDING, TR_ARGUMENT
} tr_outcome;
typedef struct {
    tr_outcome outcome;
    int raw_status, cleanup_status;
    void *retained_envelope, *retained_payload;
} tr_result; /* separate caller metadata; never in a native message */
typedef struct {
    void *context;
    int (*route)(void *context, unsigned destination);
    int (*allocate)(void *context, size_t bytes, void **envelope);
    void (*prepare)(void *context, void *envelope, unsigned event,
                    void *payload_for_header);
    int (*submit)(void *context, int mailbox, void *envelope);
    int (*release_envelope)(void *context, void *envelope);
    void (*release_payload)(void *context, void *payload);
    void (*continue_debug)(void *context, const void *stack_snapshot,
                           unsigned debug_code);
} tr_dependencies;
/* Exact static predicate used by drv_disp_tp_debug (machine40 state13A/13D).
 * Labels for other service screens do not invent a new mode flag. */
int tr_debug_state(unsigned ui_state);
int tr_payload_event(unsigned event);
/* Replaces the missing normal forwarding and original payload free only.
 * Valid events0x113..0x124, sentinel0x211, all other events are dropped locally.
 * UNKNOWN retains resources and has no free/retry/cancel API. */
void tr_forward(const tr_dependencies *, unsigned ui_state, unsigned event,
                void *payload, tr_result *);
/* Source replacement for completed tail: select ONE path at ui_state snapshot;
 * selected normal forwards and skips original debug; selected service drops
 * the normal heap then calls original debug with
 * a distinct, still-live stack snapshot, NEVER the possibly consumed payload.
 * The original separate debug call must be replaced (not duplicated). A later
 * service-state change inside the native debug guard may suppress that debug
 * frame; it never reselects normal for the same frame. */
void tr_completed_tail(const tr_dependencies *, unsigned ui_state, unsigned event,
                       void *payload, const void *stack_snapshot,
                       unsigned debug_code, tr_result *);
#endif
