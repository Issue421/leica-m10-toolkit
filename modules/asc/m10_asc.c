#include "m10_asc.h"
#ifdef M10_TARGET_ABI
_Static_assert(sizeof(void *)==4 && sizeof(size_t)==4,"AAPCS32 only");
#endif
#define STRIDE 2704u
static uint32_t le32(const uint8_t *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void copy_bytes(void *d,const void *s,size_t n) {
 volatile uint8_t *dst=(volatile uint8_t *)d; const uint8_t *src=(const uint8_t *)s;
 size_t i; for(i=0;i<n;i++) dst[i]=src[i];
}
static void zero_bytes(void *d,size_t n) {
 volatile uint8_t *dst=(volatile uint8_t *)d; size_t i; for(i=0;i<n;i++) dst[i]=0;
}
static int name_valid(const uint8_t *s) {
 uint32_t i; for(i=0;i<64u;i++) { if(s[i]==0u)return 1; if(s[i]>127u)return 0; } return 0;
}
static int name_equal(const uint8_t *s,const char *q) {
 uint32_t i; for(i=0;i<64u;i++) { if(s[i]!=(uint8_t)q[i])return 0; if(s[i]==0u)return 1; } return 0;
}
m10_status m10_b2y_parse(const uint8_t *b,uint32_t mapped,uint32_t registered,
 uint32_t read,uint32_t epoch,m10_resource *out) {
 uint32_t count,data_at,i,j,at,n,off,size; m10_resource r;
 if(b==NULL||out==NULL)return M10_ARGUMENT;
 if(mapped!=registered||read!=registered)return M10_SHORT_READ;
 if(mapped<16u)return M10_FORMAT;
 count=le32(b+8);
 if(le32(b)!=0xca11bf11u||le32(b+4)!=1u||le32(b+12)!=0u||count==0u||count>1000u)return M10_FORMAT;
 data_at=16u+count*STRIDE; if(data_at>mapped)return M10_FORMAT;
 for(i=0;i<count;i++) {
  at=16u+i*STRIDE; off=le32(b+at+4); size=le32(b+at+8); n=le32(b+at+12);
  if(n==0u||n>42u||size==0u||off>mapped-data_at||size>mapped-data_at-off)return M10_FORMAT;
  for(j=0;j<n;j++)if(!name_valid(b+at+16u+j*64u))return M10_FORMAT;
 }
 r.body=b;r.bytes=mapped;r.count=count;r.data_at=data_at;r.epoch=epoch;
 copy_bytes(out,&r,sizeof r);return M10_OK;
}
m10_status m10_b2y_query(const m10_resource *r,uint32_t category,const char *q,m10_entry *out) {
 uint32_t i,j,n,at; m10_entry found; m10_resource check; m10_status st; int any=0;
 if(r==NULL||q==NULL||out==NULL)return M10_ARGUMENT;
 for(i=0;i<64u;i++) { if(q[i]==0)break; if((uint8_t)q[i]>127u)return M10_ARGUMENT; }
 if(i==64u)return M10_ARGUMENT;
 st=m10_b2y_parse(r->body,r->bytes,r->bytes,r->bytes,r->epoch,&check);if(st!=M10_OK)return st;
 if(check.count!=r->count||check.data_at!=r->data_at)return M10_FORMAT;
 for(i=0;i<r->count;i++) {
  at=16u+i*STRIDE;if(le32(r->body+at)!=category)continue;n=le32(r->body+at+12);
  for(j=0;j<n;j++)if(name_equal(r->body+at+16u+j*64u,q)) {
   found.payload=r->body+r->data_at+le32(r->body+at+4);found.bytes=le32(r->body+at+8);
   found.category=category;found.index=i;any=1;break;
  }
 }
 if(!any)return M10_MISSING;copy_bytes(out,&found,sizeof found);return M10_OK;
}
/* SHA256 over the fixed complete resource. Digest is an integrity/version pin,
   never native identity authentication. All arithmetic is 32 bit. */
static uint32_t rr(uint32_t x,uint32_t n){return (x>>n)|(x<<(32u-n));}
static const uint32_t k[64]={
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
static void block(uint32_t h[8],const uint8_t b[64]) {
 uint32_t w[64],a,c,d,e,f,g,t,bb,z,x,i,s0,s1;
 for(i=0;i<16u;i++)w[i]=((uint32_t)b[4u*i]<<24)|((uint32_t)b[4u*i+1u]<<16)|((uint32_t)b[4u*i+2u]<<8)|b[4u*i+3u];
 for(i=16;i<64u;i++){s0=rr(w[i-15u],7)^rr(w[i-15u],18)^(w[i-15u]>>3);s1=rr(w[i-2u],17)^rr(w[i-2u],19)^(w[i-2u]>>10);w[i]=w[i-16u]+s0+w[i-7u]+s1;}
 a=h[0];bb=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];z=h[7];
 for(i=0;i<64u;i++){t=z+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^((~e)&g))+k[i]+w[i];x=(rr(a,2)^rr(a,13)^rr(a,22))+((a&bb)^(a&c)^(bb&c));z=g;g=f;f=e;e=d+t;d=c;c=bb;bb=a;a=t+x;}
 h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
}
m10_status m10_b2y_pin(const m10_resource *r) {
 static const uint32_t expected[8]={0x0ed4a05cu,0xc5b74c07u,0x108c0d7eu,0x6ab531a7u,0xa86a7984u,0x20a246e2u,0xdafa8a1du,0xa1efcaf3u};
 static const uint32_t initial[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
 uint32_t h[8];uint8_t tail[64];uint32_t at=0,n,i,bits;
 copy_bytes(h,initial,sizeof h);
 if(r==NULL||r->body==NULL)return M10_ARGUMENT;
 if(r->bytes!=1678832u||r->count!=105u||r->data_at!=283936u)return M10_OUTDATED;
 while(r->bytes-at>=64u){block(h,r->body+at);at+=64u;}
 n=r->bytes-at;zero_bytes(tail,sizeof tail);copy_bytes(tail,r->body+at,n);tail[n]=128;
 if(n>=56u){block(h,tail);zero_bytes(tail,sizeof tail);}
 bits=r->bytes>>29;for(i=0;i<4u;i++)tail[59u-i]=(uint8_t)(bits>>(8u*i));
 bits=r->bytes<<3;for(i=0;i<4u;i++)tail[63u-i]=(uint8_t)(bits>>(8u*i));block(h,tail);
 for(i=0;i<8u;i++)if(h[i]!=expected[i])return M10_OUTDATED;return M10_OK;
}
m10_status m10_cc1_pack(const m10_entry *e,const uint8_t *runtime,uint32_t n,m10_look look,m10_field out[9]) {
 m10_field f[9];uint32_t i,v,typ,idx,shift;const uint8_t *p;
 if(e==NULL||out==NULL||e->payload==NULL||(uint32_t)look>2u)return M10_ARGUMENT;
 if(e->bytes!=68u)return M10_SIZE;
 if(look==M10_NORMAL&&(runtime==NULL||n<92u))return M10_SIZE;
 zero_bytes(f,sizeof f);p=e->payload;typ=look==M10_NORMAL?le32(runtime+88):le32(p+4);
 f[0].mask=0x31u;f[0].value=((uint32_t)p[0]&1u)|((typ&3u)<<4);
 for(i=0;i<9u;i++) {
  v=look==M10_NORMAL?le32(runtime+52u+4u*i):le32(p+8u+4u*i);
  idx=1u+(i>>1);shift=(i&1u)*16u;f[idx].offset=0x20u+(i>>1)*4u;
  f[idx].mask|=0xfffu<<shift;f[idx].value|=(v&0xfffu)<<shift;
 }
 for(i=0;i<3u;i++){f[6u+i].offset=0x40u+4u*i;f[6u+i].mask=0xffff7fffu;f[6u+i].value=(le32(p+44u+8u*i)&0x7fffu)|((le32(p+48u+8u*i)&0xffffu)<<16);}
 copy_bytes(out,f,sizeof f);return M10_OK;
}
m10_status m10_gamma_consumer(const m10_entry *e,uint32_t channel,uint32_t small,uint32_t fallback,m10_gamma_view *out) {
 m10_gamma_view v;uint32_t consume=small?4096u:32768u;
 if(out==NULL||channel<1u||channel>5u||small>1u||fallback>1u)return M10_ARGUMENT;
 zero_bytes(&v,sizeof v);v.channel=channel;v.small=small;v.fallback=fallback;
 if(fallback){v.fallback_mask=3u<<24;v.fallback_value=small?0u:1u<<25;}
 v.bank=channel==5u?(small?0x20a68000u:0x20a40000u):(small?(0x20a68000u+channel*4096u):(0x20a40000u+channel*32768u));
 if(!fallback){
  if(e==NULL||e->payload==NULL)return M10_MISSING;
  if(e->bytes!=consume*(channel==5u?1u:2u))return M10_SIZE;
  v.payload=e->payload;v.full_bytes=e->bytes;v.consume_bytes=consume;
 }
 copy_bytes(out,&v,sizeof v);return M10_OK;
}
static m10_status need(const m10_resource *r,uint32_t cat,const char *name,uint32_t bytes,m10_entry *e){
 m10_status st=m10_b2y_query(r,cat,name,e);if(st!=M10_OK)return st;return e->bytes==bytes?M10_OK:M10_SIZE;
}
m10_status m10_asc_preflight(const m10_resource *r,const uint8_t *rt,uint32_t rn,m10_epochs ep,m10_look look,m10_mode mode,m10_plan *out) {
 static const char *const modes[4]={"_B2YMODE:SLIVE","_B2YMODE:SMAGNI","_B2YMODE:STILL","_B2YMODE:STILLLINK"};
 static const char *const films[3]={"","_FILMMODE:ASCCLASSIC","_FILMMODE:ASCCONTEMPORARY"};
 m10_plan p;m10_status st;m10_entry e;uint32_t i,c,ch;const char *name;
 if(r==NULL||out==NULL||(uint32_t)look>2u||(uint32_t)mode>3u)return M10_ARGUMENT;
 if(ep.resource==0u||ep.calibration==0u||ep.hardware==0u||ep.resource!=r->epoch)return M10_EPOCH;
 st=m10_b2y_pin(r);if(st!=M10_OK)return st;
 zero_bytes(&p,sizeof p);p.look=look;p.mode=mode;copy_bytes(&p.epochs,&ep,sizeof ep);
 st=need(r,10u,films[look],68u,&p.cc1);if(st!=M10_OK)return st;
 st=m10_cc1_pack(&p.cc1,rt,rn,look,p.cc1_fields);if(st!=M10_OK)return st;
 name=look==M10_NORMAL?modes[mode]:films[look];
 st=need(r,11u,name,8u,&p.control);if(st!=M10_OK)return st;
 if(le32(p.control.payload)>1u||le32(p.control.payload+4)>1u)return M10_FORMAT;
 p.control_mask=3u<<24;p.control_value=(le32(p.control.payload)<<24)|(le32(p.control.payload+4)<<25);
 if(look==M10_NORMAL&&p.control_value!=(3u<<24))return M10_FORMAT;
 p.gamma_count=look==M10_NORMAL?2u:8u;
 for(i=0;i<p.gamma_count;i++) {
  ch=look==M10_NORMAL?5u:1u+(i>>1);c=look==M10_NORMAL?28u+i:39u+i;
  st=need(r,c,name,(i&1u)?(ch==5u?4096u:8192u):(ch==5u?32768u:65536u),&e);if(st!=M10_OK)return st;
  st=m10_gamma_consumer(&e,ch,i&1u,0u,&p.gamma[i]);if(st!=M10_OK)return st;
 }
 copy_bytes(out,&p,sizeof p);return M10_OK;
}
/* No cached owner model: original scheduler/cache operations are named imports.
   Inputs are acquired once and held; all preflight precedes enter-idle. */
m10_result m10_asc_apply(m10_look look,m10_mode mode) {
 m10_native_inputs in;m10_resource r;m10_plan p;m10_result result;m10_status st;uint32_t i;
 zero_bytes(&result,sizeof result);zero_bytes(&in,sizeof in);st=m10_native_acquire_current(&in);
 if(st!=M10_OK){result.status=st;result.cause=st;return result;}
 result.stage=1u;if(in.current_mode!=mode){st=M10_EPOCH;goto done;}
 st=m10_b2y_parse(in.resource,in.mapped,in.registered,in.bytes_read,in.epochs.resource,&r);
 if(st==M10_OK)st=m10_asc_preflight(&r,in.runtime,in.runtime_bytes,in.epochs,look,mode,&p);
 if(st!=M10_OK)goto done;
 result.stage=2u;st=m10_native_enter_confirmed_idle();if(st!=M10_OK)goto abort;result.idle_confirmed=1u;
 result.stage=3u;st=m10_native_check_current_epochs(in.epochs);if(st!=M10_OK)goto abort;
 result.stage=4u;st=m10_native_invalidate_both_caches();if(st!=M10_OK)goto abort;
 result.caches_invalid_confirmed=1u;
 result.stage=5u;st=m10_native_verify_idle_cacheinvalid();if(st!=M10_OK)goto abort;
 result.stage=6u;st=m10_native_write_cc1(p.cc1_fields);if(st!=M10_OK)goto abort;
 for(i=0;i<p.gamma_count;i++){result.stage=7u+i;st=m10_native_transfer_gamma(&p.gamma[i]);if(st!=M10_OK)goto abort;}
 result.stage=15u;st=m10_native_write_gamma_control(p.control_mask,p.control_value);if(st!=M10_OK)goto abort;
 result.stage=16u;st=m10_native_check_current_epochs(in.epochs);if(st!=M10_OK)goto abort;
 result.stage=17u;st=m10_native_verify_complete_idle(&p);if(st!=M10_OK)goto abort;
 result.stage=18u;st=m10_native_check_current_epochs(in.epochs);if(st!=M10_OK)goto abort;
 st=m10_b2y_pin(&r);if(st!=M10_OK)goto abort;
 if(look==M10_NORMAL){m10_field current[9];st=m10_cc1_pack(&p.cc1,in.runtime,in.runtime_bytes,look,current);if(st!=M10_OK)goto abort;
 for(i=0;i<9u;i++)if(current[i].value!=p.cc1_fields[i].value){st=M10_EPOCH;goto abort;}}
 result.stage=19u;st=m10_native_publish_and_resume(&p);if(st!=M10_OK)goto abort;
 result.idle_confirmed=0u;result.caches_invalid_confirmed=0u;
 result.stage=20u;st=m10_native_verify_running(&p);if(st!=M10_OK)goto abort;
 goto done;
abort:
 result.cause=st;result.recovery_status=m10_native_abort_quiescent_cacheinvalid();
 if(result.recovery_status!=M10_OK){st=M10_RECOVERY_REQUIRED;result.recovery_required=1u;result.idle_confirmed=0u;result.caches_invalid_confirmed=0u;}
 else {result.idle_confirmed=1u;result.caches_invalid_confirmed=1u;}
done:
 result.status=st;if(result.cause==M10_OK)result.cause=st;m10_native_release_current();return result;
}
