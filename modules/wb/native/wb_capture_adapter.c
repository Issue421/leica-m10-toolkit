#include "wb_capture_adapter.h"
#define WBCA_MAGIC UINT32_C(0x57424341)
/* The accepted -Oz profile otherwise shares this helper through a literal and
 * BLX. Keep byte copies inline; do not relax the no-indirect-call audit. */
static inline __attribute__((always_inline)) void ca_copy(void *destination,const void *source,size_t count) {
 uint8_t *d=(uint8_t *)destination; const uint8_t *s=(const uint8_t *)source;
 while (count!=0u) { *d++=*s++; --count; }
}
static void ca_zero(void *destination,size_t count) {
 uint8_t *d=(uint8_t *)destination;
 while (count!=0u) { *d++=0u; --count; }
}
static int ca_equal(const void *a,const void *b,size_t count) {
 const uint8_t *x=(const uint8_t *)a,*y=(const uint8_t *)b;
 while (count!=0u) { if (*x++!=*y++) return 0; --count; } return 1;
}
static int ca_overlap(const void *a,size_t an,const void *b,size_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if (an==0u || bn==0u) return 0;
 if (x>UINTPTR_MAX-(an-1u) || y>UINTPTR_MAX-(bn-1u)) return 1;
 return x<=y+bn-1u && y<=x+an-1u;
}
static int ca_external(const wbca_adapter *a,const void *p,size_t n) {
 return p!=NULL && n!=0u && !ca_overlap(a,sizeof *a,p,n);
}
static int ca_live(const wbca_adapter *a) { return a!=NULL && a->magic==WBCA_MAGIC; }
static int ca_pinned_overlap(const wbca_adapter *a,const void *span,size_t bytes) {
 uint32_t i;
 for (i=0u;i<WBCA_CAPTURE_SLOTS;++i) if (a->captures[i].active!=0u &&
  ca_overlap(span,bytes,a->captures[i].snapshot,a->captures[i].snapshot_bytes)) return 1;
 return 0;
}
static int ca_terminal(uint32_t state) {
 return state==WBCA_SAVE_COMPLETED || state==WBCA_SAVE_CANCELLED || state==WBCA_SAVE_ABANDONED;
}
static wbca_status ca_capture(wbca_adapter *a,const wbca_capture_token *expected,
 wbca_capture_record **record) {
 wbca_capture_record *c;
 if (!ca_live(a)) return WBCA_ESTATE;
 if (!ca_external(a,expected,sizeof *expected)) return WBCA_EARG;
 if (expected->identity_lease==0u) return WBCA_EIDENTITY;
 if (expected->model!=wb_selected_profile.model || expected->epoch!=a->epoch ||
     expected->generation==0u || expected->capture_id==0u ||
     expected->slot>=WBCA_CAPTURE_SLOTS || expected->incarnation==0u) return WBCA_ESTALE;
 c=&a->captures[expected->slot];
 if (c->active!=1u || !ca_equal(expected,&c->token,sizeof *expected)) return WBCA_ESTALE;
 *record=c; return WBCA_OK;
}
static wbca_status ca_metadata(wbca_adapter *a,const wbca_metadata_token *token,
 const wbca_capture_token *expected,wbca_metadata_record **record) {
 wbca_capture_record *c; uint32_t i; wbca_status status=ca_capture(a,expected,&c);
 if (status!=WBCA_OK) return status;
 if (!ca_external(a,token,sizeof *token)) return WBCA_EARG;
 if (token->instance_id==0u || !ca_equal(&token->capture,expected,sizeof *expected)) return WBCA_ESTALE;
 for (i=0u;i<WBCA_METADATA_CAPACITY;++i) {
  wbca_metadata_record *m=&a->metadata[i];
  if (m->used==1u && ca_equal(token,&m->token,sizeof *token)) { *record=m; return WBCA_OK; }
 }
 return WBCA_ESTALE;
}
static wbca_status ca_save(wbca_adapter *a,const wbca_save_token *token,
 const wbca_capture_token *expected,wbca_save_record **record,wbca_metadata_record **metadata) {
 uint32_t i; wbca_status status;
 if (!ca_live(a)) return WBCA_ESTATE;
 if (!ca_external(a,token,sizeof *token)) return WBCA_EARG;
 status=ca_metadata(a,&token->metadata,expected,metadata); if (status!=WBCA_OK) return status;
 if (token->job_id==0u || token->attempt_id==0u) return WBCA_ESTALE;
 for (i=0u;i<WBCA_SAVE_CAPACITY;++i) {
  wbca_save_record *s=&a->saves[i];
  if (s->used==1u && ca_equal(token,&s->token,sizeof *token)) {
   if (ca_terminal(s->state)) return WBCA_ESTALE;
   *record=s; return WBCA_OK;
  }
 }
 return WBCA_ESTALE;
}
static void ca_retire(wbca_adapter *a,wbca_capture_record *c) {
 uint32_t i;
 if (c->producer_open!=0u) return;
 for (i=0u;i<WBCA_METADATA_CAPACITY;++i)
  if (a->metadata[i].used!=0u &&
      ca_equal(&a->metadata[i].token.capture,&c->token,sizeof c->token)) return;
 c->active=0u;
}
static int ca_output_span(const wbca_adapter *a,const void *output,size_t bytes,
 const void *token,size_t token_bytes,const wbca_capture_token *expected) {
 return ca_external(a,output,bytes) && !ca_pinned_overlap(a,output,bytes) &&
  !ca_overlap(output,bytes,token,token_bytes) &&
  !ca_overlap(output,bytes,expected,sizeof *expected);
}
uint32_t wbca_native_admission(void) { return 0u; }
wbca_status wbca_init(wbca_adapter *a,uint32_t unique_session_epoch) {
 const uint8_t *bytes=(const uint8_t *)a; size_t i;
 if (a==NULL) return WBCA_EARG;
 if (unique_session_epoch==0u) return WBCA_EARG;
 if ((wb_selected_profile.model==WB_MODEL_M10P && wb_selected_profile.snapshot_bytes!=0x720u) ||
     (wb_selected_profile.model==WB_MODEL_M10 && wb_selected_profile.snapshot_bytes!=0x718u) ||
     (wb_selected_profile.model!=WB_MODEL_M10P && wb_selected_profile.model!=WB_MODEL_M10)) return WB_EPROFILE;
 for (i=0u;i<sizeof *a;++i) if (bytes[i]!=0u) return WBCA_ESTATE;
 a->magic=WBCA_MAGIC; a->epoch=unique_session_epoch; return WBCA_OK;
}
wbca_status wbca_reboot(wbca_adapter *a,uint32_t unique_session_epoch) {
 uint32_t i;
 if (!ca_live(a)) return WBCA_ESTATE;
 for (i=0u;i<WBCA_CAPTURE_SLOTS;++i) if (a->captures[i].active!=0u) return WBCA_EBUSY;
 for (i=0u;i<WBCA_METADATA_CAPACITY;++i) if (a->metadata[i].used!=0u) return WBCA_EBUSY;
 for (i=0u;i<WBCA_SAVE_CAPACITY;++i) if (a->saves[i].used!=0u) return WBCA_EBUSY;
 if (a->epoch==UINT32_MAX) return WBCA_EEXHAUSTED;
 if (unique_session_epoch<=a->epoch) return WBCA_ESTALE;
 a->epoch=unique_session_epoch; a->generation=0u; a->current_valid=0u; ca_zero(&a->current,sizeof a->current);
 return WBCA_OK;
}
wbca_status wbca_config_read(wbca_adapter *a,uint32_t success,const uint8_t *wire,size_t bytes) {
 wb_config local; wb_status status;
 if (!ca_live(a)) return WBCA_ESTATE;
 a->current_valid=0u;
 if (success!=1u) return WBCA_EREAD;
 if (!ca_external(a,wire,bytes)) return WBCA_EARG;
 status=wb_config_decode(&local,wire,bytes); if (status!=WB_OK) return (wbca_status)status;
 if (a->generation==UINT32_MAX) return WBCA_EEXHAUSTED;
 local.epoch=a->epoch; local.generation=a->generation+1u;
 ca_copy(&a->current,&local,sizeof local); ++a->generation; a->current_valid=1u;
 return WBCA_OK;
}
wbca_status wbca_prepare(wbca_adapter *a,const wbca_prepare_claim *claim,wbca_capture_token *capture) {
 wbca_capture_record *c; wb_shot shot; wb_provenance provenance; wbca_capture_token token; wb_status status;
 if (!ca_live(a)) return WBCA_ESTATE;
 if (!ca_external(a,claim,sizeof *claim) || !ca_external(a,capture,sizeof *capture)) return WBCA_EARG;
 if (ca_overlap(claim,sizeof *claim,capture,sizeof *capture) ||
     ca_pinned_overlap(a,capture,sizeof *capture)) return WBCA_EALIAS;
 if (claim->binding!=WBCA_HOST_CAPTURE_BOUND) return WBCA_EBINDING;
 if (claim->identity_lease==0u) return WBCA_EIDENTITY;
 if (claim->model!=wb_selected_profile.model) return WB_EPROFILE;
 if (claim->slot>=WBCA_CAPTURE_SLOTS) return WBCA_EARG;
 if (claim->snapshot_bytes!=wb_selected_profile.snapshot_bytes) return WB_ESIZE;
 if (!ca_external(a,claim->snapshot,claim->snapshot_bytes)) return WBCA_EARG;
 if (ca_overlap(capture,sizeof *capture,claim->snapshot,claim->snapshot_bytes)) return WBCA_EALIAS;
 if (a->current_valid!=1u) return WBCA_EREAD;
 c=&a->captures[claim->slot]; if (c->active!=0u) return WBCA_EBUSY;
 if (ca_pinned_overlap(a,claim->snapshot,claim->snapshot_bytes)) return WBCA_EBINDING;
 if (c->incarnation==UINT32_MAX || a->next_capture==UINT32_MAX) return WBCA_EEXHAUSTED;
 provenance.model=claim->model; provenance.mode_token=claim->mode_token;
 provenance.epoch=a->epoch; provenance.generation=a->generation;
 status=wb_shot_prepare(&shot,&a->current,&provenance); if (status!=WB_OK) return (wbca_status)status;
 token.model=claim->model; token.epoch=a->epoch; token.generation=a->generation;
 token.capture_id=a->next_capture+1u; token.slot=claim->slot; token.incarnation=c->incarnation+1u;
 token.mode_token=claim->mode_token; token.identity_lease=claim->identity_lease;
 ca_copy(&c->shot,&shot,sizeof shot); ca_copy(&c->token,&token,sizeof token);
 c->snapshot=claim->snapshot; c->snapshot_bytes=claim->snapshot_bytes;
 c->incarnation=token.incarnation; c->producer_open=1u; c->active=1u; ++a->next_capture;
 ca_copy(capture,&token,sizeof token); return WBCA_OK;
}
wbca_status wbca_capture_close(wbca_adapter *a,const wbca_capture_token *expected) {
 wbca_capture_record *c; wbca_status status=ca_capture(a,expected,&c);
 if (status!=WBCA_OK) return status;
 if (c->producer_open!=1u) return WBCA_ESTATE;
 c->producer_open=0u; ca_retire(a,c); return WBCA_OK;
}
wbca_status wbca_metadata_create(wbca_adapter *a,const wbca_capture_token *expected,wbca_metadata_token *metadata) {
 wbca_capture_record *c; wbca_metadata_record *m=NULL; uint32_t i;
 wbca_status status=ca_capture(a,expected,&c); if (status!=WBCA_OK) return status;
 if (!ca_output_span(a,metadata,sizeof *metadata,expected,sizeof *expected,expected)) return WBCA_EALIAS;
 if (c->producer_open!=1u) return WBCA_ESTATE;
 for (i=0u;i<WBCA_METADATA_CAPACITY;++i) if (a->metadata[i].used==0u) { m=&a->metadata[i]; break; }
 if (m==NULL) return WBCA_ECAPACITY;
 if (a->next_metadata==UINT32_MAX) return WBCA_EEXHAUSTED;
 ca_zero(m,sizeof *m); ca_copy(&m->token.capture,&c->token,sizeof c->token);
 m->token.instance_id=a->next_metadata+1u; m->used=1u; ++a->next_metadata;
 ca_copy(metadata,&m->token,sizeof *metadata); return WBCA_OK;
}
wbca_status wbca_metadata_compute(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t image_type,int32_t builder_status,uint32_t original_source,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,const uint8_t *snapshot,size_t snapshot_bytes,
 uint8_t *destination,size_t destination_bytes) {
 wbca_metadata_record *m; wbca_capture_record *c; uint8_t result[WB_NEUTRAL_BYTES],channels[6];
 uint32_t i; wb_status core;
 wbca_status status=ca_metadata(a,metadata,expected,&m); if (status!=WBCA_OK) return status;
 if (image_type!=WBCA_OUTPUT_DNG) return WBCA_EOUTPUT;
 if (builder_status!=WBCA_BUILDER_SUCCEEDED || original_source!=WBCA_FRESH_BUILDER_ORIGINAL) return WBCA_EBUILDER;
 if (original_bytes!=WB_NEUTRAL_BYTES || destination_bytes!=WB_NEUTRAL_BYTES ||
     snapshot_bytes!=wb_selected_profile.snapshot_bytes) return WB_ESIZE;
 if (order!=WB_LITTLE_ENDIAN && order!=WB_BIG_ENDIAN) return WB_EORDER;
 if (!ca_external(a,original,original_bytes) || !ca_external(a,snapshot,snapshot_bytes)) return WBCA_EARG;
 c=&a->captures[expected->slot];
 if (snapshot!=c->snapshot || snapshot_bytes!=c->snapshot_bytes) return WBCA_EBINDING;
 if (!ca_output_span(a,destination,destination_bytes,metadata,sizeof *metadata,expected) ||
     ca_overlap(destination,destination_bytes,original,original_bytes) ||
     ca_overlap(destination,destination_bytes,snapshot,snapshot_bytes)) return WBCA_EALIAS;
 for (i=0u;i<3u;++i) {
  uint32_t at=wb_selected_profile.wb_offset[i];
  if (at>snapshot_bytes || snapshot_bytes-at<2u) return WB_EPROFILE;
  channels[2u*i]=snapshot[at]; channels[2u*i+1u]=snapshot[at+1u];
 }
 if (m->computed!=0u) {
  if (m->order!=(uint32_t)order || !ca_equal(original,m->original,WB_NEUTRAL_BYTES) ||
      !ca_equal(channels,m->channels,sizeof channels)) return WBCA_ECHANGED;
  ca_copy(destination,m->result,WB_NEUTRAL_BYTES); return WBCA_OK;
 }
 core=wb_snapshot_apply(result,sizeof result,original,original_bytes,order,snapshot,snapshot_bytes,
  &c->shot,&c->shot.provenance);
 if (core!=WB_OK) return (wbca_status)core;
 ca_copy(m->original,original,WB_NEUTRAL_BYTES); ca_copy(m->result,result,WB_NEUTRAL_BYTES);
 ca_copy(m->channels,channels,sizeof channels); m->order=(uint32_t)order; m->computed=1u;
 ca_copy(destination,result,WB_NEUTRAL_BYTES); return WBCA_OK;
}
wbca_status wbca_metadata_copy(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint8_t *destination,size_t bytes) {
 wbca_metadata_record *m; wbca_status status=ca_metadata(a,metadata,expected,&m);
 if (status!=WBCA_OK) return status;
 if (bytes!=WB_NEUTRAL_BYTES) return WB_ESIZE;
 if (!ca_output_span(a,destination,bytes,metadata,sizeof *metadata,expected)) return WBCA_EALIAS;
 if (m->computed!=1u) return WBCA_ESTATE;
 ca_copy(destination,m->result,bytes); return WBCA_OK;
}
wbca_status wbca_metadata_release(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected) {
 wbca_metadata_record *m; uint32_t i; wbca_status status=ca_metadata(a,metadata,expected,&m);
 if (status!=WBCA_OK) return status;
 for (i=0u;i<WBCA_SAVE_CAPACITY;++i) if (a->saves[i].used!=0u &&
  ca_equal(&a->saves[i].token.metadata,&m->token,sizeof m->token) && !ca_terminal(a->saves[i].state)) return WBCA_EBUSY;
 for (i=0u;i<WBCA_SAVE_CAPACITY;++i) if (a->saves[i].used!=0u &&
  ca_equal(&a->saves[i].token.metadata,&m->token,sizeof m->token)) ca_zero(&a->saves[i],sizeof a->saves[i]);
 ca_zero(m,sizeof *m); ca_retire(a,&a->captures[expected->slot]); return WBCA_OK;
}
static wbca_status ca_submit(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t media,uint32_t output,uint32_t count,wbca_save_token *tickets) {
 wbca_metadata_record *m; wbca_save_record *first=NULL,*second=NULL; uint32_t i;
 wbca_save_token token; wbca_status status=ca_metadata(a,metadata,expected,&m);
 if (status!=WBCA_OK) return status;
 if (!ca_output_span(a,tickets,sizeof *tickets*count,metadata,sizeof *metadata,expected)) return WBCA_EALIAS;
 if (media!=WBCA_MEDIA_SD && media!=WBCA_MEDIA_USB) return WBCA_EOUTPUT;
 if (output!=WBCA_OUTPUT_DNG && output!=WBCA_OUTPUT_JPEG) return WBCA_EOUTPUT;
 if (m->computed!=1u) return WBCA_ESTATE;
 for (i=0u;i<WBCA_SAVE_CAPACITY;++i) if (a->saves[i].used==0u) {
  if (first==NULL) first=&a->saves[i]; else { second=&a->saves[i]; break; }
 }
 if (first==NULL || (count==2u && second==NULL)) return WBCA_ECAPACITY;
 if (a->next_job>UINT32_MAX-count || a->next_attempt>UINT32_MAX-count) return WBCA_EEXHAUSTED;
 ca_copy(&token.metadata,metadata,sizeof *metadata); token.media=media; token.output=output;
 token.job_id=a->next_job+1u; token.attempt_id=a->next_attempt+1u;
 ca_copy(&first->token,&token,sizeof token); first->used=1u; first->state=WBCA_SAVE_QUEUED;
 ca_copy(tickets,&token,sizeof token);
 if (count==2u) {
  ++token.job_id; ++token.attempt_id; token.output=WBCA_OUTPUT_JPEG;
  ca_copy(&second->token,&token,sizeof token); second->used=1u; second->state=WBCA_SAVE_QUEUED;
  ca_copy(tickets+1,&token,sizeof token);
 }
 a->next_job+=count; a->next_attempt+=count; return WBCA_OK;
}
wbca_status wbca_save_submit(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t media,uint32_t output,wbca_save_token *ticket) {
 return ca_submit(a,metadata,expected,media,output,1u,ticket);
}
wbca_status wbca_save_pair(wbca_adapter *a,const wbca_metadata_token *metadata,
 const wbca_capture_token *expected,uint32_t media,wbca_save_token tickets[2]) {
 return ca_submit(a,metadata,expected,media,WBCA_OUTPUT_DNG,2u,tickets);
}
static wbca_status ca_transition(wbca_adapter *a,const wbca_save_token *ticket,
 const wbca_capture_token *expected,uint32_t from,uint32_t to) {
 wbca_save_record *s; wbca_metadata_record *m; wbca_status status=ca_save(a,ticket,expected,&s,&m);
 if (status!=WBCA_OK) return status;
 if (s->state!=from) return WBCA_ESTATE;
 s->state=to; return WBCA_OK;
}
wbca_status wbca_save_start(wbca_adapter *a,const wbca_save_token *ticket,const wbca_capture_token *expected) {
 return ca_transition(a,ticket,expected,WBCA_SAVE_QUEUED,WBCA_SAVE_INFLIGHT);
}
wbca_status wbca_save_fail(wbca_adapter *a,const wbca_save_token *ticket,const wbca_capture_token *expected) {
 return ca_transition(a,ticket,expected,WBCA_SAVE_INFLIGHT,WBCA_SAVE_FAILED);
}
wbca_status wbca_save_complete(wbca_adapter *a,const wbca_save_token *ticket,const wbca_capture_token *expected) {
 return ca_transition(a,ticket,expected,WBCA_SAVE_INFLIGHT,WBCA_SAVE_COMPLETED);
}
wbca_status wbca_save_abandon(wbca_adapter *a,const wbca_save_token *ticket,const wbca_capture_token *expected) {
 return ca_transition(a,ticket,expected,WBCA_SAVE_FAILED,WBCA_SAVE_ABANDONED);
}
wbca_status wbca_save_retry(wbca_adapter *a,const wbca_save_token *ticket,
 const wbca_capture_token *expected,wbca_save_token *next) {
 wbca_save_record *s; wbca_metadata_record *m; wbca_save_token token;
 wbca_status status=ca_save(a,ticket,expected,&s,&m);
 if (status!=WBCA_OK) return status;
 if (!ca_output_span(a,next,sizeof *next,ticket,sizeof *ticket,expected)) return WBCA_EALIAS;
 if (s->state!=WBCA_SAVE_FAILED) return WBCA_ESTATE;
 if (a->next_attempt==UINT32_MAX) return WBCA_EEXHAUSTED;
 ca_copy(&token,&s->token,sizeof token); token.attempt_id=a->next_attempt+1u;
 ca_copy(&s->token,&token,sizeof token); s->state=WBCA_SAVE_QUEUED; ++a->next_attempt;
 ca_copy(next,&token,sizeof token); return WBCA_OK;
}
wbca_status wbca_save_cancel(wbca_adapter *a,const wbca_save_token *ticket,const wbca_capture_token *expected) {
 wbca_save_record *s; wbca_metadata_record *m; wbca_status status=ca_save(a,ticket,expected,&s,&m);
 if (status!=WBCA_OK) return status;
 if (s->state==WBCA_SAVE_INFLIGHT) s->state=WBCA_SAVE_CANCEL_PENDING;
 else if (s->state==WBCA_SAVE_QUEUED || s->state==WBCA_SAVE_FAILED) s->state=WBCA_SAVE_CANCELLED;
 else return WBCA_ESTATE;
 return WBCA_OK;
}
wbca_status wbca_save_ack_cancel(wbca_adapter *a,const wbca_save_token *ticket,
 const wbca_capture_token *expected,uint32_t owner_quiescent) {
 if (owner_quiescent!=WBCA_OWNER_QUIESCENT) return WBCA_EQUIESCENCE;
 return ca_transition(a,ticket,expected,WBCA_SAVE_CANCEL_PENDING,WBCA_SAVE_CANCELLED);
}
wbca_status wbca_save_copy(wbca_adapter *a,const wbca_save_token *ticket,
 const wbca_capture_token *expected,uint8_t *destination,size_t bytes) {
 wbca_save_record *s; wbca_metadata_record *m; wbca_status status=ca_save(a,ticket,expected,&s,&m);
 if (status!=WBCA_OK) return status;
 if (s->state!=WBCA_SAVE_INFLIGHT) return WBCA_ESTATE;
 if (ticket->output!=WBCA_OUTPUT_DNG) return WBCA_EOUTPUT;
 if (bytes!=WB_NEUTRAL_BYTES) return WB_ESIZE;
 if (!ca_output_span(a,destination,bytes,ticket,sizeof *ticket,expected)) return WBCA_EALIAS;
 if (m->computed!=1u) return WBCA_ESTATE;
 ca_copy(destination,m->result,bytes); return WBCA_OK;
}
