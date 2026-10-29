#include "m10_native.h"
/* Native in-process data layout only. No camera command or wire-frame encoder. */
typedef struct {
    uint32_t magic;
    uint16_t event, reserved;
    uint32_t payload, reserved_zero;
} native_header;
typedef struct {
    uint32_t link;
    int16_t priority;
    uint16_t reserved;
    int16_t destination, source;
    uint32_t parameter, length;
    native_header header;
} native_envelope;
typedef char check_pointer32[(sizeof(void *)==4) ? 1 : -1];
typedef char check_header16[(sizeof(native_header)==16) ? 1 : -1];
typedef char check_envelope36[(sizeof(native_envelope)==36) ? 1 : -1];
typedef char check_header_offset[(offsetof(native_envelope,header)==20) ? 1 : -1];
typedef char check_source_offset[(offsetof(native_envelope,source)==10) ? 1 : -1];
typedef char check_payload_offset[(offsetof(native_header,payload)==8) ? 1 : -1];
const tr_m10_binding_requirements tr_m10_required_bindings={
    "393d3c13a13e88ef43b58a07585dce06ce667f7c596da849f41af4653d9a0460",
    0x40975801,0x4205f119,0x42135e89,0x4205f301,0x4213c0b1,
    0x4205f0f1,0x421b5a81,0x420ccf45,0x420cced2,0x420cced8
};
static int native_route(void *context, unsigned destination) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    return i->routes[destination];
}
static int native_allocate(void *context, size_t bytes, void **out) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    return i->message_allocate(1,(unsigned)bytes,out);
}
static int native_release(void *context, void *env) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    return i->message_release(env);
}
static void native_payload_free(void *context, void *payload) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    i->payload_free(payload);
}
static void native_prepare(void *context, void *envelope, unsigned event,
                           void *payload) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    native_envelope *env=(native_envelope *)envelope;
    int16_t source=0;
    (void)i->current_task_id(&source);
    env->link=0; env->priority=5; env->reserved=0;
    env->destination=2; env->source=source;
    env->parameter=(uint32_t)(uintptr_t)&env->header;
    env->length=16;
    env->header.magic=UINT32_C(0x22000001);
    env->header.event=(uint16_t)event; env->header.reserved=0;
    env->header.payload=(uint32_t)(uintptr_t)payload;
    env->header.reserved_zero=0;
}
static int native_submit(void *context, int mailbox, void *env) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    return i->mailbox_send(mailbox,env); /* full raw status; no gesture wrapper */
}
static void native_debug(void *context,const void *snapshot,unsigned code) {
    const tr_m10_imports *i=(const tr_m10_imports *)context;
    i->debug(snapshot,code);
}
void tr_m10_completed_tail(const tr_m10_imports *i, unsigned event, void *payload,
                          const void *snapshot,unsigned code,tr_result *out) {
    tr_dependencies d;
    unsigned state;
    if (!out) return;
    if (!i || !i->routes || !i->message_allocate || !i->message_release ||
        !i->mailbox_send || !i->payload_free || !i->current_task_id ||
        !i->ui_state || !i->debug) {
        tr_forward(NULL,0,event,payload,out); return;
    }
    state=i->ui_state(40);
    d.context=(void *)i; d.route=native_route; d.allocate=native_allocate;
    d.prepare=native_prepare; d.submit=native_submit;
    d.release_envelope=native_release; d.release_payload=native_payload_free;
    d.continue_debug=native_debug;
    tr_completed_tail(&d,state,event,payload,snapshot,code,out);
}
