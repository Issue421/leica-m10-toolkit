/* MIT. Explicit HOST boundary. Every GUI record is constructed from fields,
 * using artificial address labels; no recovered firmware bytes are used.
 * Native imports below operate on ordinary host arrays only, never a camera.
 */
#include "style_framing.h"
#include <string.h>

typedef struct {
 struct m10gui_record table[M10GUI_COUNT];struct m10gui_manager manager;
 uint32_t child[3][2];struct m10gui_memory memory;struct m10gui_domain domain;
 struct m10gui gui;int acquired,fail_once;unsigned calls,finishes;
} host_gui;
static host_gui g;
static const unsigned parents[7]={0,0,1,1,0,4,4},groups[3]={0,1,4};
static struct m10gui_record *record(unsigned i){return &g.table[M10GUI_FIRST+i];}
static uint32_t address(unsigned id){return 0x10000000u+id*M10GUI_STRIDE;}
int m10gui_guard_owner_inspect(void *owner,const struct m10gui_memory *memory,struct m10gui_domain *domain){
 if(owner!=&g||memory->table!=g.table)return 1;*domain=g.domain;return 0;
}
int m10gui_guard_owner_acquire(void *owner,const struct m10gui_memory *memory,uint64_t *lease){
 if(owner!=&g||memory->table!=g.table||g.acquired)return 1;g.acquired=1;*lease=7;return 0;
}
int m10gui_guard_owner_check(void *owner,uint64_t lease,const struct m10gui_memory *memory,struct m10gui_domain *domain){
 if(owner!=&g||memory->table!=g.table||lease!=7||!g.acquired)return 1;*domain=g.domain;return 0;
}
void m10gui_guard_owner_abort(void *owner,uint64_t lease){if(owner==&g&&lease==7)g.acquired=0;}
int m10gui_guard_owner_finish(void *owner,uint64_t lease,const struct m10gui_redraw_receipt *receipt){
 (void)receipt;if(owner!=&g||lease!=7||!g.acquired)return 1;g.acquired=0;++g.finishes;return 0;
}
static void mark(struct m10gui_record *r){if(r->effective){r->dirty=1;g.manager.dirty=1;}}
static void effective(unsigned i){
 unsigned j;struct m10gui_record *r=record(i);uint8_t p=i==0?g.table[0].effective:record(parents[i])->effective;
 uint8_t value=(uint8_t)(r->requested&p);if(r->effective!=value){r->effective=value;r->dirty=1;g.manager.dirty=1;}
 for(j=1;j<7;j++)if(parents[j]==i)effective(j);
}
int32_t m10gui_native_set_x(uint32_t id,int32_t value){struct m10gui_record *r=&g.table[id];int changed=r->inset_x!=value;r->inset_x=value;if(changed)mark(r);++g.calls;if(g.fail_once){g.fail_once=0;return 7;}return changed;}
int32_t m10gui_native_set_y(uint32_t id,int32_t value){struct m10gui_record *r=&g.table[id];int changed=r->inset_y!=value;r->inset_y=value;if(changed)mark(r);++g.calls;return changed;}
int32_t m10gui_native_set_size(uint32_t id,int32_t w,int32_t h){struct m10gui_record *r=&g.table[id];int changed=r->width!=w||r->height!=h;r->width=w;r->height=h;if(changed)mark(r);++g.calls;return changed;}
void m10gui_native_set_color(uint32_t id,uint16_t value){struct m10gui_record *r=&g.table[id];if(r->color!=value){r->color=value;mark(r);}++g.calls;}
void m10gui_native_set_visible(uint32_t id,uint32_t value){g.table[id].requested=(uint8_t)value;effective(id-M10GUI_FIRST);++g.calls;}
void m10gui_native_dirty(struct m10gui_record *r){mark(r);++g.calls;}
static int setup(void){
 unsigned i,j;static const int widths[7]={720,0,720,720,0,2,2},heights[7]={480,0,2,2,0,480,480};
 static const unsigned children[3][2]={{223,226},{224,225},{227,228}};
 memset(&g,0,sizeof g);g.table[0].id=0;g.table[0].type=6;g.table[0].effective=1;g.manager.scale=480;
 g.memory=(struct m10gui_memory){g.table,&g.manager,0x10000000u,{g.child[0],g.child[1],g.child[2]},{0x11000000u,0x11000008u,0x11000010u}};
 g.domain=(struct m10gui_domain){{720,480,480,720,480,0,LVF_UPRIGHT,1},0,0};
 for(i=0;i<7;i++){
  struct m10gui_record *r=record(i);int group=i==0||i==1||i==4;
  r->id=(uint16_t)(M10GUI_FIRST+i);r->state=2;r->opaque3=0xa5;r->type=(uint8_t)(group?6:0);
  r->width=widths[i];r->height=heights[i];r->parent=address(i==0?0:M10GUI_FIRST+parents[i]);
  r->anchor_x=(uint8_t)(i==6?2:0);r->anchor_y=(uint8_t)(i==0?1:i==3?2:0);
  r->relative_width=r->relative_height=(i==1||i==4)?10000:0;r->descendants=i==0?6:group?2:0;r->child_count=group?2:0;
  if(group){r->right=r->clip_right=720;r->bottom=r->clip_bottom=480;}
 }
 for(i=0;i<3;i++){record(groups[i])->children=g.memory.child_address[i];for(j=0;j<2;j++)g.child[i][j]=address(children[i][j]);}
 return m10gui_init(&g.gui,&g.memory,&g);
}

