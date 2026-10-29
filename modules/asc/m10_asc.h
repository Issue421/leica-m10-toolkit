#ifndef M10_ASC_H
#define M10_ASC_H
#include <stddef.h>
#include <stdint.h>
/* Ordinary M10 04.26.36.10 only. Borrowed ordinary host memory; no MMIO. */
typedef enum { M10_OK=0, M10_ARGUMENT, M10_SHORT_READ, M10_FORMAT,
 M10_MISSING, M10_SIZE, M10_OUTDATED, M10_EPOCH, M10_UNQUALIFIED,
 M10_IMPORT, M10_POSTCONDITION, M10_RECOVERY_REQUIRED } m10_status;
typedef enum { M10_NORMAL=0, M10_ASC_CLASSIC=1, M10_ASC_CONTEMPORARY=2 } m10_look;
typedef enum { M10_SLIVE=0, M10_SMAGNI=1, M10_STILL=2, M10_STILLLINK=3 } m10_mode;
typedef struct { uint32_t resource, calibration, hardware; } m10_epochs;
typedef struct { const uint8_t *body; uint32_t bytes, count, data_at, epoch; } m10_resource;
typedef struct { const uint8_t *payload; uint32_t bytes, category, index; } m10_entry;
typedef struct { uint32_t offset, mask, value; } m10_field;
typedef struct {
 const uint8_t *payload; uint32_t full_bytes, consume_bytes, bank;
 uint32_t channel, small, fallback, fallback_mask, fallback_value;
} m10_gamma_view;
typedef struct {
 m10_look look; m10_mode mode; m10_epochs epochs;
 m10_entry cc1, control; m10_field cc1_fields[9];
 m10_gamma_view gamma[8]; uint32_t gamma_count, control_mask, control_value;
} m10_plan;
/* Parser validates every directory entry and all active names; exact 42-slot ABI.
   mapped/registered/read lengths must agree. No allocation/registration occurs. */
m10_status m10_b2y_parse(const uint8_t *, uint32_t mapped, uint32_t registered,
 uint32_t bytes_read, uint32_t resource_epoch, m10_resource *);
m10_status m10_b2y_query(const m10_resource *, uint32_t category, const char *, m10_entry *);
m10_status m10_b2y_pin(const m10_resource *);
m10_status m10_cc1_pack(const m10_entry *, const uint8_t *runtime,
 uint32_t runtime_bytes, m10_look, m10_field out[9]);
/* Native channel ABI: 1=R,2=G,3=B,4=Y,5=ordinary shared RGBY.
   fallback=1 consumes no source; original large fallback sets bit25 and
   clears bit24; small fallback clears both. Masks are returned as data.
   Complete plans reject missing resources instead of issuing fallback transfers. */
m10_status m10_gamma_consumer(const m10_entry *, uint32_t channel,
 uint32_t small, uint32_t fallback, m10_gamma_view *);
m10_status m10_asc_preflight(const m10_resource *, const uint8_t *runtime,
 uint32_t runtime_bytes, m10_epochs, m10_look, m10_mode, m10_plan *);

typedef struct {
 const uint8_t *resource, *runtime;
 uint32_t mapped, registered, bytes_read, runtime_bytes;
 m10_epochs epochs; m10_mode current_mode;
} m10_native_inputs;
typedef struct { m10_status status, cause, recovery_status; uint32_t stage, recovery_required, idle_confirmed, caches_invalid_confirmed; } m10_result;
/* Real import boundary, deliberately unresolved by this source adaptation.
   No function-pointer table, caller qualification flag, or host credential exists.
   All inputs must be immutable/pinned under the original serialized scheduler
   until release. These are NEW required adapters, not claimed ROM symbols.
   Production closed_imports.c refuses acquire, before idle or hardware effects. */
m10_status m10_native_acquire_current(m10_native_inputs *);
m10_status m10_native_enter_confirmed_idle(void);
m10_status m10_native_invalidate_both_caches(void);
m10_status m10_native_verify_idle_cacheinvalid(void);
m10_status m10_native_write_cc1(const m10_field fields[9]);
m10_status m10_native_transfer_gamma(const m10_gamma_view *);
m10_status m10_native_write_gamma_control(uint32_t mask, uint32_t value);
m10_status m10_native_check_current_epochs(m10_epochs);
m10_status m10_native_verify_complete_idle(const m10_plan *);
/* Must atomically publish both cache keys + resume only after all views verified.
   On failure, must not resume. Recovery on any later failure is mandatory. */
m10_status m10_native_publish_and_resume(const m10_plan *);
m10_status m10_native_verify_running(const m10_plan *);
m10_status m10_native_abort_quiescent_cacheinvalid(void);
void m10_native_release_current(void);
m10_result m10_asc_apply(m10_look, m10_mode);
#endif
