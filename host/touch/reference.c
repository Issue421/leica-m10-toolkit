/* MIT. HOST ordinary-memory callbacks only; never target imports. */
#include "touch_repair.h"
#include <stdint.h>
typedef struct { int raw; uint32_t submits,frees,debug,prepares; uint8_t envelope[36]; } fixture;
static int route(void *v,unsigned destination){(void)v;return destination==2u?7:0;}
static int allocate(void *v,size_t n,void **out){fixture *f=v;if(n!=36u)return -1;*out=f->envelope;return 0;}
static void prepare(void *v,void *e,unsigned event,void *payload){fixture *f=v;(void)e;(void)event;(void)payload;++f->prepares;}
static int submit(void *v,int box,void *e){fixture *f=v;(void)box;(void)e;++f->submits;return f->raw;}
static int release_env(void *v,void *e){(void)v;(void)e;return 0;}
static void release_payload(void *v,void *p){fixture *f=v;(void)p;++f->frees;}
static void debug(void *v,const void *s,unsigned c){fixture *f=v;(void)s;(void)c;++f->debug;}
int reference_touch(unsigned state,unsigned event,int raw,uint32_t out[7]){
 fixture f={0};uint8_t payload[360]={0},snapshot[360]={0};tr_result r;
 tr_dependencies d={&f,route,allocate,prepare,submit,release_env,release_payload,debug};f.raw=raw;
 tr_completed_tail(&d,state,event,payload,snapshot,0u,&r);
 out[0]=(uint32_t)r.outcome;out[1]=f.submits;out[2]=f.frees;out[3]=f.debug;
 out[4]=r.retained_envelope!=0;out[5]=r.retained_payload!=0;out[6]=f.prepares;return 0;
}
