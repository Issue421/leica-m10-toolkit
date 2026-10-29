/* MIT. Host-only GUI memory fixture. Calls real lvf open/draw/close APIs.
   These callbacks never represent the unbound ordinary-M10 native GUI owner. */
#include "lvf.h"
#include <stddef.h>
typedef struct {int objects[7],parent;unsigned applied,redrawn,restored;} fixture;
static int inspect(void *v,void *o,struct lvf_description *d){
 fixture *f=v;ptrdiff_t i=(int *)o-f->objects;static const int p[7]={0,0,1,1,0,4,4};
 static const int w[7]={720,0,720,720,0,2,2},h[7]={480,0,2,2,0,480,480};
 if(i<0||i>6)return 1;
 *d=(struct lvf_description){(uint16_t)(222+i),(uint16_t)(i==0?0:222+p[i]),(uint8_t)(i==0||i==1||i==4?6:0),(uint8_t)(i==6?2:0),(uint8_t)(i==0?1:i==3?2:0),i==0?(void *)&f->parent:(void *)&f->objects[p[i]],w[i],h[i],i==1||i==4?10000:0,i==1||i==4?10000:0};return 0;
}
static int begin(void *v,const struct lvf_binding *b,uint64_t *lease){(void)v;(void)b;*lease=1;return 0;}
static int check(void *v,uint64_t l){(void)v;return l==1?0:1;}
static int visible(void *v,uint64_t l,void *o,uint8_t x){(void)v;(void)l;(void)o;(void)x;return 0;}
static int apply(void *v,uint64_t l,void *o,const struct lvf_line *line,uint16_t color){fixture *f=v;(void)l;(void)o;(void)line;(void)color;++f->applied;return 0;}
static int redraw(void *v,uint64_t l,void *o){fixture *f=v;(void)l;(void)o;++f->redrawn;return 0;}
static int restore(void *v,uint64_t l){fixture *f=v;(void)l;++f->restored;return 0;}
int reference_frame(unsigned preset,int32_t out[26]){
 fixture f={0};struct lvf s={0};struct lvf_binding b={0};struct lvf_frame frame={0};
 struct lvf_backend backend={inspect,begin,check,visible,apply,redraw,restore};
 struct lvf_config c={1,1,LVF_PRESET,(uint8_t)preset,{3600,3600},{2400,2400},65535};unsigned i;
 b.display=(struct lvf_display){720,480,480,720,480,0,LVF_UPRIGHT,1};for(i=0;i<7;i++)b.object[i]=&f.objects[i];
 if(lvf_open(&s,&backend,&f,&b)!=LVF_OK||lvf_draw(&s,&c,&frame)!=LVF_OK)return 1;
 out[0]=frame.nominal_gate.x;out[1]=frame.nominal_gate.y;out[2]=frame.nominal_gate.width;out[3]=frame.nominal_gate.height;
 out[4]=frame.strict_aperture.x;out[5]=frame.strict_aperture.y;out[6]=frame.strict_aperture.width;out[7]=frame.strict_aperture.height;
 for(i=0;i<4;i++){out[8+4*i]=frame.line[i].pixels.x;out[9+4*i]=frame.line[i].pixels.y;out[10+4*i]=frame.line[i].pixels.width;out[11+4*i]=frame.line[i].pixels.height;}
 if(lvf_close(&s)!=LVF_OK)return 2;out[24]=(int32_t)f.applied;out[25]=(int32_t)f.restored;return 0;
}
