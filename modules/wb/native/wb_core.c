#include "wb_target.h"
static void copy_bytes(void *destination,const void *source,size_t bytes) {
 uint8_t *d=(uint8_t *)destination; const uint8_t *s=(const uint8_t *)source;
 while (bytes!=0u) { *d++=*s++; --bytes; }
}
static void zero_bytes(void *destination,size_t bytes) {
 uint8_t *d=(uint8_t *)destination;
 while (bytes!=0u) { *d++=0u; --bytes; }
}
static int overlap(const void *a,size_t an,const void *b,size_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 if (an==0u || bn==0u) return 0;
 if (x>UINTPTR_MAX-(an-1u) || y>UINTPTR_MAX-(bn-1u)) return 1;
 return x<=y+bn-1u && y<=x+an-1u;
}
static uint32_t read32(const uint8_t *p,wb_byte_order order) {
 if (order==WB_LITTLE_ENDIAN)
  return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
 return (uint32_t)p[3]|((uint32_t)p[2]<<8)|((uint32_t)p[1]<<16)|((uint32_t)p[0]<<24);
}
/* Keep fixed byte stores inline: -Oz otherwise chooses a function literal/BLX. */
static inline __attribute__((always_inline)) void write32(uint8_t *p,uint32_t value,wb_byte_order order) {
 unsigned int i;
 for (i=0u;i<4u;++i) {
  unsigned int shift=order==WB_LITTLE_ENDIAN ? i*8u : (3u-i)*8u;
  p[i]=(uint8_t)(value>>shift);
 }
}
/* Exactly 32 iterations. carry represents the 33rd remainder bit. d>0 is an
 * internal precondition established by positive-factor/gcd validation. */
