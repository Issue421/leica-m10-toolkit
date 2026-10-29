#ifndef WB_CAPTURE_ADAPTER_H
#define WB_CAPTURE_ADAPTER_H
#include "wb_target.h"

/* HOST_REQUIRED source contract. No native callbacks, locks, I/O or admission.
 * All calls, input storage and the complete adapter are externally serialized.
 * Zero-initialize once, then init once with a unique owner-issued session epoch.
 * Owner guarantees epoch uniqueness across allocations/restarts/power loss.
 * Tokens are scoped to this object lifetime and epoch;
 * never copy/reset an initialized adapter, mutate its private-by-contract fields,
 * or accept tokens from a different adapter. Reboot only through wbca_reboot.
 * Every expected capture is independently supplied by its serialized owner.
 * identity_lease is an opaque, nonzero binding to the accepted identity owner's
 * immutable capture identity; that owner validates it before every call. This
 * module implements no serial parsing, publication or identity authority.
 * All spans must be accessible caller-owned ordinary memory, never device I/O.
 */
#define WBCA_CAPTURE_SLOTS 70u /* decimal, per separately selected model */
#define WBCA_METADATA_CAPACITY 140u
#define WBCA_SAVE_CAPACITY 280u
#define WBCA_SNAPSHOT_CAPACITY 0x720u
#define WBCA_HOST_CAPTURE_BOUND 1u
#define WBCA_BUILDER_SUCCEEDED 0 /* accepted original WB builder raw status */
#define WBCA_FRESH_BUILDER_ORIGINAL 1u
#define WBCA_OWNER_QUIESCENT 1u
#define WBCA_MEDIA_SD 0x10u
#define WBCA_MEDIA_USB 0x10000u
#define WBCA_OUTPUT_DNG 0u
#define WBCA_OUTPUT_JPEG 2u
typedef int wbca_status;
enum {
 WBCA_OK=0, WBCA_EARG=100, WBCA_ESTATE=101, WBCA_ESTALE=102,
 WBCA_ECAPACITY=103, WBCA_EEXHAUSTED=104, WBCA_EREAD=105,
 WBCA_EIDENTITY=106, WBCA_EBINDING=107, WBCA_ECHANGED=108,
 WBCA_EBUILDER=109, WBCA_EBUSY=110, WBCA_EALIAS=111,
 WBCA_EOUTPUT=112, WBCA_EQUIESCENCE=113
}; /* 1..12 propagate the accepted wb_status unchanged. */
enum {
 WBCA_SAVE_QUEUED=1, WBCA_SAVE_INFLIGHT=2, WBCA_SAVE_FAILED=3,
 WBCA_SAVE_CANCEL_PENDING=4, WBCA_SAVE_COMPLETED=5,
 WBCA_SAVE_CANCELLED=6, WBCA_SAVE_ABANDONED=7
};
typedef struct {
 uint32_t model,epoch,generation,capture_id,slot,incarnation,mode_token,identity_lease;
} wbca_capture_token;
typedef struct { wbca_capture_token capture; uint32_t instance_id; } wbca_metadata_token;
typedef struct {
 wbca_metadata_token metadata; uint32_t job_id,attempt_id,media,output;
} wbca_save_token;
typedef struct {
 uint32_t model,slot,mode_token,identity_lease,binding;
 const uint8_t *snapshot; size_t snapshot_bytes;
} wbca_prepare_claim;

/* Public layout permits fixed storage allocation; fields are private by contract.
 * Only the six WB input bytes are retained per metadata instance: other S fields
 * are not WB inputs. All S nevertheless stays pinned until ownership drains.
 */
typedef struct {
 uint32_t active,producer_open,incarnation;
 wbca_capture_token token; wb_shot shot;
 const uint8_t *snapshot; size_t snapshot_bytes;
} wbca_capture_record;
typedef struct {
 uint32_t used,computed,order;
 wbca_metadata_token token;
 uint8_t original[WB_NEUTRAL_BYTES],result[WB_NEUTRAL_BYTES];
 uint8_t channels[6];
} wbca_metadata_record;
typedef struct { uint32_t used,state; wbca_save_token token; } wbca_save_record;
typedef struct {
 uint32_t magic,epoch,generation,next_capture,next_metadata,next_job,next_attempt,current_valid;
 wb_config current;
 wbca_capture_record captures[WBCA_CAPTURE_SLOTS];
 wbca_metadata_record metadata[WBCA_METADATA_CAPACITY];
 wbca_save_record saves[WBCA_SAVE_CAPACITY];
} wbca_adapter;
_Static_assert(sizeof(wbca_capture_token)==32u && sizeof(wbca_metadata_token)==36u &&
 sizeof(wbca_save_token)==52u,"host token value ABI");
