#include "wb_capture_purpose.h"
/* Keep byte decode inline: accepted -Oz otherwise emits a function literal/BLX. */
static inline __attribute__((always_inline)) uint32_t pa_u32(const uint8_t *p) {
 return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int pa_span(const void *p,size_t n) {
 return p!=NULL && n!=0u && (uintptr_t)p<=UINTPTR_MAX-(n-1u);
}
static int pa_overlap(const void *a,size_t an,const void *b,size_t bn) {
 if (!pa_span(a,an) || !pa_span(b,bn)) return 1;
 return (uintptr_t)a<=(uintptr_t)b+bn-1u && (uintptr_t)b<=(uintptr_t)a+an-1u;
}
static int pa_claim_equal(const wbca_prepare_claim *a,const wbca_prepare_claim *b) {
 return a->model==b->model && a->slot==b->slot && a->mode_token==b->mode_token &&
  a->identity_lease==b->identity_lease && a->binding==b->binding &&
  a->snapshot==b->snapshot && a->snapshot_bytes==b->snapshot_bytes;
}
static int pa_token_equal(const wbca_capture_token *a,const wbca_capture_token *b) {
 return a->model==b->model && a->epoch==b->epoch && a->generation==b->generation &&
  a->capture_id==b->capture_id && a->slot==b->slot && a->incarnation==b->incarnation &&
  a->mode_token==b->mode_token && a->identity_lease==b->identity_lease;
}
wbca_status wbpa_check_purpose(const wbpa_purpose *p,const wbca_prepare_claim *current) {
 size_t j,extent; uint32_t selector,reason,output,workflow;
 if (!pa_span(p,sizeof *p) || !pa_span(current,sizeof *current)) return WBPA_EARG;
 if (p->schema_version!=WBPA_SCHEMA_VERSION || p->stage!=WBPA_STAGE_POST_DISPATCH) return WBPA_ESCHEMA;
 if (!pa_claim_equal(&p->claim,current) || current->identity_lease==0u ||
     current->binding!=WBCA_HOST_CAPTURE_BOUND || current->slot>=WBCA_CAPTURE_SLOTS) return WBPA_EBINDING;
 if (current->model!=wb_selected_profile.model) return WBPA_EBINDING;
 if (current->model==WB_MODEL_M10P) { j=0x270u; extent=0x720u; }
 else if (current->model==WB_MODEL_M10) { j=0x268u; extent=0x718u; }
 else return WBPA_BYPASS_UNKNOWN;
 if (current->snapshot_bytes!=extent || wb_selected_profile.snapshot_bytes!=extent ||
     !pa_span(current->snapshot,extent)) return WBPA_EBINDING;
 selector=pa_u32(current->snapshot+0x20u); reason=pa_u32(current->snapshot+0x24u);
 output=pa_u32(current->snapshot+0x68u); workflow=pa_u32(current->snapshot+j+0x60u);
 if (reason==WBPA_REASON_DSM || workflow==WBPA_WORKFLOW_DSM ||
     p->reason==WBPA_REASON_DSM || p->workflow==WBPA_WORKFLOW_DSM) return WBPA_BYPASS_DSM;
 if (selector!=p->selector || reason!=p->reason || output!=p->output || workflow!=p->workflow) return WBPA_ECHANGED;
 if (selector!=0u || reason!=WBPA_REASON_ORDINARY || output>2u || workflow!=3u+output) return WBPA_BYPASS_UNKNOWN;
 return WBCA_OK;
}
wbca_status wbpa_prepare(wbca_adapter *a,const wbpa_purpose *p,
 const wbca_prepare_claim *current,wbca_capture_token *capture) {
 wbca_status status=wbpa_check_purpose(p,current);
 if (status!=WBCA_OK) return status;
 if (!pa_span(capture,sizeof *capture)) return WBPA_EARG;
 /* Accepted prepare already protects adapter/claim/all retained S. Add the new
  * purpose input span so a successful token write cannot overwrite evidence. */
 if (pa_overlap(capture,sizeof *capture,p,sizeof *p)) return WBPA_EALIAS;
 return wbca_prepare(a,current,capture);
}
wbca_status wbpa_check_capture(const wbpa_capture_binding *b,const wbca_capture_token *expected,
 const uint8_t *snapshot,size_t snapshot_bytes) {
 wbca_status status; const wbca_prepare_claim *c;
 if (!pa_span(b,sizeof *b) || !pa_span(expected,sizeof *expected)) return WBPA_EARG;
 c=&b->purpose.claim;
 if (!pa_token_equal(&b->capture,expected) || expected->epoch==0u || expected->generation==0u ||
     expected->capture_id==0u || expected->incarnation==0u ||
     expected->model!=c->model || expected->slot!=c->slot ||
     expected->mode_token!=c->mode_token || expected->identity_lease!=c->identity_lease ||
     snapshot!=c->snapshot || snapshot_bytes!=c->snapshot_bytes) return WBPA_EBINDING;
 status=wbpa_check_purpose(&b->purpose,c); return status;
}