/* HOST ASC hardware/ownership callbacks, isolated from production closed imports.
 * They exercise the real m10_asc_apply sequence against pinned ordinary input.
 * No target owner, MMIO, physical idle/cache or qualification is established. */
static const uint8_t *resource;static uint32_t resource_bytes;
static uint8_t runtime[92];static unsigned idle,invalid,transfers,running;
static m10_field host_cc1[9];static uint8_t host_banks[8][32768];
static uint32_t host_lengths[8],host_control_mask,host_control_value,host_transferred;
m10_status m10_native_acquire_current(m10_native_inputs *in){
 if(!resource)return M10_UNQUALIFIED;memset(in,0,sizeof *in);in->resource=resource;in->mapped=in->registered=in->bytes_read=resource_bytes;
 in->runtime=runtime;in->runtime_bytes=sizeof runtime;in->epochs=(m10_epochs){1,1,1};in->current_mode=M10_STILL;transfers=0;host_transferred=0;return M10_OK;
}
m10_status m10_native_enter_confirmed_idle(void){idle=1;return M10_OK;}
m10_status m10_native_invalidate_both_caches(void){invalid=1;return idle?M10_OK:M10_POSTCONDITION;}
m10_status m10_native_verify_idle_cacheinvalid(void){return idle&&invalid?M10_OK:M10_POSTCONDITION;}
m10_status m10_native_write_cc1(const m10_field fields[9]){if(!fields||!idle||!invalid)return M10_POSTCONDITION;memcpy(host_cc1,fields,sizeof host_cc1);return M10_OK;}
m10_status m10_native_transfer_gamma(const m10_gamma_view *view){
 if(!idle||!invalid||!view->payload||transfers>=8||view->consume_bytes>view->full_bytes||view->consume_bytes>32768)return M10_POSTCONDITION;
 memcpy(host_banks[transfers],view->payload,view->consume_bytes);host_lengths[transfers]=view->consume_bytes;
 host_transferred+=view->consume_bytes;++transfers;return M10_OK;
}
m10_status m10_native_write_gamma_control(uint32_t mask,uint32_t value){if(mask!=(3u<<24)||!idle)return M10_POSTCONDITION;host_control_mask=mask;host_control_value=value;return M10_OK;}
m10_status m10_native_check_current_epochs(m10_epochs epochs){return epochs.resource==1&&epochs.calibration==1&&epochs.hardware==1?M10_OK:M10_EPOCH;}
m10_status m10_native_verify_complete_idle(const m10_plan *plan){
 unsigned i;if(!idle||!invalid||transfers!=plan->gamma_count||memcmp(host_cc1,plan->cc1_fields,sizeof host_cc1)||host_control_mask!=plan->control_mask||host_control_value!=plan->control_value)return M10_POSTCONDITION;
 for(i=0;i<transfers;i++)if(host_lengths[i]!=plan->gamma[i].consume_bytes||memcmp(host_banks[i],plan->gamma[i].payload,host_lengths[i]))return M10_POSTCONDITION;
 return M10_OK;
}
m10_status m10_native_publish_and_resume(const m10_plan *plan){running=(unsigned)plan->look;idle=invalid=0;return M10_OK;}
m10_status m10_native_verify_running(const m10_plan *plan){return !idle&&!invalid&&running==(unsigned)plan->look?M10_OK:M10_POSTCONDITION;}
m10_status m10_native_abort_quiescent_cacheinvalid(void){idle=invalid=1;return M10_OK;}
void m10_native_release_current(void){}

