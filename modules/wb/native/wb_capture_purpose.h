#ifndef WB_CAPTURE_PURPOSE_H
#define WB_CAPTURE_PURPOSE_H
#include "wb_capture_adapter.h"

/* Ordinary caller-owned memory only; externally serialized with the unchanged
 * adapter and native-source observation. No factory call, I/O, identity issuer,
 * ownership registry or native admission. Profile IDs refer to the exact accepted
 * IMG hashes in source-purpose.json; model equality does not attest a camera.
 *
 * At the post-dispatch seam the identity owner must validate the immutable lease
 * and independently current claim, then freeze purpose by VALUE. P: immediately
 * after4216AEB0; M10: after4216574C. J+60 is now an actual workflow result, before
 * optional wbca_prepare freezes config/registers S. The owner guarantees that S
 * is accessible, owned, correctly initialized and alive, and this is that event.
 * Schema/stage integers and matching bytes cannot prove those native facts.
 * Before prepare, stale/ABA exclusion remains the identity owner's obligation.
 * This is later than the prior native-prepare candidate: wbca_prepare still
 * freezes current configuration at THIS call. No earlier config cache exists;
 * config-at-exposure/native-prepare semantics need a separate native proof.
 */
#define WBPA_SCHEMA_VERSION 1u
#define WBPA_STAGE_POST_DISPATCH 1u
#define WBPA_REASON_ORDINARY 0x3e8u
#define WBPA_REASON_DSM 0x3edu
#define WBPA_WORKFLOW_DSM 27u
/* Nonzero values are OPTIONAL WB bypass/refusal, never factory status. Native
 * capture/calibration/store continues with its own original status unchanged. */
enum {
 WBPA_BYPASS_DSM=200, WBPA_BYPASS_UNKNOWN=201, WBPA_ESCHEMA=202,
 WBPA_EBINDING=203, WBPA_ECHANGED=204, WBPA_EALIAS=205, WBPA_EARG=206
};
typedef struct {
 uint32_t schema_version,stage;
 wbca_prepare_claim claim;
 /* Actual LE u32 fields: S+20, S+24, S+68, embedded J+60. Offsets hex.
  * Never normalize an arbitrary reason to ordinary or predict workflow. */
 uint32_t selector,reason,output,workflow;
} wbpa_purpose;
/* Caller freezes ONLY after successful wbpa_prepare: purpose by value plus the
 * ACTUAL returned full token. The caller must not synthesize/modify a token.
 * This record is caller storage, not a new token or authority. */
typedef struct { wbpa_purpose purpose; wbca_capture_token capture; } wbpa_capture_binding;
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(wbpa_purpose)==52u && sizeof(wbpa_capture_binding)==84u,"purpose target32 ABI");
#elif UINTPTR_MAX==UINT64_MAX
_Static_assert(sizeof(wbpa_purpose)==64u && sizeof(wbpa_capture_binding)==96u,"purpose host64 ABI");
#endif

/* Pure pre-call check. Exact allow-set: selector0,reason3E8,output0/1/2 paired
 * with workflow3/4/5. Reads all four fields from the same exact S span and
 * compares them with frozen evidence and the independently supplied claim.
 * DSM/unknown/malformed/mismatch never changes any state or output. */
wbca_status wbpa_check_purpose(const wbpa_purpose *purpose,
 const wbca_prepare_claim *current_claim);
/* Only ordinary success can call accepted wbca_prepare. No config publication,
 * metadata or save call occurs here. All rejection outputs remain untouched. */
wbca_status wbpa_prepare(wbca_adapter *adapter,const wbpa_purpose *purpose,
 const wbca_prepare_claim *current_claim,wbca_capture_token *capture);
/* Pure check before metadata/save adaptation: compare actual returned full
 * capture token with an independently supplied expected token; bind model,
 * slot, mode, identity lease and exact S span to the retained purpose. Re-read
 * native purpose fields each time. The subsequent accepted API still validates
 * active token/lifetime/instance/attempt ownership. Equality here is no proof
 * that a factory callback is authentic or that ownership is still live.
 * Apply before metadata creation/compute/copy or save submit/start/copy/retry.
 * Always allow accepted release/drain handling for existing owners; a later
 * refusal does not authorize dropping S or inventing quiescence. */
wbca_status wbpa_check_capture(const wbpa_capture_binding *binding,
 const wbca_capture_token *expected,const uint8_t *snapshot,size_t snapshot_bytes);
#endif