_Static_assert(sizeof(wbca_metadata_record)==104u && sizeof(wbca_save_record)==60u,"bounded record ABI");
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(wbca_capture_record)==144u && sizeof(wbca_adapter)==41548u,"target32 storage ABI");
#elif UINTPTR_MAX==UINT64_MAX
_Static_assert(sizeof(wbca_capture_record)==152u && sizeof(wbca_adapter)==42112u,"host64 storage ABI");
#else
#error unsupported pointer width
#endif

uint32_t wbca_native_admission(void); /* always zero */
wbca_status wbca_init(wbca_adapter *adapter,uint32_t unique_session_epoch);
/* Requires drained ownership and strictly greater owner-issued session epoch.
 * All old tokens stay invalid. Clears publication; no native epoch is read.
 * At UINT32_MAX this context cannot reboot; counter wrap is never permitted. */
wbca_status wbca_reboot(wbca_adapter *adapter,uint32_t unique_session_epoch);
/* Observed unsuccessful/malformed read invalidates ONLY new-capture authority.
 * Even malformed success values and exhaustion invalidate. Existing frozen
 * captures survive. Valid WBFT input epoch/generation are syntactic only; this
 * adapter replaces them with its own epoch and next publication generation.
 */
wbca_status wbca_config_read(wbca_adapter *adapter,uint32_t success,
 const uint8_t *wire,size_t bytes);
/* Prepare seam: after S allocation/init, before processing submission; no native
 * patch placement is certified. Copies current config by VALUE into this capture.
 * Retains the exact ordinary-memory S span (P720/M718) without copying it.
 * Unknown binding/missing identity, pinned slots and overlap with another active
 * S span reject atomically. Every output span is disjoint from all pinned S.
 */
wbca_status wbca_prepare(wbca_adapter *adapter,const wbca_prepare_claim *claim,
 wbca_capture_token *capture);
wbca_status wbca_capture_close(wbca_adapter *adapter,const wbca_capture_token *expected);
wbca_status wbca_metadata_create(wbca_adapter *adapter,const wbca_capture_token *expected,
 wbca_metadata_token *metadata);
/* DNG type0 only; explicit original builder success and original-source claim
 * required. Does not invoke builder or replace its return/status. On any error,
 * all state/inputs/destination are unchanged. First success stores original24,
 * WB channel bytes and result; repeat must match original/order/channels bytewise
 * and merely copies stored result. snapshot must be the exact pointer/extent
 * retained for this capture. Fresh invocations use a NEW metadata token
 * and true original: wb_snapshot_apply rejects calibrated-tag substitutions.
 * Output must be disjoint from adapter and ALL input spans.
 */
wbca_status wbca_metadata_compute(wbca_adapter *adapter,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t image_type,int32_t builder_status,
 uint32_t original_source,const uint8_t *original,size_t original_bytes,
 wb_byte_order order,const uint8_t *snapshot,size_t snapshot_bytes,
 uint8_t *destination,size_t destination_bytes);
/* Fanout/retry copies stored result, without re-reading config/S/current tags. */
wbca_status wbca_metadata_copy(wbca_adapter *adapter,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint8_t *destination,size_t bytes);
wbca_status wbca_metadata_release(wbca_adapter *adapter,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected);
wbca_status wbca_save_submit(wbca_adapter *adapter,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t media,uint32_t output,wbca_save_token *ticket);
/* Atomic paired DNG+JPEG ownership. JPEG retains S; it never receives C628. */
wbca_status wbca_save_pair(wbca_adapter *adapter,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t media,wbca_save_token tickets[2]);
wbca_status wbca_save_start(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected);
/* Failure retains a retryable owner. Complete or abandon is a separate event. */
wbca_status wbca_save_fail(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected);
wbca_status wbca_save_retry(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected,wbca_save_token *next);
wbca_status wbca_save_complete(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected);
wbca_status wbca_save_abandon(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected);
wbca_status wbca_save_cancel(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected);
/* No native quiescence inferred: owner must certify it explicitly. */
wbca_status wbca_save_ack_cancel(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected,uint32_t owner_quiescent);
/* A save callback has no input-original argument and cannot compound. DNG only,
 * current inflight attempt only; cancellation/terminal/stale attempts reject. */
wbca_status wbca_save_copy(wbca_adapter *adapter,const wbca_save_token *ticket,
 const wbca_capture_token *expected,uint8_t *destination,size_t bytes);
#endif