int reference_controller(unsigned style,unsigned guides,unsigned preset,const uint8_t *body,uint32_t bytes,unsigned fail_gui,int32_t out[48]){
 struct sfc controller={0};struct sfc_config source={0},frozen;struct sfc_gui_input gui;struct lvf_frame frame={0};struct sfc_result applied,restored;unsigned domains,i;
 resource=body;resource_bytes=bytes;memset(runtime,0,sizeof runtime);runtime[88]=1;idle=invalid=running=host_transferred=0;
 if(setup()!=0)return 1;
 source.style_enabled=1;source.style=(enum sfc_style)style;source.mode=M10_STILL;
 source.guides=(struct lvf_config){1,(uint8_t)guides,LVF_PRESET,(uint8_t)preset,{3600,3600},{2400,2400},65535};
 if(sfc_freeze(&source,&frozen)!=SFC_OK)return 2;
 source.guides.preset=(uint8_t)((preset+1u)%8u); /* caller changes after freeze */
 if(m10gui_binding(&g.gui,&gui.binding)!=0)return 3;gui.context=&g.gui;g.fail_once=(int)fail_gui;
 domains=style==SFC_NATIVE_FILM?SFC_STYLE:(style==SFC_NORMAL&&!body?SFC_GUIDES:(guides?SFC_BOTH:SFC_STYLE));
 applied=sfc_apply(&controller,&frozen,domains,&gui,&frame);
 out[0]=(int32_t)applied.status;out[1]=(int32_t)applied.style.status;out[2]=(int32_t)applied.style.stage;
 out[3]=(int32_t)applied.attempted;out[4]=(int32_t)applied.completed;out[5]=(int32_t)applied.needs_restore;
 out[38]=(int32_t)host_transferred;
 if(style==SFC_NATIVE_FILM&&guides){struct sfc_result independent=sfc_apply(&controller,&frozen,SFC_GUIDES,&gui,&frame);out[6]=(int32_t)independent.status;out[7]=(int32_t)independent.completed;}
 out[8]=frame.nominal_gate.x;out[9]=frame.nominal_gate.y;out[10]=frame.nominal_gate.width;out[11]=frame.nominal_gate.height;
 out[12]=frame.strict_aperture.x;out[13]=frame.strict_aperture.y;out[14]=frame.strict_aperture.width;out[15]=frame.strict_aperture.height;
 for(i=0;i<4;i++){out[16+4*i]=frame.line[i].pixels.x;out[17+4*i]=frame.line[i].pixels.y;out[18+4*i]=frame.line[i].pixels.width;out[19+4*i]=frame.line[i].pixels.height;}
 restored=sfc_restore(&controller,SFC_BOTH,M10_STILL);out[32]=(int32_t)restored.status;out[33]=(int32_t)restored.needs_restore;
 out[34]=(int32_t)g.finishes;out[35]=(int32_t)g.calls;out[36]=(int32_t)controller.guides.opened;out[37]=(int32_t)running;
 out[39]=(int32_t)host_transferred;
 for(i=0;i<7;i++){
  const struct m10gui_record *r=record(i);
  if(r->id!=M10GUI_FIRST+i||r->state!=2||r->opaque3!=0xa5||r->requested||r->effective)return 4;
  if(i==2||i==3){if(r->inset_x||r->inset_y||r->width!=720||r->height!=2||r->color)return 5;}
  if(i==5||i==6){if(r->inset_x||r->inset_y||r->width!=2||r->height!=480||r->color)return 5;}
 }
 resource=0;resource_bytes=0;return 0;
}
