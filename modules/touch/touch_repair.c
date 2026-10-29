#include "touch_repair.h"
int tr_debug_state(unsigned state) { return state == 0x13a || state == 0x13d; }
int tr_payload_event(unsigned event) {
    return (event >= 0x113 && event <= 0x116) ||
           (event >= 0x120 && event <= 0x124);
}
static void result_init(tr_result *r) {
    r->outcome=TR_ARGUMENT; r->raw_status=0; r->cleanup_status=0;
    r->retained_envelope=NULL; r->retained_payload=NULL;
}
static int dependencies_valid(const tr_dependencies *d) {
    return d && d->route && d->allocate && d->prepare && d->submit &&
           d->release_envelope && d->release_payload;
}
static int precommit_rejection(int raw) {
    /* Actual fixed send kernel returns only these precommit errors. Generic
     * route/allocator failures are handled before submit below. */
    return raw == -17 || raw == -18 || raw == -42;
}
static void reject_cleanup(const tr_dependencies *d, void *env, void *payload,
                           int raw, tr_result *r) {
    r->outcome=TR_REJECTED; r->raw_status=raw;
    if (env) {
        r->cleanup_status=d->release_envelope(d->context,env);
        if (r->cleanup_status != 0) {
            r->retained_envelope=env;
            r->outcome=TR_REJECTED_CLEANUP_PENDING;
        }
    }
    /* No submission accepted this pointer, independent of envelope free result. */
    d->release_payload(d->context,payload);
}
void tr_forward(const tr_dependencies *d, unsigned state, unsigned event,
                void *payload, tr_result *r) {
    int mailbox, raw;
    int has_payload;
    void *env=NULL;
    if (!r) return; /* caller retains input: argument rejected */
    result_init(r);
    if (!dependencies_valid(d)) { r->retained_payload=payload; return; }
    if (!payload) { r->outcome=TR_DROPPED; return; } /* allocation failure */
    if (tr_debug_state(state) || event == 0x211 || event < 0x113 || event > 0x124) {
        d->release_payload(d->context,payload);
        r->outcome=TR_DROPPED;
        return;
    }
    has_payload=tr_payload_event(event);
    mailbox=d->route(d->context,2);
    if (mailbox <= 0 || mailbox > 32767) {
        reject_cleanup(d,NULL,payload,-17,r); return;
    }
    raw=d->allocate(d->context,36,&env); /* native envelope20 + header16 */
    if (raw != 0 || !env) {
        if (env) {
            /* Unexpected allocator result; its cleanup ownership is unproved. */
            r->outcome=TR_UNKNOWN; r->raw_status=raw;
            r->retained_envelope=env; r->retained_payload=payload;
            return;
        }
        reject_cleanup(d,NULL,payload,-50,r); return;
    }
    d->prepare(d->context,env,event,has_payload ? payload : NULL);
    raw=d->submit(d->context,mailbox,env);
    /* env and (for payload events) payload may already be freed by direct
     * handoff. From here no dereference or update of either allocation. */
    if (raw == 0) {
        r->outcome=TR_ACCEPTED;
        if (!has_payload) d->release_payload(d->context,payload);
        return;
    }
    if (precommit_rejection(raw)) {
        reject_cleanup(d,env,payload,raw,r); return;
    }
    r->outcome=TR_UNKNOWN; r->raw_status=raw; r->retained_envelope=env;
    if (has_payload) r->retained_payload=payload;
    else d->release_payload(d->context,payload); /* pointer was not in header */
}
void tr_completed_tail(const tr_dependencies *d, unsigned state, unsigned event,
                       void *payload, const void *snapshot, unsigned code,
                       tr_result *r) {
    tr_forward(d,state,event,payload,r);
    /* Exactly one mode decision for this frame. Normal selection SKIPS the
     * original debug call even if submit reentry changes the active UI state.
     * Existing M10 tail passes sp+48 to debug after freeing heap(sp+44).
     * The caller must provide that independent stack object, never payload. */
    if (tr_debug_state(state) && d && d->continue_debug && snapshot)
        d->continue_debug(d->context,snapshot,code);
}
