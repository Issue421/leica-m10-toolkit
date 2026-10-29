#include "m10_gui_native.h"

static const unsigned parents[LVF_OBJECTS]={0,0,1,1,0,4,4};
static const unsigned lines[LVF_LINES]={2,3,5,6};
static const uint8_t types[LVF_OBJECTS]={6,6,0,0,6,0,0};
static const int32_t widths[LVF_OBJECTS]={720,0,720,720,0,2,2};
static const int32_t heights[LVF_OBJECTS]={480,0,2,2,0,480,480};
static const uint32_t child_ids[3][2]={{223,226},{224,225},{227,228}};
static const unsigned groups[3]={0,1,4};
static const volatile struct m10gui_record *record(const struct m10gui *p,unsigned i) {
    return &p->memory.table[M10GUI_FIRST+i];
}
static uint32_t address(const struct m10gui *p,uint32_t id) {
    return p->memory.table_address+id*(uint32_t)M10GUI_STRIDE;
}
static int role(const struct m10gui *p,const void *object) {
    unsigned i;
    for(i=0;i<LVF_OBJECTS;++i)
        if(object == (const void *)&p->memory.table[M10GUI_FIRST+i]) return (int)i;
    return -1; /* do not dereference arbitrary incoming handles */
}
static int display_equal(const struct lvf_display *a,const struct lvf_display *b) {
    return a->logical_width==b->logical_width && a->logical_height==b->logical_height &&
        a->scale==b->scale && a->pixel_width==b->pixel_width && a->pixel_height==b->pixel_height &&
        a->device_code==b->device_code && a->orientation==b->orientation && a->qualified==b->qualified;
}
static int domain_ok(const struct m10gui *p,const struct m10gui_domain *d) {
    const volatile struct m10gui_record *r=record(p,0);
    const volatile struct m10gui_record *parent=&p->memory.table[0];
    const volatile struct m10gui_manager *m=p->memory.manager;
    int32_t scale=d->display.device_code==2U ? 682 : 480;
    int64_t right=(int64_t)d->root_left+d->display.pixel_width;
    int64_t bottom=(int64_t)d->root_top+d->display.pixel_height;
    unsigned i;
    if(d->display.qualified!=1U || d->display.device_code>2U ||
       d->display.orientation!=LVF_UPRIGHT || d->display.logical_width!=720U ||
       d->display.logical_height!=480U || d->display.scale!=scale || m->scale!=scale ||
       d->display.pixel_width!=(scale==682 ? 1023U : 720U) || d->display.pixel_height!=scale ||
       parent->id!=0U || parent->type!=6U || parent->effective!=1U || parent->parent!=0U ||
       r->left!=d->root_left || r->top!=d->root_top || r->right!=right || r->bottom!=bottom)
        return 0;
    /* Type 6 skips ancestor marking, not traversal: parent 0 must terminate.
       Root and full-size groups must already have a usable, unclipped footprint.
       We read layout output; we do not duplicate/call the anchor geometry. */
    for(i=0;i<3U;++i) {
        const volatile struct m10gui_record *g=record(p,groups[i]);
        if(g->left!=r->left || g->top!=r->top || g->right!=r->right || g->bottom!=r->bottom ||
           g->clip_left!=g->left || g->clip_top!=g->top ||
           g->clip_right!=g->right || g->clip_bottom!=g->bottom) return 0;
    }
    return 1;
}
static int family_ok(const struct m10gui *p,int initial) {
    unsigned i,j;
    for(i=0;i<LVF_OBJECTS;++i) {
        const volatile struct m10gui_record *r=record(p,i);
        int group=(i==1U || i==4U);
        if(r->id!=M10GUI_FIRST+i || r->type!=types[i] ||
           r->parent!=address(p,i==0U?0U:M10GUI_FIRST+parents[i]) ||
           r->anchor_x!=(i==6U?2U:0U) || r->anchor_y!=(i==0U?1U:(i==3U?2U:0U)) ||
           r->offset_x!=0 || r->offset_y!=0 || r->relative_x!=0 || r->relative_y!=0 ||
           r->pixel_width!=0 || r->pixel_height!=0 ||
           r->relative_width!=(group?10000:0) || r->relative_height!=(group?10000:0) ||
           r->requested>1U || r->effective>1U ||
           r->descendants!=(i==0U?6U:(group?2U:0U)) ||
           r->child_count!=((i==0U||group)?2U:0U)) return 0;
        if((initial || i==0U || group) && (r->width!=widths[i] || r->height!=heights[i])) return 0;
        if((i==0U || group) && (r->inset_x!=0 || r->inset_y!=0)) return 0;
        if(p->active && r->callback!=p->callbacks[i]) return 0;
    }
    for(i=0;i<3U;++i) {
        if(record(p,groups[i])->children!=p->memory.child_address[i]) return 0;
        for(j=0;j<2U;++j)
            if(p->memory.child[i][j]!=address(p,child_ids[i][j])) return 0;
    }
    return 1;
}
static int visibility_ok(const struct m10gui *p) {
    unsigned i;
    for(i=0;i<LVF_OBJECTS;++i) {
        const volatile struct m10gui_record *r=record(p,i);
        uint8_t parent=i==0U?((const volatile struct m10gui_record *)&p->memory.table[0])->effective:record(p,parents[i])->effective;
        if(r->effective!=(uint8_t)(r->requested & parent)) return 0;
    }
    return 1;
}
static int checked(struct m10gui *p,uint64_t lease) {
    struct m10gui_domain d={0};
    if(!p || !p->active || !lease || lease!=p->lease ||
       m10gui_guard_owner_check(p->owner,lease,&p->memory,&d)!=0) return 0;
    return display_equal(&d.display,&p->domain.display) && d.root_left==p->domain.root_left &&
        d.root_top==p->domain.root_top && domain_ok(p,&d) && family_ok(p,0) && visibility_ok(p);
}
static int inspect(void *ctx,void *object,struct lvf_description *out) {
    struct m10gui *p=ctx;
    struct m10gui_domain d={0};
    const volatile struct m10gui_record *r;
    int i;
    if(!p || !out || (i=role(p,object))<0 ||
       m10gui_guard_owner_inspect(p->owner,&p->memory,&d)!=0 || !domain_ok(p,&d) || !family_ok(p,!p->active)) return 1;
    r=record(p,(unsigned)i);
    *out=(struct lvf_description){(uint16_t)r->id,
        (uint16_t)(i==0?0U:M10GUI_FIRST+parents[(unsigned)i]),r->type,r->anchor_x,r->anchor_y,
        &p->memory.table[i==0?0U:M10GUI_FIRST+parents[(unsigned)i]],
        r->width,r->height,r->relative_width,r->relative_height};
    return 0;
}
static int begin(void *ctx,const struct lvf_binding *b,uint64_t *lease) {
    struct m10gui *p=ctx;
    struct m10gui_domain d={0};
    const volatile struct m10gui_manager *manager;
    uint64_t acquired=0;
    unsigned i;
    if(!p || !b || !lease || p->active) return 1;
    for(i=0;i<LVF_OBJECTS;++i)
        if(b->object[i]!=(void *)&p->memory.table[M10GUI_FIRST+i]) return 1;
    if(m10gui_guard_owner_acquire(p->owner,&p->memory,&acquired)!=0) return 1;
    if(acquired==0U) { m10gui_guard_owner_abort(p->owner,acquired); return 1; }
    if(m10gui_guard_owner_check(p->owner,acquired,&p->memory,&d)!=0 ||
       !display_equal(&b->display,&d.display) || !domain_ok(p,&d) || !family_ok(p,1) || !visibility_ok(p)) {
        m10gui_guard_owner_abort(p->owner,acquired); return 1;
    }
    /* No native writes before the full snapshot exists. */
    for(i=0;i<LVF_OBJECTS;++i) {
        const volatile struct m10gui_record *r=record(p,i);
        p->requests[i]=r->requested; p->redraw_before.family_dirty[i]=r->dirty; p->callbacks[i]=r->callback;
    }
    for(i=0;i<LVF_LINES;++i) {
        const volatile struct m10gui_record *r=record(p,lines[i]);
        p->saved[i]=(struct m10gui_saved_line){r->inset_x,r->inset_y,r->width,r->height,r->color};
    }
    manager=p->memory.manager; p->redraw_before.manager_dirty=manager->dirty;
    p->binding=*b; p->domain=d; p->lease=acquired; p->active=1; *lease=acquired;
    return 0;
}
static int check(void *ctx,uint64_t lease) { return checked(ctx,lease)?0:1; }
static int visible(void *ctx,uint64_t lease,void *object,uint8_t request) {
    struct m10gui *p=ctx;
    uint8_t before[LVF_OBJECTS];
    const volatile struct m10gui_manager *m;
    int index;
    unsigned i;
    if(!checked(p,lease) || request>1U || (index=role(p,object))<0) return 1;
    for(i=0;i<LVF_OBJECTS;++i) before[i]=record(p,i)->effective;
    m10gui_native_set_visible(M10GUI_FIRST+(uint32_t)index,request);
    if(!checked(p,lease) || record(p,(unsigned)index)->requested!=request) return 1;
    m=p->memory.manager;
    for(i=0;i<LVF_OBJECTS;++i)
        if(record(p,i)->effective!=before[i] && (record(p,i)->dirty!=1U || m->dirty!=1U)) return 1;
    return 0;
}
static int set_line(struct m10gui *p,uint64_t lease,unsigned index,const struct m10gui_saved_line *s) {
    const volatile struct m10gui_record *r;
    int32_t before,ret;
    uint32_t id=M10GUI_FIRST+index;
    if(!checked(p,lease) || types[index]!=0U) return 1;
    r=record(p,index); before=r->inset_x;
    ret=m10gui_native_set_x(id,s->x);
    if(!checked(p,lease) || ret!=(before!=s->x?1:0) || r->inset_x!=s->x) return 1;
    before=r->inset_y; ret=m10gui_native_set_y(id,s->y);
    if(!checked(p,lease) || ret!=(before!=s->y?1:0) || r->inset_y!=s->y) return 1;
    before=(r->width!=s->w || r->height!=s->h)?1:0;
    ret=m10gui_native_set_size(id,s->w,s->h);
    if(!checked(p,lease) || ret!=before || r->width!=s->w || r->height!=s->h) return 1;
    m10gui_native_set_color(id,s->color); /* void: validate field, never inspect garbage r0 */
    if(!checked(p,lease) || r->color!=s->color) return 1;
    return 0;
}
static int apply(void *ctx,uint64_t lease,void *object,const struct lvf_line *line,uint16_t color) {
    struct m10gui *p=ctx;
    struct m10gui_saved_line s;
    int index;
    if(!p || !line || (index=role(p,object))<0 || types[(unsigned)index]!=0U ||
       !checked(p,lease) || record(p,0)->requested!=0U || record(p,0)->effective!=0U) return 1;
    /* These are pre-scale logical insets/extents. line->pixels is a diagnostic
       result from frozen lvf.c, NEVER written into native inset fields. */
    s=(struct m10gui_saved_line){line->x,line->y,line->width,line->height,color};
    return set_line(p,lease,(unsigned)index,&s);
}
static int redraw(void *ctx,uint64_t lease,void *object) {
    struct m10gui *p=ctx;
    const volatile struct m10gui_manager *m;
    if(!checked(p,lease) || role(p,object)!=0) return 1;
    m10gui_native_dirty(&p->memory.table[M10GUI_FIRST]);
    if(!checked(p,lease)) return 1;
    m=p->memory.manager;
    /* Native dirty is intentionally a no-op when effective visibility is 0.
       A visibility transition has already queued its own dirty propagation. */
    return record(p,0)->effective==1U && (record(p,0)->dirty!=1U || m->dirty!=1U) ? 1:0;
}
static int restore_release(void *ctx,uint64_t lease) {
    struct m10gui *p=ctx;
    unsigned i;
    if(!checked(p,lease) || visible(p,lease,p->binding.object[0],0)!=0) return 1;
    for(i=0;i<LVF_LINES;++i)
        if(set_line(p,lease,lines[i],&p->saved[i])!=0) return 1;
    for(i=1;i<LVF_OBJECTS;++i)
        if(visible(p,lease,p->binding.object[i],p->requests[i])!=0) return 1;
    if(visible(p,lease,p->binding.object[0],p->requests[0])!=0 ||
       redraw(p,lease,p->binding.object[0])!=0) return 1;
    /* No raw restoration of object/manager/ancestor dirty bookkeeping. */
    if(m10gui_guard_owner_finish(p->owner,lease,&p->redraw_before)!=0) return 1;
    p->active=0; p->lease=0; return 0;
}
int m10gui_init(struct m10gui *p,const struct m10gui_memory *memory,void *owner) {
    unsigned i;
    if(!p || !memory || !owner || p->active || !memory->table || !memory->manager ||
       memory->table_address==0U || (memory->table_address & 3U)!=0U ||
       memory->table_address>UINT32_MAX-(uint32_t)M10GUI_COUNT*(uint32_t)M10GUI_STRIDE) return 1;
    for(i=0;i<3U;++i)
        if(!memory->child[i] || !memory->child_address[i] || (memory->child_address[i]&3U)!=0U) return 1;
    *p=(struct m10gui){0}; p->memory=*memory; p->owner=owner; return 0;
}
int m10gui_binding(struct m10gui *p,struct lvf_binding *out) {
    struct m10gui_domain d={0};
    struct lvf_binding b={0};
    unsigned i;
    if(!p || !out || p->active || m10gui_guard_owner_inspect(p->owner,&p->memory,&d)!=0 ||
       !domain_ok(p,&d) || !family_ok(p,1) || !visibility_ok(p)) return 1;
    for(i=0;i<LVF_OBJECTS;++i) b.object[i]=&p->memory.table[M10GUI_FIRST+i];
    b.display=d.display; *out=b; return 0;
}
const struct lvf_backend *m10gui_backend(void) {
    static const struct lvf_backend b={inspect,begin,check,visible,apply,redraw,restore_release};
    return &b;
}
