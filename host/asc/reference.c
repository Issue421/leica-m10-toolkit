/* MIT. Parse/plan real pinned local resources; no pixel renderer or MMIO. */
#include "m10_asc.h"
int reference_asc(const uint8_t *body,uint32_t bytes,unsigned look,unsigned mode,uint32_t out[50]){
 m10_resource r;m10_plan p;m10_epochs epochs={1,1,1};m10_status status;unsigned i;uint8_t runtime[92]={0};
 status=m10_b2y_parse(body,bytes,bytes,bytes,1,&r);if(status!=M10_OK)return (int)status;
 status=m10_asc_preflight(&r,runtime,sizeof runtime,epochs,(m10_look)look,(m10_mode)mode,&p);if(status!=M10_OK)return (int)status;
 out[0]=p.gamma_count;out[1]=p.control_mask;out[2]=p.control_value;
 for(i=0;i<9;i++){out[3+3*i]=p.cc1_fields[i].offset;out[4+3*i]=p.cc1_fields[i].mask;out[5+3*i]=p.cc1_fields[i].value;}
 for(i=0;i<p.gamma_count;i++){out[30+2*i]=p.gamma[i].full_bytes;out[31+2*i]=p.gamma[i].consume_bytes;}return 0;
}
int reference_asc_closed(unsigned look){return (int)m10_asc_apply((m10_look)look,M10_STILL).status;}