static uint32_t divmod32(uint32_t n,uint32_t d,uint32_t *remainder) {
 uint32_t q=0u,r=0u; unsigned int bit=32u;
 while (bit!=0u) {
  uint32_t carry=r>>31; --bit;
  r=(r<<1)|((n>>bit)&1u);
  if (carry!=0u || r>=d) { r-=d; q|=UINT32_C(1)<<bit; }
 }
 *remainder=r; return q;
}
static uint32_t divide32(uint32_t n,uint32_t d) {
 uint32_t r; return divmod32(n,d,&r);
}
static uint32_t gcd32(uint32_t a,uint32_t b) {
 while (b!=0u) { uint32_t r; (void)divmod32(a,b,&r); a=b; b=r; }
 return a;
}
static uint32_t ceil_quarter(uint32_t value) {
 return (value>>2)+(uint32_t)((value&3u)!=0u);
}
static int valid_gain(wb_gain gain) {
 return gain.numerator!=0u && gain.denominator!=0u &&
  gain.numerator>=ceil_quarter(gain.denominator) &&
  gain.denominator>=ceil_quarter(gain.numerator);
}
static int equal_gain(wb_gain a,wb_gain b) {
 uint32_t ga=gcd32(a.numerator,a.denominator),gb=gcd32(b.numerator,b.denominator);
 return divide32(a.numerator,ga)==divide32(b.numerator,gb) &&
  divide32(a.denominator,ga)==divide32(b.denominator,gb);
}
static wb_status product32(const uint32_t factors[4],uint32_t *result) {
 uint32_t value=1u; unsigned int i;
 for (i=0u;i<4u;++i) {
  if (value>divide32(UINT32_MAX,factors[i])) return WB_EOVERFLOW;
  value*=factors[i];
 }
 *result=value; return WB_OK;
}
static wb_status ratio(wb_gain n,wb_gain green,wb_gain c,wb_gain cg,wb_gain *result) {
 uint32_t numerator[4],denominator[4]; unsigned int i,j; wb_status status;
 numerator[0]=n.numerator; numerator[1]=c.denominator;
 numerator[2]=green.denominator; numerator[3]=cg.numerator;
 denominator[0]=n.denominator; denominator[1]=c.numerator;
 denominator[2]=green.numerator; denominator[3]=cg.denominator;
 for (i=0u;i<4u;++i) for (j=0u;j<4u;++j) {
  uint32_t divisor=gcd32(numerator[i],denominator[j]);
  numerator[i]=divide32(numerator[i],divisor);
  denominator[j]=divide32(denominator[j],divisor);
 }
 status=product32(numerator,&result->numerator);
 if (status!=WB_OK) return status;
 return product32(denominator,&result->denominator);
}
wb_status wb_neutral_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const void *gains,size_t gains_count) {
 uint8_t input[24],output[24]; wb_gain local[3],neutral[3],result;
 unsigned int i; wb_status status;
 if (destination==NULL || original==NULL || gains==NULL) return WB_EARG;
 if (destination_bytes!=24u || original_bytes!=24u || gains_count!=3u) return WB_ESIZE;
 if (order!=WB_LITTLE_ENDIAN && order!=WB_BIG_ENDIAN) return WB_EORDER;
 copy_bytes(input,original,24u); copy_bytes(local,gains,sizeof local);
 for (i=0u;i<3u;++i) {
  neutral[i].numerator=read32(input+i*8u,order);
  neutral[i].denominator=read32(input+i*8u+4u,order);
  if (neutral[i].numerator==0u || neutral[i].denominator==0u) return WB_ENEUTRAL;
  if (!valid_gain(local[i])) return WB_EGAIN;
 }
 if (equal_gain(local[0],local[1]) && equal_gain(local[1],local[2])) {
  copy_bytes(destination,input,24u); return WB_OK;
 }
 for (i=0u;i<3u;++i) {
  status=ratio(neutral[i],neutral[1],local[i],local[1],&result);
  if (status!=WB_OK) return status;
  write32(output+i*8u,result.numerator,order);
  write32(output+i*8u+4u,result.denominator,order);
 }
 copy_bytes(destination,output,24u); return WB_OK;
}
static int hash_equal(const uint8_t *a,const uint8_t *b) {
 unsigned int i; for (i=0u;i<32u;++i) if (a[i]!=b[i]) return 0;
 return 1;
}
wb_status wb_config_validate(const wb_config *config) {
 wb_config local; unsigned int i;
 if (config==NULL) return WB_EARG;
 copy_bytes(&local,config,sizeof local);
 if (local.version!=WB_CONFIG_VERSION || local.enabled>1u ||
     local.epoch==0u || local.generation==0u) return WB_ECONFIG;
 if (local.model!=wb_selected_profile.model ||
     !hash_equal(local.img_sha256,wb_selected_profile.img_sha256)) return WB_EPROFILE;
 for (i=0u;i<3u;++i) if (!valid_gain(local.gains[i])) return WB_EGAIN;
 return WB_OK;
}
wb_status wb_config_defaults(wb_config *destination,uint32_t epoch,uint32_t generation) {
 wb_config local; unsigned int i;
 if (destination==NULL) return WB_EARG;
 if (epoch==0u || generation==0u) return WB_ECONFIG;
 zero_bytes(&local,sizeof local); local.version=WB_CONFIG_VERSION;
 local.model=wb_selected_profile.model; local.epoch=epoch; local.generation=generation;
 copy_bytes(local.img_sha256,wb_selected_profile.img_sha256,32u);
 for (i=0u;i<3u;++i) { local.gains[i].numerator=1u; local.gains[i].denominator=1u; }
 copy_bytes(destination,&local,sizeof local); return WB_OK;
}
static uint32_t crc32(const uint8_t *bytes,size_t count) {
 uint32_t crc=UINT32_MAX; size_t i; unsigned int bit;
 for (i=0u;i<count;++i) {
  crc^=bytes[i];
  for (bit=0u;bit<8u;++bit) crc=(crc>>1)^((crc&1u)!=0u ? UINT32_C(0xedb88320) : 0u);
 }
 return crc^UINT32_MAX;
}
wb_status wb_config_encode(uint8_t *destination,size_t bytes,const wb_config *config) {
 wb_config local; uint8_t output[88]; unsigned int i; wb_status status;
 if (destination==NULL || config==NULL) return WB_EARG;
 if (bytes!=88u) return WB_ESIZE;
 copy_bytes(&local,config,sizeof local); status=wb_config_validate(&local);
 if (status!=WB_OK) return status;
 zero_bytes(output,sizeof output); output[0]='W';output[1]='B';output[2]='F';output[3]='T';
 write32(output+4u,local.version,WB_LITTLE_ENDIAN);
 write32(output+8u,local.model,WB_LITTLE_ENDIAN);copy_bytes(output+12u,local.img_sha256,32u);
 write32(output+44u,local.enabled,WB_LITTLE_ENDIAN);
 write32(output+48u,local.epoch,WB_LITTLE_ENDIAN);
 write32(output+52u,local.generation,WB_LITTLE_ENDIAN);
 for (i=0u;i<3u;++i) { write32(output+56u+i*8u,local.gains[i].numerator,WB_LITTLE_ENDIAN);
  write32(output+60u+i*8u,local.gains[i].denominator,WB_LITTLE_ENDIAN); }
 write32(output+84u,crc32(output,84u),WB_LITTLE_ENDIAN);
 copy_bytes(destination,output,sizeof output);return WB_OK;
}
wb_status wb_config_decode(wb_config *destination,const uint8_t *source,size_t bytes) {
 uint8_t input[88]; wb_config local; unsigned int i; wb_status status;
 if (destination==NULL || source==NULL) return WB_EARG;
 if (bytes!=88u) return WB_ESIZE;
 copy_bytes(input,source,sizeof input);
 if (input[0]!='W' || input[1]!='B' || input[2]!='F' || input[3]!='T' ||
     read32(input+80u,WB_LITTLE_ENDIAN)!=0u ||
     read32(input+84u,WB_LITTLE_ENDIAN)!=crc32(input,84u)) return WB_ECONFIG;
 local.version=read32(input+4u,WB_LITTLE_ENDIAN);local.model=read32(input+8u,WB_LITTLE_ENDIAN);
 copy_bytes(local.img_sha256,input+12u,32u);
 local.enabled=read32(input+44u,WB_LITTLE_ENDIAN);local.epoch=read32(input+48u,WB_LITTLE_ENDIAN);
 local.generation=read32(input+52u,WB_LITTLE_ENDIAN);
 for (i=0u;i<3u;++i) { local.gains[i].numerator=read32(input+56u+i*8u,WB_LITTLE_ENDIAN);
  local.gains[i].denominator=read32(input+60u+i*8u,WB_LITTLE_ENDIAN); }
 status=wb_config_validate(&local);if (status!=WB_OK) return status;
 copy_bytes(destination,&local,sizeof local);return WB_OK;
}
static wb_status provenance_check(const wb_config *config,const wb_provenance *provenance) {
 if (provenance->model!=config->model) return WB_EPROFILE;
 if (provenance->epoch!=config->epoch || provenance->generation!=config->generation) return WB_ESTALE;
 return WB_OK;
}
wb_status wb_shot_prepare(wb_shot *destination,const wb_config *config,const wb_provenance *provenance) {
 wb_shot local; wb_status status;
 if (destination==NULL || config==NULL || provenance==NULL) return WB_EARG;
 if (overlap(destination,sizeof *destination,config,sizeof *config) ||
     overlap(destination,sizeof *destination,provenance,sizeof *provenance)) return WB_EALIAS;
 copy_bytes(&local.config,config,sizeof local.config);
 copy_bytes(&local.provenance,provenance,sizeof local.provenance);
 status=wb_config_validate(&local.config); if (status!=WB_OK) return status;
 status=provenance_check(&local.config,&local.provenance);if (status!=WB_OK) return status;
 copy_bytes(destination,&local,sizeof local);return WB_OK;
}
static wb_status shot_check(const wb_shot *shot,const wb_provenance *expected) {
 wb_status status=wb_config_validate(&shot->config);
 if (status!=WB_OK) return status;
 status=provenance_check(&shot->config,&shot->provenance);if (status!=WB_OK) return status;
 status=provenance_check(&shot->config,expected);if (status!=WB_OK) return status;
 if (shot->provenance.mode_token!=expected->mode_token) return WB_ESTALE;
 return WB_OK;
}
wb_status wb_shot_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const wb_shot *shot,const wb_provenance *expected) {
 wb_gain gains[3]; unsigned int i; wb_status status;
 if (destination==NULL || original==NULL || shot==NULL || expected==NULL) return WB_EARG;
 if (destination_bytes!=24u || original_bytes!=24u) return WB_ESIZE;
 if (order!=WB_LITTLE_ENDIAN && order!=WB_BIG_ENDIAN) return WB_EORDER;
 if (overlap(destination,24u,shot,sizeof *shot) || overlap(destination,24u,expected,sizeof *expected)) return WB_EALIAS;
 status=shot_check(shot,expected);if (status!=WB_OK) return status;
 for (i=0u;i<3u;++i) {
  if (shot->config.enabled!=0u) copy_bytes(&gains[i],&shot->config.gains[i],sizeof gains[i]);
  else { gains[i].numerator=1u;gains[i].denominator=1u; }
 }
 return wb_neutral_apply(destination,destination_bytes,original,original_bytes,order,gains,3u);
}
wb_status wb_snapshot_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const uint8_t *snapshot,size_t snapshot_bytes,
 const wb_shot *shot,const wb_provenance *expected) {
 unsigned int i; wb_status status;
 if (destination==NULL || original==NULL || snapshot==NULL || shot==NULL || expected==NULL) return WB_EARG;
 if (destination_bytes!=24u || original_bytes!=24u || snapshot_bytes!=wb_selected_profile.snapshot_bytes) return WB_ESIZE;
 if (order!=WB_LITTLE_ENDIAN && order!=WB_BIG_ENDIAN) return WB_EORDER;
 if (overlap(destination,24u,original,24u) || overlap(destination,24u,snapshot,snapshot_bytes) ||
     overlap(destination,24u,shot,sizeof *shot) || overlap(destination,24u,expected,sizeof *expected)) return WB_EALIAS;
 status=shot_check(shot,expected);if (status!=WB_OK) return status;
 for (i=0u;i<3u;++i) {
  uint32_t at=wb_selected_profile.wb_offset[i]; uint32_t channel;
  if (at>snapshot_bytes || snapshot_bytes-at<2u) return WB_EPROFILE;
  /* high bit is the signed16 sign; reject before interpreting as a u32 divisor. */
  if ((snapshot[at+1u]&0x80u)!=0u || (snapshot[at]==0u && snapshot[at+1u]==0u)) return WB_ESIGNED;
  channel=(uint32_t)snapshot[at]|((uint32_t)snapshot[at+1u]<<8);
  if (read32(original+i*8u,order)!=256u || read32(original+i*8u+4u,order)!=channel) return WB_EBUILDER;
 }
 return wb_shot_apply(destination,destination_bytes,original,original_bytes,order,shot,expected);
}
